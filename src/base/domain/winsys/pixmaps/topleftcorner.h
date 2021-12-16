/*  GIMP header image file format (RGB): /home/vandy/EROS/eros/src/base/domain/winsys/pixmaps/topleftcorner.h  */

#ifndef corner_width
 #define corner_width  6
#endif

#ifndef corner_height
  #define corner_height  6
#endif

#ifndef corner_depth
  #define corner_depth  32
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

static uint8_t *topleftcorner_pixmap =
	"#!A)#!A)#!A)#!A)#!A)#!A)#!A)N;OLN;OLN;OLN;OLN;OL#!A)N;OLX^`4X^`4"
	"X^`4X^`4#!A)N;OLX^`4[OL;[OL;[OL;#!A)N;OLX^`4[OL;N;OLN;OL#!A)N;OL"
	"X^`4[OL;N;OL#!A)";
