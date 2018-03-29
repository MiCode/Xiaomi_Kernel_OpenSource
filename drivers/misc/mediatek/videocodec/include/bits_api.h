/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _BITS_API_H_
#define _BITS_API_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "val_types_public.h"
#include "hal_api.h"

typedef VAL_UINT32_T(*fgPrepare32FN)(VAL_HANDLE_T * a_phBitsHandle);

/**
 * @par Structure
 *  VBITS_HANDLE_T
 * @par Description
 *  This is a parameter for bitstream parsing utility related function
 */
typedef struct __VBITS_HANDLE_T {
	VAL_HANDLE_T    hHALHandle;                             /* /< HAL Handle */
	VAL_HANDLE_T    hVALHandle;                             /* /< VAL Handle */
	VAL_MEM_ADDR_T  BitsStart;                              /* /< Bits Start */
	VAL_MEMORY_T    rHandleMem;                             /* /< Handle memory */
	VAL_UINT32_T    nReadingMode;                           /* /< 0 for software, 1 for mmap, 2 for hardware */
	VAL_ULONG_T     StartAddr;                              /* /< used for software mode fast access */
	VAL_ULONG_T     nSize;                                  /* /< Size */
	VAL_UINT32_T    nBitCnt;                                /* /< bits count */
	VAL_UINT32_T    nZeroCnt;                               /* /< zero count */
	VAL_UINT32_T    Cur32Bits;                              /* /< current 32 bits */
	VAL_UINT32_T    CurBitCnt;                              /* /< current bits count */
	VAL_UINT32_T    n03RemoveCount;                         /* /< 03 Remove Count */
	VAL_UINT32_T    n03CountBit;                            /* /< 03 Count Bit */
	VAL_INT32_T     n03FirstIndex;                          /* /< 03 First Index */
	VAL_INT32_T     n03SecondIndex;                         /* /< 03 Second Index */
	VAL_UINT32_T    n03RemoveIgnore;                        /* /< 03 Remove Ignore */
	VAL_BOOL_T      bFirstCheck;                            /* /< First Check */
	VAL_BOOL_T      bEverRemove;                            /* /< Ever Remove */
	VAL_BOOL_T      bIgnoreByBS;                            /* /< Ignore By BS */
	VAL_BOOL_T      bEOF;                                   /* /< EOF */
	fgPrepare32FN   Prepare32Bits;                          /* /< Prepare 32 Bits */
	VAL_DRIVER_TYPE_T vFormat;                              /* /< Format */
	VAL_UINT32_T    value;                                  /* /< value */
} VBITS_HANDLE_T;


/**
 * @par Enumeration
 *   VBITS_READTYPE_T
 * @par Description
 *   This is the item used for bits read type
 */
typedef enum VBITS_READTYPE_T {
	VBITS_SOFTWARE = 0,         /* /< software */
	VBITS_MMAP,                 /* /< mmap */
	VBITS_HARDWARE,             /* /< hardware */
	VBITS_MAX                   /* /< MAX value */
} VBITS_READTYPE_T;
/*=============================================================================
 *                             Function Declaration
 *===========================================================================*/


/**
 * @par Function
 *   eBufEnable
 * @par Description
 *   The hal init & HW enable function
 * @param
 *   a_phBitsHandle     [IN/OUT] The bits handle
 * @param
 *   hHALHandle         [IN/OUT] The hal handle
 * @param
 *   nMode              [IN] VBITS_READTYPE_T
 * @param
 *   vFormat            [IN] VAL_DRIVER_TYPE_T
 * @par Returns
 *   VAL_UINT32_T, return VAL_RESULT_NO_ERROR if success, return VAL_RESULT_UNKNOWN_ERROR if failed
 */
VAL_UINT32_T eBufEnable(
	VAL_HANDLE_T *a_phBitsHandle,
	VAL_HANDLE_T hHALHandle,
	VAL_UINT32_T nMode,
	VAL_DRIVER_TYPE_T vFormat
);


/**
 * @par Function
 *   eBufEnable
 * @par Description
 *   The HW disable function
 * @param
 *   a_phBitsHandle     [IN/OUT] The bits handle
 * @param
 *   hHALHandle         [IN/OUT] The hal handle
 * @param
 *   nMode              [IN] VBITS_READTYPE_T
 * @param
 *   vFormat            [IN] VAL_DRIVER_TYPE_T
 * @par Returns
 *   VAL_UINT32_T, return VAL_RESULT_NO_ERROR if success, return VAL_RESULT_UNKNOWN_ERROR if failed
 */
VAL_UINT32_T eBufDisable(
	VAL_HANDLE_T *a_phBitsHandle,
	VAL_HANDLE_T hHALHandle,
	VAL_UINT32_T nMode,
	VAL_DRIVER_TYPE_T vFormat
);


/**
 * @par Function
 *   eBufInit
 * @par Description
 *   The common init function
 * @param
 *   a_phBitsHandle     [IN/OUT] The bits handle
 * @param
 *   hVALHandle         [IN/OUT] The val handle
 * @param
 *   hHALHandle         [IN/OUT] The hal handle
 * @param
 *   rBufAddrStart      [IN] The buffer start address
 * @param
 *   nMode              [IN] VBITS_READTYPE_T
 * @param
 *   vFormat            [IN] VAL_DRIVER_TYPE_T
 * @par Returns
 *   VAL_RESULT_T, return VAL_RESULT_NO_ERROR if success, return others if failed
 */
