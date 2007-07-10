/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, Strawberry Development Group.
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

uint32_t
create_new_process(uint32_t krBank, uint32_t krDomKey)
{
  uint32_t isGood;
  int i;

  DEBUG kdprintf(KR_OSTREAM, "Check official bank...\n");

  if (spcbank_verify_bank(KR_BANK, krBank, &isGood) != RC_OK ||
      isGood == 0)
    return RC_ProcCre_BadBank;
  
  DEBUG kdprintf(KR_OSTREAM, "OK -- buy process nodes\n");

#if defined(EROS_TARGET_i486)

  /* Bank is okay, try to buy the space: */
  if (spcbank_buy_nodes(krBank, 2, krDomKey, KR_SCRATCH0,
			KR_VOID) != RC_OK)
    return RC_capros_key_NoMoreNodes;

  DEBUG kdprintf(KR_OSTREAM, "Assemble them\n");

  /* We have the nodes.  Make the second the key registers node of the
     first: */
  (void) node_swap(krDomKey, ProcGenKeys, KR_SCRATCH0, KR_VOID);

  /* Initialize the fixed registers to zero number keys. */
  const capros_Number_value zeroNumber = {{0, 0, 0}};
  for (i = ProcFirstRootRegSlot; i <= ProcLastRootRegSlot; i++)
    (void) node_write_number(krDomKey, i, &zeroNumber);

  /* Now install the brand: */
  (void) node_swap(krDomKey, ProcBrand, KR_OURBRAND, KR_VOID);

#elif defined(EROS_TARGET_arm)

  /* Bank is okay, try to buy the space: */
  if (spcbank_buy_nodes(krBank, 2, krDomKey, KR_SCRATCH0,
			KR_VOID) != RC_OK)
    return RC_capros_key_NoMoreNodes;

  DEBUG kdprintf(KR_OSTREAM, "Assemble them\n");

  /* We have the nodes.  Make the second the key registers node of the
     first: */
  (void) node_swap(krDomKey, ProcGenKeys, KR_SCRATCH0, KR_VOID);

  /* Initialize the fixed registers to zero number keys. */
  const capros_Number_value zeroNumber = {{0, 0, 0}};
  for (i = ProcFirstRootRegSlot; i <= ProcLastRootRegSlot; i++)
    (void) node_write_number(krDomKey, i, &zeroNumber);

  /* Now install the brand: */
  (void) node_swap(krDomKey, ProcBrand, KR_OURBRAND, KR_VOID);

#endif

  DEBUG kdprintf(KR_OSTREAM, "Build process key:\n");

  /* Now make a process key of this: */
  (void) capros_ProcTool_makeProcess(KR_PROCTOOL, krDomKey, krDomKey);

  DEBUG kdprintf(KR_OSTREAM, "Got new process key:\n");

  return RC_OK;
}
