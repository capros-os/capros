/*	$NetBSD: db_watch.h,v 1.8 1994/10/09 08:41:20 mycroft Exp $	*/

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
 * 	Author: David B. Golub, Carnegie Mellon University
 *	Date:	10/90
 */

#ifndef	_OPTION_DDB_DB_WATCH_
#define	_OPTION_DDB_DB_WATCH_

/*
 * Watchpoint.
 */
typedef struct db_watchpoint {
	vm_map_t map;			/* in this map */
	db_addr_t loaddr;		/* from this address */
	db_addr_t hiaddr;		/* to this address */
	struct db_watchpoint *link;	/* link in in-use or free chain */
} *db_watchpoint_t;

bool db_find_watchpoint (vm_map_t, db_addr_t, db_regs_t *);
void db_set_watchpoints (void);
void db_clear_watchpoints (void);

void db_set_watchpoint (vm_map_t, db_addr_t, vm_size_t);
void db_delete_watchpoint (vm_map_t, db_addr_t);
void db_list_watchpoints (void);

#endif	/* _OPTION_DDB_DB_WATCH_ */
