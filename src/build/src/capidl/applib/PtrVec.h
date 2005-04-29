#ifndef __PTRVEC_H__
#define __PTRVEC_H__

/*
 * Copyright (C) 2002, The EROS Group, LLC.
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

typedef struct PtrVec PtrVec;
struct PtrVec {
  unsigned      size;
  unsigned      maxSize;
  void          **elements;
};

PtrVec *ptrvec_create(void);
void    ptrvec_destroy(PtrVec*);
void    ptrvec_append(PtrVec *vec, void *vp);
void    ptrvec_insert(PtrVec *vec, void *vp, unsigned ndx);
void    ptrvec_set(PtrVec *vec, unsigned ndx, void *vp);
void    ptrvec_sort_using(PtrVec *vec, int (*cmp)(const void *, const void *));
PtrVec *ptrvec_shallow_copy(PtrVec *vec);
bool    ptrvec_contains(PtrVec *vec, void *vp);

#define vec_len(vec)       ((vec)->size)
#define vec_fetch(vec,ndx)  ((vec)->elements[(ndx)])
#define symvec_fetch(vec,ndx)  ((Symbol *)(vec)->elements[(ndx)])

/* These leverage the underlying representation pun... */
extern void vec_remove(void *vec, unsigned ndx);
extern void vec_reset(void *vec);

void ptrvec_sort_using(PtrVec *vec, int (*cmp)(const void *, const void *));

#endif /* __PTRVEC_H__ */
