/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#ifndef __APUSYS_REVISER_REMOTE_CMD_H__
#define __APUSYS_REVISER_REMOTE_CMD_H__

#define RVR_RPMSG_RAED(var, data, size, idx) do {\
			memcpy(var, data + idx, size); \
			idx = idx + size/sizeof(uint32_t); \
	} while (0)

#define RVR_RPMSG_write(var, data, size, idx) do {\
			memcpy(data + idx, var, size); \
			idx = idx + size/sizeof(uint32_t); \
	} while (0)


extern struct reviser_msg *g_reply;
extern struct reviser_msg_mgr *g_rvr_msg;

int reviser_remote_check_reply(void *reply);
int reviser_remote_print_hw_boundary(void *drvinfo);
int reviser_remote_print_hw_ctx(void *drvinfo);
int reviser_remote_print_hw_rmp_table(void *drvinfo);
int reviser_remote_print_hw_default_iova(void *drvinfo);
int reviser_remote_print_hw_exception(void *drvinfo);

int reviser_remote_print_table_tcm(void *drvinfo);
int reviser_remote_print_table_ctx(void *drvinfo);
int reviser_remote_print_table_vlm(void *drvinfo, uint32_t ctx);

int reviser_remote_set_dbg_loglevel(void *drvinfo, uint32_t level);
int reviser_remote_get_dbg_loglevel(void *drvinfo, uint32_t *level);
int reviser_remote_set_op(void *drvinfo, uint32_t *argv, uint32_t argc);

int reviser_remote_handshake(void *drvinfo, void *remote);
int reviser_remote_set_hw_default_iova(void *drvinfo, uint32_t ctx, uint64_t iova);
int reviser_remote_alloc_mem(void *drvinfo,
		uint32_t type, uint64_t input_addr, uint32_t size,
		uint64_t *addr, uint32_t *sid);
int reviser_remote_free_mem(void *drvinfo, uint32_t sid, uint32_t *type,
		uint64_t *addr, uint32_t *size);
int reviser_remote_map_mem(void *drvinfo,
		uint64_t session, uint32_t sid, uint64_t *addr);
int reviser_remote_unmap_mem(void *drvinfo,
		uint64_t session, uint32_t sid);
int reviser_remote_import_mem(void *drvinfo, uint64_t session, uint32_t sid);
int reviser_remote_unimport_mem(void *drvinfo, uint64_t session, uint32_t sid);
int reviser_remote_get_mem_info(void *drvinfo, uint32_t type);


#endif
