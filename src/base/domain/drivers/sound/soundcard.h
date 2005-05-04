#ifndef __soundcard_h__
#define __soundcard_h__


//Port Values
#define S_BASE          0x220
#define DSP_RESET       (S_BASE + 0x6)
#define DSP_READ        (S_BASE + 0xA)
#define DSP_DATA_AV     (S_BASE + 0xE)
#define DSP_COMMAND     (S_BASE + 0xC)
#define DSP_STATUS      (S_BASE + 0xC)
#define DSP_WRITE	(S_BASE + 0xC)
#define DSP_COMMAND	(S_BASE + 0xC)
#define DSP_STATUS	(S_BASE + 0xC)

#define MIXER_ADDR	(S_BASE + 0x4)
#define MIXER_DATA	(S_BASE + 0x5)


//#define DSP_DATA_AVAIL	(S_BASE + 0xE)
//#define DSP_DATA_AVL16	(S_BASE + 0xF)



#define OPL3_LEFT	(S_BASE + 0x0)
#define OPL3_RIGHT	(S_BASE + 0x2)
#define OPL3_BOTH	(S_BASE + 0x8)
/*
#define     (S_BASE+)
#define     (S_BASE+)
#define     (S_BASE+)
#define     (S_BASE+)
#define     (S_BASE+)
#define     (S_BASE+)
#define     (S_BASE+)
*/

typedef struct voc_header {
  unsigned char Description[20];
  unsigned short DataBlockOffset;
  unsigned short Version;
  unsigned short IDCode;
} voc_header;




//Fuction Prototypes
uint32_t sb16_dsp_reset();   //DSP_Reset 
uint32_t sound_initialize();
void sb16_play();


#endif /* __soundcard_h__ */
