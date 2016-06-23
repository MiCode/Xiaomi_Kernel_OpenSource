/*
 * Copyright (c) 2012-2014 The Linux Foundation. All rights reserved.
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
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/*===========================================================================
  @file vos_memory.c

  @brief Virtual Operating System Services Memory API

  
  Copyright (c) 2008 QUALCOMM Incorporated.
  All Rights Reserved.
  Qualcomm Confidential and Proprietary
===========================================================================*/

/*=========================================================================== 
    
                       EDIT HISTORY FOR FILE 
   
                         
  This section contains comments describing changes made to the module. 
  Notice that changes are listed in reverse chronological order. 
   
   
  $Header:$ $DateTime: $ $Author: $ 
   
   
  when        who    what, where, why 
  --------    ---    --------------------------------------------------------
     
===========================================================================*/ 

/*---------------------------------------------------------------------------
 * Include Files
 * ------------------------------------------------------------------------*/
#include "vos_memory.h"
#include "vos_trace.h"

#ifdef CONFIG_WCNSS_MEM_PRE_ALLOC
#include <linux/wcnss_wlan.h>
#define WCNSS_PRE_ALLOC_GET_THRESHOLD (4*1024)
#endif

#ifdef MEMORY_DEBUG
#include "wlan_hdd_dp_utils.h"

hdd_list_t vosMemList;

static v_U8_t WLAN_MEM_HEADER[] =  {0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68 };
static v_U8_t WLAN_MEM_TAIL[]   =  {0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87};
static int    memory_dbug_flag;

struct s_vos_mem_struct
{
   hdd_list_node_t pNode;
   char* fileName;
   unsigned int lineNum;
   unsigned int size;
   v_U8_t header[8];
};
#endif

/*---------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * ------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
 * Type Declarations
 * ------------------------------------------------------------------------*/
  
/*---------------------------------------------------------------------------
 * Data definitions
 * ------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
 * External Function implementation
 * ------------------------------------------------------------------------*/
#ifdef MEMORY_DEBUG
void vos_mem_init()
{
   /* Initalizing the list with maximum size of 60000 */
   hdd_list_init(&vosMemList, 60000);  
   memory_dbug_flag = 1;
   return; 
}

void vos_mem_clean()
{
    v_SIZE_t listSize;
    hdd_list_size(&vosMemList, &listSize);

    if(listSize)
    {
       hdd_list_node_t* pNode;
       VOS_STATUS vosStatus;

       struct s_vos_mem_struct* memStruct;
       char* prev_mleak_file = "";
       unsigned int prev_mleak_lineNum = 0;
       unsigned int prev_mleak_sz = 0;
       unsigned int mleak_cnt = 0;
 
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
             "%s: List is not Empty. listSize %d ", __func__, (int)listSize);

       do
       {
          spin_lock(&vosMemList.lock);
          vosStatus = hdd_list_remove_front(&vosMemList, &pNode);
          spin_unlock(&vosMemList.lock);
          if(VOS_STATUS_SUCCESS == vosStatus)
          {
             memStruct = (struct s_vos_mem_struct*)pNode;

             /* Take care to log only once multiple memory leaks from
              * the same place */
             if(strcmp(prev_mleak_file, memStruct->fileName) ||
                (prev_mleak_lineNum != memStruct->lineNum) ||
                (prev_mleak_sz !=  memStruct->size))
             {
                if(mleak_cnt != 0)
                {
                   VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                      "%d Time Memory Leak@ File %s, @Line %d, size %d",
                      mleak_cnt, prev_mleak_file, prev_mleak_lineNum,
                      prev_mleak_sz);
                }
                prev_mleak_file = memStruct->fileName;
                prev_mleak_lineNum = memStruct->lineNum;
                prev_mleak_sz =  memStruct->size;
                mleak_cnt = 0;
             }
             mleak_cnt++;

             kfree((v_VOID_t*)memStruct);
          }
       }while(vosStatus == VOS_STATUS_SUCCESS);

       /* Print last memory leak from the module */
       if(mleak_cnt)
       {
          VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                      "%d Time memory Leak@ File %s, @Line %d, size %d",
                      mleak_cnt, prev_mleak_file, prev_mleak_lineNum,
                      prev_mleak_sz);
       }


