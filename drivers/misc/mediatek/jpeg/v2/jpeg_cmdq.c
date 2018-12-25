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

#include <cmdq_core.h>
#include <cmdq_reg.h>

#if defined(CONFIG_MTK_LEGACY)
#include <mach/mt_clkmgr.h>
#endif

#include "jpeg_drv_reg.h"
#include "jpeg_cmdq.h"


int32_t cmdqJpegClockOn(uint64_t engineFlag)
{
#if 0
	if (engineFlag & (1 << CMDQ_ENG_JPEG_DEC))
		enable_clock(MT65XX_PDN_MM_JPEG_DEC, "JPEG");

	if (engineFlag & (1 << CMDQ_ENG_JPEG_ENC_PASS1))
		enable_clock(MT65XX_PDN_MM_JPEG_DEC, "JPEG");

	if (engineFlag & (1 << CMDQ_ENG_JPEG_ENC_PASS2))
		enable_clock(MT65XX_PDN_MM_JPEG_DEC, "JPEG");

#endif
	return 0;
}


int32_t cmdqJpegDumpInfo(uint64_t engineFlag, int level)
{
	return 0;
}


int32_t cmdqJpegResetEng(uint64_t engineFlag)
{
	if (engineFlag & (1 << CMDQ_ENG_JPEG_DEC)) {
		IMG_REG_WRITE(0x0000FFFF, REG_ADDR_JPGDEC_INTERRUPT_STATUS);
		IMG_REG_WRITE(0x00, REG_ADDR_JPGDEC_RESET);
		IMG_REG_WRITE(0x01, REG_ADDR_JPGDEC_RESET);

		IMG_REG_WRITE(0x00, REG_ADDR_JPGDEC_RESET);
		IMG_REG_WRITE(0x10, REG_ADDR_JPGDEC_RESET);
	}

	if (engineFlag & (1 << CMDQ_ENG_JPEG_ENC)) {
		IMG_REG_WRITE(0, REG_ADDR_JPEG_ENC_RSTB);
		IMG_REG_WRITE(1, REG_ADDR_JPEG_ENC_RSTB);
	}

	if (engineFlag & (1 << CMDQ_ENG_JPEG_REMDC)) {
		IMG_REG_WRITE(0, REG_ADDR_JPEG_ENC_PASS2_RSTB);
		IMG_REG_WRITE(1, REG_ADDR_JPEG_ENC_PASS2_RSTB);
	}

	return 0;
}


int32_t cmdqJpegClockOff(uint64_t engineFlag)
{

#if 0
	if (engineFlag & (1 << CMDQ_ENG_JPEG_DEC))
		disable_clock(MT65XX_PDN_MM_JPEG_DEC, "JPEG");

	if (engineFlag & (1 << CMDQ_ENG_JPEG_ENC_PASS1))
		disable_clock(MT65XX_PDN_MM_JPEG_DEC, "JPEG");

	if (engineFlag & (1 << CMDQ_ENG_JPEG_ENC_PASS2))
		disable_clock(MT65XX_PDN_MM_JPEG_DEC, "JPEG");

#endif
	return 0;
}
