/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, Strawberry Development Group.
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

#include <assert.h>
#include <string.h>

#include <eros/target.h>
#include <erosimg/ArchDescrip.h>
#include <erosimg/ErosImage.h>
#include <erosimg/Diag.h>
#include <erosimg/DiskKey.h>

/* NOTE: Having all of this information hard-coded into the program is
 * stupid.  It should be read out of a file!  It's easy enough to make
 * that change, but at the moment I don't have the time to do so.
 */

/* All initialization values are viewed as strings expressed in human
 * readable hexadecimal.  The string is padded on the right with 0's
 * to be the proper number of bytes for the register.  It is then
 * converted to binary form.
 * 
 * If the register is big endian, the value is simply copied in order.
 * 
 * The current register description is not sufficient to deal with
 * bit-reversed registers.
 */



/* BIG indicates that the register is big endian.  The binary
 *    formatted value must be reversed before copying into the number
 *    key.
 * 
 * LTL indicates that the register is little endian.  The value should
 *     be copied into the number key verbatim.
 * 
 */
#define REG(s) #s

#define BIG(s) REG(s), true
#define LTL(s) REG(s), false

#define ARCH(s) s

/* The 'RegLayout' structure describes where a register's value is
 * placed in the domain.  Since a register value may be split across
 * multiple keys, we need to list all of the keys, the offset into the
 * keys' number value, the offset into the register value, and the
 * number of bytes to store.
 * 
 * Most architectures have only one general registers annex.  SPARC
 * may prove an exception, so I'm including a whichAnnex field.  It's
 * easier to remove it (or ignore it) later than to backfill.
 * 
 */

typedef struct RegLayout RegLayout;
struct RegLayout {
  const char *arch;		/* architecture name */
  const char *name;		/* register name */

  uint32_t annex;		/* 0 == domain root, 14 == genKeys */
  uint32_t slot;		/* slot of key */

  uint32_t slotOffset;		/* offset in number key */
  uint32_t valueOffset;		/* offset in register value */
  uint32_t len;			/* bytes to copy */
};

#include "gen.RegisterDescriptions.c"

static uint32_t nRegDescrip = sizeof(Registers) / sizeof(RegDescrip);
static uint32_t nRegLayout = sizeof(Layout) / sizeof(RegLayout);


RegDescrip *
RD_Lookup(const char *arch, const char * regName)
{
  uint32_t i;

  for (i = 0; i < nRegDescrip; i++)
    if ( (strcmp(Registers[i].arch, arch) == 0) &&
	 (strcmp(Registers[i].name, regName) == 0) )
      return &Registers[i];

  return 0;
}

bool
RD_IsArchName(const char *arch)
{
  uint32_t i;

  for (i = 0; i < nRegDescrip; i++)
    if ( strcmp(Registers[i].arch, arch) == 0 ) 
      return true;

  return false;
}

static uint8_t
Makeuint8_tFromHex(char xhi, char xlo)
{
  uint8_t bhi = 0;
  uint8_t blo = 0;

  if (xhi >= 'A' && xhi <= 'F')
    bhi = (xhi - 'A' + 10);
  else if (xhi >= 'a' && xhi <= 'f')
    bhi = (xhi - 'a' + 10);
  else 
    bhi = (xhi - '0');

  bhi <<= 4;
  
  if (xlo >= 'A' && xlo <= 'F')
    blo = (xlo - 'A' + 10);
  else if (xlo >= 'a' && xlo <= 'f')
    blo = (xlo - 'a' + 10);
  else 
    blo = (xlo - '0');

  return bhi | blo;
}


static void
zero_extend_value(const char *value,
		       char padded_value[RD_MaxRegSize*2])
{
  int i;
  char *offset_pos;
  int valLen = strlen(value);
  
  for (i = 0; i < RD_MaxRegSize*2; i++)
    padded_value[i] = '0';

  offset_pos = &padded_value[RD_MaxRegSize * 2];
  offset_pos -= valLen;
  
  for (i = 0; i < valLen; i++)
    *offset_pos++ = value[i];
}

