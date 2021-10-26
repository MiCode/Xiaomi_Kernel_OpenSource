/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mddp_if.h - Structure/API provided by mddp_if.c
 *
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MDDP_IF_H
#define __MDDP_IF_H

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
int32_t mddp_on_set_ct_value(enum mddp_app_type_e type,
		uint8_t *buf, uint32_t buf_len);
#endif /* __MDDP_IF_H */
