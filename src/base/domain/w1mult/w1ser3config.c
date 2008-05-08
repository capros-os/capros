
#include "w1multConfig.h"

// Devices in the powerhouse:
struct W1DevConfig _start[] = {
  // A coupler must precede the devices branched off it.
  {0, -1, 0, 0x22000003bd4bc009ULL},	// DS2502 in DS9097U-009
  {1, -1, 0, 0xc30000004704ab28ULL},	// DS18B20 at air intake
  {2, -1, 0, 0x660000006db58c26ULL},	// DS2438 #1 (leftmost)
  {3, -1, 0, 0x9b0000006e275b26ULL},	// DS2438 #2
  {4, -1, 0, 0xfc0000006e9cdd26ULL},	// DS2438 #3
  {5, -1, 0, 0xc00000006dd26626ULL},	// DS2438 #4 (rightmost)

  {-1, 0, 0, 0}	// end of list
};
