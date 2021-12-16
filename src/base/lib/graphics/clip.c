/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
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

#ifndef __linux__
#include <eros/target.h>
#endif
#include <stdlib.h>
#include "clip.h"

/* makes an initial clip_vector_t */
clip_vector_t *
make_clip_vector(int len)
{
    clip_vector_t *dest;
    dest = (clip_vector_t *)malloc(sizeof(clip_region_t)*(len+1)+sizeof(clip_vector_t));
    dest->len = 0;
    dest->allocated = len;
    return dest;
}


/*  Will allocate len space for clip regions and will copy from 0 
    to the lesser of len and orig->len.
*/
clip_vector_t *
vector_expand(clip_vector_t **orig, int len)
{
  clip_vector_t *dest;
  int i;

  dest = make_clip_vector(len);
	
  if ( dest == NULL )
    return NULL;

  dest->len = (*orig)->len;
    
  for (i = 0; i < (*orig)->len; i++) {
    dest->c[i] = (*orig)->c[i];
  }
    
  //    free( *orig );
  *orig = dest;

  return dest;
}


clip_vector_t *
vector_append_rect(clip_vector_t **vec_a, const rect_t *r, bool in)
{
  clip_vector_t *vec_b = NULL;

  //    printf( "VAR: ( %d, %d ) : ( %d, %d ) : %d\n",
  //	    r->topLeft.x, r->topLeft.y,
  //	    r->bottomRight.x, r->bottomRight.y,
  //	    in );

  if (*vec_a == NULL)
      vec_b = make_clip_vector(5);
  else if ((*vec_a)->allocated == 0)
      vec_b = vector_expand(vec_a, 5);
  else if ((*vec_a)->len == (*vec_a)->allocated)
      vec_b = vector_expand(vec_a, (*vec_a)->allocated*2);
  else
      vec_b = *vec_a;

  if (vec_b == NULL) return *vec_a;

  vec_b->c[vec_b->len].r = *r;
  vec_b->c[vec_b->len].in = in;
  vec_b->len++;

  *vec_a = vec_b;
  return vec_b;
}


clip_vector_t *
vector_concat(clip_vector_t **a, clip_vector_t *b)
{
  clip_vector_t *v = NULL;
  int i;

  if ((v=vector_expand(a, (*a)->allocated + b->allocated)) == NULL)
    return *a;

  for(i=0;i<b->len;i++)
    v->c[(*a)->len+i] = b->c[i];
  v->len = (*a)->len + b->len;
  return v;
}

/* precondition: a - a clip_vector_t pointer
 *               in - the value intersection value of the rectangles 
 *               that should be returned
 *  postcondition: returns a vector containing the rectangles with 
 *                 the "in" value through "a".  The return value is
 *                 all rectangles != to "a"
 */
clip_vector_t *
vector_remove_rects(clip_vector_t **a, bool in)
{
  int i,
      innies = 0,
      outties = 0;
  clip_vector_t *b; 
  clip_vector_t *c; 
  
  for (i=0; i < (*a)->len; i++)
      if ((*a)->c[i].in == in)
	  innies++;
      else
	  outties++;

  b = make_clip_vector(innies);
  c = make_clip_vector(outties);

  for (i = 0; i < (*a)->len;i++)
    if ((*a)->c[i].in == in)
      vector_append_rect( &b, &((*a)->c[i].r), (*a)->c[i].in );
    else
      vector_append_rect( &c, &((*a)->c[i].r), (*a)->c[i].in );

  *a = c;

  return b;
}


/* precondition: b is a sub component of a 
 * postcontiion: b and b's siblings are returned in a clip_vector_t
 */
static clip_vector_t *
decompose_single_clip( clip_vector_t **c, rect_t *a, rect_t *b )
{
  rect_t r;

  if ( a->topLeft.y < b->topLeft.y ) 
    {
      // add the top empty rectangle to vector
      r.topLeft = a->topLeft;
      r.bottomRight.x = a->bottomRight.x;
      r.bottomRight.y = b->topLeft.y;
      vector_append_rect(c,&r,false);
    }

  if ( a->topLeft.x < b->topLeft.x ) 
    {	
      // add empty to the left of b
      r.topLeft.y = b->topLeft.y;
      r.topLeft.x = a->topLeft.x;
      r.bottomRight.y = b->bottomRight.y;
      r.bottomRight.x = b->topLeft.x;	
      vector_append_rect(c,&r,false);
    }
	
  // add b
  vector_append_rect(c,b, true);

  if ( a->bottomRight.x > b->bottomRight.x ) 
    {
      // add empty to the right of b
      r.topLeft.y = b->topLeft.y;
      r.topLeft.x = b->bottomRight.x;
      r.bottomRight.y = b->bottomRight.y;
      r.bottomRight.x = a->bottomRight.x;
      vector_append_rect(c,&r,false);
    }

  if ( a->bottomRight.y > b->bottomRight.y ) 
    {
      // add bottom band to vector
      r.topLeft.y = b->bottomRight.y;
      r.topLeft.x = a->topLeft.x;
      r.bottomRight = a->bottomRight;
      vector_append_rect(c,&r,false);
    }
    
  return  *c;
}


