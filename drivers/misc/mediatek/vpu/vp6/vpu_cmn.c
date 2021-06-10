/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/ktime.h>
#include <linux/pm_qos.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include "mtk_devinfo.h"

#ifdef MTK_PERF_OBSERVER
#include <mt-plat/mtk_perfobserver.h>
#endif

#include <mt-plat/mtk_secure_api.h>

#define VPU_TRACE_ENABLED

/* #define BYPASS_M4U_DBG */

#include "vpu_algo.h"
#include "vpu_cmn.h"
#include "vpu_dbg.h"
#include "vpu_hw.h"
#include "vpu_load_image.h"
#include "vpu_reg.h"
#include "vpu_platform_config.h"
#include "vpu_pm.h"
#include "vpu_qos.h"
#include "vpubuf-core.h"
#include "vpu_utilization.h"

#define CMD_WAIT_TIME_MS    (3 * 1000)
#define OPP_WAIT_TIME_MS    (300)
#define PWR_KEEP_TIME_MS    (2000)
#define SDSP_KEEP_TIME_MS   (5000)
#define IOMMU_VA_START      (0x7DA00000)
#define IOMMU_VA_END        (0x82600000)
#define POWER_ON_MAGIC      (2)

/* 20180703, 00:00 : vpu log mechanism */
#define HOST_VERSION	(0x18070300)

static const uint32_t g_vpu_mva_reset_vector[] = {
	VPU_MVA_RESET_VECTOR,
#if (MTK_VPU_CORE >= 2)
	VPU2_MVA_RESET_VECTOR,
#endif
#if (MTK_VPU_CORE >= 3)
	VPU3_MVA_RESET_VECTOR,
#endif
};

static const uint32_t g_vpu_mva_main_program[] = {
	VPU_MVA_MAIN_PROGRAM,
#if (MTK_VPU_CORE >= 2)
	VPU2_MVA_MAIN_PROGRAM,
#endif
#if (MTK_VPU_CORE >= 3)
	VPU3_MVA_MAIN_PROGRAM,
#endif
};

static const uint32_t g_vpu_mva_kernel_lib[] = {
	VPU_MVA_KERNEL_LIB,
#if (MTK_VPU_CORE >= 2)
	VPU2_MVA_KERNEL_LIB,
#endif
#if (MTK_VPU_CORE >= 3)
	VPU3_MVA_KERNEL_LIB,
#endif
};

#ifndef MTK_VPU_FPGA_PORTING
#define VPU_EXCEPTION_MAGIC 0x52310000
#endif

static void vpu_sdsp_routine(struct work_struct *work);

static void vpu_dump_ftrace_workqueue(struct work_struct *);
static void vpu_power_counter_routine(struct work_struct *);

inline struct vpu_device *vpu_get_vpu_device(void)
{
	struct device_node *node = NULL;
	struct platform_device *pdev = NULL;
	struct vpu_device *vpu_device = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8168-vpu");
	if (node == NULL)
		return NULL;

	pdev = of_find_device_by_node(node);
	if (pdev == NULL)
		return NULL;

	vpu_device = platform_get_drvdata(pdev);
	return vpu_device;
}

inline struct vpu_core *vpu_get_vpu_core(int core)
{
	struct vpu_device *vpu_device = vpu_get_vpu_device();

	if (vpu_device == NULL)
		return NULL;

	return vpu_device->vpu_core[core];
}

static inline void lock_command(struct vpu_core *vpu_core)
{
	mutex_lock(&(vpu_core->cmd_mutex));
	vpu_core->is_cmd_done = false;
}

static inline int wait_command(struct vpu_core *vpu_core)
{
	struct vpu_device *vpu_device = vpu_core->vpu_device;
	int ret = 0;
	unsigned int PWAITMODE = 0x0;
	bool jump_out = false;
	int count = 0;
	int core = vpu_core->core;

	ret = wait_event_interruptible_timeout(vpu_device->cmd_wait,
				vpu_core->is_cmd_done,
				msecs_to_jiffies(CMD_WAIT_TIME_MS));

	/* ret == -ERESTARTSYS, if signal interrupt */
	if ((ret != 0) && (!vpu_core->is_cmd_done)) {
		LOG_WRN("[vpu_%d]%s, done(%d), ret=%d, wait cmd again\n", core,
			"interrupt by signal",
			vpu_core->is_cmd_done, ret);

		ret = wait_event_interruptible_timeout(
					vpu_device->cmd_wait,
					vpu_core->is_cmd_done,
					msecs_to_jiffies(CMD_WAIT_TIME_MS));
	}

	if ((ret != 0) && (!vpu_core->is_cmd_done)) {
		/* ret == -ERESTARTSYS, if signal interrupt */
		LOG_ERR("[vpu_%d] %s, done(%d), ret=%d\n",
				core,
				"interrupt by signal again",
				vpu_core->is_cmd_done, ret);
		ret = -ERESTARTSYS;
	} else {
		LOG_DBG("[vpu_%d] test ret(%d)\n", core, ret);
		if (ret > 0) {
			/* check PWAITMODE, request by DE */
			do {
				PWAITMODE = vpu_read_field(vpu_core->vpu_base,
								FLD_PWAITMODE);
				count++;
				if (PWAITMODE & 0x1) {
					ret = 0;
					jump_out = true;

					LOG_DBG("[vpu_%d] %s(%d), ret(%d)\n",
						core,
						"test PWAITMODE status",
						PWAITMODE, ret);
				} else {
					LOG_WRN(
						"[vpu_%d]%s(%d)%s(%d)%s(%d)%s(0x%x)\n",
						core,
						"PWAITMODE", count,
						"error status", PWAITMODE,
						"ret", ret, "info25",
					vpu_read_field(vpu_core->vpu_base,
							FLD_XTENSA_INFO25));
						vpu_dump_register(NULL,
								  vpu_device);
					if (count == 5) {
						ret = -ETIMEDOUT;
						jump_out = true;
					}
					/*wait 2 ms to check, total 5 times*/
					mdelay(2);
				}
			} while (!jump_out);
		} else {
			ret = -ETIMEDOUT;
		}
	}

	return ret;
}

static inline void unlock_command(struct vpu_core *vpu_core)
{
	mutex_unlock(&(vpu_core->cmd_mutex));
}

/******************************************************************************
 * Add MET ftrace event for power profilling.
 *****************************************************************************/
#if defined(VPU_MET_READY)
void MET_Events_DVFS_Trace(struct vpu_device *vpu_device)
{
	int vcore_opp = 0;
	int apu_freq = 0, apu_if_freq = 0;

	vpu_get_opp_freq(vpu_device, &vcore_opp, &apu_freq, &apu_if_freq);
	vpu_met_event_dvfs(vcore_opp, apu_freq, apu_if_freq);
}

void MET_Events_BusyRate_Trace(struct vpu_device *vpu_device, int core)
{
	unsigned long total_time;
	unsigned long busy_time;
	struct vpu_core *vpu_core = vpu_device->vpu_core[core];
	int ret;

	ret = vpu_dvfs_get_usage(vpu_core, &total_time, &busy_time);
	if (ret) {
		LOG_ERR("vpu loading : error\n");
		return;
	}
	vpu_met_event_busyrate(core, (unsigned int)(busy_time  *
			       100U / total_time));
}

void MET_Events_Trace(struct vpu_device *vpu_device, bool enter, int core,
			int algo_id)
{
	if (enter) {
		int vcore_opp = 0;
		int apu_freq = 0, apu_if_freq = 0;

		vpu_get_opp_freq(vpu_device, &vcore_opp, &apu_freq,
					&apu_if_freq);
		vpu_met_event_enter(core, algo_id, apu_freq);
	} else {
		vpu_met_event_leave(core, algo_id);
	}
}
#endif

static inline bool vpu_other_core_idle_check(struct vpu_core *vpu_core)
{
	int i = 0;
	bool idle = true;
	struct vpu_device *vpu_device = vpu_core->vpu_device;
	struct vpu_core *vpu_core_tmp;
	int core = vpu_core->core;

	LOG_DBG("vpu %s+\n", __func__);

	for (i = 0 ; i < vpu_device->core_num ; i++) {
		if (i == core) {
			continue;
		} else {
			vpu_core_tmp = vpu_device->vpu_core[i];

			LOG_DBG("vpu test %d/%d/%d\n", core,
					i, vpu_core_tmp->state);

			mutex_lock(&(vpu_core_tmp->state_mutex));
			switch (vpu_core_tmp->state) {
			case VCT_SHUTDOWN:
			case VCT_BOOTUP:
			case VCT_IDLE:
				break;
			case VCT_EXECUTING:
			case VCT_NONE:
			case VCT_VCORE_CHG:
				idle = false;
				mutex_unlock(
					&(vpu_core_tmp->state_mutex));
				goto out;
				/*break;*/
			}
			mutex_unlock(&(vpu_core_tmp->state_mutex));
		}
	}

	for (i = 0; i < vpu_device->core_num; i++)
		LOG_DBG("vpu core_idle_check %d, core %d state = %d\n", idle,
				i, vpu_device->vpu_core[i]->state);

out:
	return idle;
}

static inline bool vpu_opp_change_idle_check(struct vpu_core *vpu_core)
{
	struct vpu_device *vpu_device = vpu_core->vpu_device;
	struct vpu_core *vpu_core_tmp;
	int i = 0;
	bool idle = true;
	int core = vpu_core->core;

	for (i = 0; i < vpu_device->core_num; i++) {
		if (i == core) {
			continue;
		} else {
			vpu_core_tmp = vpu_device->vpu_core[i];

			LOG_DBG("vpu test %d/%d/%d\n", core, i,
					vpu_core_tmp->state);

			mutex_lock(&(vpu_core_tmp->state_mutex));
			switch (vpu_core_tmp->state) {
			case VCT_SHUTDOWN:
			case VCT_BOOTUP:
			case VCT_IDLE:
			case VCT_EXECUTING:
			case VCT_NONE:
				break;
			case VCT_VCORE_CHG:
				idle = false;
				mutex_unlock(
					&(vpu_core_tmp->state_mutex));
				goto out;
				/*break;*/
			}
			mutex_unlock(&(vpu_core_tmp->state_mutex));
		}
	}

out:
	return idle;
}

int wait_to_do_change_vcore_opp(struct vpu_core *vpu_core)
{
	struct vpu_device *vpu_device;
	int ret = 0;
	int retry = 0;
	int core;

	if (vpu_core == NULL)
		return 0;

	vpu_device = vpu_core->vpu_device;

	core = vpu_core->core;
	/* now this function just return directly. NO WAIT */
	if (vpu_device->func_mask & VFM_NEED_WAIT_VCORE) {
		if (vpu_device->vpu_log_level > Log_STATE_MACHINE) {
			LOG_INF("[vpu_%d_0x%x] wait for vcore change now\n",
					core, vpu_device->func_mask);
		}
	} else {
		return ret;
	}

	do {
		ret = wait_event_interruptible_timeout(
					vpu_device->waitq_change_vcore,
					vpu_other_core_idle_check(vpu_core),
					msecs_to_jiffies(OPP_WAIT_TIME_MS));

		/* ret == -ERESTARTSYS, if signal interrupt */
		if (ret < 0) {
			retry += 1;
			ret = -EINTR;
			LOG_ERR("[vpu_%d/%d] %s, %s, ret=%d\n",
				core, retry,
				"interrupt by signal",
				"while wait to change vcore",
				ret);
		} else {
			/* normal case */
			if (ret > 0) {
				ret = 0;
			} else {
				ret = -ETIMEDOUT;
				LOG_ERR("[vpu_%d] %s timeout, ret=%d\n", core,
					__func__, ret);
			}
			break;
		}
	} while (retry < 3);

	return ret;
}

static inline int wait_to_do_vpu_running(struct vpu_core *vpu_core)
{
	struct vpu_device *vpu_device = vpu_core->vpu_device;
	int ret = 0;
	int retry = 0;
	int core = vpu_core->core;

	/* now this function just return directly. NO WAIT */
	if (vpu_device->func_mask & VFM_NEED_WAIT_VCORE) {
		if (vpu_device->vpu_log_level > Log_STATE_MACHINE) {
			LOG_INF("[vpu_%d_0x%x] wait for vpu running now\n",
					core, vpu_device->func_mask);
		}
	} else {
		return ret;
	}

	do {
		ret = wait_event_interruptible_timeout(
					vpu_device->waitq_do_core_executing,
					vpu_opp_change_idle_check(vpu_core),
					msecs_to_jiffies(OPP_WAIT_TIME_MS));

		/* ret == -ERESTARTSYS, if signal interrupt */
		if (ret < 0) {
			retry += 1;
			ret = -EINTR;
			LOG_ERR("[vpu_%d/%d] %s, %s, ret=%d\n",
				core, retry,
				"interrupt by signal",
				"while wait to do vpu running",
				ret);
		} else {
			/* normal case */
			if (ret > 0) {
				ret = 0;
			} else {
				ret = -ETIMEDOUT;
				LOG_ERR("[vpu_%d] %s timeout, ret=%d\n",
					core, __func__, ret);
			}
			break;
		}
	} while (retry < 3);

	return ret;
}

int get_vpu_init_done(void)
{
	struct vpu_device *vpu_device = vpu_get_vpu_device();

	if (vpu_device == NULL)
		return 0;

	return vpu_device->vpu_init_done;
}
EXPORT_SYMBOL(get_vpu_init_done);

int32_t vpu_thermal_en_throttle_cb(uint8_t vcore_opp, uint8_t vpu_opp)
{
	struct vpu_device *vpu_device = vpu_get_vpu_device();

	if (vpu_device == NULL)
		return 0;

	return vpu_thermal_en_throttle_cb_set(vpu_device, vcore_opp, vpu_opp);
}

int32_t vpu_thermal_dis_throttle_cb(void)
{
	struct vpu_device *vpu_device = vpu_get_vpu_device();

	if (vpu_device == NULL)
		return 0;

	return vpu_thermal_dis_throttle_cb_set(vpu_device);
}

#ifdef MET_VPU_LOG
/* log format */
#define VPUBUF_MARKER		(0xBEEF)
#define VPULOG_START_MARKER	(0x55AA)
#define VPULOG_END_MARKER	(0xAA55)
#define MX_LEN_STR_DESC		(128)

#pragma pack(push)
#pragma pack(2)
struct vpulog_format_head_t {
	unsigned short start_mark;
	unsigned char desc_len;
	unsigned char action_id;
	unsigned int sys_timer_h;
	unsigned int sys_timer_l;
	unsigned short sessid;
};
#pragma pack(pop)

struct vpu_log_reader_t {
	struct list_head list;
	int core;
	unsigned int buf_addr;
	unsigned int buf_size;
	void *ptr;
};

static int vpu_check_postcond(struct vpu_core *vpu_core);

static void __MET_PACKET__(struct vpu_core *vpu_core, unsigned long long wclk,
	 unsigned char action_id, char *str_desc, unsigned int sessid)
{
	char action = 'Z';
	char null_str[] = "null";
	char *__str_desc = str_desc;
	int val = 0;
	int core = vpu_core->core;

	switch (action_id) {
	/* For Sync Maker Begin/End */
	case 0x01:
		action = 'B';
		break;
	case 0x02:
		action = 'E';
		break;
	/* For Counter Maker */
	case 0x03:
		action = 'C';
		/* counter type: */
		/* bit 0~11: string desc */
		/* bit 12~15: count val */
		val = *(unsigned int *)(str_desc + 12);
		break;
	/* for Async Marker Start/Finish */
	case 0x04:
		action = 'S';
		break;
	case 0x05:
		action = 'F';
		break;
	}
	if (str_desc[0] == '\0') {
		/* null string handle */
		__str_desc =  null_str;
	}

	vpu_met_packet(wclk, action, core, vpu_core->ftrace_dump_work.pid,
			sessid + 0x8000 * core, __str_desc, val);
}

static void dump_buf(void *ptr, int leng)
{
	int idx = 0;
	unsigned short *usaddr;

	for (idx = 0; idx < leng; idx += 16) {
		usaddr = (unsigned short *)(ptr + idx);
		vpu_trace_dump("%08x: %04x%04x %04x%04x %04x%04x %04x%04x,",
			idx,
			usaddr[0], usaddr[1], usaddr[2], usaddr[3],
			usaddr[4], usaddr[5], usaddr[6], usaddr[7]
		);
	}
}

static int vpulog_clone_buffer(struct vpu_core *vpu_core, unsigned int addr,
				unsigned int size, void *ptr)
{
	int idx = 0;

	for (idx = 0; idx < size; idx += 4) {
		/* read 4 bytes from VPU DMEM */
		*((unsigned int *)(ptr + idx)) =
			vpu_read_reg32(vpu_core->vpu_base, addr + idx);
	}
	/* notify VPU buffer copy is finish */
	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO18, 0x00000000);
	return 0;
}

static bool vpulog_check_buff_ready(void *buf, int buf_len)
{
	if (VPUBUF_MARKER != *(unsigned short *)buf) {
		/* front marker is invalid*/
		vpu_trace_dump("Error front maker: %04x",
		*(unsigned short *)buf);
		return false;
	}
	if (VPUBUF_MARKER != *(unsigned short *)(buf + buf_len - 2)) {
		vpu_trace_dump("Error end maker: %04x",
			*(unsigned short *)(buf + buf_len - 2));
		return false;
	}
	return true;
}

static bool vpulog_check_packet_valid(struct vpulog_format_head_t *lft)
{
	if (lft->start_mark != VPULOG_START_MARKER) {
		vpu_trace_dump("Error lft->start_mark: %02x", lft->start_mark);
		return false;
	}
	if (lft->desc_len > 0x10) {
		vpu_trace_dump("Error lft->desc_len: %02x", lft->desc_len);
		return false;
	}
	return true;
}

static void vpu_log_parser(struct vpu_core *vpu_core, void *ptr, int buf_leng)
{
	struct vpu_device *vpu_device = vpu_core->vpu_device;
	int idx = 0;
	void *start_ptr = ptr;
	int valid_len = 0;
	struct vpulog_format_head_t *lft;
	char trace_data[MX_LEN_STR_DESC];
	int vpulog_format_header_size = sizeof(struct vpulog_format_head_t);
	int core = vpu_core->core;

	/*check buffer status*/
	if (false == vpulog_check_buff_ready(start_ptr, buf_leng)) {
		vpu_trace_dump("vpulog_check_buff_ready fail: %p %d",
			ptr, buf_leng);
		dump_buf(start_ptr, buf_leng);
		return;
	}

	/* get valid length*/
	valid_len = *(unsigned short *)(start_ptr + 2);
	if (valid_len >= buf_leng) {
		vpu_trace_dump("valid_len: %d large than buf_leng: %d",
			valid_len, buf_leng);
		return;
	}
	/*data offset start after Marker,Length*/
	idx += 4;

	if (vpu_device->vpu_log_level > VpuLogThre_PERFORMANCE)
		vpu_trace_begin("%s|vpu%d|@%p/%d",
		__func__, core, ptr, buf_leng);
	while (1) {
		unsigned long long sys_t;
		int packet_size = 0;
		void *data_ptr;

		if (idx >= valid_len)
			break;

		lft = (struct vpulog_format_head_t *)(start_ptr + idx);
		data_ptr = (start_ptr + idx) + vpulog_format_header_size;
		if (false == vpulog_check_packet_valid(lft)) {
			vpu_trace_dump("vpulog_check_packet_valid fail");
			dump_buf(start_ptr, buf_leng);
			break;
		}

		/*calculate packet size: header + data + end_mark*/
		packet_size = vpulog_format_header_size + lft->desc_len + 2;
		if (idx + packet_size > valid_len) {
			vpu_trace_dump(
					"error length (idx: %d, packet_size: %d)",
					idx, packet_size);
			vpu_trace_dump("out of bound: valid_len: %d",
			valid_len);
			dump_buf(start_ptr, buf_leng);
			break;
		}

		if (lft->desc_len > MX_LEN_STR_DESC) {
			vpu_trace_dump(
					"lft->desc_len(%d) > MX_LEN_STR_DESC(%d)",
					lft->desc_len, MX_LEN_STR_DESC);
			dump_buf(start_ptr, buf_leng);
			break;
		}
		memset(trace_data, 0x00, MX_LEN_STR_DESC);
		if (lft->desc_len > 0) {
			/*copy data buffer*/
			memcpy(trace_data, data_ptr, lft->desc_len);
		}
		sys_t = lft->sys_timer_h;
		sys_t = (sys_t << 32) + (lft->sys_timer_l & 0xFFFFFFFF);

		__MET_PACKET__(
			vpu_core,
			sys_t,
			lft->action_id,
			trace_data,
			lft->sessid
		);
		idx += packet_size;
	}
	if (vpu_device->vpu_log_level > VpuLogThre_PERFORMANCE)
		vpu_trace_end();
}

static unsigned int addr_tran_vpu2apmcu(int core, unsigned int buf_addr)
{
	unsigned int apmcu_log_buf = 0x0;
	unsigned int apmcu_log_buf_ofst = 0xFFFFFFFF;

	switch (core) {
	case 0:
		apmcu_log_buf = ((buf_addr & 0x000fffff) | 0x19100000);
		apmcu_log_buf_ofst = apmcu_log_buf - 0x19100000;
		break;
	case 1:
		apmcu_log_buf = ((buf_addr & 0x000fffff) | 0x19200000);
		apmcu_log_buf_ofst = apmcu_log_buf - 0x19200000;
		break;
	default:
		LOG_ERR("wrong core(%d)\n", core);
		goto out;
	}
out:
	return apmcu_log_buf_ofst;
}

