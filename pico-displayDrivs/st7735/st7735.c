#include "pico/stdlib.h"

#include "st7735.h"

uint16_t _colstart = 0, _rowstart = 0, _colstart2 = 0, _rowstart2 = 0;

uint8_t tabcolor;

uint16_t _width;  ///< Display width as modified by current rotation
uint16_t _height; ///< Display height as modified by current rotation

int16_t _xstart = 0; ///< Internal framebuffer X offset
int16_t _ystart = 0; ///< Internal framebuffer Y offset

uint8_t rotation;

uint16_t st7735_pinCS = PICO_DEFAULT_SPI_CSN_PIN;
uint16_t st7735_pinDC = 20;
int16_t st7735_pinRST = 16;

uint16_t st7735_pinSCK = PICO_DEFAULT_SPI_SCK_PIN;
uint16_t st7735_pinTX = PICO_DEFAULT_SPI_TX_PIN;

const uint8_t Bcmd[] = {			  // Init commands for 7735B screens
	18,								  // 18 commands in list:
	ST77XX_SWRESET, ST_CMD_DELAY,	  //  1: Software reset, no args, w/delay
	50,								  //     50 ms delay
	ST77XX_SLPOUT, ST_CMD_DELAY,	  //  2: Out of sleep mode, no args, w/delay
	255,							  //     255 = max (500 ms) delay
	ST77XX_COLMOD, 1 + ST_CMD_DELAY,  //  3: Set color mode, 1 arg + delay:
	0x05,							  //     16-bit color
	10,								  //     10 ms delay
	ST7735_FRMCTR1, 3 + ST_CMD_DELAY, //  4: Frame rate control, 3 args + delay:
	0x00,							  //     fastest refresh
	0x06,							  //     6 lines front porch
	0x03,							  //     3 lines back porch
	10,								  //     10 ms delay
	ST77XX_MADCTL, 1,				  //  5: Mem access ctl (directions), 1 arg:
	0x08,							  //     Row/col addr, bottom-top refresh
	ST7735_DISSET5, 2,				  //  6: Display settings #5, 2 args:
	0x15,							  //     1 clk cycle nonoverlap, 2 cycle gate
									  //     rise, 3 cycle osc equalize
	0x02,							  //     Fix on VTL
	ST7735_INVCTR, 1,				  //  7: Display inversion control, 1 arg:
	0x0,							  //     Line inversion
	ST7735_PWCTR1, 2 + ST_CMD_DELAY,  //  8: Power control, 2 args + delay:
	0x02,							  //     GVDD = 4.7V
	0x70,							  //     1.0uA
	10,								  //     10 ms delay
	ST7735_PWCTR2, 1,				  //  9: Power control, 1 arg, no delay:
	0x05,							  //     VGH = 14.7V, VGL = -7.35V
	ST7735_PWCTR3, 2,				  // 10: Power control, 2 args, no delay:
	0x01,							  //     Opamp current small
	0x02,							  //     Boost frequency
	ST7735_VMCTR1, 2 + ST_CMD_DELAY,  // 11: Power control, 2 args + delay:
	0x3C,							  //     VCOMH = 4V
	0x38,							  //     VCOML = -1.1V
	10,								  //     10 ms delay
	ST7735_PWCTR6, 2,				  // 12: Power control, 2 args, no delay:
	0x11, 0x15,
	ST7735_GMCTRP1, 16,		// 13: Gamma Adjustments (pos. polarity), 16 args + delay:
	0x09, 0x16, 0x09, 0x20, //     (Not entirely necessary, but provides
	0x21, 0x1B, 0x13, 0x19, //      accurate colors)
	0x17, 0x15, 0x1E, 0x2B, 0x04, 0x05, 0x02, 0x0E,
	ST7735_GMCTRN1, 16 + ST_CMD_DELAY,					// 14: Gamma Adjustments (neg. polarity), 16 args + delay:
	0x0B, 0x14, 0x08, 0x1E,								//     (Not entirely necessary, but provides
	0x22, 0x1D, 0x18, 0x1E,								//      accurate colors)
	0x1B, 0x1A, 0x24, 0x2B, 0x06, 0x06, 0x02, 0x0F, 10, //     10 ms delay
	ST77XX_CASET, 4,									// 15: Column addr set, 4 args, no delay:
	0x00, 0x02,											//     XSTART = 2
	0x00, 0x81,											//     XEND = 129
	ST77XX_RASET, 4,									// 16: Row addr set, 4 args, no delay:
	0x00, 0x02,											//     XSTART = 1
	0x00, 0x81,											//     XEND = 160
	ST77XX_NORON, ST_CMD_DELAY,							// 17: Normal display on, no args, w/delay
	10,													//     10 ms delay
	ST77XX_DISPON, ST_CMD_DELAY,						// 18: Main screen turn on, no args, delay
	255},												//     255 = max (500 ms) delay

	Rcmd1[] = {						  // 7735R init, part 1 (red or green tab)
		15,							  // 15 commands in list:
		ST77XX_SWRESET, ST_CMD_DELAY, //  1: Software reset, 0 args, w/delay
		150,						  //     150 ms delay
		ST77XX_SLPOUT, ST_CMD_DELAY,  //  2: Out of sleep mode, 0 args, w/delay
		255,						  //     500 ms delay
		ST7735_FRMCTR1, 3,			  //  3: Framerate ctrl - normal mode, 3 arg:
		0x01, 0x2C, 0x2D,			  //     Rate = fosc/(1x2+40) * (LINE+2C+2D)
		ST7735_FRMCTR2, 3,			  //  4: Framerate ctrl - idle mode, 3 args:
		0x01, 0x2C, 0x2D,			  //     Rate = fosc/(1x2+40) * (LINE+2C+2D)
		ST7735_FRMCTR3, 6,			  //  5: Framerate - partial mode, 6 args:
		0x01, 0x2C, 0x2D,			  //     Dot inversion mode
		0x01, 0x2C, 0x2D,			  //     Line inversion mode
		ST7735_INVCTR, 1,			  //  6: Display inversion ctrl, 1 arg:
		0x07,						  //     No inversion
		ST7735_PWCTR1, 3,			  //  7: Power control, 3 args, no delay:
		0xA2, 0x02,					  //     -4.6V
		0x84,						  //     AUTO mode
		ST7735_PWCTR2, 1,			  //  8: Power control, 1 arg, no delay:
		0xC5,						  //     VGH25=2.4C VGSEL=-10 VGH=3 * AVDD
		ST7735_PWCTR3, 2,			  //  9: Power control, 2 args, no delay:
		0x0A,						  //     Opamp current small
		0x00,						  //     Boost frequency
		ST7735_PWCTR4, 2,			  // 10: Power control, 2 args, no delay:
		0x8A,						  //     BCLK/2,
		0x2A,						  //     opamp current small & medium low
		ST7735_PWCTR5, 2,			  // 11: Power control, 2 args, no delay:
		0x8A, 0xEE, ST7735_VMCTR1, 1, // 12: Power control, 1 arg, no delay:
		0x0E, ST77XX_INVOFF, 0,		  // 13: Don't invert display, no args
		ST77XX_MADCTL, 1,			  // 14: Mem access ctl (directions), 1 arg:
		0xC8,						  //     row/col addr, bottom-top refresh
		ST77XX_COLMOD, 1,			  // 15: set color mode, 1 arg, no delay:
		0x05},						  //     16-bit color

	Rcmd2green[] = {		// 7735R init, part 2 (green tab only)
		2,					//  2 commands in list:
		ST77XX_CASET, 4,	//  1: Column addr set, 4 args, no delay:
		0x00, 0x02,			//     XSTART = 0
		0x00, 0x7F + 0x02,	//     XEND = 127
		ST77XX_RASET, 4,	//  2: Row addr set, 4 args, no delay:
		0x00, 0x01,			//     XSTART = 0
		0x00, 0x9F + 0x01}, //     XEND = 159

	Rcmd2red[] = {		 // 7735R init, part 2 (red tab only)
		2,				 //  2 commands in list:
		ST77XX_CASET, 4, //  1: Column addr set, 4 args, no delay:
		0x00, 0x00,		 //     XSTART = 0
		0x00, 0x7F,		 //     XEND = 127
		ST77XX_RASET, 4, //  2: Row addr set, 4 args, no delay:
		0x00, 0x00,		 //     XSTART = 0
		0x00, 0x9F},	 //     XEND = 159

	Rcmd2green144[] = {	 // 7735R init, part 2 (green 1.44 tab)
		2,				 //  2 commands in list:
		ST77XX_CASET, 4, //  1: Column addr set, 4 args, no delay:
		0x00, 0x00,		 //     XSTART = 0
		0x00, 0x7F,		 //     XEND = 127
		ST77XX_RASET, 4, //  2: Row addr set, 4 args, no delay:
		0x00, 0x00,		 //     XSTART = 0
		0x00, 0x7F},	 //     XEND = 127

	Rcmd2green160x80[] = { // 7735R init, part 2 (mini 160x80)
		2,				   //  2 commands in list:
		ST77XX_CASET, 4,   //  1: Column addr set, 4 args, no delay:
		0x00, 0x00,		   //     XSTART = 0
		0x00, 0x4F,		   //     XEND = 79
		ST77XX_RASET, 4,   //  2: Row addr set, 4 args, no delay:
		0x00, 0x00,		   //     XSTART = 0
		0x00, 0x9F},	   //     XEND = 159

	Rcmd3[] = {																		// 7735R init, part 3 (red or green tab)
		4,																			//  4 commands in list:
		ST7735_GMCTRP1, 16,															//  1: Gamma Adjustments (pos. polarity), 16 args + delay:
		0x02, 0x1c, 0x07, 0x12,														//     (Not entirely necessary, but provides
		0x37, 0x32, 0x29, 0x2d,														//      accurate colors)
		0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10, ST7735_GMCTRN1, 16,			//  2: Gamma Adjustments (neg. polarity), 16 args + delay:
		0x03, 0x1d, 0x07, 0x06,														//     (Not entirely necessary, but provides
		0x2E, 0x2C, 0x29, 0x2D,														//      accurate colors)
		0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10, ST77XX_NORON, ST_CMD_DELAY, //  3: Normal display on, no args, w/delay
		10,																			//     10 ms delay
		ST77XX_DISPON, ST_CMD_DELAY,												//  4: Main screen turn on, no args w/delay
		100};																		//    10 ms delay

