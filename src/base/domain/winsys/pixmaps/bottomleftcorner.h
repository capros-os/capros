/*  GIMP header image file format (RGB): /home/vandy/EROS/eros/src/base/domain/winsys/pixmaps/bottomleftcorner.h  */
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

static uint8_t *bottomleftcorner_pixmap =
	"#!A)N;OLX^`4[OL;TMD)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)"
	"#!A)#!A)#!A)#!A)#!A)N;OLX^`4[OL;TMD)N;OLN;OLN;OLN;OLN;OLN;OLN;OL"
	"N;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OL#!A)N;OLX^`4[OL;:G:GTMD)TMD)TMD)"
	"TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)#!A)N;OLX^`4[OL;"
	"````:G:GTMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)"
	"#!A)N;OLX^`4[OL;TMD)````:G:GTMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)"
	"TMD)TMD)TMD)TMD)#!A)N;OLX^`4[OL;TMD)TMD)````:G:GTMD)TMD)TMD)TMD)"
	"TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)#!A)N;OLX^`4[OL;:G:GTMD)TMD)````"
	":G:GTMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)#!A)N;OLX^`4[OL;"
	"````:G:GTMD)TMD)````:G:GTMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)"
	"#!A)N;OLX^`4[OL;TMD)````:G:GTMD)TMD)````:G:GTMD)TMD)TMD)TMD)TMD)"
	"TMD)TMD)TMD)TMD)#!A)N;OLX^`4[OL;TMD)TMD)````:G:GTMD)TMD)````:G:G"
	"TMD)TMD)TMD)TMD)TMD)TMD)TMD)TMD)#!A)N;OLX^`4[OL;:G:GTMD)TMD)````"
	":G:GTMD)TMD)````:G:GTMD)TMD)TMD)TMD)TMD)TMD)TMD)#!A)N;OLX^`4[OL;"
	"````:G:GTMD)TMD)````:G:GTMD)TMD)````:G:GTMD)TMD)TMD)TMD)TMD)TMD)"
	"#!A)N;OLX^`4[OL;TMD)````:G:GTMD)TMD)````:G:GTMD)TMD)````:G:GTMD)"
	"TMD)TMD)TMD)TMD)#!A)N;OLX^`4[OL;TMD)TMD)````:G:GTMD)TMD)````:G:G"
	"TMD)TMD)````:G:GTMD)TMD)TMD)TMD)#!A)N;OLX^`4[OL;:G:GTMD)TMD)````"
	":G:GTMD)TMD)````:G:GTMD)TMD)````:G:GTMD)TMD)TMD)#!A)N;OLX^`4[OL;"
	"````:G:GTMD)TMD)````:G:GTMD)TMD)````:G:GTMD)TMD)````:G:GTMD)TMD)"
	"#!A)N;OLX^`4[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;[OL;"
	"[OL;[OL;[OL;[OL;#!A)N;OLX^`4X^`4X^`4X^`4X^`4X^`4X^`4X^`4X^`4X^`4"
	"X^`4X^`4X^`4X^`4X^`4X^`4X^`4X^`4#!A)N;OLN;OLN;OLN;OLN;OLN;OLN;OL"
	"N;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OLN;OL#!A)#!A)#!A)#!A)"
	"#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)"
	"";