static void vpu_dump_ftrace_workqueue(struct work_struct *work)
{
	struct vpu_device *vpu_device = NULL;
	struct vpu_core *vpu_core = NULL;
	unsigned int log_buf_size = 0x0;
	struct my_ftworkQ_struct_t *my_work_core = container_of(work,
		struct my_ftworkQ_struct_t, my_work);
	struct list_head *entry;
	struct vpu_log_reader_t *vlr;
	void *ptr;
	int core;

	vpu_core = container_of(my_work_core, struct vpu_core,
					ftrace_dump_work);
	vpu_device = vpu_core->vpu_device;
	if (vpu_device->vpu_log_level > VpuLogThre_PERFORMANCE)
		vpu_trace_begin("VPU_LOG_ISR_BOTTOM_HALF");
	/* protect area start */
list_rescan:
	spin_lock_irq(&(my_work_core->my_lock));
	list_for_each(entry, &(my_work_core->list)) {
		vlr = list_entry(entry, struct vpu_log_reader_t, list);
		if (vlr != NULL) {
			log_buf_size = vlr->buf_size;
			core = vlr->core;
			vpu_trace_dump("%s %d addr/size/ptr: %08x/%08x/%p",
					__func__, __LINE__, vlr->buf_addr,
					vlr->buf_size, vlr->ptr);
			ptr = vlr->ptr;
			if (ptr) {
				vpu_log_parser(vpu_core, ptr, log_buf_size);
				kfree(ptr);
				vlr->ptr = NULL;
			} else {
				vpu_trace_dump("my_work_core->ptr is NULL");
			}
			list_del(entry);
			kfree(vlr);
		} else {
			vpu_trace_dump("%s %d vlr is null",
				__func__, __LINE__);
			list_del(entry);
		}
		spin_unlock_irq(&(my_work_core->my_lock));
		goto list_rescan;
	}
	spin_unlock_irq(&(my_work_core->my_lock));
	/* protect area end */
	if (vpu_device->vpu_log_level > VpuLogThre_PERFORMANCE)
		vpu_trace_end();
}

#define VPU_MOVE_WAKE_TO_BACK
static int isr_common_handler(struct vpu_core *vpu_core)
{
	struct vpu_device *vpu_device = vpu_core->vpu_device;
	int req_cmd = 0, normal_check_done = 0;
	int req_dump = 0;
	unsigned int apmcu_log_buf_ofst;
	unsigned int log_buf_addr = 0x0;
	unsigned int log_buf_size = 0x0;
	struct vpu_log_reader_t *vpu_log_reader;
	void *ptr;
	uint32_t status;
	int core = vpu_core->core;

	LOG_DBG("vpu %d received a interrupt\n", core);

	/* INFO 17 was used to reply command done */
	req_cmd = vpu_read_field(vpu_core->vpu_base, FLD_XTENSA_INFO17);
	LOG_DBG("INFO17=0x%08x\n", req_cmd);
	vpu_trace_dump("VPU%d_ISR_RECV|INFO17=0x%08x", core, req_cmd);
	switch (req_cmd) {
	case 0:
		break;
	case VPU_REQ_DO_CHECK_STATE:
	default:
		if (vpu_check_postcond(vpu_core) == -EBUSY) {
		/* host may receive isr to dump */
		/* ftrace log while d2d is stilling running */
		/* but the info17 is set as 0x100 */
		/* in this case, we do nothing for */
		/* cmd state control */
		/* flow while device status is busy */
			vpu_trace_dump("VPU%d VPU_REQ_DO_CHECK_STATE BUSY",
			core);
		} else {
			/* other normal cases for cmd state control flow */
			normal_check_done = 1;
#ifndef VPU_MOVE_WAKE_TO_BACK
			vpu_core->is_cmd_done = true;
			wake_up_interruptible(&vpu_device->cmd_wait);
			vpu_trace_dump("VPU%d VPU_REQ_DO_CHECK_STATE OK", core);
#endif
		}
		break;
	}


	/* INFO18 was used to dump MET Log */
	req_dump = vpu_read_field(vpu_core->vpu_base, FLD_XTENSA_INFO18);
	LOG_DBG("INFO18=0x%08x\n", req_dump);
	vpu_trace_dump("VPU%d_ISR_RECV|INFO18=0x%08x", core, req_dump);
	/* dispatch interrupt by INFO18 */
	switch (req_dump) {
	case 0:
		break;
	case VPU_REQ_DO_DUMP_LOG:
		/* handle output log */
		if (vpu_device->func_mask & VFM_ROUTINE_PRT_SYSLOG) {
			log_buf_addr = vpu_read_field(vpu_core->vpu_base,
							FLD_XTENSA_INFO05);
			log_buf_size = vpu_read_field(vpu_core->vpu_base,
							FLD_XTENSA_INFO06);

			/* translate vpu address to apmcu */
			apmcu_log_buf_ofst =
			addr_tran_vpu2apmcu(core, log_buf_addr);
			if (vpu_device->vpu_log_level
				> VpuLogThre_PERFORMANCE) {
				vpu_trace_dump(
					"[vpu_%d] log buf/size(0x%08x/0x%08x)-> log_apmcu offset(0x%08x)",
					core,
					log_buf_addr,
					log_buf_size,
					apmcu_log_buf_ofst);
			}
			if (apmcu_log_buf_ofst == 0xFFFFFFFF) {
				LOG_ERR(
					"addr_tran_vpu2apmcu translate fail!\n");
				goto info18_out;
			}
			/* in ISR we need use ATOMIC flag to alloc memory */
			ptr = kmalloc(log_buf_size, GFP_ATOMIC);
			if (ptr) {
				if (vpu_device->vpu_log_level
					> VpuLogThre_PERFORMANCE)
					vpu_trace_begin(
						"VPULOG_ISR_TOPHALF|VPU%d"
						, core);
					vpu_log_reader =
						kmalloc(sizeof(
						struct vpu_log_reader_t),
						GFP_ATOMIC);
				if (vpu_log_reader) {
					/* fill vpu_log reader's information */
					vpu_log_reader->core = core;
					vpu_log_reader->buf_addr = log_buf_addr;
					vpu_log_reader->buf_size = log_buf_size;
					vpu_log_reader->ptr = ptr;

					LOG_DBG(
						"%s %d [vpu_%d] addr/size/ptr: %08x/%08x/%p\n",
						__func__, __LINE__,
						core,
						log_buf_addr,
						log_buf_size,
						ptr);
					/* clone buffer in isr*/
					if (vpu_device->vpu_log_level
						> VpuLogThre_PERFORMANCE)
						vpu_trace_begin(
							"VPULOG_CLONE_BUFFER|VPU%d|%08x/%u->%p",
							core,
							log_buf_addr,
							log_buf_size,
							ptr);
					vpulog_clone_buffer(vpu_core,
							apmcu_log_buf_ofst,
							log_buf_size, ptr);
					if (vpu_device->vpu_log_level
						> VpuLogThre_PERFORMANCE)
						vpu_trace_end();
					/* dump_buf(ptr, log_buf_size); */

				/* protect area start */
				spin_lock(
					&(vpu_core->ftrace_dump_work.my_lock));
				list_add_tail(&(vpu_log_reader->list),
					&(vpu_core->ftrace_dump_work.list));
				spin_unlock(
					&(vpu_core->ftrace_dump_work.my_lock));
				/* protect area end */

					/* dump log to ftrace on BottomHalf */
					schedule_work(
					&(vpu_core->ftrace_dump_work.my_work)
					);
				} else {
					LOG_ERR("vpu_log_reader alloc fail");
					kfree(ptr);
				}
				if (vpu_device->vpu_log_level
						> VpuLogThre_PERFORMANCE)
					vpu_trace_end();
			} else {
				LOG_ERR("vpu buffer alloc fail");
			}
		}
		break;
	case VPU_REQ_DO_CLOSED_FILE:
		break;
	default:
		LOG_ERR("vpu %d not support cmd..(%d)\n", core,
			req_dump);
		break;
	}

info18_out:
	/* clear int */
	status = vpu_read_field(vpu_core->vpu_base, FLD_APMCU_INT);

	if (status != 0) {
		LOG_ERR("vpu %d FLD_APMCU_INT = (%d)\n", core,
			status);
	}
	vpu_write_field(vpu_core->vpu_base, FLD_APMCU_INT, 1);

#ifdef VPU_MOVE_WAKE_TO_BACK
	if (normal_check_done == 1) {
		vpu_trace_dump("VPU%d VPU_REQ_DO_CHECK_STATE OK", core);
		LOG_DBG("normal_check_done UNLOCK\n");
		vpu_core->is_cmd_done = true;
		wake_up_interruptible(&vpu_device->cmd_wait);
	}
#endif
	return IRQ_HANDLED;
}

irqreturn_t vpu_isr_handler(int irq, void *dev_id)
{
	struct vpu_core *vpu_core = (struct vpu_core *)dev_id;

	if (vpu_core->vpu_device->in_sec_world)
		return IRQ_NONE;

	return isr_common_handler(vpu_core);
}
#else
irqreturn_t vpu_isr_handler(int irq, void *dev_id)
{
	struct vpu_core *vpu_core = (struct vpu_core *)dev_id;
	struct vpu_device *vpu_device = vpu_core->vpu_device;

	if (vpu_core->vpu_device->in_sec_world)
		return IRQ_NONE;

	LOG_DBG("vpu 0 received a interrupt\n");
	vpu_core->is_cmd_done = true;
	wake_up_interruptible(&vpu_device->cmd_wait);

	/* clear int */
	vpu_write_field(vpu_core->core, FLD_APMCU_INT, 1);

	return IRQ_HANDLED;
}
#endif

static bool service_pool_is_empty(struct vpu_core *vpu_core)
{
	bool is_empty = true;

	mutex_lock(&vpu_core->servicepool_mutex);
	if (!list_empty(&vpu_core->pool_list))
		is_empty = false;

	mutex_unlock(&vpu_core->servicepool_mutex);
	return is_empty;
}

static bool common_pool_is_empty(struct vpu_device *vpu_device)
{
	bool is_empty = true;

	mutex_lock(&vpu_device->commonpool_mutex);
	if (!list_empty(&vpu_device->cmnpool_list))
		is_empty = false;

	mutex_unlock(&vpu_device->commonpool_mutex);
	return is_empty;
}

static int vpu_service_routine(void *arg)
{
	struct vpu_user *user = NULL;
	struct vpu_request *req = NULL;
	struct vpu_core *vpu_core = (struct vpu_core *)arg;
	struct vpu_device *vpu_device = vpu_core->vpu_device;
	/*struct vpu_algo *algo = NULL;*/
	uint8_t dsp_freq_index = 0xFF;
	struct vpu_user *user_in_list = NULL;
	struct list_head *head = NULL;
	int service_core;
	bool get = false;
	int i = 0, j = 0, cnt = 0;

	DEFINE_WAIT_FUNC(wait, woken_wake_function);

	service_core = vpu_core->core;
	for (; !kthread_should_stop();) {
		/* wait for requests if there is no one in user's queue */
		add_wait_queue(&vpu_device->req_wait, &wait);
		while (1) {
			if ((!service_pool_is_empty(vpu_core)) ||
					(!common_pool_is_empty(vpu_device)))
				break;
			wait_woken(&wait, TASK_INTERRUPTIBLE,
						MAX_SCHEDULE_TIMEOUT);
		}
		remove_wait_queue(&vpu_device->req_wait, &wait);

		/* this thread will be stopped if start direct link */
		/* todo, no need in this currently */
		/*wait_event_interruptible(lock_wait, !is_locked);*/

		/* consume the user's queue */
		req = NULL;
		user_in_list = NULL;
		head = NULL;
		get = false;
		cnt = 0;

		mutex_lock(&vpu_core->servicepool_mutex);
		if (!(list_empty(&vpu_core->pool_list))) {

			req = vlist_node_of(vpu_core->pool_list.next,
						struct vpu_request);

			list_del_init(vlist_link(req, struct vpu_request));

			vpu_core->servicepool_list_size -= 1;
			/*priority list*/
			vpu_core->priority_list[req->priority] -= 1;
			LOG_DBG("[vpu] flag - : selfpool(%d)_size(%d)\n",
				service_core,
				vpu_core->servicepool_list_size);
			for (i = 0 ; i < VPU_REQ_MAX_NUM_PRIORITY ; i++) {
				LOG_DBG("[vpu_%d] priority_list num[%d]:%d\n",
				service_core, i,
				vpu_core->priority_list[i]);
			}
			mutex_unlock(&vpu_core->servicepool_mutex);

			LOG_DBG("[vpu] flag - 2: get selfpool\n");
		} else {
			mutex_unlock(&vpu_core->servicepool_mutex);

			mutex_lock(&vpu_device->commonpool_mutex);
			if (!(list_empty(&vpu_device->cmnpool_list))) {
				req = vlist_node_of(
					vpu_device->cmnpool_list.next,
					struct vpu_request);

				list_del_init(vlist_link(req,
						struct vpu_request));

				vpu_device->commonpool_list_size -= 1;

				LOG_DBG("[vpu] flag - :common pool_size(%d)\n",
					vpu_device->commonpool_list_size);

				LOG_DBG("[vpu] flag - 3: get common pool\n");
			}
			mutex_unlock(&vpu_device->commonpool_mutex);
		}

		/* suppose that req is null would not happen
		 * due to we check service_pool_is_empty and
		 * common_pool_is_empty
		 */
		if (req != NULL) {
			LOG_DBG("[vpu] service core index...: %d/%d\n",
					service_core, service_core);

			user = (struct vpu_user *)req->user_id;

			LOG_DBG("[vpu_%d] user...0x%lx/0x%lx/0x%lx/0x%lx\n",
				service_core,
				(unsigned long)user,
				(unsigned long)&user,
				(unsigned long)req->user_id,
				(unsigned long)&(req->user_id));

			mutex_lock(&vpu_device->user_mutex);

			/* check to avoid user had been removed from list,
			 * and kernel vpu thread still do the request
			 */
			list_for_each(head, &vpu_device->user_list)
			{
				user_in_list = vlist_node_of(head,
							struct vpu_user);

				LOG_DBG("[vpu_%d] user->id = 0x%lx, 0x%lx\n",
					service_core,
					(unsigned long)(user_in_list->id),
					(unsigned long)(user));

				if ((unsigned long)(user_in_list->id) ==
							(unsigned long)(user)) {
					get = true;
					LOG_DBG("[vpu_%d] get_0x%lx = true\n",
							service_core,
							(unsigned long)(user));
					break;
				}
			}
			if (!get) {
				mutex_unlock(&vpu_device->user_mutex);

				LOG_WRN("[vpu_%d] %s(0x%lx) %s\n",
					service_core,
					"get request that the original user",
					(unsigned long)(user),
					"is deleted");
				/* release buf ref cnt if user is deleted*/
				for (i = 0 ; i < req->buffer_count ; i++) {
					for (j = 0;
						j < req->buffers[i].plane_count;
									j++) {
						uint64_t id;

						id = req->buf_ion_infos[cnt];
						vbuf_free_handle(vpu_device,
									id);
						cnt++;
					}
				}
				continue;
			}

			mutex_lock(&user->data_mutex);
			user->running[service_core] = true; /* */
			mutex_unlock(&user->data_mutex);

			/* unlock for avoiding long time locking */
			mutex_unlock(&vpu_device->user_mutex);
			vpu_set_opp_check(vpu_core, req);

			LOG_INF(
				"[v%d<-0x%x]0x%lx,ID=0x%lx_%d,%d->%d,%d,%d/%d,%d-%d,0x%x,%d/%d/%d/0x%x\n",
				service_core,
				req->requested_core,
				(unsigned long)req->user_id,
				(unsigned long)req->request_id,
				req->frame_magic,
				vpu_core->current_algo,
				(int)(req->algo_id[service_core]),
				req->power_param.opp_step,
				req->power_param.freq_step,
				vpu_device->opps.vcore.index,
				dsp_freq_index,
				vpu_device->opps.apu.index,
				vpu_device->func_mask,
				vpu_core->servicepool_list_size,
				vpu_device->commonpool_list_size,
				vpu_device->is_locked,
				vpu_device->efuse_data);


			#ifdef CONFIG_PM_WAKELOCKS
			__pm_stay_awake(&(vpu_core->vpu_wake_lock));
			#else
			wake_lock(&(vpu_core->vpu_wake_lock));
			#endif
			vpu_core->exception_isr_check = true;
			if (vpu_hw_processing_request(vpu_core, req)) {
				LOG_WRN("[vpu_%d] =========================\n",
						service_core);
				LOG_WRN("[vpu_%d] %s failed, retry once\n",
						service_core,
						"hw_processing_request");
				LOG_WRN("[vpu_%d] =========================\n",
						service_core);
				vpu_core->exception_isr_check = true;
				if (vpu_hw_processing_request(vpu_core, req)) {
					LOG_ERR("[vpu_%d] %s failed @ Q\n",
						service_core,
						"hw_processing_request");
					req->status = VPU_REQ_STATUS_FAILURE;
					goto out;
				} else {
					req->status = VPU_REQ_STATUS_SUCCESS;
				}
			} else {
				req->status = VPU_REQ_STATUS_SUCCESS;
			}
			LOG_DBG("[vpu] flag - 5: hw enque_request done\n");
		} else {
			/* consider that only one req in common pool and all
			 * services get pass through.
			 * do nothing if the service do not get the request
			 */
			LOG_WRN("[vpu_%d] get null request, %d/%d/%d\n",
				service_core,
				vpu_core->servicepool_list_size,
				vpu_device->commonpool_list_size,
				vpu_device->is_locked);
			continue;
		}
out:
		cnt = 0;
		for (i = 0 ; i < req->buffer_count ; i++) {
			for (j = 0 ; j < req->buffers[i].plane_count ; j++) {
				uint64_t id;

				id = req->buf_ion_infos[cnt];
				vbuf_free_handle(vpu_device, id);
				cnt++;
			}
		}

		/* if req is null, we should not do anything of
		 * following codes
		 */
		mutex_lock(&(vpu_core->state_mutex));
		if (vpu_core->state != VCT_SHUTDOWN)
			vpu_core->state = VCT_IDLE;
		mutex_unlock(&(vpu_core->state_mutex));
		#ifdef CONFIG_PM_WAKELOCKS
		__pm_relax(&(vpu_core->vpu_wake_lock));
		#else
		wake_unlock(&(vpu_core->vpu_wake_lock));
		#endif
		mutex_lock(&vpu_device->user_mutex);

		LOG_DBG("[vpu] flag - 5.5 : ....\n");
		/* check to avoid user had been removed from list,
		 * and kernel vpu thread finish the task
		 */
		get = false;
		head = NULL;
		user_in_list = NULL;
		list_for_each(head, &vpu_device->user_list)
		{
			user_in_list = vlist_node_of(head, struct vpu_user);
			if ((unsigned long)(user_in_list->id) ==
							(unsigned long)(user)) {
				get = true;
				break;
			}
		}

		if (get) {
			mutex_lock(&user->data_mutex);

			LOG_DBG("[vpu] flag - 6: add to deque list\n");

			req->occupied_core = (0x1 << service_core);
			list_add_tail(vlist_link(req, struct vpu_request),
						&user->deque_list);

			LOG_INF("[vpu_%d, 0x%x->0x%x] %s(%d_%d), st(%d) %s\n",
				service_core,
				req->requested_core, req->occupied_core,
				"algo_id", (int)(req->algo_id[service_core]),
				req->frame_magic,
				req->status,
				"add to deque list done");

			user->running[service_core] = false;
			mutex_unlock(&user->data_mutex);
			wake_up_interruptible_all(&user->deque_wait);
			wake_up_interruptible_all(&user->delete_wait);
		} else {
			LOG_WRN("[vpu_%d]%s(0x%lx) is deleted\n",
				service_core,
				"done request that the original user",
				(unsigned long)(user));
		}
		mutex_unlock(&vpu_device->user_mutex);
		wake_up_interruptible(&vpu_device->waitq_change_vcore);
		/* leave loop of round-robin */
		if (vpu_device->is_locked)
			break;
		/* release cpu for another operations */
		usleep_range(1, 10);
	}

	return 0;
}

