#ifndef __DEBUG_H__
#define __DEBUG_H__

/*FIX
#include "arch/cc.h"
*/
/** lower two bits indicate debug level
 * - 0 off
 * - 1 warning
 * - 2 serious
 * - 3 severe
 */

#define DBG_LEVEL_OFF     0
#define DBG_LEVEL_WARNING 1	/* bad checksums, dropped packets, ... */
#define DBG_LEVEL_SERIOUS 2	/* memory allocation failures, ... */
#define DBG_LEVEL_SEVERE  3	/* */ 
#define DBG_MASK_LEVEL    3

/** flag for DEBUGF to enable that debug message */
#define DBG_ON  0x80U
/** flag for DEBUGF to disable that debug message */
#define DBG_OFF 0x00U

/** flag for DEBUGF indicating a tracing message (to follow program flow) */
#define DBG_TRACE   0x40U
/** flag for DEBUGF indicating a state debug message (to follow module states) */
#define DBG_STATE   0x20U
/** flag for DEBUGF indicating newly added code, not thoroughly tested yet */
#define DBG_FRESH   0x10U
/** flag for DEBUGF to halt after printing this debug message */
#define DBG_HALT    0x08U

#define ERIP_ASSERT(x,y) do { if(0) kdprintf(KR_OSTREAM,x); } while(0)
#if 0 //#ifdef ERIP_DEBUG
#  define ERIP_ASSERT(x,y) do { if(!(y)) ERIP_PLATFORM_ASSERT(x); } while(0)
/** print debug message only if debug message type is enabled...
 *  AND is of correct type AND is at least DBG_LEVEL
 */
#  define DEBUGF(debug,x) do { if (((debug) & DBG_ON) && ((debug) & DBG_TYPES_ON) && (((debug) & DBG_MASK_LEVEL) >= DBG_MIN_LEVEL)) { ERIP_PLATFORM_DIAG(x); if ((debug) & DBG_HALT) while(1); } } while(0)
#  define ERIP_ERROR(x)	 do { ERIP_PLATFORM_DIAG(x); } while(0)	
//#else /* ERIP_DEBUG */
#endif /*#if 0 */

#  define DEBUGF(debug,x) 
#  define ERIP_ERROR(x)	
//#endif /* ERIP_DEBUG */

#endif /* __DEBUG_H__ */






