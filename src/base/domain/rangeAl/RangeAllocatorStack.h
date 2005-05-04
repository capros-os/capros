#ifndef RALLOCSTACK
#define RALLOCSTACK
// define how many ranges that should be tracked.  
// in this implementation it should be small, since 
// we try to minimize memory overhead by making funtion calls slow

#define STACKSIZE 128

// define range types for free variable
//  undefined means range is not claimed
//  free means it is allowed to be allocated, but is not in use
// allocated means it cannot be assigned during any other funciton call
#define UNDEFINED 0
#define FREE 1
#define ALLOC 2

//Define lastErrors
#define NO_ERROR 0
#define RELEASE_FAIL 1
#define DEFINE_FAIL 2
#define REQUEST_FAIL 3
#define REQUEST_TAKEN 4

// Basic range of integers.  Starts from any number and has nonzero size
// Ends at (start+ size), exclusive.
// All ranges of the same type are condensed into one if they are adjacent
//  One undefined range exists at the creation of a RangeAllocator.
//  Free and allocated ranges can be obtained through the function calls 
// that follow.  

typedef struct Range{
 unsigned long start, size;
 char free;
}Range;

// Stack implementation of  functions defined in header realized through
// Range allocator struct
typedef struct RangeAllocator{
  Range stack[STACKSIZE];
  unsigned long size;
  char lastError;
}  RA;

// create RA
// Before using an allocator, it must be created.  It is best to 
// use CreateRangeAlloc instead of mallocing a RA
// 
// Likewise with DeleteRangeAlloc instead of free
RA* CreateRangeAlloc();
int DeleteRangeAlloc(RA* ra);


// makes an undefined region defined as free
int DefineRange(unsigned long start,unsigned long size, RA* ra);


// allocate first region fitting request
//
// above and below are inclusive max and mins for the start and end of
// the allocated range
//
//  size is the requested range's size
//
// alignment forces the start of a range to be a multiple of some number
// specified in alignment
//
//
unsigned long RequestRange(unsigned long above, unsigned long below,
 unsigned long size,  unsigned long alignment, RA* ra);


// free range previously assigned
int ReleaseRange(unsigned long start, RA* ra);

char GetLastError(RA* ra);
#endif
