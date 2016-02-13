#ifndef N6100LCD_H
#define N6100LCD_H

#include <stdint.h>

class N6100LCD
{
public:

// 12-Bit Color
#define RGB444(r,g,b)    ( ((r&0xF0)<<4) | (g&0xF0) | (b>>4) )

#define WHITE       RGB444(255,255,255)
#define BLACK       RGB444(0,0,0)
#define RED         RGB444(255,0,0)
#define GREEN       RGB444(0,255,0)
#define BLUE        RGB444(0,0,255)
#define CYAN        RGB444(0,255,255)
#define MAGENTA     RGB444(255,0,255)
#define YELLOW      RGB444(255,255,0)
#define BROWN       RGB444(165,42,42)
#define ORANGE      RGB444(255,165,0)
#define PINK        RGB444(255,192,203)

    enum {
        TYPE_EPSON   = 0,
        TYPE_PHILIPS = 1
    };

    enum {
        ROW_LENGTH = 132,
        COL_HEIGHT = 132
    };

    N6100LCD(int reset_pin=25, char type=TYPE_PHILIPS, const char *dev="/dev/spidev0.0");
    int init(void);
    void clear(uint16_t color);
    void set_pixel(uint8_t x, uint8_t y, uint16_t color);
    void draw_hline(uint16_t color, uint8_t x, uint8_t y, uint8_t width);
    void draw_hline(uint16_t *buff, uint8_t x, uint8_t y, uint8_t width);

    void set_area(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);

private:
    // EPSON controller
    enum {
        DISON       = 0xAF,
        DISOFF      = 0xAE,
        DISNOR      = 0xA6,
        DISINV      = 0xA7,
        SLPIN       = 0x95,
        SLPOUT      = 0x94,
        COMSCN      = 0xBB,
        DISCTL      = 0xCA,
        PASET       = 0x75,
        CASET       = 0x15,
        DATCTL      = 0xBC,
        RGBSET8     = 0xCE,
        RAMWR       = 0x5C,
        RAMRD       = 0x5D,
        PTLIN       = 0xA8,
        PTLOUT      = 0xA9,
        RMWIN       = 0xE0,
        RMWOUT      = 0xEE,
        ASCSET      = 0xAA,
        SCSTART     = 0xAB,
        OSCON       = 0xD1,
        OSCOFF      = 0xD2,
        PWRCTR      = 0x20,
        VOLCTR      = 0x81,
        VOLUP       = 0xD6,
        VOLDOWN     = 0xD7,
        TMPGRD      = 0x82,
        EPCTIN      = 0xCD,
        EPCOUT      = 0xCC,
        EPMWR       = 0xFC,
        EPMRD       = 0xFD,
        EPSRRD1     = 0x7C,
        EPSRRD2     = 0x7D,
        NOP         = 0x25
    };

    // PHILLIPS controller
    enum {
        NOPP        = 0x00,
        BSTRON      = 0x03,
        SLEEPIN     = 0x10,
        SLEEPOUT    = 0x11,
        NORON       = 0x13,
        INVOFF      = 0x20,
        INVON       = 0x21,
        SETCON      = 0x25,
        DISPOFF     = 0x28,
        DISPON      = 0x29,
        CASETP      = 0x2A,
        PASETP      = 0x2B,
        RAMWRP      = 0x2C,
        RGBSET      = 0x2D,
        MADCTL      = 0x36,
        COLMOD      = 0x3A,
        DISCTR      = 0xB9,
        EC          = 0xC0,
    };

    const int rst_;
    const char type_;
    const char *dev_;

    volatile uint32_t *gpio_;
    int spi_;

    enum {
        SPI_BUFF_SIZE = 64,
    };

    uint16_t spibuff_[SPI_BUFF_SIZE];
    uint16_t buffsz;
    uint16_t *pbuff;

    int gpio_init(void);
    int spi_init(void); // https://github.com/msperl/spi-bcm2835/wiki

    void spi_word(uint16_t word);
    int  spi_flush(void);
};

#endif // N6100LCD_H
