/* Keyboard Tranlation Routines */


#include <eros/target.h>
#include <domain/domdbg.h>
#include <eros/NodeKey.h>
#include "keytrans.h"



/* I Borrowed this from FreeBSD.syscons.c */


const keymap_t key_map = { 0x6d, {
/*                                                         alt
 * scan                       cntrl          alt    alt   cntrl
 * code  base   shift  cntrl  shift   alt   shift  cntrl  shift    spcl flgs
 * ---------------------------------------------------------------------------
 */
/*00*/{{  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP, }, 0xFF,0x00 },
/*01*/{{ 0x1B,  0x1B,  0x1B,  0x1B,  0x1B,  0x1B,   DBG,  0x1B, }, 0x02,0x00 },
/*02*/{{  '1',   '!',   NOP,   NOP,   '1',   '!',   NOP,   NOP, }, 0x33,0x00 },
/*03*/{{  '2',   '@',  0x00,  0x00,   '2',   '@',  0x00,  0x00, }, 0x00,0x00 },
/*04*/{{  '3',   '#',   NOP,   NOP,   '3',   '#',   NOP,   NOP, }, 0x33,0x00 },
/*05*/{{  '4',   '$',   NOP,   NOP,   '4',   '$',   NOP,   NOP, }, 0x33,0x00 },
/*06*/{{  '5',   '%',   NOP,   NOP,   '5',   '%',   NOP,   NOP, }, 0x33,0x00 },
/*07*/{{  '6',   '^',  0x1E,  0x1E,   '6',   '^',  0x1E,  0x1E, }, 0x00,0x00 },
/*08*/{{  '7',   '&',   NOP,   NOP,   '7',   '&',   NOP,   NOP, }, 0x33,0x00 },
/*09*/{{  '8',   '*',   NOP,   NOP,   '8',   '*',   NOP,   NOP, }, 0x33,0x00 },
/*0a*/{{  '9',   '(',   NOP,   NOP,   '9',   '(',   NOP,   NOP, }, 0x33,0x00 },
/*0b*/{{  '0',   ')',   NOP,   NOP,   '0',   ')',   NOP,   NOP, }, 0x33,0x00 },
/*0c*/{{  '-',   '_',  0x1F,  0x1F,   '-',   '_',  0x1F,  0x1F, }, 0x00,0x00 },
/*0d*/{{  '=',   '+',   NOP,   NOP,   '=',   '+',   NOP,   NOP, }, 0x33,0x00 },
/*0e*/{{ 0x08,  0x08,  0x7F,  0x7F,  0x08,  0x08,  0x7F,  0x7F, }, 0x00,0x00 },
/*0f*/{{ 0x09,  BTAB,   NOP,   NOP,  0x09,  BTAB,   NOP,   NOP, }, 0x77,0x00 },
/*10*/{{  'q',   'Q',  0x11,  0x11,   'q',   'Q',  0x11,  0x11, }, 0x00,0x01 },
/*11*/{{  'w',   'W',  0x17,  0x17,   'w',   'W',  0x17,  0x17, }, 0x00,0x01 },
/*12*/{{  'e',   'E',  0x05,  0x05,   'e',   'E',  0x05,  0x05, }, 0x00,0x01 },
/*13*/{{  'r',   'R',  0x12,  0x12,   'r',   'R',  0x12,  0x12, }, 0x00,0x01 },
/*14*/{{  't',   'T',  0x14,  0x14,   't',   'T',  0x14,  0x14, }, 0x00,0x01 },
/*15*/{{  'y',   'Y',  0x19,  0x19,   'y',   'Y',  0x19,  0x19, }, 0x00,0x01 },
/*16*/{{  'u',   'U',  0x15,  0x15,   'u',   'U',  0x15,  0x15, }, 0x00,0x01 },
/*17*/{{  'i',   'I',  0x09,  0x09,   'i',   'I',  0x09,  0x09, }, 0x00,0x01 },
/*18*/{{  'o',   'O',  0x0F,  0x0F,   'o',   'O',  0x0F,  0x0F, }, 0x00,0x01 },
/*19*/{{  'p',   'P',  0x10,  0x10,   'p',   'P',  0x10,  0x10, }, 0x00,0x01 },
/*1a*/{{  '[',   '{',  0x1B,  0x1B,   '[',   '{',  0x1B,  0x1B, }, 0x00,0x00 },
/*1b*/{{  ']',   '}',  0x1D,  0x1D,   ']',   '}',  0x1D,  0x1D, }, 0x00,0x00 },
/*1c*/{{ 0x0D,  0x0D,  0x0A,  0x0A,  0x0D,  0x0D,  0x0A,  0x0A, }, 0x00,0x00 },
/*1d*/{{ LCTR,  LCTR,  LCTR,  LCTR,  LCTR,  LCTR,  LCTR,  LCTR, }, 0xFF,0x00 },
/*1e*/{{  'a',   'A',  0x01,  0x01,   'a',   'A',  0x01,  0x01, }, 0x00,0x01 },
/*1f*/{{  's',   'S',  0x13,  0x13,   's',   'S',  0x13,  0x13, }, 0x00,0x01 },
/*20*/{{  'd',   'D',  0x04,  0x04,   'd',   'D',  0x04,  0x04, }, 0x00,0x01 },
/*21*/{{  'f',   'F',  0x06,  0x06,   'f',   'F',  0x06,  0x06, }, 0x00,0x01 },
/*22*/{{  'g',   'G',  0x07,  0x07,   'g',   'G',  0x07,  0x07, }, 0x00,0x01 },
/*23*/{{  'h',   'H',  0x08,  0x08,   'h',   'H',  0x08,  0x08, }, 0x00,0x01 },
/*24*/{{  'j',   'J',  0x0A,  0x0A,   'j',   'J',  0x0A,  0x0A, }, 0x00,0x01 },
/*25*/{{  'k',   'K',  0x0B,  0x0B,   'k',   'K',  0x0B,  0x0B, }, 0x00,0x01 },
/*26*/{{  'l',   'L',  0x0C,  0x0C,   'l',   'L',  0x0C,  0x0C, }, 0x00,0x01 },
/*27*/{{  ';',   ':',   NOP,   NOP,   ';',   ':',   NOP,   NOP, }, 0x33,0x00 },
/*28*/{{ '\'',   '"',   NOP,   NOP,  '\'',   '"',   NOP,   NOP, }, 0x33,0x00 },
/*29*/{{  '`',   '~',   NOP,   NOP,   '`',   '~',   NOP,   NOP, }, 0x33,0x00 },
/*2a*/{{  LSH,   LSH,   LSH,   LSH,   LSH,   LSH,   LSH,   LSH, }, 0xFF,0x00 },
/*2b*/{{ '\\',   '|',  0x1C,  0x1C,  '\\',   '|',  0x1C,  0x1C, }, 0x00,0x00 },
/*2c*/{{  'z',   'Z',  0x1A,  0x1A,   'z',   'Z',  0x1A,  0x1A, }, 0x00,0x01 },
/*2d*/{{  'x',   'X',  0x18,  0x18,   'x',   'X',  0x18,  0x18, }, 0x00,0x01 },
/*2e*/{{  'c',   'C',  0x03,  0x03,   'c',   'C',  0x03,  0x03, }, 0x00,0x01 },
/*2f*/{{  'v',   'V',  0x16,  0x16,   'v',   'V',  0x16,  0x16, }, 0x00,0x01 },
/*30*/{{  'b',   'B',  0x02,  0x02,   'b',   'B',  0x02,  0x02, }, 0x00,0x01 },
/*31*/{{  'n',   'N',  0x0E,  0x0E,   'n',   'N',  0x0E,  0x0E, }, 0x00,0x01 },
/*32*/{{  'm',   'M',  0x0D,  0x0D,   'm',   'M',  0x0D,  0x0D, }, 0x00,0x01 },
/*33*/{{  ',',   '<',   NOP,   NOP,   ',',   '<',   NOP,   NOP, }, 0x33,0x00 },
/*34*/{{  '.',   '>',   NOP,   NOP,   '.',   '>',   NOP,   NOP, }, 0x33,0x00 },
/*35*/{{  '/',   '?',   NOP,   NOP,   '/',   '?',   NOP,   NOP, }, 0x33,0x00 },
/*36*/{{  RSH,   RSH,   RSH,   RSH,   RSH,   RSH,   RSH,   RSH, }, 0xFF,0x00 },
/*37*/{{  '*',   '*',   '*',   '*',   '*',   '*',   '*',   '*', }, 0x00,0x00 },
/*38*/{{ LALT,  LALT,  LALT,  LALT,  LALT,  LALT,  LALT,  LALT, }, 0xFF,0x00 },
/*39*/{{  ' ',   ' ',  0x00,   ' ',   ' ',   ' ',  SUSP,   ' ', }, 0x02,0x00 },
/*3a*/{{  CLK,   CLK,   CLK,   CLK,   CLK,   CLK,   CLK,   CLK, }, 0xFF,0x00 },
/*3b*/{{ F( 1), F(13), F(25), F(37), S( 1), S(11), S( 1), S(11),}, 0xFF,0x00 },
/*3c*/{{ F( 2), F(14), F(26), F(38), S( 2), S(12), S( 2), S(12),}, 0xFF,0x00 },
/*3d*/{{ F( 3), F(15), F(27), F(39), S( 3), S(13), S( 3), S(13),}, 0xFF,0x00 },
/*3e*/{{ F( 4), F(16), F(28), F(40), S( 4), S(14), S( 4), S(14),}, 0xFF,0x00 },
/*3f*/{{ F( 5), F(17), F(29), F(41), S( 5), S(15), S( 5), S(15),}, 0xFF,0x00 },
/*40*/{{ F( 6), F(18), F(30), F(42), S( 6), S(16), S( 6), S(16),}, 0xFF,0x00 },
/*41*/{{ F( 7), F(19), F(31), F(43), S( 7), S( 7), S( 7), S( 7),}, 0xFF,0x00 },
/*42*/{{ F( 8), F(20), F(32), F(44), S( 8), S( 8), S( 8), S( 8),}, 0xFF,0x00 },
/*43*/{{ F( 9), F(21), F(33), F(45), S( 9), S( 9), S( 9), S( 9),}, 0xFF,0x00 },
/*44*/{{ F(10), F(22), F(34), F(46), S(10), S(10), S(10), S(10),}, 0xFF,0x00 },
/*45*/{{  NLK,   NLK,   NLK,   NLK,   NLK,   NLK,   NLK,   NLK, }, 0xFF,0x00 },
/*46*/{{  SLK,   SLK,   SLK,   SLK,   SLK,   SLK,   SLK,   SLK, }, 0xFF,0x00 },
/*47*/{{ F(49),  '7',   '7',   '7',   '7',   '7',   '7',   '7', }, 0x80,0x02 },
/*48*/{{ F(50),  '8',   '8',   '8',   '8',   '8',   '8',   '8', }, 0x80,0x02 },
/*49*/{{ F(51),  '9',   '9',   '9',   '9',   '9',   '9',   '9', }, 0x80,0x02 },
/*4a*/{{ '-',  '-',   '-',   '-',   '-',   '-',   '-',   '-', }, 0x80,0x02 },
/*4b*/{{ F(53),  '4',   '4',   '4',   '4',   '4',   '4',   '4', }, 0x80,0x02 },
/*4c*/{{ F(54),  '5',   '5',   '5',   '5',   '5',   '5',   '5', }, 0x80,0x02 },
/*4d*/{{ F(55),  '6',   '6',   '6',   '6',   '6',   '6',   '6', }, 0x80,0x02 },
/*4e*/{{ '+',  '+',   '+',   '+',   '+',   '+',   '+',   '+', }, 0x80,0x02 },
/*4f*/{{ F(57),  '1',   '1',   '1',   '1',   '1',   '1',   '1', }, 0x80,0x02 },
/*50*/{{ F(58),  '2',   '2',   '2',   '2',   '2',   '2',   '2', }, 0x80,0x02 },
/*51*/{{ F(59),  '3',   '3',   '3',   '3',   '3',   '3',   '3', }, 0x80,0x02 },
/*52*/{{ F(60),  '0',   '0',   '0',   '0',   '0',   '0',   '0', }, 0x80,0x02 },
/*53*/{{ 0x7F,   '.',   '.',   '.',   '.',   '.',   RBT,   RBT, }, 0x03,0x02 },
/*54*/{{  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP, }, 0xFF,0x00 },
/*55*/{{  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP, }, 0xFF,0x00 },
/*56*/{{  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP, }, 0xFF,0x00 },
/*57*/{{ F(11), F(23), F(35), F(47), S(11), S(11), S(11), S(11),}, 0xFF,0x00 },
/*58*/{{ F(12), F(24), F(36), F(48), S(12), S(12), S(12), S(12),}, 0xFF,0x00 },
/*59*/{{ 0x0D,  0x0D,  0x0A,  0x0A,  0x0D,  0x0D,  0x0A,  0x0A, }, 0x00,0x00 },
/*5a*/{{ RCTR,  RCTR,  RCTR,  RCTR,  RCTR,  RCTR,  RCTR,  RCTR, }, 0xFF,0x00 },
/*5b*/{{  '/',   '/',   '/',   '/',   '/',   '/',   '/',   '/', }, 0x00,0x02 },
/*5c*/{{ NEXT,  NEXT,   DBG,   DBG,   NOP,   NOP,   NOP,   NOP, }, 0xFF,0x00 },
/*5d*/{{ RALT,  RALT,  RALT,  RALT,  RALT,  RALT,  RALT,  RALT, }, 0xFF,0x00 },
/*5e*/{{ F(49), F(49), F(49), F(49), F(49), F(49), F(49), F(49),}, 0xFF,0x00 },
/*5f*/{{ F(50), F(50), F(50), F(50), F(50), F(50), F(50), F(50),}, 0xFF,0x00 },
/*60*/{{ F(51), F(51), F(51), F(51), F(51), F(51), F(51), F(51),}, 0xFF,0x00 },
/*61*/{{ F(53), F(53), F(53), F(53), F(53), F(53), F(53), F(53),}, 0xFF,0x00 },
/*62*/{{ F(55), F(55), F(55), F(55), F(55), F(55), F(55), F(55),}, 0xFF,0x00 },
/*63*/{{ F(57), F(57), F(57), F(57), F(57), F(57), F(57), F(57),}, 0xFF,0x00 },
/*64*/{{ F(58), F(58), F(58), F(58), F(58), F(58), F(58), F(58),}, 0xFF,0x00 },
/*65*/{{ F(59), F(59), F(59), F(59), F(59), F(59), F(59), F(59),}, 0xFF,0x00 },
/*66*/{{ F(60), F(60), F(60), F(60), F(60), F(60), F(60), F(60),}, 0xFF,0x00 },
/*67*/{{ F(61), F(61), F(61), F(61), F(61), F(61),  RBT,  F(61),}, 0xFF,0x00 },
/*68*/{{  SLK,  SPSC,   SLK,  SPSC,  SUSP,   NOP,  SUSP,   NOP, }, 0xFF,0x00 },
/*69*/{{ F(62), F(62), F(62), F(62), F(62), F(62), F(62), F(62),}, 0xFF,0x00 },
/*6a*/{{ F(63), F(63), F(63), F(63), F(63), F(63), F(63), F(63),}, 0xFF,0x00 },
/*6b*/{{ F(64), F(64), F(64), F(64), F(64), F(64), F(64), F(64),}, 0xFF,0x00 },
/*6c*/{{  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP, }, 0xFF,0x00 },
} };




