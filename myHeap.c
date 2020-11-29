///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019-2020 Jim Skrentny
// Posting or sharing this file is prohibited, including any changes/additions.
// Used by permission Fall 2020, CS354-deppeler
//
///////////////////////////////////////////////////////////////////////////////
 
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include "myHeap.h"
#include <stdbool.h>
 
/*
 * This structure serves as the header for each allocated and free block.
 * It also serves as the footer for each free block but only containing size.
 */
typedef struct blockHeader {           
    int size_status;
    /*
    * Size of the block is always a multiple of 8.
    * Size is stored in all block headers and free block footers.
    *
    * Status is stored only in headers using the two least significant bits.
    *   Bit0 => least significant bit, last bit
    *   Bit0 == 0 => free block
    *   Bit0 == 1 => allocated block
    *
    *   Bit1 => second last bit 
    *   Bit1 == 0 => previous block is free
    *   Bit1 == 1 => previous block is allocated
    * 
    * End Mark: 
    *  The end of the available memory is indicated using a size_status of 1.
    * 
    * Examples:
    * 
    * 1. Allocated block of size 24 bytes:
    *    Header:
    *      If the previous block is allocated, size_status should be 27
    *      If the previous block is free, size_status should be 25
    * 
    * 2. Free block of size 24 bytes:
    *    Header:
    *      If the previous block is allocated, size_status should be 26
    *      If the previous block is free, size_status should be 24
    *    Footer:
    *      size_status should be 24
    */
} blockHeader;         

/* Global variable - DO NOT CHANGE. It should always point to the first block,
 * i.e., the block at the lowest address.
 */
blockHeader *heapStart = NULL;     

/* Size of heap allocation padded to round to nearest page size.
 */
int allocsize;

/*
 * Additional global variables may be added as needed below
 */

/*
 * keeps track of most recently allocated block header
 */
blockHeader *recentAlloc = NULL;
 
/* 
 * Function for allocating 'size' bytes of heap memory.
 * Argument size: requested size for the payload
 * Returns address of allocated block on success.
 * Returns NULL on failure.
 * This function should:
 * - Check size - Return NULL if not positive or if larger than heap space.
 * - Determine block size rounding up to a multiple of 8 and possibly adding padding as a result.
 * - Use NEXT-FIT PLACEMENT POLICY to chose a free block
 * - Use SPLITTING to divide the chosen free block into two if it is too large.
 * - Update header(s) and footer as needed.
 * Tips: Be careful with pointer arithmetic and scale factors.
 */
void* myAlloc(int size) {     
    //TODO: Your code goes in here.
    if (recentAlloc == NULL){//start at beginning if no block been allocated
        recentAlloc = heapStart;
    }

    blockHeader *current;//traversal block
    blockHeader *next;//split block
    blockHeader *footer;//footer of split block
    int tSize;//traversal size
    int pad;//padding needed
    bool found = false;//determines if block found to allocate
    int prevAlloc;//keeps track of p-bit

    //determine proper size of block and if padding needed
    size+=4;
    pad = size % 8;
    pad = (8 - pad) % 8;
    size += pad;
    //printf("%i",size);

    current = recentAlloc;//start traversal by next-fit PP
    prevAlloc = 2;
    do {


        tSize = current->size_status;

        //check to see if this is allocated
        if(tSize & 1) {//if allocated
            tSize -= 1;
            if(tSize & 2) {//compute size without p-bit
                tSize -= 2;
            }
            //traverse to next block by size of current block
            current = (blockHeader*)((char*)current + tSize);

            prevAlloc = 2; //this will set p-bit to 1 if next block is hit

        }
        else {//if unallocated
            if(tSize & 2) { //compute size without p-bit
                tSize -= 2;
                //printf("%i",tSize);
            }

            if(tSize >= size) {//found a free block of proper size
                found = true;

            }
            else {
                prevAlloc = 0;//this will set p-bit to 0 if next block is hit
                //traverse to next block by size of current block
                current = (blockHeader*)((char*)current + tSize);

            }
        }

        if(current->size_status == 1) {//if found endmark, go to beginning
                    current = heapStart;
                }
    } while(!found && (current != recentAlloc));

    if(found) {
        //set size_status to size of allocated block
        //set a-bit to 1 and p-bit to whatever was status of previous
        current->size_status = size + 1 + prevAlloc;

        //if splitting is necessary
        if (tSize > size) {
            next = (blockHeader*)((char*)current + size);//split block
            //set free block to size difference and set p-bit
            next->size_status = (tSize - size) + 2;
            //set footer of next block
            footer = (blockHeader*)((char*)next + (tSize - size) - sizeof(blockHeader));
           footer->size_status = (tSize - size);
        }

        recentAlloc = current;//set this block to recently allocated GB

        return (blockHeader*)((char*)current + sizeof(blockHeader));
    }
    else {
        return NULL;
    }
} 
 
