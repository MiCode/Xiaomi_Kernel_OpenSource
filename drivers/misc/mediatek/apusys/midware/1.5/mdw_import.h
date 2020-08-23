/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_MDW_IMPORT_H__
#define __APUSYS_MDW_IMPORT_H__

//#define APUSYS_MDW_MNOC_SUPPORT
#define APUSYS_MDW_REVISER_SUPPORT
//#define APUSYS_MDW_TAG_SUPPORT
//#define APUSYS_MDW_POWER_SUPPORT

bool mdw_pwr_check(void);
int mdw_rvs_set_ctx(int type, int idx, uint8_t ctx);
int mdw_rvs_free_vlm(uint32_t ctx);
int mdw_rvs_get_vlm(uint32_t req_size, bool force,
		unsigned long *id, uint32_t *tcm_size);
int mdw_qos_cmd_start(uint64_t cmd_id, uint64_t sc_id,
		int type, int core, uint32_t boost);
int mdw_qos_cmd_end(uint64_t cmd_id, uint64_t sc_id,
		int type, int core);

#endif
