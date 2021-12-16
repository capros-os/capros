#ifndef __BLOCKDEV_HXX__
#define __BLOCKDEV_HXX__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System.
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

#include <kerninc/Thread.hxx>

struct CHS {
  uint32_t cyl;
  uint32_t hd;
  uint32_t sec;
};

struct Request;
struct Invocation;
struct ObjectHeader;

struct BlockUnitInfo {
  CHS      b_geom;		/* geometry as seen by BIOS */
  CHS      d_geom;		/* drive geometry (IDE logical geometry) */
  uint64_t  nSecs;		/* size in sectors */

  bool     isDisk;
  bool     isEros;		/* might be raw eros volume */
  bool     isBoot;
  bool     isMounted;
  bool     needsMount;
  bool     hasPartitions;
};

/* Common base class for all block-oriented controllers: */
struct BlockDev {
protected:
  static uint32_t totalMountedUnits;
  static struct Task *pTask;
  static void StartAll(struct Task*);
  
  friend class MountDaemon;

  /* Worker function for the mount thread: */
  static void CheckErosPartition(struct Partition *);
  /* Worker function for the mount thread: */
  static bool DoAutoMount();

  bool isRegistered;

  static ThreadPile MountWait;
public:
  char *name;
  uint16_t  devClass;

  static BlockDev* registry[];

  INLINE
  static BlockDev *Get(uint32_t ndx)
  {
    return registry[ndx];
  }

  void Invoke(Invocation& inv);
  
  bool inDuplexWait;
  static BlockDev *readyChain;
  
  BlockDev *next;		/* in duplex stall chain or ready chain */
  
  uint32_t nUnits;
  
  void Register();
  void Unregister();
  
  static void Init();
  static void ActivateTask();

  static void RunMountDaemon();
  static void WakeMountWaiters();
  static void WaitForMount();

  INLINE static uint32_t TotalMountedUnits()
  {
    return totalMountedUnits;
  }
  
  virtual void GetUnitInfo(uint8_t unit, BlockUnitInfo& ui) = 0;
  virtual void MountUnit(uint8_t unit) = 0;
  
  virtual void InsertRequest(Request* req) = 0;
  virtual void StartIO() = 0;

  void DoDeviceRead(uint8_t unit, ObjectHeader *pOb, uint32_t startSec,
		  uint32_t nSec);
  void DoDeviceWrite(uint8_t unit, ObjectHeader *pOb, uint32_t startSec,
		   uint32_t nSec);
  
#if 0
  /* For completeness -- not currently used */
  void DevicePlug(uint8_t unit);
#endif
  static void PlugAllBlockDevices(void (*callBack)(struct DuplexedIO *));
  
  BlockDev();

  virtual ~BlockDev()
  {
    Unregister();
  }
};

#endif /* __BLOCKDEV_HXX__ */
