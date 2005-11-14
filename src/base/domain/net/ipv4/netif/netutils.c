#include <stddef.h>

#include <eros/target.h>
#include <eros/NodeKey.h>
#include <eros/Invoke.h>
#include <eros/machine/io.h>
#include <eros/ProcessKey.h>
#include <eros/KeyConst.h>

#include <idl/eros/key.h>
#include <idl/eros/Sleep.h>
#include <idl/eros/Number.h>

#include <domain/ConstructorKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include <domain/SpaceBankKey.h>
#include <domain/MemmapKey.h>
#include <domain/drivers/NetKey.h>

#include <string.h>

#include "constituents.h"
#include "../keyring.h"

#include "netutils.h"

#define DEBUG_NETUTILS  if(0)


/* Following is used to compute 32 ^ lss for patching together 
 * address space */
#define LWK_FACTOR(lss) (mult(EROS_NODE_SIZE, lss) * EROS_PAGE_SIZE)
uint32_t 
mult(uint32_t base, uint32_t exponent)
{
  uint32_t u;
  int32_t result = 1u;

  if (exponent == 0)  return result;

  for (u = 0; u < exponent; u++)
    result = result * base;
  
  return result;
}

/* Convenience routine for buying a new node for use in expanding the
 * address space. */
uint32_t
make_new_addrspace(uint16_t lss, fixreg_t key)
{
  uint32_t result = spcbank_buy_nodes(KR_BANK, 1, key, KR_VOID, KR_VOID);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM,"Error: make_new_addrspace: buying node "
	    "returned error code: %u.\n", result);
    return result;
  }

  result = node_make_node_key(key, lss, 0, key);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "Error: make_new_addrspace: making node key "
	    "returned error code: %u.\n", result);
    return result;
  }
  return RC_OK;
}

/* Place the newly constructed "mapped memory" tree into the process's
 * address space. */
void
patch_addrspace(uint16_t dma_lss)
{
  eros_Number_value window_key;
  uint32_t next_slot = 0;
  
  /* Stash the current ProcAddrSpace capability */
  process_copy(KR_SELF, ProcAddrSpace, KR_SCRATCH);
  
  /* Make a node with max lss */
  make_new_addrspace(EROS_ADDRESS_LSS, KR_ADDRSPC);
  
  /* Patch up KR_ADDRSPC as follows:
     slot 0 = capability for original ProcAddrSpace
     slots 1-15 = local window keys for ProcAddrSpace
     slot 16 = capability for FIFO
     slot 16 - ?? = local window keys for FIFO, as needed
     remaining slot(s) = capability for FRAMEBUF and any needed window keys
  */
  node_swap(KR_ADDRSPC, 0, KR_SCRATCH, KR_VOID);

  for (next_slot = 1; next_slot < 16; next_slot++) {
    window_key.value[2] = 0;    /* slot 0 of local node */
    window_key.value[1] = 0;    /* high order 32 bits of address
                                   offset */

    /* low order 32 bits: multiple of EROS_NODE_SIZE ^ (LSS-1) pages */
    window_key.value[0] = next_slot * LWK_FACTOR(EROS_ADDRESS_LSS-1); 

    /* insert the window key at the appropriate slot */
    node_write_number(KR_ADDRSPC, next_slot, &window_key); 
  }

  next_slot = 16;
  
  node_swap(KR_ADDRSPC, next_slot, KR_MEMMAP_C, KR_VOID);
  if (dma_lss == EROS_ADDRESS_LSS)
    kdprintf(KR_OSTREAM, "** ERROR: lance(): no room for local window "
             "keys for DMA!");
  next_slot++;

  /* Finally, patch up the ProcAddrSpace register */
  process_swap(KR_SELF, ProcAddrSpace, KR_ADDRSPC, KR_VOID);
}

/* Generate address faults in the entire mapped region in order to
 * ensure entire address subspace is fabricated and populated with
 *  correct page keys. */
void
init_mapped_memory(uint32_t *base, uint32_t size)
{
  uint32_t u;

  kprintf(KR_OSTREAM,"lance: initing mapped memory at 0x%08x",(uint32_t)base);

  for (u=0; u < (size / (sizeof(uint32_t))); u=u+EROS_PAGE_SIZE)
    base[u] &= 0xffffffffu;

  kprintf(KR_OSTREAM, "lance: init mapped memory complete.");
}

/* Start the helper thread & then pass it our start key so that
 * the helper can notify us of interrupts */
uint32_t
StartHelper(uint32_t irq) 
{
  Message msg;
  
  memset(&msg,0,sizeof(Message));
  /* Pass KR_HELPER_TYPE start key to the helper process */
  msg.snd_invKey = KR_HELPER;
  msg.snd_code = OC_netdriver_key;
  msg.snd_w1 = irq;
  msg.snd_key0 = KR_HELPER_TYPE;
  CALL(&msg);

  DEBUG_NETUTILS kprintf(KR_OSTREAM, "enet:sendingkey+IRQ ... [SUCCESS]");
    
  return RC_OK;
 
}

