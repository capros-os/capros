#ifndef _ETHERNET_H__
#define _ETHERNET_H__

/*The number of bytes in an ethernet (MAC) address*/
#define	ETHER_ADDR_LEN	6

/*The number of bytes in the type field*/
#define	ETHER_TYPE_LEN	2

/*The number of bytes in the trailing CRC field*/
#define	ETHER_CRC_LEN	4

/*The length of the combined header*/
#define	ETHER_HDR_LEN	(ETHER_ADDR_LEN*2+ETHER_TYPE_LEN)

/*The minimum packet length*/
#define	ETHER_MIN_LEN	64

/*The maximum packet length*/
#define	ETHER_MAX_LEN	1518

#define	ETHERMTU	(ETHER_MAX_LEN-ETHER_HDR_LEN-ETHER_CRC_LEN)
#define	ETHERMIN	(ETHER_MIN_LEN-ETHER_HDR_LEN-ETHER_CRC_LEN)

/*A macro to validate a length with*/
#define	ETHER_IS_VALID_LEN(foo)	\
	((foo) >= ETHER_MIN_LEN && (foo) <= ETHER_MAX_LEN)

/*Structure of a 10Mb/s Ethernet header*/
struct	ether_header {
  uint8_t  ether_dhost[ETHER_ADDR_LEN];
  uint8_t  ether_shost[ETHER_ADDR_LEN];
  uint8_t ether_type[2];
};

struct ether_pkt {
  struct ether_header header;
  char   data[ETHERMTU];
};


/*Structure of a 48-bit Ethernet address*/
struct	ether_addr {
  uint8_t octet[ETHER_ADDR_LEN];
};

/* Type of packet the ethernet carries */
#define	ETHERTYPE_8023	    0x0004  /*IEEE 802.3 packet*/
#define ETHERTYPE_IP        0x0800  /*IP Protocol*/
#define	ETHERTYPE_IPV6	    0x86DD  /* IP protocol version 6 */
#define	ETHERTYPE_LOOPBACK  0x9000  /* Loopback: used to test interfaces */
#define	ETHERTYPE_MAX	    0xFFFF  /*Maximum valid ethernet type, reserved*/

#endif /* _ETHERNET_H__ */