/* 
 * Function for freeing up a previously allocated block.
 * Argument ptr: address of the block to be freed up.
 * Returns 0 on success.
 * Returns -1 on failure.
 * This function should:
 * - Return -1 if ptr is NULL.
 * - Return -1 if ptr is not a multiple of 8.
 * - Return -1 if ptr is outside of the heap space.
 * - Return -1 if ptr block is already freed.
 * - USE IMMEDIATE COALESCING if one or both of the adjacent neighbors are free.
 * - Update header(s) and footer as needed.
 */                    
int myFree(void *ptr) {    
    //TODO: Your code goes in here.
    //error if ptr null
    if(ptr == NULL) {
        return -1;
    }
    //check to ptr multiple of 8
    if((int)ptr % 8 != 0) {
        return -1;
    }
    //check ptr within address bounds
    if((unsigned int)ptr<(unsigned int)heapStart || (unsigned int)ptr>(unsigned int)((blockHeader*)((char*)heapStart + allocsize))){
        return -1;
    }


    blockHeader *current;//block to free
    blockHeader *prev;//block before block to free
    blockHeader *footer;//footer used for various footers
    blockHeader *next;//next block
    int tSize;//keeps track of block size, before and during coalescing
    int prevAlloc = 0;//keeps track of p-bit


    current = (blockHeader*)(ptr - sizeof(blockHeader));//find header of block to be freed

    //check ptr not already freed
    if(!(current->size_status & 1)) {
        return -1;
    }
    tSize = current->size_status;

    //decrement a-bit and p-bit(if necessary) to find size of block
    tSize -= 1;
    if(tSize & 2) {
        tSize -= 2;
        prevAlloc = 2; //keep track of previous allocation
    }

    next = (blockHeader*)((char*)current + tSize); //find next block

    //if next block is free then add size of blocks together
    if(!(next->size_status & 1) && next->size_status != 1) {
        next->size_status -= 2;
        tSize += next->size_status;

    } else if(next->size_status != 1) {
        next->size_status -= 2;
    }

    //if the previous block is free, go to the footer
    //then use size found in footer to go to header of previous
    //coalesce current block and previous then set footer appropriately
    if (!prevAlloc) {
        footer = (blockHeader*)((char*)current - sizeof(blockHeader));
        prev = (blockHeader*)((char*)current - footer->size_status);
        prev->size_status += tSize;
        tSize = prev->size_status;

        if(tSize & 2) {
            tSize -= 2;
        }

        footer = (blockHeader*)((char*)prev + tSize - sizeof(blockHeader));
        footer->size_status = tSize;
    } else { //else set size to tSize found and add p-bit, then set footer
        current->size_status = tSize + 2;
        footer = (blockHeader*)((char*)current + tSize - sizeof(blockHeader));
        footer->size_status = tSize;
    }

    return 0;
} 
 
/*
 * Function used to initialize the memory allocator.
 * Intended to be called ONLY once by a program.
 * Argument sizeOfRegion: the size of the heap space to be allocated.
 * Returns 0 on success.
 * Returns -1 on failure.
 */                    