#ifndef MTK_VPU_EMULATOR
static int vpu_map_mva_of_bin(struct vpu_core *vpu_core, uint64_t bin_pa)
{
	struct vpu_device *vpu_device = vpu_core->vpu_device;
	int ret = 0;
	int core = vpu_core->core;
	uint32_t mva_reset_vector;
	uint32_t mva_main_program;
	uint64_t binpa_reset_vector;
	uint64_t binpa_main_program;
	uint64_t binpa_iram_data;
	struct vpu_shared_memory_param mem_param;

	LOG_DBG("%s, bin_pa(0x%lx)\n", __func__, (unsigned long)bin_pa);


	mva_reset_vector = g_vpu_mva_reset_vector[core];
	mva_main_program = g_vpu_mva_main_program[core];
	binpa_reset_vector = bin_pa + VPU_DDR_SHIFT_RESET_VECTOR * core;
	binpa_main_program = binpa_reset_vector + VPU_OFFSET_MAIN_PROGRAM;
	binpa_iram_data = bin_pa + VPU_OFFSET_MAIN_PROGRAM_IMEM +
				VPU_DDR_SHIFT_IRAM_DATA * core;

	LOG_DBG("%s(core:%d), pa resvec/mainpro(0x%lx/0x%lx)\n",
					__func__, core,
					(unsigned long)binpa_reset_vector,
					(unsigned long)binpa_main_program);

	/* 1. map reset vector */
	memset(&mem_param, 0x0, sizeof(mem_param));
	mem_param.size = VPU_SIZE_RESET_VECTOR;
	mem_param.fixed_addr = mva_reset_vector;
	mem_param.phy_addr = binpa_reset_vector;
	mem_param.kva_addr = vpu_core->bin_base +
				VPU_OFFSET_RESET_VECTOR +
				VPU_DDR_SHIFT_RESET_VECTOR * core;
	ret = vpu_alloc_shared_memory(vpu_device, &vpu_core->reset_vector,
					&mem_param);
	LOG_DBG("reset vector va (0x%lx),pa(0x%x)",
			(unsigned long)(vpu_core->reset_vector->va),
			vpu_core->reset_vector->pa);
	if (ret) {
		LOG_ERR("fail to allocate reset vector!\n");
		goto out;
	}

	/* 2. map main program */
	memset(&mem_param, 0x0, sizeof(mem_param));
	mem_param.size = VPU_SIZE_MAIN_PROGRAM;
	mem_param.fixed_addr = mva_main_program;
	mem_param.phy_addr = binpa_main_program;
	mem_param.kva_addr = vpu_core->bin_base +
				VPU_OFFSET_MAIN_PROGRAM +
				VPU_DDR_SHIFT_RESET_VECTOR * core;
	ret = vpu_alloc_shared_memory(vpu_device, &vpu_core->main_program,
					&mem_param);
	LOG_DBG("main program va (0x%lx),pa(0x%x)",
			(unsigned long)(vpu_core->main_program->va),
			vpu_core->main_program->pa);
	if (ret) {
		LOG_ERR("fail to allocate main program!\n");
		goto out;
	}

	/* 3. map main program iram data */
	/* no need reserved mva, use SG_READY*/
	memset(&mem_param, 0x0, sizeof(mem_param));
	mem_param.size = VPU_SIZE_MAIN_PROGRAM_IMEM;
	mem_param.phy_addr = binpa_iram_data;
	mem_param.kva_addr = vpu_core->bin_base +
				VPU_OFFSET_MAIN_PROGRAM_IMEM +
				VPU_DDR_SHIFT_IRAM_DATA * core;

	ret = vpu_alloc_shared_memory(vpu_device, &vpu_core->iram_data,
					&mem_param);
	LOG_DBG("iram data va (0x%lx),pa(0x%x)",
			(unsigned long)(vpu_core->iram_data->va),
			vpu_core->iram_data->pa);
	if (ret) {
		LOG_ERR("fail to allocate iram data!\n");
		goto out;
	}

	vpu_core->iram_data_mva = (uint64_t)(vpu_core->iram_data->pa);

out:
	if (ret)
		vpu_unmap_mva_of_bin(vpu_core);

	return ret;
}

void vpu_unmap_mva_of_bin(struct vpu_core *vpu_core)
{
	if (vpu_core->iram_data) {
		vpu_free_shared_memory(vpu_core->vpu_device,
			vpu_core->iram_data);
		vpu_core->iram_data = NULL;
		vpu_core->iram_data_mva = 0;
	}

	if (vpu_core->main_program) {
		vpu_free_shared_memory(vpu_core->vpu_device,
			vpu_core->main_program);
		vpu_core->main_program = NULL;
	}

	if (vpu_core->reset_vector) {
		vpu_free_shared_memory(vpu_core->vpu_device,
			vpu_core->reset_vector);
		vpu_core->reset_vector = NULL;
	}
}
#endif

int vpu_get_default_algo_num(struct vpu_core *vpu_core, vpu_id_t *algo_num)
{
	int ret = 0;

	*algo_num = vpu_core->default_algo_num;

	return ret;
}

int vpu_get_power(struct vpu_core *vpu_core, bool secure)
{
	int ret = 0;
	int core = vpu_core->core;

	LOG_DBG("[vpu_%d/%d] gp +\n", core, vpu_core->power_counter);
	mutex_lock(&vpu_core->power_counter_mutex);
	vpu_core->power_counter++;
	ret = vpu_boot_up(vpu_core, secure);
	mutex_unlock(&vpu_core->power_counter_mutex);
	LOG_DBG("[vpu_%d/%d] gp + 2\n", core, vpu_core->power_counter);
	if (ret == POWER_ON_MAGIC)
		vpu_get_power_set_opp(vpu_core);
	LOG_DBG("[vpu_%d/%d] gp -\n", core, vpu_core->power_counter);

	if (ret == POWER_ON_MAGIC)
		return 0;
	else
		return ret;
}

void vpu_put_power(struct vpu_core *vpu_core, enum VpuPowerOnType type)
{
	struct vpu_device *vpu_device = vpu_core->vpu_device;
	int core = vpu_core->core;

	LOG_DBG("[vpu_%d/%d] pp +\n", core, vpu_core->power_counter);
	mutex_lock(&vpu_core->power_counter_mutex);
	if (--vpu_core->power_counter == 0) {
		switch (type) {
		case VPT_PRE_ON:
			LOG_DBG("[vpu_%d] VPT_PRE_ON\n", core);
			mod_delayed_work(vpu_device->wq,
				&(vpu_core->power_counter_work),
				msecs_to_jiffies(10 * PWR_KEEP_TIME_MS));
			break;
		case VPT_IMT_OFF:
			LOG_INF("[vpu_%d] VPT_IMT_OFF\n", core);
			mod_delayed_work(vpu_device->wq,
				&(vpu_core->power_counter_work),
				msecs_to_jiffies(0));
			break;
		case VPT_ENQUE_ON:
		default:
			LOG_DBG("[vpu_%d] VPT_ENQUE_ON\n", core);
			mod_delayed_work(vpu_device->wq,
				&(vpu_core->power_counter_work),
				msecs_to_jiffies(PWR_KEEP_TIME_MS));
			break;
		}
	}
	mutex_unlock(&vpu_core->power_counter_mutex);
	LOG_DBG("[vpu_%d/%d] pp -\n", core, vpu_core->power_counter);
}

int vpu_set_power(struct vpu_user *user, struct vpu_power *power)
{
	int ret = 0;
	struct vpu_device *vpu_device = NULL;
	struct vpu_core *vpu_core = NULL;
	int i = 0, core = -1;

	vpu_device = dev_get_drvdata(user->dev);

	for (i = 0 ; i < vpu_device->core_num ; i++) {
		/*LOG_DBG("debug i(%d), (0x1 << i) (0x%x)", i, (0x1 << i));*/
		if (power->core == (0x1 << i)) {
			core = i;
			break;
		}
	}

	if (core >= vpu_device->core_num || core < 0) {
		LOG_ERR("wrong core index (0x%x/%d/%d)",
				power->core, core, vpu_device->core_num);
		ret = -1;
		return ret;
	}

	LOG_INF("[vpu_%d] set power opp:%d, pid=%d, tid=%d\n",
			core, power->opp_step,
			user->open_pid, user->open_tgid);


	vpu_core = vpu_device->vpu_core[core];
	ret = vpu_set_power_set_opp(vpu_core, power);
	if (ret != 0)
		return ret;

	user->power_opp = power->opp_step;

	ret = vpu_get_power(vpu_core, false);
	mutex_lock(&(vpu_core->state_mutex));
	vpu_core->state = VCT_IDLE;
	mutex_unlock(&(vpu_core->state_mutex));

	/* to avoid power leakage, power on/off need be paired */
	vpu_put_power(vpu_core, VPT_PRE_ON);
	LOG_INF("[vpu_%d] %s -\n", core, __func__);
	return ret;
}

static void vpu_power_counter_routine(struct work_struct *work)
{
	int core = 0;
	struct vpu_device *vpu_device = NULL;
	struct vpu_core *vpu_core = NULL;

	vpu_core = container_of(work, struct vpu_core, power_counter_work.work);
	core = vpu_core->core;
	vpu_device = vpu_core->vpu_device;
	LOG_DVFS("vpu_%d counterR (%d)+\n", core, vpu_core->power_counter);

	mutex_lock(&vpu_core->power_counter_mutex);
	if (vpu_core->power_counter == 0)
		vpu_shut_down(vpu_core);
	else
		LOG_DBG("vpu_%d no need this time.\n", core);
	mutex_unlock(&vpu_core->power_counter_mutex);

	LOG_DVFS("vpu_%d counterR -", core);
}

bool vpu_is_idle(struct vpu_core *vpu_core)
{
	bool idle = false;
	int core = vpu_core->core;

	mutex_lock(&(vpu_core->state_mutex));

	if (vpu_core->state == VCT_SHUTDOWN ||
		vpu_core->state == VCT_IDLE)
		idle = true;

	mutex_unlock(&(vpu_core->state_mutex));

	LOG_INF("%s vpu_%d, idle = %d, state = %d  !!\r\n", __func__,
		core, idle, vpu_core->state);

	return idle;
}

int vpu_quick_suspend(struct vpu_core *vpu_core)
{
	struct vpu_device *vpu_device = vpu_core->vpu_device;
	int core = vpu_core->core;

	LOG_DBG("[vpu_%d] q_suspend +\n", core);
	mutex_lock(&vpu_core->power_counter_mutex);
	LOG_INF("[vpu_%d] q_suspend (%d/%d)\n", core,
		vpu_core->power_counter, vpu_core->state);

	if (vpu_core->power_counter == 0) {
		mutex_unlock(&vpu_core->power_counter_mutex);

		mutex_lock(&(vpu_core->state_mutex));

		switch (vpu_core->state) {
		case VCT_SHUTDOWN:
		case VCT_NONE:
			/* vpu has already been shut down, do nothing*/
			mutex_unlock(&(vpu_core->state_mutex));
			break;
		case VCT_IDLE:
		case VCT_BOOTUP:
		case VCT_EXECUTING:
		case VCT_VCORE_CHG:
		default:
			mutex_unlock(&(vpu_core->state_mutex));
			mod_delayed_work(vpu_device->wq,
				&(vpu_core->power_counter_work),
				msecs_to_jiffies(0));
			break;
		}
	} else {
		mutex_unlock(&vpu_core->power_counter_mutex);
	}

	return 0;
}

int vpu_init_device(struct vpu_device *vpu_device)
{
	int i;
	int ret = 0;
	struct vpu_shared_memory_param mem_param;
	const uint64_t size_algos = VPU_SIZE_BINARY_CODE - VPU_OFFSET_ALGO_AREA
		- (VPU_SIZE_BINARY_CODE - VPU_OFFSET_MAIN_PROGRAM_IMEM);

	init_waitqueue_head(&vpu_device->cmd_wait);
	vpu_device->vpu_dump_exception = 0;
	INIT_DELAYED_WORK(&vpu_device->opp_keep_work, vpu_opp_keep_routine);
	INIT_DELAYED_WORK(&vpu_device->sdsp_work, vpu_sdsp_routine);
	vpu_device->sdsp_power_counter = 0;

	mutex_init(&vpu_device->lock_mutex);
	init_waitqueue_head(&vpu_device->lock_wait);
	mutex_init(&vpu_device->power_lock_mutex);
	init_waitqueue_head(&vpu_device->waitq_change_vcore);
	init_waitqueue_head(&vpu_device->waitq_do_core_executing);
	vpu_device->is_locked = false;
	mutex_init(&vpu_device->vpu_load_image_lock);

	vpu_device->vpu_num_users = 0;
	vpu_device->vpu_log_level = 1;

	vpu_device->in_sec_world = false;

	vbuf_init_phy_iova(vpu_device);

	if (vpu_device->vpu_load_image_state == VPU_LOAD_IMAGE_NONE) {
		/* map all algo binary data(src addr for dps to copy) */
		/* no need reserved mva, use SG_READY*/
		memset(&mem_param, 0x0, sizeof(mem_param));
		mem_param.size = size_algos;
		mem_param.phy_addr = vpu_device->bin_pa + VPU_OFFSET_ALGO_AREA;
		mem_param.kva_addr = vpu_device->bin_base +
					VPU_OFFSET_ALGO_AREA;

		ret = vpu_alloc_shared_memory(vpu_device,
					      &vpu_device->algo_binary_data,
					      &mem_param);
		if (ret) {
			LOG_ERR("fail to allocate algo buffer!\n");
			goto out;
		}

		/* assign mva of algo binary data to each core */
		for (i = 0; i < vpu_device->core_num; i++)
			vpu_device->vpu_core[i]->algo_data_mva =
				vpu_device->algo_binary_data->pa;
	}

	/* multi-core shared  */
	/* need in reserved region, set end as the end of reserved
	 * address, m4u use start-from
	 */
	memset(&mem_param, 0x0, sizeof(mem_param));
	mem_param.require_pa = true;
	mem_param.size = VPU_SIZE_SHARED_DATA;
	mem_param.fixed_addr = VPU_MVA_SHARED_DATA;

	ret = vpu_alloc_shared_memory(vpu_device,
					&(vpu_device->core_shared_data),
					&mem_param);
	LOG_DBG("shared_data va (0x%lx),pa(0x%x)",
			(unsigned long)(vpu_device->core_shared_data->va),
			vpu_device->core_shared_data->pa);
	if (ret) {
		LOG_ERR("fail to allocate working buffer!\n");
		goto out;
	}

	vpu_device->wq = create_workqueue("vpu_wq");
	vpu_device->func_mask = 0x0;

	vpu_qos_counter_init();
	vpu_init_opp(vpu_device);

	return 0;

out:
	vpu_uninit_device(vpu_device);
	return ret;
}

int vpu_uninit_device(struct vpu_device *vpu_device)
{
	int i;

	if (vpu_device->core_shared_data) {
		vpu_free_shared_memory(vpu_device,
					vpu_device->core_shared_data);
		vpu_device->core_shared_data = NULL;
	}

	if (vpu_device->vpu_load_image_state == VPU_LOAD_IMAGE_NONE) {
		if (vpu_device->algo_binary_data) {
			vpu_free_shared_memory(vpu_device,
					       vpu_device->algo_binary_data);
			vpu_device->algo_binary_data = NULL;
		}

		for (i = 0; i < vpu_device->core_num; i++)
			vpu_device->vpu_core[i]->algo_data_mva = 0;
	} else {
		vpu_unload_image(vpu_device);
	}

	if (vpu_device->wq) {
		flush_workqueue(vpu_device->wq);
		destroy_workqueue(vpu_device->wq);
		vpu_device->wq = NULL;
	}

	vbuf_deinit_phy_iova(vpu_device);
	vpu_uninit_opp(vpu_device);

	return 0;
}

int vpu_init_hw(struct vpu_core *vpu_core)
{
	struct vpu_device *vpu_device = vpu_core->vpu_device;
	int core = vpu_core->core;
	int ret = 0, i;
	struct vpu_shared_memory_param mem_param;

	INIT_DELAYED_WORK(&(vpu_core->power_counter_work),
				vpu_power_counter_routine);

#ifdef MTK_VPU_EMULATOR
	vpu_request_emulator_irq(vpu_device->irq_num[core], vpu_isr_handler);
#else
	if (vpu_device->vpu_load_image_state == VPU_LOAD_IMAGE_NONE) {
		ret = vpu_map_mva_of_bin(vpu_core, vpu_device->bin_pa);
		if (ret) {
			LOG_ERR("[vpu_%d]fail to map binary data!\n", core);
			goto out;
		}

		vpu_total_algo_num(vpu_core);
	}

	ret = request_irq(vpu_core->irq, (irq_handler_t)vpu_isr_handler,
				vpu_core->irq_flags | IRQF_SHARED,
				(const char *)vpu_core->name,
				vpu_core);

	if (ret) {
		LOG_ERR("[vpu_%d]fail to request vpu irq!\n", core);
		goto free_image;
	}
#endif


#ifdef MTK_VPU_FPGA_PORTING
	vpu_core->vpu_hw_support = true;
#else
	vpu_device->efuse_data = (get_devinfo_with_index(3) & 0x4000) >> 14;

	LOG_INF("efuse_data: efuse_data(0x%x)\n", vpu_device->efuse_data);

	vpu_core->vpu_hw_support = !(vpu_device->efuse_data & (1 << core));

	if (vpu_core->vpu_hw_support == false) {
		LOG_ERR("[vpu_%d]vpu hw don't support\n", core);
		ret = -EINVAL;
		goto free_irq;
	}
#endif

	mutex_init(&(vpu_core->power_mutex));
	mutex_init(&(vpu_core->power_counter_mutex));
	vpu_core->power_counter = 0;
	vpu_core->exception_isr_check = false;

#ifdef MET_VPU_LOG
	/* Init ftrace dumpwork information */
	spin_lock_init(&(vpu_core->ftrace_dump_work.my_lock));
	INIT_LIST_HEAD(&(vpu_core->ftrace_dump_work.list));
	INIT_WORK(&(vpu_core->ftrace_dump_work.my_work),
		vpu_dump_ftrace_workqueue);
#endif

	sprintf(vpu_core->vpu_wake_name, "vpu_wakelock_%d", core);
#ifdef CONFIG_PM_WAKELOCKS
	wakeup_source_init(&vpu_core->vpu_wake_lock, vpu_core->vpu_wake_name);
#else
	wake_lock_init(&(vpu_core->vpu_wake_lock), WAKE_LOCK_SUSPEND,
		       vpu_core->vpu_wake_name);
#endif

	vpu_core->srvc_task = kthread_create(vpu_service_routine,
					     vpu_core, vpu_core->name);
	if (IS_ERR(vpu_core->srvc_task)) {
		ret = -EINVAL;
		goto free_irq;
	}
#ifdef MET_VPU_LOG
	/* to save correct pid */
	vpu_core->ftrace_dump_work.pid = vpu_core->srvc_task->pid;
#endif
	wake_up_process(vpu_core->srvc_task);

	mutex_init(&(vpu_core->cmd_mutex));
	vpu_core->is_cmd_done = false;
	mutex_init(&(vpu_core->state_mutex));
	vpu_core->state = VCT_SHUTDOWN;

	/* working buffer */
	/* no need in reserved region */
	memset(&mem_param, 0x0, sizeof(mem_param));
	mem_param.require_pa = true;
	mem_param.size = VPU_SIZE_WORK_BUF;
	mem_param.fixed_addr = 0;

	ret = vpu_alloc_shared_memory(vpu_device, &(vpu_core->work_buf),
					&mem_param);

	LOG_INF("core(%d):work_buf va (0x%lx),pa(0x%x)\n",
		core, (unsigned long) (vpu_core->work_buf->va),
		vpu_core->work_buf->pa);
	if (ret) {
		LOG_ERR("core(%d):fail to allocate %s!\n",
			core, "working buffer");
		goto free_irq;
	}

	/* execution kernel library */
	/* need in reserved region, set end as the end of
	 * reserved address, m4u use start-from
	 */
	memset(&mem_param, 0x0, sizeof(mem_param));
	mem_param.require_pa = true;
	mem_param.size = VPU_SIZE_ALGO_KERNEL_LIB;
	mem_param.fixed_addr = g_vpu_mva_kernel_lib[core];
	ret = vpu_alloc_shared_memory(vpu_device,
					&(vpu_core->exec_kernel_lib),
					&mem_param);

	LOG_INF("core(%d):kernel_lib va (0x%lx),pa(0x%x)\n",
		core, (unsigned long)(vpu_core->exec_kernel_lib->va),
		vpu_core->exec_kernel_lib->pa);
	if (ret) {
		LOG_ERR("core(%d):fail to allocate %s!\n",
				core, "kernel_lib buffer");
		goto free_work_buf;
	}

	ret = vpu_init_pm(vpu_core);
	if (ret) {
		LOG_ERR("[vpu]fail to prepare regulator or clk!\n");
		goto free_kernel_lib;
	}

	/*init vpu lock power struct*/
	for (i = 0 ; i < VPU_OPP_PRIORIYY_NUM ; i++) {
		vpu_core->lock_power[i].core = 0;
		vpu_core->lock_power[i].max_boost_value = 100;
		vpu_core->lock_power[i].min_boost_value = 0;
		vpu_core->lock_power[i].lock = false;
		vpu_core->lock_power[i].priority = NORMAL;
	}

	vpu_core->min_opp = VPU_MAX_NUM_OPPS-1;
	vpu_core->max_opp = 0;
	vpu_core->min_boost = 0;
	vpu_core->max_boost = 100;

	/* pmqos  */
#ifdef ENABLE_PMQOS
	pm_qos_add_request(&vpu_core->vpu_qos_vcore_request,
			   PM_QOS_VCORE_OPP,
			   PM_QOS_VCORE_OPP_DEFAULT_VALUE);
#endif

	vpu_core->vpu_hw_init_done = 1;

	return 0;

free_kernel_lib:
	if (vpu_core->exec_kernel_lib) {
		vpu_free_shared_memory(vpu_device, vpu_core->exec_kernel_lib);
		vpu_core->exec_kernel_lib = NULL;
	}

free_work_buf:
	if (vpu_core->work_buf) {
		vpu_free_shared_memory(vpu_device, vpu_core->work_buf);
		vpu_core->work_buf = NULL;
	}

free_irq:
	/* Release IRQ */
	disable_irq(vpu_core->irq);
	free_irq(vpu_core->irq, vpu_core);

free_image:
	if (vpu_device->vpu_load_image_state == VPU_LOAD_IMAGE_NONE)
		vpu_unmap_mva_of_bin(vpu_core);
out:
	cancel_delayed_work(&(vpu_core->power_counter_work));

	return ret;
}

