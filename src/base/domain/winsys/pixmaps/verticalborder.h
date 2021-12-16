/*  GIMP header image file format (RGB): /home/vandy/EROS/eros/src/base/domain/winsys/pixmaps/verticalborder.h  */

#define vert_width    6
#define vert_height 100
#define vert_depth   32
/*  Call this macro repeatedly.  After each use, the pixel data can be extracted  */
#ifndef HEADER_PIXEL
#define HEADER_PIXEL(data,pixel) {\
  pixel[0] = (((data[0] - 33) << 2) | ((data[1] - 33) >> 4)); \
  pixel[1] = ((((data[1] - 33) & 0xF) << 4) | ((data[2] - 33) >> 2)); \
  pixel[2] = ((((data[2] - 33) & 0x3) << 6) | ((data[3] - 33))); \
  data += 4; \
}
#endif

static uint8_t *vertborder_pixmap =
	"#!A)N;OL[?H:Y/$4N;OL#!A)#!A)N;OL[OL;Y/$4N;OL#!A)#!A)N;OL[OL;Y/$4"
	"N;OL#!A)#!A)N;OL[OL;X^`4N;OL#!A)#!A)N;OL[OL;Y/$4N;OL#!A)#!A)N;OL"
	"[OL;Y/$4N;OL#!A)#!A)N;OL[OL;X^`4N;OL#!A)#!A)N;OL[OL;Y/$4N;OL#!A)"
	"#!A)N;OL[OL;Y/$4N;OL#!A)#!A)N;OL[OL;X^`4N;OL#!A)#!A)N;OL[OL;Y/$4"
	"N;OL#!A)#!A)N;OL[OL;Y/$4N;OL#!A)#!A)N;OL[OL;X^`4N;OL#!A)#!A)N;OL"
	"[OL;Y/$4N;OL#!A)#!A)N;OL[OL;Y?(5N;OL#!A)#!A)N;OL[OL;Y/$5N;OL#!A)"
	"#!A)N;OL[OL;Y?(5N;OL#!A)#!A)N;OL[OL;Y?(5N;OL#!A)#!A)N;OL[OL;Y/$5"
	"N;OL#!A)#!A)N;OL[OL;Y?(5N;OL#!A)#!A)N;OL[OL;Y?(5N;OL#!A)#!A)N;OL"
	"[OL;Y/$5N;OL#!A)#!A)N;OL[OL;Y?(5N;OL#!A)#!A)N;OL[OL;Y?(5N;OL#!A)"
	"#!A)N;OL[OL;Y/$5N;OL#!A)#!A)N;OL[OL;Y?(5N;OL#!A)#!A)N;OL[OL;Y?(5"
	"N;OL#!A)#!A)N;OL[OL;Y/$5N;OL#!A)#!A)N;OL[OL;Y?(5N;OL#!A)#!A)N;OL"
	"[OL;Y?(5N;OL#!A)#!A)N;OL[OL;Y/$5N;OL#!A)#!A)N;OL[OL;Y?(5N;OL#!A)"
	"#!A)N;OL[OL;Y?(5N;OL#!A)#!A)N;OL[OL;Y/$5N;OL#!A)#!A)N;OL[OL;Y?(5"
	"N;OL#!A)#!A)N;OL[OL;Y?(5N;OL#!A)#!A)N;OL[OL;Y/$5N;OL#!A)#!A)N;OL"
	"[OL;Y?(5N;OL#!A)#!A)N;OL[OL;Y?(5N;OL#!A)#!A)N;OL[OL;Y/$5N;OL#!A)"
	"#!A)N;OL[OL;Y?(5N;OL#!A)#!A)N;OL[OL;Y?(5N;OL#!A)#!A)N;OL[OL;Y/$5"
	"N;OL#!A)#!A)N;OL[OL;Y?(5N;OL#!A)#!A)N;OL[OL;Y?(5N;OL#!A)#!A)N;OL"
	"[OL;Y/$5N;OL#!A)#!A)N;OL[OL;Y?(5N;OL#!A)#!A)N;OL[OL;Y?(5N;OL#!A)"
	"#!A)N;OL[OL;Y/$5N;OL#!A)#!A)N;OL[OL;Y?(5N;OL#!A)#!A)N;OL[OL;Y?(5"
	"N;OL#!A)#!A)N;OL[OL;Y/$5N;OL#!A)#!A)N;OL[OL;Y?(5N;OL#!A)#!A)N;OL"
	"[OL;Y?(5N;OL#!A)#!A)N;OL[OL;X^`4N;OL#!A)#!A)N;OL[OL;Y/$4N;OL#!A)"
	"#!A)N;OL[OL;Y/$4N;OL#!A)#!A)N;OL[OL;X^`4N;OL#!A)#!A)N;OL[OL;Y/$4"
	"N;OL#!A)#!A)N;OL[OL;Y/$4N;OL#!A)#!A)N;OL[OL;X^`4N;OL#!A)#!A)N;OL"
	"[OL;Y/$4N;OL#!A)#!A)N;OL[OL;Y/$4N;OL#!A)#!A)N;OL[OL;X^`4N;OL#!A)"
	"#!A)N;OL[OL;Y/$4N;OL#!A)#!A)N;OL[OL;Y/$4N;OL#!A)#!A)N;OL[OL;X^`4"
	"N;OL#!A)#!A)N;OL[OL;Y/$4N;OL#!A)#!A)N;OL[OL;Y/$4N;OL#!A)#!A)N;OL"
	"[OL;X^`4N;OL#!A)#!A)N;OL[OL;Y/$4N;OL#!A)#!A)N;OL[OL;Y/$4N;OL#!A)"
	"#!A)N;OL[OL;X^`4N;OL#!A)#!A)N;OL[OL;Y/$4N;OL#!A)#!A)N;OL[OL;Y/$4"
	"N;OL#!A)#!A)N;OL[OL;X^`4N;OL#!A)#!A)N;OL[OL;Y/$4N;OL#!A)#!A)N;OL"
	"[OL;Y/$4N;OL#!A)#!A)N;OL[OL;X^`4N;OL#!A)#!A)N;OL[OL;YO,6N;OL#!A)"
	"#!A)N;OL[OL;ZO<9N;OL#!A)#!A)N;OL[OL;Z_@9N;OL#!A)#!A)N;OL[OL;Z_@9"
	"N;OL#!A)#!A)N;OL[OL;[/D:N;OL#!A)#!A)N;OL[OL;Z_@9N;OL#!A)#!A)N;OL"
	"[OL;Z_@9N;OL#!A)#!A)N;OL[OL;[/D:N;OL#!A)#!A)N;OL[OL;Z_@9N;OL#!A)"
	"#!A)N;OL[OL;Z_@9N;OL#!A)#!A)N;OL[OL;[/D:N;OL#!A)#!A)N;OL[OL;Z_@9"
	"N;OL#!A)#!A)N;OL[OL;Z_@9N;OL#!A)#!A)N;OL[OL;[/D:N;OL#!A)#!A)N;OL"
	"[OL;Z_@9N;OL#!A)#!A)N;OL[OL;ZO<8N;OL#!A)#!A)N;OL[OL;Z_@8N;OL#!A)"
	"#!A)N;OL[OL;ZO<8N;OL#!A)#!A)N;OL[OL;ZO<8N;OL#!A)#!A)N;OL[OL;ZO<8"
	"N;OL#!A)#!A)N;OL[OL;Y_05N;OL#!A)";
