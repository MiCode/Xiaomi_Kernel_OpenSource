/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_APU_MDW_IMPORT_H__
#define __MTK_APU_MDW_IMPORT_H__

/* import function */
bool mdw_pwr_check(void);
int mdw_rvs_set_ctx(int type, int idx, uint8_t ctx);
int mdw_rvs_free_vlm(uint32_t ctx);
int mdw_rvs_get_vlm(uint32_t req_size, bool force,
		uint32_t *id, uint32_t *tcm_size);
int mdw_rvs_get_vlm_property(uint64_t *start, uint32_t *size);
int mdw_rvs_map_ext(uint64_t addr, uint32_t size,
	uint64_t session, uint32_t *sid);
int mdw_rvs_unmap_ext(uint64_t session, uint32_t sid);
int mdw_rvs_import_ext(uint64_t session, uint32_t sid);
int mdw_rvs_unimport_ext(uint64_t session, uint32_t sid);

int mdw_qos_cmd_start(uint64_t cmd_id, uint64_t sc_id,
		int type, int core, uint32_t boost);
int mdw_qos_cmd_end(uint64_t cmd_id, uint64_t sc_id,
		int type, int core);

#endif
