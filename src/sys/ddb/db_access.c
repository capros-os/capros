/*	$NetBSD: db_access.c,v 1.8 1994/10/09 08:37:35 mycroft Exp $	*/

/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 *
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */

#include <arch-kerninc/db_machdep.h>		/* type definitions */
#include <eros/endian.h>

#include <ddb/db_access.h>

/*
 * Access unaligned data items on aligned (longword)
 * boundaries.
 */

int db_extend[] = {	/* table for sign-extending */
	0,
	0xFFFFFF80,
	0xFFFF8000,
	0xFF800000
};

db_expr_t
db_get_value(db_addr_t addr, size_t size, bool is_signed)
{
	char data[sizeof(int)];
	register db_expr_t value;
	register int i;

	db_read_bytes(addr, size, data);

	value = 0;
#if BYTE_ORDER == LITTLE_ENDIAN
	for (i = size - 1; i >= 0; i--)
#else /* BYTE_ORDER == BIG_ENDIAN */
	for (i = 0; (unsigned) i < size; i++)
#endif /* BYTE_ORDER */
		value = (value << 8) + (data[i] & 0xFF);
	    
	if (size < 4 && is_signed && (value & db_extend[size]) != 0)
		value |= db_extend[size];
	return (value);
}

void
db_put_value(db_addr_t addr, size_t size, db_expr_t value)
{
	char data[sizeof(int)];
	register int i;

#if BYTE_ORDER == LITTLE_ENDIAN
	for (i = 0; (unsigned) i < size; i++)
#else /* BYTE_ORDER == BIG_ENDIAN */
	for (i = size - 1; i >= 0; i--)
#endif /* BYTE_ORDER */
	{
		data[i] = value & 0xFF;
		value >>= 8;
	}

	db_write_bytes(addr, size, data);
}