int myInit(int sizeOfRegion) {    
 
    static int allocated_once = 0; //prevent multiple myInit calls
 
    int pagesize;  // page size
    int padsize;   // size of padding when heap size not a multiple of page size
    void* mmap_ptr; // pointer to memory mapped area
    int fd;

    blockHeader* endMark;
  
    if (0 != allocated_once) {
        fprintf(stderr, 
        "Error:mem.c: InitHeap has allocated space during a previous call\n");
        return -1;
    }
    if (sizeOfRegion <= 0) {
        fprintf(stderr, "Error:mem.c: Requested block size is not positive\n");
        return -1;
    }

    // Get the pagesize
    pagesize = getpagesize();

    // Calculate padsize as the padding required to round up sizeOfRegion 
    // to a multiple of pagesize
    padsize = sizeOfRegion % pagesize;
    padsize = (pagesize - padsize) % pagesize;

    allocsize = sizeOfRegion + padsize;

    // Using mmap to allocate memory
    fd = open("/dev/zero", O_RDWR);
    if (-1 == fd) {
        fprintf(stderr, "Error:mem.c: Cannot open /dev/zero\n");
        return -1;
    }
    mmap_ptr = mmap(NULL, allocsize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (MAP_FAILED == mmap_ptr) {
        fprintf(stderr, "Error:mem.c: mmap cannot allocate space\n");
        allocated_once = 0;
        return -1;
    }
  
    allocated_once = 1;

    // for double word alignment and end mark
    allocsize -= 8;

    // Initially there is only one big free block in the heap.
    // Skip first 4 bytes for double word alignment requirement.
    heapStart = (blockHeader*) mmap_ptr + 1;

    // Set the end mark
    endMark = (blockHeader*)((void*)heapStart + allocsize);
    endMark->size_status = 1;

    // Set size in header
    heapStart->size_status = allocsize;

    // Set p-bit as allocated in header
    // note a-bit left at 0 for free
    heapStart->size_status += 2;

    // Set the footer
    blockHeader *footer = (blockHeader*) ((void*)heapStart + allocsize - 4);

    footer->size_status = allocsize;

    return 0;
} 
                  
/* 
 * Function to be used for DEBUGGING to help you visualize your heap structure.
 * Prints out a list of all the blocks including this information:
 * No.      : serial number of the block 
 * Status   : free/used (allocated)
 * Prev     : status of previous block free/used (allocated)
 * t_Begin  : address of the first byte in the block (where the header starts) 
 * t_End    : address of the last byte in the block 
 * t_Size   : size of the block as stored in the block header
 */                     
void dispMem() {     
 
    int counter;
    char status[5];
    char p_status[5];
    char *t_begin = NULL;
    char *t_end   = NULL;
    int t_size;

    blockHeader *current = heapStart;
    counter = 1;

    int used_size = 0;
    int free_size = 0;
    int is_used   = -1;

    fprintf(stdout, "************************************Block list***\
                    ********************************\n");
    fprintf(stdout, "No.\tStatus\tPrev\tt_Begin\t\tt_End\t\tt_Size\n");
    fprintf(stdout, "-------------------------------------------------\
                    --------------------------------\n");
  
    while (current->size_status != 1) {
        t_begin = (char*)current;
        t_size = current->size_status;
    
        if (t_size & 1) {
            // LSB = 1 => used block
            strcpy(status, "used");
            is_used = 1;
            t_size = t_size - 1;
        } else {
            strcpy(status, "Free");
            is_used = 0;
        }

        if (t_size & 2) {
            strcpy(p_status, "used");
            t_size = t_size - 2;
        } else {
            strcpy(p_status, "Free");
        }

        if (is_used) 
            used_size += t_size;
        else 
            free_size += t_size;

        t_end = t_begin + t_size - 1;
    
        fprintf(stdout, "%d\t%s\t%s\t0x%08lx\t0x%08lx\t%d\n", counter, status, 
        p_status, (unsigned long int)t_begin, (unsigned long int)t_end, t_size);
    
        current = (blockHeader*)((char*)current + t_size);
        counter = counter + 1;
    }

    fprintf(stdout, "---------------------------------------------------\
                    ------------------------------\n");
    fprintf(stdout, "***************************************************\
                    ******************************\n");
    fprintf(stdout, "Total used size = %d\n", used_size);
    fprintf(stdout, "Total free size = %d\n", free_size);
    fprintf(stdout, "Total size = %d\n", used_size + free_size);
    fprintf(stdout, "***************************************************\
                    ******************************\n");
    fflush(stdout);

    return;  
} 


// end of myHeap.c (fall 2020)

