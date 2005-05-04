/*  GIMP header image file format (RGB): /home/vandy/EROS/eros/src/base/domain/winsys/pixmaps/killbutton.h  */

#ifndef kill_width
#define kill_width   20
#endif

#ifndef kill_height
#define kill_height  20
#endif

#ifndef kill_depth
#define kill_depth   32
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

static uint8_t *kill_in_pixmap =
	";WNL;WNL;WNL;WNL;WNL;WNL;WNL;WNL;WNL;WNL;WNL;WNL;WNL;WNL;WNL;WNL"
	";WNL;WNL;WNL;WNL;WNLB)3%B)3%B)3%B)3%B)3%B)3%B)3%B)3%B)3%B)3%B)3%"
	"B)3%B)3%B)3%B)3%B)3%B)3%B)3%````;WNLB)3%;WNLB)3%B)3%B)3%B)3%B)3%"
	"B)3%B)3%B)3%B)3%B)3%B)3%B)3%B)3%B)3%;WNLB)3%````;WNLB)3%````;WNL"
	"B)3%B)3%B)3%B)3%B)3%B)3%B)3%B)3%B)3%B)3%B)3%B)3%;WNL````B)3%````"
	";WNLB)3%B)3%````;WNLB)3%B)3%B)3%B)3%B)3%B)3%B)3%B)3%B)3%B)3%;WNL"
	"````B)3%B)3%````;WNLB)3%B)3%B)3%````;WNLB)3%B)3%B)3%B)3%B)3%B)3%"
	"B)3%B)3%;WNL````B)3%B)3%B)3%````;WNLB)3%B)3%B)3%B)3%````;WNLB)3%"
	"B)3%B)3%B)3%B)3%B)3%;WNL````B)3%B)3%B)3%B)3%````;WNLB)3%B)3%B)3%"
	"B)3%B)3%````;WNLB)3%B)3%B)3%B)3%;WNL````B)3%B)3%B)3%B)3%B)3%````"
	";WNLB)3%B)3%B)3%B)3%B)3%B)3%````;WNLB)3%B)3%;WNL````B)3%B)3%B)3%"
	"B)3%B)3%B)3%````;WNLB)3%B)3%B)3%B)3%B)3%B)3%B)3%````B)3%;WNL````"
	"B)3%B)3%B)3%B)3%B)3%B)3%B)3%````;WNLB)3%B)3%B)3%B)3%B)3%B)3%B)3%"
	"B)3%;WNLB)3%B)3%B)3%B)3%B)3%B)3%B)3%B)3%B)3%````;WNLB)3%B)3%B)3%"
	"B)3%B)3%B)3%B)3%;WNL````B)3%;WNLB)3%B)3%B)3%B)3%B)3%B)3%B)3%````"
	";WNLB)3%B)3%B)3%B)3%B)3%B)3%;WNL````B)3%B)3%````;WNLB)3%B)3%B)3%"
	"B)3%B)3%B)3%````;WNLB)3%B)3%B)3%B)3%B)3%;WNL````B)3%B)3%B)3%B)3%"
	"````;WNLB)3%B)3%B)3%B)3%B)3%````;WNLB)3%B)3%B)3%B)3%;WNL````B)3%"
	"B)3%B)3%B)3%B)3%B)3%````;WNLB)3%B)3%B)3%B)3%````;WNLB)3%B)3%B)3%"
	";WNL````B)3%B)3%B)3%B)3%B)3%B)3%B)3%B)3%````;WNLB)3%B)3%B)3%````"
	";WNLB)3%B)3%;WNL````B)3%B)3%B)3%B)3%B)3%B)3%B)3%B)3%B)3%B)3%````"
	";WNLB)3%B)3%````;WNLB)3%;WNL````B)3%B)3%B)3%B)3%B)3%B)3%B)3%B)3%"
	"B)3%B)3%B)3%B)3%````;WNLB)3%````;WNLB)3%B)3%B)3%B)3%B)3%B)3%B)3%"
	"B)3%B)3%B)3%B)3%B)3%B)3%B)3%B)3%B)3%B)3%B)3%````;WNL````````````"
	"````````````````````````````````````````````````````````````````"
	"";
