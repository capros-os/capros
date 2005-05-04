/* Sample small program: the obligatory ``hello world'' demo. */

#include <eros/target.h>
#include <eros/NodeKey.h>
#include <eros/KeyConst.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include "constituents.h"

#define KR_OSTREAM KR_APP(0)
#define KR_KEYBITS KR_APP(1)
#define KR_TREE    KR_APP(2)
#define KR_BIGTREE KR_APP(3)
#define KR_SCRATCH KR_APP(4)
#define KR_ROTREE  KR_APP(5)

int
main(void)
{
  node_copy(KR_CONSTIT, KC_KEYBITS, KR_KEYBITS);
  node_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  node_copy(KR_CONSTIT, KC_TREE, KR_TREE);
  node_copy(KR_CONSTIT, KC_BIGTREE, KR_BIGTREE);

  kdprintf(KR_OSTREAM, "About to call node_extended_copy()\n");

  node_extended_copy(KR_BIGTREE, 0, KR_SCRATCH);
  ShowKey(KR_OSTREAM, KR_KEYBITS, KR_SCRATCH);

  node_extended_copy(KR_BIGTREE, 1, KR_SCRATCH);
  ShowKey(KR_OSTREAM, KR_KEYBITS, KR_SCRATCH);

  node_extended_copy(KR_BIGTREE, 0x20, KR_SCRATCH);
  ShowKey(KR_OSTREAM, KR_KEYBITS, KR_SCRATCH);

  node_extended_copy(KR_BIGTREE, 0x21, KR_SCRATCH);
  ShowKey(KR_OSTREAM, KR_KEYBITS, KR_SCRATCH);

  kdprintf(KR_OSTREAM, "About to call node_extended_swap()\n");

  node_extended_swap(KR_BIGTREE, 0, KR_SCRATCH, KR_SCRATCH);
  ShowKey(KR_OSTREAM, KR_KEYBITS, KR_SCRATCH);

  node_extended_swap(KR_BIGTREE, 1, KR_SCRATCH, KR_SCRATCH);
  ShowKey(KR_OSTREAM, KR_KEYBITS, KR_SCRATCH);

  node_extended_swap(KR_BIGTREE, 0x20, KR_SCRATCH, KR_SCRATCH);
  ShowKey(KR_OSTREAM, KR_KEYBITS, KR_SCRATCH);

  node_extended_swap(KR_BIGTREE, 0x21, KR_SCRATCH, KR_SCRATCH);
  ShowKey(KR_OSTREAM, KR_KEYBITS, KR_SCRATCH);

  kdprintf(KR_OSTREAM, "Values should now be rotated...\n");

  node_extended_copy(KR_BIGTREE, 0, KR_SCRATCH);
  ShowKey(KR_OSTREAM, KR_KEYBITS, KR_SCRATCH);

  node_extended_copy(KR_BIGTREE, 1, KR_SCRATCH);
  ShowKey(KR_OSTREAM, KR_KEYBITS, KR_SCRATCH);

  node_extended_copy(KR_BIGTREE, 0x20, KR_SCRATCH);
  ShowKey(KR_OSTREAM, KR_KEYBITS, KR_SCRATCH);

  node_extended_copy(KR_BIGTREE, 0x21, KR_SCRATCH);
  ShowKey(KR_OSTREAM, KR_KEYBITS, KR_SCRATCH);

  kdprintf(KR_OSTREAM, "Making RO variant in key %d...\n", KR_ROTREE);

  {
    uint16_t keyData;
    node_get_key_data(KR_BIGTREE, &keyData);
    node_make_node_key(KR_BIGTREE, keyData | SEGMODE_RO, KR_ROTREE);
  }

  kdprintf(KR_OSTREAM, "Following swap operation should trigger process keeper...\n");

  node_extended_swap(KR_ROTREE, 0x0, KR_SCRATCH, KR_SCRATCH);
  ShowKey(KR_OSTREAM, KR_KEYBITS, KR_SCRATCH);


  kdprintf(KR_OSTREAM, "Test exits...\n");

  return 0;
}