int vpu_uninit_hw(struct vpu_core *vpu_core)
{
	struct vpu_device *vpu_device = vpu_core->vpu_device;

	cancel_delayed_work(&(vpu_core->power_counter_work));

	if (!IS_ERR(vpu_core->srvc_task)) {
		if (vpu_device->vpu_init_done) {
			/* When task is running, task is able to be stopped.
			 * But, to call kthread_create and wake_up_process
			 * doesn't mean task is running.
			 */
			kthread_stop(vpu_core->srvc_task);
			vpu_core->srvc_task = NULL;
		}
	}

	if (vpu_core->exec_kernel_lib) {
		vpu_free_shared_memory(vpu_device, vpu_core->exec_kernel_lib);
		vpu_core->exec_kernel_lib = NULL;
	}

	if (vpu_core->work_buf) {
		vpu_free_shared_memory(vpu_device, vpu_core->work_buf);
		vpu_core->work_buf = NULL;
	}

	if (vpu_device->vpu_load_image_state == VPU_LOAD_IMAGE_NONE)
		vpu_unmap_mva_of_bin(vpu_core);

	/* pmqos  */
#ifdef ENABLE_PMQOS
	pm_qos_remove_request(&vpu_core->vpu_qos_vcore_request);
#endif

	vpu_release_pm(vpu_core);

	/* Release IRQ */
	disable_irq(vpu_core->irq);
	free_irq(vpu_core->irq, vpu_core);

	vpu_core->vpu_hw_init_done = 0;

	vpu_qos_counter_destroy();
	return 0;
}

static int vpu_check_precond(struct vpu_core *vpu_core)
{
	uint32_t status;
	size_t i;

	/* wait 1 seconds, if not ready or busy */
	for (i = 0; i < 50; i++) {
		status = vpu_read_field(vpu_core->vpu_base, FLD_XTENSA_INFO00);
		switch (status) {
		case VPU_STATE_READY:
		case VPU_STATE_IDLE:
		case VPU_STATE_ERROR:
			return 0;
		case VPU_STATE_NOT_READY:
		case VPU_STATE_BUSY:
			msleep(20);
			break;
		case VPU_STATE_TERMINATED:
			return -EBADFD;
		}
	}
	LOG_ERR("core(%d) still busy(%d) after wait 1 second.\n",
			vpu_core->core, status);
	return -EBUSY;
}

static int vpu_check_postcond(struct vpu_core *vpu_core)
{
	uint32_t status = vpu_read_field(vpu_core->vpu_base, FLD_XTENSA_INFO00);

	LOG_DBG("%s (0x%x)\n", __func__, status);

	switch (status) {
	case VPU_STATE_READY:
	case VPU_STATE_IDLE:
		return 0;
	case VPU_STATE_NOT_READY:
	case VPU_STATE_ERROR:
		return -EIO;
	case VPU_STATE_BUSY:
		return -EBUSY;
	case VPU_STATE_TERMINATED:
		return -EBADFD;
	default:
		return -EINVAL;
	}
}

int vpu_hw_enable_jtag(struct vpu_core *vpu_core, bool enabled)
{
	int ret = 0;

	vpu_get_power(vpu_core, false);

	vpu_write_field(vpu_core->vpu_base, FLD_SPNIDEN, enabled);
	vpu_write_field(vpu_core->vpu_base, FLD_SPIDEN, enabled);
	vpu_write_field(vpu_core->vpu_base, FLD_NIDEN, enabled);
	vpu_write_field(vpu_core->vpu_base, FLD_DBG_EN, enabled);

	vpu_put_power(vpu_core, VPT_ENQUE_ON);
	return ret;
}

int vpu_hw_boot_sequence(struct vpu_core *vpu_core)
{
	struct vpu_device *vpu_device = vpu_core->vpu_device;
	int ret;
	uint64_t ptr_ctrl;
	uint64_t ptr_reset;
	uint64_t ptr_axi_0;
	uint64_t ptr_axi_1;
	unsigned int reg_value = 0;
	int core = vpu_core->core;
	struct timespec start, end;
	uint64_t boot_latency = 0;

	vpu_trace_begin("%s", __func__);
	LOG_INF("[vpu_%d] boot-seq core(%d)\n", core, core);

	ktime_get_ts(&start);

	LOG_DBG("CTRL(0x%x)\n",
			vpu_read_reg32(vpu_core->vpu_base,
						CTRL_BASE_OFFSET + 0x110));
	LOG_DBG("XTENSA_INT(0x%x)\n",
			vpu_read_reg32(vpu_core->vpu_base,
						CTRL_BASE_OFFSET + 0x114));
	LOG_DBG("CTL_XTENSA_INT(0x%x)\n",
			vpu_read_reg32(vpu_core->vpu_base,
						CTRL_BASE_OFFSET + 0x118));
	LOG_DBG("CTL_XTENSA_INT_CLR(0x%x)\n",
			vpu_read_reg32(vpu_core->vpu_base,
						CTRL_BASE_OFFSET + 0x11C));

	lock_command(vpu_core);
	ptr_ctrl = vpu_core->vpu_base +
				g_vpu_reg_descs[REG_CTRL].offset;

	ptr_reset = vpu_core->vpu_base +
				g_vpu_reg_descs[REG_SW_RST].offset;

	ptr_axi_0 = vpu_core->vpu_base +
				g_vpu_reg_descs[REG_AXI_DEFAULT0].offset;

	ptr_axi_1 = vpu_core->vpu_base +
				g_vpu_reg_descs[REG_AXI_DEFAULT1].offset;

	/* 1. write register */
	/* set specific address for reset vector in external boot */
	reg_value = vpu_read_field(vpu_core->vpu_base,
					FLD_CORE_XTENSA_ALTRESETVEC);
	LOG_DBG("vpu bf ALTRESETVEC (0x%x), RV(0x%x)\n",
		reg_value, VPU_MVA_RESET_VECTOR);

	vpu_write_field(vpu_core->vpu_base, FLD_CORE_XTENSA_ALTRESETVEC,
				g_vpu_mva_reset_vector[core]);

	reg_value = vpu_read_field(vpu_core->vpu_base,
					FLD_CORE_XTENSA_ALTRESETVEC);
	LOG_DBG("vpu af ALTRESETVEC (0x%x), RV(0x%x)\n",
		reg_value, VPU_MVA_RESET_VECTOR);

	VPU_SET_BIT(ptr_ctrl, 31); /* csr_p_debug_enable */
	VPU_SET_BIT(ptr_ctrl, 26); /* debug interface cock gated enable */
	VPU_SET_BIT(ptr_ctrl, 19); /* force to boot based on X_ALTRESETVEC */
	VPU_SET_BIT(ptr_ctrl, 23); /* RUN_STALL pull up */
	VPU_SET_BIT(ptr_ctrl, 17); /* pif gated enable */

	VPU_CLR_BIT(ptr_ctrl, 29);
	VPU_CLR_BIT(ptr_ctrl, 28);
	VPU_CLR_BIT(ptr_ctrl, 27);

	VPU_SET_BIT(ptr_reset, 12); /* OCD_HALT_ON_RST pull up */
	ndelay(27); /* wait for 27ns */

	VPU_CLR_BIT(ptr_reset, 12); /* OCD_HALT_ON_RST pull down */
	if (core >= vpu_device->core_num) { /* set PRID */
		LOG_DBG("vpu set prid failed, core idx=%d invalid\n", core);
	} else {
		vpu_write_field(vpu_core->vpu_base, FLD_PRID, core);
	}
	VPU_SET_BIT(ptr_reset, 4); /* B_RST pull up */
	VPU_SET_BIT(ptr_reset, 8); /* D_RST pull up */
	ndelay(27); /* wait for 27ns */

	VPU_CLR_BIT(ptr_reset, 4); /* B_RST pull down */
	VPU_CLR_BIT(ptr_reset, 8); /* D_RST pull down */
	ndelay(27); /* wait for 27ns */

	/* pif gated disable, to prevent unknown propagate to BUS */
	VPU_CLR_BIT(ptr_ctrl, 17);
#ifndef BYPASS_M4U_DBG
	/* 8168 AXI Request via M4U */
	VPU_SET_BIT(ptr_axi_0, 20);
	VPU_SET_BIT(ptr_axi_0, 26);
	VPU_SET_BIT(ptr_axi_1, 3);
	VPU_SET_BIT(ptr_axi_1, 7);
#endif
	/* default set pre-ultra instead of ultra */
	VPU_SET_BIT(ptr_axi_0, 28);

	if (vpu_device->vpu_log_level > VpuLogThre_PERFORMANCE) {
		LOG_DBG("[vpu_%d] REG_AXI_DEFAULT0(0x%x)\n", core,
			vpu_read_reg32(vpu_core->vpu_base,
						CTRL_BASE_OFFSET + 0x13C));
	}

	LOG_DBG("[vpu_%d] REG_AXI_DEFAULT1(0x%x)\n", core,
			vpu_read_reg32(vpu_core->vpu_base,
						CTRL_BASE_OFFSET + 0x140));

	/* 2. trigger to run */
	LOG_DBG("vpu dsp:running (%d/0x%x)\n", core,
				vpu_read_field(vpu_core->vpu_base,
						FLD_SRAM_CONFIGURE));
	vpu_trace_begin("dsp:running");

	/* RUN_STALL pull down */
	VPU_CLR_BIT(ptr_ctrl, 23);

	/* 3. wait until done */
	ret = wait_command(vpu_core);
	ktime_get_ts(&end);

	ret |= vpu_check_postcond(vpu_core);  /* handle for err/exception isr */

	/* RUN_STALL pull up to avoid fake cmd */
	VPU_SET_BIT(ptr_ctrl, 23);

	vpu_trace_end();
	if (ret) {
		LOG_ERR("[vpu_%d] boot-up timeout , status(%d/%d), ret(%d)\n",
			core,
			vpu_read_field(vpu_core->vpu_base, FLD_XTENSA_INFO00),
			vpu_core->is_cmd_done, ret);
		vpu_dump_mesg(NULL, vpu_device);
		vpu_dump_register(NULL, vpu_device);
		vpu_dump_debug_stack(vpu_core, DEBUG_STACK_SIZE);
		vpu_dump_code_segment(vpu_core);
		vpu_aee("VPU Timeout", "timeout to external boot\n");
		goto out;
	}

	boot_latency += (uint64_t)(timespec_to_ns(&end) -
			timespec_to_ns(&start));
	if (vpu_device->vpu_log_level > Log_STATE_MACHINE) {
		LOG_INF("[vpu_%d]:boot sequence latency[%lld] ",
			core,
			boot_latency);
	}

	/* 4. check the result of boot sequence */
	ret = vpu_check_postcond(vpu_core);
	if (ret) {
		LOG_ERR("[vpu_%d]fail to boot vpu!\n", core);
		goto out;
	}

out:
	unlock_command(vpu_core);
	vpu_trace_end();
	vpu_write_field(vpu_core->vpu_base, FLD_APMCU_INT, 1);

	LOG_INF("[vpu_%d] hw_boot_sequence with clr INT-\n", core);
	return ret;
}

#ifdef MET_VPU_LOG
static int vpu_hw_set_log_option(struct vpu_core *vpu_core)
{
	struct vpu_device *vpu_device = vpu_core->vpu_device;
	int ret;

	/* update log enable and mpu enable */
	lock_command(vpu_core);
	/* set vpu internal log enable,disable */
	if (vpu_device->func_mask & VFM_ROUTINE_PRT_SYSLOG) {
		vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO01,
			VPU_CMD_SET_FTRACE_LOG);
		vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO05, 1);
	} else {
		vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO01,
			VPU_CMD_SET_FTRACE_LOG);
		vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO05, 0);
	}
	/* set vpu internal log level */
	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO06,
			vpu_core->vpu_device->vpu_internal_log_level);
	/* clear info18 */
	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO18, 0);
	vpu_write_field(vpu_core->vpu_base, FLD_RUN_STALL, 0);
	/* RUN_STALL pull down */
	vpu_write_field(vpu_core->vpu_base, FLD_CTL_INT, 1);
	ret = wait_command(vpu_core);
	vpu_write_field(vpu_core->vpu_base, FLD_RUN_STALL, 1);
	/* RUN_STALL pull up to avoid fake cmd */

	/* 4. check the result */
	ret = vpu_check_postcond(vpu_core);
	/*CHECK_RET("[vpu_%d]fail to set debug!\n", core);*/
	unlock_command(vpu_core);
	return ret;
}
#endif

int vpu_hw_set_debug(struct vpu_core *vpu_core)
{
	struct vpu_device *vpu_device = vpu_core->vpu_device;
	int ret;
	struct timespec now;
	unsigned int device_version = 0x0;
	int core = vpu_core->core;

	LOG_DBG("%s (%d)+\n", __func__, core);
	vpu_trace_begin("%s", __func__);

	lock_command(vpu_core);

	/* 1. set debug */
	getnstimeofday(&now);
	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO01,
				VPU_CMD_SET_DEBUG);
	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO19,
					vpu_core->iram_data_mva);

	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO21,
					vpu_core->work_buf->pa +
								VPU_OFFSET_LOG);

	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO22,
				VPU_SIZE_LOG_BUF);
	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO23,
					now.tv_sec * 1000000 +
						now.tv_nsec / 1000);

	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO29, HOST_VERSION);

	LOG_INF("[vpu_%d] %s=(0x%lx), INFO01(0x%x), 23(%d)\n",
		core,
		"work_buf->pa + VPU_OFFSET_LOG",
		(unsigned long)(vpu_core->work_buf->pa +
							VPU_OFFSET_LOG),
		vpu_read_field(vpu_core->vpu_base, FLD_XTENSA_INFO01),
		vpu_read_field(vpu_core->vpu_base, FLD_XTENSA_INFO23));

	LOG_DBG("vpu_set ok, running\n");

	/* 2. trigger interrupt */
	vpu_trace_begin("dsp:running");

	/* RUN_STALL pull down*/
	vpu_write_field(vpu_core->vpu_base, FLD_RUN_STALL, 0);
	vpu_write_field(vpu_core->vpu_base, FLD_CTL_INT, 1);
	LOG_DBG("debug timestamp: %.2lu:%.2lu:%.2lu:%.6lu\n",
			(now.tv_sec / 3600) % (24),
			(now.tv_sec / 60) % (60),
			now.tv_sec % 60,
			now.tv_nsec / 1000);

	/* 3. wait until done */
	ret = wait_command(vpu_core);
	ret |= vpu_check_postcond(vpu_core);

	/* RUN_STALL pull up to avoid fake cmd */
	vpu_write_field(vpu_core->vpu_base, FLD_RUN_STALL, 1);
	vpu_trace_end();

	/*3-additional. check vpu device/host version is matched or not*/
	device_version = vpu_read_field(vpu_core->vpu_base, FLD_XTENSA_INFO20);
	if ((int)device_version < (int)HOST_VERSION) {
		LOG_ERR("[vpu_%d] %s device(0x%x) v.s host(0x%x)\n",
			core,
			"wrong vpu version",
			vpu_read_field(vpu_core->vpu_base, FLD_XTENSA_INFO20),
			vpu_read_field(vpu_core->vpu_base, FLD_XTENSA_INFO29));
		ret = -EIO;
	}

	if (ret) {
		LOG_ERR("[vpu_%d] %s, status(%d/%d), ret(%d)\n",
			core,
			"set-debug timeout/fail",
			vpu_read_field(vpu_core->vpu_base, FLD_XTENSA_INFO00),
			vpu_core->is_cmd_done, ret);

		LOG_INF("%s(%d)(%d)(%d.%d)(%d)(%d/%d/%d)%d\n",
			"set-debug timeout ",
			core,
			vpu_device->is_power_debug_lock,
			vpu_device->opps.vcore.index,
			vpu_device->opps.apu.index,
			vpu_device->max_dsp_freq,
			vpu_core->force_change_vcore_opp,
			vpu_core->force_change_dsp_freq,
			vpu_core->change_freq_first,
			vpu_device->opp_keep_flag);
		vpu_dump_mesg(NULL, vpu_device);
		vpu_dump_register(NULL, vpu_device);
		vpu_dump_debug_stack(vpu_core, DEBUG_STACK_SIZE);
		vpu_dump_code_segment(vpu_core);
		vpu_aee("VPU Timeout",
			"core_%d timeout to do set_debug, (%d/%d), ret(%d)\n",
			core,
			vpu_read_field(vpu_core->vpu_base, FLD_XTENSA_INFO00),
			vpu_core->is_cmd_done, ret);
	}

	if (ret) {
		LOG_ERR("[vpu_%d]timeout of set debug\n", core);
		goto out;
	}

	/* 4. check the result */
	ret = vpu_check_postcond(vpu_core);
	if (ret) {
		LOG_ERR("[vpu_%d]fail to set debug!\n", core);
		goto out;
	}

out:
	unlock_command(vpu_core);
#ifdef MET_VPU_LOG
	/* to set vpu ftrace log */
	ret = vpu_hw_set_log_option(vpu_core);
#endif
	vpu_trace_end();
	LOG_INF("[vpu_%d] hw_set_debug - version check(0x%x/0x%x)\n",
			core,
			vpu_read_field(vpu_core->vpu_base, FLD_XTENSA_INFO20),
			vpu_read_field(vpu_core->vpu_base, FLD_XTENSA_INFO29));
	return ret;
}

int vpu_get_name_of_algo(struct vpu_core *vpu_core, int id, char **name)
{
	int i;
	int tmp = id;
	struct vpu_image_header *header;

	header = vpu_core->vpu_device->image_header;

	for (i = 0; i < VPU_NUMS_IMAGE_HEADER; i++) {
		if (tmp > header[i].algo_info_count) {
			tmp -= header[i].algo_info_count;
			continue;
		}

		*name = header[i].algo_infos[tmp - 1].name;
		return 0;
	}

	*name = NULL;
	LOG_ERR("algo is not existed, id=%d\n", id);
	return -ENOENT;
}

int vpu_total_algo_num(struct vpu_core *vpu_core)
{
	int i;
	int total = 0;
	struct vpu_image_header *header;

	LOG_DBG("[vpu] %s +\n", __func__);

	header = vpu_core->vpu_device->image_header;

	for (i = 0; i < VPU_NUMS_IMAGE_HEADER; i++)
		total += header[i].algo_info_count;

	vpu_core->default_algo_num = total;

	return total;
}

int vpu_get_entry_of_algo(struct vpu_core *vpu_core, char *name, int *id,
	unsigned int *mva, int *length)
{
	int i, j;
	int s = 1;
	unsigned int coreMagicNum;
	struct vpu_algo_info *algo_info;
	struct vpu_image_header *header;
	int core = vpu_core->core;

	LOG_DBG("[vpu] %s +\n", __func__);
	/* coreMagicNum = ( 0x60 | (0x01 << core) ); */
	/* ignore vpu version */
	coreMagicNum = (0x01 << core);

	header = vpu_core->vpu_device->image_header;
	for (i = 0; i < VPU_NUMS_IMAGE_HEADER; i++) {
		for (j = 0; j < header[i].algo_info_count; j++) {
			algo_info = &header[i].algo_infos[j];
			LOG_INF("%s: %s/%s, %s:0x%x, %s:%d, %s:0x%x, 0x%x\n",
					"algo name", name, algo_info->name,
					"core info", (algo_info->vpu_core),
					"input core", core,
					"magicNum", coreMagicNum,
					algo_info->vpu_core & coreMagicNum);
			/* CHRISTODO */
			if ((strcmp(name, algo_info->name) == 0) &&
				(algo_info->vpu_core & coreMagicNum)) {
				LOG_INF("[%d] algo_info->offset(0x%x)/0x%x",
					core,
					algo_info->offset,
					(unsigned int)
					(vpu_core->algo_data_mva));

				*mva = algo_info->offset -
					 VPU_OFFSET_ALGO_AREA +
					 vpu_core->algo_data_mva;

				LOG_INF("[%d] *mva(0x%x/0x%lx), s(%d)",
						core, *mva,
						(unsigned long)(*mva), s);

				*length = algo_info->length;
				*id = s;
				return 0;
			}
			s++;
		}
	}

	*id = 0;
	LOG_ERR("algo is not existed, name=%s\n", name);
	return -ENOENT;
}

int vpu_ext_be_busy(struct vpu_core *vpu_core)
{
	int ret;
	/* CHRISTODO */

	lock_command(vpu_core);

	/* 1. write register */
	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO01,
				VPU_CMD_EXT_BUSY);

	/* 2. trigger interrupt */
	vpu_write_field(vpu_core->vpu_base, FLD_CTL_INT, 1);

	/* 3. wait until done */
	ret = wait_command(vpu_core);

	unlock_command(vpu_core);
	return ret;
}

int vpu_debug_func_core_state(struct vpu_core *vpu_core,
				enum VpuCoreState state)
{
	mutex_lock(&(vpu_core->state_mutex));
	vpu_core->state = state;
	mutex_unlock(&(vpu_core->state_mutex));
	return 0;
}

