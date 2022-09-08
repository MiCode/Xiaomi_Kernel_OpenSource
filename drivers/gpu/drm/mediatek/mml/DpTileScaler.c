/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Chris-YC Chen <chris-yc.chen@mediatek.com>
 */
#include "DpTileScaler.h"

#define TILE_SCALER_FORMAT_YUV422	1
#define TILE_SCALER_FORMAT_YUV422_VER_2	1
#define TILE_SCALER_NEGATIVE_OFFSET	1
#define TILE_SCALER_6N_TAP_CUB_ACC	0

#define UNUSED(expr) do { (void)(expr); } while (0)

void backward_4_taps(s32 outTileStart,
		     s32 outTileEnd,
		     s32 outMaxEnd,
		     s32 coeffStep,
		     s32 precision,
		     s32 cropOffset,
		     s32 cropSubpixel,
		     s32 inMaxEnd,
		     s32 inAlignment,
		     s32 *inTileStart,
		     s32 *inTileEnd)
{
    s64 startTemp;
    s64 endTemp;

    UNUSED(outMaxEnd);

    if (cropSubpixel < 0)
        cropSubpixel = -0xfffff;

    cropSubpixel = ((s64)cropSubpixel * precision) >> TILE_SCALER_SUBPIXEL_SHIFT;

    startTemp = (s64)outTileStart * coeffStep + (s64)cropOffset * precision + cropSubpixel;
    if (startTemp < (s64)1 * precision) {
        *inTileStart = 0;
    } else {
	startTemp = DO_COMMON_DIV(startTemp, precision) - 1;
        if (!(startTemp & 0x1) || inAlignment == 1)
            *inTileStart = (s32)startTemp;
        else  /* must be even */
            *inTileStart = (s32)startTemp - 1;
    }

    endTemp = (s64)outTileEnd * coeffStep + (s64)cropOffset * precision + cropSubpixel + 2 * precision;
    if (endTemp > (s64)inMaxEnd * precision) {
        *inTileEnd = inMaxEnd;
    } else {
        /* due to ceiling in forward */
	endTemp = DO_COMMON_DIV(endTemp, precision);
        if (endTemp & 0x1 || inAlignment == 1)
            *inTileEnd = (s32)endTemp;
        else
            *inTileEnd = (s32)endTemp + 1;
    }
}

void forward_4_taps(s32 inTileStart,
		    s32 inTileEnd,
		    s32 inMaxEnd,
		    s32 coeffStep,
		    s32 precision,
		    s32 cropOffset,
		    s32 cropSubpixel,
		    s32 outMaxEnd,
		    s32 outAlignment,
		    s32 backOutStart,
		    s32 outCalOrder,
		    s32 *outTileStart,
		    s32 *outTileEnd,
		    s32 *lumaOffset,
		    s32 *lumaSubpixel,
		    s32 *chromaOffset,
		    s32 *chromaSubpixel)
{
    s32 outTemp;
    s64 startTemp;
    s64 endTemp;
    s64 subTemp;
    s64 offsetCalStart;

    if (cropSubpixel < 0)
        cropSubpixel = -0xfffff;

    // Normalize
    cropSubpixel = ((s64)cropSubpixel * precision) >> TILE_SCALER_SUBPIXEL_SHIFT;

    /* cal pos */
    if (inTileStart == 0) {
        *outTileStart = 0;
    } else {
        startTemp = (s64)(inTileStart + 1) * precision - (s64)cropOffset * precision - cropSubpixel;

	outTemp = (s32)DO_COMMON_DIV(startTemp, coeffStep);
        if ((s64)outTemp * coeffStep < startTemp)  /* ceiling be careful with value smaller than zero */
            outTemp = outTemp + 1;

        if (!(outTemp & 0x1) || outAlignment == 1)
            *outTileStart = outTemp;
        else
            *outTileStart = outTemp + 1;

        if (*outTileStart < 0)
            *outTileStart = 0;
    }

    if (inTileEnd == inMaxEnd) {
        *outTileEnd = outMaxEnd;
    } else {
        endTemp = (s64)(inTileEnd - 1) * precision - (s64)cropOffset * precision - cropSubpixel;

	outTemp = (s32)DO_COMMON_DIV(endTemp, coeffStep);
        if ((s64)outTemp * coeffStep == endTemp)
            outTemp = outTemp - 1;

        if (outTemp & 0x1 || outAlignment == 1)
            *outTileEnd = outTemp;
        else
            *outTileEnd = outTemp - 1;
    }

    if (outCalOrder & 2) { /* Means right to left */
        if (*outTileStart < backOutStart)
            offsetCalStart = backOutStart;
        else
            offsetCalStart = *outTileStart;
    } else {
        /* Normal case */
        *outTileStart = backOutStart;
        offsetCalStart = backOutStart;
    }

    /* Cal bias & offset by fxied backward_out_pos_start */
    subTemp = (s64)offsetCalStart * coeffStep + (s64)cropOffset * precision + cropSubpixel - (s64)inTileStart * precision;

	*lumaOffset = (s32)DO_COMMON_DIV(subTemp, precision);
    *lumaSubpixel = (s32)(subTemp - precision * *lumaOffset);

    if (outAlignment == 1)  /* YUV444 */
        *chromaOffset = *lumaOffset;
    else
        *chromaOffset = *lumaOffset >> 1;

    if (outAlignment == 1 || !(*lumaOffset & 0x1))  /* YUV444 */
        *chromaSubpixel = *lumaSubpixel;
    else
        *chromaSubpixel = *lumaSubpixel + precision;

    if (*outTileStart > *outTileEnd) {
        ASSERT(0);
        return; /* ISP_MESSAGE_TP4_FOR_INVALID_OUT_XYS_XYE_ERROR; */
    }

    if (*outTileEnd > outMaxEnd)
        *outTileEnd = outMaxEnd;
}

