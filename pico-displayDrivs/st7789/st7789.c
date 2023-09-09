#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"

#include "st7789.h"

uint16_t _colstart = 0, _rowstart = 0, _colstart2 = 0, _rowstart2 = 0;

uint8_t tabcolor;

uint16_t _width;  ///< Display width as modified by current rotation
uint16_t _height; ///< Display height as modified by current rotation

uint16_t windowWidth;
uint16_t windowHeight;

int16_t _xstart = 0; ///< Internal framebuffer X offset
int16_t _ystart = 0; ///< Internal framebuffer Y offset

uint8_t rotation;

spi_inst_t *st7789_spi = spi_default;

uint16_t st7789_pinCS = 17;
uint16_t st7789_pinDC = 16;
int16_t st7789_pinRST = -1;

uint16_t st7789_pinSCK = PICO_DEFAULT_SPI_SCK_PIN;
uint16_t st7789_pinTX = PICO_DEFAULT_SPI_TX_PIN;

// uint16_t st7789_pinRST;

static const uint8_t generic_st7789[] = { // Init commands for 7789 screens
	9,									  //  9 commands in list:
	ST77XX_SWRESET, ST_CMD_DELAY,		  //  1: Software reset, no args, w/delay
	150,								  //     ~150 ms delay
	ST77XX_SLPOUT, ST_CMD_DELAY,		  //  2: Out of sleep mode, no args, w/delay
	10,									  //      10 ms delay
	ST77XX_COLMOD, 1 + ST_CMD_DELAY,	  //  3: Set color mode, 1 arg + delay:
	0x55,								  //     16-bit color
	10,									  //     10 ms delay
	ST77XX_MADCTL, 1,					  //  4: Mem access ctrl (directions), 1 arg:
	0x08,								  //     Row/col addr, bottom-top refresh
	ST77XX_CASET, 4,					  //  5: Column addr set, 4 args, no delay:
	0x00,
	0, //     XSTART = 0
	0,
	240,			 //     XEND = 240
	ST77XX_RASET, 4, //  6: Row addr set, 4 args, no delay:
	0x00,
	0, //     YSTART = 0
	320 >> 8,
	320 & 0xFF,					//     YEND = 320
	ST77XX_INVON, ST_CMD_DELAY, //  7: hack
	10,
	ST77XX_NORON, ST_CMD_DELAY,	 //  8: Normal display on, no args, w/delay
	10,							 //     10 ms delay
	ST77XX_DISPON, ST_CMD_DELAY, //  9: Main screen turn on, no args, delay
	10};						 //    10 ms delay

#ifdef USE_DMA
uint dma_tx;
dma_channel_config dma_cfg;
void waitForDMA()
{

	dma_channel_wait_for_finish_blocking(dma_tx);
}
#endif

void LCD_setPins(uint16_t dc, uint16_t cs, int16_t rst, uint16_t sck, uint16_t tx)
{
	st7789_pinDC = dc;
	st7789_pinCS = cs;
	st7789_pinRST = rst;
	st7789_pinSCK = sck;
	st7789_pinTX = tx;
}

void LCD_setSPIperiph(spi_inst_t *s)
{
	st7789_spi = s;
}

void initSPI()
{
	spi_init(st7789_spi, 1000 * 40000);
	spi_set_format(st7789_spi, 16, SPI_CPOL_1, SPI_CPOL_1, SPI_MSB_FIRST);
	gpio_set_function(st7789_pinSCK, GPIO_FUNC_SPI);
	gpio_set_function(st7789_pinTX, GPIO_FUNC_SPI);

	gpio_init(st7789_pinCS);
	gpio_set_dir(st7789_pinCS, GPIO_OUT);
	gpio_put(st7789_pinCS, 1);

	gpio_init(st7789_pinDC);
	gpio_set_dir(st7789_pinDC, GPIO_OUT);
	gpio_put(st7789_pinDC, 1);

	if (st7789_pinRST != -1)
	{
		gpio_init(st7789_pinRST);
		gpio_set_dir(st7789_pinRST, GPIO_OUT);
		gpio_put(st7789_pinRST, 1);
	}

#ifdef USE_DMA
	dma_tx = dma_claim_unused_channel(true);
	dma_cfg = dma_channel_get_default_config(dma_tx);
	channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_16);
	channel_config_set_dreq(&dma_cfg, spi_get_dreq(st7789_spi, true));
#endif
}

void ST7789_Reset()
{
	if (st7789_pinRST != -1)
	{
		gpio_put(st7789_pinRST, 0);
		sleep_ms(5);
		gpio_put(st7789_pinRST, 1);
	}
}

void ST7789_Select()
{
	gpio_put(st7789_pinCS, 0);
}

void ST7789_DeSelect()
{
	gpio_put(st7789_pinCS, 1);
}

void ST7789_RegCommand()
{
	gpio_put(st7789_pinDC, 0);
}

void ST7789_RegData()
{
	gpio_put(st7789_pinDC, 1);
}

void ST7789_WriteCommand(uint8_t cmd)
{
	ST7789_RegCommand();
	spi_set_format(st7789_spi, 8, SPI_CPOL_1, SPI_CPOL_1, SPI_MSB_FIRST);
	spi_write_blocking(st7789_spi, &cmd, sizeof(cmd));
}

void ST7789_WriteData(uint8_t *buff, size_t buff_size)
{
	ST7789_RegData();
	spi_set_format(st7789_spi, 8, SPI_CPOL_1, SPI_CPOL_1, SPI_MSB_FIRST);
	spi_write_blocking(st7789_spi, buff, buff_size);
}

