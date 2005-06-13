/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, Strawberry Development Group
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


#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/eros/Sleep.h>
#include <eros/NodeKey.h>
#include <domain/SpaceBankKey.h>
#include <domain/ConstructorKey.h>
#include <domain/domdbg.h>

/**Handle the stack stuff**/
const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_stack_pointer = 0x21000;

/* The purpose of this domain is to issue escape sequences to the
 * console to test ANSI terminal emulation.  It is meant to be watched
 * interactively.
 */
#define KR_VOID 0
#define KR_TMP  1
#define KR_TMP2 2
#define KR_TMP3 3
#define KR_SUBBANK   4
#define KR_CON       5
#define KR_SPACEBANK 7
#define KR_OSTREAM   8
#define KR_SLEEP     9
#define KR_SCHED     10
#define KR_METACON   11
#define KR_CHILD_SEG 12
#define KR_CHILD_PC  13


void spaceBankStats()
{
  struct bank_limits bl;

  if ( spcbank_get_limits( KR_SPACEBANK, &bl ) != RC_OK ) {
    kprintf( KR_OSTREAM, "!! Could not get bank limits!\n" );
    return;
  };
  
  kprintf( KR_OSTREAM, "  SpaceBank statistics\n" );
  kprintf( KR_OSTREAM, "    fl=%x \t ac=%x\n    efl=%x \t eac=%x\n",
	   bl.frameLimit % 0x100000000, 
	   bl.allocCount % 0x100000000,
	   bl.effFrameLimit % 0x100000000,
	   bl.effAllocLimit % 0x100000000 );

  return;
};

void testSpaceBank( int BANK )
{
  /* spaceBankStats(); */

  kprintf( KR_OSTREAM, "Testing SpaceBank: ");

  if ( spcbank_buy_nodes( BANK, 1, KR_TMP, KR_VOID, KR_VOID ) 
       != RC_OK ) {
    kprintf( KR_OSTREAM, "could not alloc a node; " ); 
  } else {
    if ( node_swap( KR_TMP, 0,KR_OSTREAM, KR_VOID ) != RC_OK ) {
      kprintf( KR_OSTREAM, "could not node_swap; " );
    } else {
      node_copy( KR_TMP, 0, KR_TMP2 );
      kprintf( KR_TMP2, " OK; " );
    }
  }

  if ( spcbank_buy_data_pages( BANK, 1, KR_TMP, KR_VOID, KR_VOID ) 
       != RC_OK ) {
    kprintf( KR_OSTREAM, "could not alloc data page!\n" ); 
  } else {
    kprintf( KR_OSTREAM, "OK\n" );
  }

  /* spaceBankStats(); */
}


void testSubbank()
{
}


int
main()
{
  /* kprintf(KR_OSTREAM, "\033[H\033[J\n\n");		clear screen */
  /* testSpaceBank( KR_SPACEBANK); */

  eros_Sleep_sleep( KR_SLEEP, 1000 );
  
  /* create constructor */
  kprintf( KR_OSTREAM, "Requesting constructor: " );
  if (constructor_request(KR_METACON,KR_SPACEBANK,KR_SCHED,KR_VOID,KR_TMP)
      != RC_OK ) {
    kprintf( KR_OSTREAM, "constructor_request( MR_METACON, .. ) failed!\n" );
  } else {
    kprintf( KR_OSTREAM, "OK\nPopulating new constructor: " );
    
    constructor_insert( KR_TMP, 0, KR_OSTREAM );
    constructor_insert( KR_TMP, 1, KR_SLEEP );
    constructor_insert( KR_TMP, Constructor_Product_Spc, KR_CHILD_SEG );
    constructor_insert( KR_TMP, Constructor_Product_PC,  KR_CHILD_PC );
  
    kprintf( KR_OSTREAM, "OK\nSealing constructor: " );
    if (constructor_seal( KR_TMP, KR_CON ) != RC_OK ) {
      kprintf( KR_OSTREAM, "Failed!\n" );
    } else {
      kprintf( KR_OSTREAM, "OK\n" );
    };
  };
  
  for(;;) {
    eros_Sleep_sleep( KR_SLEEP, 1000 );
    
    kprintf( KR_OSTREAM, "Creating SubBank: " );
    if ( spcbank_create_subbank( KR_SPACEBANK, KR_SUBBANK ) != RC_OK ) {
      kprintf( KR_OSTREAM, "Error create_subbank\n" );
      continue;
    };
    kprintf( KR_OSTREAM, "OK\n" );
      
    
    testSpaceBank( KR_SUBBANK );
    
    kprintf( KR_OSTREAM, "Requesting product from constructor...\n" );
    if ( constructor_request( KR_CON, KR_SUBBANK, KR_SCHED, KR_VOID, KR_TMP )
	 != RC_OK ) {
      kprintf( KR_OSTREAM, "Constructor_request failed!\n" );
    };
    
    kprintf( KR_OSTREAM, "Constructor_request: OK\n" );
    
    {
      Message msg;
      msg.snd_key0 = KR_VOID;
      msg.snd_key1 = KR_VOID;
      msg.snd_key2 = KR_VOID;
      msg.snd_rsmkey = KR_VOID;
      msg.snd_code = 2;
      msg.snd_w1   = 1;
      msg.snd_w2   = 100;
      msg.snd_w3   = 500;
      msg.snd_len  = 0;
      msg.snd_invKey = KR_TMP;
      
      msg.rcv_key0 = KR_VOID;
      msg.rcv_key1 = KR_VOID;
      msg.rcv_key2 = KR_VOID;
      msg.rcv_rsmkey = KR_VOID;
      msg.snd_len = 0;
      
      SEND(&msg);
    }

    kprintf( KR_OSTREAM, "Product should be counting....\n" );
    eros_Sleep_sleep( KR_SLEEP, 2000 );

    kprintf( KR_OSTREAM, "Going to destroy SubBank...\n" );
    if ( spcbank_destroy_bank( KR_SUBBANK, 1 ) == RC_OK ) {
      kprintf( KR_OSTREAM, "Destroyed SubBank: OK\n" );
    } else {
      kprintf( KR_OSTREAM, "Destroy SubBank: Failed!\n" );
      continue;
    };

    kprintf( KR_OSTREAM, "Next test must fail: " );
    testSpaceBank( KR_SUBBANK );

    kprintf( KR_OSTREAM, "\n...let's do it again...\n" );
  };
		  
  return 0;
}
