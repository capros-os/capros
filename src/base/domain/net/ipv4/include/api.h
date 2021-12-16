#ifndef __API_H__
#define __API_H__


#include "opt.h"
#include "pbuf.h"
//FIX:#include "sys.h"
#include "ip.h"
#include "udp.h"
#include "tcp.h"
//#include "err.h"


/*Conversion to EROS types*/
/*FIX: This should finally be removed.*/
#define u8_t  uint8_t
#define u16_t uint16_t
#define u32_t uint32_t
#define u64_t uint64_t
#define s8_t  int8_t
#define s16_t int8_t
#define s32_t int32_t
#define s64_t int64_t
#define err_t uint8_t

#define NETCONN_NOCOPY 0x00
#define NETCONN_COPY   0x01



enum netconn_type {
  NETCONN_TCP,
  NETCONN_UDP,
  NETCONN_UDPLITE,
  NETCONN_UDPNOCHKSUM
};

enum netconn_state {
  NETCONN_NONE,
  NETCONN_WRITE,
  NETCONN_ACCEPT,
  NETCONN_RECV,
  NETCONN_CONNECT,
  NETCONN_CLOSE
};

enum netconn_evt {
  NETCONN_EVT_RCVPLUS,
  NETCONN_EVT_RCVMINUS,
  NETCONN_EVT_SENDPLUS,
  NETCONN_EVT_SENDMINUS
};

struct netbuf {
  struct pbuf *p, *ptr;
  struct ip_addr *fromaddr;
  u16_t fromport;
  err_t err;
};

struct netconn {
  enum netconn_type type;
  enum netconn_state state;
  union {
    struct tcp_pcb *tcp;
    struct udp_pcb *udp;
  } pcb;
  err_t err;
  sys_mbox_t mbox;
  sys_mbox_t recvmbox;
  sys_mbox_t acceptmbox;
  sys_sem_t sem;
  int socket;
  u16_t recv_avail;
  void (* callback)(struct netconn *, enum netconn_evt, u16_t len);
};

/* Network buffer functions: */
struct netbuf *   netbuf_new      (void);
void              netbuf_delete   (struct netbuf *buf);
void *            netbuf_alloc    (struct netbuf *buf, u16_t size);
void              netbuf_free     (struct netbuf *buf);
void              netbuf_ref      (struct netbuf *buf,
				   void *dataptr, u16_t size);
void              netbuf_chain    (struct netbuf *head,
				   struct netbuf *tail);

u16_t             netbuf_len      (struct netbuf *buf);
err_t             netbuf_data     (struct netbuf *buf,
				   void **dataptr, u16_t *len);
s8_t              netbuf_next     (struct netbuf *buf);
void              netbuf_first    (struct netbuf *buf);

void              netbuf_copy     (struct netbuf *buf,
				   void *dataptr, u16_t len);
void              netbuf_copy_partial(struct netbuf *buf, void *dataptr, 
				      u16_t len, u16_t offset);
struct ip_addr *  netbuf_fromaddr (struct netbuf *buf);
u16_t             netbuf_fromport (struct netbuf *buf);

/* Network connection functions: */
struct netconn *  netconn_new     (enum netconn_type type);
struct
netconn *netconn_new_with_callback(enum netconn_type t,
                                   void (*callback)(struct netconn *, enum netconn_evt, u16_t len));
err_t             netconn_delete  (struct netconn *conn);
enum netconn_type netconn_type    (struct netconn *conn);
err_t             netconn_peer    (struct netconn *conn,
				   struct ip_addr *addr,
				   u16_t *port);
err_t             netconn_addr    (struct netconn *conn,
				   struct ip_addr **addr,
				   u16_t *port);
err_t             netconn_bind    (struct netconn *conn,
				   struct ip_addr *addr,
				   u16_t port);
err_t             netconn_connect (struct netconn *conn,
				   struct ip_addr *addr,
				   u16_t port);
err_t             netconn_disconnect (struct netconn *conn);
err_t             netconn_listen  (struct netconn *conn);
struct netconn *  netconn_accept  (struct netconn *conn);
struct netbuf *   netconn_recv    (struct netconn *conn);
err_t             netconn_send    (struct netconn *conn,
				   struct netbuf *buf);
err_t             netconn_write   (struct netconn *conn,
				   void *dataptr, u16_t size,
				   u8_t copy);
err_t             netconn_close   (struct netconn *conn);

err_t             netconn_err     (struct netconn *conn);

#endif /* __API_H__ */


