// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#include <asm/dma-iommu.h>
#include <asm/memory.h>
#include <linux/clk/qcom.h>
#include <linux/coresight-stm.h>
#include <linux/delay.h>
#include <linux/devfreq.h>
#include <linux/hash.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/pm_qos.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/soc/qcom/llcc-qcom.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/socinfo.h>
#include <linux/soc/qcom/smem.h>
#include <soc/qcom/subsystem_restart.h>
#include <linux/dma-mapping.h>
#include <linux/reset.h>
#include "hfi_packetization.h"
#include "msm_cvp_debug.h"
#include "cvp_core_hfi.h"
#include "cvp_hfi_helper.h"
#include "cvp_hfi_io.h"
#include "msm_cvp_dsp.h"

#define FIRMWARE_SIZE			0X00A00000
#define REG_ADDR_OFFSET_BITMASK	0x000FFFFF
#define QDSS_IOVA_START 0x80001000
#define MIN_PAYLOAD_SIZE 3

const struct msm_cvp_hfi_defs cvp_hfi_defs[] = {
	{
		.size = HFI_DFS_CONFIG_CMD_SIZE,
		.type = HFI_CMD_SESSION_CVP_DFS_CONFIG,
		.buf_offset = 0,
		.buf_num = 0,
		.resp = HAL_SESSION_DFS_CONFIG_CMD_DONE,
	},
	{
		.size = HFI_DFS_FRAME_CMD_SIZE,
		.type = HFI_CMD_SESSION_CVP_DFS_FRAME,
		.buf_offset = HFI_DFS_FRAME_BUFFERS_OFFSET,
		.buf_num = HFI_DFS_BUF_NUM,
		.resp = HAL_NO_RESP,
	},
	{
		.size = HFI_DME_CONFIG_CMD_SIZE,
		.type = HFI_CMD_SESSION_CVP_DME_CONFIG,
		.buf_offset = 0,
		.buf_num = 0,
		.resp = HAL_SESSION_DME_CONFIG_CMD_DONE,
	},
	{
		.size = HFI_DME_BASIC_CONFIG_CMD_SIZE,
		.type = HFI_CMD_SESSION_CVP_DME_BASIC_CONFIG,
		.buf_offset = 0,
		.buf_num = 0,
		.resp = HAL_SESSION_DME_BASIC_CONFIG_CMD_DONE,
	},
	{
		.size = HFI_DME_FRAME_CMD_SIZE,
		.type = HFI_CMD_SESSION_CVP_DME_FRAME,
		.buf_offset = HFI_DME_FRAME_BUFFERS_OFFSET,
		.buf_num = HFI_DME_BUF_NUM,
		.resp = HAL_NO_RESP,
	},
	{
		.size = HFI_PERSIST_CMD_SIZE,
		.type = HFI_CMD_SESSION_CVP_SET_PERSIST_BUFFERS,
		.buf_offset = HFI_PERSIST_BUFFERS_OFFSET,
		.buf_num = HFI_PERSIST_BUF_NUM,
		.resp = HAL_SESSION_PERSIST_CMD_DONE,
	},
	{
		.size = HFI_DS_CMD_SIZE,
		.type = HFI_CMD_SESSION_CVP_DS,
		.buf_offset = HFI_DS_BUFFERS_OFFSET,
		.buf_num = HFI_DS_BUF_NUM,
		.resp = HAL_NO_RESP,
	},
	{
		.size = HFI_OF_CONFIG_CMD_SIZE,
		.type = HFI_CMD_SESSION_CVP_CV_TME_CONFIG,
		.buf_offset = 0,
		.buf_num = 0,
		.resp = HAL_SESSION_TME_CONFIG_CMD_DONE,
	},
	{
		.size = HFI_OF_FRAME_CMD_SIZE,
		.type = HFI_CMD_SESSION_CVP_CV_TME_FRAME,
		.buf_offset = HFI_OF_BUFFERS_OFFSET,
		.buf_num = HFI_OF_BUF_NUM,
		.resp = HAL_NO_RESP,
	},
	{
		.size = HFI_ODT_CONFIG_CMD_SIZE,
		.type = HFI_CMD_SESSION_CVP_CV_ODT_CONFIG,
		.buf_offset = 0,
		.buf_num = 0,
		.resp = HAL_SESSION_ODT_CONFIG_CMD_DONE,
	},
	{
		.size = HFI_ODT_FRAME_CMD_SIZE,
		.type = HFI_CMD_SESSION_CVP_CV_ODT_FRAME,
		.buf_offset = HFI_ODT_BUFFERS_OFFSET,
		.buf_num = HFI_ODT_BUF_NUM,
		.resp = HAL_NO_RESP,
	},
	{
		.size = HFI_OD_CONFIG_CMD_SIZE,
		.type = HFI_CMD_SESSION_CVP_CV_OD_CONFIG,
		.buf_offset = 0,
		.buf_num = 0,
		.resp = HAL_SESSION_OD_CONFIG_CMD_DONE,
	},
	{
		.size = HFI_OD_FRAME_CMD_SIZE,
		.type = HFI_CMD_SESSION_CVP_CV_OD_FRAME,
		.buf_offset = HFI_OD_BUFFERS_OFFSET,
		.buf_num = HFI_OD_BUF_NUM,
		.resp = HAL_NO_RESP,
	},
	{
		.size = HFI_NCC_CONFIG_CMD_SIZE,
		.type = HFI_CMD_SESSION_CVP_NCC_CONFIG,
		.buf_offset = 0,
		.buf_num = 0,
		.resp = HAL_SESSION_NCC_CONFIG_CMD_DONE,
	},
	{
		.size = HFI_NCC_FRAME_CMD_SIZE,
		.type = HFI_CMD_SESSION_CVP_NCC_FRAME,
		.buf_offset = HFI_NCC_BUFFERS_OFFSET,
		.buf_num = HFI_NCC_BUF_NUM,
		.resp = HAL_NO_RESP,
	},
	{
		.size = HFI_ICA_CONFIG_CMD_SIZE,
		.type = HFI_CMD_SESSION_CVP_ICA_CONFIG,
		.buf_offset = 0,
		.buf_num = 0,
		.resp = HAL_SESSION_ICA_CONFIG_CMD_DONE,
	},
	{
		.size = HFI_ICA_FRAME_CMD_SIZE,
		.type = HFI_CMD_SESSION_CVP_ICA_FRAME,
		.buf_offset = HFI_ICA_BUFFERS_OFFSET,
		.buf_num = HFI_ICA_BUF_NUM,
		.resp = HAL_NO_RESP,
	},
	{
		.size = HFI_HCD_CONFIG_CMD_SIZE,
		.type = HFI_CMD_SESSION_CVP_HCD_CONFIG,
		.buf_offset = 0,
		.buf_num = 0,
		.resp = HAL_SESSION_HCD_CONFIG_CMD_DONE,
	},
	{
		.size = HFI_HCD_FRAME_CMD_SIZE,
		.type = HFI_CMD_SESSION_CVP_HCD_FRAME,
		.buf_offset = HFI_HCD_BUFFERS_OFFSET,
		.buf_num = HFI_HCD_BUF_NUM,
		.resp = HAL_NO_RESP,
	},
	{
		.size = HFI_DCM_CONFIG_CMD_SIZE,
		.type = HFI_CMD_SESSION_CVP_DC_CONFIG,
		.buf_offset = 0,
		.buf_num = 0,
		.resp = HAL_SESSION_DC_CONFIG_CMD_DONE,
	},
	{
		.size = HFI_DCM_FRAME_CMD_SIZE,
		.type = HFI_CMD_SESSION_CVP_DC_FRAME,
		.buf_offset = HFI_DCM_BUFFERS_OFFSET,
		.buf_num = HFI_DCM_BUF_NUM,
		.resp = HAL_NO_RESP,
	},
	{
		.size = HFI_DCM_CONFIG_CMD_SIZE,
		.type = HFI_CMD_SESSION_CVP_DCM_CONFIG,
		.buf_offset = 0,
		.buf_num = 0,
		.resp = HAL_SESSION_DCM_CONFIG_CMD_DONE,
	},
	{
		.size = HFI_DCM_FRAME_CMD_SIZE,
		.type = HFI_CMD_SESSION_CVP_DCM_FRAME,
		.buf_offset = HFI_DCM_BUFFERS_OFFSET,
		.buf_num = HFI_DCM_BUF_NUM,
		.resp = HAL_NO_RESP,
	},
	{
		.size = HFI_PYS_HCD_CONFIG_CMD_SIZE,
		.type = HFI_CMD_SESSION_CVP_PYS_HCD_CONFIG,
		.buf_offset = 0,
		.buf_num = 0,
		.resp = HAL_SESSION_PYS_HCD_CONFIG_CMD_DONE,
	},
	{
		.size = HFI_PYS_HCD_FRAME_CMD_SIZE,
		.type = HFI_CMD_SESSION_CVP_PYS_HCD_FRAME,
		.buf_offset = HFI_PYS_HCD_BUFFERS_OFFSET,
		.buf_num = HFI_PYS_HCD_BUF_NUM,
		.resp = HAL_NO_RESP,
	},
	{
		.size = 0xFFFFFFFF,
		.type = HFI_CMD_SESSION_CVP_SET_MODEL_BUFFERS,
		.buf_offset = 0,
		.buf_num = 0,
		.resp = HAL_SESSION_MODEL_BUF_CMD_DONE,
	},
	{
		.size = 0xFFFFFFFF,
		.type = HFI_CMD_SESSION_CVP_FD_CONFIG,
		.buf_offset = 0,
		.buf_num = 0,
		.resp = HAL_SESSION_FD_CONFIG_CMD_DONE,
	},
	{
		.size = 0xFFFFFFFF,
		.type = HFI_CMD_SESSION_CVP_FD_FRAME,
		.buf_offset = 0,
		.buf_num = 0,
		.resp = HAL_NO_RESP,
	},

};

struct cvp_tzbsp_memprot {
	u32 cp_start;
	u32 cp_size;
	u32 cp_nonpixel_start;
	u32 cp_nonpixel_size;
};

#define TZBSP_PIL_SET_STATE 0xA
#define TZBSP_CVP_PAS_ID    26

/* Poll interval in uS */
#define POLL_INTERVAL_US 50

enum tzbsp_subsys_state {
	TZ_SUBSYS_STATE_SUSPEND = 0,
	TZ_SUBSYS_STATE_RESUME = 1,
	TZ_SUBSYS_STATE_RESTORE_THRESHOLD = 2,
};

const struct msm_cvp_gov_data CVP_DEFAULT_BUS_VOTE = {
	.data = NULL,
	.data_count = 0,
};

const int cvp_max_packets = 32;

static void iris_hfi_pm_handler(struct work_struct *work);
static DECLARE_DELAYED_WORK(iris_hfi_pm_work, iris_hfi_pm_handler);
static void dsp_init_work_handler(struct work_struct *work);
static inline int __resume(struct iris_hfi_device *device);
static inline int __suspend(struct iris_hfi_device *device);
static int __disable_regulators(struct iris_hfi_device *device);
static int __enable_regulators(struct iris_hfi_device *device);
static inline int __prepare_enable_clks(struct iris_hfi_device *device);
static inline void __disable_unprepare_clks(struct iris_hfi_device *device);
static void __flush_debug_queue(struct iris_hfi_device *device, u8 *packet);
static int __initialize_packetization(struct iris_hfi_device *device);
static struct cvp_hal_session *__get_session(struct iris_hfi_device *device,
		u32 session_id);
static bool __is_session_valid(struct iris_hfi_device *device,
		struct cvp_hal_session *session, const char *func);
static int __set_clocks(struct iris_hfi_device *device, u32 freq);
static int __iface_cmdq_write(struct iris_hfi_device *device,
					void *pkt);
static int __load_fw(struct iris_hfi_device *device);
static void __unload_fw(struct iris_hfi_device *device);
static int __tzbsp_set_cvp_state(enum tzbsp_subsys_state state);
static int __enable_subcaches(struct iris_hfi_device *device);
static int __set_subcaches(struct iris_hfi_device *device);
static int __release_subcaches(struct iris_hfi_device *device);
static int __disable_subcaches(struct iris_hfi_device *device);
static int __power_collapse(struct iris_hfi_device *device, bool force);
static int iris_hfi_noc_error_info(void *dev);

static void interrupt_init_iris2(struct iris_hfi_device *device);
static void setup_dsp_uc_memmap_vpu5(struct iris_hfi_device *device);
static void clock_config_on_enable_vpu5(struct iris_hfi_device *device);
static int reset_ahb2axi_bridge(struct iris_hfi_device *device);
static void power_off_iris2(struct iris_hfi_device *device);

static int __set_ubwc_config(struct iris_hfi_device *device);
static void __noc_error_info_iris2(struct iris_hfi_device *device);

static struct iris_hfi_vpu_ops iris2_ops = {
	.interrupt_init = interrupt_init_iris2,
	.setup_dsp_uc_memmap = setup_dsp_uc_memmap_vpu5,
	.clock_config_on_enable = clock_config_on_enable_vpu5,
	.reset_ahb2axi_bridge = reset_ahb2axi_bridge,
	.power_off = power_off_iris2,
	.noc_error_info = __noc_error_info_iris2,
};

/**
 * Utility function to enforce some of our assumptions.  Spam calls to this
 * in hotspots in code to double check some of the assumptions that we hold.
 */
static inline void __strict_check(struct iris_hfi_device *device)
{
	msm_cvp_res_handle_fatal_hw_error(device->res,
		!mutex_is_locked(&device->lock));
}

static inline void __set_state(struct iris_hfi_device *device,
		enum iris_hfi_state state)
{
	device->state = state;
}

static inline bool __core_in_valid_state(struct iris_hfi_device *device)
{
	return device->state != IRIS_STATE_DEINIT;
}

static inline bool is_sys_cache_present(struct iris_hfi_device *device)
{
	return device->res->sys_cache_present;
}

#define ROW_SIZE 32

int get_pkt_index(struct cvp_hal_session_cmd_pkt *hdr)
{
	int i, pkt_num = ARRAY_SIZE(cvp_hfi_defs);

	for (i = 0; i < pkt_num; i++)
		if (cvp_hfi_defs[i].type == hdr->packet_type)
			return i;

	return -EINVAL;
}

int set_feature_bitmask(int pkt_idx, unsigned long *bitmask)
{
	if (!bitmask) {
		dprintk(CVP_ERR, "%s: invalid bitmask\n", __func__);
		return -EINVAL;
	}

	if (cvp_hfi_defs[pkt_idx].type == HFI_CMD_SESSION_CVP_DME_FRAME) {
		set_bit(DME_BIT_OFFSET, bitmask);
		return 0;
	}

	if (cvp_hfi_defs[pkt_idx].type == HFI_CMD_SESSION_CVP_ICA_FRAME) {
		set_bit(ICA_BIT_OFFSET, bitmask);
		return 0;
	}

	if (cvp_hfi_defs[pkt_idx].type == HFI_CMD_SESSION_CVP_FD_FRAME) {
		set_bit(FD_BIT_OFFSET, bitmask);
		return 0;
	}

	dprintk(CVP_ERR, "%s: invalid pkt_idx %d\n", __func__, pkt_idx);
	return -EINVAL;
}

int get_hfi_version(void)
{
	struct msm_cvp_core *core;
	struct iris_hfi_device *hfi;

	core = list_first_entry(&cvp_driver->cores, struct msm_cvp_core, list);
	hfi = (struct iris_hfi_device *)core->device->hfi_device_data;

	return hfi->version;
}

unsigned int get_msg_size(void)
{
	unsigned int ver;

	ver = get_hfi_version();
	ver = (ver & HFI_VERSION_MINOR_MASK) >> HFI_VERSION_MINOR_SHIFT;

	if (ver < 1)
		return sizeof(struct cvp_hfi_msg_session_hdr_d);

	return sizeof(struct cvp_hfi_msg_session_hdr);
}

unsigned int get_msg_session_id(void *msg)
{
	unsigned int ver;
	struct cvp_hfi_msg_session_hdr *hdr =
		(struct cvp_hfi_msg_session_hdr *)msg;

	ver = get_hfi_version();
	ver = (ver & HFI_VERSION_MINOR_MASK) >> HFI_VERSION_MINOR_SHIFT;

	if (ver < 1) {
		struct cvp_hfi_msg_session_hdr_d *old_hdr =
			(struct cvp_hfi_msg_session_hdr_d *)msg;
		return old_hdr->session_id;
	}
	return hdr->session_id;
}

unsigned int get_msg_errorcode(void *msg)
{
	unsigned int ver;
	struct cvp_hfi_msg_session_hdr *hdr =
		(struct cvp_hfi_msg_session_hdr *)msg;

	ver = get_hfi_version();
	ver = (ver & HFI_VERSION_MINOR_MASK) >> HFI_VERSION_MINOR_SHIFT;

	if (ver < 1) {
		struct cvp_hfi_msg_session_hdr_d *old_hdr =
			(struct cvp_hfi_msg_session_hdr_d *)msg;
		return old_hdr->error_type;
	}
	return hdr->error_type;
}

int get_msg_opconfigs(void *msg, unsigned int *session_id,
		unsigned int *error_type, unsigned int *config_id)
{
	unsigned int ver;
	struct cvp_hfi_msg_session_op_cfg_packet *cfg =
		(struct cvp_hfi_msg_session_op_cfg_packet *)msg;

	ver = get_hfi_version();
	ver = (ver & HFI_VERSION_MINOR_MASK) >> HFI_VERSION_MINOR_SHIFT;

	if (ver < 1) {
		struct cvp_hfi_msg_session_op_cfg_packet_d *old_cfg
			= (struct cvp_hfi_msg_session_op_cfg_packet_d *)msg;
		*session_id = old_cfg->session_id;
		*error_type = old_cfg->error_type;
		*config_id = old_cfg->op_conf_id;
		return 0;
	}
	*session_id = cfg->session_id;
	*error_type = cfg->error_type;
	*config_id = cfg->op_conf_id;
	return 0;
}

int get_signal_from_pkt_type(unsigned int type)
{
	int i, pkt_num = ARRAY_SIZE(cvp_hfi_defs);

	for (i = 0; i < pkt_num; i++)
		if (cvp_hfi_defs[i].type == type)
			return cvp_hfi_defs[i].resp;

	return -EINVAL;
}

static void __dump_packet(u8 *packet, enum cvp_msg_prio log_level)
{
	u32 c = 0, packet_size = *(u32 *)packet;
	/*
	 * row must contain enough for 0xdeadbaad * 8 to be converted into
	 * "de ad ba ab " * 8 + '\0'
	 */
	char row[3 * ROW_SIZE];

	for (c = 0; c * ROW_SIZE < packet_size; ++c) {
		int bytes_to_read = ((c + 1) * ROW_SIZE > packet_size) ?
			packet_size % ROW_SIZE : ROW_SIZE;
		hex_dump_to_buffer(packet + c * ROW_SIZE, bytes_to_read,
				ROW_SIZE, 4, row, sizeof(row), false);
		dprintk(log_level, "%s\n", row);
	}
}

static int __dsp_send_hfi_queue(struct iris_hfi_device *device)
{
	int rc;

	if (!device->dsp_iface_q_table.mem_data.dma_handle) {
		dprintk(CVP_ERR, "%s: invalid dsm_handle\n", __func__);
		return -EINVAL;
	}

	if (device->dsp_flags & DSP_INIT) {
		dprintk(CVP_DBG, "%s: dsp already inited\n", __func__);
		return 0;
	}

	dprintk(CVP_DBG, "%s: hfi queue %#llx size %d\n",
		__func__, device->dsp_iface_q_table.mem_data.dma_handle,
		device->dsp_iface_q_table.mem_data.size);
	rc = cvp_dsp_send_cmd_hfi_queue(
		(phys_addr_t *)device->dsp_iface_q_table.mem_data.dma_handle,
		device->dsp_iface_q_table.mem_data.size, device);
	if (rc) {
		dprintk(CVP_ERR, "%s: dsp hfi queue init failed\n", __func__);
		return rc;
	}

	device->dsp_flags |= DSP_INIT;
	dprintk(CVP_DBG, "%s: dsp inited\n", __func__);
	return rc;
}

static int __dsp_suspend(struct iris_hfi_device *device, bool force, u32 flags)
{
	int rc;
	struct cvp_hal_session *temp;

	if (!(device->dsp_flags & DSP_INIT))
		return 0;

	if (device->dsp_flags & DSP_SUSPEND)
		return 0;

	list_for_each_entry(temp, &device->sess_head, list) {
		/* if forceful suspend, don't check session pause info */
		if (force)
			continue;

		/* don't suspend if cvp session is not paused */
		if (!(temp->flags & SESSION_PAUSE)) {
			dprintk(CVP_DBG,
				"%s: cvp session %x not paused\n",
				__func__, hash32_ptr(temp));
			return -EBUSY;
		}
	}

	dprintk(CVP_DBG, "%s: suspend dsp\n", __func__);
	rc = cvp_dsp_suspend(flags);
	if (rc) {
		dprintk(CVP_ERR, "%s: dsp suspend failed with error %d\n",
			__func__, rc);
		return -EINVAL;
	}

	device->dsp_flags |= DSP_SUSPEND;
	dprintk(CVP_DBG, "%s: dsp suspended\n", __func__);
	return 0;
}