int
updShiftState(int keycode, int up, int *shiftstate) 
{

  struct keyent_t key;
  int i;
  int state = *shiftstate;
  int action;
  
  i = keycode;
  key = key_map.key[i];
  i = ((state & SHIFTS) ? 1 : 0)
    | ((state & CTLS) ? 2 : 0)
    | ((state & ALTS) ? 4 : 0);

  if (!up) {
    if (((keycode >= 0x47) && (keycode <= 0x53)) && (state & NLKED)) {
      return 0;
    }
    else if ((keycode == 0x4a) || (keycode == 0x4e)) {
      return 0;
    }
  }

  action = key.map[i];
  if (up) {	/* break: key released */
    if (key.spcl & (0x80 >> i)) {
      /* special keys */
      switch (action) {

      case LSH:
	state &= ~SHIFTS1;
	break;
      case RSH:
	state &= ~SHIFTS2;
	break;
      case LCTR:
	state &= ~CTLS1;
	break;
      case RCTR:
	state &= ~CTLS2;
	break;
      case LALT:
	state &= ~ALTS1;
	break;
      case RALT:
	state &= ~ALTS2;
	break;
      case ASH:
	state &= ~AGRS1;
	break;
      case META:
	state &= ~METAS1;
	break;
      case NLK:
	state &= ~NLKDOWN;
	break;
      case CLK:
	state &= ~CLKDOWN;
	break;
      case SLK:
	state &= ~SLKDOWN;
	break;
      case ALK:
	state &= ~ALKDOWN;
	break;
      }
      *shiftstate = state;
      return 1;
    }
    /* release events of regular keys are not reported */
    return 2;
  } 
  else {	/* make: key pressed */
      /* special keys */
    if (key.spcl & (0x80 >> i)) {
      switch (action) {
	/* LOCKING KEYS */
      case NLK:
	if ((state & NLKDOWN) == 0) {
	  if (state & NLKED) {
	    state &= ~NLKED;
	    state |= LED_UPDATE;
	  }
	  else {
	    state |= NLKED;
	    state |= LED_UPDATE;
	  }
	  state |= NLKDOWN;
	}
	break;
    case CLK:
      if ((state & CLKDOWN) == 0) {
	if (state & CLKED) {
	  state &= ~CLKED;
	  state |= LED_UPDATE;
	}
	else {
	  state |= CLKED;
	  state |= LED_UPDATE;
	}
	state |= CLKDOWN;
      }
      break;
      case SLK:
	if ((state & SLKDOWN) == 0) {
	  if (state & SLKED) {
	    state &= ~SLKED;
	    state |= LED_UPDATE;
	  }
	  else {
	    state |= SLKED;
	    state |= LED_UPDATE;
	  }
	  state |= SLKDOWN;
	}
	break;
      case ALK:
	if ((state & ALKDOWN) == 0) {
	  if (state & ALKED) {
	    state &= ~ALKED;
	  }
	  else {
	    state |= ALKED;
	  }
	  state |= ALKDOWN;
	}
	break;
      case LSH:
	state |= SHIFTS1;
	break;
      case RSH:
	state |= SHIFTS2;
	break;
      case LCTR:
	state |= CTLS1;
	break;
      case RCTR:
	state |= CTLS2;
	break;
      case LALT:
	state |= ALTS1;
	break;
      case RALT:
	state |= ALTS2;
	break;
      case ASH:
	state |= AGRS1;
	break;
      case META:
	state |= METAS1;
	break;
      default:
	*shiftstate = state;
	return 1;
      }
      *shiftstate = state;
      return 1;
    }
    else {
      /* regular keys */
      *shiftstate = state;
      return 0;
    }
  }
  /* NOT REACHED */
}


