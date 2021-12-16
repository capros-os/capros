/*  GIMP header image file format (RGB): /home/vandy/EROS/eros/src/base/domain/winsys/pixmaps/bottomrightcorner.h  */

#ifndef bottomcorner_width
  #define bottomcorner_width  20
#endif

#ifndef bottomcorner_height
  #define bottomcorner_height 20
#endif

#ifndef bottomcorner_depth
  #define bottomcorner_depth  32
#endif

/*  Call this macro repeatedly.  After each use, the pixel data can be extracted  */
#ifndef HEADER_PIXEL
#define HEADER_PIXEL(data,pixel) {\
  pixel[0] = (((data[0] - 33) << 2) | ((data[1] - 33) >> 4)); \
  pixel[1] = ((((data[1] - 33) & 0xF) << 4) | ((data[2] - 33) >> 2)); \
  pixel[2] = ((((data[2] - 33) & 0x3) << 6) | ((data[3] - 33))); \
  data += 4; \
}
#endif

static uint8_t *bottomrightcorner_pixmap =
	"#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)TMD)"
	"[OL;X^`4N;OL#!A)N;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OL"
	"N;OLN;OLN;OLTMD)[OL;X^`4N;OL#!A)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)"
	"TMD)TMD)TMD)TMD)TMD)TMD)TMD):G:G[OL;X^`4N;OL#!A)TMD)TMD)TMD)TMD)"
	"TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD):G:G````[OL;X^`4N;OL#!A)"
	"TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD):G:G````TMD)"
	"[OL;X^`4N;OL#!A)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)"
	":G:G````TMD)TMD)[OL;X^`4N;OL#!A)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)"
	"TMD)TMD)TMD):G:G````TMD)TMD):G:G[OL;X^`4N;OL#!A)TMD)TMD)TMD)TMD)"
	"TMD)TMD)TMD)TMD)TMD)TMD):G:G````TMD)TMD):G:G````[OL;X^`4N;OL#!A)"
	"TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD):G:G````TMD)TMD):G:G````TMD)"
	"[OL;X^`4N;OL#!A)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD):G:G````TMD)TMD)"
	":G:G````TMD)TMD)[OL;X^`4N;OL#!A)TMD)TMD)TMD)TMD)TMD)TMD)TMD):G:G"
	"````TMD)TMD):G:G````TMD)TMD):G:G[OL;X^`4N;OL#!A)TMD)TMD)TMD)TMD)"
	"TMD)TMD):G:G````TMD)TMD):G:G````TMD)TMD):G:G````[OL;X^`4N;OL#!A)"
	"TMD)TMD)TMD)TMD)TMD):G:G````TMD)TMD):G:G````TMD)TMD):G:G````TMD)"
	"[OL;X^`4N;OL#!A)TMD)TMD)TMD)TMD):G:G````TMD)TMD):G:G````TMD)TMD)"
	":G:G````TMD)TMD)[OL;X^`4N;OL#!A)TMD)TMD)TMD):G:G````TMD)TMD):G:G"
	"````TMD)TMD):G:G````TMD)TMD):G:G[OL;X^`4N;OL#!A)TMD)TMD):G:G````"
	"TMD)TMD):G:G````TMD)TMD):G:G````TMD)TMD):G:G````[OL;X^`4N;OL#!A)"
	"[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;"
	"[OL;X^`4N;OL#!A)X^`4X^`4X^`4X^`4X^`4X^`4X^`4X^`4X^`4X^`4X^`4X^`4"
	"X^`4X^`4X^`4X^`4X^`4X^`4N;OL#!A)N;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OL"
	"N;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OL#!A)#!A)#!A)#!A)#!A)"
	"#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)"
	"";