void backward_6_taps(s32 outTileStart,
		     s32 outTileEnd,
		     s32 outMaxEnd,
		     s32 coeffStep,
		     s32 precision,
		     s32 cropOffset,
		     s32 cropSubpixel,
		     s32 inMaxEnd,
		     s32 inAlignment,
		     s32 *inTileStart,
		     s32 *inTileEnd)
{
    s64 startTemp;
    s64 endTemp;

    UNUSED(outMaxEnd);

    if (cropSubpixel < 0)
        cropSubpixel = -0xfffff;

    cropSubpixel = ((s64)cropSubpixel * precision) >> TILE_SCALER_SUBPIXEL_SHIFT;

    startTemp = (s64)outTileStart * coeffStep + (s64)cropOffset * precision + cropSubpixel;
#if TILE_SCALER_NEGATIVE_OFFSET
    if (startTemp < (s64)3 * precision)
#else
    if (startTemp < (s64)2 * precision)
#endif
    {
        *inTileStart = 0;
    } else {
#if TILE_SCALER_NEGATIVE_OFFSET
	startTemp = DO_COMMON_DIV(startTemp, precision) - 3;
#else
	startTemp = DO_COMMON_DIV(startTemp, precision) - 2;
#endif
        if (!(startTemp & 0x1) || inAlignment == 1)
            *inTileStart = (s32)startTemp;
        else  /* must be even */
            *inTileStart = (s32)startTemp - 1;
    }

#if TILE_SCALER_FORMAT_YUV422
    endTemp = (s64)outTileEnd * coeffStep + (s64)cropOffset * precision + cropSubpixel + (3 + 2) * precision;
#else
    endTemp = (s64)outTileEnd * coeffStep + (s64)cropOffset * precision + cropSubpixel + 3 * precision;
#endif
    if (endTemp > (s64)inMaxEnd * precision) {
        *inTileEnd = inMaxEnd;
    } else {
        /* due to ceiling in forward */
	endTemp = DO_COMMON_DIV(endTemp, precision);
        if (endTemp & 0x1 || inAlignment == 1)
            *inTileEnd = (s32)endTemp;
        else
            *inTileEnd = (s32)endTemp + 1;
    }
}

