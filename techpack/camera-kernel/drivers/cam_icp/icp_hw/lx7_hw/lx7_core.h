/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_LX7_CORE_H_
#define _CAM_LX7_CORE_H_

#include "cam_hw_intf.h"
#include "cam_icp_hw_intf.h"

#define UNSUPPORTED_PROC_PAS_ID   30
#define CAM_FW_PAS_ID             33

enum cam_lx7_reg_base {
	LX7_CSR_BASE,
	LX7_CIRQ_BASE,
	LX7_WD0_BASE,
	LX7_SYS_BASE,
	LX7_BASE_MAX,
};

struct cam_lx7_core_info {
	struct cam_icp_irq_cb irq_cb;
	uint32_t cpas_handle;
	bool cpas_start;
	bool use_sec_pil;
	struct {
		const struct firmware *fw_elf;
		void *fw;
		uint32_t fw_buf;
		uintptr_t fw_kva_addr;
		uint64_t fw_buf_len;
	} fw_params;
};

int cam_lx7_hw_init(void *priv, void *args, uint32_t arg_size);
int cam_lx7_hw_deinit(void *priv, void *args, uint32_t arg_size);
int cam_lx7_process_cmd(void *priv, uint32_t cmd_type,
			void *args, uint32_t arg_size);

int cam_lx7_cpas_register(struct cam_hw_intf *lx7_intf);
int cam_lx7_cpas_unregister(struct cam_hw_intf *lx7_intf);

irqreturn_t cam_lx7_handle_irq(int irq_num, void *data);

void cam_lx7_irq_raise(void *priv);
void cam_lx7_irq_enable(void *priv);
void __iomem *cam_lx7_iface_addr(void *priv);

#endif /* _CAM_LX7_CORE_H_ */