#define spi_set_mosi(value) gpio_put(st7735_pinTX, value)

#define spi_pulse_sck()                \
    {                                  \
        asm("nop");                    \
        gpio_put(st7735_pinSCK, 1); \
        asm("nop");                    \
        gpio_put(st7735_pinSCK, 0); \
    }

void spi_tx_array(const uint8_t *data, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        uint8_t byte = data[i];
        for (int j = 7; j >= 0; j--)
        {
            spi_set_mosi((byte >> j) & 0x01);
            spi_pulse_sck();
        }
    }
}

void LCD_setPins(uint16_t dc, uint16_t cs, int16_t rst, uint16_t sck, uint16_t tx)
{
	st7735_pinDC = dc;
	st7735_pinCS = cs;
	st7735_pinRST = rst;
	st7735_pinSCK = sck;
	st7735_pinTX = tx;
}

void initSPI()
{
	// spi_init(st7735_spi, 1000 * 50000);
	// spi_set_format(st7735_spi, 16, SPI_CPOL_1, SPI_CPOL_1, SPI_MSB_FIRST);
	// gpio_set_function(st7735_pinTX, GPIO_FUNC_SPI);
	// gpio_set_function(st7735_pinSCK, GPIO_FUNC_SPI);

	gpio_init(st7735_pinTX);
    gpio_init(st7735_pinSCK);

	gpio_set_dir(st7735_pinTX, GPIO_OUT);
    gpio_set_dir(st7735_pinSCK, GPIO_OUT);

	gpio_init(st7735_pinCS);
	gpio_set_dir(st7735_pinCS, GPIO_OUT);
	gpio_put(st7735_pinCS, 1);

	gpio_init(st7735_pinDC);
	gpio_set_dir(st7735_pinDC, GPIO_OUT);
	gpio_put(st7735_pinDC, 1);

	if (st7735_pinRST != -1)
	{
		gpio_init(st7735_pinRST);
		gpio_set_dir(st7735_pinRST, GPIO_OUT);
		gpio_put(st7735_pinRST, 1);
	}
}

