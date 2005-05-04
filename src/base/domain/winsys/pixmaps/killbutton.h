/*  GIMP header image file format (RGB): /home/vandy/EROS/eros/src/base/domain/winsys/pixmaps/killbutton.h  */

#define kill_width   20
#define kill_height  20
#define kill_depth   32

/*  Call this macro repeatedly.  After each use, the pixel data can be extracted  */
#ifndef HEADER_PIXEL
#define HEADER_PIXEL(data,pixel) {\
  pixel[0] = (((data[0] - 33) << 2) | ((data[1] - 33) >> 4)); \
  pixel[1] = ((((data[1] - 33) & 0xF) << 4) | ((data[2] - 33) >> 2)); \
  pixel[2] = ((((data[2] - 33) & 0x3) << 6) | ((data[3] - 33))); \
  data += 4; \
}
#endif

#if 0
static uint8_t *kill_pixmap =
	"````````````````````````````````````````````````````````````````"
	"````````````;WNL````ML+SML+SML+SML+SML+SML+SML+SML+SML+SML+SML+S"
	"ML+SML+SML+SML+SML+SML+SML+S;WNL````ML+S;WNLML+SML+SML+SML+SML+S"
	"ML+SML+SML+SML+SML+SML+SML+SML+SML+S;WNLML+S;WNL````ML+S````;WNL"
	"ML+SML+SML+SML+SML+SML+SML+SML+SML+SML+SML+SML+S;WNL````ML+S;WNL"
	"````ML+SML+S````;WNLML+SML+SML+SML+SML+SML+SML+SML+SML+SML+S;WNL"
	"````ML+SML+S;WNL````ML+SML+SML+S````;WNLML+SML+SML+SML+SML+SML+S"
	"ML+SML+S;WNL````ML+SML+SML+S;WNL````ML+SML+SML+SML+S````;WNLML+S"
	"ML+SML+SML+SML+SML+S;WNL````ML+SML+SML+SML+S;WNL````ML+SML+SML+S"
	"ML+SML+S````;WNLML+SML+SML+SML+S;WNL````ML+SML+SML+SML+SML+S;WNL"
	"````ML+SML+SML+SML+SML+SML+S````;WNLML+SML+S;WNL````ML+SML+SML+S"
	"ML+SML+SML+S;WNL````ML+SML+SML+SML+SML+SML+SML+S````ML+S;WNL````"
	"ML+SML+SML+SML+SML+SML+SML+S;WNL````ML+SML+SML+SML+SML+SML+SML+S"
	"ML+S;WNLML+SML+SML+SML+SML+SML+SML+SML+SML+S;WNL````ML+SML+SML+S"
	"ML+SML+SML+SML+S;WNL````ML+S;WNLML+SML+SML+SML+SML+SML+SML+S;WNL"
	"````ML+SML+SML+SML+SML+SML+S;WNL````ML+SML+S````;WNLML+SML+SML+S"
	"ML+SML+SML+S;WNL````ML+SML+SML+SML+SML+S;WNL````ML+SML+SML+SML+S"
	"````;WNLML+SML+SML+SML+SML+S;WNL````ML+SML+SML+SML+S;WNL````ML+S"
	"ML+SML+SML+SML+SML+S````;WNLML+SML+SML+SML+S;WNL````ML+SML+SML+S"
	";WNL````ML+SML+SML+SML+SML+SML+SML+SML+S````;WNLML+SML+SML+S;WNL"
	"````ML+SML+S;WNL````ML+SML+SML+SML+SML+SML+SML+SML+SML+SML+S````"
	";WNLML+SML+S;WNL````ML+S;WNL````ML+SML+SML+SML+SML+SML+SML+SML+S"
	"ML+SML+SML+SML+S````;WNLML+S;WNL````ML+SML+SML+SML+SML+SML+SML+S"
	"ML+SML+SML+SML+SML+SML+SML+SML+SML+SML+SML+S;WNL;WNL;WNL;WNL;WNL"
	";WNL;WNL;WNL;WNL;WNL;WNL;WNL;WNL;WNL;WNL;WNL;WNL;WNL;WNL;WNL;WNL"
	"";
