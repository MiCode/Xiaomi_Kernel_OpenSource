/* Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
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

#ifndef _AIS_VFE_CORE_H_
#define _AIS_VFE_CORE_H_

#include <linux/spinlock.h>
#include "cam_hw_intf.h"
#include "ais_vfe_hw_intf.h"
#include "ais_ife_csid_hw_intf.h"
#include "ais_vfe_bus_ver2.h"
#include "ais_vfe_top_ver2.h"

#define AIS_VFE_WORKQ_NUM_TASK             20
#define AIS_VFE_MAX_BUF                    12
#define AIS_VFE_MAX_SOF_INFO               8

enum ais_vfe_hw_irq_event {
	AIS_VFE_HW_IRQ_EVENT_SOF,
	AIS_VFE_HW_IRQ_EVENT_BUS_WR,
	AIS_VFE_HW_IRQ_EVENT_ERROR,
};

/**
 * struct ais_csid_hw_work_data- work data for csid
 * Later other fields can be added to this data
 * @evt_type   : Event type from CSID
 * @irq_status : IRQ Status register
 *
 */
struct ais_vfe_hw_work_data {
	enum ais_vfe_hw_irq_event evt_type;
	uint32_t           path;
	uint64_t           ts;
	uint32_t           bus_wr_status[3];
	uint32_t           last_addr[AIS_IFE_PATH_MAX];
	struct ais_ife_rdi_timestamps ts_hw[AIS_IFE_PATH_MAX];
};

struct ais_vfe_hw_info {
	struct ais_irq_controller_reg_info *irq_reg_info;

	uint32_t                          bus_version;
	struct ais_vfe_bus_ver2_hw_info  *bus_hw_info;

	uint32_t                          bus_rd_version;
	void                             *bus_rd_hw_info;

	uint32_t                          top_version;
	struct ais_vfe_top_ver2_hw_info  *top_hw_info;

	uint32_t                          camif_version;
	void                             *camif_reg;

	uint32_t                          camif_lite_version;
	void                             *camif_lite_reg;

	uint32_t                          testgen_version;
	void                             *testgen_reg;

	uint32_t                          num_qos_settings;
	struct cam_isp_reg_val_pair      *qos_settings;

	uint32_t                          num_ds_settings;
	struct cam_isp_reg_val_pair      *ds_settings;

	uint32_t                          num_vbif_settings;
	struct cam_isp_reg_val_pair      *vbif_settings;
};

struct ais_vfe_buffer_t {
	struct list_head           list;
	int32_t                    mem_handle;
	uint64_t                   iova_addr;
	uint32_t                   bufIdx;
	struct ais_ife_rdi_timestamps    ts_hw;
};

struct ais_sof_info_t {
	struct list_head        list;
	uint64_t                frame_cnt;
	uint64_t                sof_ts;
	uint64_t                cur_sof_hw_ts;
	uint64_t                prev_sof_hw_ts;
};

struct ais_vfe_rdi_output {
	enum ais_isp_resource_state      state;

	uint32_t                         en_cfg;
	uint32_t                         secure_mode;

	spinlock_t                       buffer_lock;
	struct ais_vfe_buffer_t          buffers[AIS_VFE_MAX_BUF];
	struct list_head                 buffer_q;
	uint8_t                          num_buffer_hw_q;
	struct list_head                 buffer_hw_q;
	struct list_head                 free_buffer_list;

	uint64_t                         frame_cnt;

	uint8_t                          num_sof_info_q;
	struct ais_sof_info_t            sof_info[AIS_VFE_MAX_SOF_INFO];
	struct list_head                 sof_info_q;
	struct list_head                 free_sof_info_list;
	struct ais_sof_info_t            last_sof_info;
};

struct ais_vfe_hw_core_info {
	struct ais_vfe_hw_info             *vfe_hw_info;
	uint32_t                            vfe_idx;

	void __iomem                       *mem_base;
	int                                 iommu_hdl;
	int                                 iommu_hdl_secure;

	uint32_t                            max_rdis;
	struct ais_vfe_rdi_output           rdi_out[AIS_IFE_PATH_MAX];

	uint32_t                            cpas_handle;
	int                                 irq_handle;
	int                                 irq_err_handle;

	uint32_t                            irq_mask0;
	uint32_t                            irq_mask1;
	uint32_t                            bus_wr_mask1;

	spinlock_t                          spin_lock;

	struct cam_req_mgr_core_workq      *workq;
	struct ais_vfe_hw_work_data         work_data[AIS_VFE_WORKQ_NUM_TASK];

	struct cam_hw_intf                 *csid_hw;
	struct ais_ife_event_data           event;
	void                               *event_cb_priv;
	ais_ife_event_cb_func               event_cb;
};

int ais_vfe_get_hw_caps(void *device_priv,
	void *get_hw_cap_args, uint32_t arg_size);
int ais_vfe_init_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size);
int ais_vfe_deinit_hw(void *hw_priv,
	void *deinit_hw_args, uint32_t arg_size);
int ais_vfe_force_reset(void *device_priv,
	void *reset_core_args, uint32_t arg_size);
int ais_vfe_reserve(void *device_priv,
	void *reserve_args, uint32_t arg_size);
int ais_vfe_release(void *device_priv,
	void *reserve_args, uint32_t arg_size);
int ais_vfe_start(void *device_priv,
	void *start_args, uint32_t arg_size);
int ais_vfe_stop(void *device_priv,
	void *stop_args, uint32_t arg_size);
int ais_vfe_read(void *device_priv,
	void *read_args, uint32_t arg_size);
int ais_vfe_write(void *device_priv,
	void *write_args, uint32_t arg_size);
int ais_vfe_process_cmd(void *device_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size);

irqreturn_t ais_vfe_irq(int irq_num, void *data);

int ais_vfe_core_init(struct ais_vfe_hw_core_info *core_info,
	struct cam_hw_soc_info             *soc_info,
	struct cam_hw_intf                 *hw_intf,
	struct ais_vfe_hw_info             *vfe_hw_info);

int ais_vfe_core_deinit(struct ais_vfe_hw_core_info *core_info,
	struct ais_vfe_hw_info             *vfe_hw_info);

#endif /* _AIS_VFE_CORE_H_ */
