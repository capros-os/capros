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

void kvprintf(uint32_t streamkey, const char *fmt, void *vap);
void kprintf(uint32_t streamkey, const char *fmt, ...);
void kdprintf(uint32_t streamkey, const char *fmt, ...);
int sprintf(char *pBuf, const char *fmt, ...);
void wrstream(uint32_t streamkey, const char *txt, uint32_t len);
size_t strlen(const char *s);
void ShowKey(uint32_t krConsole, uint32_t krKeyBits, uint32_t kr);
