#ifndef __ERR_H__
#define __ERR_H__

#include "include/opt.h"
/*FIX: 
#include "arch/cc.h"
*/

//typedef s8_t err_t;

/* Definitions for error constants. */

#define ERR_OK    0      /* No error, everything OK. */
#define ERR_MEM  11      /* Out of memory error.     */
#define ERR_BUF  12      /* Buffer error.            */


#define ERR_ABRT 13      /* Connection aborted.      */
#define ERR_RST  14      /* Connection reset.        */
#define ERR_CLSD 15      /* Connection closed.       */
#define ERR_CONN 16      /* Not connected.           */

#define ERR_VAL  17      /* Illegal value.           */

#define ERR_ARG  18      /* Illegal argument.        */

#define ERR_RTE  19      /* Routing problem.         */

#define ERR_USE  20     /* Address in use.          */

#define ERR_IF   21     /* Low-level netif error    */


#ifdef ERIP_DEBUG
extern char *erip_strerr(err_t err);
#else
#define erip_strerr(x) ""
#endif /* ERIP_DEBUG */
#endif /* __ERR_H__ */
