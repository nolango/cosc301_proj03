#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

// function declarations
void *malloc(size_t);
void mfree(void*);
void dump_memory_map(void);
int upSize(size_t);
void get_header(void*,int*,int*);
void modify_header(void*,int,int);
void *find_free_space(int);
int* split(int, int*);
void mupdate(void*, int);
void combine(void*, void*);
uint64_t get_offset(void *);
int change_free_list(void*,int*);
void get_diff(void*,void*,int*);	

const int HEAPSIZE = (1*1024*1024); // 1 MB
const int MINIMUM_ALLOC = sizeof(int) * 2;

// global file-scope variables for keeping track
// of the beginning of the heap.
void *heap_begin = NULL;
void *free_list = NULL;


void *mmalloc(size_t request_size) {

    // if heap_begin is NULL, then this must be the first
    // time that malloc has been called. ask for a new
    // heap segment from the OS using mmap and initialize
    // the heap begin pointer.
    if (!heap_begin) {
        heap_begin = mmap(NULL, HEAPSIZE, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
		// initial declerations if heap has not been made yet //
		free_list = heap_begin;
		*((int*)free_list + 0) = HEAPSIZE;
		*((int*)free_list + 1) = 0;
        atexit(dump_memory_map);
    }
	// if user requests an un allocatable size ... //
	if (request_size + 8 > HEAPSIZE || request_size < MINIMUM_ALLOC){
		printf("SIZEFAULT, requested size: %d , heap size: %d, min allocate: %d \n",request_size, HEAPSIZE, MINIMUM_ALLOC);
		return NULL;
	}
	int upped_size = upSize(request_size);
	void *free_space = find_free_space(upped_size);
	if(free_space == NULL){
		printf("No more space available on heap, sorry\n");
		return NULL;
	}
	// free_space_size is to see if the size of the
	// free space is the same after split or not
	int free_space_size = *((int*)free_space + 0);
	split(upped_size, free_space); // free_space header still holds free space offset 
	mupdate(free_space, free_space_size);
	// assigning 1 to offset of new alloc space to differentiate
	modify_header(free_space, ((int*)free_space)[0], 1);
	return free_space;


	// notes:
	// after find_free_space -- check to see if free_space is null before continuing
	// also there's an error when doing two consecutive mallocs because we have yet
	// to make sure that the freespace after the newly allocated block has a header
}

void mfree(void *memory_block) {
	void *free_listp = free_list;
	int msize, moffset, lsize, loffset, diff;
	int choice;
	get_header(memory_block, &msize, &moffset);
	
	// if memory_block is free already
	if(moffset != 1){
		printf("This memory block is allready free\n");
		return;
	}
	
	// this if statement is for all situations in which the memory
	// block to be freed is before the beginning of free_list --
	// which means we would have to change the free_list pointer.
	if(change_free_list(memory_block, &diff) == TRUE){

		// if block to be freed is right before
		// free list pointer
		if(diff == msize){
			combine(memory_block,free_listp);
			free_list = memory_block;
			return;
		}
		modify_header(memory_block, msize, diff);
		// memory_block points to past free_list block
		// and free_list now points to memory_block
		free_list = memory_block;
		return;
	}
	// this is a little cryptic -- it's traversing the free list in
	// order to see if the current block pointed to by free_listp is
	// is pointing over the memory block to be freed (ie. if(loffset
	// > diff)). diff is the difference/offset between current
	// free_listp and the memory block to be freed. These tests are
	// to figure out which steps to take on free_lisetp and memory
	// block.
	while(1){
		get_header(free_listp, &lsize, &loffset);
		get_diff(memory_block,free_listp, &diff);
		// note: diff cannot be zero unless free_listp == memory_block
		if(loffset > diff){
			// this checks to see if free_listp is right before
			// memory_block(assuming memory_block is located after
			// free_listp) or not.
			choice = diff > lsize ? TRUE : FALSE;
			break;
		} else {
			// if free_listp is the last free block
			if(loffset == 0){
				// and if memory block is not right next
				// to last free block (ie. buddies)
				if(diff > lsize){
					choice = TRUE;
					break;
				} else if(diff == lsize) { // they are buddies
					choice = FALSE;
					break;
				} else {
					free_listp = free_listp + loffset;
					continue;
				}
			}// end loffset == 0
			free_listp = free_listp + loffset;
			continue;
		}// end loffset > diff
	}// end while

	int last_loffset;
	if(choice == TRUE){
		((int*)memory_block)[1] = loffset;
		((int*)free_listp)[1] = diff;

		// if memory_block pointing to a buddy
		if(((int*)memory_block)[1] == msize)
			combine(memory_block, memory_block + msize);
		get_header(memory_block, &msize, &moffset);
		// if newly combined block is next to a free block
		if( ((int*)(memory_block + msize))[1] != 1){
			combine(memory_block, memory_block + msize);
		}
		return;
	} else { // choice == FALSE
		// free_listp and memory block are buddies
		// and free_listp is last free block

		last_loffset = loffset;
		combine(free_listp, memory_block);
		// new combined free block offset = offset of free_listp
		((int*)free_listp)[1] = last_loffset;
		// if new combined free block's buddy is free
			
		get_header(free_listp, &lsize, &loffset);
		if( ((int*)(free_listp + lsize))[1] != 1){
			combine(free_listp, free_listp + ((int*)free_listp)[0]);
		}
		return;
	}		
	return;
}

void dump_memory_map(void) {
	if(heap_begin == NULL)
		return;
    void *traverse = heap_begin;
    int size;
	int offset;
 
    while (1){
		get_header(traverse, &size, &offset);

        if (offset == 1){
            printf("Block size: %d, offset: %d, offset to heap: %d, Allocated\n", size, offset, (int)get_offset(traverse));
        }else{
            // it is free
            printf("Block size: %d, offset: %d, offset to heap: %d,  Free\n", size, offset, (int)get_offset(traverse));
		}

		// traverse list until current block's offset == 0
		// ie. current block is last free block
   		if( ((int*)traverse)[1] == 0 )
			break;
    	traverse = traverse + size;
    }
	printf("\n");
	return;
}

//					//
// HELPER FUNCTIONS //
//					//

void get_header(void *block, int *size, int *offset) {
    int *iblock = (int*)block;
    *size = iblock[0];  // same as *size = *(block + 0)
    *offset = iblock[1];  // same as *offset = *(block + 1)
}

void modify_header(void *block, int size, int offset) {
        int* iblock = (int*)block;
        iblock[0] = size;
        iblock[1] = offset;
}

void get_diff(void *blockA, void *blockB, int *diff){
	uint64_t addrA = (uint64_t)blockA;
	uint64_t addrB = (uint64_t)blockB;
	// returns neg if blockA is before blockB
	*diff =  (int)(addrA - addrB);

	return;
}

uint64_t get_offset(void *block){
	uint64_t block_addr = (uint64_t)block;
	uint64_t heap_begin_addr = (uint64_t)heap_begin;
	uint64_t offset = block_addr - heap_begin_addr;
	return offset == 0? 0 : offset;
}

int* split(int upped_size, int* block){
	int size = block[0];
	// if odd
	if(size % 2){
		size = size + 2;
		block[0] = size;
	}
	if(size/2 < upped_size)	// base case
		return block;
	block[0] = size/2;
	return split(upped_size,block); // recursion
}

int upSize(size_t request_size){
	int upped_size = request_size + 8;
	int i = 0;
	int base = 2;
	for(;i < 20;i++){
		if(base >= upped_size){
			upped_size = base;
			break;
		}
		base *= 2;
		i++;
	}
	return upped_size;
}

int change_free_list(void *memory_block, int *diff){
	// checking to see what situation we're dealing with
	// we handle the different situations of free with 2
	// catagories: if memory_block to be freed is before
	// free list pointer or not.
	int test_diff;
	get_diff(free_list, memory_block, &test_diff);
	if(test_diff > 0){
		*diff = test_diff;
		return TRUE;
	}
	return FALSE;
}

void combine(void *block,void *buddy){
	int *iblock = (int*)block;
	int *ibuddy = (int*)buddy;

	// if buddy is last free block
	if(ibuddy[1] == 0){
		modify_header((void*)iblock,iblock[0],0);
	} else {
		// new combined memory block gets offset of buddy
		// added with size of block
		modify_header((void*)iblock, iblock[0], iblock[0] + ibuddy[1]);
	}
	// fix size
	iblock[0] = iblock[0] + ibuddy[0];
	return;
	
}

void* find_free_space(int upped_size){
	// creating temporary pointer to free_list
	// in order to traverse through free list
	int* free_list_temp = free_list;
	int size;
	int offset;
	
	while(1){
		get_header(free_list_temp, &size, &offset);
		if(size >= upped_size){
			return free_list_temp;
		} else {
			if(offset == 0){
				// if this is last block of free space... //
				return NULL;
			}
			free_list_temp = free_list_temp + offset;
		}
	}
}

void mupdate(void* free_space, int free_space_size){
	int size, offset;
	get_header(free_space, &size, &offset);				// size is reduced size from split and
														// offset is still offset from free_space
	// if free list pointer will change
	// from the result of the malloc
	if(free_list == free_space){
		// if the space about to be allocated
		// fills up the space completely
		if(free_space_size == size){
			// if free_space is last avalaible block to allocate
			if(offset == 0){
				free_list = NULL;
				return;
			} else {
				// free_list points to what the
				// free space was pointing to
				free_list = free_list + offset;
				return;
			}
		} else { // free_space_size != size
			free_list = free_list + size;
			int fsize = free_space_size - size;
			// offset stays the same //
			modify_header(free_list, fsize, offset);
			return;
		}
	} else { // free_list != free_space								// have to be extra careful with this condition because
		void *free_listp = free_list;								// it will be difficult to check if it's ok -- STILL NOT TESTED
		int list_size, list_offset;
		// nextp points to next free block from curent list_p
		void *nextp;

		// traverse through free_list until free_listp points
		// to the free block before the free_space that's
		// being allocated 
		while(1){
			get_header(free_listp, &list_size, &list_offset);
			nextp = free_listp + list_offset;
			if(nextp == free_space)
				break;
			free_listp = free_listp + list_offset;
		}
	
		// if allocated block fills entire free space
		if(free_space_size == size){
			// if next free space is last free space
			if(((int*)nextp)[1] == 0){
				modify_header(free_listp, list_size, 0);
				return;
			} else {
				list_offset = list_offset + offset;
				modify_header(free_listp, list_size, list_offset);
				return;
			}
		} else { // free_space_size != size
			list_offset = list_offset + size;
			modify_header(free_listp, list_size, list_offset);	
			// nextp points to free block after new alloc block	
			nextp = nextp + size;
			int free_size = free_space_size - size;								
			int free_offset = offset - free_space_size;			
			modify_header(nextp, free_size, free_offset);
			return;	

/*										  new offset
									|---------------------\
		_____________________________________________________________
		||||||||||||| free	|||||||||  new	| free	||||||| free	 |
		|||||||||||||		||||||||| alloc	|		|||||||			 |
		-------------------------------------------------------------
									    	\-------------|
											  free offset
*/
		}
			
	} // end else
	
	
	
	

}


// note: what happens if you call mfree(1) twice?
