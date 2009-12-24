#ifndef _SCSI_PRIV_H
#define _SCSI_PRIV_H
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

#include <idl/capros/Node.h>
#include <linux/device.h>

struct request_queue;
struct request;
struct scsi_cmnd;
struct scsi_device;
struct scsi_host_template;
struct Scsi_Host;
struct scsi_nl_hdr;


/*
 * Scsi Error Handler Flags
 */
#define SCSI_EH_CANCEL_CMD	0x0001	/* Cancel this cmd */

#define SCSI_SENSE_VALID(scmd) \
	(((scmd)->sense_buffer[0] & 0x70) == 0x70)

/* hosts.c */
extern int scsi_init_hosts(void);
extern void scsi_exit_hosts(void);

/* scsi.c */
extern int scsi_dispatch_cmd(struct scsi_cmnd *cmd,
  int * resultp, unsigned int * residualp);
extern int scsi_setup_command_freelist(struct Scsi_Host *shost);
extern void scsi_destroy_command_freelist(struct Scsi_Host *shost);
#ifdef CONFIG_SCSI_LOGGING
void scsi_log_send(struct scsi_cmnd *cmd);
void scsi_log_completion(struct scsi_cmnd *cmd, int disposition);
#else
static inline void scsi_log_send(struct scsi_cmnd *cmd) 
	{ };
static inline void scsi_log_completion(struct scsi_cmnd *cmd, int disposition)
	{ };
#endif

/* scsi_devinfo.c */
extern int scsi_get_device_flags(struct scsi_device *sdev,
				 const char * vendor,
				 const char * model);
extern int __init scsi_init_devinfo(void);
extern void scsi_exit_devinfo(void);

/* scsi_error.c */
extern enum blk_eh_timer_return scsi_times_out(struct request *req);
extern int scsi_error_handler(void *host);
extern int scsi_decide_disposition(struct scsi_cmnd *cmd);
extern void scsi_eh_wakeup(struct Scsi_Host *shost);
extern int scsi_eh_scmd_add(struct scsi_cmnd *, int);
void scsi_eh_ready_devs(struct Scsi_Host *shost,
			struct list_head *work_q,
			struct list_head *done_q);
int scsi_eh_get_sense(struct list_head *work_q,
		      struct list_head *done_q);
int scsi_noretry_cmd(struct scsi_cmnd *scmd);

/* scsi_lib.c */
extern int scsi_maybe_unblock_host(struct scsi_device *sdev);
extern void scsi_device_unbusy(struct scsi_device *sdev);
extern int scsi_queue_insert(struct scsi_cmnd *cmd, int reason);
extern void scsi_next_command(struct scsi_cmnd *cmd);
extern void scsi_io_completion(struct scsi_cmnd *, unsigned int,
  int * resultp, unsigned int * residualp);
extern void scsi_run_host_queues(struct Scsi_Host *shost);
extern struct request_queue *scsi_alloc_queue(struct scsi_device *sdev);
extern void scsi_free_queue(struct request_queue *q);
extern int scsi_init_queue(void);
extern void scsi_exit_queue(void);
struct request_queue;
struct request;
extern int scsi_prep_fn(struct request_queue *, struct request *);
extern struct kmem_cache *scsi_sdb_cache;

/* scsi_proc.c */
#ifdef CONFIG_SCSI_PROC_FS
extern void scsi_proc_hostdir_add(struct scsi_host_template *);
extern void scsi_proc_hostdir_rm(struct scsi_host_template *);
extern void scsi_proc_host_add(struct Scsi_Host *);
extern void scsi_proc_host_rm(struct Scsi_Host *);
extern int scsi_init_procfs(void);
extern void scsi_exit_procfs(void);
#else
# define scsi_proc_hostdir_add(sht)	do { } while (0)
# define scsi_proc_hostdir_rm(sht)	do { } while (0)
# define scsi_proc_host_add(shost)	do { } while (0)
# define scsi_proc_host_rm(shost)	do { } while (0)
# define scsi_init_procfs()		(0)
# define scsi_exit_procfs()		do { } while (0)
#endif /* CONFIG_PROC_FS */

/* scsi_scan.c */
extern int scsi_scan_host_selected(struct Scsi_Host *, unsigned int,
				   unsigned int, unsigned int, int);
extern void scsi_forget_host(struct Scsi_Host *);
extern void scsi_rescan_device(struct device *);

/* scsi_sysctl.c */
#ifdef CONFIG_SYSCTL
extern int scsi_init_sysctl(void);
extern void scsi_exit_sysctl(void);
#else
# define scsi_init_sysctl()		(0)
# define scsi_exit_sysctl()		do { } while (0)
#endif /* CONFIG_SYSCTL */

/* scsi_sysfs.c */
extern int scsi_sysfs_add_sdev(struct scsi_device *);
extern int scsi_sysfs_add_host(struct Scsi_Host *);
extern int scsi_sysfs_register(void);
extern void scsi_sysfs_unregister(void);
extern void scsi_sysfs_device_initialize(struct scsi_device *);
extern int scsi_sysfs_target_initialize(struct scsi_device *);
extern struct scsi_transport_template blank_transport_template;
extern void __scsi_remove_device(struct scsi_device *);

extern struct bus_type scsi_bus_type;
extern struct attribute_group *scsi_sysfs_shost_attr_groups[];

/* scsi_netlink.c */
#ifdef CONFIG_SCSI_NETLINK
extern void scsi_netlink_init(void);
extern void scsi_netlink_exit(void);
extern struct sock *scsi_nl_sock;
#else
static inline void scsi_netlink_init(void) {}
static inline void scsi_netlink_exit(void) {}
#endif

/* 
 * internal scsi timeout functions: for use by mid-layer and transport
 * classes.
 */

#define SCSI_DEVICE_BLOCK_MAX_TIMEOUT	600	/* units in seconds */
extern int scsi_internal_device_block(struct scsi_device *sdev);
extern int scsi_internal_device_unblock(struct scsi_device *sdev);

void scsi_softirq_done_cmd(struct scsi_cmnd * cmd,
  int * resultp, unsigned int * residualp);

static inline capros_Node_extAddr_t
SCSIDevCapSlot(struct Scsi_Host * shost)
{
  // The SCSIDevice cap is stored at an index equal to the address shost.
  return (capros_Node_extAddr_t)shost;
}

#endif /* _SCSI_PRIV_H */