static int __dsp_resume(struct iris_hfi_device *device, u32 flags)
{
	int rc;

	if (!(device->dsp_flags & DSP_SUSPEND)) {
		dprintk(CVP_DBG, "%s: dsp not suspended\n", __func__);
		return 0;
	}

	dprintk(CVP_DBG, "%s: resume dsp\n", __func__);
	rc = cvp_dsp_resume(flags);
	if (rc) {
		dprintk(CVP_ERR,
			"%s: dsp resume failed with error %d\n",
			__func__, rc);
		return rc;
	}

	device->dsp_flags &= ~DSP_SUSPEND;
	dprintk(CVP_DBG, "%s: dsp resumed\n", __func__);
	return rc;
}

static int __dsp_shutdown(struct iris_hfi_device *device, u32 flags)
{
	int rc;

	cvp_dsp_set_cvp_ssr();

	if (!(device->dsp_flags & DSP_INIT)) {
		dprintk(CVP_WARN, "%s: dsp not inited\n", __func__);
		return 0;
	}

	dprintk(CVP_DBG, "%s: shutdown dsp\n", __func__);
	rc = cvp_dsp_shutdown(flags);
	if (rc) {
		dprintk(CVP_ERR,
			"%s: dsp shutdown failed with error %d\n",
			__func__, rc);
		WARN_ON(1);
	}

	device->dsp_flags &= ~DSP_INIT;
	dprintk(CVP_DBG, "%s: dsp shutdown successful\n", __func__);
	return rc;
}

static int __acquire_regulator(struct regulator_info *rinfo,
				struct iris_hfi_device *device)
{
	int rc = 0;

	if (rinfo->has_hw_power_collapse) {
		rc = regulator_set_mode(rinfo->regulator,
				REGULATOR_MODE_NORMAL);
		if (rc) {
			/*
			 * This is somewhat fatal, but nothing we can do
			 * about it. We can't disable the regulator w/o
			 * getting it back under s/w control
			 */
			dprintk(CVP_WARN,
				"Failed to acquire regulator control: %s\n",
					rinfo->name);
		} else {

			dprintk(CVP_DBG,
					"Acquire regulator control from HW: %s\n",
					rinfo->name);

		}
	}

	if (!regulator_is_enabled(rinfo->regulator)) {
		dprintk(CVP_WARN, "Regulator is not enabled %s\n",
			rinfo->name);
		msm_cvp_res_handle_fatal_hw_error(device->res, true);
	}

	return rc;
}

static int __hand_off_regulator(struct regulator_info *rinfo)
{
	int rc = 0;

	if (rinfo->has_hw_power_collapse) {
		rc = regulator_set_mode(rinfo->regulator,
				REGULATOR_MODE_FAST);
		if (rc) {
			dprintk(CVP_WARN,
				"Failed to hand off regulator control: %s\n",
					rinfo->name);
		} else {
			dprintk(CVP_DBG,
					"Hand off regulator control to HW: %s\n",
					rinfo->name);
		}
	}

	return rc;
}

static int __hand_off_regulators(struct iris_hfi_device *device)
{
	struct regulator_info *rinfo;
	int rc = 0, c = 0;

	iris_hfi_for_each_regulator(device, rinfo) {
		rc = __hand_off_regulator(rinfo);
		/*
		 * If one regulator hand off failed, driver should take
		 * the control for other regulators back.
		 */
		if (rc)
			goto err_reg_handoff_failed;
		c++;
	}

	return rc;
err_reg_handoff_failed:
	iris_hfi_for_each_regulator_reverse_continue(device, rinfo, c)
		__acquire_regulator(rinfo, device);

	return rc;
}

static int __write_queue(struct cvp_iface_q_info *qinfo, u8 *packet,
		bool *rx_req_is_set)
{
	struct cvp_hfi_queue_header *queue;
	u32 packet_size_in_words, new_write_idx;
	u32 empty_space, read_idx, write_idx;
	u32 *write_ptr;

	if (!qinfo || !packet) {
		dprintk(CVP_ERR, "Invalid Params\n");
		return -EINVAL;
	} else if (!qinfo->q_array.align_virtual_addr) {
		dprintk(CVP_WARN, "Queues have already been freed\n");
		return -EINVAL;
	}

	queue = (struct cvp_hfi_queue_header *) qinfo->q_hdr;
	if (!queue) {
		dprintk(CVP_ERR, "queue not present\n");
		return -ENOENT;
	}

	if (msm_cvp_debug & CVP_PKT) {
		dprintk(CVP_PKT, "%s: %pK\n", __func__, qinfo);
		__dump_packet(packet, CVP_PKT);
	}

	packet_size_in_words = (*(u32 *)packet) >> 2;
	if (!packet_size_in_words || packet_size_in_words >
		qinfo->q_array.mem_size>>2) {
		dprintk(CVP_ERR, "Invalid packet size\n");
		return -ENODATA;
	}

	spin_lock(&qinfo->hfi_lock);
	read_idx = queue->qhdr_read_idx;
	write_idx = queue->qhdr_write_idx;

	empty_space = (write_idx >= read_idx) ?
		((qinfo->q_array.mem_size>>2) - (write_idx - read_idx)) :
		(read_idx - write_idx);
	if (empty_space <= packet_size_in_words) {
		queue->qhdr_tx_req =  1;
		spin_unlock(&qinfo->hfi_lock);
		dprintk(CVP_ERR, "Insufficient size (%d) to write (%d)\n",
					  empty_space, packet_size_in_words);
		return -ENOTEMPTY;
	}

	queue->qhdr_tx_req =  0;

	new_write_idx = write_idx + packet_size_in_words;
	write_ptr = (u32 *)((qinfo->q_array.align_virtual_addr) +
		(write_idx << 2));
	if (write_ptr < (u32 *)qinfo->q_array.align_virtual_addr ||
		write_ptr > (u32 *)(qinfo->q_array.align_virtual_addr +
		qinfo->q_array.mem_size)) {
		spin_unlock(&qinfo->hfi_lock);
		dprintk(CVP_ERR, "Invalid write index\n");
		return -ENODATA;
	}

	if (new_write_idx < (qinfo->q_array.mem_size >> 2)) {
		memcpy(write_ptr, packet, packet_size_in_words << 2);
	} else {
		new_write_idx -= qinfo->q_array.mem_size >> 2;
		memcpy(write_ptr, packet, (packet_size_in_words -
			new_write_idx) << 2);
		memcpy((void *)qinfo->q_array.align_virtual_addr,
			packet + ((packet_size_in_words - new_write_idx) << 2),
			new_write_idx  << 2);
	}

	/*
	 * Memory barrier to make sure packet is written before updating the
	 * write index
	 */
	mb();
	queue->qhdr_write_idx = new_write_idx;
	if (rx_req_is_set)
		*rx_req_is_set = queue->qhdr_rx_req == 1;
	/*
	 * Memory barrier to make sure write index is updated before an
	 * interrupt is raised.
	 */
	mb();
	spin_unlock(&qinfo->hfi_lock);
	return 0;
}

static int __read_queue(struct cvp_iface_q_info *qinfo, u8 *packet,
		u32 *pb_tx_req_is_set)
{
	struct cvp_hfi_queue_header *queue;
	u32 packet_size_in_words, new_read_idx;
	u32 *read_ptr;
	u32 receive_request = 0;
	u32 read_idx, write_idx;
		int rc = 0;

	if (!qinfo || !packet || !pb_tx_req_is_set) {
		dprintk(CVP_ERR, "Invalid Params\n");
		return -EINVAL;
	} else if (!qinfo->q_array.align_virtual_addr) {
		dprintk(CVP_WARN, "Queues have already been freed\n");
		return -EINVAL;
	}

	/*
	 * Memory barrier to make sure data is valid before
	 *reading it
	 */
	mb();
	queue = (struct cvp_hfi_queue_header *) qinfo->q_hdr;

	if (!queue) {
		dprintk(CVP_ERR, "Queue memory is not allocated\n");
		return -ENOMEM;
	}

	/*
	 * Do not set receive request for debug queue, if set,
	 * Iris generates interrupt for debug messages even
	 * when there is no response message available.
	 * In general debug queue will not become full as it
	 * is being emptied out for every interrupt from Iris.
	 * Iris will anyway generates interrupt if it is full.
	 */
	spin_lock(&qinfo->hfi_lock);
	if (queue->qhdr_type & HFI_Q_ID_CTRL_TO_HOST_MSG_Q)
		receive_request = 1;

	read_idx = queue->qhdr_read_idx;
	write_idx = queue->qhdr_write_idx;

	if (read_idx == write_idx) {
		queue->qhdr_rx_req = receive_request;
		/*
		 * mb() to ensure qhdr is updated in main memory
		 * so that iris reads the updated header values
		 */
		mb();
		*pb_tx_req_is_set = 0;
		if (write_idx != queue->qhdr_write_idx) {
			queue->qhdr_rx_req = 0;
		} else {
			spin_unlock(&qinfo->hfi_lock);
			dprintk(CVP_DBG,
				"%s queue is empty, rx_req = %u, tx_req = %u, read_idx = %u\n",
				receive_request ? "message" : "debug",
				queue->qhdr_rx_req, queue->qhdr_tx_req,
				queue->qhdr_read_idx);
			return -ENODATA;
		}
	}

	read_ptr = (u32 *)((qinfo->q_array.align_virtual_addr) +
				(read_idx << 2));
	if (read_ptr < (u32 *)qinfo->q_array.align_virtual_addr ||
		read_ptr > (u32 *)(qinfo->q_array.align_virtual_addr +
		qinfo->q_array.mem_size - sizeof(*read_ptr))) {
		spin_unlock(&qinfo->hfi_lock);
		dprintk(CVP_ERR, "Invalid read index\n");
		return -ENODATA;
	}

	packet_size_in_words = (*read_ptr) >> 2;
	if (!packet_size_in_words) {
		spin_unlock(&qinfo->hfi_lock);
		dprintk(CVP_ERR, "Zero packet size\n");
		return -ENODATA;
	}

	new_read_idx = read_idx + packet_size_in_words;
	if (((packet_size_in_words << 2) <= CVP_IFACEQ_VAR_HUGE_PKT_SIZE)
			&& read_idx <= (qinfo->q_array.mem_size >> 2)) {
		if (new_read_idx < (qinfo->q_array.mem_size >> 2)) {
			memcpy(packet, read_ptr,
					packet_size_in_words << 2);
		} else {
			new_read_idx -= (qinfo->q_array.mem_size >> 2);
			memcpy(packet, read_ptr,
			(packet_size_in_words - new_read_idx) << 2);
			memcpy(packet + ((packet_size_in_words -
					new_read_idx) << 2),
					(u8 *)qinfo->q_array.align_virtual_addr,
					new_read_idx << 2);
		}
	} else {
		dprintk(CVP_WARN,
			"BAD packet received, read_idx: %#x, pkt_size: %d\n",
			read_idx, packet_size_in_words << 2);
		dprintk(CVP_WARN, "Dropping this packet\n");
		new_read_idx = write_idx;
		rc = -ENODATA;
	}

	if (new_read_idx != queue->qhdr_write_idx)
		queue->qhdr_rx_req = 0;
	else
		queue->qhdr_rx_req = receive_request;
	queue->qhdr_read_idx = new_read_idx;
	/*
	 * mb() to ensure qhdr is updated in main memory
	 * so that iris reads the updated header values
	 */
	mb();

	*pb_tx_req_is_set = (queue->qhdr_tx_req == 1) ? 1 : 0;

	spin_unlock(&qinfo->hfi_lock);

	if ((msm_cvp_debug & CVP_PKT) &&
		!(queue->qhdr_type & HFI_Q_ID_CTRL_TO_HOST_DEBUG_Q)) {
		dprintk(CVP_PKT, "%s: %pK\n", __func__, qinfo);
		__dump_packet(packet, CVP_PKT);
	}

	return rc;
}

static int __smem_alloc(struct iris_hfi_device *dev, struct cvp_mem_addr *mem,
			u32 size, u32 align, u32 flags)
{
	struct msm_cvp_smem *alloc = &mem->mem_data;
	int rc = 0;

	if (!dev || !mem || !size) {
		dprintk(CVP_ERR, "Invalid Params\n");
		return -EINVAL;
	}

	dprintk(CVP_INFO, "start to alloc size: %d, flags: %d\n", size, flags);
	rc = msm_cvp_smem_alloc(
		size, align, flags, 1, (void *)dev->res,
		MSM_CVP_UNKNOWN, alloc);
	if (rc) {
		dprintk(CVP_ERR, "Alloc failed\n");
		rc = -ENOMEM;
		goto fail_smem_alloc;
	}

	dprintk(CVP_DBG, "%s: ptr = %pK, size = %d\n", __func__,
			alloc->kvaddr, size);

	mem->mem_size = alloc->size;
	mem->align_virtual_addr = alloc->kvaddr;
	mem->align_device_addr = alloc->device_addr;

	return rc;
fail_smem_alloc:
	return rc;
}

static void __smem_free(struct iris_hfi_device *dev, struct msm_cvp_smem *mem)
{
	if (!dev || !mem) {
		dprintk(CVP_ERR, "invalid param %pK %pK\n", dev, mem);
		return;
	}

	msm_cvp_smem_free(mem);
}

static void __write_register(struct iris_hfi_device *device,
		u32 reg, u32 value)
{
	u32 hwiosymaddr = reg;
	u8 *base_addr;

	if (!device) {
		dprintk(CVP_ERR, "Invalid params: %pK\n", device);
		return;
	}

	__strict_check(device);

	if (!device->power_enabled) {
		dprintk(CVP_WARN,
			"HFI Write register failed : Power is OFF\n");
		msm_cvp_res_handle_fatal_hw_error(device->res, true);
		return;
	}

	base_addr = device->cvp_hal_data->register_base;
	dprintk(CVP_DBG, "Base addr: %pK, written to: %#x, Value: %#x...\n",
		base_addr, hwiosymaddr, value);
	base_addr += hwiosymaddr;
	writel_relaxed(value, base_addr);

	/*
	 * Memory barrier to make sure value is written into the register.
	 */
	wmb();
}

static int __read_register(struct iris_hfi_device *device, u32 reg)
{
	int rc = 0;
	u8 *base_addr;

	if (!device) {
		dprintk(CVP_ERR, "Invalid params: %pK\n", device);
		return -EINVAL;
	}

	__strict_check(device);

	if (!device->power_enabled) {
		dprintk(CVP_WARN,
			"HFI Read register failed : Power is OFF\n");
		msm_cvp_res_handle_fatal_hw_error(device->res, true);
		return -EINVAL;
	}

	base_addr = device->cvp_hal_data->register_base;

	rc = readl_relaxed(base_addr + reg);
	/*
	 * Memory barrier to make sure value is read correctly from the
	 * register.
	 */
	rmb();
	dprintk(CVP_DBG, "Base addr: %pK, read from: %#x, value: %#x...\n",
		base_addr, reg, rc);

	return rc;
}

static void __set_registers(struct iris_hfi_device *device)
{
	struct reg_set *reg_set;
	int i;

	if (!device->res) {
		dprintk(CVP_ERR,
			"device resources null, cannot set registers\n");
		return;
	}

	reg_set = &device->res->reg_set;
	for (i = 0; i < reg_set->count; i++) {
		__write_register(device, reg_set->reg_tbl[i].reg,
				reg_set->reg_tbl[i].value);
		dprintk(CVP_DBG, "write_reg offset=%x, val=%x\n",
					reg_set->reg_tbl[i].reg,
					reg_set->reg_tbl[i].value);
	}
}

/*
 * The existence of this function is a hack for 8996 (or certain Iris versions)
 * to overcome a hardware bug.  Whenever the GDSCs momentarily power collapse
 * (after calling __hand_off_regulators()), the values of the threshold
 * registers (typically programmed by TZ) are incorrectly reset.  As a result
 * reprogram these registers at certain agreed upon points.
 */
static void __set_threshold_registers(struct iris_hfi_device *device)
{
	u32 version = __read_register(device, CVP_WRAPPER_HW_VERSION);

	version &= ~GENMASK(15, 0);
	if (version != (0x3 << 28 | 0x43 << 16))
		return;

	if (__tzbsp_set_cvp_state(TZ_SUBSYS_STATE_RESTORE_THRESHOLD))
		dprintk(CVP_ERR, "Failed to restore threshold values\n");
}

#ifdef USE_DEVFREQ_SCALE_BUS
static int __devfreq_target(struct device *devfreq_dev,
		unsigned long *freq, u32 flags)
{
	int rc = 0;
	uint64_t ab = 0;
	struct bus_info *bus = NULL, *temp = NULL;
	struct iris_hfi_device *device = dev_get_drvdata(devfreq_dev);

	iris_hfi_for_each_bus(device, temp) {
		if (temp->dev == devfreq_dev) {
			bus = temp;
			break;
		}
	}

	if (!bus) {
		rc = -EBADHANDLE;
		goto err_unknown_device;
	}

	/*
	 * Clamp for all non zero frequencies. This clamp is necessary to stop
	 * devfreq driver from spamming - Couldn't update frequency - logs, if
	 * the scaled ab value is not part of the frequency table.
	 */
	if (*freq)
		*freq = clamp_t(typeof(*freq), *freq, bus->range[0],
				bus->range[1]);

	/* we expect governors to provide values in kBps form, convert to Bps */
	ab = *freq * 1000;
	rc = msm_bus_scale_update_bw(bus->client, ab, 0);
		dprintk(CVP_ERR, "Failed voting bus %s to ab %llu\n: %d",
				bus->name, ab, rc);
		goto err_unknown_device;
	}

	dprintk(CVP_PROF, "Voting bus %s to ab %llu\n", bus->name, ab);

	return 0;
err_unknown_device:
	return rc;
}

static int __devfreq_get_status(struct device *devfreq_dev,
		struct devfreq_dev_status *stat)
{
	int rc = 0;
	struct bus_info *bus = NULL, *temp = NULL;
	struct iris_hfi_device *device = dev_get_drvdata(devfreq_dev);

	iris_hfi_for_each_bus(device, temp) {
		if (temp->dev == devfreq_dev) {
			bus = temp;
			break;
		}
	}

	if (!bus) {
		rc = -EBADHANDLE;
		goto err_unknown_device;
	}

	*stat = (struct devfreq_dev_status) {
		.private_data = &device->bus_vote,
		/*
		 * Put in dummy place holder values for upstream govs, our
		 * custom gov only needs .private_data.  We should fill this in
		 * properly if we can actually measure busy_time accurately
		 * (which we can't at the moment)
		 */
		.total_time = 1,
		.busy_time = 1,
		.current_frequency = 0,
	};

err_unknown_device:
	return rc;
}
#endif

static int __unvote_buses(struct iris_hfi_device *device)
{
	int rc = 0;
	struct bus_info *bus = NULL;

	kfree(device->bus_vote.data);
	device->bus_vote.data = NULL;
	device->bus_vote.data_count = 0;

	iris_hfi_for_each_bus(device, bus) {
#ifdef USE_DEVFREQ_SCALE_BUS
		unsigned long zero = 0;

		if (!bus->is_prfm_gov_used)
			rc = devfreq_suspend_device(bus->devfreq);
		else
			rc = __devfreq_target(bus->dev, &zero, 0);
#else
		rc = msm_bus_scale_update_bw(bus->client, 0, 0);
#endif

		if (rc) {
			dprintk(CVP_ERR,
			"%s: Failed unvoting bus\n", __func__);
			goto err_unknown_device;
		}
	}

err_unknown_device:
	return rc;
}

static int __vote_buses(struct iris_hfi_device *device,
		struct cvp_bus_vote_data *data, int num_data)
{
	int rc = 0;
	struct bus_info *bus = NULL;
	struct cvp_bus_vote_data *new_data = NULL;

	if (!num_data) {
		dprintk(CVP_DBG, "No vote data available\n");
		goto no_data_count;
	} else if (!data) {
		dprintk(CVP_ERR, "Invalid voting data\n");
		return -EINVAL;
	}

	new_data = kmemdup(data, num_data * sizeof(*new_data), GFP_KERNEL);
	if (!new_data) {
		dprintk(CVP_ERR, "Can't alloc memory to cache bus votes\n");
		rc = -ENOMEM;
		goto err_no_mem;
	}

no_data_count:
	kfree(device->bus_vote.data);
	device->bus_vote.data = new_data;
	device->bus_vote.data_count = num_data;

	iris_hfi_for_each_bus(device, bus) {
		if (bus) {
#ifdef USE_DEVFREQ_SCALE_BUS
			if (bus->devfreq) {
				if (!bus->is_prfm_gov_used) {
					rc = devfreq_resume_device(
						bus->devfreq);
					if (rc)
						goto err_no_mem;
				} else {
					bus->devfreq->nb.notifier_call(
						&bus->devfreq->nb, 0, NULL);
				}
			}
#else
			rc = msm_bus_scale_update_bw(bus->client,
				bus->range[1], 0);
			if (rc)
				dprintk(CVP_ERR,
				"Failed voting bus %s to ab %u\n",
				bus->name, bus->range[1]*1000);
#endif
		}
	}

err_no_mem:
	return rc;
}