int
scanToKey(int scancode, int *prev_key, int shiftstate) 
{

  int keycode, ks_prefix;
  ks_prefix = *prev_key;
  
  keycode = scancode & 0x7F;
  switch (ks_prefix) {
  case 0x00:	/* normal scancode */
    switch(scancode) {

#if 0
    case 0xB8:	/* left alt (compose key) released */
      if (state->ks_flags & COMPOSE) {
	state->ks_flags &= ~COMPOSE;
	if (state->ks_composed_char > UCHAR_MAX)
	  state->ks_composed_char = 0;
      }
      break;
    case 0x38:	/* left alt (compose key) pressed */
      if (!(state->ks_flags & COMPOSE)) {
	state->ks_flags |= COMPOSE;
	state->ks_composed_char = 0;
      }
      break;
#endif

    case 0xE0:
    case 0xE1:
      ks_prefix = scancode;
      goto next_code;
    }
    break;
  case 0xE0:      /* 0xE0 prefix */
    ks_prefix = 0;
    switch (keycode) {
    case 0x1C:	/* right enter key */
      keycode = 0x59;
      break;
    case 0x1D:	/* right ctrl key */
      keycode = 0x5A;
      break;
    case 0x35:	/* keypad divide key */
      keycode = 0x5B;
      break;
    case 0x37:	/* print scrn key */
      keycode = 0x5C;
      break;
    case 0x38:	/* right alt key (alt gr) */
      keycode = 0x5D;
      break;
    case 0x46:	/* ctrl-pause/break on AT 101 (see below) */
      keycode = 0x68;
      break;
    case 0x47:	/* grey home key */
      keycode = 0x5E;
      break;
    case 0x48:	/* grey up arrow key */
      keycode = 0x5F;
      break;
    case 0x49:	/* grey page up key */
      keycode = 0x60;
      break;
    case 0x4B:	/* grey left arrow key */
      keycode = 0x61;
      break;
    case 0x4D:	/* grey right arrow key */
      keycode = 0x62;
      break;
    case 0x4F:	/* grey end key */
      keycode = 0x63;
      break;
    case 0x50:	/* grey down arrow key */
      keycode = 0x64;
      break;
    case 0x51:	/* grey page down key */
      keycode = 0x65;
      break;
    case 0x52:	/* grey insert key */
      keycode = 0x66;
      break;
    case 0x53:	/* grey delete key */
      keycode = 0x67;
      break;
      /* the following 3 are only used on the MS "Natural" keyboard */
    case 0x5b:	/* left Window key */
      keycode = 0x69;
      break;
    case 0x5c:	/* right Window key */
      keycode = 0x6a;
      break;
    case 0x5d:	/* menu key */
      keycode = 0x6b;
      break;
    default:	/* ignore everything else */
      goto next_code;
    }
    break;
  case 0xE1:	/* 0xE1 prefix */
    /* 
     * The pause/break key on the 101 keyboard produces:
     * E1-1D-45 E1-9D-C5
     * Ctrl-pause/break produces:
     * E0-46 E0-C6 (See above.)
     */
    ks_prefix = 0;
    if (keycode == 0x1D)
      ks_prefix = 0x1D;
    goto next_code;
    /* NOT REACHED */
  case 0x1D:	/* pause / break */
    ks_prefix = 0;
    if (keycode != 0x45)
      goto next_code;
    keycode = 0x68;
    break;
  }

  switch (keycode) {
  case 0x5c:	/* print screen */
    if (shiftstate & ALTS)
      keycode = 0x54;	/* sysrq */
    break;
  case 0x68:	/* pause/break */
    if (shiftstate & CTLS)
      keycode = 0x6c;	/* break */
    break;
  }

  *prev_key = ks_prefix;
  /*  return (keycode | (scancode & 0x80));*/
  return keycode;

 next_code:

  *prev_key = ks_prefix;
  return -1;
  

}