#endif

static uint8_t *kill_pixmap =
	"````````````````````````````````````````````````````````````````"
	"````````````````````````````````````````````````````````````````"
	"````````````````````````````L\\S[````````XO8EXO8EXO8EXO8EXO8EXO8E"
	"XO8EXO8EXO8EXO8EXO8EXO8EXO8EXO8EXO8EXO8EL\\S[L\\S[````````XO8E>Y*`"
	">Y*`XO8EXO8EXO8EXO8EXO8EXO8EXO8EXO8EXO8EXO8EXO8E>Y*`XO8EL\\S[L\\S["
	"````````XO8E````>Y*`>Y*`XO8EXO8EXO8EXO8EXO8EXO8EXO8EXO8EXO8E>Y*`"
	">Y*`XO8EL\\S[L\\S[````````XO8E````````>Y*`>Y*`XO8EXO8EXO8EXO8EXO8E"
	"XO8EXO8E>Y*`>Y*`````XO8EL\\S[L\\S[````````XO8EXO8E````````>Y*`>Y*`"
	"XO8EXO8EXO8EXO8EXO8E>Y*`>Y*`````````XO8EL\\S[L\\S[````````XO8EXO8E"
	"XO8E````````>Y*`>Y*`XO8EXO8EXO8E>Y*`>Y*`````````XO8EXO8EL\\S[L\\S["
	"````````XO8EXO8EXO8EXO8E````````>Y*`>Y*`XO8E>Y*`>Y*`````````XO8E"
	"XO8EXO8EL\\S[L\\S[````````XO8EXO8EXO8EXO8EXO8E````````>Y*`>Y*`>Y*`"
	"````````XO8EXO8EXO8EXO8EL\\S[L\\S[````````XO8EXO8EXO8EXO8EXO8EXO8E"
	"````>Y*`>Y*`>Y*`````XO8EXO8EXO8EXO8EXO8EL\\S[L\\S[````````XO8EXO8E"
	"XO8EXO8EXO8EXO8E>Y*`>Y*`````>Y*`>Y*`XO8EXO8EXO8EXO8EXO8EL\\S[L\\S["
	"````````XO8EXO8EXO8EXO8EXO8E>Y*`>Y*`````````````>Y*`>Y*`XO8EXO8E"
	"XO8EXO8EL\\S[L\\S[````````XO8EXO8EXO8EXO8E>Y*`>Y*`````````XO8E````"
	"````>Y*`>Y*`XO8EXO8EXO8EL\\S[L\\S[````````XO8EXO8EXO8E>Y*`>Y*`````"
	"````XO8EXO8EXO8E````````>Y*`>Y*`XO8EXO8EL\\S[L\\S[````````XO8EXO8E"
	">Y*`>Y*`````````XO8EXO8EXO8EXO8EXO8E````````>Y*`>Y*`XO8EL\\S[L\\S["
	"````````XO8E>Y*`>Y*`````````XO8EXO8EXO8EXO8EXO8EXO8EXO8E````````"
	">Y*`XO8EL\\S[L\\S[````````XO8EXO8EXO8EXO8EXO8EXO8EXO8EXO8EXO8EXO8E"
	"XO8EXO8EXO8EXO8EXO8EXO8EL\\S[L\\S[````````L\\S[L\\S[L\\S[L\\S[L\\S[L\\S["
	"L\\S[L\\S[L\\S[L\\S[L\\S[L\\S[L\\S[L\\S[L\\S[L\\S[L\\S[L\\S[````L\\S[L\\S[L\\S["
	"L\\S[L\\S[L\\S[L\\S[L\\S[L\\S[L\\S[L\\S[L\\S[L\\S[L\\S[L\\S[L\\S[L\\S[L\\S[L\\S["
	"";
