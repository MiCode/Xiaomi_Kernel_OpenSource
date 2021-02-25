/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef OPE_BUS_RD_H
#define OPE_BUS_RD_H

#include <linux/types.h>
#include <linux/completion.h>
#include <media/cam_ope.h>
#include "ope_hw.h"
#include "cam_hw_mgr_intf.h"
#include "cam_hw_intf.h"
#include "cam_soc_util.h"
#include "cam_context.h"
#include "cam_ope_context.h"
#include "cam_ope_hw_mgr.h"


/**
 * struct ope_bus_rd_cdm_info
 *
 * @offset: Offset
 * @addr:   Address
 * @len:    Length
 */
struct ope_bus_rd_cdm_info {
	uint32_t offset;
	uint32_t *addr;
	uint32_t len;
};

/**
 * struct ope_bus_rd_io_port_cdm_info
 *
 * @num_frames_cmds: Number of frame commands
 * @f_cdm_info:      Frame cdm info
 * @num_stripes:     Number of stripes
 * @num_s_cmd_bufs:  Number of stripe commands
 * @s_cdm_info:      Stripe cdm info
 * @go_cmd_addr:     GO command address
 * @go_cmd_len:      GO command length
 */
struct ope_bus_rd_io_port_cdm_info {
	uint32_t num_frames_cmds;
	struct ope_bus_rd_cdm_info f_cdm_info[MAX_RD_CLIENTS];
	uint32_t num_stripes;
	uint32_t num_s_cmd_bufs[OPE_MAX_STRIPES];
	struct ope_bus_rd_cdm_info s_cdm_info[OPE_MAX_STRIPES][MAX_RD_CLIENTS];
	uint32_t go_cmd_offset;
	uint32_t *go_cmd_addr;
	uint32_t go_cmd_len;
};

/**
 * struct ope_bus_rd_io_port_cdm_batch
 *
 * num_batch:   Number of batches
 * io_port_cdm: CDM IO Port Info
 */
struct ope_bus_rd_io_port_cdm_batch {
	uint32_t num_batch;
	struct ope_bus_rd_io_port_cdm_info io_port_cdm[OPE_MAX_BATCH_SIZE];
};

/**
 * struct ope_bus_rd_rm
 *
 * @rm_port_id:  RM port ID
 * @format_type: Format type
 */
struct ope_bus_rd_rm {
	uint32_t rm_port_id;
	uint32_t format_type;
};

/**
 * struct ope_bus_in_port_to_rm
 *
 * @input_port_id:  Intput port ID
 * @num_combos:     Number of combos
 * @num_rm:         Number of RMs
 * @rm_port_id:     RM port Id
 */
struct ope_bus_in_port_to_rm {
	uint32_t input_port_id;
	uint32_t num_combos;
	uint32_t num_rm[BUS_RD_COMBO_MAX];
	uint32_t rm_port_id[BUS_RD_COMBO_MAX][MAX_RD_CLIENTS];
};

/**
 * struct ope_bus_rd_io_port_info
 *
 * @pixel_pattern:      Pixel pattern
 * @input_port_id:      Port Id
 * @input_format_type:  Format type
 * @latency_buf_size:   Latency buffer size
 */
struct ope_bus_rd_io_port_info {
	uint32_t pixel_pattern[OPE_IN_RES_MAX];
	uint32_t input_port_id[OPE_IN_RES_MAX];
	uint32_t input_format_type[OPE_IN_RES_MAX];
	uint32_t latency_buf_size;
};

/**
 * struct ope_bus_rd_ctx
 *
 * @ope_acquire:       OPE acquire structure
 * @security_flag:     security flag
 * @num_in_ports:      Number of in ports
 * @io_port_info:      IO port info
 * @io_port_cdm_batch: IO port cdm info
 */
struct ope_bus_rd_ctx {
	struct ope_acquire_dev_info *ope_acquire;
	bool security_flag;
	uint32_t num_in_ports;
	struct ope_bus_rd_io_port_info io_port_info;
	struct ope_bus_rd_io_port_cdm_batch io_port_cdm_batch;
};

/**
 * struct ope_bus_rd
 *
 * @ope_hw_info:    OPE hardware info
 * @in_port_to_rm:  IO port to RM mapping
 * @bus_rd_ctx:     RM context
 */
struct ope_bus_rd {
	struct ope_hw *ope_hw_info;
	struct ope_bus_in_port_to_rm in_port_to_rm[OPE_IN_RES_MAX];
	struct ope_bus_rd_ctx *bus_rd_ctx[OPE_CTX_MAX];
	struct completion reset_complete;
};
#endif /* OPE_BUS_RD_H */