static int iris_hfi_vote_buses(void *dev, struct cvp_bus_vote_data *d, int n)
{
	int rc = 0;
	struct iris_hfi_device *device = dev;

	if (!device)
		return -EINVAL;

	mutex_lock(&device->lock);
	rc = __vote_buses(device, d, n);
	mutex_unlock(&device->lock);

	return rc;

}

static int __core_set_resource(struct iris_hfi_device *device,
		struct cvp_resource_hdr *resource_hdr, void *resource_value)
{
	struct cvp_hfi_cmd_sys_set_resource_packet *pkt;
	u8 packet[CVP_IFACEQ_VAR_SMALL_PKT_SIZE];
	int rc = 0;

	if (!device || !resource_hdr || !resource_value) {
		dprintk(CVP_ERR, "set_res: Invalid Params\n");
		return -EINVAL;
	}

	pkt = (struct cvp_hfi_cmd_sys_set_resource_packet *) packet;

	rc = call_hfi_pkt_op(device, sys_set_resource,
			pkt, resource_hdr, resource_value);
	if (rc) {
		dprintk(CVP_ERR, "set_res: failed to create packet\n");
		goto err_create_pkt;
	}

	rc = __iface_cmdq_write(device, pkt);
	if (rc)
		rc = -ENOTEMPTY;

err_create_pkt:
	return rc;
}

static int __core_release_resource(struct iris_hfi_device *device,
		struct cvp_resource_hdr *resource_hdr)
{
	struct cvp_hfi_cmd_sys_release_resource_packet *pkt;
	u8 packet[CVP_IFACEQ_VAR_SMALL_PKT_SIZE];
	int rc = 0;

	if (!device || !resource_hdr) {
		dprintk(CVP_ERR, "release_res: Invalid Params\n");
		return -EINVAL;
	}

	pkt = (struct cvp_hfi_cmd_sys_release_resource_packet *) packet;

	rc = call_hfi_pkt_op(device, sys_release_resource,
			pkt, resource_hdr);

	if (rc) {
		dprintk(CVP_ERR, "release_res: failed to create packet\n");
		goto err_create_pkt;
	}

	rc = __iface_cmdq_write(device, pkt);
	if (rc)
		rc = -ENOTEMPTY;

err_create_pkt:
	return rc;
}

static int __tzbsp_set_cvp_state(enum tzbsp_subsys_state state)
{
	int tzbsp_rsp = 0;
	int rc = 0;
	struct scm_desc desc = {0};

	desc.args[0] = state;
	desc.args[1] = TZBSP_CVP_PAS_ID;
	desc.arginfo = SCM_ARGS(2);

	rc = scm_call2(SCM_SIP_FNID(SCM_SVC_BOOT,
			TZBSP_PIL_SET_STATE), &desc);
	tzbsp_rsp = desc.ret[0];

	if (rc) {
		dprintk(CVP_ERR, "Failed scm_call %d\n", rc);
		return rc;
	}

	dprintk(CVP_DBG, "Set state %d, resp %d\n", state, tzbsp_rsp);
	if (tzbsp_rsp) {
		dprintk(CVP_ERR,
			"Failed to set cvp core state to suspend: %d\n",
			tzbsp_rsp);
		return -EINVAL;
	}

	return 0;
}

static inline int __boot_firmware(struct iris_hfi_device *device)
{
	int rc = 0;
	u32 ctrl_init_val = 0, ctrl_status = 0, count = 0, max_tries = 1000;

	ctrl_init_val = BIT(0);
	__write_register(device, CVP_CTRL_INIT, ctrl_init_val);
	while (!ctrl_status && count < max_tries) {
		ctrl_status = __read_register(device, CVP_CTRL_STATUS);
		if ((ctrl_status & CVP_CTRL_ERROR_STATUS__M) == 0x4) {
			dprintk(CVP_ERR, "invalid setting for UC_REGION\n");
			rc = -ENODATA;
			break;
		}

		/* Reduce to 1/100th and x100 of max_tries */
		usleep_range(500, 1000);
		count++;
	}

	if (!(ctrl_status & CVP_CTRL_INIT_STATUS__M)) {
		dprintk(CVP_ERR, "Failed to boot FW status: %x\n",
			ctrl_status);
		rc = -ENODEV;
	}

	/* Enable interrupt before sending commands to tensilica */
	__write_register(device, CVP_CPU_CS_H2XSOFTINTEN, 0x1);
	__write_register(device, CVP_CPU_CS_X2RPMh, 0x0);

	return rc;
}

static int iris_hfi_suspend(void *dev)
{
	int rc = 0;
	struct iris_hfi_device *device = (struct iris_hfi_device *) dev;

	if (!device) {
		dprintk(CVP_ERR, "%s invalid device\n", __func__);
		return -EINVAL;
	} else if (!device->res->sw_power_collapsible) {
		return -ENOTSUPP;
	}

	dprintk(CVP_DBG, "Suspending Iris\n");
	mutex_lock(&device->lock);
	rc = __power_collapse(device, true);
	if (rc) {
		dprintk(CVP_WARN, "%s: Iris is busy\n", __func__);
		rc = -EBUSY;
	}
	mutex_unlock(&device->lock);

	/* Cancel pending delayed works if any */
	if (!rc)
		cancel_delayed_work(&iris_hfi_pm_work);

	return rc;
}

static void cvp_dump_csr(struct iris_hfi_device *dev)
{
	u32 reg;

	if (!dev)
		return;
	if (!dev->power_enabled || dev->reg_dumped)
		return;
	reg = __read_register(dev, CVP_WRAPPER_CPU_STATUS);
	dprintk(CVP_ERR, "CVP_WRAPPER_CPU_STATUS: %x\n", reg);
	reg = __read_register(dev, CVP_CPU_CS_SCIACMDARG0);
	dprintk(CVP_ERR, "CVP_CPU_CS_SCIACMDARG0: %x\n", reg);
	reg = __read_register(dev, CVP_WRAPPER_CPU_CLOCK_CONFIG);
	dprintk(CVP_ERR, "CVP_WRAPPER_CPU_CLOCK_CONFIG: %x\n", reg);
	reg = __read_register(dev, CVP_WRAPPER_CORE_CLOCK_CONFIG);
	dprintk(CVP_ERR, "CVP_WRAPPER_CORE_CLOCK_CONFIG: %x\n", reg);
	reg = __read_register(dev, CVP_WRAPPER_INTR_STATUS);
	dprintk(CVP_ERR, "CVP_WRAPPER_INTR_STATUS: %x\n", reg);
	reg = __read_register(dev, CVP_CPU_CS_H2ASOFTINT);
	dprintk(CVP_ERR, "CVP_CPU_CS_H2ASOFTINT: %x\n", reg);
	reg = __read_register(dev, CVP_CPU_CS_A2HSOFTINT);
	dprintk(CVP_ERR, "CVP_CPU_CS_A2HSOFTINT: %x\n", reg);
	reg = __read_register(dev, CVP_CC_MVS1C_GDSCR);
	dprintk(CVP_ERR, "CVP_CC_MVS1C_GDSCR: %x\n", reg);
	reg = __read_register(dev, CVP_CC_MVS1C_CBCR);
	dprintk(CVP_ERR, "CVP_CC_MVS1C_CBCR: %x\n", reg);
	dev->reg_dumped = true;
}

static int iris_hfi_flush_debug_queue(void *dev)
{
	int rc = 0;
	struct iris_hfi_device *device = (struct iris_hfi_device *) dev;

	if (!device) {
		dprintk(CVP_ERR, "%s invalid device\n", __func__);
		return -EINVAL;
	}

	cvp_dump_csr(device);
	mutex_lock(&device->lock);

	if (!device->power_enabled) {
		dprintk(CVP_WARN, "%s: iris power off\n", __func__);
		rc = -EINVAL;
		goto exit;
	}
	__flush_debug_queue(device, NULL);
exit:
	mutex_unlock(&device->lock);
	return rc;
}

static int __set_clocks(struct iris_hfi_device *device, u32 freq)
{
	struct clock_info *cl;
	int rc = 0;

	iris_hfi_for_each_clock(device, cl) {
		if (cl->has_scaling) {/* has_scaling */
			device->clk_freq = freq;
			if (msm_cvp_clock_voting)
				freq = msm_cvp_clock_voting;

			rc = clk_set_rate(cl->clk, freq);
			if (rc) {
				dprintk(CVP_ERR,
					"Failed to set clock rate %u %s: %d %s\n",
					freq, cl->name, rc, __func__);
				return rc;
			}

			trace_msm_cvp_perf_clock_scale(cl->name, freq);
			dprintk(CVP_DBG, "Scaling clock %s to %u\n",
					cl->name, freq);
		}
	}

	return 0;
}

static int iris_hfi_scale_clocks(void *dev, u32 freq)
{
	int rc = 0;
	struct iris_hfi_device *device = dev;

	if (!device) {
		dprintk(CVP_ERR, "Invalid args: %pK\n", device);
		return -EINVAL;
	}

	mutex_lock(&device->lock);

	if (__resume(device)) {
		dprintk(CVP_ERR, "Resume from power collapse failed\n");
		rc = -ENODEV;
		goto exit;
	}

	rc = __set_clocks(device, freq);
exit:
	mutex_unlock(&device->lock);

	return rc;
}

static int __scale_clocks(struct iris_hfi_device *device)
{
	int rc = 0;
	struct allowed_clock_rates_table *allowed_clks_tbl = NULL;
	u32 rate = 0;

	allowed_clks_tbl = device->res->allowed_clks_tbl;

	rate = device->clk_freq ? device->clk_freq :
		allowed_clks_tbl[0].clock_rate;

	dprintk(CVP_DBG, "%s: scale clock rate %d\n", __func__, rate);
	rc = __set_clocks(device, rate);
	return rc;
}

/* Writes into cmdq without raising an interrupt */
static int __iface_cmdq_write_relaxed(struct iris_hfi_device *device,
		void *pkt, bool *requires_interrupt)
{
	struct cvp_iface_q_info *q_info;
	struct cvp_hal_cmd_pkt_hdr *cmd_packet;
	int result = -E2BIG;

	if (!device || !pkt) {
		dprintk(CVP_ERR, "Invalid Params\n");
		return -EINVAL;
	}

	__strict_check(device);

	if (!__core_in_valid_state(device)) {
		dprintk(CVP_ERR, "%s - fw not in init state\n", __func__);
		result = -EINVAL;
		goto err_q_null;
	}

	cmd_packet = (struct cvp_hal_cmd_pkt_hdr *)pkt;
	device->last_packet_type = cmd_packet->packet_type;

	q_info = &device->iface_queues[CVP_IFACEQ_CMDQ_IDX];
	if (!q_info) {
		dprintk(CVP_ERR, "cannot write to shared Q's\n");
		goto err_q_null;
	}

	if (!q_info->q_array.align_virtual_addr) {
		dprintk(CVP_ERR, "cannot write to shared CMD Q's\n");
		result = -ENODATA;
		goto err_q_null;
	}

	if (__resume(device)) {
		dprintk(CVP_ERR, "%s: Power on failed\n", __func__);
		goto err_q_write;
	}

	if (!__write_queue(q_info, (u8 *)pkt, requires_interrupt)) {
		if (device->res->sw_power_collapsible) {
			cancel_delayed_work(&iris_hfi_pm_work);
			if (!queue_delayed_work(device->iris_pm_workq,
				&iris_hfi_pm_work,
				msecs_to_jiffies(
				device->res->msm_cvp_pwr_collapse_delay))) {
				dprintk(CVP_DBG,
				"PM work already scheduled\n");
			}
		}

		result = 0;
	} else {
		dprintk(CVP_ERR, "__iface_cmdq_write: queue full\n");
	}

err_q_write:
err_q_null:
	return result;
}

static int __iface_cmdq_write(struct iris_hfi_device *device, void *pkt)
{
	bool needs_interrupt = false;
	int rc = __iface_cmdq_write_relaxed(device, pkt, &needs_interrupt);

	if (!rc && needs_interrupt) {
		/* Consumer of cmdq prefers that we raise an interrupt */
		rc = 0;
		__write_register(device, CVP_CPU_CS_H2ASOFTINT, 1);
	}

	return rc;
}

static int __iface_msgq_read(struct iris_hfi_device *device, void *pkt)
{
	u32 tx_req_is_set = 0;
	int rc = 0;
	struct cvp_iface_q_info *q_info;

	if (!pkt) {
		dprintk(CVP_ERR, "Invalid Params\n");
		return -EINVAL;
	}

	__strict_check(device);

	if (!__core_in_valid_state(device)) {
		dprintk(CVP_DBG, "%s - fw not in init state\n", __func__);
		rc = -EINVAL;
		goto read_error_null;
	}

	q_info = &device->iface_queues[CVP_IFACEQ_MSGQ_IDX];
	if (q_info->q_array.align_virtual_addr == NULL) {
		dprintk(CVP_ERR, "cannot read from shared MSG Q's\n");
		rc = -ENODATA;
		goto read_error_null;
	}

	if (!__read_queue(q_info, (u8 *)pkt, &tx_req_is_set)) {
		if (tx_req_is_set)
			__write_register(device, CVP_CPU_CS_H2ASOFTINT, 1);
		rc = 0;
	} else
		rc = -ENODATA;

read_error_null:
	return rc;
}

static int __iface_dbgq_read(struct iris_hfi_device *device, void *pkt)
{
	u32 tx_req_is_set = 0;
	int rc = 0;
	struct cvp_iface_q_info *q_info;

	if (!pkt) {
		dprintk(CVP_ERR, "Invalid Params\n");
		return -EINVAL;
	}

	__strict_check(device);

	q_info = &device->iface_queues[CVP_IFACEQ_DBGQ_IDX];
	if (q_info->q_array.align_virtual_addr == NULL) {
		dprintk(CVP_ERR, "cannot read from shared DBG Q's\n");
		rc = -ENODATA;
		goto dbg_error_null;
	}

	if (!__read_queue(q_info, (u8 *)pkt, &tx_req_is_set)) {
		if (tx_req_is_set)
			__write_register(device, CVP_CPU_CS_H2ASOFTINT, 1);
		rc = 0;
	} else
		rc = -ENODATA;

dbg_error_null:
	return rc;
}

static void __set_queue_hdr_defaults(struct cvp_hfi_queue_header *q_hdr)
{
	q_hdr->qhdr_status = 0x1;
	q_hdr->qhdr_type = CVP_IFACEQ_DFLT_QHDR;
	q_hdr->qhdr_q_size = CVP_IFACEQ_QUEUE_SIZE / 4;
	q_hdr->qhdr_pkt_size = 0;
	q_hdr->qhdr_rx_wm = 0x1;
	q_hdr->qhdr_tx_wm = 0x1;
	q_hdr->qhdr_rx_req = 0x1;
	q_hdr->qhdr_tx_req = 0x0;
	q_hdr->qhdr_rx_irq_status = 0x0;
	q_hdr->qhdr_tx_irq_status = 0x0;
	q_hdr->qhdr_read_idx = 0x0;
	q_hdr->qhdr_write_idx = 0x0;
}

static void __interface_dsp_queues_release(struct iris_hfi_device *device)
{
	int i;
	struct msm_cvp_smem *mem_data = &device->dsp_iface_q_table.mem_data;
	struct context_bank_info *cb = mem_data->mapping_info.cb_info;

	if (!device->dsp_iface_q_table.align_virtual_addr) {
		dprintk(CVP_ERR, "%s: already released\n", __func__);
		return;
	}

	dma_unmap_single_attrs(cb->dev, mem_data->device_addr,
		mem_data->size, DMA_BIDIRECTIONAL, 0);
	dma_free_coherent(device->res->mem_cdsp.dev, mem_data->size,
		mem_data->kvaddr, mem_data->dma_handle);

	for (i = 0; i < CVP_IFACEQ_NUMQ; i++) {
		device->dsp_iface_queues[i].q_hdr = NULL;
		device->dsp_iface_queues[i].q_array.align_virtual_addr = NULL;
		device->dsp_iface_queues[i].q_array.align_device_addr = 0;
	}
	device->dsp_iface_q_table.align_virtual_addr = NULL;
	device->dsp_iface_q_table.align_device_addr = 0;
}

static int __interface_dsp_queues_init(struct iris_hfi_device *dev)
{
	int rc = 0;
	u32 i;
	struct cvp_hfi_queue_table_header *q_tbl_hdr;
	struct cvp_hfi_queue_header *q_hdr;
	struct cvp_iface_q_info *iface_q;
	int offset = 0;
	phys_addr_t fw_bias = 0;
	size_t q_size;
	struct msm_cvp_smem *mem_data;
	void *kvaddr;
	dma_addr_t dma_handle;
	dma_addr_t iova;
	struct context_bank_info *cb;

	q_size = ALIGN(QUEUE_SIZE, SZ_1M);
	mem_data = &dev->dsp_iface_q_table.mem_data;

	/* Allocate dsp queues from CDSP device memory */
	kvaddr = dma_alloc_coherent(dev->res->mem_cdsp.dev, q_size,
				&dma_handle, GFP_KERNEL);
	if (IS_ERR_OR_NULL(kvaddr)) {
		dprintk(CVP_ERR, "%s: failed dma allocation\n", __func__);
		goto fail_dma_alloc;
	}
	cb = msm_cvp_smem_get_context_bank(MSM_CVP_UNKNOWN, 0, dev->res, 0);
	if (!cb) {
		dprintk(CVP_ERR,
			"%s: failed to get context bank\n", __func__);
		goto fail_dma_map;
	}
	iova = dma_map_single_attrs(cb->dev, phys_to_virt(dma_handle),
				q_size, DMA_BIDIRECTIONAL, 0);
	if (dma_mapping_error(cb->dev, iova)) {
		dprintk(CVP_ERR, "%s: failed dma mapping\n", __func__);
		goto fail_dma_map;
	}
	dprintk(CVP_DBG,
		"%s: kvaddr %pK dma_handle %#llx iova %#llx size %zd\n",
		__func__, kvaddr, dma_handle, iova, q_size);

	memset(mem_data, 0, sizeof(struct msm_cvp_smem));
	mem_data->kvaddr = kvaddr;
	mem_data->device_addr = iova;
	mem_data->dma_handle = dma_handle;
	mem_data->size = q_size;
	mem_data->buffer_type = 0;
	mem_data->mapping_info.cb_info = cb;

	if (!is_iommu_present(dev->res))
		fw_bias = dev->cvp_hal_data->firmware_base;

	dev->dsp_iface_q_table.align_virtual_addr = kvaddr;
	dev->dsp_iface_q_table.align_device_addr = iova - fw_bias;
	dev->dsp_iface_q_table.mem_size = CVP_IFACEQ_TABLE_SIZE;
	offset = dev->dsp_iface_q_table.mem_size;

	for (i = 0; i < CVP_IFACEQ_NUMQ; i++) {
		iface_q = &dev->dsp_iface_queues[i];
		iface_q->q_array.align_device_addr = iova + offset - fw_bias;
		iface_q->q_array.align_virtual_addr = kvaddr + offset;
		iface_q->q_array.mem_size = CVP_IFACEQ_QUEUE_SIZE;
		offset += iface_q->q_array.mem_size;
		iface_q->q_hdr = CVP_IFACEQ_GET_QHDR_START_ADDR(
			dev->dsp_iface_q_table.align_virtual_addr, i);
		__set_queue_hdr_defaults(iface_q->q_hdr);
		spin_lock_init(&iface_q->hfi_lock);
	}

	q_tbl_hdr = (struct cvp_hfi_queue_table_header *)
			dev->dsp_iface_q_table.align_virtual_addr;
	q_tbl_hdr->qtbl_version = 0;
	q_tbl_hdr->device_addr = (void *)dev;
	strlcpy(q_tbl_hdr->name, "msm_v4l2_cvp", sizeof(q_tbl_hdr->name));
	q_tbl_hdr->qtbl_size = CVP_IFACEQ_TABLE_SIZE;
	q_tbl_hdr->qtbl_qhdr0_offset =
				sizeof(struct cvp_hfi_queue_table_header);
	q_tbl_hdr->qtbl_qhdr_size = sizeof(struct cvp_hfi_queue_header);
	q_tbl_hdr->qtbl_num_q = CVP_IFACEQ_NUMQ;
	q_tbl_hdr->qtbl_num_active_q = CVP_IFACEQ_NUMQ;

	iface_q = &dev->dsp_iface_queues[CVP_IFACEQ_CMDQ_IDX];
	q_hdr = iface_q->q_hdr;
	q_hdr->qhdr_start_addr = iface_q->q_array.align_device_addr;
	q_hdr->qhdr_type |= HFI_Q_ID_HOST_TO_CTRL_CMD_Q;

	iface_q = &dev->dsp_iface_queues[CVP_IFACEQ_MSGQ_IDX];
	q_hdr = iface_q->q_hdr;
	q_hdr->qhdr_start_addr = iface_q->q_array.align_device_addr;
	q_hdr->qhdr_type |= HFI_Q_ID_CTRL_TO_HOST_MSG_Q;

	iface_q = &dev->dsp_iface_queues[CVP_IFACEQ_DBGQ_IDX];
	q_hdr = iface_q->q_hdr;
	q_hdr->qhdr_start_addr = iface_q->q_array.align_device_addr;
	q_hdr->qhdr_type |= HFI_Q_ID_CTRL_TO_HOST_DEBUG_Q;
	/*
	 * Set receive request to zero on debug queue as there is no
	 * need of interrupt from cvp hardware for debug messages
	 */
	q_hdr->qhdr_rx_req = 0;
	return rc;

fail_dma_map:
	dma_free_coherent(dev->res->mem_cdsp.dev, q_size, kvaddr, dma_handle);
fail_dma_alloc:
	return -ENOMEM;
}