void forward_6_taps(s32 inTileStart,
		    s32 inTileEnd,
		    s32 inMaxEnd,
		    s32 coeffStep,
		    s32 precision,
		    s32 cropOffset,
		    s32 cropSubpixel,
		    s32 outMaxEnd,
		    s32 outAlignment,
		    s32 backOutStart,
		    s32 outCalOrder,
		    s32 *outTileStart,
		    s32 *outTileEnd,
		    s32 *lumaOffset,
		    s32 *lumaSubpixel,
		    s32 *chromaOffset,
		    s32 *chromaSubpixel)
{
    s32 outTemp;
    s64 startTemp;
    s64 endTemp;
    s64 subTemp;
    s64 offsetCalStart;

    if (cropSubpixel < 0)
        cropSubpixel = -0xfffff;

    // Normalize
    cropSubpixel = ((s64)cropSubpixel * precision) >> TILE_SCALER_SUBPIXEL_SHIFT;

    /* cal pos */
    if (inTileStart == 0) {
        *outTileStart = 0;
    } else {
#if TILE_SCALER_NEGATIVE_OFFSET
        startTemp = (s64)(inTileStart + 3) * precision - (s64)cropOffset * precision - cropSubpixel;
#else
        startTemp = (s64)(inTileStart + 2) * precision - (s64)cropOffset * precision - cropSubpixel;
#endif

	outTemp = (s32)DO_COMMON_DIV(startTemp, coeffStep);
        if ((s64)outTemp * coeffStep < startTemp)  /* ceiling be careful with value smaller than zero */
            outTemp = outTemp + 1;

        if (!(outTemp & 0x1) || outAlignment == 1)
            *outTileStart = outTemp;
        else
            *outTileStart = outTemp + 1;

        if (*outTileStart < 0)
            *outTileStart = 0;
    }

    if (inTileEnd == inMaxEnd) {
        *outTileEnd = outMaxEnd;
    } else {
#if TILE_SCALER_FORMAT_YUV422
        endTemp = (s64)(inTileEnd - 2 - 2) * precision - (s64)cropOffset * precision - cropSubpixel;
#else
        endTemp = (s64)(inTileEnd - 2) * precision - (s64)cropOffset * precision - cropSubpixel;
#endif

	outTemp = (s32)DO_COMMON_DIV(endTemp, coeffStep);
        if ((s64)outTemp * coeffStep == endTemp)
            outTemp = outTemp - 1;

        if (outTemp & 0x1 || outAlignment == 1)
            *outTileEnd = outTemp;
        else
            *outTileEnd = outTemp - 1;
    }

    if (outCalOrder & 2) { /* Means right to left */
        if (*outTileStart < backOutStart)
            offsetCalStart = backOutStart;
        else
            offsetCalStart = *outTileStart;
    } else {
        /* Normal case */
        *outTileStart = backOutStart;
        offsetCalStart = backOutStart;
    }

    /* Cal bias & offset by fxied backward_out_pos_start */
    subTemp = (s64)offsetCalStart * coeffStep + (s64)cropOffset * precision + cropSubpixel - (s64)inTileStart * precision;

	*lumaOffset = (s32)DO_COMMON_DIV(subTemp, precision);
    *lumaSubpixel = (s32)(subTemp - precision * *lumaOffset);

#if TILE_SCALER_NEGATIVE_OFFSET
    if (*lumaSubpixel < 0) {
        *lumaOffset -= 1;
        *lumaSubpixel += precision;
    }
#endif

#if TILE_SCALER_FORMAT_YUV422_VER_2
    *chromaOffset = *lumaOffset;
    *chromaSubpixel = *lumaSubpixel;
#else
    if (outAlignment == 1)  /* YUV444 */
        *chromaOffset = *lumaOffset;
    else
        *chromaOffset = *lumaOffset >> 1;

    if (outAlignment == 1 || !(*lumaOffset & 0x1))  /* YUV444 */
        *chromaSubpixel = *lumaSubpixel;
    else
        *chromaSubpixel = *lumaSubpixel + precision;
#endif

    if (*outTileStart > *outTileEnd) {
        ASSERT(0);
        return; /* ISP_MESSAGE_TP6_FOR_INVALID_OUT_XYS_XYE_ERROR; */
    }

    if (*outTileEnd > outMaxEnd)
        *outTileEnd = outMaxEnd;
}

