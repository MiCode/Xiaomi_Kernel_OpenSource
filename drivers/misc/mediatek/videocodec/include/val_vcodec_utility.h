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

#ifndef _VAL_VCODEC_UTILITY_H_
#define _VAL_VCODEC_UTILITY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "val_types_public.h"
#include "hal_api.h"

/* for hardware vc1_dec + */

/**
 * @par Function
 *   BPDec
 * @par Description
 *   The function used to BP dec
 * @param
 *   hHandle            [IN/OUT] handle
 * @param
 *   hBitHandle         [IN/OUT] bits nandle
 * @param
 *   bpType             [IN] WMV_BP_TYPE
 * @par Returns
 *   VDDRV_MRESULT_T, return VDDRV_MRESULT_SUCCESS is success, return others if fail
 */
VDDRV_MRESULT_T BPDec(VAL_HANDLE_T hHandle, VAL_HANDLE_T *hBitHandle, WMV_BP_TYPE bpType);


/**
 * @par Function
 *   GetReadBSPt
 * @par Description
 *   The function used to get bitstream pointer
 * @param
 *   hHandle            [IN/OUT] handle
 * @param
 *   hBitsHandle        [IN/OUT] bits nandle
 * @param
 *   pBits              [IN] Bits
 * @par Returns
 *   VAL_UINT32_T, return bitstream pointer
 */
VAL_UINT32_T GetReadBSPt(VAL_HANDLE_T hHandle, VAL_HANDLE_T hBitsHandle, VAL_UINT32_T *pBits);


/**
 * @par Function
 *   GetBPDecBits
 * @par Description
 *   The function used to get decode bits
 * @param
 *   hHandle            [IN/OUT] handle
 * @par Returns
 *   VAL_UINT32_T, return decode bits
 */
VAL_UINT32_T GetBPDecBits(VAL_HANDLE_T hHandle);


/**
 * @par Function
 *   WMVDecode_HW
 * @par Description
 *   The function used to decode WMV
 * @param
 *   hHandle            [IN/OUT] handle
 * @param
 *   hBitHandle         [IN/OUT] bits nandle
 * @par Returns
 *   VDDRV_MRESULT_T, return VDDRV_MRESULT_SUCCESS is success, return others if fail
 */
VDDRV_MRESULT_T WMVDecode_HW(VAL_HANDLE_T hHandle, VAL_HANDLE_T *hBitHandle);
/* for hardware vc1_dec - */

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _VAL_VCODEC_UTILITY_H_ */
