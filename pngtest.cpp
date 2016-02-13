
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <png.h>
#include "n6100lcd.h"

int show_png(const char* file_name, N6100LCD *plcd)
{
    uint8_t header[8];    // 8 is the maximum size that can be checked

    /* open file and test for it being a png */
    FILE *fp = fopen(file_name, "rb");
    if (!fp) {
        return -1;
    }

    fread(header, 1, 8, fp);
    if (png_sig_cmp(header, 0, 8)) {
        fclose(fp);
        return -1; // not png file
    }

    /* initialize stuff */
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info_ptr = png_create_info_struct(png_ptr);

    setjmp(png_jmpbuf(png_ptr));

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8);

    png_read_info(png_ptr, info_ptr);

    int width = png_get_image_width(png_ptr, info_ptr);
    int height = png_get_image_height(png_ptr, info_ptr);

    png_set_interlace_handling(png_ptr);
    png_read_update_info(png_ptr, info_ptr);


    /* read file */
    setjmp(png_jmpbuf(png_ptr));
    png_bytep *row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * height);
    for (int y=0; y<height; y++)
            row_pointers[y] = (png_byte*) malloc(png_get_rowbytes(png_ptr,info_ptr));

    png_read_image(png_ptr, row_pointers);

    printf("read %s, width = %d, height = %d\n", file_name, width, height);

    uint16_t hlinebuff[132];
    for (int y=0; y<height; y++) {
        png_byte* row = row_pointers[y];
        uint16_t *p = hlinebuff;
        for (int x=0; x<width; x++) {
            png_byte* ptr = &(row[x*4]);

            *p++ = RGB444(ptr[0], ptr[1], ptr[2]);
        }
        plcd->draw_hline(hlinebuff,0,y,132);
    }

    fclose(fp);
    return 0;
}

int main(void)
{
    N6100LCD lcd;
    lcd.init();
    lcd.clear(GREEN);
    sleep(1);

    show_png("/home/pi/n6100lcd/lab.png", &lcd);

    return 0;
}
