/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#if !defined( __VOS_MEMORY_H )
#define __VOS_MEMORY_H
 
/**=========================================================================
  
  \file  vos_memory.h
  
  \brief virtual Operating System Servies (vOSS)
               
   Memory management functions
  
   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <vos_types.h>

/*-------------------------------------------------------------------------- 
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/

#ifdef MEMORY_DEBUG
v_VOID_t vos_mem_init(v_VOID_t);
v_VOID_t vos_mem_exit(v_VOID_t);
void vos_mem_clean(void);
#endif

/*-------------------------------------------------------------------------- 
  Type declarations
  ------------------------------------------------------------------------*/


/*------------------------------------------------------------------------- 
  Function declarations and documenation
  ------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
  
  \brief vos_mem_malloc() - vOSS Memory Allocation

  This function will dynamicallly allocate the specified number of bytes of 
  memory.
  
  \param size - the number of bytes of memory to allocate.  
  
  \return Upon successful allocate, returns a non-NULL pointer to the 
  allocated memory.  If this function is unable to allocate the amount of 
  memory specified (for any reason) it returns NULL.
    
  \sa
  
  --------------------------------------------------------------------------*/
#ifdef MEMORY_DEBUG
#define vos_mem_malloc(size) vos_mem_malloc_debug(size, __FILE__, __LINE__)
v_VOID_t * vos_mem_malloc_debug( v_SIZE_t size, char* fileName, v_U32_t lineNum);
#else
v_VOID_t * vos_mem_malloc( v_SIZE_t size );
#endif


/*----------------------------------------------------------------------------
  
  \brief vos_mem_free() - vOSS Free Memory

  This function will free the memory pointed to by 'ptr'.
  
  \param ptr - pointer to the starting address of the memory to be 
               free'd.  
  
  \return Nothing
    
  \sa
  
  --------------------------------------------------------------------------*/
v_VOID_t vos_mem_free( v_VOID_t *ptr );


/*----------------------------------------------------------------------------

  \fn vos_mem_set() - set (fill) memory with a specified byte value.
  
  \param pMemory - pointer to memory that will be set
    
  \param numBytes - the number of bytes to be set
    
  \param value - the byte set in memory
    
  \return - Nothing.  
  
  \sa vos_mem_zero()
  
  --------------------------------------------------------------------------*/
v_VOID_t vos_mem_set( v_VOID_t *ptr, v_SIZE_t numBytes, v_BYTE_t value );


/*----------------------------------------------------------------------------

  \fn vos_mem_zero() - zero out memory
  
  This function sets the memory location to all zeros, essentially clearing
  the memory.
  
  \param pMemory - pointer to memory that will be set to zero
    
  \param numBytes - the number of bytes zero
    
  \param value - the byte set in memory
    
  \return - Nothing.  
  
  \sa vos_mem_set()
  
  --------------------------------------------------------------------------*/
v_VOID_t vos_mem_zero( v_VOID_t *ptr, v_SIZE_t numBytes );


/*----------------------------------------------------------------------------
  
  \brief vos_mem_copy() - Copy memory

  Copy host memory from one location to another, similar to memcpy in 
  standard C.  Note this function does not specifically handle overlapping
  source and destination memory locations.  Calling this function with
  overlapping source and destination memory locations will result in
  unpredictable results.  Use vos_mem_move() if the memory locations
  for the source and destination are overlapping (or could be overlapping!)

  \param pDst - pointer to destination memory location (to copy to)
  
  \param pSrc - pointer to source memory location (to copy from)
  
  \param numBytes - number of bytes to copy.
  
  \return - Nothing
    
  \sa vos_mem_move()
  
  --------------------------------------------------------------------------*/
v_VOID_t vos_mem_copy( v_VOID_t *pDst, const v_VOID_t *pSrc, v_SIZE_t numBytes );


/*----------------------------------------------------------------------------
  
  \brief vos_mem_move() - Move memory

  Move host memory from one location to another, similar to memmove in 
  standard C.  Note this function *does* handle overlapping
  source and destination memory locations.

  \param pDst - pointer to destination memory location (to move to)
  
  \param pSrc - pointer to source memory location (to move from)
  
  \param numBytes - number of bytes to move.
  
  \return - Nothing
    
  \sa vos_mem_move()
  
  --------------------------------------------------------------------------*/