int vpu_boot_up(struct vpu_core *vpu_core, bool secure)
{
	struct vpu_device *vpu_device = vpu_core->vpu_device;
	int ret = 0;
	int core;
	/*secure flag is for sdsp force shut down*/

	core = vpu_core->core;
	LOG_DBG("[vpu_%d] boot_up +\n", core);
	mutex_lock(&vpu_core->power_mutex);
	LOG_DBG("[vpu_%d] is_power_on(%d)\n", core, vpu_core->is_power_on);
	if (vpu_core->is_power_on) {
		if (secure) {
			if (vpu_core->power_counter != 1)
				LOG_ERR("vpu_%d power counter %d > 1 .\n",
				core, vpu_core->power_counter);
			LOG_WRN("force shut down for sdsp..\n");
			mutex_unlock(&vpu_core->power_mutex);
			vpu_shut_down(vpu_core);
			mutex_lock(&vpu_core->power_mutex);
		} else {
			mutex_unlock(&vpu_core->power_mutex);
			mutex_lock(&(vpu_core->state_mutex));
			vpu_core->state = VCT_BOOTUP;
			mutex_unlock(&(vpu_core->state_mutex));
			wake_up_interruptible(&vpu_device->waitq_change_vcore);
			return POWER_ON_MAGIC;
		}
	}
	LOG_DBG("[vpu_%d] boot_up flag2\n", core);

	vpu_trace_begin("%s", __func__);
	mutex_lock(&(vpu_core->state_mutex));
	vpu_core->state = VCT_BOOTUP;
	mutex_unlock(&(vpu_core->state_mutex));
	wake_up_interruptible(&vpu_device->waitq_change_vcore);

	ret = vpu_enable_regulator_and_clock(vpu_core);
	if (ret) {
		LOG_ERR("[vpu_%d]fail to enable regulator or clock\n", core);
		goto out;
	}

	/* core: clear CG */
	vpu_init_reg(vpu_core);
	LOG_DBG("[vpu_%d] init_reg done\n", core);

	if (!secure) {
		ret = vpu_hw_boot_sequence(vpu_core);
		if (ret) {
			LOG_ERR("[vpu_%d]fail to do boot sequence\n", core);
			goto out;
		}

		LOG_DBG("[vpu_%d] vpu_hw_boot_sequence done\n", core);

		ret = vpu_hw_set_debug(vpu_core);
		if (ret) {
			LOG_ERR("[vpu_%d]fail to set debug\n", core);
			goto out;
		}
	}

	LOG_DBG("[vpu_%d] vpu_hw_set_debug done\n", core);

#ifdef MET_POLLING_MODE
	if (vpu_device->func_mask & VFM_ROUTINE_PRT_SYSLOG) {
		ret = vpu_profile_state_set(vpu_core, 1);
		if (ret) {
			LOG_ERR("[vpu_%d] fail to profile_state_set 1\n", core);
			goto out;
		}
	}
#endif

out:
#if 0 /* control on/off outside the via get_power/put_power */
	if (ret) {
		ret = vpu_disable_regulator_and_clock(core);
		if (ret) {
			LOG_ERR("[vpu_%d]fail to disable regulator and clk\n",
					core);
			goto out;
		}
	}
#endif
	vpu_trace_end();
	mutex_unlock(&vpu_core->power_mutex);

#ifdef MTK_PERF_OBSERVER
	if (!ret) {
		struct pob_xpufreq_info pxi;

		pxi.id = core;
		pxi.opp = vpu_device->opps.apu.index;

		pob_xpufreq_update(POB_XPUFREQ_VPU, &pxi);
	}
#endif

	vpu_qos_counter_start(core);

	return ret;
}

int vpu_shut_down(struct vpu_core *vpu_core)
{
	struct vpu_device *vpu_device = vpu_core->vpu_device;
	int ret = 0;
	int core = vpu_core->core;

	vpu_qos_counter_end(core);

	LOG_DBG("[vpu_%d] shutdown +\n", core);
	mutex_lock(&vpu_core->power_mutex);
	if (!vpu_core->is_power_on) {
		mutex_unlock(&vpu_core->power_mutex);
		return 0;
	}

	mutex_lock(&(vpu_core->state_mutex));
	switch (vpu_core->state) {
	case VCT_SHUTDOWN:
	case VCT_IDLE:
	case VCT_NONE:
#ifdef MTK_VPU_FPGA_PORTING
	case VCT_BOOTUP:
#endif
		vpu_core->current_algo = 0;
		vpu_core->state = VCT_SHUTDOWN;
		mutex_unlock(&(vpu_core->state_mutex));
		break;
#ifndef MTK_VPU_FPGA_PORTING
	case VCT_BOOTUP:
#endif
	case VCT_EXECUTING:
	case VCT_VCORE_CHG:
		mutex_unlock(&(vpu_core->state_mutex));
		goto out;
		/*break;*/
	}

#ifdef MET_POLLING_MODE
	if (vpu_device->func_mask & VFM_ROUTINE_PRT_SYSLOG) {
		ret = vpu_profile_state_set(vpu_core, 0);
		if (ret) {
			LOG_ERR("[vpu_%d] fail to profile_state_set 0\n", core);
			goto out;
		}
	}
#endif

	vpu_trace_begin("%s", __func__);
	ret = vpu_disable_regulator_and_clock(vpu_core);
	if (ret) {
		LOG_ERR("[vpu_%d]fail to disable regulator and clock\n", core);
		goto out;
	}

	wake_up_interruptible(&vpu_device->waitq_change_vcore);
out:
	vpu_trace_end();
	mutex_unlock(&vpu_core->power_mutex);
	LOG_DBG("[vpu_%d] shutdown -\n", core);

#ifdef MTK_PERF_OBSERVER
	if (!ret) {
		struct pob_xpufreq_info pxi;

		pxi.id = core;
		pxi.opp = -1;

		pob_xpufreq_update(POB_XPUFREQ_VPU, &pxi);
	}
#endif

	return ret;
}

int vpu_hw_load_algo(struct vpu_core *vpu_core, struct vpu_algo *algo)
{
	struct vpu_device *vpu_device = vpu_core->vpu_device;
	struct vpu_dvfs_opps *opps = &vpu_device->opps;
	int ret;
	int core = vpu_core->core;

	LOG_DBG("[vpu_%d] %s +\n", core, __func__);
	/* no need to reload algo if have same loaded algo*/
	if (vpu_core->current_algo == algo->id[core])
		return 0;

	vpu_trace_begin("%s(%d)", __func__, algo->id[core]);
	ret = vpu_get_power(vpu_core, false);
	if (ret) {
		LOG_ERR("[vpu_%d]fail to get power!\n", core);
		goto out;
	}

	LOG_DBG("[vpu_%d] %s done\n", core, __func__);

	ret = wait_to_do_vpu_running(vpu_core);
	if (ret) {
		LOG_ERR("[vpu_%d] %s, ret=%d\n", core,
			"load_algo fail to wait_to_do_vpu_running!",
			ret);
		goto out;
	}

	mutex_lock(&(vpu_core->state_mutex));
	vpu_core->state = VCT_EXECUTING;
	mutex_unlock(&(vpu_core->state_mutex));

	lock_command(vpu_core);
	LOG_DBG("start to load algo\n");

	ret = vpu_check_precond(vpu_core);
	if (ret) {
		LOG_ERR("[vpu_%d]have wrong status before do loader!\n", core);
		goto out;
	}

	LOG_DBG("[vpu_%d] vpu_check_precond done\n", core);

	LOG_DBG("[vpu_%d] algo ptr/length (0x%lx/0x%x)\n", core,
		(unsigned long)algo->bin_ptr, algo->bin_length);
	/* 1. write register */
	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO01,
				VPU_CMD_DO_LOADER);

	/* binary data's address */
	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO12, algo->bin_ptr);

	/* binary data's length */
	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO13,
				algo->bin_length);

	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO15,
				opps->apu.values[opps->apu.index]);

	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO16,
				opps->apu_if.values[opps->apu_if.index]);

	/* 2. trigger interrupt */
	vpu_trace_begin("dsp:running");
	LOG_DBG("[vpu_%d] dsp:running\n", core);

	/* RUN_STALL down */
	vpu_write_field(vpu_core->vpu_base, FLD_RUN_STALL, 0);
	vpu_write_field(vpu_core->vpu_base, FLD_CTL_INT, 1);

	/* 3. wait until done */
	ret = wait_command(vpu_core);
	LOG_DBG("[vpu_%d] algo done\n", core);

	/* RUN_STALL pull up to avoid fake cmd */
	vpu_write_field(vpu_core->vpu_base, FLD_RUN_STALL, 1);
	vpu_trace_end();
	if (ret) {
		LOG_ERR("[vpu_%d] load_algo timeout, status(%d/%d), ret(%d)\n",
				core,
				vpu_read_field(vpu_core->vpu_base,
						FLD_XTENSA_INFO00),
				vpu_core->is_cmd_done, ret);

		vpu_dump_mesg(NULL, vpu_device);
		vpu_dump_register(NULL, vpu_device);
		vpu_dump_debug_stack(vpu_core, DEBUG_STACK_SIZE);
		vpu_dump_code_segment(vpu_core);
		vpu_dump_algo_segment(vpu_core, algo->id[core], 0x0);
		vpu_aee("VPU Timeout",
			"core_%d timeout to do loader, algo_id=%d\n", core,
			vpu_core->current_algo);
		goto out;
	}

	/* 4. update the id of loaded algo */
	vpu_core->current_algo = algo->id[core];

out:
	unlock_command(vpu_core);
	if (ret) {
		mutex_lock(&(vpu_core->state_mutex));
		vpu_core->state = VCT_SHUTDOWN;
		mutex_unlock(&(vpu_core->state_mutex));
		/*vpu_put_power(core, VPT_IMT_OFF);*/
		/* blocking use same ththread to avoid race between
		 * delayedworkQ and serviceThread
		 */
		mutex_lock(&vpu_core->power_counter_mutex);
		vpu_core->power_counter--;
		mutex_unlock(&vpu_core->power_counter_mutex);
		LOG_ERR("[vpu_%d] pr hw error, force shutdown, cnt(%d)\n",
				core, vpu_core->power_counter);
		if (vpu_core->power_counter == 0)
			vpu_shut_down(vpu_core);
	} else {
		vpu_put_power(vpu_core, VPT_ENQUE_ON);
	}

	vpu_trace_end();
	LOG_DBG("[vpu] %s -\n", __func__);
	return ret;
}

int vpu_hw_enque_request(struct vpu_core *vpu_core, struct vpu_request *request)
{
	struct vpu_device *vpu_device = vpu_core->vpu_device;
	int ret;
	int core = vpu_core->core;

	LOG_DBG("[vpu_%d/%d] eq + ", core, request->algo_id[core]);

	vpu_trace_begin("%s(%d)", __func__, request->algo_id[core]);
	ret = vpu_get_power(vpu_core, false);
	if (ret) {
		LOG_ERR("[vpu_%d]fail to get power!\n", core);
		goto out;
	}

	ret = wait_to_do_vpu_running(vpu_core);
	if (ret) {
		LOG_ERR("[vpu_%d] %s, ret = %d\n", core,
			"enq fail to wait_to_do_vpu_running!",
			ret);
		goto out;
	}

	mutex_lock(&(vpu_core->state_mutex));
	vpu_core->state = VCT_EXECUTING;
	mutex_unlock(&(vpu_core->state_mutex));

	lock_command(vpu_core);
	LOG_DBG("start to enque request\n");

	ret = vpu_check_precond(vpu_core);
	if (ret) {
		request->status = VPU_REQ_STATUS_BUSY;
		LOG_ERR("error state before enque request!\n");
		goto out;
	}

	memcpy((void *) (uintptr_t)vpu_core->work_buf->va,
			request->buffers,
			sizeof(struct vpu_buffer) * request->buffer_count);

	if (vpu_device->vpu_log_level > VpuLogThre_DUMP_BUF_MVA)
		vpu_dump_buffer_mva(request);
	LOG_INF("[vpu_%d] start d2d, id/frm (%d/%d)\n", core,
		request->algo_id[core], request->frame_magic);
	/* 1. write register */
	/* command: d2d */
	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO01, VPU_CMD_DO_D2D);
	/* buffer count */
	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO12,
				request->buffer_count);
	/* pointer to array of struct vpu_buffer */
	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO13,
				vpu_core->work_buf->pa);
	/* pointer to property buffer */
	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO14,
				request->sett_ptr);
	/* size of property buffer */
	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO15,
				request->sett_length);

	/* 2. trigger interrupt */

	vpu_trace_begin("dsp:running");
	LOG_DBG("[vpu] %s running... ", __func__);
	#if defined(VPU_MET_READY)
	MET_Events_Trace(vpu_device, 1, core, request->algo_id[core]);
	#endif

	/* RUN_STALL pull down */
	vpu_write_field(vpu_core->vpu_base, FLD_RUN_STALL, 0);
	vpu_write_field(vpu_core->vpu_base, FLD_CTL_INT, 1);

	/* 3. wait until done */
	ret = wait_command(vpu_core);
	LOG_DBG("[vpu_%d] end d2d\n", core);

	/* RUN_STALL pull up to avoid fake cmd */
	vpu_write_field(vpu_core->vpu_base, FLD_RUN_STALL, 1);

	vpu_trace_end();
	#if defined(VPU_MET_READY)
	MET_Events_Trace(vpu_device, 0, core, request->algo_id[core]);
	#endif
	if (ret) {
		request->status = VPU_REQ_STATUS_TIMEOUT;
		LOG_ERR("[vpu_%d] %s, status(%d/%d), ret(%d)\n", core,
			"hw_enque_request timeout",
			vpu_read_field(vpu_core->vpu_base, FLD_XTENSA_INFO00),
			vpu_core->is_cmd_done, ret);

		vpu_dump_buffer_mva(request);
		vpu_dump_mesg(NULL, vpu_device);
		vpu_dump_register(NULL, vpu_device);
		vpu_dump_debug_stack(vpu_core, DEBUG_STACK_SIZE);
		vpu_dump_code_segment(vpu_core);
		vpu_aee("VPU Timeout",
			"core_%d timeout to do d2d, algo_id=%d\n", core,
			vpu_core->current_algo);
		goto out;
	}

	request->status = (vpu_check_postcond(vpu_core)) ?
			VPU_REQ_STATUS_FAILURE : VPU_REQ_STATUS_SUCCESS;

out:
	unlock_command(vpu_core);
	if (ret) {
		mutex_lock(&(vpu_core->state_mutex));
		vpu_core->state = VCT_SHUTDOWN;
		mutex_unlock(&(vpu_core->state_mutex));
		/* vpu_put_power(core, VPT_IMT_OFF); */
		/* blocking use same ththread to avoid race between
		 * delayedworkQ and serviceThread
		 */
		mutex_lock(&vpu_core->power_counter_mutex);
		vpu_core->power_counter--;
		mutex_unlock(&vpu_core->power_counter_mutex);

		LOG_ERR("[vpu_%d] pr hw error, force shutdown, cnt(%d)\n",
				core, vpu_core->power_counter);

		if (vpu_core->power_counter == 0)
			vpu_shut_down(vpu_core);
	} else {
		vpu_put_power(vpu_core, VPT_ENQUE_ON);
	}

	vpu_trace_end();
	LOG_DBG("[vpu] %s - (%d)", __func__, request->status);
	return ret;
}

/* do whole processing for enque request, including check algo, load algo,
 * run d2d. minimize timing gap between each step for a single eqneu request
 * and minimize the risk of timing issue
 */
int vpu_hw_processing_request(struct vpu_core *vpu_core,
				struct vpu_request *request)
{
	struct vpu_device *vpu_device = vpu_core->vpu_device;
	struct vpu_dvfs_opps *opps = &vpu_device->opps;
	int ret;
	struct vpu_algo *algo = NULL;
	bool need_reload = false;
	struct timespec start, end;
	uint64_t latency = 0;
	uint64_t load_latency = 0;
	int core = vpu_core->core;

	LOG_INF("%s, lock sdsp(%d) in + ", __func__, core);

	mutex_lock(&vpu_core->sdsp_control_mutex);

	LOG_INF("%s, lock sdsp(%d) in - ", __func__, core);

	if (vpu_device->vpu_log_level > Log_ALGO_OPP_INFO)
		LOG_INF("[vpu_%d/%d] pr + ", core, request->algo_id[core]);

	/* step1, enable clocks and boot-up if needed */
	ret = vpu_get_power(vpu_core, false);
	if (ret) {
		LOG_ERR("[vpu_%d] fail to get power!\n", core);
		goto out2;
	}
	LOG_DBG("[vpu_%d] vpu_get_power done\n", core);

	/* step2. check algo */
	if (request->algo_id[core] != vpu_core->current_algo) {
		struct vpu_user *user = (struct vpu_user *)request->user_id;

		ret = vpu_find_algo_by_id(vpu_core,
			request->algo_id[core], &algo, user);
		need_reload = true;
		if (ret) {
			request->status = VPU_REQ_STATUS_INVALID;
			LOG_ERR("[vpu_%d] pr can not find the algo, id=%d\n",
				core, request->algo_id[core]);
			goto out2;
		}
	}

	/* step3. do processing, algo loader and d2d*/
	ret = wait_to_do_vpu_running(vpu_core);
	if (ret) {
		LOG_ERR("[vpu_%d] %s, ret=%d\n", core,
			"pr load_algo fail to wait_to_do_vpu_running!",
			ret);
		goto out;
	}

	mutex_lock(&(vpu_core->state_mutex));
	vpu_core->state = VCT_EXECUTING;
	mutex_unlock(&(vpu_core->state_mutex));

	/* algo loader if needed */
	if (need_reload) {
		vpu_trace_begin("[vpu_%d] hw_load_algo(%d)",
						core, algo->id[core]);
		lock_command(vpu_core);
		vpu_utilization_compute_enter(vpu_core);
		LOG_DBG("start to load algo\n");

		ret = vpu_check_precond(vpu_core);
		if (ret) {
			LOG_ERR("[vpu_%d]have wrong status before do loader!\n",
					core);
			goto out;
		}

		LOG_DBG("[vpu_%d] vpu_check_precond done\n", core);

		if (vpu_device->vpu_log_level > Log_ALGO_OPP_INFO)
			LOG_INF(
				"[vpu_%d] algo_%d ptr/length (0x%lx/0x%x), bf(%d)\n",
				core, algo->id[core],
				(unsigned long)algo->bin_ptr,
				algo->bin_length,
				vpu_core->is_cmd_done);

		/* 1. write register */
		vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO01,
					VPU_CMD_DO_LOADER);

