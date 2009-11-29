/*
 * Copyright (C) 2009, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library.
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

#include <stdlib.h>
#include <linuxk/linux-emul.h>
#include <linux/async.h>
#include <linux/mutex.h>

async_cookie_t next_cookie = 1;
static DEFINE_MUTEX(async_lock);

async_cookie_t
async_schedule(async_func_ptr * ptr, void * data)
{
  /* Always execute synchronously.
     In CapROS you can use threads to achieve asynchronous execution. */

  mutex_lock(&async_lock);
  async_cookie_t newcookie = next_cookie++;
  mutex_unlock(&async_lock);

  ptr(data, newcookie);
  return newcookie;
}
