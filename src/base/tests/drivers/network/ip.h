/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System distribution.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

/* Internet Protocol : see rfc 791. Taken from freebsd */

#ifndef _IP_H
#define _IP_H

#include "in.h"

/* From rfc 791 */

typedef struct{
  uint8_t   ip_vhl;           /* header length & version */	
  uint8_t   ip_tos;           /* type of service */
  uint16_t  ip_len;           /* total length */
  uint16_t  ip_id;            /* identification */
  uint16_t  ip_off;           /* fragment offset field */
#define	IP_RF 0x8000	      /* reserved fragment flag */
#define	IP_DF 0x4000	      /* dont fragment flag */
#define	IP_MF 0x2000	      /* more fragments flag */
#define	IP_OFFMASK 0x1fff     /* mask for fragmenting bits */
  uint8_t  ip_ttl;	      /* time to live */
  uint8_t  ip_p;	      /* protocol */
  uint16_t ip_sum;	      /* checksum */
  IN_ADDR ip_src,ip_dst;      /* source and dest address */
}IP_HEADER;

#define	IP_MAXPACKET	65535	/* maximum packet size */

/* Definitions for IP type of service (ip_tos) */
#define	IPTOS_LOWDELAY	   0x10
#define	IPTOS_THROUGHPUT   0x08
#define	IPTOS_RELIABILITY  0x04
#define	IPTOS_MINCOST	   0x02
/* ECN bits proposed by Sally Floyd */
#define	IPTOS_CE	   0x01	/* congestion experienced */
#define	IPTOS_ECT	   0x02	/* ECN-capable transport */


/* Definitions for IP precedence (also in ip_tos) (hopefully unused) */
#define	IPTOS_PREC_NETCONTROL	     0xe0
#define	IPTOS_PREC_INTERNETCONTROL   0xc0
#define	IPTOS_PREC_CRITIC_ECP	     0xa0
#define	IPTOS_PREC_FLASHOVERRIDE     0x80
#define	IPTOS_PREC_FLASH	     0x60
#define	IPTOS_PREC_IMMEDIATE	     0x40
#define	IPTOS_PREC_PRIORITY	     0x20
#define	IPTOS_PREC_ROUTINE	     0x00

/* Definitions for options.*/
#define	IPOPT_COPIED(o)		((o)&0x80)
#define	IPOPT_CLASS(o)		((o)&0x60)
#define	IPOPT_NUMBER(o)		((o)&0x1f)

#define	IPOPT_CONTROL		0x00
#define	IPOPT_RESERVED1		0x20
#define	IPOPT_DEBMEAS		0x40
#define	IPOPT_RESERVED2		0x60

#define	IPOPT_EOL		0      /* end of option list */
#define	IPOPT_NOP		1      /* no operation */

#define	IPOPT_RR		7      /* record packet route */
#define	IPOPT_TS		68     /* timestamp */
#define	IPOPT_SECURITY		130    /* provide s,c,h,tcc */
#define	IPOPT_LSRR		131    /* loose source route */
#define	IPOPT_ESO		133    /* extended security */
#define	IPOPT_CIPSO		134    /* commerical security */
#define	IPOPT_SATID		136    /* satnet id */
#define	IPOPT_SSRR		137    /* strict source route */
#define	IPOPT_RA		148    /* router alert */

/* Offsets to fields in options other than EOL and NOP */
#define	IPOPT_OPTVAL		0      /* option ID */
#define	IPOPT_OLEN		1      /* option length */
#define IPOPT_OFFSET		2      /* offset within option */
#define	IPOPT_MINOFF		4      /* min value of above */

/* Time stamp option structure */
struct	ip_timestamp {
  uint8_t	ipt_code;		/* IPOPT_TS */
  uint8_t	ipt_len;		/* size of structure (variable) */
  uint8_t	ipt_ptr;		/* index of current entry */
  uint16_t	ipt_flg:4,	     	/* flags, see below */
		ipt_oflw:4;		/* overflow counter */
  union ipt_timestamp {
    uint32_t	ipt_time[1];
    struct	ipt_ta {
      IN_ADDR   ipt_addr;
      uint32_t  ipt_time;
    } ipt_ta[1];
  } ipt_timestamp;
};

/* flag bits for ipt_flg */
#define	IPOPT_TS_TSONLY		0      /* timestamps only */
#define	IPOPT_TS_TSANDADDR	1      /* timestamps and addresses */
#define	IPOPT_TS_PRESPEC	3      /* specified modules only */

/* bits for security (not byte swapped) */
#define	IPOPT_SECUR_UNCLASS	0x0000
#define	IPOPT_SECUR_CONFID	0xf135
#define	IPOPT_SECUR_EFTO	0x789a
#define	IPOPT_SECUR_MMMM	0xbc4d
#define	IPOPT_SECUR_RESTR	0xaf13
#define	IPOPT_SECUR_SECRET	0xd788
#define	IPOPT_SECUR_TOPSECRET	0x6bc5

/* Internet implementation parameters */
#define	MAXTTL		255		/* maximum time to live (seconds) */
#define	IPDEFTTL	64		/* default ttl, from RFC 1340 */
#define	IPFRAGTTL	60		/* time to live for frags, slowhz */
#define	IPTTLDEC	1		/* subtracted when forwarding */

#define	IP_MSS		576		/* default maximum segment size */


#endif /* _IP_H*/
