#include "n6100lcd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

//#define DBG(mgs,...)
#define DBG(msg,...)    printf(msg, ##__VA_ARGS__)


//#define BCM2708_PERI_BASE_DEFAULT   0x20000000
#define BCM2709_PERI_BASE_DEFAULT   0x3f000000 // pi 2
#define GPIO_BASE_OFFSET            0x200000

#define GPIO_BASE   (BCM2709_PERI_BASE_DEFAULT + GPIO_BASE_OFFSET)

#define PAGE_SIZE  (4*1024)
#define BLOCK_SIZE (4*1024)

// GPIO macros
#define INP_GPIO(g) *(gpio_ + ((g) / 10)) &= ~(7 << (((g) % 10) * 3))
#define OUT_GPIO(g) *(gpio_ + ((g) / 10)) |=  (1 << (((g) % 10) * 3))

#define GPIO_SET(g) (*(gpio_ + 7)  = (1<<g))
#define GPIO_CLR(g) (*(gpio_ + 10) = (1<<g))


#define SPI_BITSPERWORD (9)
#define MAX_SPI_SPEED   (8 * 1000 * 1000)
//#define MAX_SPI_SPEED   (500 * 1000)

#define spi_cmd(b)  spi_word((b)&0x0FF)
#define spi_data(b) spi_word(((b)&0xFF)|0x100)

N6100LCD::N6100LCD(int reset_pin, char type, const char *dev)
    : rst_(reset_pin), type_(type), dev_(dev),
      gpio_((uint32_t *)MAP_FAILED), spi_(-1),
      buffsz(0), pbuff(spibuff_)
{

}

int N6100LCD::init(void)
{
    if (MAP_FAILED==gpio_ && gpio_init() < 0) {
        DBG("gpio init failed\n");
        return -1;
    }
    if (spi_ < 0 && spi_init() < 0) {
        DBG("spi init failed\n");
        return -1;
    }

    DBG("gpio = %X\n", gpio_);
    DBG("spi  = %d\n", spi_);

    INP_GPIO(rst_);
    OUT_GPIO(rst_);

    GPIO_CLR(rst_);
    usleep(200*1000);
    GPIO_SET(rst_);
    usleep(200*1000);

    if (type_ == TYPE_EPSON) {
        spi_cmd(DISCTL); // Display control (0xCA)
        spi_data(0x0C);  // 12 = 1100 - CL dividing ratio [don't divide] switching period 8H (default)
        spi_data(0x20);  // nlines/4 - 1 = 132/4 - 1 = 32 duty
        spi_data(0x00);  // No inversely highlighted lines

        spi_cmd(COMSCN); // common scanning direction (0xBB)
        spi_data(0x01);  // 1->68, 132<-69 scan direction

        spi_cmd(OSCON); // internal oscialltor ON (0xD1)
        spi_cmd(SLPOUT); // sleep out (0x94)

        spi_cmd(PWRCTR); // power ctrl (0x20)
        spi_data(0x0F);  // everything on, no external reference resistors

        spi_cmd(DISINV); // invert display mode (0xA7)

        spi_cmd(DATCTL); // data control (0xBC)
        spi_data(0x03);  // Inverse page address, reverse rotation column address, column scan-direction !!! try 0x01
        spi_data(0x00);  // normal RGB arrangement
        spi_data(0x02);  // 16-bit Grayscale Type A (12-bit color)

        spi_cmd(VOLCTR); // electronic volume, this is the contrast/brightness (0x81)
        spi_data(32);  // volume (contrast) setting - fine tuning, original (0-63)
        spi_data(3);   // internal resistor ratio - coarse adjustment (0-7)

        spi_cmd(NOP);
        usleep(100 * 1000);
        spi_cmd(DISON); // display on (0xAF)
      } else if (type_ == TYPE_PHILIPS) {
        spi_cmd(SLEEPOUT); // Sleep Out (0x11)
        spi_cmd(BSTRON);   // Booster voltage on (0x03)
        spi_cmd(DISPON);   // Display on (0x29)

        //spi_cmd(INVON);    // Inversion on (0x20)

        // 12-bit color pixel format:
        spi_cmd(COLMOD);   // Color interface format (0x3A)
        spi_data(0x03);        // 0b011 is 12-bit/pixel mode

        spi_cmd(MADCTL);   // Memory Access Control(PHILLIPS)
        //spi_data(0x08);
        spi_data(0xC0);

        spi_cmd(SETCON);   // Set Contrast(PHILLIPS)
        spi_data(0x3E);

        spi_cmd(NOPP);     // nop(PHILLIPS)
      }

    spi_flush();
    DBG("lcd init ok\n");
    return 0;
}

