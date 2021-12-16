#ifndef __linedisc_h__
#define __linedisc_h__


#define MAXLINES 25
#define MAXLEN 4096

#define OC_print_line 10
#define OC_read_line  11

#define RC_retry      25

typedef struct linecont {

  struct linecont *next;
  uint8_t chars[4096];
  uint32_t offset;
  uint32_t len;

} Line;


#endif /* __linedisc_h__ */
