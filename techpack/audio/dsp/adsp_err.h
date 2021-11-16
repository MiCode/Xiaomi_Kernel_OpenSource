/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 */

#ifndef __ADSP_ERR__
#define __ADSP_ERR__

int adsp_err_get_lnx_err_code(u32 adsp_error);

char *adsp_err_get_err_str(u32 adsp_error);

#endif
