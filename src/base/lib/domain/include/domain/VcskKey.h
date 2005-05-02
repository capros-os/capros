#ifndef __VCSKKEY_H__
#define __VCSKKEY_H__

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

#include <eros/ProcessState.h>
#include <eros/SegmentKey.h>

/* Honors the following segment orders: */
#define OC_Vcsk_InvokeKeeper  OC_SEGFAULT /* 0 */
#define OC_Vcsk_MakeSpaceKey  OC_Seg_MakeSpaceKey /* 1 */

#define OC_Vcsk_Truncate        16
#define OC_Vcsk_Pack            17

/* Following order codes are for FSK: */
#define OC_FSK_Freeze          18
#define OC_FSK_MakeSibling     18

#define RC_Vcsk_Malformed      1

#endif /* __VCSKKEY_H__ */