static void __interface_queues_release(struct iris_hfi_device *device)
{
	int i;
	struct cvp_hfi_mem_map_table *qdss;
	struct cvp_hfi_mem_map *mem_map;
	int num_entries = device->res->qdss_addr_set.count;
	unsigned long mem_map_table_base_addr;
	struct context_bank_info *cb;

	if (device->qdss.align_virtual_addr) {
		qdss = (struct cvp_hfi_mem_map_table *)
			device->qdss.align_virtual_addr;
		qdss->mem_map_num_entries = num_entries;
		mem_map_table_base_addr =
			device->qdss.align_device_addr +
			sizeof(struct cvp_hfi_mem_map_table);
		qdss->mem_map_table_base_addr =
			(u32)mem_map_table_base_addr;
		if ((unsigned long)qdss->mem_map_table_base_addr !=
			mem_map_table_base_addr) {
			dprintk(CVP_ERR,
				"Invalid mem_map_table_base_addr %#lx",
				mem_map_table_base_addr);
		}

		mem_map = (struct cvp_hfi_mem_map *)(qdss + 1);
		cb = msm_cvp_smem_get_context_bank(MSM_CVP_UNKNOWN,
			false, device->res, 0);

		for (i = 0; cb && i < num_entries; i++) {
			iommu_unmap(cb->domain,
						mem_map[i].virtual_addr,
						mem_map[i].size);
		}

		__smem_free(device, &device->qdss.mem_data);
	}

	__smem_free(device, &device->iface_q_table.mem_data);
	__smem_free(device, &device->sfr.mem_data);

	for (i = 0; i < CVP_IFACEQ_NUMQ; i++) {
		device->iface_queues[i].q_hdr = NULL;
		device->iface_queues[i].q_array.align_virtual_addr = NULL;
		device->iface_queues[i].q_array.align_device_addr = 0;
	}

	device->iface_q_table.align_virtual_addr = NULL;
	device->iface_q_table.align_device_addr = 0;

	device->qdss.align_virtual_addr = NULL;
	device->qdss.align_device_addr = 0;

	device->sfr.align_virtual_addr = NULL;
	device->sfr.align_device_addr = 0;

	device->mem_addr.align_virtual_addr = NULL;
	device->mem_addr.align_device_addr = 0;

	__interface_dsp_queues_release(device);
}

static int __get_qdss_iommu_virtual_addr(struct iris_hfi_device *dev,
		struct cvp_hfi_mem_map *mem_map,
		struct iommu_domain *domain)
{
	int i;
	int rc = 0;
	dma_addr_t iova = QDSS_IOVA_START;
	int num_entries = dev->res->qdss_addr_set.count;
	struct addr_range *qdss_addr_tbl = dev->res->qdss_addr_set.addr_tbl;

	if (!num_entries)
		return -ENODATA;

	for (i = 0; i < num_entries; i++) {
		if (domain) {
			rc = iommu_map(domain, iova,
					qdss_addr_tbl[i].start,
					qdss_addr_tbl[i].size,
					IOMMU_READ | IOMMU_WRITE);

			if (rc) {
				dprintk(CVP_ERR,
						"IOMMU QDSS mapping failed for addr %#x\n",
						qdss_addr_tbl[i].start);
				rc = -ENOMEM;
				break;
			}
		} else {
			iova =  qdss_addr_tbl[i].start;
		}

		mem_map[i].virtual_addr = (u32)iova;
		mem_map[i].physical_addr = qdss_addr_tbl[i].start;
		mem_map[i].size = qdss_addr_tbl[i].size;
		mem_map[i].attr = 0x0;

		iova += mem_map[i].size;
	}

	if (i < num_entries) {
		dprintk(CVP_ERR,
			"QDSS mapping failed, Freeing other entries %d\n", i);

		for (--i; domain && i >= 0; i--) {
			iommu_unmap(domain,
				mem_map[i].virtual_addr,
				mem_map[i].size);
		}
	}

	return rc;
}

static void __setup_ucregion_memory_map(struct iris_hfi_device *device)
{
	__write_register(device, CVP_UC_REGION_ADDR,
			(u32)device->iface_q_table.align_device_addr);
	__write_register(device, CVP_UC_REGION_SIZE, SHARED_QSIZE);
	__write_register(device, CVP_QTBL_ADDR,
			(u32)device->iface_q_table.align_device_addr);
	__write_register(device, CVP_QTBL_INFO, 0x01);
	if (device->sfr.align_device_addr)
		__write_register(device, CVP_SFR_ADDR,
				(u32)device->sfr.align_device_addr);
	if (device->qdss.align_device_addr)
		__write_register(device, CVP_MMAP_ADDR,
				(u32)device->qdss.align_device_addr);
	call_iris_op(device, setup_dsp_uc_memmap, device);
}

static int __interface_queues_init(struct iris_hfi_device *dev)
{
	struct cvp_hfi_queue_table_header *q_tbl_hdr;
	struct cvp_hfi_queue_header *q_hdr;
	u32 i;
	int rc = 0;
	struct cvp_hfi_mem_map_table *qdss;
	struct cvp_hfi_mem_map *mem_map;
	struct cvp_iface_q_info *iface_q;
	struct cvp_hfi_sfr_struct *vsfr;
	struct cvp_mem_addr *mem_addr;
	int offset = 0;
	int num_entries = dev->res->qdss_addr_set.count;
	phys_addr_t fw_bias = 0;
	size_t q_size;
	unsigned long mem_map_table_base_addr;
	struct context_bank_info *cb;

	q_size = SHARED_QSIZE - ALIGNED_SFR_SIZE - ALIGNED_QDSS_SIZE;
	mem_addr = &dev->mem_addr;
	if (!is_iommu_present(dev->res))
		fw_bias = dev->cvp_hal_data->firmware_base;
	rc = __smem_alloc(dev, mem_addr, q_size, 1, SMEM_UNCACHED);
	if (rc) {
		dprintk(CVP_ERR, "iface_q_table_alloc_fail\n");
		goto fail_alloc_queue;
	}

	dev->iface_q_table.align_virtual_addr = mem_addr->align_virtual_addr;
	dev->iface_q_table.align_device_addr = mem_addr->align_device_addr -
					fw_bias;
	dev->iface_q_table.mem_size = CVP_IFACEQ_TABLE_SIZE;
	dev->iface_q_table.mem_data = mem_addr->mem_data;
	offset += dev->iface_q_table.mem_size;

	for (i = 0; i < CVP_IFACEQ_NUMQ; i++) {
		iface_q = &dev->iface_queues[i];
		iface_q->q_array.align_device_addr = mem_addr->align_device_addr
			+ offset - fw_bias;
		iface_q->q_array.align_virtual_addr =
			mem_addr->align_virtual_addr + offset;
		iface_q->q_array.mem_size = CVP_IFACEQ_QUEUE_SIZE;
		offset += iface_q->q_array.mem_size;
		iface_q->q_hdr = CVP_IFACEQ_GET_QHDR_START_ADDR(
				dev->iface_q_table.align_virtual_addr, i);
		__set_queue_hdr_defaults(iface_q->q_hdr);
		spin_lock_init(&iface_q->hfi_lock);
	}

	if ((msm_cvp_fw_debug_mode & HFI_DEBUG_MODE_QDSS) && num_entries) {
		rc = __smem_alloc(dev, mem_addr, ALIGNED_QDSS_SIZE, 1,
				SMEM_UNCACHED);
		if (rc) {
			dprintk(CVP_WARN,
				"qdss_alloc_fail: QDSS messages logging will not work\n");
			dev->qdss.align_device_addr = 0;
		} else {
			dev->qdss.align_device_addr =
				mem_addr->align_device_addr - fw_bias;
			dev->qdss.align_virtual_addr =
				mem_addr->align_virtual_addr;
			dev->qdss.mem_size = ALIGNED_QDSS_SIZE;
			dev->qdss.mem_data = mem_addr->mem_data;
		}
	}

	rc = __smem_alloc(dev, mem_addr, ALIGNED_SFR_SIZE, 1, SMEM_UNCACHED);
	if (rc) {
		dprintk(CVP_WARN, "sfr_alloc_fail: SFR not will work\n");
		dev->sfr.align_device_addr = 0;
	} else {
		dev->sfr.align_device_addr = mem_addr->align_device_addr -
					fw_bias;
		dev->sfr.align_virtual_addr = mem_addr->align_virtual_addr;
		dev->sfr.mem_size = ALIGNED_SFR_SIZE;
		dev->sfr.mem_data = mem_addr->mem_data;
	}

	q_tbl_hdr = (struct cvp_hfi_queue_table_header *)
			dev->iface_q_table.align_virtual_addr;
	q_tbl_hdr->qtbl_version = 0;
	q_tbl_hdr->device_addr = (void *)dev;
	strlcpy(q_tbl_hdr->name, "msm_v4l2_cvp", sizeof(q_tbl_hdr->name));
	q_tbl_hdr->qtbl_size = CVP_IFACEQ_TABLE_SIZE;
	q_tbl_hdr->qtbl_qhdr0_offset =
				sizeof(struct cvp_hfi_queue_table_header);
	q_tbl_hdr->qtbl_qhdr_size = sizeof(struct cvp_hfi_queue_header);
	q_tbl_hdr->qtbl_num_q = CVP_IFACEQ_NUMQ;
	q_tbl_hdr->qtbl_num_active_q = CVP_IFACEQ_NUMQ;

	iface_q = &dev->iface_queues[CVP_IFACEQ_CMDQ_IDX];
	q_hdr = iface_q->q_hdr;
	q_hdr->qhdr_start_addr = iface_q->q_array.align_device_addr;
	q_hdr->qhdr_type |= HFI_Q_ID_HOST_TO_CTRL_CMD_Q;

	iface_q = &dev->iface_queues[CVP_IFACEQ_MSGQ_IDX];
	q_hdr = iface_q->q_hdr;
	q_hdr->qhdr_start_addr = iface_q->q_array.align_device_addr;
	q_hdr->qhdr_type |= HFI_Q_ID_CTRL_TO_HOST_MSG_Q;

	iface_q = &dev->iface_queues[CVP_IFACEQ_DBGQ_IDX];
	q_hdr = iface_q->q_hdr;
	q_hdr->qhdr_start_addr = iface_q->q_array.align_device_addr;
	q_hdr->qhdr_type |= HFI_Q_ID_CTRL_TO_HOST_DEBUG_Q;
	/*
	 * Set receive request to zero on debug queue as there is no
	 * need of interrupt from cvp hardware for debug messages
	 */
	q_hdr->qhdr_rx_req = 0;

	if (dev->qdss.align_virtual_addr) {
		qdss =
		(struct cvp_hfi_mem_map_table *)dev->qdss.align_virtual_addr;
		qdss->mem_map_num_entries = num_entries;
		mem_map_table_base_addr = dev->qdss.align_device_addr +
			sizeof(struct cvp_hfi_mem_map_table);
		qdss->mem_map_table_base_addr = mem_map_table_base_addr;

		mem_map = (struct cvp_hfi_mem_map *)(qdss + 1);
		cb = msm_cvp_smem_get_context_bank(MSM_CVP_UNKNOWN, false,
			dev->res, 0);
		if (!cb) {
			dprintk(CVP_ERR,
				"%s: failed to get context bank\n", __func__);
			return -EINVAL;
		}

		rc = __get_qdss_iommu_virtual_addr(dev, mem_map, cb->domain);
		if (rc) {
			dprintk(CVP_ERR,
				"IOMMU mapping failed, Freeing qdss memdata\n");
			__smem_free(dev, &dev->qdss.mem_data);
			dev->qdss.align_virtual_addr = NULL;
			dev->qdss.align_device_addr = 0;
		}
	}

	vsfr = (struct cvp_hfi_sfr_struct *) dev->sfr.align_virtual_addr;
	if (vsfr)
		vsfr->bufSize = ALIGNED_SFR_SIZE;

	rc = __interface_dsp_queues_init(dev);
	if (rc) {
		dprintk(CVP_ERR, "dsp_queues_init failed\n");
		goto fail_alloc_queue;
	}

	__setup_ucregion_memory_map(dev);
	return 0;
fail_alloc_queue:
	return -ENOMEM;
}

static int __sys_set_debug(struct iris_hfi_device *device, u32 debug)
{
	u8 packet[CVP_IFACEQ_VAR_SMALL_PKT_SIZE];
	int rc = 0;
	struct cvp_hfi_cmd_sys_set_property_packet *pkt =
		(struct cvp_hfi_cmd_sys_set_property_packet *) &packet;

	rc = call_hfi_pkt_op(device, sys_debug_config, pkt, debug);
	if (rc) {
		dprintk(CVP_WARN,
			"Debug mode setting to FW failed\n");
		return -ENOTEMPTY;
	}

	if (__iface_cmdq_write(device, pkt))
		return -ENOTEMPTY;
	return 0;
}

static int __sys_set_idle_indicator(struct iris_hfi_device *device,
	bool enable)
{
	u8 packet[CVP_IFACEQ_VAR_SMALL_PKT_SIZE];
	int rc = 0;
	struct cvp_hfi_cmd_sys_set_property_packet *pkt =
		(struct cvp_hfi_cmd_sys_set_property_packet *) &packet;

	rc = call_hfi_pkt_op(device, sys_set_idle_indicator, pkt, enable);
	if (__iface_cmdq_write(device, pkt))
		return -ENOTEMPTY;
	return 0;
}

static int __sys_set_coverage(struct iris_hfi_device *device, u32 mode)
{
	u8 packet[CVP_IFACEQ_VAR_SMALL_PKT_SIZE];
	int rc = 0;
	struct cvp_hfi_cmd_sys_set_property_packet *pkt =
		(struct cvp_hfi_cmd_sys_set_property_packet *) &packet;

	rc = call_hfi_pkt_op(device, sys_coverage_config,
			pkt, mode);
	if (rc) {
		dprintk(CVP_WARN,
			"Coverage mode setting to FW failed\n");
		return -ENOTEMPTY;
	}

	if (__iface_cmdq_write(device, pkt)) {
		dprintk(CVP_WARN, "Failed to send coverage pkt to f/w\n");
		return -ENOTEMPTY;
	}

	return 0;
}

static int __sys_set_power_control(struct iris_hfi_device *device,
	bool enable)
{
	struct regulator_info *rinfo;
	bool supported = false;
	u8 packet[CVP_IFACEQ_VAR_SMALL_PKT_SIZE];
	struct cvp_hfi_cmd_sys_set_property_packet *pkt =
		(struct cvp_hfi_cmd_sys_set_property_packet *) &packet;

	iris_hfi_for_each_regulator(device, rinfo) {
		if (rinfo->has_hw_power_collapse) {
			supported = true;
			break;
		}
	}

	if (!supported)
		return 0;

	call_hfi_pkt_op(device, sys_power_control, pkt, enable);
	if (__iface_cmdq_write(device, pkt))
		return -ENOTEMPTY;
	return 0;
}

static void dsp_init_work_handler(struct work_struct *work)
{
	int rc = 0;
	static int retry_count;
	struct iris_hfi_device *device;

	if (!work) {
		dprintk(CVP_ERR, "%s: NULL device\n", __func__);
		return;
	}

	device = container_of(work, struct iris_hfi_device, dsp_init_work.work);
	if (!device) {
		dprintk(CVP_ERR, "%s: NULL device\n", __func__);
		return;
	}

	dprintk(CVP_PROF, "Entering %s\n", __func__);

	mutex_lock(&device->lock);
	rc = __dsp_send_hfi_queue(device);
	mutex_unlock(&device->lock);

	if (rc) {
		if (retry_count > MAX_DSP_INIT_ATTEMPTS) {
			dprintk(CVP_ERR, "%s: max trials exceeded\n", __func__);
			return;
		}
		dprintk(CVP_PROF, "%s: Attempt to init DSP %d\n",
			__func__, retry_count);

		schedule_delayed_work(&device->dsp_init_work,
				msecs_to_jiffies(CVP_MAX_WAIT_TIME));
		++retry_count;
	}
}

static int iris_hfi_core_init(void *device)
{
	int rc = 0;
	struct cvp_hfi_cmd_sys_init_packet pkt;
	struct cvp_hfi_cmd_sys_get_property_packet version_pkt;
	struct iris_hfi_device *dev;

	if (!device) {
		dprintk(CVP_ERR, "Invalid device\n");
		return -ENODEV;
	}

	dev = device;

	dprintk(CVP_DBG, "Core initializing\n");

	mutex_lock(&dev->lock);

	dev->bus_vote.data =
		kzalloc(sizeof(struct cvp_bus_vote_data), GFP_KERNEL);
	if (!dev->bus_vote.data) {
		dprintk(CVP_ERR, "Bus vote data memory is not allocated\n");
		rc = -ENOMEM;
		goto err_no_mem;
	}

	dev->bus_vote.data_count = 1;
	dev->bus_vote.data->power_mode = CVP_POWER_TURBO;

	rc = __load_fw(dev);
	if (rc) {
		dprintk(CVP_ERR, "Failed to load Iris FW\n");
		goto err_load_fw;
	}

	__set_state(dev, IRIS_STATE_INIT);
	dev->reg_dumped = false;

	dprintk(CVP_DBG, "Dev_Virt: %pa, Reg_Virt: %pK\n",
		&dev->cvp_hal_data->firmware_base,
		dev->cvp_hal_data->register_base);


	rc = __interface_queues_init(dev);
	if (rc) {
		dprintk(CVP_ERR, "failed to init queues\n");
		rc = -ENOMEM;
		goto err_core_init;
	}

	rc = __boot_firmware(dev);
	if (rc) {
		dprintk(CVP_ERR, "Failed to start core\n");
		rc = -ENODEV;
		goto err_core_init;
	}

	dev->version = __read_register(dev, CVP_VERSION_INFO);

	rc =  call_hfi_pkt_op(dev, sys_init, &pkt, 0);
	if (rc) {
		dprintk(CVP_ERR, "Failed to create sys init pkt\n");
		goto err_core_init;
	}

	if (__iface_cmdq_write(dev, &pkt)) {
		rc = -ENOTEMPTY;
		goto err_core_init;
	}

	rc = call_hfi_pkt_op(dev, sys_image_version, &version_pkt);
	if (rc || __iface_cmdq_write(dev, &version_pkt))
		dprintk(CVP_WARN, "Failed to send image version pkt to f/w\n");

	__sys_set_debug(device, msm_cvp_fw_debug);

	__enable_subcaches(device);
	__set_subcaches(device);

	__set_ubwc_config(device);
	__sys_set_idle_indicator(device, true);

	if (dev->res->pm_qos_latency_us) {
#ifdef CONFIG_SMP
		dev->qos.type = PM_QOS_REQ_AFFINE_IRQ;
		dev->qos.irq = dev->cvp_hal_data->irq;
#endif
		pm_qos_add_request(&dev->qos, PM_QOS_CPU_DMA_LATENCY,
				dev->res->pm_qos_latency_us);
	}

	rc = __dsp_send_hfi_queue(device);
	if (rc)
		schedule_delayed_work(&dev->dsp_init_work,
				msecs_to_jiffies(CVP_MAX_WAIT_TIME));

	dprintk(CVP_DBG, "Core inited successfully\n");
	mutex_unlock(&dev->lock);
	return 0;
err_core_init:
	__set_state(dev, IRIS_STATE_DEINIT);
	__unload_fw(dev);
err_load_fw:
err_no_mem:
	dprintk(CVP_ERR, "Core init failed\n");
	mutex_unlock(&dev->lock);
	return rc;
}

static int iris_hfi_core_release(void *dev)
{
	int rc = 0;
	struct iris_hfi_device *device = dev;
	struct cvp_hal_session *session, *next;

	if (!device) {
		dprintk(CVP_ERR, "invalid device\n");
		return -ENODEV;
	}

	mutex_lock(&device->lock);
	dprintk(CVP_WARN, "Core releasing\n");
	if (device->res->pm_qos_latency_us &&
		pm_qos_request_active(&device->qos))
		pm_qos_remove_request(&device->qos);

	__resume(device);
	__set_state(device, IRIS_STATE_DEINIT);

	__dsp_shutdown(device, 0);

	__unload_fw(device);

	/* unlink all sessions from device */
	list_for_each_entry_safe(session, next, &device->sess_head, list) {
		list_del(&session->list);
		session->device = NULL;
	}

	dprintk(CVP_DBG, "Core released successfully\n");
	mutex_unlock(&device->lock);

	return rc;
}

static void __core_clear_interrupt(struct iris_hfi_device *device)
{
	u32 intr_status = 0, mask = 0;

	if (!device) {
		dprintk(CVP_ERR, "%s: NULL device\n", __func__);
		return;
	}

	intr_status = __read_register(device, CVP_WRAPPER_INTR_STATUS);
	mask = (CVP_WRAPPER_INTR_MASK_A2HCPU_BMSK | CVP_FATAL_INTR_BMSK);

	if (intr_status & mask) {
		device->intr_status |= intr_status;
		device->reg_count++;
		dprintk(CVP_DBG,
			"INTERRUPT for device: %pK: times: %d status: %d\n",
			device, device->reg_count, intr_status);
	} else {
		device->spur_count++;
	}

	__write_register(device, CVP_CPU_CS_A2HSOFTINTCLR, 1);
}

