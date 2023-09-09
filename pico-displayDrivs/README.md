# pico-displayDrivs
*Display driver library for RP2040 pico-sdk* \
Example code [here](https://github.com/tvlad1234/pico-st7735Example)
## Supported display controllers:
ST7735 and ST7739 in SPI mode \
ILI9341 in SPI mode

## Library usage
Add the *pico-displayDrivs* subdirectory to the CMakeLists.txt of your project and include the needed libraries.

### Display driver usage:
Check the readme of each driver in its corresponding folder.

### GFX Library usage:
This package includes a graphics library, based on [Adafruit-GFX-Library](https://github.com/adafruit/Adafruit-GFX-Library).
It supports drawing basic shapes, characters and using custom fonts.

Note that the `gfx.h` header contains a convenience macro `GFX_RGB565(R, G, B)` to create a 16-bit 'rgb565' colour value from individual 8-bit components.

### GFX Framebuffer
By default, the GFX library writes pixels directly to the screen. If desired, an internal framebuffer can be used (which is recomended in cases where speed is desired). The framebuffer is created using `GFX_createFramebuf()`, which automatically tells the library to write to the framebuffer. The buffer can then be pushed to the screen by calling `GFX_flush()`. If needed, the buffer can be destroyed by calling `GFX_destroyFramebuf()`. Doing so will revert to writing pixels directly to the screen.
## GFX Library Reference
`GFX_drawPixel(int16_t x, int16_t y, uint16_t color);` draws a single pixel
### 
`GFX_drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color,
                          uint16_t bg, uint8_t size_x, uint8_t size_y);` puts a single character on screen\
`GFX_write(uint8_t c);` writes a character to the screen, handling the cursor position and text wrapping automatically\
`GFX_setCursor(int16_t x, int16_t y);` places the text cursor at the specified coordinates\
`GFX_setTextColor(uint16_t color);` sets the text color\
`GFX_setTextBack(uint16_t color);` sets the text background color\
`GFX_setFont(const GFXfont *f);`  sets the used font, using the same format as [Adafruit-GFX-Library](https://github.com/adafruit/Adafruit-GFX-Library) \
`GFX_printf` prints formatted text
###
`GFX_fillScreen(uint16_t color);` fills the screen with a specified color\
`GFX_setClearColor(uint16_t color);` sets the color the screen should be cleared with\
`GFX_clearScreen();` clears the screen, filling it with the color specified using the function above
###
`GFX_drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);` draws a line from (x0,y0) to (x1,y1)\
`GFX_drawFastHLine(int16_t x, int16_t y, int16_t l, uint16_t color);` draws a horizontal line\
`GFX_drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);` draws a vertical line
###
`GFX_drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);` draws a rectangle\
`GFX_drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color)` draws a circle\
`GFX_fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);` draws a filled rectangle\
`GFX_fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);` draws a filled circle
