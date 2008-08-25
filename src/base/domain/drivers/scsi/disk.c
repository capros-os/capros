/*
 * Copyright (C) 2008, Strawberry Development Group
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

#include <scsi/scsi.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_host.h>
#include <linux/genhd.h>

#include "disk.h"
#include <domain/assert.h>
#include <eros/Invoke.h>
#include <disk/LowVolume.h>
#include <eros/machine/DevPrivs.h>
#include <eros/machine/IORQ.h>

#define KR_IORQ KR_APP2(0)

#define dbg_mount 0x1
#define dbg_server 0x2

/* Following should be an OR of some of the above */
#define dbg_flags ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

/* Get a little-endian 4-byte value that may be unaligned. */
static uint32_t
getLE32(const uint8_t * p)
{
  return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static void
putBE16(uint16_t value, uint8_t * p)
{
  p[0] = value >> 8;
  p[1] = value;
}

static void
putBE32(uint32_t value, uint8_t * p)
{
  p[0] = value >> 24;
  p[1] = value >> 16;
  p[2] = value >> 8;
  p[3] = value;
}

static int
xferSDev(struct scsi_device * sdev, 
  uint32_t startSector, uint32_t nrSects,	// bytes / EROS_SECTOR_SIZE
  void * buffer, dma_addr_t buffer_dma,
  int data_direction,
  uint8_t opcode)
{
  int err;
  struct scsi_sense_hdr sshdr;
  uint8_t scsi_cmd[MAX_COMMAND_SIZE];
  scsi_cmd[0] = opcode;	// READ_10 or WRITE_10
  scsi_cmd[1] = 0;
  putBE32(startSector, &scsi_cmd[2]);
  scsi_cmd[6] = 0;
  putBE16(nrSects, &scsi_cmd[7]);
  scsi_cmd[9] = 0;

  err = scsi_execute_req(sdev, scsi_cmd, data_direction, 
                         buffer, buffer_dma, 
                         nrSects * EROS_SECTOR_SIZE, &sshdr,
                         10*HZ, 3);
  if (err)
    printk("xferSDev got err %d\n", err);

  return err;
}

static inline int
readSDev(struct scsi_device * sdev, 
  uint32_t startSector, uint32_t nrSects,	// bytes / EROS_SECTOR_SIZE
  void * buffer, dma_addr_t buffer_dma)
{
  return xferSDev(sdev, startSector, nrSects, buffer, buffer_dma,
                  DMA_FROM_DEVICE, READ_10);
}

static void
parse_capros_partition(struct scsi_device * sdev,
  uint32_t startSector, uint32_t nrSects,
  uint8_t * buffer, dma_addr_t buffer_dma)
{
  result_t result;
  DEBUG(mount) kprintf(KR_OSTREAM, "found capros partition\n");

  // Read the volume header.
  readSDev(sdev, startSector,
           EROS_PAGE_SIZE / EROS_SECTOR_SIZE, buffer, buffer_dma);
  VolHdr * pVolHdr = (VolHdr *)buffer;
  assert(pVolHdr->PageSize == EROS_PAGE_SIZE);

  uint32_t DivTableSector = pVolHdr->DivTable;
  assert(! pVolHdr->AltDivTable);	// not supported yet
  // Read the division table.
  readSDev(sdev, startSector + DivTableSector,
           EROS_PAGE_SIZE / EROS_SECTOR_SIZE, buffer, buffer_dma);
  DEBUG(mount) kprintf(KR_OSTREAM, "Div table at %#x sector %#x+%#x\n",
                 buffer, startSector, DivTableSector);
  
  // The division table is an array of NDIVENT Divisions.
  int i;
  for (i = 0; i < NDIVENT; i++) {
    Division * d = &((Division *)buffer)[i];
    switch (d->type) {
    case dt_Log:
      DEBUG(mount) kprintf(KR_OSTREAM, "Log division at %#llx\n", d->startOid);
      // Sanity check the division:
      if (d->end <= d->start) {
        kdprintf(KR_OSTREAM, "Division %#x invalid!\n", d);
      }

      // Register this Division with the kernel:
      result = capros_IOReqQ_registerLIDRange(KR_IORQ, d->startOid, d->endOid,
                                            startSector + d->start);
      assert(result == RC_OK);
      break;

    case dt_Object:
      DEBUG(mount) kprintf(KR_OSTREAM, "Obj division oid %#llx at %d\n",
                     d->startOid, d->start);
      // Sanity check the division:
      if (d->end <= d->start) {
        kdprintf(KR_OSTREAM, "Division %#x invalid!\n", d);
      }

      // Register this Division with the kernel:
      result = capros_IOReqQ_registerOIDRange(KR_IORQ, d->startOid, d->endOid,
                                            startSector + d->start);
      assert(result == RC_OK);
      break;

    default:
      kdprintf(KR_OSTREAM, "Division[%d] has unrecognized type %d!\n",
              i, d->type);
    case dt_Boot:
    case dt_Kernel:
    case dt_DivTbl:
    case dt_Unused: ;
    }
  }
}

/* This thread serves the IORQ for this lun. */
void *
disk_thread(void * arg)
{
  struct scsi_device * sdev = arg;
  struct Scsi_Host * shost = sdev->host;
  result_t result;
  int i;
  uint8_t * buffer;
  dma_addr_t buffer_dma;

  DEBUG(server) kdprintf(KR_OSTREAM, "disk_thread started.\n");

  buffer = dma_alloc_coherent(shost->shost_gendev.parent,
                              EROS_PAGE_SIZE, &buffer_dma, 0);
  if (!buffer) {
    kdprintf(KR_OSTREAM, "Can't alloc DMA!\n");
    return NULL;
  }

  readSDev(sdev, 0, 1, buffer, buffer_dma);

  if (buffer[510] != 0x55 || buffer[511] != 0xaa) {
    kdprintf(KR_OSTREAM, "MBR has bad signature! %#x\n", buffer);
    return NULL;
  }

  bool caprosPartitions = false;

  capros_DevPrivs_declarePFHProcess(KR_DEVPRIVS, KR_SELF);

  // Scan the primary partitions:
  struct partition * parti;
  for (i = 0, parti = (struct partition *)&buffer[446]; i < 4; i++, parti++) {
    uint32_t nrSects = getLE32((uint8_t *)&parti->nr_sects);
    DEBUG(mount) printk("Partition %d type=%#x sectors %#x\n",
                        i, parti->sys_ind, nrSects);
    if (nrSects == 0)
      continue;

    uint32_t startSector = getLE32((uint8_t *)&parti->start_sect);

    switch (parti->sys_ind) {
    default:
      break;	// ignore unknown partitions

    case 0x05:
    case 0x0f:
    case 0x85:	// Linux extended partition
      assert(false);	// not implemented yet
      break;

    case EROS_PARTITION_TYPE:
      if (! caprosPartitions) {	// first one
        result = capros_DevPrivs_allocateIORQ(KR_DEVPRIVS, KR_IORQ);
        if (result != RC_OK) {
          kprintf(KR_OSTREAM, "disk_thread could not create IORQ!\n");
          return NULL;
        }

        caprosPartitions = true;
      }
      parse_capros_partition(sdev, startSector, nrSects, buffer, buffer_dma);
    }
  }

  if (! caprosPartitions) {
    return NULL;
  }

  DEBUG(server) kprintf(KR_OSTREAM, "disk_thread serving queue\n");
  for (;;) {
    result_t result;
    capros_IOReqQ_IORequest Ioreq;
    int err;
    unsigned int opcode;
    int data_direction;

    result = capros_IOReqQ_waitForRequest(KR_IORQ, &Ioreq);
    assert(result == RC_OK);

    DEBUG(server) kprintf(KR_OSTREAM, "disk_thread serving request, %#x\n",
                          &Ioreq);
    switch (Ioreq.requestType) {
    default: ;
      assert(false);

    case capros_IOReqQ_RequestType_readRangeLoc:
      opcode = READ_10;
      data_direction = DMA_FROM_DEVICE;
      goto xferRangeLoc;

    case capros_IOReqQ_RequestType_writeRangeLoc:
      opcode = WRITE_10;
      data_direction = DMA_TO_DEVICE;
    xferRangeLoc: ;
      // At the moment there is no parallelism in serving I/O requests.
      err = xferSDev(sdev,
              Ioreq.rangeOpaque // starting sector of the range
              + Ioreq.rangeLoc * (EROS_PAGE_SIZE / EROS_SECTOR_SIZE),
              EROS_PAGE_SIZE / EROS_SECTOR_SIZE,
              NULL, Ioreq.bufferDMAAddr, data_direction, opcode);
      result = capros_IOReqQ_completeRequest(KR_IORQ,
                 Ioreq.requestID, err);
      assert(result == RC_OK);
      break;

    case capros_IOReqQ_RequestType_synchronizeCache:
    {
      int sd_issue_flush(struct device *, sector_t *);
      int err = sd_issue_flush(&sdev->sdev_gendev, NULL /* not used */ );
      if (err)
        printk("Sync cache got err %d\n", err);

      result = capros_IOReqQ_completeRequest(KR_IORQ,
                 Ioreq.requestID, err);
      assert(result == RC_OK);
      break;
    }
    }
  }
}

void
mount_capros_disk(struct scsi_device * sdev)
{
  kprintf(KR_OSTREAM, "Mounting sdev %#x lun %d\n", sdev, sdev->lun);
#if 1
  result_t result;

  switch (sdev->type) {
  case TYPE_RBC:
  case TYPE_DISK:
    if (sdev->inq_periph_qual == 0) {	// a device is connected
      result = lthread_new_thread(4096, &disk_thread, sdev, &sdev->threadNum);
      if (result != RC_OK) {
        kprintf(KR_OSTREAM, "mount_capros_disk could not create thread!\n");
        return;
      }
    }
  }
#endif
}
