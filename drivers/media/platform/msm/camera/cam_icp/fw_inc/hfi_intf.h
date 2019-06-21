/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _HFI_INTF_H_
#define _HFI_INTF_H_

#include <linux/types.h>

/**
 * struct hfi_mem
 * @len: length of memory
 * @kva: kernel virtual address
 * @iova: IO virtual address
 * @reserved: reserved field
 */
struct hfi_mem {
	uint64_t  len;
	uintptr_t kva;
	uint32_t  iova;
	uint32_t  reserved;
};

/**
 * struct hfi_mem_info
 * @qtbl: qtable hfi memory
 * @cmd_q: command queue hfi memory for host to firmware communication
 * @msg_q: message queue hfi memory for firmware to host communication
 * @dbg_q: debug queue hfi memory for firmware debug information
 * @sfr_buf: buffer for subsystem failure reason[SFR]
 * @sec_heap: secondary heap hfi memory for firmware
 * @qdss: qdss mapped memory for fw
 * @icp_base: icp base address
 */
struct hfi_mem_info {
	struct hfi_mem qtbl;
	struct hfi_mem cmd_q;
	struct hfi_mem msg_q;
	struct hfi_mem dbg_q;
	struct hfi_mem sfr_buf;
	struct hfi_mem sec_heap;
	struct hfi_mem shmem;
	struct hfi_mem qdss;
	void __iomem *icp_base;
};

/**
 * hfi_write_cmd() - function for hfi write
 * @cmd_ptr: pointer to command data for hfi write
 *
 * Returns success(zero)/failure(non zero)
 */
int hfi_write_cmd(void *cmd_ptr);

/**
 * hfi_read_message() - function for hfi read
 * @pmsg: buffer to place read message for hfi queue
 * @q_id: queue id
 * @words_read: total number of words read from the queue
 *              returned as output to the caller
 *
 * Returns success(zero)/failure(non zero)
 */
int hfi_read_message(uint32_t *pmsg, uint8_t q_id, uint32_t *words_read);

/**
 * hfi_init() - function initialize hfi after firmware download
 * @event_driven_mode: event mode
 * @hfi_mem: hfi memory info
 * @icp_base: icp base address
 * @debug: debug flag
 *
 * Returns success(zero)/failure(non zero)
 */
int cam_hfi_init(uint8_t event_driven_mode, struct hfi_mem_info *hfi_mem,
	void *__iomem icp_base, bool debug);

/**
 * hfi_get_hw_caps() - hardware capabilities from firmware
 * @query_caps: holds query information from hfi
 *
 * Returns success(zero)/failure(non zero)
 */
int hfi_get_hw_caps(void *query_caps);

/**
 * hfi_send_system_cmd() - send hfi system command to firmware
 * @type: type of system command
 * @data: command data
 * @size: size of command data
 */
void hfi_send_system_cmd(uint32_t type, uint64_t data, uint32_t size);

/**
 * cam_hfi_enable_cpu() - enable A5 CPU
 * @icp_base: icp base address
 */
void cam_hfi_enable_cpu(void __iomem *icp_base);

/**
 * cam_hfi_disable_cpu() - disable A5 CPU
 * @icp_base: icp base address
 */
void cam_hfi_disable_cpu(void __iomem *icp_base);

/**
 * cam_hfi_deinit() - cleanup HFI
 */
void cam_hfi_deinit(void __iomem *icp_base);
/**
 * hfi_set_debug_level() - set debug level
 * @a5_dbg_type: 1 for debug_q & 2 for qdss
 * @lvl: FW debug message level
 */
int hfi_set_debug_level(u64 a5_dbg_type, uint32_t lvl);

/**
 * hfi_set_fw_dump_level() - set firmware dump level
 * @lvl: level of firmware dump level
 */
int hfi_set_fw_dump_level(uint32_t lvl);

/**
 * hfi_enable_ipe_bps_pc() - Enable interframe pc
 * Host sends a command to firmware to enable interframe
 * power collapse for IPE and BPS hardware.
 *
 * @enable: flag to enable/disable
 * @core_info: Core information to firmware
 */
int hfi_enable_ipe_bps_pc(bool enable, uint32_t core_info);

/**
 * hfi_cmd_ubwc_config() - UBWC configuration to firmware
 * @ubwc_cfg: UBWC configuration parameters
 */
int hfi_cmd_ubwc_config(uint32_t *ubwc_cfg);

/**
 * cam_hfi_resume() - function to resume
 * @hfi_mem: hfi memory info
 * @icp_base: icp base address
 * @debug: debug flag
 *
 * Returns success(zero)/failure(non zero)
 */
int cam_hfi_resume(struct hfi_mem_info *hfi_mem,
	void __iomem *icp_base, bool debug);

/**
 * cam_hfi_queue_dump() - utility function to dump hfi queues
 */
void cam_hfi_queue_dump(void);


#endif /* _HFI_INTF_H_ */
