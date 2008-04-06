
#include "w1mult.h"

struct W1DevConfig _start[] = {
  // A coupler must precede the devices branched off it.
  {0, -1, 0, 0x1c00000001c27e1fULL},	// coupler in data closet
  {1, -1, 0, 0xe400000001c2751fULL},	// coupler in data closet
  {2, -1, 0, 0x2b00000001c27f1fULL},	// coupler in data closet
  {3, -1, 0, 0x7e0000004709e428ULL},	// DS18B20 in loose plug
  {4, -1, 0, 0xef00000047100528ULL},	// DS18B20 at pool solar panels
  {5, -1, 0, 0x4000000004844020ULL},	// DS2450 insolometer at pool panels
  {6, -1, 0, 0x730000002414d081ULL},	// DS2401 in DS9490
  {-1, 0, 0, 0}	// end of list
};