static void
convert_to_binary(char padded_value[RD_MaxRegSize*2],
		       uint8_t binary_value[RD_MaxRegSize],
		       bool big_endian)
{
  uint32_t i;

  if (big_endian) {
    for (i = 0; i < RD_MaxRegSize; i++)
      binary_value[i] =
	Makeuint8_tFromHex(padded_value[i*2],
			padded_value[i*2+1]);
  }
  else {
    uint32_t last_pos = RD_MaxRegSize - 1;

    for (i = 0; i < RD_MaxRegSize; i++)
      binary_value[last_pos - i] =
	Makeuint8_tFromHex(padded_value[i*2],
			padded_value[i*2+1]);
  }
}

/* This is the heart of the architected register support.  Given a key
 * to a domain root and a RegDescrip pointer, it updates the domain
 * according to the description specified by the relevant RegLayout
 * entries.
 * 
 * The passed value should be a string of hexadecimal characters
 * beginning with "0x" with the most significant digit first
 * (i.e. written according to C/C++ input conventions).  This routine
 * will craft the binary format according to the endian requirements
 * of the target architecture and write the value into the appropriate
 * slots.
 */
void
RD_WriteValue(const RegDescrip *rd, ErosImage* pImage, 
	      KeyBits rootNodeKey, const char *value)
{
  uint32_t i;
  const char *valStr = value;
  char padded_value[RD_MaxRegSize * 2];
  uint8_t binary_buffer[RD_MaxRegSize];
  uint8_t *binary_value;

  if (valStr[0] != '0' || (valStr[1] != 'x' && valStr[1] != 'X'))
    diag_fatal(5,
		"Invalid register value \"%s\" passed to WriteValue\n",
		valStr);

  valStr += 2;
  zero_extend_value(valStr, padded_value);
  convert_to_binary(padded_value, binary_buffer, rd->big_endian);
  
  binary_value = binary_buffer;

  /* If the register is big endian, it is now way over on the right of
   * the binary converted value.  We need to find the proper start of
   * the value if it is smaller than that.
   */
  if (rd->big_endian)
    binary_value = &binary_buffer[RD_MaxRegSize - rd->len];

  for (i = 0; i < nRegLayout; i++) {
    KeyBits nodeKey;
    KeyBits key;
    uint8_t *numString;
    const uint8_t *valString;
    uint32_t l;

    keyBits_InitToVoid(&nodeKey);
    keyBits_InitToVoid(&key);

    if (strcmp(Layout[i].arch, rd->arch) != 0)
      continue;
    if (strcmp(Layout[i].name, rd->name) != 0)
      continue;

#if 0
    diag_printf("Annex %d slot %d ofs %d len %d valofs %d val 0x%x\n", 
      Layout[i].annex, Layout[i].slot, Layout[i].slotOffset, Layout[i].len,
      Layout[i].valueOffset, *(uint32_t *)binary_value);
#endif
    memcpy(&nodeKey, &rootNodeKey, sizeof(KeyBits));

    if (Layout[i].annex)
      nodeKey = ei_GetNodeSlot(pImage, nodeKey, Layout[i].annex);

    key = ei_GetNodeSlot(pImage, nodeKey, Layout[i].slot);

    assert(keyBits_GetType(&key) == KKT_Number && keyBits_IsUnprepared(&key));
    
    numString = (uint8_t *) &key.u.nk.value[0];
    numString += Layout[i].slotOffset;

    valString = binary_value + Layout[i].valueOffset;

    for (l = 0; l < Layout[i].len; l++) {
      numString[l] = valString[l];
    }

    ei_SetNodeSlot(pImage, nodeKey, Layout[i].slot, key);
  }
}

void
RD_InitProcess(ErosImage* pImage, KeyBits rootNodeKey,
	       const char *arch)
{
  unsigned int i;

  /* Put number keys in all the slots that need them. */
  KeyBits zeroNumberKey;
  init_SmallNumberKey(&zeroNumberKey, 0);

#if EROS_NODE_SIZE == 32
  for (i = ProcFirstRootRegSlot; i <= ProcLastRootRegSlot; i++)
    ei_SetNodeSlot(pImage, rootNodeKey, i, zeroNumberKey);
#else
#error "Unsupported node size"
#endif

  /* Loose end: If there are annexes, need to zero them. */

  for (i = 0; i < nRegDescrip; i++)
    if ( strcmp(Registers[i].arch, arch) == 0 )
      RD_WriteValue(&Registers[i], pImage, rootNodeKey, Registers[i].dfltValue);
}