		/* binary data's address */
		vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO12,
					algo->bin_ptr);

		/* binary data's length */
		vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO13,
					algo->bin_length);
		vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO15,
					opps->apu.values[opps->apu.index]);

		vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO16,
				opps->apu_if.values[opps->apu_if.index]);

		/* 2. trigger interrupt */
		vpu_trace_begin("[vpu_%d] dsp:load_algo running", core);
		LOG_DBG("[vpu_%d] dsp:load_algo running\n", core);

		if (vpu_device->vpu_log_level > Log_STATE_MACHINE) {
			LOG_INF("[0x%lx]:load algo start ",
				(unsigned long)request->request_id);
		}

		/* RUN_STALL down */
		vpu_write_field(vpu_core->vpu_base, FLD_RUN_STALL, 0);
		vpu_write_field(vpu_core->vpu_base, FLD_CTL_INT, 1);
		ktime_get_ts(&start);

		/* 3. wait until done */
		ret = wait_command(vpu_core);

		ktime_get_ts(&end);

		/* handle for err/exception isr */
		if (vpu_core->exception_isr_check)
			ret |= vpu_check_postcond(vpu_core);

		if (vpu_device->vpu_log_level > Log_ALGO_OPP_INFO)
			LOG_INF(
				"[vpu_%d] algo_%d %s=0x%lx, %s=%d, %s=%d, %s=%d, %d\n",
				core, algo->id[core],
				"bin_ptr", (unsigned long)algo->bin_ptr,
				"done", vpu_core->is_cmd_done,
				"ret", ret,
				"info00",
				vpu_read_field(vpu_core->vpu_base,
						FLD_XTENSA_INFO00),
				vpu_core->exception_isr_check);
		if (ret) {
			vpu_core->exception_isr_check = false;
			LOG_ERR("[vpu_%d] %s, status(%d/%d), ret(%d), %d\n",
				core,
				"pr load_algo err",
				vpu_read_field(vpu_core->vpu_base,
						FLD_XTENSA_INFO00),
				vpu_core->is_cmd_done,
				ret, vpu_core->exception_isr_check);
		} else {
			LOG_DBG("[vpu_%d] temp test1.2\n", core);

			/* RUN_STALL pull up to avoid fake cmd */
			vpu_write_field(vpu_core->vpu_base, FLD_RUN_STALL, 1);
		}

		load_latency += (uint64_t)(timespec_to_ns(&end) -
			timespec_to_ns(&start));
		if (vpu_device->vpu_log_level > Log_STATE_MACHINE) {
			LOG_INF("[0x%lx]:load algo latency[%lld] ",
				(unsigned long)request->request_id,
				load_latency);
		}

		vpu_trace_end();

		if (ret) {
			request->status = VPU_REQ_STATUS_TIMEOUT;
			LOG_ERR("[vpu_%d] %s, status(%d/%d), ret(%d)\n",
				core,
				"pr_load_algo timeout/fail",
				vpu_read_field(vpu_core->vpu_base,
						FLD_XTENSA_INFO00),
				vpu_core->is_cmd_done,
				ret);
			vpu_dump_mesg(NULL, vpu_device);
			vpu_dump_register(NULL, vpu_device);
			vpu_dump_debug_stack(vpu_core, DEBUG_STACK_SIZE);
			vpu_dump_code_segment(vpu_core);
			vpu_dump_algo_segment(vpu_core, request->algo_id[core],
							0x0);
			vpu_aee("VPU Timeout",
				"core_%d timeout to do loader, algo_id=%d\n",
				core,
				vpu_core->current_algo);
			goto out;
		}

		/* 4. update the id of loaded algo */
		vpu_core->current_algo = algo->id[core];
		vpu_utilization_compute_leave(vpu_core);
		unlock_command(vpu_core);
		vpu_trace_end();
	}

	/* d2d operation */
	vpu_trace_begin("[vpu_%d] hw_processing_request(%d)",
				core, request->algo_id[core]);

	LOG_DBG("start to enque request\n");
	lock_command(vpu_core);
	vpu_utilization_compute_enter(vpu_core);
	ret = vpu_check_precond(vpu_core);
	if (ret) {
		request->status = VPU_REQ_STATUS_BUSY;
		LOG_ERR("error state before enque request!\n");
		goto out;
	}

	memcpy((void *) (uintptr_t)vpu_core->work_buf->va,
			request->buffers,
			sizeof(struct vpu_buffer) * request->buffer_count);

	if (vpu_device->vpu_log_level > VpuLogThre_DUMP_BUF_MVA)
		vpu_dump_buffer_mva(request);
	LOG_DBG("[vpu_%d]start d2d, %s(%d/%d), %s(%d), %s(%d/%d,%d), %s(%d)\n",
		core,
		"id/frm", request->algo_id[core], request->frame_magic,
		"bw", request->power_param.bw,
		"algo", vpu_core->current_algo,
		request->algo_id[core], need_reload,
		"done", vpu_core->is_cmd_done);
	/* 1. write register */
	/* command: d2d */
	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO01, VPU_CMD_DO_D2D);
	/* buffer count */
	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO12,
				request->buffer_count);
	/* pointer to array of struct vpu_buffer */
	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO13,
				vpu_core->work_buf->pa);
	/* pointer to property buffer */
	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO14,
				request->sett_ptr);
	/* size of property buffer */
	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO15,
				request->sett_length);

	/* 2. trigger interrupt */
	vpu_trace_begin("[vpu_%d] dsp:d2d running", core);
	LOG_DBG("[vpu_%d] d2d running...\n", core);
	#if defined(VPU_MET_READY)
	MET_Events_Trace(vpu_device, 1, core, request->algo_id[core]);
	#endif

	vpu_cmd_qos_start(core);

	if (vpu_device->vpu_log_level > Log_ALGO_OPP_INFO)
		LOG_INF("[0x%lx]:d2d start ",
			(unsigned long)request->request_id);

	/* RUN_STALL pull down */
	vpu_write_field(vpu_core->vpu_base, FLD_RUN_STALL, 0);
	vpu_write_field(vpu_core->vpu_base, FLD_CTL_INT, 1);
	ktime_get_ts(&start);

	/* 3. wait until done */
	ret = wait_command(vpu_core);

	ktime_get_ts(&end);

	/* handle for err/exception isr */
	if (vpu_core->exception_isr_check)
		ret |= vpu_check_postcond(vpu_core);

	if (vpu_device->vpu_log_level > VpuLogThre_PERFORMANCE) {
		LOG_INF("[vpu_%d] end d2d, done(%d), ret(%d), info00(%d), %d\n",
			core,
			vpu_core->is_cmd_done, ret,
			vpu_read_field(vpu_core->vpu_base, FLD_XTENSA_INFO00),
			vpu_core->exception_isr_check);
	}

	if (ret) {
		vpu_core->exception_isr_check = false;
		LOG_ERR("[vpu_%d] hw_d2d err, status(%d/%d), ret(%d), %d\n",
			core,
			vpu_read_field(vpu_core->vpu_base, FLD_XTENSA_INFO00),
			vpu_core->is_cmd_done,
			ret, vpu_core->exception_isr_check);
	} else {
		LOG_DBG("[vpu_%d] temp test2.2\n", core);

		/* RUN_STALL pull up to avoid fake cmd */
		vpu_write_field(vpu_core->vpu_base, FLD_RUN_STALL, 1);
	}

	latency += (uint64_t)(timespec_to_ns(&end) -
		timespec_to_ns(&start));
	if (vpu_device->vpu_log_level > Log_STATE_MACHINE) {
		LOG_INF("[0x%lx]: algo d2d latency[%lld] ",
			(unsigned long)request->request_id, latency);
	}

	vpu_trace_end();
	#if defined(VPU_MET_READY)
	MET_Events_Trace(vpu_device, 0, core, request->algo_id[core]);
	#endif
	LOG_DBG("[vpu_%d] hw_d2d test , status(%d/%d), ret(%d)\n",
			core,
			vpu_read_field(vpu_core->vpu_base, FLD_XTENSA_INFO00),
			vpu_core->is_cmd_done, ret);
	if (ret) {
		request->status = VPU_REQ_STATUS_TIMEOUT;
		LOG_ERR("[vpu_%d] %s, status(%d/%d), ret(%d)\n",
				core,
				"hw_d2d timeout/fail",
				vpu_read_field(vpu_core->vpu_base,
						FLD_XTENSA_INFO00),
				vpu_core->is_cmd_done,
				ret);

		vpu_dump_buffer_mva(request);
		vpu_dump_mesg(NULL, vpu_device);
		vpu_dump_register(NULL, vpu_device);
		vpu_dump_debug_stack(vpu_core, DEBUG_STACK_SIZE);
		vpu_dump_code_segment(vpu_core);
		vpu_dump_algo_segment(vpu_core, request->algo_id[core], 0x0);

		LOG_INF("%s(%d)(%d)(%d.%d)(%d/%d)(%d/%d/%d)%d\n",
			"timeout to do d2d ",
			core,
			vpu_device->is_power_debug_lock,
			vpu_device->opps.vcore.index,
			vpu_device->opps.apu.index,
			vpu_device->max_vcore_opp,
			vpu_device->max_dsp_freq,
			vpu_core->force_change_vcore_opp,
			vpu_core->force_change_dsp_freq,
			vpu_core->change_freq_first,
			vpu_device->opp_keep_flag);
		vpu_aee("VPU Timeout",
			"core_%d timeout to do d2d, algo_id=%d\n", core,
			vpu_core->current_algo);
		goto out;
	}

	request->status = (vpu_check_postcond(vpu_core)) ?
			VPU_REQ_STATUS_FAILURE : VPU_REQ_STATUS_SUCCESS;
out:
	vpu_utilization_compute_leave(vpu_core);
	unlock_command(vpu_core);
	vpu_trace_end();
out2:
	if (ret) {
		mutex_lock(&(vpu_core->state_mutex));
		vpu_core->state = VCT_SHUTDOWN;
		mutex_unlock(&(vpu_core->state_mutex));
		/* vpu_put_power(core, VPT_IMT_OFF);*/
		/* blocking use same ththread to avoid race between
		 * delayedworkQ and serviceThread
		 */
		mutex_lock(&vpu_core->power_counter_mutex);
		vpu_core->power_counter--;
		mutex_unlock(&vpu_core->power_counter_mutex);

		LOG_ERR("[vpu_%d] pr hw error, force shutdown, cnt(%d)\n",
				core, vpu_core->power_counter);

		if (vpu_core->power_counter == 0)
			vpu_shut_down(vpu_core);
	} else {
		vpu_put_power(vpu_core, VPT_ENQUE_ON);
	}
	LOG_DBG("[vpu] %s - (%d)", __func__, request->status);
	request->busy_time = (uint64_t)latency;
	/*Todo*/
	//request->bandwidth=(uint32_t)get_vpu_bw();
	request->bandwidth = vpu_cmd_qos_end(core);
	if (vpu_device->vpu_log_level > Log_STATE_MACHINE) {
		LOG_INF("[0x%lx]:vpu busy_time[%lld],bw[%d] ",
			(unsigned long)request->request_id,
			request->busy_time, request->bandwidth);
	}

	mutex_unlock(&vpu_core->sdsp_control_mutex);
	LOG_INF("%s, unlock sdsp(%d) in - ", __func__, core);

	return ret;
}

int vpu_hw_get_algo_info(struct vpu_core *vpu_core, struct vpu_algo *algo)
{
	int ret = 0;
	int port_count = 0;
	int info_desc_count = 0;
	int sett_desc_count = 0;
	unsigned int ofs_ports, ofs_info, ofs_info_descs, ofs_sett_descs;
	int i;
	int core = vpu_core->core;

	vpu_trace_begin("%s(%d)", __func__, algo->id[core]);
	ret = vpu_get_power(vpu_core, false);
	if (ret) {
		LOG_ERR("[vpu_%d]fail to get power!\n", core);
		goto out;
	}

	lock_command(vpu_core);
	LOG_DBG("start to get algo, algo_id=%d\n", algo->id[core]);

	ret = vpu_check_precond(vpu_core);
	if (ret) {
		LOG_ERR("[vpu_%d]have wrong status before get algo!\n", core);
		goto out;
	}

	ofs_ports = 0;
	ofs_info = sizeof(((struct vpu_algo *)0)->ports);
	ofs_info_descs = ofs_info + algo->info_length;
	ofs_sett_descs = ofs_info_descs +
			sizeof(((struct vpu_algo *)0)->info_descs);

	LOG_INF("[vpu_%d] %s check precond done\n", core, __func__);

	/* 1. write register */
	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO01,
				VPU_CMD_GET_ALGO);
	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO06,
				vpu_core->work_buf->pa + ofs_ports);
	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO07,
				vpu_core->work_buf->pa + ofs_info);
	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO08,
				algo->info_length);
	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO10,
			vpu_core->work_buf->pa + ofs_info_descs);
	vpu_write_field(vpu_core->vpu_base, FLD_XTENSA_INFO12,
			vpu_core->work_buf->pa + ofs_sett_descs);

	/* 2. trigger interrupt */
	vpu_trace_begin("dsp:running");
	LOG_DBG("[vpu] %s running...\n", __func__);

	/* RUN_STALL pull down */
	vpu_write_field(vpu_core->vpu_base, FLD_RUN_STALL, 0);
	vpu_write_field(vpu_core->vpu_base, FLD_CTL_INT, 1);

	/* 3. wait until done */
	ret = wait_command(vpu_core);
	LOG_INF("[vpu_%d] VPU_CMD_GET_ALGO done\n", core);
	vpu_trace_end();
	if (ret) {
		vpu_dump_mesg(NULL, vpu_core->vpu_device);
		vpu_dump_register(NULL, vpu_core->vpu_device);
		vpu_dump_debug_stack(vpu_core, DEBUG_STACK_SIZE);
		vpu_dump_code_segment(vpu_core);
		vpu_aee("VPU Timeout",
			"core_%d timeout to get algo, algo_id=%d\n", core,
			vpu_core->current_algo);
		goto out;
	}

	/* RUN_STALL pull up to avoid fake cmd */
	vpu_write_field(vpu_core->vpu_base, FLD_RUN_STALL, 1);

	/* 4. get the return value */
	port_count = vpu_read_field(vpu_core->vpu_base, FLD_XTENSA_INFO05);
	info_desc_count = vpu_read_field(vpu_core->vpu_base, FLD_XTENSA_INFO09);
	sett_desc_count = vpu_read_field(vpu_core->vpu_base, FLD_XTENSA_INFO11);
	algo->port_count = port_count;
	algo->info_desc_count = info_desc_count;
	algo->sett_desc_count = sett_desc_count;

	LOG_DBG("end of get algo, %s=%d, %s=%d, %s=%d\n",
		"port_count", port_count,
		"info_desc_count", info_desc_count,
		"sett_desc_count", sett_desc_count);

	/* 5. write back data from working buffer */
	memcpy((void *)(uintptr_t)algo->ports,
		    (void *)((uintptr_t)vpu_core->work_buf->va +
							ofs_ports),
			sizeof(struct vpu_port) * port_count);

	for (i = 0 ; i < algo->port_count ; i++) {
		LOG_DBG("port %d.. id=%d, name=%s, dir=%d, usage=%d\n",
			i, algo->ports[i].id, algo->ports[i].name,
			algo->ports[i].dir, algo->ports[i].usage);
	}

	memcpy((void *)(uintptr_t)algo->info_ptr,
		     (void *)((uintptr_t)vpu_core->work_buf->va +
							ofs_info),
			algo->info_length);

	memcpy((void *)(uintptr_t)algo->info_descs,
		     (void *)((uintptr_t)vpu_core->work_buf->va +
							ofs_info_descs),
			sizeof(struct vpu_prop_desc) * info_desc_count);

	memcpy((void *)(uintptr_t)algo->sett_descs,
		     (void *)((uintptr_t)vpu_core->work_buf->va +
							ofs_sett_descs),
			sizeof(struct vpu_prop_desc) * sett_desc_count);

	LOG_DBG("end of get algo 2, %s=%d, %s=%d, %s=%d\n",
		"port_count", algo->port_count,
		"info_desc_count", algo->info_desc_count,
		"sett_desc_count", algo->sett_desc_count);

out:
	unlock_command(vpu_core);
	vpu_put_power(vpu_core, VPT_ENQUE_ON);
	vpu_trace_end();
	LOG_DBG("[vpu] %s -\n", __func__);
	return ret;
}

void vpu_hw_lock(struct vpu_user *user)
{
	/* CHRISTODO */
	struct vpu_device *vpu_device = dev_get_drvdata(user->dev);

	if (user->locked)
		LOG_ERR("double locking bug, pid=%d, tid=%d\n",
				user->open_pid, user->open_tgid);
	else {
		mutex_lock(&vpu_device->lock_mutex);
		vpu_device->is_locked = true;
		user->locked = true;
		vpu_get_power(vpu_device->vpu_core[0], false);
	}
}

void vpu_hw_unlock(struct vpu_user *user)
{
	/* CHRISTODO */
	struct vpu_device *vpu_device = dev_get_drvdata(user->dev);

	if (user->locked) {
		vpu_put_power(vpu_device->vpu_core[0], VPT_ENQUE_ON);
		vpu_device->is_locked = false;
		user->locked = false;
		wake_up_interruptible(&vpu_device->lock_wait);
		mutex_unlock(&vpu_device->lock_mutex);
	} else
		LOG_ERR("should not unlock while unlocked, pid=%d, tid=%d\n",
				user->open_pid, user->open_tgid);
}

int vpu_alloc_shared_memory(struct vpu_device *vpu_device,
				struct vpu_shared_memory **shmem,
				struct vpu_shared_memory_param *param)
{
	int ret = 0;
	struct vpu_kernel_buf *vkbuf;
	enum vkbuf_map_case map_case = VKBUF_MAP_FPHY_FIOVA;

	*shmem = kzalloc(sizeof(struct vpu_shared_memory), GFP_KERNEL);
	ret = (*shmem == NULL);
	if (ret) {
		LOG_ERR("fail to kzalloc 'struct memory'!\n");
		goto out1;
	}

	if (param->require_pa == false) {
		if (param->fixed_addr)
			map_case = VKBUF_MAP_FPHY_FIOVA;
		else
			map_case = VKBUF_MAP_FPHY_DIOVA;
	} else {
		if (param->fixed_addr)
			map_case = VKBUF_MAP_DPHY_FIOVA;
		else
			map_case = VKBUF_MAP_DPHY_DIOVA;
	}

	vkbuf = vbuf_kmap_phy_iova(vpu_device, map_case, param->phy_addr,
					param->kva_addr, param->fixed_addr,
					param->size);
	if (vkbuf == NULL) {
		LOG_ERR("fail to vbuf_kmap_phy_iova!\n");
		ret = -ENOMEM;
		goto out1;
	}

	(*shmem)->vkbuf = vkbuf;
	(*shmem)->handle = (void *)(uintptr_t)vkbuf->handle;
	(*shmem)->pa = vkbuf->iova_addr;
	(*shmem)->length = vkbuf->size;
	(*shmem)->va = (uint64_t)(uintptr_t)vkbuf->kva;

	ret = ((*shmem)->va) ? 0 : -ENOMEM;
	if (ret) {
		LOG_ERR("fail to map va of buffer!\n");
		goto out;
	}
	return 0;

out:
	vbuf_kunmap_phy_iova(vpu_device, (*shmem)->vkbuf);
out1:
	if (*shmem) {
		kfree(*shmem);
		*shmem = NULL;
	}

	return ret;
}

void vpu_free_shared_memory(struct vpu_device *vpu_device,
				struct vpu_shared_memory *shmem)
{
	if (shmem == NULL)
		return;
	vbuf_kunmap_phy_iova(vpu_device, shmem->vkbuf);
	kfree(shmem);
}

int vpu_dump_buffer_mva(struct vpu_request *request)
{
	struct vpu_buffer *buf;
	struct vpu_plane *plane;
	int i, j;

	LOG_INF("dump request - setting: 0x%x, length: %d\n",
			(uint32_t) request->sett_ptr, request->sett_length);

	for (i = 0; i < request->buffer_count; i++) {
		buf = &request->buffers[i];
		LOG_INF("  buffer[%d] - %s:%d, %s:%dx%d, %s:%d\n",
				i,
				"port", buf->port_id,
				"size", buf->width, buf->height,
				"format", buf->format);

		for (j = 0; j < buf->plane_count; j++) {
			plane = &buf->planes[j];
			LOG_INF("	 plane[%d] - %s:0x%x, %s:%d, %s:%d\n",
				j,
				"ptr", (uint32_t) plane->ptr,
				"length", plane->length,
				"stride", plane->stride);
		}
	}

	return 0;
}

int vpu_dump_vpu_memory(struct seq_file *s, struct vpu_device *gvpu_device)
{
	struct vpu_device *vpu_device;
	struct vpu_core *vpu_core;
	int i = 0;
	int core = 0;
	unsigned int size = 0x0;
	unsigned long addr = 0x0;
	unsigned int dump_addr = 0x0;
	unsigned int value_1, value_2, value_3, value_4;
	unsigned int bin_offset = 0x0;
	//unsigned int vpu_domain_addr = 0x0;
	//unsigned int vpu_dump_size = 0x0;
	//unsigned int vpu_shift_offset = 0x0;

	if (gvpu_device) {
		vpu_device = gvpu_device;
	} else {
		if (s == NULL || s->private == NULL) {
			LOG_ERR("[vpu seq_file] seq_file error");
			return -EINVAL;
		}

		vpu_device = (struct vpu_device *)s->private;
	}

	vpu_core = vpu_device->vpu_core[core];

	vpu_print_seq(s, "===%s, vpu_dump_exception = 0x%x===\n",
		__func__, vpu_device->vpu_dump_exception);

	if ((vpu_device->vpu_dump_exception & 0xFFFF0000)
		!= VPU_EXCEPTION_MAGIC)
		return 0;

	core = vpu_device->vpu_dump_exception & 0x000000FF;

	vpu_print_seq(s, "==========%s, core_%d===========\n", __func__, core);

	vpu_print_seq(s, "=====core service state=%d=====\n",
		vpu_core->state);

	if (core >= vpu_device->core_num) {
		vpu_print_seq(s, "vpu_dump_exception data error...\n");
		return 0;
	}


#if 0
	vpu_print_seq(s, "[vpu_%d] hw_d2d err, status(%d/%d), %d\n",
		core,
		vpu_read_field(vpu_core->vpu_base, FLD_XTENSA_INFO00),
		vpu_service_cores[core].is_cmd_done,
		vpu_core->exception_isr_check);

	vpu_dump_register(s);
	vpu_dump_mesg(s);
#else
	vpu_print_seq(s, "======== dump message=======\n");

	vpu_dump_mesg(s, vpu_device);

	vpu_print_seq(s, "========no dump register=======\n");
#endif

#if 0
	vpu_print_seq(s, " ========== stack segment dump start ==========\n");

	/* dmem 0 */
	vpu_domain_addr = 0x7FF00000;
	vpu_shift_offset = 0x0;
	vpu_dump_size = 0x20000;
	vpu_print_seq(s, "==========dmem0 => 0x%x/0x%x: 0x%x==============\n",
		vpu_domain_addr, vpu_shift_offset, vpu_dump_size);
	for (i = 0 ; i < (int)vpu_dump_size / 4 ; i = i + 4) {
		vpu_print_seq(s, "%08X %08X %08X %08X %08X\n", vpu_domain_addr,
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i)),
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i + 4)),
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i + 8)),
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i + 12)));
		vpu_domain_addr += (4 * 4);
	}
	/* dmem 1 */
	vpu_domain_addr = 0x7FF20000;
	vpu_shift_offset = 0x20000;
	vpu_dump_size = 0x20000;
	vpu_print_seq(s, "==========dmem1 => 0x%x/0x%x: 0x%x==============\n",
		vpu_domain_addr, vpu_shift_offset, vpu_dump_size);
	for (i = 0 ; i < (int)vpu_dump_size / 4 ; i = i + 4) {
		vpu_print_seq(s, "%08X %08X %08X %08X %08X\n", vpu_domain_addr,
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i)),
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i + 4)),
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i + 8)),
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i + 12)));
		vpu_domain_addr += (4 * 4);
	}
	/* imem */
	vpu_domain_addr = 0x7FF40000;
	vpu_shift_offset = 0x40000;
	vpu_dump_size = 0x10000;
	vpu_print_seq(s, "==========imem => 0x%x/0x%x: 0x%x==============\n",
		vpu_domain_addr, vpu_shift_offset, vpu_dump_size);
	for (i = 0 ; i < (int)vpu_dump_size / 4 ; i = i + 4) {
		vpu_print_seq(s, "%08X %08X %08X %08X %08X\n", vpu_domain_addr,
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i)),
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i + 4)),
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i + 8)),
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i + 12)));
		vpu_domain_addr += (4 * 4);
	}
#else
	vpu_print_seq(s, " ========== no dump stack segment ==========\n");

