/*  GIMP header image file format (RGB): /home/vandy/EROS/eros/src/base/domain/winsys/pixmaps/horizontalborder.h  */

#define hori_width    100
#define hori_height     6
#define hori_depth     32

/*  Call this macro repeatedly.  After each use, the pixel data can be extracted  */
#ifndef HEADER_PIXEL
#define HEADER_PIXEL(data,pixel) {\
  pixel[0] = (((data[0] - 33) << 2) | ((data[1] - 33) >> 4)); \
  pixel[1] = ((((data[1] - 33) & 0xF) << 4) | ((data[2] - 33) >> 2)); \
  pixel[2] = ((((data[2] - 33) & 0x3) << 6) | ((data[3] - 33))); \
  data += 4; \
}
#endif

static uint8_t *horiborder_pixmap =
	"#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)"
	"#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)"
	"#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)"
	"#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)"
	"#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)"
	"#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)"
	"#!A)#!A)#!A)#!A)N;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OL"
	"N;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OL"
	"N;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OL"
	"N;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OL"
	"N;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OL"
	"N;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OL"
	"N;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OL[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;"
	"[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;"
	"[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;"
	"[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;"
	"[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;"
	"[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;"
	"[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[?H:Y_05ZO<8ZO<8ZO<8"
	"Z_@8ZO<8Z_@9[/D:Z_@9Z_@9[/D:Z_@9Z_@9[/D:Z_@9Z_@9[/D:Z_@9Z_@9ZO<9"
	"YO,6X^`4Y/$4Y/$4X^`4Y/$4Y/$4X^`4Y/$4Y/$4X^`4Y/$4Y/$4X^`4Y/$4Y/$4"
	"X^`4Y/$4Y/$4X^`4Y/$4Y/$4X^`4Y/$4Y/$4X^`4Y?(5Y?(5Y/$5Y?(5Y?(5Y/$5"
	"Y?(5Y?(5Y/$5Y?(5Y?(5Y/$5Y?(5Y?(5Y/$5Y?(5Y?(5Y/$5Y?(5Y?(5Y/$5Y?(5"
	"Y?(5Y/$5Y?(5Y?(5Y/$5Y?(5Y?(5Y/$5Y?(5Y?(5Y/$5Y?(5Y?(5Y/$5Y?(5Y?(5"
	"Y/$5Y?(5Y/$4X^`4Y/$4Y/$4X^`4Y/$4Y/$4X^`4Y/$4Y/$4X^`4Y/$4Y/$4Y/$4"
	"N;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OL"
	"N;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OL"
	"N;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OL"
	"N;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OL"
	"N;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OL"
	"N;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OL"
	"N;OLN;OLN;OLN;OL#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)"
	"#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)"
	"#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)"
	"#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)"
	"#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)"
	"#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)"
	"#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)";