static int iris_hfi_core_trigger_ssr(void *device,
		enum hal_ssr_trigger_type type)
{
	struct cvp_hfi_cmd_sys_test_ssr_packet pkt;
	int rc = 0;
	struct iris_hfi_device *dev;

	if (!device) {
		dprintk(CVP_ERR, "invalid device\n");
		return -ENODEV;
	}

	dev = device;
	mutex_lock(&dev->lock);

	rc = call_hfi_pkt_op(dev, ssr_cmd, type, &pkt);
	if (rc) {
		dprintk(CVP_ERR, "%s: failed to create packet\n", __func__);
		goto err_create_pkt;
	}

	if (__iface_cmdq_write(dev, &pkt))
		rc = -ENOTEMPTY;

err_create_pkt:
	mutex_unlock(&dev->lock);
	return rc;
}

static void __set_default_sys_properties(struct iris_hfi_device *device)
{
	if (__sys_set_debug(device, msm_cvp_fw_debug))
		dprintk(CVP_WARN, "Setting fw_debug msg ON failed\n");
	if (__sys_set_power_control(device, msm_cvp_fw_low_power_mode))
		dprintk(CVP_WARN, "Setting h/w power collapse ON failed\n");
}

static void __session_clean(struct cvp_hal_session *session)
{
	struct cvp_hal_session *temp, *next;
	struct iris_hfi_device *device;

	if (!session || !session->device) {
		dprintk(CVP_WARN, "%s: invalid params\n", __func__);
		return;
	}
	device = session->device;
	dprintk(CVP_DBG, "deleted the session: %pK\n", session);
	/*
	 * session might have been removed from the device list in
	 * core_release, so check and remove if it is in the list
	 */
	list_for_each_entry_safe(temp, next, &device->sess_head, list) {
		if (session == temp) {
			list_del(&session->list);
			break;
		}
	}
	/* Poison the session handle with zeros */
	*session = (struct cvp_hal_session){ {0} };
	kfree(session);
}

