#include <string.h>
#include "RangeAllocatorStack.h"
/* internally called once during create. initializes first element*/
int AddStack(unsigned long start, unsigned long size,
		 char free, RA* ra){
	if(ra->size>= STACKSIZE)
	  return 0;
	ra->stack[0].start= start;
	ra->stack[0].size= size;
	ra->stack[0].free= free;
	ra->size++;
return 1;
}

char GetLastError(RA* ra){
return ra->lastError;
}

RA*CreateRangeAlloc(){
  RA* result= (RA*)malloc(sizeof(RA));
  if(result==0)
	return 0;
  memset(result, 0, sizeof(RA));
  AddStack(0, -1, UNDEFINED, result);
  return result;
}

/* does nothing, as free not implemented in the malloc i grabbed.
	Change once someone implements free
*/
int DeleteRangeAlloc(RA* ra){
// free(ra);
return 1;
}
/* Bad name, used internally to take construct a new range from 
	one already there.  Then it places new ranges in stack
*/
int BreakAndFix( int index, Range* insert,RA* ra ){
	int numNodesToAdd= 0;
	int i = 0;
	int left= 0;
	int right = 0;
	Range low, mid, high;
	low.size=0;
	mid.start= insert->start;
	mid.size= insert->size;
	if(insert->start != ra->stack[index].start){
	  	numNodesToAdd++;
		left++;
		low.free= ra->stack[index].free;
		low.start= ra->stack[index].start;
		low.size= insert->start-ra->stack[index].start;
	  }
	if( (insert->start+insert->size)<
		(ra->stack[index].start+ra->stack[index].size)){
		numNodesToAdd++;
	   right++;
		high.free= ra->stack[index].free;
	  	high.start= insert->start+insert->size;
		high.size= ra->stack[index].size-low.size-mid.size;
	  }
        if((ra->size+numNodesToAdd) >= STACKSIZE)
		return -1;
	 i = ra->size-1;
	while(i!= index){
	  memcpy(&ra->stack[i+numNodesToAdd],&ra->stack[i], sizeof(Range));
	  i--; 
	}
ra->size+=numNodesToAdd;
// add node on most significant
	if(right){
	  ra->stack[i+numNodesToAdd].start= high.start;
	  ra->stack[i+numNodesToAdd].size= high.size;
	  ra->stack[i+numNodesToAdd].free= high.free;
	  numNodesToAdd--;  
	}
//add node on least significant
	if(left){
	  ra->stack[i].start= low.start;
	  ra->stack[i].size= low.size;
	  ra->stack[i].free= low.free;
	
	}
	ra->stack[i+numNodesToAdd].start= mid.start;
	ra->stack[i+numNodesToAdd].size= mid.size;
	ra->stack[i+numNodesToAdd].free= mid.free;
return (i+numNodesToAdd);
}

/* Takes the location of a free node on the stack, checks for free ranges
   and coalesces any into one. 
  It is assumed that there will be at most 1 free range on either
  side of the range at location i
*/
void combineFree(int i, RA* ra){
  int y= 0;
  int combined= 0;
  int sortStart= i;
  if(i!= 0&&
	(ra->stack[i-1].free==FREE)){//check and coalesce previous
    ra->stack[i-1].size+= ra->stack[i].size;
    combined++;
    sortStart= i-1;
  }
  if((i!= ra->size-1)&&
	(ra->stack[i+1].free==FREE))//check and coalesce next
  {
	ra->stack[sortStart].size+=ra->stack[i+1].size;
	combined++;
  }
  if(combined==0)return ;
  //move ranges to replace emptied spots in array
	sortStart++;
	 y= sortStart;
	while(y< ra->size-combined){
		memcpy(&ra->stack[y], &ra->stack[y+combined], sizeof(Range));
	y++;
	}//end while
	ra->size-=combined;
}//end combine free

/* API implementations */
int DefineRange(unsigned long start, unsigned long size, RA* ra){
	Range range;
//find suitable node
	int i = 0;
	while(i< ra->size){
	  if((ra->stack[i].start<= start)&&
		((ra->stack[i].start+ra->stack[i].size)
		>=(start+size)))
		break;
	i++;
	}
	  if(i==ra->size){
		ra->lastError= DEFINE_FAIL;
		return 0;
	  }
	range.start= start;
	range.size= size;
	i = BreakAndFix(i, &range, ra);
	if(i==-1){
		ra->lastError= DEFINE_FAIL;
		return 0;
	  }
	ra->stack[i].free= FREE;  
	combineFree(i, ra);
  	ra->lastError= NO_ERROR;
	return 1;
}

unsigned long RequestRange(unsigned long above, unsigned long below,
		  unsigned long size, unsigned long alignment,
		RA* ra){
  int i = 0;
  Range range;
  range.size= size;
  while(i < ra->size){
	Range* stack;
	stack= &ra->stack[i];
	if(  ((stack->start+stack->size-1>=above))
		&&(stack->free==FREE)&&(stack->start<= below)  ){
//test alignment
		unsigned long remain;	
		range.start= stack->start;
		if(above> stack->start){
			range.start= above;
		}
		remain= range.start%alignment;
		if((remain==0)&&(range.start+size-1<=below)){
			break;
		}
		else{
		  remain= range.start+(alignment-remain);
		  if(((remain+size)<= (stack->start+stack->size))
			&&((remain+size-1)<=below)){
			range.start= remain;
			break;
			}
		}
	  }//if 1	
	i++;
	}//end while
  if(i==ra->size){
	ra->lastError= REQUEST_TAKEN;
	return 0;
	}
  i = BreakAndFix(i, &range, ra);
  if(i==-1){
	ra->lastError= REQUEST_FAIL;
	return 0;
	}
  ra->stack[i].free= ALLOC;
  ra->lastError= NO_ERROR;
  return range.start;
}

int ReleaseRange(unsigned long start, RA* ra){
  int i = 0;
  while(i < ra->size){
	if(ra->stack[i].start== start)
		break;
	i++;
  }
  if(i==ra->size){
	ra->lastError= RELEASE_FAIL;
	return 0;
	}
  ra->lastError= NO_ERROR;
  if(ra->stack[i].free!= ALLOC)return 1;
  ra->stack[i].free=FREE;
  combineFree(i, ra);
return 1;
}