VAL_RESULT_T eBufInit(
	VAL_HANDLE_T *a_phBitsHandle,
	VAL_HANDLE_T hVALHandle,
	VAL_HANDLE_T hHALHandle,
	VAL_MEM_ADDR_T rBufAddrStart,
	VAL_UINT32_T nMode,
	VAL_DRIVER_TYPE_T vFormat
);


/**
 * @par Function
 *   eBufDeinit
 * @par Description
 *   The common deinit function
 * @param
 *   a_phBitsHandle     [IN/OUT] The bits handle
 * @par Returns
 *   VAL_RESULT_T, return VAL_RESULT_NO_ERROR if success, return others if failed
 */
VAL_RESULT_T eBufDeinit(VAL_HANDLE_T *a_phBitsHandle);


/**
 * @par Function
 *   eBufGetBitCnt
 * @par Description
 *   The function is used to get current bit count
 * @param
 *   a_phBitsHandle     [IN/OUT] The bits handle
 * @par Returns
 *   VAL_UINT32_T, return current bit count
 */
VAL_UINT32_T eBufGetBitCnt(VAL_HANDLE_T *a_phBitsHandle);


/**
 * @par Function
 *   eBufGetBits
 * @par Description
 *   The function is used to get current bits by numBits
 * @param
 *   a_phBitsHandle     [IN/OUT] The bits handle
 * @param
 *   numBits            [IN] The number bits
 * @par Returns
 *   VAL_UINT32_T, return current bits by numBits
 */
VAL_UINT32_T eBufGetBits(VAL_HANDLE_T *a_phBitsHandle, VAL_UINT32_T numBits);


/**
 * @par Function
 *   eBufNextBits
 * @par Description
 *   The function is used to show current bits by numBits
 * @param
 *   a_phBitsHandle     [IN/OUT] The bits handle
 * @param
 *   numBits            [IN] The number bits
 * @par Returns
 *   VAL_UINT32_T, return current bits by numBits
 */
VAL_UINT32_T eBufNextBits(VAL_HANDLE_T *a_phBitsHandle, VAL_UINT32_T numBits);


/**
 * @par Function
 *   eBufGetUEGolomb
 * @par Description
 *   The function is used to get unsigned EGolomb bits
 * @param
 *   a_phBitsHandle     [IN/OUT] The bits handle
 * @par Returns
 *   VAL_UINT32_T, return current unsigned EGolomb bits
 */
VAL_UINT32_T eBufGetUEGolomb(VAL_HANDLE_T *a_phBitsHandle);


/**
 * @par Function
 *   eBufGetSEGolomb
 * @par Description
 *   The function is used to get signed EGolomb bits
 * @param
 *   a_phBitsHandle     [IN/OUT] The bits handle
 * @par Returns
 *   VAL_INT32_T, return current signed EGolomb bits
 */
VAL_INT32_T  eBufGetSEGolomb(VAL_HANDLE_T *a_phBitsHandle);


/**
 * @par Function
 *   eBufCheckEOF
 * @par Description
 *   The function is used to check EOF bitstream
 * @param
 *   a_phBitsHandle     [IN/OUT] The bits handle
 * @par Returns
 *   VAL_BOOL_T, return EOF or not
 */
VAL_BOOL_T   eBufCheckEOF(VAL_HANDLE_T *a_phBitsHandle);


/**
 * @par Function
 *   eBufGetBufSize
 * @par Description
 *   The function is used to get buffer size
 * @param
 *   a_phBitsHandle     [IN/OUT] The bits handle
 * @par Returns
 *   VAL_BOOL_T, return buffer size
 */
VAL_UINT32_T eBufGetBufSize(VAL_HANDLE_T *a_phBitsHandle);


/**
 * @par Function
 *   NextBytesAlignment
 * @par Description
 *   The function is used to jump bitstream pointer to next bytesalignment
 * @param
 *   a_phBitsHandle     [IN/OUT] The bits handle
 * @param
 *   nBytesAlignment    [IN] BytesAlignment
 * @par Returns
 *   void
 */
void NextBytesAlignment(VAL_HANDLE_T *a_phBitsHandle, VAL_UINT32_T nBytesAlignment);


/**
 * @par Function
 *   eBufInitBS
 * @par Description
 *   The function is used to init bit stream
 * @param
 *   a_phBitsHandle     [IN/OUT] The bits handle
 * @param
 *   cmd_queue          [IN] command queue
 * @param
 *   pIndex             [IN] command queue index
 * @par Returns
 *   VAL_BOOL_T, return VAL_TRUE if success, return VAL_FALSE if failed
 */
VAL_BOOL_T   eBufInitBS(VAL_HANDLE_T *a_phBitsHandle, P_VCODEC_DRV_CMD_T cmd_queue, VAL_UINT32_T *pIndex);


/**
 * @par Function
 *   eBufGetPAddr
 * @par Description
 *   The function is used to get physical address
 * @param
 *   a_phBitsHandle     [IN/OUT] The bits handle
 * @par Returns
 *   VAL_UINT32_T, return physical address
 */
VAL_UINT32_T eBufGetPAddr(VAL_HANDLE_T *a_phBitsHandle);


/**
 * @par Function
 *   eBufGetPAddr
 * @par Description
 *   The function is used to re init
 * @param
 *   a_phBitsHandle     [IN/OUT] The bits handle
 * @param
 *   nBytes             [IN] The Bytes
 * @param
 *   nBits              [IN] The Bits
 * @par Returns
 *   VAL_BOOL_T, return VAL_TRUE if success, return VAL_FALSE if failed
 */
VAL_BOOL_T eBufReInite(VAL_HANDLE_T *a_phBitsHandle, VAL_UINT32_T nBytes, VAL_UINT32_T nBits);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _VAL_API_H_ */