void ST7789_SendCommand(uint8_t commandByte, uint8_t *dataBytes,
						uint8_t numDataBytes)
{
	ST7789_Select();

	ST7789_WriteCommand(commandByte);
	ST7789_WriteData(dataBytes, numDataBytes);

	ST7789_DeSelect();
}

void ST7789_displayInit(const uint8_t *addr)
{

	uint8_t numCommands, cmd, numArgs;
	uint16_t ms;

	numCommands = *(addr++); // Number of commands to follow
	while (numCommands--)
	{								 // For each command...
		cmd = *(addr++);			 // Read command
		numArgs = *(addr++);		 // Number of args to follow
		ms = numArgs & ST_CMD_DELAY; // If hibit set, delay follows args
		numArgs &= ~ST_CMD_DELAY;	 // Mask out delay bit
		ST7789_SendCommand(cmd, addr, numArgs);
		addr += numArgs;

		if (ms)
		{
			ms = *(addr++); // Read post-command delay time (ms)
			if (ms == 255)
				ms = 500; // If 255, delay for 500 ms
			sleep_ms(ms);
		}
	}
}

void LCD_setRotation(uint8_t m)
{
	uint8_t madctl = 0;

	rotation = m & 3; // can't be higher than 3

	switch (rotation)
	{
	case 0:
		madctl = ST77XX_MADCTL_MX | ST77XX_MADCTL_MY | ST77XX_MADCTL_RGB;
		_xstart = _colstart;
		_ystart = _rowstart;
		_width = windowWidth;
		_height = windowHeight;
		break;
	case 1:
		madctl = ST77XX_MADCTL_MY | ST77XX_MADCTL_MV | ST77XX_MADCTL_RGB;
		_xstart = _rowstart;
		_ystart = _colstart2;
		_height = windowWidth;
		_width = windowHeight;
		break;
	case 2:
		madctl = ST77XX_MADCTL_RGB;
		_xstart = _colstart2;
		_ystart = _rowstart2;
		_width = windowWidth;
		_height = windowHeight;
		break;
	case 3:
		madctl = ST77XX_MADCTL_MX | ST77XX_MADCTL_MV | ST77XX_MADCTL_RGB;
		_xstart = _rowstart2;
		_ystart = _colstart;
		_height = windowWidth;
		_width = windowHeight;
		break;
	}

	ST7789_SendCommand(ST77XX_MADCTL, &madctl, 1);
}

void LCD_initDisplay(uint16_t width, uint16_t height)
{

	initSPI();

	if (width == 172 && height == 320)
	{
		// 1.47" display
		_rowstart = _rowstart2 = 0;
		_colstart = _colstart2 = 34;
	}
	else if (width == 240 && height == 280)
	{
		// 1.69" display
		_rowstart = 20;
		_rowstart2 = 0;
		_colstart = _colstart2 = 0;
	}
	else if (width == 135 && height == 240)
	{
		// 1.14" display
		_rowstart = _rowstart2 = (int)((320 - height) / 2);
		// This is the only device currently supported device that has different
		// values for _colstart & _colstart2. You must ensure that the extra
		// pixel lands in _colstart and not in _colstart2
		_colstart = (int)((240 - width + 1) / 2);
		_colstart2 = (int)((240 - width) / 2);
	}
	else
	{
		// 1.3", 1.54", and 2.0" displays
		_rowstart = (320 - height);
		_rowstart2 = 0;
		_colstart = _colstart2 = (240 - width);
	}

	windowWidth = width;
	windowHeight = height;
	ST7789_Select();
	ST7789_Reset();
	ST7789_displayInit(generic_st7789);
	LCD_setRotation(2);
}

void LCD_setAddrWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{

	x += _xstart;
	y += _ystart;

	uint32_t xa = ((uint32_t)x << 16) | (x + w - 1);
	uint32_t ya = ((uint32_t)y << 16) | (y + h - 1);

	xa = __builtin_bswap32(xa);
	ya = __builtin_bswap32(ya);

	ST7789_WriteCommand(ST77XX_CASET);
	ST7789_WriteData(&xa, sizeof(xa));

	// row address set
	ST7789_WriteCommand(ST77XX_RASET);
	ST7789_WriteData(&ya, sizeof(ya));

	// write to RAM
	ST7789_WriteCommand(ST77XX_RAMWR);
}

void LCD_WriteBitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
	ST7789_Select();
	LCD_setAddrWindow(x, y, w, h); // Clipped area
	ST7789_RegData();
	spi_set_format(st7789_spi, 16, SPI_CPOL_1, SPI_CPOL_1, SPI_MSB_FIRST);
#ifdef USE_DMA
	dma_channel_configure(dma_tx, &dma_cfg,
						  &spi_get_hw(st7789_spi)->dr, // write address
						  bitmap,					   // read address
						  w * h,					   // element count (each element is of size transfer_data_size)
						  true);					   // start asap
	waitForDMA();
#else

	spi_write16_blocking(st7789_spi, bitmap, w * h);
#endif

	ST7789_DeSelect();
}

void LCD_WritePixel(int x, int y, uint16_t col)
{
	ST7789_Select();
	LCD_setAddrWindow(x, y, 1, 1); // Clipped area
	ST7789_RegData();
	spi_set_format(st7789_spi, 16, SPI_CPOL_1, SPI_CPOL_1, SPI_MSB_FIRST);
	spi_write16_blocking(st7789_spi, &col, 1);
	ST7789_DeSelect();
}
