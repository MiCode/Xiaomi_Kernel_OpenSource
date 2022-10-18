/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef CRE_BUS_RD_H
#define CRE_BUS_RD_H

#include <linux/types.h>
#include <linux/completion.h>
#include <media/cam_cre.h>
#include "cre_hw.h"
#include "cam_hw_mgr_intf.h"
#include "cam_hw_intf.h"
#include "cam_soc_util.h"
#include "cam_cre_hw_mgr.h"

/**
 * struct cre_bus_rd_ctx
 *
 * @cre_acquire:    CRE acquire structure
 * @security_flag:  security flag
 * @num_in_ports:   Number of in ports
 */
struct cre_bus_rd_ctx {
	struct cam_cre_acquire_dev_info *cre_acquire;
	bool security_flag;
	uint32_t num_in_ports;
};

/**
 * struct cre_bus_in_port_to_rm
 *
 * @input_port_id:  Intput port ID
 * @num_rm:         Number of RMs
 * @rm_port_id:     RM port Id
 */
struct cre_bus_in_port_to_rm {
	uint32_t input_port_id;
	uint32_t num_rm;
	uint32_t rm_port_id[CRE_MAX_IN_RES];
};

/**
 * struct cre_bus_rd
 *
 * @cre_hw_info:    CRE hardware info
 * @in_port_to_rm:  IO port to RM mapping
 * @bus_rd_ctx:     RM context
 */
struct cre_bus_rd {
	struct cam_cre_hw *cre_hw_info;
	struct cre_bus_in_port_to_rm in_port_to_rm[CRE_MAX_IN_RES];
	struct cre_bus_rd_ctx *bus_rd_ctx[CRE_CTX_MAX];
	struct completion reset_complete;
};
#endif /* CRE_BUS_RD_H */