int N6100LCD::gpio_init(void)
{
    int mem_fd = open("/dev/mem", O_RDWR|O_SYNC);
    if (mem_fd < 0) {
        DBG("failed to open /dev/mem\n");
        return -1;
    }

    uint8_t *gpio_mem = (uint8_t *)malloc(BLOCK_SIZE + (PAGE_SIZE-1));
    if (NULL == gpio_mem) {
        DBG("failed to alloc gpio mem\n");
        return -1;
    }

    if ((uint32_t)gpio_mem % PAGE_SIZE) {
        //DBG("make gpio mem aligned\n");
        gpio_mem += PAGE_SIZE - ((uint32_t)gpio_mem % PAGE_SIZE);
    }

    void *gpio_map = mmap((void *)gpio_mem,
                             BLOCK_SIZE,
                             PROT_READ|PROT_WRITE,
                             MAP_SHARED|MAP_FIXED,
                             mem_fd,
                             GPIO_BASE);

    close(mem_fd);
    if (MAP_FAILED == gpio_map) {
        DBG("failed to nmap\n");
        return -1;
    }
    gpio_ = (volatile uint32_t *)gpio_map;

    return 0;
}

int N6100LCD::spi_init(void)
{
    int fd = open(dev_, O_RDWR);
    if (fd < 0) {
        DBG("open spi dev failed\n");
        return -1;
    }

    uint8_t  mode      = SPI_MODE_2;
    uint8_t  lsb_first = 0;
    uint8_t  bpw       = SPI_BITSPERWORD;
    uint32_t speed     = MAX_SPI_SPEED;

    if (ioctl(fd, SPI_IOC_WR_MODE         , &mode     ) < 0) return -1;
    if (ioctl(fd, SPI_IOC_WR_LSB_FIRST    , &lsb_first) < 0) return -1;
    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bpw      ) < 0) return -1;
    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ , &speed    ) < 0) return -1;

    spi_ = fd;
    return 0;
}

void N6100LCD::spi_word(uint16_t word)
{
    *pbuff++ = word;
    if(++buffsz >= SPI_BUFF_SIZE) spi_flush();
}

int N6100LCD::spi_flush(void)
{
    if (spi_<1 || buffsz<1) return -1;

    struct spi_ioc_transfer msg;
    memset((void*)&msg, 0, sizeof(msg));

    msg.tx_buf     = (unsigned long)spibuff_;
    msg.rx_buf     = (unsigned long)NULL ;
    msg.len        = buffsz * sizeof(uint16_t);
    //msg.cs_change  = true;

    pbuff  = spibuff_;
    buffsz = 0;

    return ioctl(spi_, SPI_IOC_MESSAGE(1), &msg);
}


void N6100LCD::set_area(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)
{
    uint8_t paset, caset, ramwr;

    if (type_ == TYPE_EPSON) {
        paset = PASET;
        caset = CASET;
        ramwr = RAMWR;
    } else {
        paset = PASETP;
        caset = CASETP;
        ramwr = RAMWRP;
    }

    // rows
    spi_cmd(paset);
    spi_data(y1);
    spi_data(y2);

    // columns
    spi_cmd(caset);
    spi_data(x1);
    spi_data(x2);

    // start write
    spi_cmd(ramwr);
}

void N6100LCD::clear(uint16_t color)
{
    for(int i = 0; i < COL_HEIGHT; i++) {
        draw_hline(color, 0, i, ROW_LENGTH);
    }
}

void N6100LCD::set_pixel(uint8_t x, uint8_t y, uint16_t color)
{
    set_area(x, y, x, y);

    spi_data((color >> 4) & 0xFF);
    spi_data(((color & 0x0F) << 4));
    spi_cmd(NOP);

    spi_flush();
}

void N6100LCD::draw_hline(uint16_t c, uint8_t x, uint8_t y, uint8_t width)
{
    if (width<1 || width>ROW_LENGTH) return;
    set_area(x, y, x+width-1, y);

    for (uint8_t i=0; i<width; ) {
        i += 2;
        spi_data(( c&0xFF0)>>4);                 // R1G1
        spi_data(((c&0x00F)<<4)|((c&0xF00)>>8)); // B1R2
        spi_data(  c&0x0FF );                    // G2B2
    }

    spi_flush();
}

void N6100LCD::draw_hline(uint16_t *buff, uint8_t x, uint8_t y, uint8_t width)
{
    if (width<1 || width>ROW_LENGTH) return;
    set_area(x, y, x+width-1, y);

    for (uint8_t i=0; i<width; ) {
        uint16_t c1 = *buff++;
        uint16_t c2 = *buff++;
        i += 2;

        spi_data(( c1&0xFF0)>>4);                  // R1G1
        spi_data(((c1&0x00F)<<4)|((c2&0xF00)>>8)); // B1R2
        spi_data(  c2&0x0FF );                     // G2B2
    }

    spi_flush();
}

