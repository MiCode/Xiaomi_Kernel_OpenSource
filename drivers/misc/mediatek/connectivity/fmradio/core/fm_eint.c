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

#include "osal_typedef.h"
#include "stp_exp.h"
#include "wmt_exp.h"

#include "fm_typedef.h"
#include "fm_dbg.h"
#include "fm_err.h"
#include "fm_eint.h"

fm_s32 fm_enable_eint(void)
{
	return 0;
}

fm_s32 fm_disable_eint(void)
{
	return 0;
}

fm_s32 fm_request_eint(void (*parser) (void))
{
	mtk_wcn_stp_register_event_cb(FM_TASK_INDX, parser);

	return 0;
}

fm_s32 fm_eint_pin_cfg(fm_s32 mode)
{
	return 0;
}

