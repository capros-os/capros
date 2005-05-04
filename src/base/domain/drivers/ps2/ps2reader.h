#ifndef __keyb_h__
#define __keyb_h__

#define KBD_IRQ    1
#define MOUSE_IRQ  12

#define KBCTMOUT 250    /* Timeout for talking to controller */
#define KBTMOUT  100    /* Timeout for talking to keyboard */
#define KBDSLEEP 0.0002 /* We cannot clobber the h/w status register
                         * Give it some rest before request status again */ 

const uint16_t KbdDataPort   = 0x60u;
const uint16_t KbdOutPort    = 0x60u;
const uint16_t KbdCtrlPort   = 0x64u;
const uint16_t KbdStatusPort = 0x64u;

const uint8_t KBS_BufFull    = 0x1u;
const uint8_t KBS_NotReady   = 0x2u;
const uint8_t KBS_AuxB       = 0x20u;
const uint8_t KBS_PERR       = 0xF0u;

const uint8_t KbdCSelfTest  = 0xAAu;
const uint8_t KbdCCtrlTest  = 0xABu;
const uint8_t KbdCEnableAux = 0xA8u;
const uint8_t KbdCCheckAux  = 0xA9u;
const uint8_t KbdCEnable    = 0xAEu;
const uint8_t KbdCDisable   = 0xADu;
const uint8_t KbdCWrOut     = 0xD1u;
const uint8_t KbdCAuxDev    = 0xD4u;
const uint8_t KbdCAuxEnable = 0xA8u;
const uint8_t KbdOCEcho     = 0xEEu;
const uint8_t KbdOCIDkeyb   = 0xF2u;
const uint8_t KbdOCSetLed   = 0xEDu;
const uint8_t KbdSetRate    = 0xF3u;
const uint8_t KbdOCReset    = 0xFFu;
const uint8_t kbdCInpPort   = 0xC0u;

const uint8_t KBR_ACK     = 0xFAu;
const uint8_t KBR_BATComp = 0xAAu;
const uint8_t KBR_STOK    = 0x55u;
const uint8_t KBR_CTOK    = 0x0u;

const uint8_t Mouse_STREAM   = 0xEAu;
const uint8_t Mouse_REMOTE   = 0xF0u;
const uint8_t Mouse_RESET    = 0xFFu;
const uint8_t Mouse_ENABLE   = 0xF4u;
const uint8_t Mouse_DISABLE  = 0xF5u;
const uint8_t Mouse_SAMPLE   = 0xF3u;
const uint8_t Mouse_GETID    = 0xF2u;
const uint8_t Mouse_READ     = 0xEBu;
const uint8_t Mouse_RES      = 0xE8u;
const uint8_t Mouse_STATREQ  = 0xE9u;
const uint8_t Mouse_SCALESET = 0xE7u;
const uint8_t Mouse_SETSTD   = 0xF6u;

/* function declarations */
void kbdFetchData(unsigned int bytes, uint8_t *data);
void ps2FlushOutBuf(void);
void kbdIdent(void);
void kbdWriteC(uint8_t cmd);
int kbdInit(void);
int kbdPing(void);
int kbdReadData(void);
int kbdWriteO(uint8_t cmd);
int kbdSetLed(uint8_t leds);
int kbdWaitInput(void);
uint32_t AllocIRQ(unsigned int irq);
uint8_t kbdReadStat(void);


/* function declarations */
struct mouseStats{
  uint8_t R1;           // 0-Mode-Enable-Scaling-0-L-M-R
  uint8_t R2;           //The resolution
  uint8_t R3;           //The Sampling Rate
};
typedef struct mouseStats MouseStats;

/* flag to indicate if the mouse is 4 button*/
int Is4Button;

int  initInterface(void);
int  mouseInit();
int  isIntelliMouse(void);
int  isScrollMouse(void);
void setSampleRate(int);
void setResolution(int);
void setScaling(void);
int  mouseAck(char*);
MouseStats reqStatus(void);

#endif /* __keyb_h__ */