#endif
	vpu_print_seq(s, "\n\n\n===code segment dump start===\n\n\n");

	vpu_print_seq(s, "\n\n\n==main code segment_reset_vector==\n\n\n\n");

	dump_addr = g_vpu_mva_reset_vector[core];
	bin_offset = VPU_DDR_SHIFT_RESET_VECTOR * core;
	size = DEBUG_MAIN_CODE_SEG_SIZE_1; /* define by mon/jackie*/
	vpu_print_seq(s, "==============0x%x/0x%x/0x%x/0x%x==============\n",
			bin_offset, dump_addr,
			size, VPU_SIZE_RESET_VECTOR);
	for (i = 0 ; i < (int)size / 4 ; i = i + 4) {
		value_1 = vpu_read_reg32(vpu_core->bin_base,
			bin_offset + (4 * i));
		value_2 = vpu_read_reg32(vpu_core->bin_base,
			bin_offset + (4 * i + 4));
		value_3 = vpu_read_reg32(vpu_core->bin_base,
			bin_offset + (4 * i + 8));
		value_4 = vpu_read_reg32(vpu_core->bin_base,
			bin_offset + (4 * i + 12));
		vpu_print_seq(s, "%08X %08X %08X %08X %08X\n", dump_addr,
			value_1, value_2, value_3, value_4);
		dump_addr += (4 * 4);
	}


	vpu_print_seq(s, "===main code segment_main_program===\n");
	dump_addr = g_vpu_mva_main_program[core];
	bin_offset = VPU_DDR_SHIFT_RESET_VECTOR * core +
					VPU_OFFSET_MAIN_PROGRAM;
	size = DEBUG_MAIN_CODE_SEG_SIZE_2; /* define by mon/jackie*/
	vpu_print_seq(s, "==============0x%x/0x%x/0x%x/0x%x==============\n",
			bin_offset, dump_addr,
			size, VPU_SIZE_MAIN_PROGRAM);
	for (i = 0 ; i < (int)size / 4 ; i = i + 4) {
		value_1 = vpu_read_reg32(vpu_core->bin_base,
			bin_offset + (4 * i));
		value_2 = vpu_read_reg32(vpu_core->bin_base,
			bin_offset + (4 * i + 4));
		value_3 = vpu_read_reg32(vpu_core->bin_base,
			bin_offset + (4 * i + 8));
		value_4 = vpu_read_reg32(vpu_core->bin_base,
			bin_offset + (4 * i + 12));
		vpu_print_seq(s, "%08X %08X %08X %08X %08X\n", dump_addr,
			value_1, value_2, value_3, value_4);
		dump_addr += (4 * 4);
	}

	vpu_print_seq(s, "============== kernel code segment ==============\n");
	dump_addr = g_vpu_mva_kernel_lib[core];
	addr = (unsigned long)(vpu_core->exec_kernel_lib->va);
	size = DEBUG_CODE_SEG_SIZE; /* define by mon/jackie*/
	vpu_print_seq(s, "==============0x%lx/0x%x/0x%x/0x%x==============\n",
			addr, dump_addr,
			size, DEBUG_CODE_SEG_SIZE);
	for (i = 0 ; i < (int)size / 4 ; i = i + 4) {
		value_1 = (unsigned int)
			(*((unsigned long *)((uintptr_t)addr + (4 * i))));

		value_2 = (unsigned int)
			(*((unsigned long *)((uintptr_t)addr + (4 * i + 4))));

		value_3 = (unsigned int)
			(*((unsigned long *)((uintptr_t)addr + (4 * i + 8))));

		value_4 = (unsigned int)
			(*((unsigned long *)((uintptr_t)addr + (4 * i + 12))));

		vpu_print_seq(s, "%08X %08X %08X %08X %08X\n", dump_addr,
			value_1, value_2, value_3, value_4);

		dump_addr += (4 * 4);
	}

	return 0;
}

int vpu_dump_register(struct seq_file *s, struct vpu_device *gvpu_device)
{
	int i, j;
	bool first_row_of_field;
	struct vpu_reg_desc *reg;
	struct vpu_reg_field_desc *field;
	struct vpu_device *vpu_device;
	struct vpu_core *vpu_core;
	int temp_core = 0;
	unsigned int vpu_dump_addr = 0x0;
	const char *line_bar =
		"  +------------------+-------+---+---+------------------------------------------+----------+\n"
		;

	if (gvpu_device) {
		vpu_device = gvpu_device;
	} else {
		if (s == NULL || s->private == NULL) {
			LOG_ERR("[vpu seq_file] seq_file error");
			return -EINVAL;
		}

		vpu_device = (struct vpu_device *)s->private;
	}

	vpu_print_seq(s, "%s", line_bar);
	for (temp_core = 0; temp_core < vpu_device->core_num; temp_core++) {
		vpu_print_seq(s, "  |Core: %-82d|\n", temp_core);
		vpu_print_seq(s, "%s", line_bar);
		vpu_print_seq(s, "  |%-18s|%-7s|%-3s|%-3s|%-42s|%-10s|\n",
			"Register", "Offset", "MSB", "LSB", "Field", "Value");
		vpu_print_seq(s, "%s", line_bar);

		vpu_core = vpu_device->vpu_core[temp_core];
		for (i = 0; i < VPU_NUM_REGS; i++) {
			reg = &g_vpu_reg_descs[i];
#if 0
			if (reg->reg < REG_DEBUG_INFO00)
				continue;
#endif
			first_row_of_field = true;

			for (j = 0; j < VPU_NUM_REG_FIELDS; j++) {
				field = &g_vpu_reg_field_descs[j];
				if (reg->reg != field->reg)
					continue;

				if (first_row_of_field) {
					first_row_of_field = false;
					vpu_print_seq(s,
							"  |%-18s|0x%-5.5x|%-3d|%-3d|%-42s|0x%-8.8x|\n",
							reg->name,
							reg->offset,
							field->msb,
							field->lsb,
							field->name,
							vpu_read_field(
							vpu_core->vpu_base, j));
				} else {
					vpu_print_seq(s,
							"  |%-18s|%-7s|%-3d|%-3d|%-42s|0x%-8.8x|\n",
							"", "",
							field->msb,
							field->lsb,
							field->name,
							vpu_read_field(
							vpu_core->vpu_base, j));
				}
			}
			vpu_print_seq(s, "%s", line_bar);
		}
	}

	vpu_print_seq(s, "%s", line_bar);

	for (temp_core = 0; temp_core < vpu_device->core_num; temp_core++) {
		vpu_print_seq(s, "%s", line_bar);
		/* ipu_cores */
		if (temp_core == 0)
			vpu_dump_addr = 0x19180000;
		else
			vpu_dump_addr = 0x19280000;

		vpu_core = vpu_device->vpu_core[temp_core];
		for (i = 0 ; i < (int)(0x20C) / 4 ; i = i + 4) {
			LOG_WRN("%08X %08X %08X %08X %08X\n", vpu_dump_addr,
			vpu_read_reg32(vpu_core->vpu_base,
					CTRL_BASE_OFFSET + (4 * i)),
			vpu_read_reg32(vpu_core->vpu_base,
					CTRL_BASE_OFFSET + (4 * i + 4)),
			vpu_read_reg32(vpu_core->vpu_base,
					CTRL_BASE_OFFSET + (4 * i + 8)),
			vpu_read_reg32(vpu_core->vpu_base,
					CTRL_BASE_OFFSET + (4 * i + 12)));
			vpu_dump_addr += (4 * 4);
		}
	}

	return 0;
}

int vpu_dump_image_file(struct seq_file *s, struct vpu_device *gvpu_device)
{
	struct vpu_device *vpu_device;
	int i, j, id = 1;
	struct vpu_algo_info *algo_info;
	struct vpu_image_header *header;
	int core = 0;
	struct vpu_core *vpu_core;
	const char *line_bar =
		"  +------+-----+--------------------------------+--------+-----------+----------+\n"
		;
	int ret;

	if (gvpu_device) {
		vpu_device = gvpu_device;
	} else {
		if (s == NULL || s->private == NULL) {
			LOG_ERR("[vpu seq_file] seq_file error");
			return -EINVAL;
		}

		vpu_device = (struct vpu_device *)s->private;
	}

	if (!vpu_device || !vpu_device->vpu_core[core])
		return -EINVAL;

	ret = vpu_check_load_image_state(vpu_device);
	if (ret)
		return -EINVAL;

	vpu_core = vpu_device->vpu_core[core];

	vpu_print_seq(s, "%s", line_bar);
	vpu_print_seq(s, "  |%-6s|%-5s|%-32s|%-8s|%-11s|%-10s|\n",
			"Header", "Id", "Name", "MagicNum", "MVA", "Length");
	vpu_print_seq(s, "%s", line_bar);

	header = vpu_core->vpu_device->image_header;
	for (i = 0; i < VPU_NUMS_IMAGE_HEADER; i++) {
		for (j = 0; j < header[i].algo_info_count; j++) {
			algo_info = &header[i].algo_infos[j];

			vpu_print_seq(s,
			   "  |%-6d|%-5d|%-32s|0x%-6lx|0x%-9lx|0x%-8x|\n",
			   (i + 1),
			   id,
			   algo_info->name,
			   (unsigned long)(algo_info->vpu_core),
			   algo_info->offset - VPU_OFFSET_ALGO_AREA +
			      (uintptr_t)vpu_core->algo_data_mva,
			   algo_info->length);

			id++;
		}
	}

	vpu_print_seq(s, "%s", line_bar);

/* #ifdef MTK_VPU_DUMP_BINARY */
#if 0
	{
		uint32_t dump_1k_size = (0x00000400);
		unsigned char *ptr = NULL;

		vpu_print_seq(s, "Reset Vector Data:\n");
		ptr = (unsigned char *) vpu_service_cores[core].bin_base +
				VPU_OFFSET_RESET_VECTOR;
		for (i = 0; i < dump_1k_size / 2; i++, ptr++) {
			if (i % 16 == 0)
				vpu_print_seq(s, "\n%07X0h: ", i / 16);

			vpu_print_seq(s, "%02X ", *ptr);
		}
		vpu_print_seq(s, "\n");
		vpu_print_seq(s, "\n");
		vpu_print_seq(s, "Main Program Data:\n");
		ptr = (unsigned char *) vpu_service_cores[core].bin_base +
				VPU_OFFSET_MAIN_PROGRAM;
		for (i = 0; i < dump_1k_size; i++, ptr++) {
			if (i % 16 == 0)
				vpu_print_seq(s, "\n%07X0h: ", i / 16);

			vpu_print_seq(s, "%02X ", *ptr);
		}
		vpu_print_seq(s, "\n");
	}
#endif

	return 0;
}

void vpu_dump_debug_stack(struct vpu_core *vpu_core, int size)
{
	int i = 0;
	unsigned int vpu_domain_addr = 0x0;
	unsigned int vpu_dump_size = 0x0;
	unsigned int vpu_shift_offset = 0x0;
	int core = vpu_core->core;

	vpu_domain_addr = ((DEBUG_STACK_BASE_OFFSET & 0x000fffff) | 0x7FF00000);

	LOG_ERR("===========%s, core_%d============\n", __func__, core);
	/* dmem 0 */
	vpu_domain_addr = 0x7FF00000;
	vpu_shift_offset = 0x0;
	vpu_dump_size = 0x20000;
	LOG_WRN("==========dmem0 => 0x%x/0x%x: 0x%x==============\n",
		vpu_domain_addr, vpu_shift_offset, vpu_dump_size);
	for (i = 0 ; i < (int)vpu_dump_size / 4 ; i = i + 4) {
		LOG_WRN("%08X %08X %08X %08X %08X\n", vpu_domain_addr,
			vpu_read_reg32(vpu_core->vpu_base,
			vpu_shift_offset + (4 * i)),
			vpu_read_reg32(vpu_core->vpu_base,
			vpu_shift_offset + (4 * i + 4)),
			vpu_read_reg32(vpu_core->vpu_base,
			vpu_shift_offset + (4 * i + 8)),
			vpu_read_reg32(vpu_core->vpu_base,
			vpu_shift_offset + (4 * i + 12)));
		vpu_domain_addr += (4 * 4);
		mdelay(1);
	}
	/* dmem 1 */
	vpu_domain_addr = 0x7FF20000;
	vpu_shift_offset = 0x20000;
	vpu_dump_size = 0x20000;
	LOG_WRN("==========dmem1 => 0x%x/0x%x: 0x%x==============\n",
		vpu_domain_addr, vpu_shift_offset, vpu_dump_size);
	for (i = 0 ; i < (int)vpu_dump_size / 4 ; i = i + 4) {
		LOG_WRN("%08X %08X %08X %08X %08X\n", vpu_domain_addr,
			vpu_read_reg32(vpu_core->vpu_base,
			vpu_shift_offset + (4 * i)),
			vpu_read_reg32(vpu_core->vpu_base,
			vpu_shift_offset + (4 * i + 4)),
			vpu_read_reg32(vpu_core->vpu_base,
			vpu_shift_offset + (4 * i + 8)),
			vpu_read_reg32(vpu_core->vpu_base,
			vpu_shift_offset + (4 * i + 12)));
		vpu_domain_addr += (4 * 4);
		mdelay(1);
	}
	/* imem */
	vpu_domain_addr = 0x7FF40000;
	vpu_shift_offset = 0x40000;
	vpu_dump_size = 0x10000;
	LOG_WRN("==========imem => 0x%x/0x%x: 0x%x==============\n",
		vpu_domain_addr, vpu_shift_offset, vpu_dump_size);
	for (i = 0 ; i < (int)vpu_dump_size / 4 ; i = i + 4) {
		LOG_WRN("%08X %08X %08X %08X %08X\n", vpu_domain_addr,
			vpu_read_reg32(vpu_core->vpu_base,
			vpu_shift_offset + (4 * i)),
			vpu_read_reg32(vpu_core->vpu_base,
			vpu_shift_offset + (4 * i + 4)),
			vpu_read_reg32(vpu_core->vpu_base,
			vpu_shift_offset + (4 * i + 8)),
			vpu_read_reg32(vpu_core->vpu_base,
			vpu_shift_offset + (4 * i + 12)));
		vpu_domain_addr += (4 * 4);
		mdelay(1);
	}
}

void vpu_dump_code_segment(struct vpu_core *vpu_core)
{
	/*Remove dump code segment to db*/
#ifdef VPU_DUMP_MEM_LOG
	struct vpu_device *vpu_device = vpu_core->vpu_device;
	int i = 0;
	unsigned int size = 0x0;
	unsigned long addr = 0x0;
	unsigned int dump_addr = 0x0;
	unsigned int value_1, value_2, value_3, value_4;
	unsigned int bin_offset = 0x0;
	int core = vpu_core->core;

	vpu_device->vpu_dump_exception = VPU_EXCEPTION_MAGIC | core;

	LOG_ERR("==========%s, core_%d===========\n", __func__, core);
	LOG_ERR("==========main code segment_reset_vector===========\n");

	dump_addr = g_vpu_mva_reset_vector[core];
	bin_offset = VPU_DDR_SHIFT_RESET_VECTOR * core;
	size = DEBUG_MAIN_CODE_SEG_SIZE_1; /* define by mon/jackie*/
	LOG_WRN("==============0x%x/0x%x/0x%x/0x%x==============\n",
			bin_offset, dump_addr,
			size, VPU_SIZE_RESET_VECTOR);
	for (i = 0 ; i < (int)size / 4 ; i = i + 4) {
		value_1 = vpu_read_reg32(vpu_core->bin_base,
			bin_offset + (4 * i));
		value_2 = vpu_read_reg32(vpu_core->bin_base,
			bin_offset + (4 * i + 4));
		value_3 = vpu_read_reg32(vpu_core->bin_base,
			bin_offset + (4 * i + 8));
		value_4 = vpu_read_reg32(vpu_core->bin_base,
			bin_offset + (4 * i + 12));
		LOG_WRN("%08X %08X %08X %08X %08X\n", dump_addr,
			value_1, value_2, value_3, value_4);
		dump_addr += (4 * 4);
		mdelay(1);
	}


	LOG_ERR("=========== main code segment_main_program ===========\n");

	dump_addr = g_vpu_mva_main_program[core];
	bin_offset = VPU_DDR_SHIFT_RESET_VECTOR * core +
			VPU_OFFSET_MAIN_PROGRAM;
	size = DEBUG_MAIN_CODE_SEG_SIZE_2; /* define by mon/jackie*/
	LOG_WRN("==============0x%x/0x%x/0x%x/0x%x==============\n",
			bin_offset, dump_addr,
			size, VPU_SIZE_MAIN_PROGRAM);
	for (i = 0 ; i < (int)size / 4 ; i = i + 4) {
		value_1 = vpu_read_reg32(vpu_core->bin_base,
			bin_offset + (4 * i));
		value_2 = vpu_read_reg32(vpu_core->bin_base,
			bin_offset + (4 * i + 4));
		value_3 = vpu_read_reg32(vpu_core->bin_base,
			bin_offset + (4 * i + 8));
		value_4 = vpu_read_reg32(vpu_core->bin_base,
			bin_offset + (4 * i + 12));
		LOG_WRN("%08X %08X %08X %08X %08X\n", dump_addr,
			value_1, value_2, value_3, value_4);
		dump_addr += (4 * 4);
		mdelay(1);
	}

	LOG_ERR("============== kernel code segment ==============\n");

	dump_addr = g_vpu_mva_kernel_lib[core];
	addr = (unsigned long)(vpu_service_cores[core].exec_kernel_lib->va);
	size = DEBUG_CODE_SEG_SIZE; /* define by mon/jackie*/
	LOG_WRN("==============0x%lx/0x%x/0x%x/0x%x==============\n",
			addr, dump_addr,
			size, DEBUG_CODE_SEG_SIZE);
	for (i = 0 ; i < (int)size / 4 ; i = i + 4) {
		value_1 = (unsigned int)
			(*((unsigned long *)((uintptr_t)addr + (4 * i))));

		value_2 = (unsigned int)
			(*((unsigned long *)((uintptr_t)addr + (4 * i + 4))));

		value_3 = (unsigned int)
			(*((unsigned long *)((uintptr_t)addr + (4 * i + 8))));

		value_4 = (unsigned int)
			(*((unsigned long *)((uintptr_t)addr + (4 * i + 12))));

		LOG_WRN("%08X %08X %08X %08X %08X\n", dump_addr,
			value_1, value_2, value_3, value_4);

		dump_addr += (4 * 4);
		mdelay(1);
	}
#else
	struct vpu_device *vpu_device = vpu_core->vpu_device;

	vpu_device->vpu_dump_exception = VPU_EXCEPTION_MAGIC | vpu_core->core;
#endif
}

void vpu_dump_algo_segment(struct vpu_core *vpu_core, int algo_id, int size)
{
/* we do not mapping out va(bin_ptr is mva) for algo bin file currently */
#if 1
	struct vpu_algo *algo = NULL;
	int ret = 0;
	int core = vpu_core->core;

	LOG_WRN("==========%s, core_%d, id_%d===========\n",
		__func__, core, algo_id);

	ret = vpu_find_algo_by_id(vpu_core, algo_id, &algo, NULL);
	if (ret) {
		LOG_ERR("%s can not find the algo, core=%d, id=%d\n",
				__func__, core, algo_id);
		return;
	}
	LOG_WRN("== algo name : %s ==\n", algo->name);

#endif
#if 0

	unsigned int addr = 0x0;
	unsigned int length = 0x0;
	int i = 0;

	addr = (unsigned int)algo->bin_ptr;
	length = (unsigned int)algo->bin_length;

	LOG_WRN("==============0x%x/0x%x/0x%x==============\n",
			addr, length, size);

	for (i = 0 ; i < (int)length / 4 ; i = i + 4) {
		LOG_WRN("%X %X %X %X %X\n", addr,
		(unsigned int)
			(*((unsigned int *)((uintptr_t)addr + (4 * i)))),
		(unsigned int)
			(*((unsigned int *)((uintptr_t)addr + (4 * i + 4)))),
		(unsigned int)
			(*((unsigned int *)((uintptr_t)addr + (4 * i + 8)))),
		(unsigned int)
			(*((unsigned int *)((uintptr_t)addr + (4 * i + 12)))));
		addr += (4 * 4);
	}
#endif
}

int vpu_dump_mesg(struct seq_file *s, struct vpu_device *gvpu_device)
{
	struct vpu_device *vpu_device;
	char *ptr = NULL;
	char *log_head = NULL;
	char *log_buf;
	char *log_a_pos = NULL;
	int core_index = 0;
	bool jump_out = false;

	if (gvpu_device) {
		vpu_device = gvpu_device;
	} else {
		if (s == NULL || s->private == NULL) {
			LOG_ERR("[vpu seq_file] seq_file error");
			return -EINVAL;
		}

		vpu_device = (struct vpu_device *)s->private;
	}

	for (core_index = 0 ; core_index < vpu_device->core_num; core_index++) {
		struct vpu_core *vpu_core = vpu_device->vpu_core[core_index];

		log_buf = (char *)((uintptr_t)vpu_core->work_buf->va +
						VPU_OFFSET_LOG);
	if (vpu_device->vpu_log_level > 8) {
		int i = 0;
		int line_pos = 0;
		char line_buffer[16 + 1] = {0};

		ptr = log_buf;
		vpu_print_seq(s, "VPU_%d Log Buffer:\n", core_index);
		for (i = 0; i < VPU_SIZE_LOG_BUF; i++, ptr++) {
			line_pos = i % 16;
			if (line_pos == 0)
				vpu_print_seq(s, "\n%07X0h: ", i / 16);

			line_buffer[line_pos] = isascii(*ptr) &&
						(isprint(*ptr) ? *ptr : '.');

			vpu_print_seq(s, "%02X ", *ptr);
			if (line_pos == 15)
				vpu_print_seq(s, " %s", line_buffer);

		}
		vpu_print_seq(s, "\n\n");
	}

	ptr = log_buf;
	log_head = log_buf;

	/* set the last byte to '\0' */
	#if 0
	*(ptr + VPU_SIZE_LOG_BUF - 1) = '\0';

	/* skip the header part */
	ptr += VPU_SIZE_LOG_HEADER;
	log_head = strchr(ptr, '\0') + 1;

	vpu_print_seq(s, "=== VPU_%d Log Buffer ===\n", core_index);
	vpu_print_seq(s, "vpu: print dsp log\n%s%s", log_head, ptr);
	#else
	vpu_print_seq(s, "=== VPU_%d Log Buffer ===\n", core_index);
	vpu_print_seq(s, "vpu: print dsp log (0x%x):\n",
					(unsigned int)(uintptr_t)log_buf);

	/* in case total log < VPU_SIZE_LOG_SHIFT and there's '\0' */
	*(log_head + VPU_SIZE_LOG_BUF - 1) = '\0';
	vpu_print_seq(s, "%s", ptr+VPU_SIZE_LOG_HEADER);

	ptr += VPU_SIZE_LOG_HEADER;
	log_head = ptr;

	jump_out = false;
	*(log_head + VPU_SIZE_LOG_DATA - 1) = '\n';
	do {
		if ((ptr + VPU_SIZE_LOG_SHIFT) >=
				(log_head + VPU_SIZE_LOG_DATA)) {

			/* last part of log buffer */
			*(log_head + VPU_SIZE_LOG_DATA - 1) = '\0';
			jump_out = true;
		} else {
			log_a_pos = strchr(ptr + VPU_SIZE_LOG_SHIFT, '\n');
			if (log_a_pos == NULL)
				break;
			*log_a_pos = '\0';
		}
		vpu_print_seq(s, "%s\n", ptr);
		ptr = log_a_pos + 1;

		/* incase log_a_pos is at end of string */
		if (ptr >= log_head + VPU_SIZE_LOG_DATA)
			break;

		mdelay(1);
	} while (!jump_out);

	#endif
	}
	return 0;
}