clip_vector_t *
decompose_multi_clip( rect_t *a, clip_vector_t *c )
{
  clip_vector_t *v = make_clip_vector(5);
  rect_t r = *a;
  int i;

  while(r.topLeft.y != a->bottomRight.y)
    {
      /** look for the least y greater that r.topLeft.y */
      for(i=0;i<c->len; i++)
	{
	  if (!c->c[i].in)
	    continue;	   

	  if ((r.bottomRight.y > c->c[i].r.bottomRight.y) &&
	      (r.topLeft.y < c->c[i].r.bottomRight.y))       
	    {
	      r.bottomRight.y = c->c[i].r.bottomRight.y;
	    }

	  if ((r.bottomRight.y > c->c[i].r.topLeft.y) &&
	      (r.topLeft.y < c->c[i].r.topLeft.y))
	    {
	      r.bottomRight.y = c->c[i].r.topLeft.y;
	    }
	}
	
      // printf( "\t\t\tDMC: ( %d, %d ) : ( %d, %d )\n", 
      //	 r.topLeft.x, r.topLeft.y, 
      //	 r.bottomRight.x, r.bottomRight.y );

      while (r.topLeft.x < r.bottomRight.x)
	{
	  int i;
	    
	  for (i=0;(i < c->len) && (r.topLeft.x < r.bottomRight.x);i++)
	    {
	      rect_t r_i;

	      if (!c->c[i].in) continue;
		
	      if (!rect_intersect(&r,&c->c[i].r,&r_i)) continue;

	      // printf( "INT: ( %d , %d ) : ( %d, %d )\n", 
	      //	 r_i.topLeft.x, r_i.topLeft.y, 
	      //	 r_i.bottomRight.x, r_i.bottomRight.y );
		
	      if ((r.bottomRight.x >= c->c[i].r.topLeft.x) &&
		  (r.topLeft.x < c->c[i].r.topLeft.x))
		{
		  // printf ( "shift left from %d to %d\n", r.bottomRight.x, c->c[i].r.topLeft.x );
		  r.bottomRight.x = c->c[i].r.topLeft.x;
		  i = 0;
		  continue;
		}

	      if ((r.topLeft.x == c->c[i].r.topLeft.x))
		{
		  // printf ( "shift right from %d to %d\n", r.topLeft.x, c->c[i].r.bottomRight.x );
		  r.topLeft.x = c->c[i].r.bottomRight.x;
		  i = 0;
		  continue;
		}
	    }
	    
	  vector_append_rect( &v, &r, false );
	  r.topLeft.x = r.bottomRight.x;
	  r.bottomRight.x = a->bottomRight.x;
	}

      r.topLeft.y = r.bottomRight.y;
      r.topLeft.x = a->topLeft.x;
      r.bottomRight = a->bottomRight;
    }

  return v;
}

clip_vector_t *
clip(rect_t *r, clip_vector_t *c)
{
  int i;
  rect_t r_i;
    
  clip_vector_t *v = make_clip_vector(5*c->len);

  for (i=0;i<c->len;i++) 
    {
      if (rect_intersect(r,&c->c[i].r,&r_i)) 
	decompose_single_clip(&v, &(c->c[i].r), &r_i);
      else 
	vector_append_rect(&v,&c->c[i].r,false);
    }
#if 0
  vector_concat(&v, decompose_multi_clip(r,v)); 
#endif
  return v;

}

clip_vector_t *
vector_remove_rect_at_index(clip_vector_t **vec, int index) 
{
  clip_vector_t *retvec = NULL;
  int i;

  if (*vec != NULL) {
    if (index >= 0 && index < (*vec)->len) { 
      for (i = index; i < (*vec)->len - 1; i++)
	(*vec)->c[i] = (*vec)->c[i+1];
      (*vec)->len--;
    }
    else
      return (*vec);
  }
  else
    retvec = (*vec) = make_clip_vector(0);

  return retvec;
}