void ST7735_Reset()
{
	if (st7735_pinRST != -1)
	{
		gpio_put(st7735_pinRST, 0);
		sleep_ms(5);
		gpio_put(st7735_pinRST, 1);
	}
}

void ST7735_Select()
{
	gpio_put(st7735_pinCS, 0);
}

void ST7735_DeSelect()
{
	gpio_put(st7735_pinCS, 1);
}

void ST7735_RegCommand()
{
	gpio_put(st7735_pinDC, 0);
}

void ST7735_RegData()
{
	gpio_put(st7735_pinDC, 1);
}

void ST7735_WriteCommand(uint8_t cmd)
{
	ST7735_RegCommand();
	spi_tx_array(&cmd, sizeof(cmd));
}

void ST7735_WriteData(uint8_t *buff, size_t buff_size)
{
	ST7735_RegData();
	// spi_set_format(st7735_spi, 8, SPI_CPOL_1, SPI_CPOL_1, SPI_MSB_FIRST);
	// spi_write_blocking(st7735_spi, buff, buff_size);
	spi_tx_array(buff, buff_size);
}

void ST7735_SendCommand(uint8_t commandByte, uint8_t *dataBytes,
						uint8_t numDataBytes)
{
	ST7735_Select();

	ST7735_WriteCommand(commandByte);
	ST7735_WriteData(dataBytes, numDataBytes);

	ST7735_DeSelect();
}

