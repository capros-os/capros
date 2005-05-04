#ifndef __keytrans_h__
#define __keytrans_h__


/* Keytrans.h contains the translation routines to get scancodes into
 * keycodes and then turn keycodes into something possibly meaningful
 * like letters and ASCII characters.
 * These routines ought to be sufficiently generic that by including the
 * header and sourcefile in programs, they will actually translate keys.
 * However, this still remains to be seen.
 */


/* I Borrowed this from FreeBSD
 * It's a mess at the moment, but I'll clean it up eventually
 * No, really, I will.
 */


/* shift key state */
#define SHIFTS1		(1 << 16)
#define SHIFTS2		(1 << 17)
#define SHIFTS		(SHIFTS1 | SHIFTS2)
#define CTLS1		(1 << 18)
#define CTLS2		(1 << 19)
#define CTLS		(CTLS1 | CTLS2)
#define ALTS1		(1 << 20)
#define ALTS2		(1 << 21)
#define ALTS		(ALTS1 | ALTS2)
#define AGRS1		(1 << 22)
#define AGRS2		(1 << 23)
#define AGRS		(AGRS1 | AGRS2)
#define METAS1		(1 << 24)
#define METAS2		(1 << 25)
#define METAS		(METAS1 | METAS2)
#define NLKDOWN		(1 << 26)
#define SLKDOWN		(1 << 27)
#define CLKDOWN		(1 << 28)
#define ALKDOWN		(1 << 29)
#define SHIFTAON	(1 << 30)

/* lock key states */

#define CLKED		LED_CAP
#define NLKED		LED_NUM
#define SLKED		LED_SCR
#define ALKED		(1 << 3)
#define LOCK_MASK	(CLKED | NLKED | SLKED | ALKED)
#define LED_CAP		(1 << 2)
#define LED_NUM		(1 << 1)
#define LED_SCR		(1 << 0)
#define LED_MASK	(LED_CAP | LED_NUM | LED_SCR)
#define LED_UPDATE      (1 << 4)


#define NUM_KEYS	256		/* number of keys in table	*/
#define NUM_STATES	8		/* states per key		*/


struct keyent_t {
	unsigned char	map[NUM_STATES];
	unsigned char	spcl;
	unsigned char	flgs;
#define	FLAG_LOCK_O	0
#define	FLAG_LOCK_C	1
#define FLAG_LOCK_N	2
};

struct keymap {
	unsigned short	n_keys;
	struct keyent_t	key[NUM_KEYS];
};
typedef struct keymap keymap_t;


#define F(x)		((x)+F_FN-1)
#define	S(x)		((x)+F_SCR-1)


/* defines for "special" keys (spcl bit set in keymap) */
#define NOP		0x00		/* nothing (dead key)		*/
#define LSH		0x02		/* left shift key		*/
#define RSH		0x03		/* right shift key		*/
#define CLK		0x04		/* caps lock key		*/
#define NLK		0x05		/* num lock key			*/
#define SLK		0x06		/* scroll lock key		*/
#define LALT		0x07		/* left alt key			*/
#define BTAB		0x08		/* backwards tab		*/
#define LCTR		0x09		/* left control key		*/
#define NEXT		0x0a		/* switch to next screen 	*/
#define F_SCR		0x0b		/* switch to first screen 	*/
#define L_SCR		0x1a		/* switch to last screen 	*/
#define F_FN		0x1b		/* first function key 		*/
#define L_FN		0x7a		/* last function key 		*/
/*			0x7b-0x7f	   reserved do not use !	*/
#define RCTR		0x80		/* right control key		*/
#define RALT		0x81		/* right alt (altgr) key	*/
#define ALK		0x82		/* alt lock key			*/
#define ASH		0x83		/* alt shift key		*/
#define META		0x84		/* meta key			*/
#define RBT		0x85		/* boot machine			*/
#define DBG		0x86		/* call debugger		*/
#define SUSP		0x87		/* suspend power (APM)		*/
#define SPSC		0x88		/* toggle splash/text screen	*/

#define F_ACC		DGRA		/* first accent key		*/
#define DGRA		0x89		/* grave			*/
#define DACU		0x8a		/* acute			*/
#define DCIR		0x8b		/* circumflex			*/
#define DTIL		0x8c		/* tilde			*/
#define DMAC		0x8d		/* macron			*/
#define DBRE		0x8e		/* breve			*/
#define DDOT		0x8f		/* dot				*/
#define DUML		0x90		/* umlaut/diaresis		*/
#define DDIA		0x90		/* diaresis			*/
#define DSLA		0x91		/* slash			*/
#define DRIN		0x92		/* ring				*/
#define DCED		0x93		/* cedilla			*/
#define DAPO		0x94		/* apostrophe			*/
#define DDAC		0x95		/* double acute			*/
#define DOGO		0x96		/* ogonek			*/
#define DCAR		0x97		/* caron			*/
#define L_ACC		DCAR		/* last accent key		*/

#define STBY		0x98		/* Go into standby mode (apm)   */
#define PREV		0x99		/* switch to previous screen 	*/
#define PNC		0x9a		/* force system panic */
#define LSHA		0x9b		/* left shift key / alt lock	*/
#define RSHA		0x9c		/* right shift key / alt lock	*/
#define LCTRA		0x9d		/* left ctrl key / alt lock	*/
#define RCTRA		0x9e		/* right ctrl key / alt lock	*/
#define LALTA		0x9f		/* left alt key / alt lock	*/
#define RALTA		0xa0		/* right alt key / alt lock	*/
#define HALT		0xa1		/* halt machine */
#define PDWN		0xa2		/* halt machine and power down */

/* Convert scancode to an ASCII character.  Return 'true' if
   successful */
bool convert_scancode(int32_t data, uint8_t *c, bool *special);

#endif /* __keytrans_h__ */
