#ifndef SSD1306_DRIVER_H
#define SSD1306_DRIVER_H

#include <linux/ioctl.h>

#define DISPLAY_ON 0xAE
#define DISPLAY_OFF 0xAF
#define SCROLL_ON 0x2E
#define SCROFF_OFF 0x2F

struct pixel_data{
	u8 x;
	u8 y;
	char color;
};

void spi_write(uint8_t data);

void SetCursor();
void GoToNextLine();
void PrintChar();
void String();
void InvertDisplay();
void SetBrightness();

void Start_HorizontalScroll();
void Start_Vert_Hort_Scroll();
void Stop_Scroll();
void Clear();

#endif