#ifdef CONFIG_HALT_KMEMLEAK
       BUG_ON(0);
#endif
    }
}

void vos_mem_exit()
{
    if (memory_dbug_flag)
    {
       vos_mem_clean();
       hdd_list_destroy(&vosMemList);
    }
}

v_VOID_t * vos_mem_malloc_debug( v_SIZE_t size, char* fileName, v_U32_t lineNum)
{
   struct s_vos_mem_struct* memStruct;
   v_VOID_t* memPtr = NULL;
   v_SIZE_t new_size;
   int flags = GFP_KERNEL;
   unsigned long IrqFlags;


   if (size > (1024*1024))
   {
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: called with arg > 1024K; passed in %d !!!", __func__,size); 
       return NULL;
   }

   if (in_interrupt())
   {
      flags = GFP_ATOMIC;
   }

   if (!memory_dbug_flag)
   {
#ifdef CONFIG_WCNSS_MEM_PRE_ALLOC
      v_VOID_t* pmem;
      if (size > WCNSS_PRE_ALLOC_GET_THRESHOLD)
      {
           pmem = wcnss_prealloc_get(size);
           if (NULL != pmem)
               return pmem;
      }
#endif
      return kmalloc(size, flags);
   }

   new_size = size + sizeof(struct s_vos_mem_struct) + 8; 

   memStruct = (struct s_vos_mem_struct*)kmalloc(new_size, flags);

   if(memStruct != NULL)
   {
      VOS_STATUS vosStatus;

      memStruct->fileName = fileName;
      memStruct->lineNum  = lineNum;
      memStruct->size     = size;

      vos_mem_copy(&memStruct->header[0], &WLAN_MEM_HEADER[0], sizeof(WLAN_MEM_HEADER));
      vos_mem_copy( (v_U8_t*)(memStruct + 1) + size, &WLAN_MEM_TAIL[0], sizeof(WLAN_MEM_TAIL));

      spin_lock_irqsave(&vosMemList.lock, IrqFlags);
      vosStatus = hdd_list_insert_front(&vosMemList, &memStruct->pNode);
      spin_unlock_irqrestore(&vosMemList.lock, IrqFlags);
      if(VOS_STATUS_SUCCESS != vosStatus)
      {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
             "%s: Unable to insert node into List vosStatus %d", __func__, vosStatus);
      }

      memPtr = (v_VOID_t*)(memStruct + 1); 
   }
   return memPtr;
}

v_VOID_t vos_mem_free( v_VOID_t *ptr )
{

    unsigned long IrqFlags;
    if (ptr == NULL)
        return;

    if (!memory_dbug_flag)
    {
#ifdef CONFIG_WCNSS_MEM_PRE_ALLOC
        if (wcnss_prealloc_put(ptr))
           return;
#endif
        kfree(ptr);
    }
    else
    {
        VOS_STATUS vosStatus;
        struct s_vos_mem_struct* memStruct = ((struct s_vos_mem_struct*)ptr) - 1;

        spin_lock_irqsave(&vosMemList.lock, IrqFlags);
        vosStatus = hdd_list_remove_node(&vosMemList, &memStruct->pNode);
        spin_unlock_irqrestore(&vosMemList.lock, IrqFlags);

        if(VOS_STATUS_SUCCESS == vosStatus)
        {
            if(0 == vos_mem_compare(memStruct->header, &WLAN_MEM_HEADER[0], sizeof(WLAN_MEM_HEADER)) )
            {
               VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL, 
                    "Memory Header is corrupted. MemInfo: Filename %s, LineNum %d", 
                                memStruct->fileName, (int)memStruct->lineNum);
            }
            if(0 == vos_mem_compare( (v_U8_t*)ptr + memStruct->size, &WLAN_MEM_TAIL[0], sizeof(WLAN_MEM_TAIL ) ) )
            {
               VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL, 
                    "Memory Trailer is corrupted. MemInfo: Filename %s, LineNum %d", 
                                memStruct->fileName, (int)memStruct->lineNum);
            }
            kfree((v_VOID_t*)memStruct);
        }
        else
        {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                      "%s: Unallocated memory (double free?)", __func__);
            VOS_BUG(0);
        }
    }
}
#else
v_VOID_t * vos_mem_malloc( v_SIZE_t size )
{
   int flags = GFP_KERNEL;
#ifdef CONFIG_WCNSS_MEM_PRE_ALLOC
    v_VOID_t* pmem;
#endif    
   if (size > (1024*1024))
   {
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: called with arg > 1024K; passed in %d !!!", __func__,size); 
       return NULL;
   }
   if (in_interrupt() || irqs_disabled() || in_atomic())
   {
      flags = GFP_ATOMIC;
   }
#ifdef CONFIG_WCNSS_MEM_PRE_ALLOC
   if(size > WCNSS_PRE_ALLOC_GET_THRESHOLD)
   {
       pmem = wcnss_prealloc_get(size);
       if(NULL != pmem) 
           return pmem;
   }
#endif
   return kmalloc(size, flags);
}   