void backward_src_acc(s32 outTileStart,
		      s32 outTileEnd,
		      s32 outMaxEnd,
		      s32 coeffStep,
		      s32 precision,
		      s32 cropOffset,
		      s32 cropSubpixel,
		      s32 inMaxEnd,
		      s32 inAlignment,
		      s32 *inTileStart,
		      s32 *inTileEnd)
{
    s32 inTemp;
    s64 startTemp;
    s64 endTemp;

    UNUSED(outMaxEnd);

    if (cropSubpixel < 0)
        cropSubpixel = -0xfffff;
    // Normalize
    cropSubpixel = ((s64)cropSubpixel * coeffStep) >> TILE_SCALER_SUBPIXEL_SHIFT;

    if (coeffStep > precision) {
        ASSERT(0);
        return; /* ISP_MESSAGE_RESIZER_SRC_ACC_SCALING_UP_ERROR; */
    }

    startTemp = (s64)outTileStart * precision + (s64)cropOffset * coeffStep + cropSubpixel;
    if (startTemp * 2 + coeffStep <= precision) {
        *inTileStart = 0;
    } else {
	inTemp = (s32)DO_COMMON_DIV((startTemp * 2 + coeffStep - precision), (2 * coeffStep));
        if (!(inTemp & 0x1) || inAlignment == 1)
            *inTileStart = inTemp;
        else  /* must be even */
            *inTileStart = inTemp - 1;
    }

    endTemp = (s64)outTileEnd * precision + (s64)cropOffset * coeffStep + cropSubpixel;
    if (endTemp * 2 + coeffStep + precision >= (s64)2 * coeffStep * inMaxEnd) {
        *inTileEnd = inMaxEnd;
    } else {
	inTemp = (s32)DO_COMMON_DIV((endTemp * 2 + coeffStep + precision), (2 * coeffStep));
        if ((s64)inTemp * 2 * coeffStep == (s64)2 * endTemp + coeffStep + precision)
            inTemp = inTemp - 1;

        if (inTemp & 0x1 || inAlignment == 1)
            *inTileEnd = inTemp;
        else
            *inTileEnd = inTemp + 1;
    }
}

void forward_src_acc(s32 inTileStart,
		     s32 inTileEnd,
		     s32 inMaxEnd,
		     s32 coeffStep,
		     s32 precision,
		     s32 cropOffset,
		     s32 cropSubpixel,
		     s32 outMaxEnd,
		     s32 outAlignment,
		     s32 backOutStart,
		     s32 outCalOrder,
		     s32 *outTileStart,
		     s32 *outTileEnd,
		     s32 *lumaOffset,
		     s32 *lumaSubpixel,
		     s32 *chromaOffset,
		     s32 *chromaSubpixel)
{
    s32 outTemp;
    s64 startTemp;
    s64 endTemp;
    s64 subTemp;
    s64 offsetCalStart;

    if (cropSubpixel < 0)
        cropSubpixel = -0xfffff;

    // Normalize
    cropSubpixel = ((s64)cropSubpixel *coeffStep) >> TILE_SCALER_SUBPIXEL_SHIFT;

    if (coeffStep > precision) {
        ASSERT(0);
        return; /* ISP_MESSAGE_RESIZER_SRC_ACC_SCALING_UP_ERROR; */
    }

    /* cal pos */
    if (inTileStart == 0) {
        *outTileStart = 0;
    } else {
        startTemp = (s64)inTileStart * coeffStep - ((u32)coeffStep >> 1) - (s64)cropOffset * coeffStep - cropSubpixel;

	outTemp = (s32)DO_COMMON_DIV((startTemp * 2 + precision), (2 * precision));
        if ((s64)outTemp * 2 * precision < startTemp * 2 + precision)  /* ceiling be careful with value smaller than zero */
            outTemp = outTemp + 1;

        if (outTemp < 0)
            *outTileStart = 0;

        if (!(outTemp & 0x1) || outAlignment == 1)
            *outTileStart = outTemp;
        else
            *outTileStart = outTemp + 1;
    }

    if (inTileEnd >= inMaxEnd) {
        *outTileEnd = outMaxEnd;
    } else {
        endTemp = (s64)inTileEnd * coeffStep + ((u32)coeffStep >> 1) - (s64)cropOffset * coeffStep - cropSubpixel;

	outTemp = (s32)DO_COMMON_DIV((endTemp * 2 - precision), (2 * precision));
        if (outTemp & 0x1 || outAlignment == 1)
            *outTileEnd = outTemp;
        else
            *outTileEnd = outTemp - 1;
    }

    if (outCalOrder & 2) { /* Means right to left */
        if (*outTileStart < backOutStart)
            offsetCalStart = backOutStart;
        else
            offsetCalStart = *outTileStart;
    } else {
        /* Normal case */
        *outTileStart = backOutStart;
        offsetCalStart = backOutStart;
    }

    /* cal bias & offset by fxied backward_out_pos_start */
    subTemp = (s64)offsetCalStart * precision + (s64)cropOffset * coeffStep + cropSubpixel - (s64)inTileStart * coeffStep;

	*lumaOffset = (s32)DO_COMMON_DIV(subTemp, precision);
    *chromaOffset = *lumaOffset;
    *lumaSubpixel = (s32)(subTemp - precision * *lumaOffset);
    *chromaSubpixel = *lumaSubpixel;

    if (*outTileStart > *outTileEnd) {
        ASSERT(0);
        return; /* ISP_MESSAGE_SRC_ACC_FOR_INVALID_OUT_XYS_XYE_ERROR; */
    }

    if (*outTileEnd > outMaxEnd)
        *outTileEnd = outMaxEnd;
}