static int iris_hfi_session_clean(void *session)
{
	struct cvp_hal_session *sess_close;
	struct iris_hfi_device *device;

	if (!session) {
		dprintk(CVP_ERR, "Invalid Params %s\n", __func__);
		return -EINVAL;
	}

	sess_close = session;
	device = sess_close->device;

	if (!device) {
		dprintk(CVP_ERR, "Invalid device handle %s\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&device->lock);

	__session_clean(sess_close);

	mutex_unlock(&device->lock);
	return 0;
}

static int iris_hfi_session_init(void *device, void *session_id,
		void **new_session)
{
	struct cvp_hfi_cmd_sys_session_init_packet pkt;
	struct iris_hfi_device *dev;
	struct cvp_hal_session *s;

	if (!device || !new_session) {
		dprintk(CVP_ERR, "%s - invalid input\n", __func__);
		return -EINVAL;
	}

	dev = device;
	mutex_lock(&dev->lock);

	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (!s) {
		dprintk(CVP_ERR, "new session fail: Out of memory\n");
		goto err_session_init_fail;
	}

	s->session_id = session_id;
	s->device = dev;
	dprintk(CVP_DBG,
		"%s: inst %pK, session %pK\n", __func__, session_id, s);

	list_add_tail(&s->list, &dev->sess_head);

	__set_default_sys_properties(device);

	if (call_hfi_pkt_op(dev, session_init, &pkt, s)) {
		dprintk(CVP_ERR, "session_init: failed to create packet\n");
		goto err_session_init_fail;
	}

	*new_session = s;
	if (__iface_cmdq_write(dev, &pkt))
		goto err_session_init_fail;

	mutex_unlock(&dev->lock);
	return 0;

err_session_init_fail:
	if (s)
		__session_clean(s);
	*new_session = NULL;
	mutex_unlock(&dev->lock);
	return -EINVAL;
}

static int __send_session_cmd(struct cvp_hal_session *session, int pkt_type)
{
	struct cvp_hal_session_cmd_pkt pkt;
	int rc = 0;
	struct iris_hfi_device *device = session->device;

	if (!__is_session_valid(device, session, __func__))
		return -ECONNRESET;

	rc = call_hfi_pkt_op(device, session_cmd,
			&pkt, pkt_type, session);
	if (rc == -EPERM)
		return 0;

	if (rc) {
		dprintk(CVP_ERR, "send session cmd: create pkt failed\n");
		goto err_create_pkt;
	}

	if (__iface_cmdq_write(session->device, &pkt))
		rc = -ENOTEMPTY;

err_create_pkt:
	return rc;
}

static int iris_hfi_session_end(void *session)
{
	struct cvp_hal_session *sess;
	struct iris_hfi_device *device;
	int rc = 0;

	if (!session) {
		dprintk(CVP_ERR, "Invalid Params %s\n", __func__);
		return -EINVAL;
	}

	sess = session;
	device = sess->device;
	if (!device) {
		dprintk(CVP_ERR, "Invalid session %s\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&device->lock);

	if (msm_cvp_fw_coverage) {
		if (__sys_set_coverage(sess->device, msm_cvp_fw_coverage))
			dprintk(CVP_WARN, "Fw_coverage msg ON failed\n");
	}

	rc = __send_session_cmd(session, HFI_CMD_SYS_SESSION_END);

	mutex_unlock(&device->lock);

	return rc;
}

static int iris_hfi_session_abort(void *sess)
{
	struct cvp_hal_session *session = sess;
	struct iris_hfi_device *device;
	int rc = 0;

	if (!session || !session->device) {
		dprintk(CVP_ERR, "Invalid Params %s\n", __func__);
		return -EINVAL;
	}

	device = session->device;

	mutex_lock(&device->lock);

	rc = __send_session_cmd(session, HFI_CMD_SYS_SESSION_ABORT);

	mutex_unlock(&device->lock);

	return rc;
}

static int iris_hfi_session_set_buffers(void *sess,
				struct cvp_buffer_addr_info *buffer_info)
{
	struct cvp_hfi_cmd_session_set_buffers_packet pkt;
	int rc = 0;
	struct cvp_hal_session *session = sess;
	struct iris_hfi_device *device;

	if (!session || !session->device || !buffer_info) {
		dprintk(CVP_ERR, "Invalid Params\n");
		return -EINVAL;
	}

	device = session->device;
	mutex_lock(&device->lock);

	if (!__is_session_valid(device, session, __func__)) {
		rc = -ECONNRESET;
		goto err_create_pkt;
	}

	rc = call_hfi_pkt_op(device, session_set_buffers,
			&pkt, session, buffer_info);
	if (rc) {
		dprintk(CVP_ERR, "set buffers: failed to create packet\n");
		goto err_create_pkt;
	}

	dprintk(CVP_DBG, "set buffers: %#x\n", buffer_info->buffer_type);
	if (__iface_cmdq_write(session->device, &pkt))
		rc = -ENOTEMPTY;

err_create_pkt:
	mutex_unlock(&device->lock);
	return rc;
}

static int iris_hfi_session_release_buffers(void *sess,
				struct cvp_buffer_addr_info *buffer_info)
{
	struct cvp_session_release_buffers_packet pkt;
	int rc = 0;
	struct cvp_hal_session *session = sess;
	struct iris_hfi_device *device;

	if (!session || !session->device || !buffer_info) {
		dprintk(CVP_ERR, "Invalid Params\n");
		return -EINVAL;
	}

	device = session->device;
	mutex_lock(&device->lock);

	if (!__is_session_valid(device, session, __func__)) {
		rc = -ECONNRESET;
		goto err_create_pkt;
	}

	rc = call_hfi_pkt_op(device, session_release_buffers,
			&pkt, session, buffer_info);
	if (rc) {
		dprintk(CVP_ERR, "release buffers: failed to create packet\n");
		goto err_create_pkt;
	}

	if (__iface_cmdq_write(session->device, &pkt))
		rc = -ENOTEMPTY;

err_create_pkt:
	mutex_unlock(&device->lock);
	return rc;
}

static int iris_hfi_session_send(void *sess,
		struct cvp_kmd_hfi_packet *in_pkt)
{
	int rc = 0;
	struct cvp_kmd_hfi_packet pkt;
	struct cvp_hal_session *session = sess;
	struct iris_hfi_device *device;

	if (!session || !session->device) {
		dprintk(CVP_ERR, "invalid session");
		return -ENODEV;
	}

	device = session->device;
	mutex_lock(&device->lock);

	if (!__is_session_valid(device, session, __func__)) {
		rc = -ECONNRESET;
		goto err_send_pkt;
	}
	rc = call_hfi_pkt_op(device, session_send,
			&pkt, session, in_pkt);
	if (rc) {
		dprintk(CVP_ERR,
				"failed to create pkt\n");
		goto err_send_pkt;
	}

	if (__iface_cmdq_write(session->device, &pkt))
		rc = -ENOTEMPTY;

err_send_pkt:
	mutex_unlock(&device->lock);
	return rc;

	return rc;
}

static int __check_core_registered(struct iris_hfi_device *device,
		phys_addr_t fw_addr, u8 *reg_addr, u32 reg_size,
		phys_addr_t irq)
{
	struct cvp_hal_data *cvp_hal_data;

	if (!device) {
		dprintk(CVP_INFO, "no device Registered\n");
		return -EINVAL;
	}

	cvp_hal_data = device->cvp_hal_data;
	if (!cvp_hal_data)
		return -EINVAL;

	if (cvp_hal_data->irq == irq &&
		(CONTAINS(cvp_hal_data->firmware_base,
				FIRMWARE_SIZE, fw_addr) ||
		CONTAINS(fw_addr, FIRMWARE_SIZE,
				cvp_hal_data->firmware_base) ||
		CONTAINS(cvp_hal_data->register_base,
				reg_size, reg_addr) ||
		CONTAINS(reg_addr, reg_size,
				cvp_hal_data->register_base) ||
		OVERLAPS(cvp_hal_data->register_base,
				reg_size, reg_addr, reg_size) ||
		OVERLAPS(reg_addr, reg_size,
				cvp_hal_data->register_base,
				reg_size) ||
		OVERLAPS(cvp_hal_data->firmware_base,
				FIRMWARE_SIZE, fw_addr,
				FIRMWARE_SIZE) ||
		OVERLAPS(fw_addr, FIRMWARE_SIZE,
				cvp_hal_data->firmware_base,
				FIRMWARE_SIZE))) {
		return 0;
	}

	dprintk(CVP_INFO, "Device not registered\n");
	return -EINVAL;
}

static void __process_fatal_error(
		struct iris_hfi_device *device)
{
	struct msm_cvp_cb_cmd_done cmd_done = {0};

	cmd_done.device_id = device->device_id;
	device->callback(HAL_SYS_ERROR, &cmd_done);
}

static int __prepare_pc(struct iris_hfi_device *device)
{
	int rc = 0;
	struct cvp_hfi_cmd_sys_pc_prep_packet pkt;

	rc = call_hfi_pkt_op(device, sys_pc_prep, &pkt);
	if (rc) {
		dprintk(CVP_ERR, "Failed to create sys pc prep pkt\n");
		goto err_pc_prep;
	}

	if (__iface_cmdq_write(device, &pkt))
		rc = -ENOTEMPTY;
	if (rc)
		dprintk(CVP_ERR, "Failed to prepare iris for power off");
err_pc_prep:
	return rc;
}

static void iris_hfi_pm_handler(struct work_struct *work)
{
	int rc = 0;
	struct msm_cvp_core *core;
	struct iris_hfi_device *device;

	core = list_first_entry(&cvp_driver->cores, struct msm_cvp_core, list);
	if (core)
		device = core->device->hfi_device_data;
	else
		return;

	if (!device) {
		dprintk(CVP_ERR, "%s: NULL device\n", __func__);
		return;
	}

	dprintk(CVP_PROF,
		"Entering %s\n", __func__);
	/*
	 * It is ok to check this variable outside the lock since
	 * it is being updated in this context only
	 */
	if (device->skip_pc_count >= CVP_MAX_PC_SKIP_COUNT) {
		dprintk(CVP_WARN, "Failed to PC for %d times\n",
				device->skip_pc_count);
		device->skip_pc_count = 0;
		__process_fatal_error(device);
		return;
	}

	mutex_lock(&device->lock);
	rc = __power_collapse(device, false);
	mutex_unlock(&device->lock);
	switch (rc) {
	case 0:
		device->skip_pc_count = 0;
		/* Cancel pending delayed works if any */
		cancel_delayed_work(&iris_hfi_pm_work);
		dprintk(CVP_PROF, "%s: power collapse successful!\n",
			__func__);
		break;
	case -EBUSY:
		device->skip_pc_count = 0;
		dprintk(CVP_DBG, "%s: retry PC as cvp is busy\n", __func__);
		queue_delayed_work(device->iris_pm_workq,
			&iris_hfi_pm_work, msecs_to_jiffies(
			device->res->msm_cvp_pwr_collapse_delay));
		break;
	case -EAGAIN:
		device->skip_pc_count++;
		dprintk(CVP_WARN, "%s: retry power collapse (count %d)\n",
			__func__, device->skip_pc_count);
		queue_delayed_work(device->iris_pm_workq,
			&iris_hfi_pm_work, msecs_to_jiffies(
			device->res->msm_cvp_pwr_collapse_delay));
		break;
	default:
		dprintk(CVP_ERR, "%s: power collapse failed\n", __func__);
		break;
	}
}

static int __power_collapse(struct iris_hfi_device *device, bool force)
{
	int rc = 0;
	u32 wfi_status = 0, idle_status = 0, pc_ready = 0;
	u32 flags = 0;
	int count = 0;
	const int max_tries = 150;

	if (!device) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	if (!device->power_enabled) {
		dprintk(CVP_DBG, "%s: Power already disabled\n",
				__func__);
		goto exit;
	}

	rc = __core_in_valid_state(device);
	if (!rc) {
		dprintk(CVP_WARN,
				"Core is in bad state, Skipping power collapse\n");
		return -EINVAL;
	}

	rc = __dsp_suspend(device, force, flags);
	if (rc == -EBUSY)
		goto exit;
	else if (rc)
		goto skip_power_off;

	pc_ready = __read_register(device, CVP_CTRL_STATUS) &
		CVP_CTRL_STATUS_PC_READY;
	if (!pc_ready) {
		wfi_status = __read_register(device,
				CVP_WRAPPER_CPU_STATUS);
		idle_status = __read_register(device,
				CVP_CTRL_STATUS);
		if (!(wfi_status & BIT(0))) {
			dprintk(CVP_WARN,
				"Skipping PC as wfi_status (%#x) bit not set\n",
				wfi_status);
			goto skip_power_off;
		}
		if (!(idle_status & BIT(30))) {
			dprintk(CVP_WARN,
				"Skipping PC as idle_status (%#x) bit not set\n",
				idle_status);
			goto skip_power_off;
		}

		rc = __prepare_pc(device);
		if (rc) {
			dprintk(CVP_WARN, "Failed PC %d\n", rc);
			goto skip_power_off;
		}

		while (count < max_tries) {
			wfi_status = __read_register(device,
					CVP_WRAPPER_CPU_STATUS);
			pc_ready = __read_register(device,
					CVP_CTRL_STATUS);
			if ((wfi_status & BIT(0)) && (pc_ready &
				CVP_CTRL_STATUS_PC_READY))
				break;
			usleep_range(150, 250);
			count++;
		}

		if (count == max_tries) {
			dprintk(CVP_ERR,
					"Skip PC. Core is not in right state (%#x, %#x)\n",
					wfi_status, pc_ready);
			goto skip_power_off;
		}
	}

	__flush_debug_queue(device, device->raw_packet);

	rc = __suspend(device);
	if (rc)
		dprintk(CVP_ERR, "Failed __suspend\n");

exit:
	return rc;

skip_power_off:
	dprintk(CVP_WARN, "Skip PC(%#x, %#x, %#x)\n",
		wfi_status, idle_status, pc_ready);
	__flush_debug_queue(device, device->raw_packet);
	return -EAGAIN;
}

static void print_sfr_message(struct iris_hfi_device *device)
{
	struct cvp_hfi_sfr_struct *vsfr = NULL;
	u32 vsfr_size = 0;
	void *p = NULL;

	vsfr = (struct cvp_hfi_sfr_struct *)device->sfr.align_virtual_addr;
	if (vsfr) {
		if (vsfr->bufSize != device->sfr.mem_size) {
			dprintk(CVP_ERR, "Invalid SFR buf size %d actual %d\n",
			vsfr->bufSize, device->sfr.mem_size);
			return;
		}
		vsfr_size = vsfr->bufSize - sizeof(u32);
		p = memchr(vsfr->rg_data, '\0', vsfr_size);
		/*
		 * SFR isn't guaranteed to be NULL terminated
		 */
		if (p == NULL)
			vsfr->rg_data[vsfr_size - 1] = '\0';

		dprintk(CVP_ERR, "SFR Message from FW: %s\n",
				vsfr->rg_data);
	}
}

static void __flush_debug_queue(struct iris_hfi_device *device, u8 *packet)
{
	bool local_packet = false;
	enum cvp_msg_prio log_level = CVP_FW;

	if (!device) {
		dprintk(CVP_ERR, "%s: Invalid params\n", __func__);
		return;
	}

	if (!packet) {
		packet = kzalloc(CVP_IFACEQ_VAR_HUGE_PKT_SIZE, GFP_KERNEL);
		if (!packet) {
			dprintk(CVP_ERR, "In %s() Fail to allocate mem\n",
				__func__);
			return;
		}

		local_packet = true;

		/*
		 * Local packek is used when something FATAL occurred.
		 * It is good to print these logs by default.
		 */

		log_level = CVP_ERR;
	}

#define SKIP_INVALID_PKT(pkt_size, payload_size, pkt_hdr_size) ({ \
		if (pkt_size < pkt_hdr_size || \
			payload_size < MIN_PAYLOAD_SIZE || \
			payload_size > \
			(pkt_size - pkt_hdr_size + sizeof(u8))) { \
			dprintk(CVP_ERR, \
				"%s: invalid msg size - %d\n", \
				__func__, pkt->msg_size); \
			continue; \
		} \
	})

	while (!__iface_dbgq_read(device, packet)) {
		struct cvp_hfi_packet_header *pkt =
			(struct cvp_hfi_packet_header *) packet;

		if (pkt->size < sizeof(struct cvp_hfi_packet_header)) {
			dprintk(CVP_ERR, "Invalid pkt size - %s\n",
				__func__);
			continue;
		}

		if (pkt->packet_type == HFI_MSG_SYS_DEBUG) {
			struct cvp_hfi_msg_sys_debug_packet *pkt =
				(struct cvp_hfi_msg_sys_debug_packet *) packet;

			SKIP_INVALID_PKT(pkt->size,
				pkt->msg_size, sizeof(*pkt));

			/*
			 * All fw messages starts with new line character. This
			 * causes dprintk to print this message in two lines
			 * in the kernel log. Ignoring the first character
			 * from the message fixes this to print it in a single
			 * line.
			 */
			pkt->rg_msg_data[pkt->msg_size-1] = '\0';
			dprintk(log_level, "%s", &pkt->rg_msg_data[1]);
		}
	}
#undef SKIP_INVALID_PKT

	if (local_packet)
		kfree(packet);
}

static bool __is_session_valid(struct iris_hfi_device *device,
		struct cvp_hal_session *session, const char *func)
{
	struct cvp_hal_session *temp = NULL;

	if (!device || !session)
		goto invalid;

	list_for_each_entry(temp, &device->sess_head, list)
		if (session == temp)
			return true;

invalid:
	dprintk(CVP_WARN, "%s: device %pK, invalid session %pK\n",
			func, device, session);
	return false;
}

static struct cvp_hal_session *__get_session(struct iris_hfi_device *device,
		u32 session_id)
{
	struct cvp_hal_session *temp = NULL;

	list_for_each_entry(temp, &device->sess_head, list) {
		if (session_id == hash32_ptr(temp))
			return temp;
	}

	return NULL;
}

#define _INVALID_MSG_ "Unrecognized MSG (%#x) session (%pK), discarding\n"
#define _INVALID_STATE_ "Ignore responses from %d to %d invalid state\n"
#define _DEVFREQ_FAIL_ "Failed to add devfreq device bus %s governor %s: %d\n"

static void process_system_msg(struct msm_cvp_cb_info *info,
		struct iris_hfi_device *device,
		void *raw_packet)
{
	struct cvp_hal_sys_init_done sys_init_done = {0};

	switch (info->response_type) {
	case HAL_SYS_ERROR:
		print_sfr_message(device);
		break;
	case HAL_SYS_RELEASE_RESOURCE_DONE:
		dprintk(CVP_DBG, "Received SYS_RELEASE_RESOURCE\n");
		break;
	case HAL_SYS_INIT_DONE:
		dprintk(CVP_DBG, "Received SYS_INIT_DONE\n");
		sys_init_done.capabilities =
			device->sys_init_capabilities;
		cvp_hfi_process_sys_init_done_prop_read(
			(struct cvp_hfi_msg_sys_init_done_packet *)
				raw_packet, &sys_init_done);
		info->response.cmd.data.sys_init_done = sys_init_done;
		break;
	default:
		break;
	}
}


static void **get_session_id(struct msm_cvp_cb_info *info)
{
	void **session_id = NULL;

	/* For session-related packets, validate session */
	switch (info->response_type) {
	case HAL_SESSION_INIT_DONE:
	case HAL_SESSION_END_DONE:
	case HAL_SESSION_ABORT_DONE:
	case HAL_SESSION_STOP_DONE:
	case HAL_SESSION_FLUSH_DONE:
	case HAL_SESSION_SET_BUFFER_DONE:
	case HAL_SESSION_SUSPEND_DONE:
	case HAL_SESSION_RESUME_DONE:
	case HAL_SESSION_SET_PROP_DONE:
	case HAL_SESSION_GET_PROP_DONE:
	case HAL_SESSION_RELEASE_BUFFER_DONE:
	case HAL_SESSION_REGISTER_BUFFER_DONE:
	case HAL_SESSION_UNREGISTER_BUFFER_DONE:
	case HAL_SESSION_DFS_CONFIG_CMD_DONE:
	case HAL_SESSION_DME_CONFIG_CMD_DONE:
	case HAL_SESSION_TME_CONFIG_CMD_DONE:
	case HAL_SESSION_ODT_CONFIG_CMD_DONE:
	case HAL_SESSION_OD_CONFIG_CMD_DONE:
	case HAL_SESSION_NCC_CONFIG_CMD_DONE:
	case HAL_SESSION_ICA_CONFIG_CMD_DONE:
	case HAL_SESSION_HCD_CONFIG_CMD_DONE:
	case HAL_SESSION_DCM_CONFIG_CMD_DONE:
	case HAL_SESSION_DC_CONFIG_CMD_DONE:
	case HAL_SESSION_PYS_HCD_CONFIG_CMD_DONE:
	case HAL_SESSION_DME_BASIC_CONFIG_CMD_DONE:
	case HAL_SESSION_DFS_FRAME_CMD_DONE:
	case HAL_SESSION_DME_FRAME_CMD_DONE:
	case HAL_SESSION_ICA_FRAME_CMD_DONE:
	case HAL_SESSION_FD_FRAME_CMD_DONE:
	case HAL_SESSION_PERSIST_CMD_DONE:
	case HAL_SESSION_FD_CONFIG_CMD_DONE:
	case HAL_SESSION_MODEL_BUF_CMD_DONE:
	case HAL_SESSION_PROPERTY_INFO:
		session_id = &info->response.cmd.session_id;
		break;
	case HAL_SESSION_ERROR:
		session_id = &info->response.data.session_id;
		break;
	case HAL_SESSION_EVENT_CHANGE:
		session_id = &info->response.event.session_id;
		break;
	case HAL_RESPONSE_UNUSED:
	default:
		session_id = NULL;
		break;
	}
	return session_id;
}

static void print_msg_hdr(void *hdr)
{
	unsigned int ver;

	ver = get_hfi_version();
	ver = (ver & HFI_VERSION_MINOR_MASK) >> HFI_VERSION_MINOR_SHIFT;

	if (ver >= 1) {
		struct cvp_hfi_msg_session_hdr *new_hdr =
			(struct cvp_hfi_msg_session_hdr *)hdr;
	dprintk(CVP_DBG, "HFI MSG received: %x %x %x %x %x %x %x\n",
			new_hdr->size, new_hdr->packet_type,
			new_hdr->session_id,
			new_hdr->client_data.transaction_id,
			new_hdr->client_data.data1,
			new_hdr->client_data.data2,
			new_hdr->error_type);
	} else {
		struct cvp_hfi_msg_session_hdr_d *old_hdr =
			(struct cvp_hfi_msg_session_hdr_d *)hdr;
		dprintk(CVP_DBG, "HFI MSG received: %x %x %x %x %x %x %x\n",
			old_hdr->size, old_hdr->packet_type,
			old_hdr->session_id,
			old_hdr->client_data.transaction_id,
			old_hdr->client_data.data1,
			old_hdr->client_data.data2,
			old_hdr->error_type);
	}
}

static int __response_handler(struct iris_hfi_device *device)
{
	struct msm_cvp_cb_info *packets;
	int packet_count = 0;
	u8 *raw_packet = NULL;
	bool requeue_pm_work = true;

	if (!device || device->state != IRIS_STATE_INIT)
		return 0;

	packets = device->response_pkt;

	raw_packet = device->raw_packet;

	if (!raw_packet || !packets) {
		dprintk(CVP_ERR,
			"%s: Invalid args : Res packet = %p, Raw packet = %p\n",
			__func__, packets, raw_packet);
		return 0;
	}

	if (device->intr_status & CVP_FATAL_INTR_BMSK) {
		struct msm_cvp_cb_info info = {
			.response_type = HAL_SYS_WATCHDOG_TIMEOUT,
			.response.cmd = {
				.device_id = device->device_id,
			}
		};

		print_sfr_message(device);

		if (device->intr_status & CVP_WRAPPER_INTR_MASK_CPU_NOC_BMSK)
			dprintk(CVP_ERR, "Received Xtensa NOC error\n");

		if (device->intr_status & CVP_WRAPPER_INTR_MASK_CORE_NOC_BMSK)
			dprintk(CVP_ERR, "Received CVP core NOC error\n");

		if (device->intr_status & CVP_WRAPPER_INTR_MASK_A2HWD_BMSK)
			dprintk(CVP_ERR, "Received CVP watchdog timeout\n");

		packets[packet_count++] = info;
		goto exit;
	}

	/* Bleed the msg queue dry of packets */
	while (!__iface_msgq_read(device, raw_packet)) {
		void **session_id = NULL;
		struct msm_cvp_cb_info *info = &packets[packet_count++];
		struct cvp_hfi_msg_session_hdr *hdr =
			(struct cvp_hfi_msg_session_hdr *)raw_packet;
		int rc = 0;

		print_msg_hdr(hdr);
		rc = cvp_hfi_process_msg_packet(device->device_id,
			(struct cvp_hal_msg_pkt_hdr *)raw_packet, info);
		if (rc) {
			dprintk(CVP_WARN,
					"Corrupt/unknown packet found, discarding\n");
			--packet_count;
			continue;
		} else if (info->response_type == HAL_NO_RESP) {
			--packet_count;
			continue;
		}

		/* Process the packet types that we're interested in */
		process_system_msg(info, device, raw_packet);

		session_id = get_session_id(info);
		/*
		 * hfi_process_msg_packet provides a session_id that's a hashed
		 * value of struct cvp_hal_session, we need to coerce the hashed
		 * value back to pointer that we can use. Ideally, hfi_process\
		 * _msg_packet should take care of this, but it doesn't have
		 * required information for it
		 */
		if (session_id) {
			struct cvp_hal_session *session = NULL;

			if (upper_32_bits((uintptr_t)*session_id) != 0) {
				dprintk(CVP_ERR,
					"Upper 32-bits != 0 for sess_id=%pK\n",
					*session_id);
			}
			session = __get_session(device,
					(u32)(uintptr_t)*session_id);
			if (!session) {
				dprintk(CVP_ERR, _INVALID_MSG_,
						info->response_type,
						*session_id);
				--packet_count;
				continue;
			}

			*session_id = session->session_id;
		}

		if (packet_count >= cvp_max_packets) {
			dprintk(CVP_WARN,
				"Too many packets in message queue!\n");
			break;
		}

		/* do not read packets after sys error packet */
		if (info->response_type == HAL_SYS_ERROR)
			break;
	}

	if (requeue_pm_work && device->res->sw_power_collapsible) {
		cancel_delayed_work(&iris_hfi_pm_work);
		if (!queue_delayed_work(device->iris_pm_workq,
			&iris_hfi_pm_work,
			msecs_to_jiffies(
				device->res->msm_cvp_pwr_collapse_delay))) {
			dprintk(CVP_ERR, "PM work already scheduled\n");
		}
	}

exit:
	__flush_debug_queue(device, raw_packet);

	return packet_count;
}

static void iris_hfi_core_work_handler(struct work_struct *work)
{
	struct msm_cvp_core *core;
	struct iris_hfi_device *device;
	int num_responses = 0, i = 0;
	u32 intr_status;

	core = list_first_entry(&cvp_driver->cores, struct msm_cvp_core, list);
	if (core)
		device = core->device->hfi_device_data;
	else
		return;

	mutex_lock(&device->lock);


	if (!__core_in_valid_state(device)) {
		dprintk(CVP_DBG, "%s - Core not in init state\n", __func__);
		goto err_no_work;
	}

	if (!device->callback) {
		dprintk(CVP_ERR, "No interrupt callback function: %pK\n",
				device);
		goto err_no_work;
	}

	if (__resume(device)) {
		dprintk(CVP_ERR, "%s: Power enable failed\n", __func__);
		goto err_no_work;
	}

	__core_clear_interrupt(device);
	num_responses = __response_handler(device);
	dprintk(CVP_DBG, "%s:: cvp_driver_debug num_responses = %d ",
		__func__, num_responses);

err_no_work:

	/* Keep the interrupt status before releasing device lock */
	intr_status = device->intr_status;
	mutex_unlock(&device->lock);

	/*
	 * Issue the callbacks outside of the locked contex to preserve
	 * re-entrancy.
	 */

	for (i = 0; !IS_ERR_OR_NULL(device->response_pkt) &&
		i < num_responses; ++i) {
		struct msm_cvp_cb_info *r = &device->response_pkt[i];

		if (!__core_in_valid_state(device)) {
			dprintk(CVP_ERR,
				_INVALID_STATE_, (i + 1), num_responses);
			break;
		}
		dprintk(CVP_DBG, "Processing response %d of %d, type %d\n",
			(i + 1), num_responses, r->response_type);
		device->callback(r->response_type, &r->response);
	}

	/* We need re-enable the irq which was disabled in ISR handler */
	if (!(intr_status & CVP_WRAPPER_INTR_STATUS_A2HWD_BMSK))
		enable_irq(device->cvp_hal_data->irq);

	/*
	 * XXX: Don't add any code beyond here.  Reacquiring locks after release
	 * it above doesn't guarantee the atomicity that we're aiming for.
	 */
}

static DECLARE_WORK(iris_hfi_work, iris_hfi_core_work_handler);

static irqreturn_t iris_hfi_isr(int irq, void *dev)
{
	struct iris_hfi_device *device = dev;

	disable_irq_nosync(irq);
	queue_work(device->cvp_workq, &iris_hfi_work);
	return IRQ_HANDLED;
}

static int __init_regs_and_interrupts(struct iris_hfi_device *device,
		struct msm_cvp_platform_resources *res)
{
	struct cvp_hal_data *hal = NULL;
	int rc = 0;

	rc = __check_core_registered(device, res->firmware_base,
			(u8 *)(uintptr_t)res->register_base,
			res->register_size, res->irq);
	if (!rc) {
		dprintk(CVP_ERR, "Core present/Already added\n");
		rc = -EEXIST;
		goto err_core_init;
	}

	hal = kzalloc(sizeof(*hal), GFP_KERNEL);
	if (!hal) {
		dprintk(CVP_ERR, "Failed to alloc\n");
		rc = -ENOMEM;
		goto err_core_init;
	}

	hal->irq = res->irq;
	hal->firmware_base = res->firmware_base;
	hal->register_base = devm_ioremap_nocache(&res->pdev->dev,
			res->register_base, res->register_size);
	hal->register_size = res->register_size;
	if (!hal->register_base) {
		dprintk(CVP_ERR,
			"could not map reg addr %pa of size %d\n",
			&res->register_base, res->register_size);
		goto error_irq_fail;
	}

	device->cvp_hal_data = hal;
	rc = request_irq(res->irq, iris_hfi_isr, IRQF_TRIGGER_HIGH,
			"msm_cvp", device);
	if (unlikely(rc)) {
		dprintk(CVP_ERR, "() :request_irq failed\n");
		goto error_irq_fail;
	}

	disable_irq_nosync(res->irq);
	dprintk(CVP_INFO,
		"firmware_base = %pa, register_base = %pa, register_size = %d\n",
		&res->firmware_base, &res->register_base,
		res->register_size);
	return rc;

error_irq_fail:
	kfree(hal);
err_core_init:
	return rc;

}

static inline void __deinit_clocks(struct iris_hfi_device *device)
{
	struct clock_info *cl;

	device->clk_freq = 0;
	iris_hfi_for_each_clock_reverse(device, cl) {
		if (cl->clk) {
			clk_put(cl->clk);
			cl->clk = NULL;
		}
	}
}

static inline int __init_clocks(struct iris_hfi_device *device)
{
	int rc = 0;
	struct clock_info *cl = NULL;

	if (!device) {
		dprintk(CVP_ERR, "Invalid params: %pK\n", device);
		return -EINVAL;
	}

	iris_hfi_for_each_clock(device, cl) {

		dprintk(CVP_DBG, "%s: scalable? %d, count %d\n",
				cl->name, cl->has_scaling, cl->count);
	}

	iris_hfi_for_each_clock(device, cl) {
		if (!cl->clk) {
			cl->clk = clk_get(&device->res->pdev->dev, cl->name);
			if (IS_ERR_OR_NULL(cl->clk)) {
				dprintk(CVP_ERR,
					"Failed to get clock: %s\n", cl->name);
				rc = PTR_ERR(cl->clk) ?: -EINVAL;
				cl->clk = NULL;
				goto err_clk_get;
			}
		}
	}
	device->clk_freq = 0;
	return 0;

err_clk_get:
	__deinit_clocks(device);
	return rc;
}

static int __handle_reset_clk(struct msm_cvp_platform_resources *res,
			int reset_index, enum reset_state state)
{
	int rc = 0;
	struct reset_control *rst;
	struct reset_set *rst_set = &res->reset_set;

	if (!rst_set->reset_tbl)
		return 0;

	rst = rst_set->reset_tbl[reset_index].rst;
	dprintk(CVP_DBG, "reset_clk: name %s reset_state %d rst %pK\n",
		rst_set->reset_tbl[reset_index].name, state, rst);

	switch (state) {
	case INIT:
		if (rst)
			goto skip_reset_init;

		rst = devm_reset_control_get(&res->pdev->dev,
				rst_set->reset_tbl[reset_index].name);
		if (IS_ERR(rst))
			rc = PTR_ERR(rst);

		rst_set->reset_tbl[reset_index].rst = rst;
		break;
	case ASSERT:
		if (!rst) {
			rc = PTR_ERR(rst);
			goto failed_to_reset;
		}

		rc = reset_control_assert(rst);
		break;
	case DEASSERT:
		if (!rst) {
			rc = PTR_ERR(rst);
			goto failed_to_reset;
		}
		rc = reset_control_deassert(rst);
		break;
	default:
		dprintk(CVP_ERR, "Invalid reset request\n");
		if (rc)
			goto failed_to_reset;
	}

	return 0;

skip_reset_init:
failed_to_reset:
	return rc;
}

static inline void __disable_unprepare_clks(struct iris_hfi_device *device)
{
	struct clock_info *cl;
	int rc = 0;

	if (!device) {
		dprintk(CVP_ERR, "Invalid params: %pK\n", device);
		return;
	}

	iris_hfi_for_each_clock_reverse(device, cl) {
		dprintk(CVP_DBG, "Clock: %s disable and unprepare\n",
				cl->name);
		rc = clk_set_flags(cl->clk, CLKFLAG_NORETAIN_PERIPH);
		if (rc) {
			dprintk(CVP_WARN,
				"Failed set flag NORETAIN_PERIPH %s\n",
					cl->name);
		}
		rc = clk_set_flags(cl->clk, CLKFLAG_NORETAIN_MEM);
		if (rc) {
			dprintk(CVP_WARN,
				"Failed set flag NORETAIN_MEM %s\n",
					cl->name);
		}
		clk_disable_unprepare(cl->clk);
	}
}

static int reset_ahb2axi_bridge(struct iris_hfi_device *device)
{
	int rc, i;

	if (!device) {
		dprintk(CVP_ERR, "NULL device\n");
		rc = -EINVAL;
		goto failed_to_reset;
	}

	for (i = 0; i < device->res->reset_set.count; i++) {
		rc = __handle_reset_clk(device->res, i, ASSERT);
		if (rc) {
			dprintk(CVP_ERR,
				"failed to assert reset clocks\n");
			goto failed_to_reset;
		}

		/* wait for deassert */
		usleep_range(150, 250);

		rc = __handle_reset_clk(device->res, i, DEASSERT);
		if (rc) {
			dprintk(CVP_ERR,
				"failed to deassert reset clocks\n");
			goto failed_to_reset;
		}
	}

	return 0;

failed_to_reset:
	return rc;
}

static inline int __prepare_enable_clks(struct iris_hfi_device *device)
{
	struct clock_info *cl = NULL, *cl_fail = NULL;
	int rc = 0, c = 0;

	if (!device) {
		dprintk(CVP_ERR, "Invalid params: %pK\n", device);
		return -EINVAL;
	}

	iris_hfi_for_each_clock(device, cl) {
		/*
		 * For the clocks we control, set the rate prior to preparing
		 * them.  Since we don't really have a load at this point, scale
		 * it to the lowest frequency possible
		 */
		if (cl->has_scaling)
			clk_set_rate(cl->clk, clk_round_rate(cl->clk, 0));

		rc = clk_set_flags(cl->clk, CLKFLAG_RETAIN_PERIPH);
		if (rc) {
			dprintk(CVP_WARN,
				"Failed set flag RETAIN_PERIPH %s\n",
					cl->name);
		}
		rc = clk_set_flags(cl->clk, CLKFLAG_RETAIN_MEM);
		if (rc) {
			dprintk(CVP_WARN,
				"Failed set flag RETAIN_MEM %s\n",
					cl->name);
		}
		rc = clk_prepare_enable(cl->clk);
		if (rc) {
			dprintk(CVP_ERR, "Failed to enable clocks\n");
			cl_fail = cl;
			goto fail_clk_enable;
		}

		c++;
		dprintk(CVP_DBG, "Clock: %s prepared and enabled\n", cl->name);
	}

	return rc;

fail_clk_enable:
	iris_hfi_for_each_clock_reverse_continue(device, cl, c) {
		dprintk(CVP_ERR, "Clock: %s disable and unprepare\n",
			cl->name);
		clk_disable_unprepare(cl->clk);
	}

	return rc;
}

static void __deinit_bus(struct iris_hfi_device *device)
{
	struct bus_info *bus = NULL;

	if (!device)
		return;

	kfree(device->bus_vote.data);
	device->bus_vote = CVP_DEFAULT_BUS_VOTE;

	iris_hfi_for_each_bus_reverse(device, bus) {
#ifdef USE_DEVFREQ_SCALE_BUS
		devfreq_remove_device(bus->devfreq);
		bus->devfreq = NULL;
#endif
		dev_set_drvdata(bus->dev, NULL);

		msm_bus_scale_unregister(bus->client);
		bus->client = NULL;
	}
}

static int __init_bus(struct iris_hfi_device *device)
{
	struct bus_info *bus = NULL;
	int rc = 0;

	if (!device)
		return -EINVAL;

	iris_hfi_for_each_bus(device, bus) {
#ifdef USE_DEVFREQ_SCALE_BUS
		struct devfreq_dev_profile profile = {
			.initial_freq = 0,
			.polling_ms = INT_MAX,
			.freq_table = NULL,
			.max_state = 0,
			.target = __devfreq_target,
			.get_dev_status = __devfreq_get_status,
			.exit = NULL,
		};

		if (!strcmp(bus->governor, "msm-cvp-llcc")) {
			if (msm_cvp_syscache_disable) {
				dprintk(CVP_DBG,
					 "Skipping LLC bus init %s: %s\n",
				bus->name, bus->governor);
				continue;
			}
		}
#endif
		/*
		 * This is stupid, but there's no other easy way to ahold
		 * of struct bus_info in iris_hfi_devfreq_*()
		 */
		WARN(dev_get_drvdata(bus->dev), "%s's drvdata already set\n",
				dev_name(bus->dev));
		dev_set_drvdata(bus->dev, device);

		bus->client = msm_bus_scale_register(bus->master, bus->slave,
				bus->name, false);
		if (IS_ERR_OR_NULL(bus->client)) {
			rc = PTR_ERR(bus->client) ?: -EBADHANDLE;
			dprintk(CVP_ERR, "Failed to register bus %s: %d\n",
					bus->name, rc);
			bus->client = NULL;
			goto err_add_dev;
		}

#ifdef USE_DEVFREQ_SCALE_BUS
		bus->devfreq_prof = profile;
		bus->devfreq = devfreq_add_device(bus->dev,
				&bus->devfreq_prof, bus->governor, NULL);
		if (IS_ERR_OR_NULL(bus->devfreq)) {
			rc = PTR_ERR(bus->devfreq) ?: -EBADHANDLE;
			dprintk(CVP_ERR, _DEVFREQ_FAIL_,
				bus->name, bus->governor, rc);
			bus->devfreq = NULL;
			goto err_add_dev;
		}

		/*
		 * Devfreq starts monitoring immediately, since we are just
		 * initializing stuff at this point, force it to suspend
		 */
		devfreq_suspend_device(bus->devfreq);
#endif
	}

	return 0;

err_add_dev:
	__deinit_bus(device);
	return rc;
}

static void __deinit_regulators(struct iris_hfi_device *device)
{
	struct regulator_info *rinfo = NULL;

	iris_hfi_for_each_regulator_reverse(device, rinfo) {
		if (rinfo->regulator) {
			regulator_put(rinfo->regulator);
			rinfo->regulator = NULL;
		}
	}
}

static int __init_regulators(struct iris_hfi_device *device)
{
	int rc = 0;
	struct regulator_info *rinfo = NULL;

	iris_hfi_for_each_regulator(device, rinfo) {
		rinfo->regulator = regulator_get(&device->res->pdev->dev,
				rinfo->name);
		if (IS_ERR_OR_NULL(rinfo->regulator)) {
			rc = PTR_ERR(rinfo->regulator) ?: -EBADHANDLE;
			dprintk(CVP_ERR, "Failed to get regulator: %s\n",
					rinfo->name);
			rinfo->regulator = NULL;
			goto err_reg_get;
		}
	}

	return 0;

err_reg_get:
	__deinit_regulators(device);
	return rc;
}

static void __deinit_subcaches(struct iris_hfi_device *device)
{
	struct subcache_info *sinfo = NULL;

	if (!device) {
		dprintk(CVP_ERR, "deinit_subcaches: invalid device %pK\n",
			device);
		goto exit;
	}

	if (!is_sys_cache_present(device))
		goto exit;

	iris_hfi_for_each_subcache_reverse(device, sinfo) {
		if (sinfo->subcache) {
			dprintk(CVP_DBG, "deinit_subcaches: %s\n",
				sinfo->name);
			llcc_slice_putd(sinfo->subcache);
			sinfo->subcache = NULL;
		}
	}

exit:
	return;
}

static int __init_subcaches(struct iris_hfi_device *device)
{
	int rc = 0;
	struct subcache_info *sinfo = NULL;

	if (!device) {
		dprintk(CVP_ERR, "init_subcaches: invalid device %pK\n",
			device);
		return -EINVAL;
	}

	if (!is_sys_cache_present(device))
		return 0;

	iris_hfi_for_each_subcache(device, sinfo) {
		if (!strcmp("cvp", sinfo->name)) {
			sinfo->subcache = llcc_slice_getd(LLCC_CVP);
		} else if (!strcmp("cvpfw", sinfo->name)) {
			sinfo->subcache = llcc_slice_getd(LLCC_CVPFW);
		} else {
			dprintk(CVP_ERR, "Invalid subcache name %s\n",
					sinfo->name);
		}
		if (IS_ERR_OR_NULL(sinfo->subcache)) {
			rc = PTR_ERR(sinfo->subcache) ?
				PTR_ERR(sinfo->subcache) : -EBADHANDLE;
			dprintk(CVP_ERR,
				 "init_subcaches: invalid subcache: %s rc %d\n",
				sinfo->name, rc);
			sinfo->subcache = NULL;
			goto err_subcache_get;
		}
		dprintk(CVP_DBG, "init_subcaches: %s\n",
			sinfo->name);
	}

	return 0;

err_subcache_get:
	__deinit_subcaches(device);
	return rc;
}

static int __init_resources(struct iris_hfi_device *device,
				struct msm_cvp_platform_resources *res)
{
	int i, rc = 0;

	rc = __init_regulators(device);
	if (rc) {
		dprintk(CVP_ERR, "Failed to get all regulators\n");
		return -ENODEV;
	}

	rc = __init_clocks(device);
	if (rc) {
		dprintk(CVP_ERR, "Failed to init clocks\n");
		rc = -ENODEV;
		goto err_init_clocks;
	}

	for (i = 0; i < device->res->reset_set.count; i++) {
		rc = __handle_reset_clk(res, i, INIT);
		if (rc) {
			dprintk(CVP_ERR, "Failed to init reset clocks\n");
			rc = -ENODEV;
			goto err_init_reset_clk;
		}
	}

	rc = __init_bus(device);
	if (rc) {
		dprintk(CVP_ERR, "Failed to init bus: %d\n", rc);
		goto err_init_bus;
	}

	rc = __init_subcaches(device);
	if (rc)
		dprintk(CVP_WARN, "Failed to init subcaches: %d\n", rc);

	device->sys_init_capabilities =
		kzalloc(sizeof(struct msm_cvp_capability)
		* CVP_MAX_SESSIONS, GFP_KERNEL);

	return rc;

err_init_reset_clk:
err_init_bus:
	__deinit_clocks(device);
err_init_clocks:
	__deinit_regulators(device);
	return rc;
}

static void __deinit_resources(struct iris_hfi_device *device)
{
	__deinit_subcaches(device);
	__deinit_bus(device);
	__deinit_clocks(device);
	__deinit_regulators(device);
	kfree(device->sys_init_capabilities);
	device->sys_init_capabilities = NULL;
}

static int __protect_cp_mem(struct iris_hfi_device *device)
{
	struct cvp_tzbsp_memprot memprot;
	unsigned int resp = 0;
	int rc = 0;
	struct context_bank_info *cb;
	struct scm_desc desc = {0};

	if (!device)
		return -EINVAL;

	memprot.cp_start = 0x0;
	memprot.cp_size = 0x0;
	memprot.cp_nonpixel_start = 0x0;
	memprot.cp_nonpixel_size = 0x0;

	list_for_each_entry(cb, &device->res->context_banks, list) {
		if (!strcmp(cb->name, "cvp_hlos")) {
			desc.args[1] = memprot.cp_size =
				cb->addr_range.start;
			dprintk(CVP_DBG, "%s memprot.cp_size: %#x\n",
				__func__, memprot.cp_size);
		}

		if (!strcmp(cb->name, "cvp_sec_nonpixel")) {
			desc.args[2] = memprot.cp_nonpixel_start =
				cb->addr_range.start;
			desc.args[3] = memprot.cp_nonpixel_size =
				cb->addr_range.size;
			dprintk(CVP_DBG,
				"%s memprot.cp_nonpixel_start: %#x size: %#x\n",
				__func__, memprot.cp_nonpixel_start,
				memprot.cp_nonpixel_size);
		}
	}

	desc.arginfo = SCM_ARGS(4);
	rc = 0;
	resp = desc.ret[0];

	if (rc) {
		dprintk(CVP_ERR, "Failed to protect memory(%d) response: %d\n",
				rc, resp);
	}

	return rc;
}

static int __disable_regulator(struct regulator_info *rinfo,
				struct iris_hfi_device *device)
{
	int rc = 0;

	dprintk(CVP_DBG, "Disabling regulator %s\n", rinfo->name);

	/*
	 * This call is needed. Driver needs to acquire the control back
	 * from HW in order to disable the regualtor. Else the behavior
	 * is unknown.
	 */

	rc = __acquire_regulator(rinfo, device);
	if (rc) {
		/*
		 * This is somewhat fatal, but nothing we can do
		 * about it. We can't disable the regulator w/o
		 * getting it back under s/w control
		 */
		dprintk(CVP_WARN,
			"Failed to acquire control on %s\n",
			rinfo->name);

		goto disable_regulator_failed;
	}

	rc = regulator_disable(rinfo->regulator);
	if (rc) {
		dprintk(CVP_WARN,
			"Failed to disable %s: %d\n",
			rinfo->name, rc);
		goto disable_regulator_failed;
	}

	return 0;
disable_regulator_failed:

	/* Bring attention to this issue */
	msm_cvp_res_handle_fatal_hw_error(device->res, true);
	return rc;
}

static int __enable_hw_power_collapse(struct iris_hfi_device *device)
{
	int rc = 0;

	if (!msm_cvp_fw_low_power_mode) {
		dprintk(CVP_DBG, "Not enabling hardware power collapse\n");
		return 0;
	}

	rc = __hand_off_regulators(device);
	if (rc)
		dprintk(CVP_WARN,
			"%s : Failed to enable HW power collapse %d\n",
				__func__, rc);
	return rc;
}

static int __enable_regulators(struct iris_hfi_device *device)
{
	int rc = 0, c = 0;
	struct regulator_info *rinfo;

	dprintk(CVP_DBG, "Enabling regulators\n");

	iris_hfi_for_each_regulator(device, rinfo) {
		rc = regulator_enable(rinfo->regulator);
		if (rc) {
			dprintk(CVP_ERR, "Failed to enable %s: %d\n",
					rinfo->name, rc);
			goto err_reg_enable_failed;
		}

		dprintk(CVP_DBG, "Enabled regulator %s\n", rinfo->name);
		c++;
	}

	return 0;

err_reg_enable_failed:
	iris_hfi_for_each_regulator_reverse_continue(device, rinfo, c)
		__disable_regulator(rinfo, device);

	return rc;
}

static int __disable_regulators(struct iris_hfi_device *device)
{
	struct regulator_info *rinfo;

	dprintk(CVP_DBG, "Disabling regulators\n");

	iris_hfi_for_each_regulator_reverse(device, rinfo) {
		__disable_regulator(rinfo, device);
		if (rinfo->has_hw_power_collapse)
			regulator_set_mode(rinfo->regulator,
				REGULATOR_MODE_NORMAL);
	}

	return 0;
}

static int __enable_subcaches(struct iris_hfi_device *device)
{
	int rc = 0;
	u32 c = 0;
	struct subcache_info *sinfo;

	if (msm_cvp_syscache_disable || !is_sys_cache_present(device))
		return 0;

	/* Activate subcaches */
	iris_hfi_for_each_subcache(device, sinfo) {
		rc = llcc_slice_activate(sinfo->subcache);
		if (rc) {
			dprintk(CVP_WARN, "Failed to activate %s: %d\n",
				sinfo->name, rc);
			msm_cvp_res_handle_fatal_hw_error(device->res, true);
			goto err_activate_fail;
		}
		sinfo->isactive = true;
		dprintk(CVP_DBG, "Activated subcache %s\n", sinfo->name);
		c++;
	}

	dprintk(CVP_DBG, "Activated %d Subcaches to CVP\n", c);

	return 0;

err_activate_fail:
	__release_subcaches(device);
	__disable_subcaches(device);
	return 0;
}

static int __set_subcaches(struct iris_hfi_device *device)
{
	int rc = 0;
	u32 c = 0;
	struct subcache_info *sinfo;
	u32 resource[CVP_MAX_SUBCACHE_SIZE];
	struct cvp_hfi_resource_syscache_info_type *sc_res_info;
	struct cvp_hfi_resource_subcache_type *sc_res;
	struct cvp_resource_hdr rhdr;

	if (device->res->sys_cache_res_set) {
		dprintk(CVP_DBG, "Subcaches already set to CVP\n");
		return 0;
	}

	memset((void *)resource, 0x0, (sizeof(u32) * CVP_MAX_SUBCACHE_SIZE));

	sc_res_info = (struct cvp_hfi_resource_syscache_info_type *)resource;
	sc_res = &(sc_res_info->rg_subcache_entries[0]);

	iris_hfi_for_each_subcache(device, sinfo) {
		if (sinfo->isactive) {
			sc_res[c].size = sinfo->subcache->slice_size;
			sc_res[c].sc_id = sinfo->subcache->slice_id;
			c++;
		}
	}

	/* Set resource to CVP for activated subcaches */
	if (c) {
		dprintk(CVP_DBG, "Setting %d Subcaches\n", c);

		rhdr.resource_handle = sc_res_info; /* cookie */
		rhdr.resource_id = CVP_RESOURCE_SYSCACHE;

		sc_res_info->num_entries = c;

		rc = __core_set_resource(device, &rhdr, (void *)sc_res_info);
		if (rc) {
			dprintk(CVP_WARN, "Failed to set subcaches %d\n", rc);
			goto err_fail_set_subacaches;
		}

		iris_hfi_for_each_subcache(device, sinfo) {
			if (sinfo->isactive)
				sinfo->isset = true;
		}

		dprintk(CVP_DBG, "Set Subcaches done to CVP\n");
		device->res->sys_cache_res_set = true;
	}

	return 0;

err_fail_set_subacaches:
	__disable_subcaches(device);

	return 0;
}

static int __release_subcaches(struct iris_hfi_device *device)
{
	struct subcache_info *sinfo;
	int rc = 0;
	u32 c = 0;
	u32 resource[CVP_MAX_SUBCACHE_SIZE];
	struct cvp_hfi_resource_syscache_info_type *sc_res_info;
	struct cvp_hfi_resource_subcache_type *sc_res;
	struct cvp_resource_hdr rhdr;

	if (msm_cvp_syscache_disable || !is_sys_cache_present(device))
		return 0;

	memset((void *)resource, 0x0, (sizeof(u32) * CVP_MAX_SUBCACHE_SIZE));

	sc_res_info = (struct cvp_hfi_resource_syscache_info_type *)resource;
	sc_res = &(sc_res_info->rg_subcache_entries[0]);

	/* Release resource command to Iris */
	iris_hfi_for_each_subcache_reverse(device, sinfo) {
		if (sinfo->isset) {
			/* Update the entry */
			sc_res[c].size = sinfo->subcache->slice_size;
			sc_res[c].sc_id = sinfo->subcache->slice_id;
			c++;
			sinfo->isset = false;
		}
	}

	if (c > 0) {
		dprintk(CVP_DBG, "Releasing %d subcaches\n", c);
		rhdr.resource_handle = sc_res_info; /* cookie */
		rhdr.resource_id = CVP_RESOURCE_SYSCACHE;

		rc = __core_release_resource(device, &rhdr);
		if (rc)
			dprintk(CVP_WARN,
				"Failed to release %d subcaches\n", c);
	}

	device->res->sys_cache_res_set = false;

	return 0;
}

static int __disable_subcaches(struct iris_hfi_device *device)
{
	struct subcache_info *sinfo;
	int rc = 0;

	if (msm_cvp_syscache_disable || !is_sys_cache_present(device))
		return 0;

	/* De-activate subcaches */
	iris_hfi_for_each_subcache_reverse(device, sinfo) {
		if (sinfo->isactive) {
			dprintk(CVP_DBG, "De-activate subcache %s\n",
				sinfo->name);
			rc = llcc_slice_deactivate(sinfo->subcache);
			if (rc) {
				dprintk(CVP_WARN,
					"Failed to de-activate %s: %d\n",
					sinfo->name, rc);
			}
			sinfo->isactive = false;
		}
	}

	return 0;
}

static void interrupt_init_iris2(struct iris_hfi_device *device)
{
	u32 mask_val = 0;

	/* All interrupts should be disabled initially 0x1F6 : Reset value */
	mask_val = __read_register(device, CVP_WRAPPER_INTR_MASK);

	/* Write 0 to unmask CPU and WD interrupts */
	mask_val &= ~(CVP_FATAL_INTR_BMSK | CVP_WRAPPER_INTR_MASK_A2HCPU_BMSK);
	__write_register(device, CVP_WRAPPER_INTR_MASK, mask_val);
	dprintk(CVP_DBG, "Init irq: reg: %x, mask value %x\n",
		CVP_WRAPPER_INTR_MASK, mask_val);
}

static void setup_dsp_uc_memmap_vpu5(struct iris_hfi_device *device)
{
	/* initialize DSP QTBL & UCREGION with CPU queues */
	__write_register(device, HFI_DSP_QTBL_ADDR,
		(u32)device->dsp_iface_q_table.align_device_addr);
	__write_register(device, HFI_DSP_UC_REGION_ADDR,
		(u32)device->dsp_iface_q_table.align_device_addr);
	__write_register(device, HFI_DSP_UC_REGION_SIZE,
		device->dsp_iface_q_table.mem_data.size);
}

static void clock_config_on_enable_vpu5(struct iris_hfi_device *device)
{
		__write_register(device, CVP_WRAPPER_CPU_CLOCK_CONFIG, 0);
}

static int __set_ubwc_config(struct iris_hfi_device *device)
{
	u8 packet[CVP_IFACEQ_VAR_SMALL_PKT_SIZE];
	int rc = 0;

	struct cvp_hfi_cmd_sys_set_property_packet *pkt =
		(struct cvp_hfi_cmd_sys_set_property_packet *) &packet;

	if (!device->res->ubwc_config)
		return 0;

	rc = call_hfi_pkt_op(device, sys_ubwc_config, pkt,
		device->res->ubwc_config);
	if (rc) {
		dprintk(CVP_WARN,
			"ubwc config setting to FW failed\n");
		rc = -ENOTEMPTY;
		goto fail_to_set_ubwc_config;
	}

	if (__iface_cmdq_write(device, pkt)) {
		rc = -ENOTEMPTY;
		goto fail_to_set_ubwc_config;
	}

fail_to_set_ubwc_config:
	return rc;
}

static int __iris_power_on(struct iris_hfi_device *device)
{
	int rc = 0;


	if (device->power_enabled)
		return 0;

	device->power_enabled = true;
	/* Vote for all hardware resources */
	rc = __vote_buses(device, device->bus_vote.data,
			device->bus_vote.data_count);
	if (rc) {
		dprintk(CVP_ERR, "Failed to vote buses, err: %d\n", rc);
		goto fail_vote_buses;
	}

	rc = __enable_regulators(device);
	if (rc) {
		dprintk(CVP_ERR, "Failed to enable GDSC, err = %d\n", rc);
		goto fail_enable_gdsc;
	}

	rc = call_iris_op(device, reset_ahb2axi_bridge, device);
	if (rc) {
		dprintk(CVP_ERR, "Failed to reset ahb2axi: %d\n", rc);
		goto fail_enable_clks;
	}

	rc = __prepare_enable_clks(device);
	if (rc) {
		dprintk(CVP_ERR, "Failed to enable clocks: %d\n", rc);
		goto fail_enable_clks;
	}

	rc = __scale_clocks(device);
	if (rc) {
		dprintk(CVP_WARN,
			"Failed to scale clocks, perf may regress\n");
		rc = 0;
	}

	dprintk(CVP_DBG, "Done with scaling\n");
	/*
	 * Re-program all of the registers that get reset as a result of
	 * regulator_disable() and _enable()
	 */
	__set_registers(device);

	dprintk(CVP_DBG, "Done with register set\n");
	call_iris_op(device, interrupt_init, device);
	dprintk(CVP_DBG, "Done with interrupt enabling\n");
	device->intr_status = 0;
	enable_irq(device->cvp_hal_data->irq);

	/*
	 * Hand off control of regulators to h/w _after_ enabling clocks.
	 * Note that the GDSC will turn off when switching from normal
	 * (s/w triggered) to fast (HW triggered) unless the h/w vote is
	 * present. Since Iris isn't up yet, the GDSC will be off briefly.
	 */
	if (__enable_hw_power_collapse(device))
		dprintk(CVP_ERR, "Failed to enabled inter-frame PC\n");

	return rc;

fail_enable_clks:
	__disable_regulators(device);
fail_enable_gdsc:
	__unvote_buses(device);
fail_vote_buses:
	device->power_enabled = false;
	return rc;
}

void power_off_common(struct iris_hfi_device *device)
{
	if (!device->power_enabled)
		return;

	if (!(device->intr_status & CVP_WRAPPER_INTR_STATUS_A2HWD_BMSK))
		disable_irq_nosync(device->cvp_hal_data->irq);
	device->intr_status = 0;

	__disable_unprepare_clks(device);
	if (__disable_regulators(device))
		dprintk(CVP_WARN, "Failed to disable regulators\n");

	if (__unvote_buses(device))
		dprintk(CVP_WARN, "Failed to unvote for buses\n");
	device->power_enabled = false;
}

static inline int __suspend(struct iris_hfi_device *device)
{
	int rc = 0;

	if (!device) {
		dprintk(CVP_ERR, "Invalid params: %pK\n", device);
		return -EINVAL;
	} else if (!device->power_enabled) {
		dprintk(CVP_DBG, "Power already disabled\n");
		return 0;
	}

	dprintk(CVP_PROF, "Entering suspend\n");

	if (device->res->pm_qos_latency_us &&
		pm_qos_request_active(&device->qos))
		pm_qos_remove_request(&device->qos);

	rc = __tzbsp_set_cvp_state(TZ_SUBSYS_STATE_SUSPEND);
	if (rc) {
		dprintk(CVP_WARN, "Failed to suspend cvp core %d\n", rc);
		goto err_tzbsp_suspend;
	}

	__disable_subcaches(device);

	call_iris_op(device, power_off, device);
	dprintk(CVP_PROF, "Iris power off\n");
	return rc;

err_tzbsp_suspend:
	return rc;
}

static void power_off_iris2(struct iris_hfi_device *device)
{
	u32 lpi_status, reg_status = 0, count = 0, max_count = 1000;
	u32 pc_ready, wfi_status, sbm_ln0_low;
	u32 main_sbm_ln0_low, main_sbm_ln1_high;

	if (!device->power_enabled)
		return;

	if (!(device->intr_status & CVP_WRAPPER_INTR_STATUS_A2HWD_BMSK))
		disable_irq_nosync(device->cvp_hal_data->irq);
	device->intr_status = 0;

	/* HPG 6.1.2 Step 1  */
	__write_register(device, CVP_CPU_CS_X2RPMh, 0x3);

	/* HPG 6.1.2 Step 2, noc to low power */
	__write_register(device, CVP_AON_WRAPPER_MVP_NOC_LPI_CONTROL, 0x1);
	while (!reg_status && count < max_count) {
		lpi_status =
			 __read_register(device,
				CVP_AON_WRAPPER_MVP_NOC_LPI_STATUS);
		reg_status = lpi_status & BIT(0);
		/* Wait for noc lpi status to be set */
		usleep_range(50, 100);
		count++;
	}
	dprintk(CVP_DBG,
		"Noc: lpi_status %x noc_status %x (count %d)\n",
		lpi_status, reg_status, count);
	if (count == max_count) {
		wfi_status = __read_register(device, CVP_WRAPPER_CPU_STATUS);
		pc_ready = __read_register(device, CVP_CTRL_STATUS);
		sbm_ln0_low =
			__read_register(device, CVP_NOC_SBM_SENSELN0_LOW);
		main_sbm_ln0_low = __read_register(device,
				CVP_NOC_MAIN_SIDEBANDMANAGER_SENSELN0_LOW);
		main_sbm_ln1_high = __read_register(device,
				CVP_NOC_MAIN_SIDEBANDMANAGER_SENSELN1_HIGH);
		dprintk(CVP_WARN,
			"NOC not in qaccept status %x %x %x %x %x %x %x\n",
			reg_status, lpi_status, wfi_status, pc_ready,
			sbm_ln0_low, main_sbm_ln0_low, main_sbm_ln1_high);
	}

	/* HPG 6.1.2 Step 3, debug bridge to low power */
	__write_register(device,
		CVP_WRAPPER_DEBUG_BRIDGE_LPI_CONTROL, 0x7);
	reg_status = 0;
	count = 0;
	while ((reg_status != 0x7) && count < max_count) {
		lpi_status = __read_register(device,
				 CVP_WRAPPER_DEBUG_BRIDGE_LPI_STATUS);
		reg_status = lpi_status & 0x7;
		/* Wait for debug bridge lpi status to be set */
		usleep_range(50, 100);
		count++;
	}
	dprintk(CVP_DBG,
		"DBLP Set : lpi_status %d reg_status %d (count %d)\n",
		lpi_status, reg_status, count);
	if (count == max_count) {
		dprintk(CVP_WARN,
			"DBLP Set: status %x %x\n", reg_status, lpi_status);
	}

	/* HPG 6.1.2 Step 4, debug bridge to lpi release */
	__write_register(device,
		CVP_WRAPPER_DEBUG_BRIDGE_LPI_CONTROL, 0x0);
	lpi_status = 0x1;
	count = 0;
	while (lpi_status && count < max_count) {
		lpi_status = __read_register(device,
				 CVP_WRAPPER_DEBUG_BRIDGE_LPI_STATUS);
		usleep_range(50, 100);
		count++;
	}
	dprintk(CVP_DBG,
		"DBLP Release: lpi_status %d(count %d)\n",
		lpi_status, count);
	if (count == max_count) {
		dprintk(CVP_WARN,
			"DBLP Release: lpi_status %x\n", lpi_status);
	}

	/* HPG 6.1.2 Step 6 */
	__disable_unprepare_clks(device);

	/* HPG 6.1.2 Step 7 & 8 */
	if (call_iris_op(device, reset_ahb2axi_bridge, device))
		dprintk(CVP_ERR, "Failed to reset ahb2axi\n");

	/* HPG 6.1.2 Step 5 */
	if (__disable_regulators(device))
		dprintk(CVP_WARN, "Failed to disable regulators\n");

	if (__unvote_buses(device))
		dprintk(CVP_WARN, "Failed to unvote for buses\n");
	device->power_enabled = false;
}

static inline int __resume(struct iris_hfi_device *device)
{
	int rc = 0;
	u32 flags = 0;

	if (!device) {
		dprintk(CVP_ERR, "Invalid params: %pK\n", device);
		return -EINVAL;
	} else if (device->power_enabled) {
		goto exit;
	} else if (!__core_in_valid_state(device)) {
		dprintk(CVP_DBG, "iris_hfi_device in deinit state.");
		return -EINVAL;
	}

	dprintk(CVP_PROF, "Resuming from power collapse\n");
	rc = __iris_power_on(device);
	if (rc) {
		dprintk(CVP_ERR, "Failed to power on cvp\n");
		goto err_iris_power_on;
	}

	/* Reboot the firmware */
	rc = __tzbsp_set_cvp_state(TZ_SUBSYS_STATE_RESUME);
	if (rc) {
		dprintk(CVP_ERR, "Failed to resume cvp core %d\n", rc);
		goto err_set_cvp_state;
	}

	__setup_ucregion_memory_map(device);
	/* Wait for boot completion */
	rc = __boot_firmware(device);
	if (rc) {
		dprintk(CVP_ERR, "Failed to reset cvp core\n");
		goto err_reset_core;
	}

	/*
	 * Work around for H/W bug, need to reprogram these registers once
	 * firmware is out reset
	 */
	__set_threshold_registers(device);

	if (device->res->pm_qos_latency_us) {
#ifdef CONFIG_SMP
		device->qos.type = PM_QOS_REQ_AFFINE_IRQ;
		device->qos.irq = device->cvp_hal_data->irq;
#endif
		pm_qos_add_request(&device->qos, PM_QOS_CPU_DMA_LATENCY,
				device->res->pm_qos_latency_us);
	}

	__sys_set_debug(device, msm_cvp_fw_debug);

	__enable_subcaches(device);
	__set_subcaches(device);


	__dsp_resume(device, flags);

	dprintk(CVP_PROF, "Resumed from power collapse\n");
exit:
	/* Don't reset skip_pc_count for SYS_PC_PREP cmd */
	if (device->last_packet_type != HFI_CMD_SYS_PC_PREP)
		device->skip_pc_count = 0;
	return rc;
err_reset_core:
	__tzbsp_set_cvp_state(TZ_SUBSYS_STATE_SUSPEND);
err_set_cvp_state:
	call_iris_op(device, power_off, device);
err_iris_power_on:
	dprintk(CVP_ERR, "Failed to resume from power collapse\n");
	return rc;
}

static int __load_fw(struct iris_hfi_device *device)
{
	int rc = 0;

	/* Initialize resources */
	rc = __init_resources(device, device->res);
	if (rc) {
		dprintk(CVP_ERR, "Failed to init resources: %d\n", rc);
		goto fail_init_res;
	}

	rc = __initialize_packetization(device);
	if (rc) {
		dprintk(CVP_ERR, "Failed to initialize packetization\n");
		goto fail_init_pkt;
	}
	trace_msm_v4l2_cvp_fw_load_start("msm_v4l2_cvp cvp fw load start");

	rc = __iris_power_on(device);
	if (rc) {
		dprintk(CVP_ERR, "Failed to power on iris in in load_fw\n");
		goto fail_iris_power_on;
	}

	if ((!device->res->use_non_secure_pil && !device->res->firmware_base)
			|| device->res->use_non_secure_pil) {
		if (!device->resources.fw.cookie)
			device->resources.fw.cookie =
				subsystem_get_with_fwname("cvpss",
				device->res->fw_name);

		if (IS_ERR_OR_NULL(device->resources.fw.cookie)) {
			dprintk(CVP_ERR, "Failed to download firmware\n");
			device->resources.fw.cookie = NULL;
			rc = -ENOMEM;
			goto fail_load_fw;
		}
	}

	if (!device->res->firmware_base) {
		rc = __protect_cp_mem(device);
		if (rc) {
			dprintk(CVP_ERR, "Failed to protect memory\n");
			goto fail_protect_mem;
		}
	}
	trace_msm_v4l2_cvp_fw_load_end("msm_v4l2_cvp cvp fw load end");
	return rc;
fail_protect_mem:
	if (device->resources.fw.cookie)
		subsystem_put(device->resources.fw.cookie);
	device->resources.fw.cookie = NULL;
fail_load_fw:
	call_iris_op(device, power_off, device);
fail_iris_power_on:
fail_init_pkt:
	__deinit_resources(device);
fail_init_res:
	trace_msm_v4l2_cvp_fw_load_end("msm_v4l2_cvp cvp fw load end");
	return rc;
}

static void __unload_fw(struct iris_hfi_device *device)
{
	if (!device->resources.fw.cookie)
		return;

	cancel_delayed_work(&iris_hfi_pm_work);
	if (device->state != IRIS_STATE_DEINIT)
		flush_workqueue(device->iris_pm_workq);

	subsystem_put(device->resources.fw.cookie);
	__interface_queues_release(device);
	call_iris_op(device, power_off, device);
	device->resources.fw.cookie = NULL;
	__deinit_resources(device);

	dprintk(CVP_WARN, "Firmware unloaded\n");
}

static int iris_hfi_get_fw_info(void *dev, struct cvp_hal_fw_info *fw_info)
{
	int i = 0, j = 0;
	struct iris_hfi_device *device = dev;
	size_t smem_block_size = 0;
	u8 *smem_table_ptr;
	char version[CVP_VERSION_LENGTH] = "";
	const u32 smem_image_index = 14 * 128;

	if (!device || !fw_info) {
		dprintk(CVP_ERR,
			"%s Invalid parameter: device = %pK fw_info = %pK\n",
			__func__, device, fw_info);
		return -EINVAL;
	}

	mutex_lock(&device->lock);

	smem_table_ptr = qcom_smem_get(QCOM_SMEM_HOST_ANY,
			SMEM_IMAGE_VERSION_TABLE, &smem_block_size);
	if (smem_table_ptr &&
			((smem_image_index +
			  CVP_VERSION_LENGTH) <= smem_block_size))
		memcpy(version,
			smem_table_ptr + smem_image_index,
			CVP_VERSION_LENGTH);

	while (version[i++] != 'V' && i < CVP_VERSION_LENGTH)
		;

	if (i == CVP_VERSION_LENGTH - 1) {
		dprintk(CVP_WARN, "Iris version string is not proper\n");
		fw_info->version[0] = '\0';
		goto fail_version_string;
	}

	for (i--; i < CVP_VERSION_LENGTH && j < CVP_VERSION_LENGTH - 1; i++)
		fw_info->version[j++] = version[i];
	fw_info->version[j] = '\0';

fail_version_string:
	dprintk(CVP_DBG, "F/W version retrieved : %s\n", fw_info->version);
	fw_info->base_addr = device->cvp_hal_data->firmware_base;
	fw_info->register_base = device->res->register_base;
	fw_info->register_size = device->cvp_hal_data->register_size;
	fw_info->irq = device->cvp_hal_data->irq;

	mutex_unlock(&device->lock);
	return 0;
}

static int iris_hfi_get_core_capabilities(void *dev)
{
	dprintk(CVP_DBG, "%s not supported yet!\n", __func__);
	return 0;
}

static void __noc_error_info_iris2(struct iris_hfi_device *device)
{
	u32 val = 0;

	val = __read_register(device, CVP_NOC_ERR_SWID_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERL_MAIN_SWID_LOW:     %#x\n", val);
	val = __read_register(device, CVP_NOC_ERR_SWID_HIGH_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERL_MAIN_SWID_HIGH:     %#x\n", val);
	val = __read_register(device, CVP_NOC_ERR_MAINCTL_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERL_MAIN_MAINCTL_LOW:     %#x\n", val);
	val = __read_register(device, CVP_NOC_ERR_ERRVLD_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERL_MAIN_ERRVLD_LOW:     %#x\n", val);
	val = __read_register(device, CVP_NOC_ERR_ERRCLR_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERL_MAIN_ERRCLR_LOW:     %#x\n", val);
	val = __read_register(device, CVP_NOC_ERR_ERRLOG0_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERL_MAIN_ERRLOG0_LOW:     %#x\n", val);
	val = __read_register(device, CVP_NOC_ERR_ERRLOG0_HIGH_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERL_MAIN_ERRLOG0_HIGH:     %#x\n", val);
	val = __read_register(device, CVP_NOC_ERR_ERRLOG1_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERL_MAIN_ERRLOG1_LOW:     %#x\n", val);
	val = __read_register(device, CVP_NOC_ERR_ERRLOG1_HIGH_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERL_MAIN_ERRLOG1_HIGH:     %#x\n", val);
	val = __read_register(device, CVP_NOC_ERR_ERRLOG2_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERL_MAIN_ERRLOG2_LOW:     %#x\n", val);
	val = __read_register(device, CVP_NOC_ERR_ERRLOG2_HIGH_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERL_MAIN_ERRLOG2_HIGH:     %#x\n", val);
	val = __read_register(device, CVP_NOC_ERR_ERRLOG3_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERL_MAIN_ERRLOG3_LOW:     %#x\n", val);
	val = __read_register(device, CVP_NOC_ERR_ERRLOG3_HIGH_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERL_MAIN_ERRLOG3_HIGH:     %#x\n", val);

	val = __read_register(device, CVP_NOC_CORE_ERR_SWID_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC__CORE_ERL_MAIN_SWID_LOW:     %#x\n", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_SWID_HIGH_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_CORE_ERL_MAIN_SWID_HIGH:     %#x\n", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_MAINCTL_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_CORE_ERL_MAIN_MAINCTL_LOW:     %#x\n", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRVLD_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_CORE_ERL_MAIN_ERRVLD_LOW:     %#x\n", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRCLR_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_CORE_ERL_MAIN_ERRCLR_LOW:     %#x\n", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRLOG0_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_CORE_ERL_MAIN_ERRLOG0_LOW:     %#x\n", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRLOG0_HIGH_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_CORE_ERL_MAIN_ERRLOG0_HIGH:     %#x\n", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRLOG1_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_CORE_ERL_MAIN_ERRLOG1_LOW:     %#x\n", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRLOG1_HIGH_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_CORE_ERL_MAIN_ERRLOG1_HIGH:     %#x\n", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRLOG2_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_CORE_ERL_MAIN_ERRLOG2_LOW:     %#x\n", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRLOG2_HIGH_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_CORE_ERL_MAIN_ERRLOG2_HIGH:     %#x\n", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRLOG3_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_CORE_ERL_MAIN_ERRLOG3_LOW:     %#x\n", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRLOG3_HIGH_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_CORE_ERL_MAIN_ERRLOG3_HIGH:     %#x\n", val);
}

static int iris_hfi_noc_error_info(void *dev)
{
	struct iris_hfi_device *device;

	if (!dev) {
		dprintk(CVP_ERR, "%s: null device\n", __func__);
		return -EINVAL;
	}
	device = dev;

	mutex_lock(&device->lock);
	dprintk(CVP_ERR, "%s: non error information\n", __func__);

	call_iris_op(device, noc_error_info, device);

	mutex_unlock(&device->lock);

	return 0;
}

static int __initialize_packetization(struct iris_hfi_device *device)
{
	int rc = 0;

	if (!device || !device->res) {
		dprintk(CVP_ERR, "%s - invalid param\n", __func__);
		return -EINVAL;
	}

	device->packetization_type = HFI_PACKETIZATION_4XX;

	device->pkt_ops = cvp_hfi_get_pkt_ops_handle(
		device->packetization_type);
	if (!device->pkt_ops) {
		rc = -EINVAL;
		dprintk(CVP_ERR, "Failed to get pkt_ops handle\n");
	}

	return rc;
}

void __init_cvp_ops(struct iris_hfi_device *device)
{
	device->vpu_ops = &iris2_ops;
}

static struct iris_hfi_device *__add_device(u32 device_id,
			struct msm_cvp_platform_resources *res,
			hfi_cmd_response_callback callback)
{
	struct iris_hfi_device *hdevice = NULL;
	int rc = 0;

	if (!res || !callback) {
		dprintk(CVP_ERR, "Invalid Parameters\n");
		return NULL;
	}

	dprintk(CVP_INFO, "%s: device_id: %d\n", __func__, device_id);

	hdevice = kzalloc(sizeof(*hdevice), GFP_KERNEL);
	if (!hdevice) {
		dprintk(CVP_ERR, "failed to allocate new device\n");
		goto exit;
	}

	hdevice->response_pkt = kmalloc_array(cvp_max_packets,
				sizeof(*hdevice->response_pkt), GFP_KERNEL);
	if (!hdevice->response_pkt) {
		dprintk(CVP_ERR, "failed to allocate response_pkt\n");
		goto err_cleanup;
	}

	hdevice->raw_packet =
		kzalloc(CVP_IFACEQ_VAR_HUGE_PKT_SIZE, GFP_KERNEL);
	if (!hdevice->raw_packet) {
		dprintk(CVP_ERR, "failed to allocate raw packet\n");
		goto err_cleanup;
	}

	rc = __init_regs_and_interrupts(hdevice, res);
	if (rc)
		goto err_cleanup;

	hdevice->res = res;
	hdevice->device_id = device_id;
	hdevice->callback = callback;

	__init_cvp_ops(hdevice);

	hdevice->cvp_workq = create_singlethread_workqueue(
		"msm_cvp_workerq_iris");
	if (!hdevice->cvp_workq) {
		dprintk(CVP_ERR, ": create cvp workq failed\n");
		goto err_cleanup;
	}

	hdevice->iris_pm_workq = create_singlethread_workqueue(
			"pm_workerq_iris");
	if (!hdevice->iris_pm_workq) {
		dprintk(CVP_ERR, ": create pm workq failed\n");
		goto err_cleanup;
	}

	mutex_init(&hdevice->lock);
	INIT_LIST_HEAD(&hdevice->sess_head);

	INIT_DELAYED_WORK(&hdevice->dsp_init_work, dsp_init_work_handler);

	return hdevice;

err_cleanup:
	if (hdevice->iris_pm_workq)
		destroy_workqueue(hdevice->iris_pm_workq);
	if (hdevice->cvp_workq)
		destroy_workqueue(hdevice->cvp_workq);
	kfree(hdevice->response_pkt);
	kfree(hdevice->raw_packet);
	kfree(hdevice);
exit:
	return NULL;
}

static struct iris_hfi_device *__get_device(u32 device_id,
				struct msm_cvp_platform_resources *res,
				hfi_cmd_response_callback callback)
{
	if (!res || !callback) {
		dprintk(CVP_ERR, "Invalid params: %pK %pK\n", res, callback);
		return NULL;
	}

	return __add_device(device_id, res, callback);
}

void cvp_iris_hfi_delete_device(void *device)
{
	struct msm_cvp_core *core;
	struct iris_hfi_device *dev = NULL;

	if (!device)
		return;

	core = list_first_entry(&cvp_driver->cores, struct msm_cvp_core, list);
	if (core)
		dev = core->device->hfi_device_data;

	if (!dev)
		return;

	mutex_destroy(&dev->lock);
	destroy_workqueue(dev->cvp_workq);
	destroy_workqueue(dev->iris_pm_workq);
	free_irq(dev->cvp_hal_data->irq, dev);
	iounmap(dev->cvp_hal_data->register_base);
	iounmap(dev->cvp_hal_data->gcc_reg_base);
	kfree(dev->cvp_hal_data);
	kfree(dev->response_pkt);
	kfree(dev->raw_packet);
	kfree(dev);
}

static int iris_hfi_validate_session(void *sess, const char *func)
{
	struct cvp_hal_session *session = sess;
	int rc = 0;
	struct iris_hfi_device *device;

	if (!session || !session->device) {
		dprintk(CVP_ERR, " %s Invalid Params %pK\n", __func__, session);
		return -EINVAL;
	}

	device = session->device;
	mutex_lock(&device->lock);
	if (!__is_session_valid(device, session, func))
		rc = -ECONNRESET;

	mutex_unlock(&device->lock);
	return rc;
}

static void iris_init_hfi_callbacks(struct cvp_hfi_device *hdev)
{
	hdev->core_init = iris_hfi_core_init;
	hdev->core_release = iris_hfi_core_release;
	hdev->core_trigger_ssr = iris_hfi_core_trigger_ssr;
	hdev->session_init = iris_hfi_session_init;
	hdev->session_end = iris_hfi_session_end;
	hdev->session_abort = iris_hfi_session_abort;
	hdev->session_clean = iris_hfi_session_clean;
	hdev->session_set_buffers = iris_hfi_session_set_buffers;
	hdev->session_release_buffers = iris_hfi_session_release_buffers;
	hdev->session_send = iris_hfi_session_send;
	hdev->scale_clocks = iris_hfi_scale_clocks;
	hdev->vote_bus = iris_hfi_vote_buses;
	hdev->get_fw_info = iris_hfi_get_fw_info;
	hdev->get_core_capabilities = iris_hfi_get_core_capabilities;
	hdev->suspend = iris_hfi_suspend;
	hdev->flush_debug_queue = iris_hfi_flush_debug_queue;
	hdev->noc_error_info = iris_hfi_noc_error_info;
	hdev->validate_session = iris_hfi_validate_session;
}

int cvp_iris_hfi_initialize(struct cvp_hfi_device *hdev, u32 device_id,
		struct msm_cvp_platform_resources *res,
		hfi_cmd_response_callback callback)
{
	int rc = 0;

	if (!hdev || !res || !callback) {
		dprintk(CVP_ERR, "Invalid params: %pK %pK %pK\n",
			hdev, res, callback);
		rc = -EINVAL;
		goto err_iris_hfi_init;
	}

	hdev->hfi_device_data = __get_device(device_id, res, callback);

	if (IS_ERR_OR_NULL(hdev->hfi_device_data)) {
		rc = PTR_ERR(hdev->hfi_device_data) ?: -EINVAL;
		goto err_iris_hfi_init;
	}

	iris_init_hfi_callbacks(hdev);

err_iris_hfi_init:
	return rc;
}