v_VOID_t vos_mem_free( v_VOID_t *ptr )
{
    if (ptr == NULL)
      return;

#ifdef CONFIG_WCNSS_MEM_PRE_ALLOC
    if(wcnss_prealloc_put(ptr))
        return;
#endif

    kfree(ptr);
}
#endif

v_VOID_t vos_mem_set( v_VOID_t *ptr, v_SIZE_t numBytes, v_BYTE_t value )
{
   if (ptr == NULL)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s called with NULL parameter ptr", __func__);
      return;
   }
   memset(ptr, value, numBytes);
}

v_VOID_t vos_mem_zero( v_VOID_t *ptr, v_SIZE_t numBytes )
{
   if (0 == numBytes)
   {
      // special case where ptr can be NULL
      return;
   }

   if (ptr == NULL)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s called with NULL parameter ptr", __func__);
      return;
   }
   memset(ptr, 0, numBytes);
   
}

v_VOID_t vos_mem_copy( v_VOID_t *pDst, const v_VOID_t *pSrc, v_SIZE_t numBytes )
{
   if (0 == numBytes)
   {
      // special case where pDst or pSrc can be NULL
      return;
   }

   if ((pDst == NULL) || (pSrc==NULL))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s called with NULL parameter, source:%p destination:%p",
                __func__, pSrc, pDst);
      VOS_ASSERT(0);
      return;
   }
   memcpy(pDst, pSrc, numBytes);
}

v_VOID_t vos_mem_move( v_VOID_t *pDst, const v_VOID_t *pSrc, v_SIZE_t numBytes )
{
   if (0 == numBytes)
   {
      // special case where pDst or pSrc can be NULL
      return;
   }

   if ((pDst == NULL) || (pSrc==NULL))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s called with NULL parameter, source:%p destination:%p",
                __func__, pSrc, pDst);
      VOS_ASSERT(0);
      return;
   }
   memmove(pDst, pSrc, numBytes);
}

v_BOOL_t vos_mem_compare( v_VOID_t *pMemory1, v_VOID_t *pMemory2, v_U32_t numBytes )
{ 
   if (0 == numBytes)
   {
      // special case where pMemory1 or pMemory2 can be NULL
      return VOS_TRUE;
   }

   if ((pMemory1 == NULL) || (pMemory2==NULL))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s called with NULL parameter, p1:%p p2:%p",
                __func__, pMemory1, pMemory2);
      VOS_ASSERT(0);
      return VOS_FALSE;
   }
   return (memcmp(pMemory1, pMemory2, numBytes)?VOS_FALSE:VOS_TRUE);
}   


v_SINT_t vos_mem_compare2( v_VOID_t *pMemory1, v_VOID_t *pMemory2, v_U32_t numBytes )

{ 
   return( (v_SINT_t) memcmp( pMemory1, pMemory2, numBytes ) );
}

/*----------------------------------------------------------------------------
  
  \brief vos_mem_dma_malloc() - vOSS DMA Memory Allocation

  This function will dynamicallly allocate the specified number of bytes of 
  memory. This memory will have special attributes making it DMA friendly i.e.
  it will exist in contiguous, 32-byte aligned uncached memory. A normal 
  vos_mem_malloc does not yield memory with these attributes. 

  NOTE: the special DMA friendly memory is very scarce and this API must be
  used sparingly

  On WM, there is nothing special about this memory. SDHC allocates the 
  DMA friendly buffer and copies the data into it
  
  \param size - the number of bytes of memory to allocate.  
  
  \return Upon successful allocate, returns a non-NULL pointer to the 
  allocated memory.  If this function is unable to allocate the amount of 
  memory specified (for any reason) it returns NULL.
    
  \sa
  
  --------------------------------------------------------------------------*/
