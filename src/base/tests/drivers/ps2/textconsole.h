#ifndef __textconsole_h__
#define __textconsole_h__

/* Right now this is just holding op and return codes.  Expect something 
 * more fancy later.
 */

#define OC_put_char_arry   6
#define OC_clear_screen    7
#define OC_put_char        8
#define OC_put_colchar_AtPos 9

#define RC_bad_pos_spec   50


enum ConsState {
  NotInit,
  WaitChar,
  GotEsc,
  WaitParam,
};

/* A few colors commonly used*/
#define WHITE 0x700u
#define GREEN 0x200u
#define RED   0x400u



#endif /* __textconsole_h__ */
