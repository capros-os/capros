/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
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

#include <assert.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <eros/target.h>
#include <eros/StdKeyType.h>
#include <eros/KeyConst.h>
#include <erosimg/DiskKey.h>
#include <erosimg/Diag.h>

#define __EROS_PRIMARY_KEYDEF(name, isValid, bindTo) #name,
/* OLD_MISCKEY(name) "OBSOLETE" #name, */

static const char *KeyNames[KKT_NUM_KEYTYPE] = {
#include <eros/StdKeyType.h>
};

void
PrintDiskKey(KeyBits key)
{
  switch(keyBits_GetType(&key)) {
  case KKT_Number:
    diag_printf("Kt_Number(0x%08x %08x %08x)", key.u.nk.value[2],
		 key.u.nk.value[1], key.u.nk.value[0]);
    break;
  case KKT_Page:
    diag_printf("KKT_Page(OID=");
    diag_printOid(key.u.unprep.oid);
    diag_printf(",blss=%x", keyBits_GetBlss(&key));
    if (keyBits_IsPrepared(&key))
      diag_printf(",P");
    if (keyBits_IsReadOnly(&key))
      diag_printf(",RO");
    diag_printf(")");
    break;
  case KKT_Node:
  case KKT_Segment:
    {
      char *typeName = 0;
      if (keyBits_GetType(&key) == KKT_Node)
	typeName = "KKT_Node";
      else
	typeName = "KKT_Segment";

      diag_printf("%s(OID=", typeName);
      diag_printOid(key.u.unprep.oid);
      if (keyBits_GetBlss(&key))
	diag_printf(",blss=%d", keyBits_GetBlss(&key));
      if (keyBits_IsNoCall(&key))
	diag_printf(",NC");
      if (keyBits_IsReadOnly(&key))
	diag_printf(",RO");
      if (keyBits_IsWeak(&key))
	diag_printf(",WK");
      diag_printf(")");
      break;
    }
  case KKT_Process:
    diag_printf("KKT_Process(OID=");
    diag_printOid(key.u.unprep.oid);
    diag_printf(")");
    break;
  case KKT_Sched:
    diag_printf("KKT_Sched(prio=%d)", key.keyData);
    break;
  case KKT_PrimeRange:
    {
      OID start = 0llu;
      OID top = UINT64_MAX;

      diag_printf("KKT_PrimeRange(OID=");
      diag_printOid(start);
      diag_printf(":");
      diag_printOid(top);
      diag_printf(")");
      break;
    }
  case KKT_PhysRange:
    {
      OID start = OID_RESERVED_PHYSRANGE;
      OID top = (UINT64_MAX * EROS_OBJECTS_PER_FRAME);

      diag_printf("KKT_PhysRange(OID=");
      diag_printOid(start);
      diag_printf(":");
      diag_printOid(top);
      diag_printf(")");
      break;
    }
  case KKT_Range:
    {
      OID start = key.u.rk.oid;
      OID top = key.u.rk.oid + key.u.rk.count;
      
      diag_printf("KKT_Range(OID=");
      diag_printOid(start);
      diag_printf(":");
      diag_printOid(top);
      diag_printf(")");
      break;
    }
  case KKT_Device:
    diag_printf("KKT_Device(ty=%d)", key.keyData);
    break;
  case KKT_Start:
    diag_printf("KKT_Start(OID=");
    diag_printOid(key.u.unprep.oid);
    diag_printf(",data=%d)", key.keyData);
    break;
  case KKT_Resume:
    diag_printf("KKT_Resume(OID=");
    diag_printOid(key.u.unprep.oid);
    diag_printf(")");
    break;
  default:
    if (keyBits_IsMiscKey(&key))
      diag_printf("misc(KKT_%s)", KeyNames[keyBits_GetType(&key)]);
    else
      diag_printf("KKT_Unknown");
  }
}
