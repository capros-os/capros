
#include "w1multConfig.h"

struct W1DevConfig _start[] = {
  // A coupler must precede the devices branched off it.
  {0, -1, 0, 0x1c00000001c27e1fULL},	// coupler #3 in data closet
  {1, -1, 0, 0x2b00000001c27f1fULL},	// coupler #2 in data closet
  {2, -1, 0, 0xe400000001c2751fULL},	// coupler #1 in data closet
  {3, 0, branch_aux, 0x7e0000004709e428ULL},	// DS18B20 in loose plug
  {4, 0, branch_aux, 0xef00000047100528ULL},	// DS18B20 at pool solar panels
  {5, 0, branch_aux, 0x4000000004844020ULL},	// DS2450 insolom. at pool solar
  {6, -1, 0, 0x730000002414d081ULL},	// DS2401 in DS9490
  // in basement:
  {7, 0, branch_main, 0x0900000047063028ULL},	// DS18B20 media rm floor
  {8, 0, branch_main, 0xb7000000470af028ULL},	// DS18B20 kitchen floor
  {9, 0, branch_main, 0x5500000047291c28ULL},	// DS18B20 DR floor
  {10, 0, branch_main, 0x29000000471c6a28ULL},	// DS18B20 foyer floor
  {11, 0, branch_main, 0xbe00000047136128ULL},	// DS18B20 LR floor
  {12, 0, branch_main, 0x9b00000046fc0528ULL},	// DS18B20 medit rm floor
  {13, 0, branch_main, 0x6a00000046ef6528ULL},	// DS18B20 hot water below kitch
  {14, 0, branch_main, 0xe800000046f90d28ULL},	// DS18B20 outside N cooler
  {15, 0, branch_main, 0xb8000000470d4328ULL},	// DS18B20 outside S cooler
  {16, 0, branch_main, 0x8700000046f05b28ULL},	// DS18B20 study floor
  {17, 0, branch_main, 0x9000000006160920ULL},	// DS2450 Hobbyboards, HVAC 1-4
  {18, 0, branch_main, 0x4d00000006190f20ULL},	// DS2450 Hobbyboards, HVAC 5
  // at pool pad:
  {19, 1, branch_main, 0xee00000046f59428ULL},	// DS18B20 from pool/spa
  {20, 1, branch_main, 0xa1000000471c8228ULL},	// DS18B20 from solar

  {-1, 0, 0, 0}	// end of list
};