void backward_cub_acc(s32 outTileStart,
		      s32 outTileEnd,
		      s32 outMaxEnd,
		      s32 coeffStep,
		      s32 precision,
		      s32 cropOffset,
		      s32 cropSubpixel,
		      s32 inMaxEnd,
		      s32 inAlignment,
		      s32 *inTileStart,
		      s32 *inTileEnd)
{
    s32 outTemp;
    s64 startTemp;
    s64 endTemp;

    UNUSED(outMaxEnd);

    if (cropSubpixel < 0)
        cropSubpixel = -0xfffff;
    // Normalize
    cropSubpixel = ((s64)cropSubpixel * coeffStep) >> TILE_SCALER_SUBPIXEL_SHIFT;

    if (coeffStep > precision) {
        ASSERT(0);
        return; /* ISP_MESSAGE_RESIZER_CUBIC_ACC_SCALING_UP_ERROR; */
    }

    startTemp = (s64)outTileStart * precision + (s64)cropOffset * coeffStep + cropSubpixel;
#if TILE_SCALER_6N_TAP_CUB_ACC
    if (startTemp < (s32)3 * precision)
#else
    if (startTemp < (s32)2 * precision)
#endif
    {
        *inTileStart = 0;
    } else {
#if TILE_SCALER_6N_TAP_CUB_ACC
	outTemp = (s32)DO_COMMON_DIV((startTemp - 3 * precision), coeffStep);
#else
	outTemp = (s32)DO_COMMON_DIV((startTemp - 2 * precision), coeffStep);
#endif
        if (!(outTemp & 0x1) || inAlignment == 1)
            *inTileStart = outTemp;
        else  /* not even */
            *inTileStart = outTemp - 1;
    }

    endTemp = (s64)outTileEnd * precision + (s64)cropOffset * coeffStep + cropSubpixel;
#if TILE_SCALER_6N_TAP_CUB_ACC
    if (endTemp + coeffStep + 3 * precision >= (s64)inMaxEnd * coeffStep)
#else
    if (endTemp + coeffStep + 2 * precision >= (s64)inMaxEnd * coeffStep)
#endif
    {
        *inTileEnd = inMaxEnd;
    }
    else {
#if TILE_SCALER_6N_TAP_CUB_ACC
	outTemp = (s32)DO_COMMON_DIV((endTemp + coeffStep + 3 * precision), coeffStep);
        if ((s64)outTemp * coeffStep == endTemp + coeffStep + 3 * precision)
#else
	outTemp = (s32)DO_COMMON_DIV((endTemp + coeffStep + 2 * precision), coeffStep);
        if ((s64)outTemp * coeffStep == endTemp + coeffStep + 2 * precision)
#endif
        {
            outTemp = outTemp - 1;
        }

        if (outTemp & 0x1 || inAlignment == 1)
            *inTileEnd = outTemp;
        else  /* not odd */
            *inTileEnd = outTemp + 1;
    }
}

