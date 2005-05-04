
#include <stddef.h>
#include <eros/target.h>
#include <domain/Runtime.h>
#include <domain/domdbg.h>
#include "keyset.h"


void sortTable(struct table_entry a[], uint32_t N )  /* N = size of table */
{
  /* uses shellsort */
  uint32_t h; /* holds current step */
  uint32_t i;
  /* generate h */
  for (h = 1; h <= N/9; h = ( 3 * h + 1 ) )
    ;

  for ( ; h > 0; h /= 3 ) {   /* h = ..., 40, 13, 4, 1 */
    for (i = h; i < N; i++) {
      uint32_t j = i;    /* temp copy of i for changes in loop */
      struct table_entry v = a[j];  /* tmp copy of entry */

      while ( ( j >= h ) && ( compare(a[j - h].w, v.w) > 0 ) ) {
	a[j] = a[j-h];
	j -= h;
      }
      a[j] = v;
    }
  }
}

struct table_entry *
findEntry(struct table_entry *table,
	  uint32_t toFind[4],
	  uint32_t sorted,
	  uint32_t unsorted)
{
  /* try a binary search of the lengthSorted */

  /* hack -- makes the "non-zeroed" addressing work.  This gets around
     the problem that we are using unsigned numbers, and (0u - 1u) > 0u */
  struct table_entry *offsetTable = table - 1;
  uint32_t l = 1;
  uint32_t u = sorted;

  DEBUG(find) kprintf(KR_OSTREAM, "Find:");
  while (l <= u) {
    uint32_t m; /* midpoint */
    int cmpres;
    
    DEBUG(find) kprintf(KR_OSTREAM, " [%d,%d]",l,u);
    
    m = (l + u) / 2;
    cmpres = compare(toFind,(offsetTable+m)->w);
    if (cmpres > 0) {
      DEBUG(find) kprintf(KR_OSTREAM, " GT");
      l = m + 1;
    } else if (cmpres == 0) {
      DEBUG(find) kprintf(KR_OSTREAM, " EQ->FOUND IT\n");
      return offsetTable + m; /* got it ! */
    } else {
      DEBUG(find) kprintf(KR_OSTREAM, " LT");
      u = m - 1;
    }
  }    
  /* didn't find it */
  DEBUG(find) kprintf(KR_OSTREAM," EMPTY\n");

  DEBUG(find) kprintf(KR_OSTREAM, "Searching %d unsorted:",unsorted);
  
  /* not in sorted part -- try the unsorted part */
  offsetTable = table + sorted; /* points to the first unsorted item */

  while (unsorted-- != 0) {
    DEBUG(find) kprintf(KR_OSTREAM, " %d",unsorted);
    if (compare(offsetTable->w,toFind) == 0) {
      DEBUG(find) kprintf(KR_OSTREAM, " GOT IT!\n");
      return offsetTable; /* RETURN "got it!" */
    } else {
      offsetTable++; /* try the next one */
    }
  }
  DEBUG(find) kprintf(KR_OSTREAM, " NOT THERE\n");
  return NULL; /* return "not there!" */
}