#ifdef MEMORY_DEBUG
v_VOID_t * vos_mem_dma_malloc_debug( v_SIZE_t size, char* fileName, v_U32_t lineNum)
{
   struct s_vos_mem_struct* memStruct;
   v_VOID_t* memPtr = NULL;
   v_SIZE_t new_size;

   if (in_interrupt())
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s cannot be called from interrupt context!!!", __func__);
      return NULL;
   }

   if (!memory_dbug_flag)
      return kmalloc(size, GFP_KERNEL);

   new_size = size + sizeof(struct s_vos_mem_struct) + 8; 

   memStruct = (struct s_vos_mem_struct*)kmalloc(new_size,GFP_KERNEL);

   if(memStruct != NULL)
   {
      VOS_STATUS vosStatus;

      memStruct->fileName = fileName;
      memStruct->lineNum  = lineNum;
      memStruct->size     = size;

      vos_mem_copy(&memStruct->header[0], &WLAN_MEM_HEADER[0], sizeof(WLAN_MEM_HEADER));
      vos_mem_copy( (v_U8_t*)(memStruct + 1) + size, &WLAN_MEM_TAIL[0], sizeof(WLAN_MEM_TAIL));

      spin_lock(&vosMemList.lock);
      vosStatus = hdd_list_insert_front(&vosMemList, &memStruct->pNode);
      spin_unlock(&vosMemList.lock);
      if(VOS_STATUS_SUCCESS != vosStatus)
      {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
             "%s: Unable to insert node into List vosStatus %d", __func__, vosStatus);
      }

      memPtr = (v_VOID_t*)(memStruct + 1); 
   }

   return memPtr;
}

v_VOID_t vos_mem_dma_free( v_VOID_t *ptr )
{
    if (ptr == NULL)
        return;

    if (memory_dbug_flag)
    {
        VOS_STATUS vosStatus;
        struct s_vos_mem_struct* memStruct = ((struct s_vos_mem_struct*)ptr) - 1;

        spin_lock(&vosMemList.lock);
        vosStatus = hdd_list_remove_node(&vosMemList, &memStruct->pNode);
        spin_unlock(&vosMemList.lock);

        if(VOS_STATUS_SUCCESS == vosStatus)
        {
            if(0 == vos_mem_compare(memStruct->header, &WLAN_MEM_HEADER[0], sizeof(WLAN_MEM_HEADER)) )
            {
               VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL, 
                    "Memory Header is corrupted. MemInfo: Filename %s, LineNum %d", 
                                memStruct->fileName, (int)memStruct->lineNum);
            }
            if(0 == vos_mem_compare( (v_U8_t*)ptr + memStruct->size, &WLAN_MEM_TAIL[0], sizeof(WLAN_MEM_TAIL ) ) )
            {
               VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL, 
                    "Memory Trailer is corrupted. MemInfo: Filename %s, LineNum %d", 
                                memStruct->fileName, (int)memStruct->lineNum);
            }
            kfree((v_VOID_t*)memStruct);
        }
    }
    else
       kfree(ptr);
}
#else
v_VOID_t* vos_mem_dma_malloc( v_SIZE_t size )
{
   if (in_interrupt())
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s cannot be called from interrupt context!!!", __func__);
      return NULL;
   }
   return kmalloc(size, GFP_KERNEL);
}

/*----------------------------------------------------------------------------
  
  \brief vos_mem_dma_free() - vOSS DMA Free Memory

  This function will free special DMA friendly memory pointed to by 'ptr'.

  On WM, there is nothing special about the memory being free'd. SDHC will
  take care of free'ing the DMA friendly buffer
  
  \param ptr - pointer to the starting address of the memory to be 
               free'd.  
  
  \return Nothing
    
  \sa
  
  --------------------------------------------------------------------------*/
v_VOID_t vos_mem_dma_free( v_VOID_t *ptr )
{
    if (ptr == NULL)
      return;
    kfree(ptr);
}
#endif
