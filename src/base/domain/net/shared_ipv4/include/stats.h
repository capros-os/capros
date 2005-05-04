#ifndef __STATS_H__
#define __STATS_H__

#include <eros/target.h>

#include "include/mem.h"
#include "include/memp.h"
#include "include/opt.h"

#if ERIP_STATS

struct stats_proto {
  uint16_t xmit;    /* Transmitted packets. */
  uint16_t rexmit;  /* Retransmitted packets. */
  uint16_t recv;    /* Received packets. */
  uint16_t fw;      /* Forwarded packets. */
  uint16_t drop;    /* Dropped packets. */
  uint16_t chkerr;  /* Checksum error. */
  uint16_t lenerr;  /* Invalid length error. */
  uint16_t memerr;  /* Out of memory error. */
  uint16_t rterr;   /* Routing error. */
  uint16_t proterr; /* Protocol error. */
  uint16_t opterr;  /* Error in options. */
  uint16_t err;     /* Misc error. */
  uint16_t cachehit;
};

struct stats_mem {
  mem_size_t avail;
  mem_size_t used;
  mem_size_t max;  
  mem_size_t err;
};

struct stats_pbuf {
  uint16_t avail;
  uint16_t used;
  uint16_t max;  
  uint16_t err;

  uint16_t alloc_locked;
  uint16_t refresh_locked;
};

struct stats_syselem {
  uint16_t used;
  uint16_t max;
  uint16_t err;
};

struct stats_sys {
  struct stats_syselem sem;
  struct stats_syselem mbox;
};

struct stats_ {
  struct stats_proto link;
  struct stats_proto ip_frag;
  struct stats_proto ip;
  struct stats_proto icmp;
  struct stats_proto udp;
  struct stats_proto tcp;
  struct stats_pbuf pbuf;
  struct stats_mem mem;
  struct stats_mem memp[MEMP_MAX];
  struct stats_sys sys;
};

extern struct stats_ erip_stats;


void stats_init(void);
#else
#define stats_init()
#endif /* ERIP_STATS */
#endif /* __STATS_H__ */