void forward_cub_acc(s32 inTileStart,
		     s32 inTileEnd,
		     s32 inMaxEnd,
		     s32 coeffStep,
		     s32 precision,
		     s32 cropOffset,
		     s32 cropSubpixel,
		     s32 outMaxEnd,
		     s32 outAlignment,
		     s32 backOutStart,
		     s32 outCalOrder,
		     s32 *outTileStart,
		     s32 *outTileEnd,
		     s32 *lumaOffset,
		     s32 *lumaSubpixel,
		     s32 *chromaOffset,
		     s32 *chromaSubpixel)
{
    s32 outTemp;
    s64 startTemp;
    s64 endTemp;
    s64 subTemp;
    s64 offsetCalStart;

    if (cropSubpixel < 0)
        cropSubpixel = -0xfffff;

    cropSubpixel = ((s64)cropSubpixel *coeffStep) >> TILE_SCALER_SUBPIXEL_SHIFT;

    if (coeffStep > precision) {
        ASSERT(0);
        return; /* ISP_MESSAGE_RESIZER_CUBIC_ACC_SCALING_UP_ERROR; */
    }

    /* cal pos */
    if (inTileStart <= 0) {
        *outTileStart = 0;
    } else {
        startTemp = (s64)inTileStart * coeffStep - (s64)cropOffset * coeffStep - cropSubpixel;

	outTemp = (s32)DO_COMMON_DIV(startTemp, precision);
        if ((s64)outTemp * precision < startTemp)  /* ceiling be careful with value smaller than zero */
            outTemp = outTemp + 1;

#if TILE_SCALER_6N_TAP_CUB_ACC
        outTemp = outTemp + 3;
#else
        outTemp = outTemp + 2;
#endif
        if (outTemp < 0)
            outTemp = 0;

        if (!(outTemp & 0x1) || outAlignment == 1)
            *outTileStart = outTemp;
        else
            *outTileStart = outTemp + 1;
    }

    if (inTileEnd >= inMaxEnd) {
        *outTileEnd = outMaxEnd;
    } else {
        endTemp = (s64)inTileEnd * coeffStep - (s64)cropOffset * coeffStep - cropSubpixel;

#if TILE_SCALER_6N_TAP_CUB_ACC
	outTemp = (s32)DO_COMMON_DIV(endTemp, precision - 3);
#else
	outTemp = (s32)DO_COMMON_DIV(endTemp, precision - 2);
#endif
        if (outTemp & 0x1 || outAlignment == 1)
            *outTileEnd = outTemp;
        else
            *outTileEnd = outTemp - 1;
    }

    if (outCalOrder & 2) { /* Means right to left */
        if (*outTileStart < backOutStart)
            offsetCalStart = backOutStart;
        else
            offsetCalStart = *outTileStart;
    } else {
        /* Normal case */
        *outTileStart = backOutStart;
        offsetCalStart = backOutStart;
    }

    /* Cal bias & offset by fxied backward_out_pos_start */
    subTemp = (s64)offsetCalStart * precision + (s64)cropOffset * coeffStep + cropSubpixel - (s64)inTileStart * coeffStep;

	*lumaOffset = (s32)DO_COMMON_DIV(subTemp, precision);
    *chromaOffset = *lumaOffset;
    *lumaSubpixel = (s32)(subTemp - *lumaOffset * precision);
    *chromaSubpixel = *lumaSubpixel;

    if (*outTileStart > *outTileEnd) {
        ASSERT(0);
        return; /* ISP_MESSAGE_CUB_ACC_FOR_INVALID_OUT_XYS_XYE_ERROR; */
    }

    if (*outTileEnd > outMaxEnd)
        *outTileEnd = outMaxEnd;
}
