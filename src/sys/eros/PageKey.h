#ifndef __PAGEKEY_H__
#define __PAGEKEY_H__

/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime library.
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

/*
 * This file resides in eros/ because the kernel and the invocation
 * library various must agree on the values.
 */

/* Local Variables: */
/* comment-column:34 */
/* End: */

/* ORDER and RESULT code values: */

#define OC_Page_MakeReadOnly	1
#define OC_Page_Zero		2
#define OC_Page_Clone           3
#define OC_Page_LssAndPerms     4

#ifndef ASSEMBLER
uint32_t page_clone(uint32_t krPage, uint32_t krFromPage);
uint32_t page_write(uint32_t krPage, uint32_t start, const void *data, uint32_t len);
#endif

#endif /* __PAGEKEY_H__ */
