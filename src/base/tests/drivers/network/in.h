#ifndef __IN_H__
#define __IN_H__

typedef uint32_t in_addr_t;


/* IP Address */
typedef struct{
  in_addr_t s_addr;
}IN_ADDR;


inline unsigned short inchksum(unsigned short *ip, int len) {
  unsigned long sum = 0;
  
  len >>= 1;
  while (len--) {
    sum += *(ip++);
    if (sum > 0xFFFF) sum -= 0xFFFF;
  }
  
  return((~sum) & 0x0000FFFF);
}

#endif /*__IN_H__*/
