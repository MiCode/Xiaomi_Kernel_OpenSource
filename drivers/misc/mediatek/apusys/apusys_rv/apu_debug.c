// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "apu.h"

#include "sw_logger.h"
#include "hw_logger.h"

int apu_debug_init(struct mtk_apu *apu)
{
	sw_logger_ipi_init(apu);

	hw_logger_ipi_init(apu);

	return 0;
}

void apu_debug_remove(struct mtk_apu *apu)
{
	sw_logger_ipi_remove(apu);

	hw_logger_ipi_remove(apu);
}