int vpu_dump_power(struct seq_file *s, struct vpu_device *gvpu_device)
{
	struct vpu_device *vpu_device;
	struct vpu_core *vpu_core;
	int i;

	if (gvpu_device) {
		vpu_device = gvpu_device;
	} else {
		if (s == NULL || s->private == NULL) {
			LOG_ERR("[vpu seq_file] seq_file error");
			return -EINVAL;
		}

		vpu_device = (struct vpu_device *)s->private;
	}

	vpu_print_seq(s, "%s[%d], %s[%d]\n",
			"apu", vpu_device->opps.apu.index,
			"apu_if", vpu_device->opps.apu_if.index);

	for (i = 0; i < vpu_device->core_num; i++) {
		vpu_core = vpu_device->vpu_core[i];
		vpu_print_seq(s, "min/max boost[%d][%d/%d]\n",
				i, vpu_core->min_boost, vpu_core->max_boost);
		vpu_print_seq(s, "min/max opp[%d][%d/%d]\n",
				i, vpu_core->min_opp, vpu_core->max_opp);
	}

	vpu_print_seq(s, "is_power_debug_lock(rw): %d\n",
			vpu_device->is_power_debug_lock);

	return 0;
}

int vpu_dump_vpu(struct seq_file *s, struct vpu_device *gvpu_device)
{
	struct vpu_device *vpu_device;
	int core;
	const char *line_bar =
		"  +------------+----------------------------------+\n";

	if (gvpu_device) {
		vpu_device = gvpu_device;
	} else {
		if (s == NULL || s->private == NULL) {
			LOG_ERR("[vpu seq_file] seq_file error");
			return -EINVAL;
		}

		vpu_device = (struct vpu_device *)s->private;
	}

	vpu_print_seq(s, "%s", line_bar);
	vpu_print_seq(s, "  |%-12s|%-34s|\n",
			"Queue#", "Waiting");
	vpu_print_seq(s, "%s", line_bar);

	mutex_lock(&vpu_device->commonpool_mutex);
	vpu_print_seq(s, "  |%-12s|%-34d|\n",
			"Common",
			vpu_device->commonpool_list_size);
	mutex_unlock(&vpu_device->commonpool_mutex);

	for (core = 0 ; core < vpu_device->core_num; core++) {
		struct vpu_core *vpu_core = vpu_device->vpu_core[core];

		mutex_lock(&vpu_core->servicepool_mutex);
		vpu_print_seq(s, "  |Core %-7d|%-34d|\n",
				      core,
				      vpu_core->servicepool_list_size);
		mutex_unlock(&vpu_core->servicepool_mutex);
	}
	vpu_print_seq(s, "%s", line_bar);

	return 0;
}

int vpu_dump_util(struct seq_file *s, struct vpu_device *gvpu_device)
{
	struct vpu_device *vpu_device;
	int core;
	const char *line_bar =
		"  +------------+----------------------------------+\n";
	unsigned long total_time;
	unsigned long busy_time;
	int ret;

	if (gvpu_device) {
		vpu_device = gvpu_device;
	} else {
		if (s == NULL || s->private == NULL) {
			LOG_ERR("[vpu seq_file] seq_file error");
			return -EINVAL;
		}

		vpu_device = (struct vpu_device *)s->private;
	}

	for (core = 0 ; core < vpu_device->core_num; core++) {
		struct vpu_core *vpu_core = vpu_device->vpu_core[core];

		ret = vpu_dvfs_get_usage(vpu_core, &total_time, &busy_time);
		if (ret) {
			vpu_print_seq(s, "%s", line_bar);
			vpu_print_seq(s, "vpu loading : error\n");
			return ret;
		}
		vpu_print_seq(s, "%s", line_bar);
		vpu_print_seq(s, "vpu loading\n");
		vpu_print_seq(s, "core%d: %d\n", core,
			(unsigned int)(busy_time  * 100U / total_time));
	}

	return 0;
}

int vpu_set_power_parameter(struct vpu_device *vpu_device, uint8_t param,
				int argc, int *args)
{
	int ret = 0;

	switch (param) {
	case VPU_POWER_PARAM_FIX_OPP:
		ret = (argc == 1) ? 0 : -EINVAL;
		if (ret) {
			LOG_ERR("invalid argument, expected:1, received:%d\n",
					argc);
			goto out;
		}

		switch (args[0]) {
		case 0:
			vpu_device->is_power_debug_lock = false;
			break;
		case 1:
			vpu_device->is_power_debug_lock = true;
			break;
		default:
			if (ret) {
				LOG_ERR("invalid argument, received:%d\n",
						(int)(args[0]));
				goto out;
			}
			ret = -EINVAL;
			goto out;
		}
		break;
	case VPU_POWER_PARAM_DVFS_DEBUG:
		ret = (argc == 1) ? 0 : -EINVAL;
		if (ret) {
			LOG_ERR("invalid argument, expected:1, received:%d\n",
					argc);
			goto out;
		}

		ret = args[0] >= vpu_device->opps.count;
		if (ret) {
			LOG_ERR("opp step(%d) is out-of-bound, count:%d\n",
					(int)(args[0]), vpu_device->opps.count);
			goto out;
		}

		vpu_set_opp_all_index(vpu_device, args[0]);

		vpu_device->is_power_debug_lock = true;

		break;
	case VPU_POWER_PARAM_JTAG:
		ret = (argc == 1) ? 0 : -EINVAL;
		if (ret) {
			LOG_ERR("invalid argument, expected:1, received:%d\n",
					argc);
			goto out;
		}

		vpu_device->is_jtag_enabled = args[0];
		ret = vpu_hw_enable_jtag(vpu_device->vpu_core[0],
						vpu_device->is_jtag_enabled);

		break;
	case VPU_POWER_PARAM_LOCK:
		ret = (argc == 1) ? 0 : -EINVAL;
		if (ret) {
			LOG_ERR("invalid argument, expected:1, received:%d\n",
					argc);
			goto out;
		}

		vpu_device->is_power_debug_lock = args[0];

		break;
	case VPU_POWER_HAL_CTL:
	{
		struct vpu_lock_power vpu_lock_power;

		ret = (argc == 3) ? 0 : -EINVAL;
		if (ret) {
			LOG_ERR("invalid argument, expected:3, received:%d\n",
					argc);
			goto out;
		}

		if (args[0] > vpu_device->core_num || args[0] < 1) {
			LOG_ERR("core(%d) is out-of-bound\n",
					(int)(args[0]));
			goto out;
		}

		if (args[1] > 100 || args[1] < 0) {
			LOG_ERR("min boost(%d) is out-of-bound\n",
					(int)(args[1]));
			goto out;
		}
		if (args[2] > 100 || args[2] < 0) {
			LOG_ERR("max boost(%d) is out-of-bound\n",
					(int)(args[2]));
			goto out;
		}
		vpu_lock_power.core = args[0];
		vpu_lock_power.lock = true;
		vpu_lock_power.priority = POWER_HAL;
		vpu_lock_power.max_boost_value = args[2];
		vpu_lock_power.min_boost_value = args[1];
		LOG_INF("[vpu]POWER_HAL_LOCK+core:%d, maxb:%d, minb:%d\n",
			vpu_lock_power.core, vpu_lock_power.max_boost_value,
				vpu_lock_power.min_boost_value);

		ret = vpu_lock_set_power(vpu_device,
						&vpu_lock_power);
		if (ret) {
			LOG_ERR("[POWER_HAL_LOCK]failed, ret=%d\n", ret);
			goto out;
		}
		break;
	}
	case VPU_EARA_CTL:
	{
		struct vpu_lock_power vpu_lock_power;

		ret = (argc == 3) ? 0 : -EINVAL;
		if (ret) {
			LOG_ERR("invalid argument, expected:3, received:%d\n",
					argc);
			goto out;
		}
		if (args[0] > vpu_device->core_num || args[0] < 1) {
			LOG_ERR("core(%d) is out-of-bound\n",
					(int)(args[0]));
			goto out;
		}

		if (args[1] > 100 || args[1] < 0) {
			LOG_ERR("min boost(%d) is out-of-bound\n",
					(int)(args[1]));
			goto out;
		}
		if (args[2] > 100 || args[2] < 0) {
			LOG_ERR("max boost(%d) is out-of-bound\n",
					(int)(args[2]));
			goto out;
		}
		vpu_lock_power.core = args[0];
		vpu_lock_power.lock = true;
		vpu_lock_power.priority = EARA_QOS;
		vpu_lock_power.max_boost_value = args[2];
		vpu_lock_power.min_boost_value = args[1];
		LOG_INF("[vpu]EARA_LOCK+core:%d, maxb:%d, minb:%d\n",
			vpu_lock_power.core, vpu_lock_power.max_boost_value,
				vpu_lock_power.min_boost_value);
		ret = vpu_lock_set_power(vpu_device,
						&vpu_lock_power);
		if (ret) {
			LOG_ERR("[POWER_HAL_LOCK]failed, ret=%d\n", ret);
			goto out;
		}
		break;
	}
	case VPU_CT_INFO:
		ret = (argc == 1) ? 0 : -EINVAL;
		if (ret) {
			LOG_ERR("invalid argument, expected:1, received:%d\n",
					argc);
			goto out;
		}
		break;
	default:
		LOG_ERR("unsupport the power parameter:%d\n", param);
		break;
	}

out:
	return ret;
}

bool vpu_update_lock_power_parameter(struct vpu_core *vpu_core,
					struct vpu_lock_power *vpu_lock_power)
{
	struct vpu_device *vpu_device = vpu_core->vpu_device;
	bool ret = true;
	int i, core = -1;
	unsigned int priority = vpu_lock_power->priority;

	for (i = 0 ; i < vpu_device->core_num ; i++) {
		if (vpu_lock_power->core == (0x1 << i)) {
			core = i;
			break;
		}
	}

	if (core >= vpu_device->core_num || core < 0) {
		LOG_ERR("wrong core index (0x%x/%d/%d)",
			vpu_lock_power->core, core, vpu_device->core_num);
		ret = false;
		return ret;
	}
	vpu_core->lock_power[priority].core = core;
	vpu_core->lock_power[priority].max_boost_value =
		vpu_lock_power->max_boost_value;
	vpu_core->lock_power[priority].min_boost_value =
		vpu_lock_power->min_boost_value;
	vpu_core->lock_power[priority].lock = true;
	vpu_core->lock_power[priority].priority =
		vpu_lock_power->priority;
	LOG_INF("power_parameter core %d, maxb:%d, minb:%d priority %d\n",
		vpu_core->lock_power[priority].core,
		vpu_core->lock_power[priority].max_boost_value,
		vpu_core->lock_power[priority].min_boost_value,
		vpu_core->lock_power[priority].priority);
	return ret;
}

bool vpu_update_unlock_power_parameter(struct vpu_core *vpu_core,
					struct vpu_lock_power *vpu_lock_power)
{
	struct vpu_device *vpu_device = vpu_core->vpu_device;
	bool ret = true;
	int i, core = -1;
	unsigned int priority = vpu_lock_power->priority;

	for (i = 0 ; i < vpu_device->core_num ; i++) {
		if (vpu_lock_power->core == (0x1 << i)) {
			core = i;
			break;
		}
	}

	if (core >= vpu_device->core_num || core < 0) {
		LOG_ERR("wrong core index (0x%x/%d/%d)",
			vpu_lock_power->core, core, vpu_device->core_num);
		ret = false;
		return ret;
	}
	vpu_core->lock_power[priority].core =
		vpu_lock_power->core;
	vpu_core->lock_power[priority].max_boost_value =
		vpu_lock_power->max_boost_value;
	vpu_core->lock_power[priority].min_boost_value =
		vpu_lock_power->min_boost_value;
	vpu_core->lock_power[priority].lock = false;
	vpu_core->lock_power[priority].priority =
		vpu_lock_power->priority;
	LOG_INF("%s\n", __func__);
	return ret;
}

uint8_t min_of(uint8_t value1, uint8_t value2)
{
	if (value1 <= value2)
		return value1;
	else
		return value2;
}

uint8_t max_of(uint8_t value1, uint8_t value2)
{
	if (value1 <= value2)
		return value2;
	else
		return value1;
}

bool vpu_update_max_opp(struct vpu_core *vpu_core,
			struct vpu_lock_power *vpu_lock_power)
{
	struct vpu_device *vpu_device = vpu_core->vpu_device;
	bool ret = true;
	int i, core = -1;
	uint8_t first_priority = NORMAL;
	uint8_t first_priority_max_boost_value = 100;
	uint8_t first_priority_min_boost_value = 0;
	uint8_t temp_max_boost_value = 100;
	uint8_t temp_min_boost_value = 0;
	bool lock = false;
	uint8_t priority = NORMAL;
	uint8_t maxboost = 100;
	uint8_t minboost = 0;

	for (i = 0 ; i < vpu_device->core_num ; i++) {
		if (vpu_lock_power->core == (0x1 << i)) {
			core = i;
			break;
		}
	}

	if (core >= vpu_device->core_num || core < 0) {
		LOG_ERR("wrong core index (0x%x/%d/%d)",
			vpu_lock_power->core, core, vpu_device->core_num);
		ret = false;
		return ret;
	}

	for (i = 0 ; i < VPU_OPP_PRIORIYY_NUM ; i++) {
		if (vpu_core->lock_power[i].lock == true
			&& vpu_core->lock_power[i].priority < NORMAL) {
			first_priority = i;
			first_priority_max_boost_value =
				vpu_core->lock_power[i].max_boost_value;
			first_priority_min_boost_value =
				vpu_core->lock_power[i].min_boost_value;
			break;
		}
	}
	temp_max_boost_value = first_priority_max_boost_value;
	temp_min_boost_value = first_priority_min_boost_value;
/*find final_max_boost_value*/
	for (i = first_priority ; i < VPU_OPP_PRIORIYY_NUM ; i++) {
		lock = vpu_core->lock_power[i].lock;
		priority = vpu_core->lock_power[i].priority;
		maxboost = vpu_core->lock_power[i].max_boost_value;
		minboost = vpu_core->lock_power[i].min_boost_value;
		if (lock == true
			&& priority < NORMAL &&
				(((maxboost <= temp_max_boost_value)
					&& (maxboost >= temp_min_boost_value))
				|| ((minboost <= temp_max_boost_value)
					&& (minboost >= temp_min_boost_value))
				|| ((maxboost >= temp_max_boost_value)
				&& (minboost <= temp_min_boost_value)))) {

			temp_max_boost_value =
	min_of(temp_max_boost_value, vpu_core->lock_power[i].max_boost_value);
			temp_min_boost_value =
	max_of(temp_min_boost_value, vpu_core->lock_power[i].min_boost_value);
		}
	}

	vpu_core->max_boost = temp_max_boost_value;
	vpu_core->min_boost = temp_min_boost_value;
	vpu_core->max_opp = vpu_boost_value_to_opp(vpu_device,
							temp_max_boost_value);
	vpu_core->min_opp = vpu_boost_value_to_opp(vpu_device,
							temp_min_boost_value);

	LOG_DVFS("final_min_boost_value:%d final_max_boost_value:%d\n",
		temp_min_boost_value, temp_max_boost_value);
	return ret;
}

int vpu_lock_set_power(struct vpu_device *vpu_device,
			struct vpu_lock_power *vpu_lock_power)
{
	struct vpu_core *vpu_core = NULL;
	int ret = -1;
	int i, core = -1;

	mutex_lock(&vpu_device->power_lock_mutex);
	for (i = 0 ; i < vpu_device->core_num ; i++) {
		if (vpu_lock_power->core == (0x1 << i)) {
			core = i;
			break;
		}
	}

	if (core >= vpu_device->core_num || core < 0) {
		LOG_ERR("wrong core index (0x%x/%d/%d)",
			vpu_lock_power->core, core, vpu_device->core_num);
		ret = -1;
		mutex_unlock(&vpu_device->power_lock_mutex);
		return ret;
	}

	vpu_core = vpu_device->vpu_core[core];
	if (!vpu_update_lock_power_parameter(vpu_core, vpu_lock_power)) {
		mutex_unlock(&vpu_device->power_lock_mutex);
		return -1;
	}

	if (!vpu_update_max_opp(vpu_core, vpu_lock_power)) {
		mutex_unlock(&vpu_device->power_lock_mutex);
		return -1;
	}
#if 0
	if (vpu_update_max_opp(vpu_lock_power)) {
		power.opp_step = min_opp[core];
		power.freq_step = min_opp[core];
		power.core = vpu_lock_power->core;
		ret = vpu_set_power(user, &power);
	}
#endif
	mutex_unlock(&vpu_device->power_lock_mutex);
	return 0;
}

int vpu_unlock_set_power(struct vpu_device *vpu_device,
				struct vpu_lock_power *vpu_lock_power)
{
	struct vpu_core *vpu_core = NULL;
	int ret = -1;
	int i, core = -1;

	mutex_lock(&vpu_device->power_lock_mutex);
	for (i = 0 ; i < vpu_device->core_num ; i++) {
		if (vpu_lock_power->core == (0x1 << i)) {
			core = i;
			break;
		}
	}

	if (core >= vpu_device->core_num || core < 0) {
		LOG_ERR("wrong core index (0x%x/%d/%d)",
			vpu_lock_power->core, core, vpu_device->core_num);
		ret = false;
		return ret;
	}

	vpu_core = vpu_device->vpu_core[core];
	if (!vpu_update_unlock_power_parameter(vpu_core, vpu_lock_power)) {
		mutex_unlock(&vpu_device->power_lock_mutex);
		return -1;
	}

	if (!vpu_update_max_opp(vpu_core, vpu_lock_power)) {
		mutex_unlock(&vpu_device->power_lock_mutex);
		return -1;
	}
	mutex_unlock(&vpu_device->power_lock_mutex);
	return ret;
}

int vpu_sdsp_get_power(struct vpu_device *vpu_device)
{
	int ret = 0;
	struct vpu_core *vpu_core = NULL;
	uint8_t vcore_opp_index = 0; /*0~15, 0 is max*/
	uint8_t dsp_freq_index = 0;  /*0~15, 0 is max*/
	int core = 0;

	if (vpu_device->sdsp_power_counter == 0) {
		for (core = 0 ; core < vpu_device->core_num ; core++) {
			vpu_core = vpu_device->vpu_core[core];
			vpu_opp_check(vpu_core, vcore_opp_index,
					dsp_freq_index);

			ret = ret | vpu_get_power(vpu_core, true);
			mutex_lock(&(vpu_core->state_mutex));
			vpu_core->state = VCT_IDLE;
			mutex_unlock(&(vpu_core->state_mutex));
		}
	}
	vpu_device->sdsp_power_counter++;
	mod_delayed_work(vpu_device->wq, &vpu_device->sdsp_work,
				msecs_to_jiffies(SDSP_KEEP_TIME_MS));

	LOG_INF("[vpu] %s -\n", __func__);
	return ret;
}

int vpu_sdsp_put_power(struct vpu_device *vpu_device)
{
	struct vpu_core *vpu_core;
	int ret = 0;
	int core = 0;

	vpu_device->sdsp_power_counter--;

	if (vpu_device->sdsp_power_counter == 0) {
		for (core = 0 ; core < vpu_device->core_num ; core++) {
			vpu_core = vpu_device->vpu_core[core];
			while (vpu_core->power_counter != 0)
				vpu_put_power(vpu_core, VPT_IMT_OFF);

			while (vpu_core->is_power_on == true)
				msleep(20);

			LOG_INF("[vpu] power_counter[%d] = %d/%d -\n",
				core, vpu_core->power_counter,
				vpu_core->is_power_on);
		}
	}
	mod_delayed_work(vpu_device->wq, &vpu_device->sdsp_work,
				msecs_to_jiffies(0));

	LOG_INF("[vpu] %s, sdsp_power_counter = %d -\n",
		__func__, vpu_device->sdsp_power_counter);
	return ret;
}

static void vpu_sdsp_routine(struct work_struct *work)
{
	struct vpu_device *vpu_device = container_of(work, struct vpu_device,
							sdsp_work.work);

	if (vpu_device->sdsp_power_counter != 0) {
		LOG_INF("%s long time not unlock!!! -\n", __func__);
		LOG_ERR("%s long time not unlock error!!! -\n", __func__);
	} else {
		LOG_INF("%s sdsp_power_counter is correct!!! -\n", __func__);
	}
}

