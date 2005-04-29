#ifndef __IDLSTRM_H__
#define __IDLSTRM_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long cap_t;
typedef unsigned long fixreg_t;
typedef unsigned long result_t;

typedef struct Message {
  fixreg_t invType;
  fixreg_t snd_invKey;		  /* key to be invoked */

  fixreg_t snd_len;
  const void *snd_data;
  
  unsigned char snd_key0;
  unsigned char snd_key1;
  unsigned char snd_key2;
  unsigned char snd_rsmkey;

  fixreg_t rcv_limit;
  void *rcv_data;
  
  unsigned char rcv_key0;
  unsigned char rcv_key1;
  unsigned char rcv_key2;
  unsigned char rcv_rsmkey;

  fixreg_t snd_code;		  /* called this for compatibility */
  fixreg_t snd_w1;
  fixreg_t snd_w2;
  fixreg_t snd_w3;


  fixreg_t rcv_code;			  /* called this for compatibility */
  fixreg_t rcv_w1;
  fixreg_t rcv_w2;
  fixreg_t rcv_w3;

  unsigned short rcv_keyInfo;
  fixreg_t rcv_sent;
} Message;

extern fixreg_t CALL(Message*);
extern fixreg_t RETURN(Message*);
extern fixreg_t SEND(Message*);

struct idlstrm {
  char *base;
  char *pos;
  unsigned offset;
  unsigned limit;
};
typedef struct idlstrm idlstrm;

extern void __capidl_serialize_i8(idlstrm *, signed char *);
extern void __capidl_serialize_u8(idlstrm *, unsigned char *);

extern void __capidl_serialize_i16(idlstrm *, signed short *);
extern void __capidl_serialize_u16(idlstrm *, unsigned short *);

extern void __capidl_serialize_i32(idlstrm *, signed long *);
extern void __capidl_serialize_u32(idlstrm *, unsigned long *);

extern void __capidl_serialize_i64(idlstrm *, signed long long *);
extern void __capidl_serialize_u64(idlstrm *, unsigned long long *);

#ifdef __cplusplus
}
#endif


#endif /* __IDLSTRM_H__ */
