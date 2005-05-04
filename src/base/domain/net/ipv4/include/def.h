#ifndef __DEF_H__
#define __DEF_H__

/* this might define NULL already */
//#include "arch/cc.h"

#define ERIP_MAX(x , y)  (x) > (y) ? (x) : (y)
#define ERIP_MIN(x , y)  (x) < (y) ? (x) : (y)

#ifndef NULL
#define NULL ((void *)0)
#endif


#endif /* __DEF_H__ */

