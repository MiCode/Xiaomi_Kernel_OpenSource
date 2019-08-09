/*
 * mddp_if.h - Structure/API provided by mddp_if.c
 *
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MDDP_IF_H
#define __MDDP_IF_H

#include <linux/types.h>

#include "mddp_ctrl.h"

//------------------------------------------------------------------------------
// Struct definition.
// -----------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Public functions.
// -----------------------------------------------------------------------------
uint32_t mddp_on_enable(enum mddp_app_type_e type);
uint32_t mddp_on_disable(enum mddp_app_type_e type);
int32_t mddp_on_activate(enum mddp_app_type_e type,
		uint8_t *ul_dev_name, uint8_t *dl_dev_name);
bool mddp_on_deactivate(enum mddp_app_type_e type);
int32_t mddp_on_get_offload_stats(enum mddp_app_type_e type,
		uint8_t *buf, uint32_t *buf_len);
int32_t mddp_on_set_data_limit(enum mddp_app_type_e type,
		uint8_t *buf, uint32_t buf_len);
#endif /* __MDDP_IF_H */
