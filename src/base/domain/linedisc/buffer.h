#ifndef __LINEBUFFER_H__
#define __LINEBUFFER_H__

#include <idl/capros/linedisc.h>

#define CCEQ(val, c) ((c) == (val))
#define ISSET(t, f)  ((t) & (f))

#define MAXLEN 4096
typedef struct LDBuffer LDBuffer;
struct LDBuffer {
  uint16_t chars[MAXLEN];
  int32_t cursor;
  int32_t len;
};

/* Initialize a line buffer */
void buffer_clear(LDBuffer *buf);

/* Attempt to append a character to the buffer.  Returns false if any
   error, such as buffer full. */
bool buffer_write(cap_t strm, eros_domain_linedisc_termios tstate, 
		  LDBuffer *buf, uint16_t c);

#endif