void ST7735_displayInit(const uint8_t *addr)
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
		ST7735_SendCommand(cmd, addr, numArgs);
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

	// For ST7735 with GREEN TAB (including //  HALloWing)...
	if ((tabcolor == INITR_144GREENTAB) || (tabcolor == INITR_HALLOWING))
	{
		// ..._rowstart is 3 for rotations 0&1, 1 for rotations 2&3
		_rowstart = (rotation < 2) ? 3 : 1;
	}

	switch (rotation)
	{
	case 0:
		if ((tabcolor == INITR_BLACKTAB) || (tabcolor == INITR_MINI160x80))
		{
			madctl = ST77XX_MADCTL_MX | ST77XX_MADCTL_MY | ST77XX_MADCTL_RGB;
		}
		else
		{
			madctl = ST77XX_MADCTL_MX | ST77XX_MADCTL_MY | ST7735_MADCTL_BGR;
		}

		if (tabcolor == INITR_144GREENTAB)
		{
			_height = ST7735_TFTHEIGHT_128;
			_width = ST7735_TFTWIDTH_128;
		}
		else if (tabcolor == INITR_MINI160x80)
		{
			_height = ST7735_TFTHEIGHT_160;
			_width = ST7735_TFTWIDTH_80;
		}
		else
		{
			_height = ST7735_TFTHEIGHT_160;
			_width = ST7735_TFTWIDTH_128;
		}
		_xstart = _colstart;
		_ystart = _rowstart;
		break;
	case 1:
		if ((tabcolor == INITR_BLACKTAB) || (tabcolor == INITR_MINI160x80))
		{
			madctl = ST77XX_MADCTL_MY | ST77XX_MADCTL_MV | ST77XX_MADCTL_RGB;
		}
		else
		{
			madctl = ST77XX_MADCTL_MY | ST77XX_MADCTL_MV | ST7735_MADCTL_BGR;
		}

		if (tabcolor == INITR_144GREENTAB)
		{
			_width = ST7735_TFTHEIGHT_128;
			_height = ST7735_TFTWIDTH_128;
		}
		else if (tabcolor == INITR_MINI160x80)
		{
			_width = ST7735_TFTHEIGHT_160;
			_height = ST7735_TFTWIDTH_80;
		}
		else
		{
			_width = ST7735_TFTHEIGHT_160;
			_height = ST7735_TFTWIDTH_128;
		}
		_ystart = _colstart;
		_xstart = _rowstart;
		break;
	case 2:
		if ((tabcolor == INITR_BLACKTAB) || (tabcolor == INITR_MINI160x80))
		{
			madctl = ST77XX_MADCTL_RGB;
		}
		else
		{
			madctl = ST7735_MADCTL_BGR;
		}

		if (tabcolor == INITR_144GREENTAB)
		{
			_height = ST7735_TFTHEIGHT_128;
			_width = ST7735_TFTWIDTH_128;
		}
		else if (tabcolor == INITR_MINI160x80)
		{
			_height = ST7735_TFTHEIGHT_160;
			_width = ST7735_TFTWIDTH_80;
		}
		else
		{
			_height = ST7735_TFTHEIGHT_160;
			_width = ST7735_TFTWIDTH_128;
		}
		_xstart = _colstart;
		_ystart = _rowstart;
		break;
	case 3:
		if ((tabcolor == INITR_BLACKTAB) || (tabcolor == INITR_MINI160x80))
		{
			madctl = ST77XX_MADCTL_MX | ST77XX_MADCTL_MV | ST77XX_MADCTL_RGB;
		}
		else
		{
			madctl = ST77XX_MADCTL_MX | ST77XX_MADCTL_MV | ST7735_MADCTL_BGR;
		}

		if (tabcolor == INITR_144GREENTAB)
		{
			_width = ST7735_TFTHEIGHT_128;
			_height = ST7735_TFTWIDTH_128;
		}
		else if (tabcolor == INITR_MINI160x80)
		{
			_width = ST7735_TFTHEIGHT_160;
			_height = ST7735_TFTWIDTH_80;
		}
		else
		{
			_width = ST7735_TFTHEIGHT_160;
			_height = ST7735_TFTWIDTH_128;
		}
		_ystart = _colstart;
		_xstart = _rowstart;
		break;
	}
	ST7735_SendCommand(ST77XX_MADCTL, &madctl, 1);
}