v_VOID_t vos_mem_move( v_VOID_t *pDst, const v_VOID_t *pSrc, v_SIZE_t numBytes );

/** ---------------------------------------------------------------------------

    \fn vos_mem_compare()

    \brief vos_mem_compare() - Memory compare
    
    Function to compare two pieces of memory, similar to memcmp function 
    in standard C.
    
    \param pMemory1 - pointer to one location in memory to compare.

    \param pMemory2 - pointer to second location in memory to compare.
    
    \param numBytes - the number of bytes to compare.
    
    \return v_BOOL_t - returns a boolean value that tells if the memory
                       locations are equal or not equal. 
    
  -------------------------------------------------------------------------------*/
v_BOOL_t vos_mem_compare( v_VOID_t *pMemory1, v_VOID_t *pMemory2, v_U32_t numBytes ); 


/** ---------------------------------------------------------------------------

    \fn vos_mem_compare2()

    \brief vos_mem_compare2() - Memory compare
    
    Function to compare two pieces of memory, similar to memcmp function 
    in standard C.

    \param pMemory1 - pointer to one location in memory to compare.

    \param pMemory2 - pointer to second location in memory to compare.
    
    \param numBytes - the number of bytes to compare.
    
    \return v_SINT_t - returns a boolean value that tells if the memory
                       locations are equal or not equal. 
                       0 -- equal
                       < 0 -- *pMemory1 is less than *pMemory2
                       > 0 -- *pMemory1 is bigger than *pMemory2
    
  -------------------------------------------------------------------------------*/
v_SINT_t vos_mem_compare2( v_VOID_t *pMemory1, v_VOID_t *pMemory2, v_U32_t numBytes );


/*----------------------------------------------------------------------------
  
  \brief vos_mem_dma_malloc() - vOSS DMA Memory Allocation

  This function will dynamicallly allocate the specified number of bytes of 
  memory. This memory will have special attributes making it DMA friendly i.e.
  it will exist in contiguous, 32-byte aligned uncached memory. A normal 
  vos_mem_malloc does not yield memory with these attributes. 

  NOTE: the special DMA friendly memory is very scarce and this API must be
  used sparingly
  
  \param size - the number of bytes of memory to allocate.  
  
  \return Upon successful allocate, returns a non-NULL pointer to the 
  allocated memory.  If this function is unable to allocate the amount of 
  memory specified (for any reason) it returns NULL.
    
  \sa
  
  --------------------------------------------------------------------------*/
#ifdef MEMORY_DEBUG
#define vos_mem_dma_malloc(size) vos_mem_dma_malloc_debug(size, __FILE__, __LINE__)
v_VOID_t * vos_mem_dma_malloc_debug( v_SIZE_t size, char* fileName, v_U32_t lineNum);
#else
v_VOID_t * vos_mem_dma_malloc( v_SIZE_t size );
#endif


/*----------------------------------------------------------------------------
  
  \brief vos_mem_dma_free() - vOSS DMA Free Memory

  This function will free special DMA friendly memory pointed to by 'ptr'.
  
  \param ptr - pointer to the starting address of the memory to be 
               free'd.  
  
  \return Nothing
    
  \sa
  
  --------------------------------------------------------------------------*/
v_VOID_t vos_mem_dma_free( v_VOID_t *ptr );

#ifdef DMA_DIRECT_ACCESS
/*----------------------------------------------------------------------------
  
  \brief vos_mem_set_dma_ptr() - vOSS DMA memory poiter set by SAL

  This function will set DMA Physical memory pointer.

  
  \param dmaBuffer - pointer to the starting address of the memory to be 
               free'd.  
  
  \return Nothing
    
  \sa
  
  --------------------------------------------------------------------------*/
v_VOID_t vos_mem_set_dma_ptr(unsigned char *dmaBuffer);
#endif /* DMA_DIRECT_ACCESS */
#endif // __VOSS_LOCK_H
