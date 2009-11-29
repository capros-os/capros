/*
 * Copyright (C) 2008, 2009, Strawberry Development Group
 *
 * This file is part of the CapROS Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <stdlib.h>
#include <linux/blkdev.h>
#include <scsi/scsi_dbg.h>
#include <linux/elevator.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/transport_class.h>
#include <scsi/scsi_dbg.h>
#include <scsi/scsi_device.h>

#include <domain/assert.h>
#include <eros/Invoke.h>

// from scsi/constants.c
void scsi_print_command(struct scsi_cmnd * cmd)
{
  printk("scsi_print_command called");
}

const char *scsi_device_state_name(enum scsi_device_state state)
{
  return "scsi_device_state_name called";
}

// from linux/blkdev.h
#if 0
#endif
void blk_free_tags(struct blk_queue_tag * r)
{
  BUG_ON("unimplemented");
}

void blk_queue_bounce_limit(struct request_queue * r, u64 l)
{
  BUG_ON("unimplemented");
}

void blk_queue_max_sectors(struct request_queue * r, unsigned int l)
{
  BUG_ON("unimplemented");
}

void blk_queue_max_phys_segments(struct request_queue * r, unsigned short l)
{
  BUG_ON("unimplemented");
}

void blk_queue_max_hw_segments(struct request_queue * r, unsigned short l)
{
  BUG_ON("unimplemented");
}

void blk_queue_segment_boundary(struct request_queue * r, unsigned long l)
{
  BUG_ON("unimplemented");
}

void blk_queue_prep_rq(struct request_queue * r, prep_rq_fn * pfn)
{
  BUG_ON("unimplemented");
}

void blk_queue_softirq_done(struct request_queue * r, softirq_done_fn * fn)
{
  BUG_ON("unimplemented");
}

void blk_cleanup_queue(struct request_queue * r)
{
  BUG_ON("unimplemented");
}

void blk_dump_rq_flags(struct request * r, char * s)
{
  BUG_ON("unimplemented");
}

void blk_plug_device(struct request_queue * r)
{
  BUG_ON("unimplemented");
}

struct request_queue * blk_init_queue(request_fn_proc * p, spinlock_t * lk)
{
  BUG_ON("unimplemented");
  return NULL;
}

void blk_run_queue(struct request_queue * q)
{
  BUG_ON("unimplemented");
}

int blk_rq_map_sg(struct request_queue * r, struct request * rq,
  struct scatterlist * sg)
{
  BUG_ON("unimplemented");
  return -1;
}

void blk_put_request(struct request * r)
{
  BUG_ON("unimplemented");
}

struct request * blk_get_request(struct request_queue * q, int l, gfp_t f)
{
  BUG_ON("unimplemented");
  return NULL;
}

int blk_rq_map_kern(struct request_queue * q, struct request * r,
  void * p, unsigned int l, gfp_t f)
{
  BUG_ON("unimplemented");
  return -1;
}

int blk_execute_rq(struct request_queue * q, struct gendisk * g,
                          struct request * r, int l)
{
  BUG_ON("unimplemented");
  return -1;
}

void blk_complete_request(struct request * r)
{
  BUG_ON("unimplemented");
}

void blk_requeue_request(struct request_queue * q, struct request * r)
{
  BUG_ON("unimplemented");
}

int blk_queue_start_tag(struct request_queue * q, struct request * r)
{
  BUG_ON("unimplemented");
  return -1;
}

void blk_queue_end_tag(struct request_queue * q, struct request * r)
{
  BUG_ON("unimplemented");
}

void end_that_request_last(struct request * r, int l)
{
  BUG_ON("unimplemented");
}

int end_that_request_chunk(struct request * r, int l, int m)
{
  BUG_ON("unimplemented");
  return -1;
}

// scsi/scsi_dbg.h
void scsi_print_sense_hdr(const char * s, struct scsi_sense_hdr * h)
{
  BUG_ON("unimplemented");
}

void scsi_print_sense(char * s, struct scsi_cmnd * c)
{
  BUG_ON("unimplemented");
}

void scsi_print_result(struct scsi_cmnd * c)
{
  BUG_ON("unimplemented");
}

void __scsi_print_command(unsigned char * s)
{
  BUG_ON("unimplemented");
}

// linux/genhd.h
void add_disk_randomness(struct gendisk * disk)
{
  BUG_ON("unimplemented");
}

// linux/elevator.h
void elv_dequeue_request(struct request_queue * q, struct request * r)
{
  BUG_ON("unimplemented");
}

struct request *elv_next_request(struct request_queue *q)
{
  BUG_ON("unimplemented");
  return NULL;
}

// scsi_error
int scsi_execute_async(struct scsi_device *sdev, const unsigned char *cmd,
                       int cmd_len, int data_direction, void *buffer, unsigned bufflen,
                       int use_sg, int timeout, int retries, void *privdata,
                       void (*done)(void *, char *, int, int), gfp_t gfp)
{
  BUG_ON("unimplemented");
  return -1;
}

#if 0
// linux/transport_class.h
void transport_remove_device(struct device * dev)
{
}

void transport_add_device(struct device * dev)
{
}

void transport_setup_device(struct device * dev)
{
  printk("transport_*_device functions are not implemented\n");
}

void transport_configure_device(struct device * dev)
{
}

void transport_destroy_device(struct device * dev)
{
}

void device_del(struct device * dev)
{
  BUG_ON("unimplemented");
}
#endif

void set_disk_ro(struct gendisk * disk, int flag)
{
  // not implemented
}