void LCD_initDisplay(uint8_t options)
{

	initSPI();

	// windowWidth = width;
	// windowHeight = height;
	ST7735_Select();
	ST7735_Reset();
	ST7735_displayInit(Rcmd1);
	if (options == INITR_GREENTAB)
	{
		ST7735_displayInit(Rcmd2green);
		_colstart = 2;
		_rowstart = 1;
	}
	else if ((options == INITR_144GREENTAB) || (options == INITR_HALLOWING))
	{
		_height = ST7735_TFTHEIGHT_128;
		_width = ST7735_TFTWIDTH_128;
		ST7735_displayInit(Rcmd2green144);
		_colstart = 2;
		_rowstart = 3; // For default rotation 0
	}
	else if (options == INITR_MINI160x80)
	{
		_height = ST7735_TFTWIDTH_80;
		_width = ST7735_TFTHEIGHT_160;
		ST7735_displayInit(Rcmd2green160x80);
		_colstart = 24;
		_rowstart = 0;
	}
	else
	{
		// colstart, rowstart left at default '0' values
		ST7735_displayInit(Rcmd2red);
	}
	ST7735_displayInit(Rcmd3);

	// Black tab, change MADCTL color filter
	if ((options == INITR_BLACKTAB) || (options == INITR_MINI160x80))
	{
		uint8_t data = 0xC0;
		ST7735_SendCommand(ST77XX_MADCTL, &data, 1);
	}

	if (options == INITR_HALLOWING)
	{
		// //  HALlowing is simply a 1.44" green tab upside-down:
		tabcolor = INITR_144GREENTAB;
		LCD_setRotation(2);
	}
	else
	{
		tabcolor = options;
		LCD_setRotation(0);
	}

	// LCD_setRotation(2);
}

void ST7735_setAddrWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
	x += _xstart;
	y += _ystart;

	uint32_t xa = ((uint32_t)x << 16) | (x + w - 1);
	uint32_t ya = ((uint32_t)y << 16) | (y + h - 1);

	xa = __builtin_bswap32(xa);
	ya = __builtin_bswap32(ya);

	ST7735_WriteCommand(ST77XX_CASET);
	ST7735_WriteData(&xa, sizeof(xa));

	// row address set
	ST7735_WriteCommand(ST77XX_RASET);
	ST7735_WriteData(&ya, sizeof(ya));

	// write to RAM
	ST7735_WriteCommand(ST77XX_RAMWR);
}

void LCD_WriteBitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
	ST7735_Select();
	ST7735_setAddrWindow(x, y, w, h); // Clipped area
	ST7735_RegData();
	
	for (size_t i = 0; i < w * h; i++)
    {
        uint16_t byte = bitmap[i];
        for (int j = 15; j >= 0; j--)
        {
            spi_set_mosi((byte >> j) & 0x01);
            spi_pulse_sck();
        }
    }
	
	ST7735_DeSelect();
}

void LCD_WritePixel(int x, int y, uint16_t col)
{
	ST7735_Select();
	ST7735_setAddrWindow(x, y, 1, 1); // Clipped area
	ST7735_RegData();
	
	uint16_t byte = col;
	for (int j = 15; j >= 0; j--)
	{
		spi_set_mosi((byte >> j) & 0x01);
		spi_pulse_sck();
	}
	ST7735_DeSelect();
}