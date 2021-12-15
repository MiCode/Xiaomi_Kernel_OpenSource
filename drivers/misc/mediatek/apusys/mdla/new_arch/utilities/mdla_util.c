// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/types.h>
#include <linux/io.h>
#include <linux/platform_device.h>

#include <apusys_device.h>

#include <common/mdla_driver.h>
#include <common/mdla_device.h>

#include <utilities/mdla_debug.h>
#include <utilities/mdla_util.h>
#include <utilities/mdla_profile.h>

#include <platform/mdla_plat_api.h>


struct mdla_pmu_result {
	u16 cmd_len;
	u16 cmd_id;
	u32 pmu_val[MDLA_PMU_COUNTERS + 1]; /* global counter + PMU counter*/
};

/* platform */
u32 mdla_util_get_core_num(void)
{
	return mdla_plat_get_core_num();
}

u32 mdla_util_get_ip_version(void)
{
	return get_major_num(mdla_plat_get_version());
}

const struct of_device_id *mdla_util_get_device_id(void)
{
	return mdla_plat_get_device();
}

int mdla_util_plat_init(struct platform_device *pdev)
{
	if (mdla_plat_init(pdev) != 0)
		return -1;

	mdla_fpga_reset();
	mdla_dbg_fs_setup(&pdev->dev);

	if (mdla_plat_micro_p_support())
		return 1;

	//mdla_drv_create_device_node(&pdev->dev);
	mdla_prof_init(mdla_plat_get_prof_ver());

	return 0;
}

void mdla_util_plat_deinit(struct platform_device *pdev)
{
	if (!mdla_plat_micro_p_support()) {
		mdla_prof_deinit();
		mdla_drv_destroy_device_node();
	}
	mdla_plat_deinit(pdev);
}

/* pmu */
static void mdla_util_dummy_cnt_save(u32 a0, struct mdla_pmu_info *a1) {}
static void mdla_util_dummy_cnt_read(u32 a0, u32 *a1) {}
static void mdla_util_dummy_cnt_clr(struct mdla_pmu_info *pmu) {}
static u32 mdla_util_dummy_get_num_evt(u32 a0, u16 a1)
{
	return 0;
}
static void mdla_util_dummy_set_num_evt(u32 a0, u16 a1, u32 a2) {}
static void mdla_util_dummy_set_mode(struct mdla_pmu_info *a0, u32 a1) {}
static int mdla_util_dummy_get_mode(struct mdla_pmu_info *a0)
{
	return 0;
}
static u64 mdla_util_dummy_get_addr(struct mdla_pmu_info *a0)
{
	return 0;
}
static u32 mdla_util_dummy_get_val(struct mdla_pmu_info *a0)
{
	return 0;
}
static u16 mdla_util_dummy_get_cmdcnt(struct mdla_pmu_info *a0)
{
	return 0;
}
static u32 mdla_util_dummy_get_data(struct mdla_pmu_info *a0, u32 a1)
{
	return 0;
}
static void mdla_util_dummy_ops(u32 a0) {}
static void mdla_util_dummy_exec(u32 a0, u16 a1) {}
static struct mdla_pmu_info *mdla_util_dummy_get_info(u32 a0, u16 a1)
{
	return NULL;
}
static void mdla_util_dummy_set_evt(struct mdla_pmu_info *a0,
		u32 a1, u32 a2)
{
}
static int mdla_util_dummy_prepare(struct mdla_dev *mdla_info,
			struct apusys_cmd_hnd *apusys_hd, u16 priority)
{
	return 0;
}

static struct mdla_util_pmu_ops pmu_ops = {
	.reg_counter_save = mdla_util_dummy_cnt_save,
	.reg_counter_read = mdla_util_dummy_cnt_read,
	.clr_counter_variable = mdla_util_dummy_cnt_clr,
	.clr_cycle_variable = mdla_util_dummy_cnt_clr,
	.get_num_evt = mdla_util_dummy_get_num_evt,
	.set_num_evt = mdla_util_dummy_set_num_evt,
	.set_percmd_mode = mdla_util_dummy_set_mode,
	.get_curr_mode = mdla_util_dummy_get_mode,
	.get_perf_end = mdla_util_dummy_get_val,
	.get_perf_cycle = mdla_util_dummy_get_val,
	.get_perf_cmdid = mdla_util_dummy_get_cmdcnt,
	.get_perf_cmdcnt = mdla_util_dummy_get_cmdcnt,
	.get_counter = mdla_util_dummy_get_data,
	.reset_counter = mdla_util_dummy_ops,
	.disable_counter = mdla_util_dummy_ops,
	.enable_counter = mdla_util_dummy_ops,
	.reset = mdla_util_dummy_ops,
	.write_evt_exec = mdla_util_dummy_exec,
	.reset_write_evt_exec = mdla_util_dummy_exec,
	.get_hnd_evt = mdla_util_dummy_get_data,
	.get_hnd_evt_num = mdla_util_dummy_get_val,
	.get_hnd_mode = mdla_util_dummy_get_val,
	.get_hnd_buf_addr = mdla_util_dummy_get_addr,
	.set_evt_handle = mdla_util_dummy_set_evt,
	.get_info = mdla_util_dummy_get_info,
	.apu_cmd_prepare = mdla_util_dummy_prepare,
};

struct mdla_util_pmu_ops *mdla_util_pmu_ops_get(void)
{
	return &pmu_ops;
}

/* apusys pmu */

static bool mdla_util_pmu_addr_is_invalid(struct apusys_cmd_hnd *apusys_hd)
{
	if ((apusys_hd == NULL) ||
		(apusys_hd->pmu_kva == apusys_hd->cmd_entry) ||
		(apusys_hd->pmu_kva == 0))
		return true;

	mdla_pmu_debug("command entry:%08llx, pmu kva: %08llx\n",
		apusys_hd->cmd_entry,
		apusys_hd->pmu_kva);

	return false;
}

static bool apusys_pmu_support;

void mdla_util_pmu_cmd_timer(bool enable)
{
	/* Not support polling pmu data with per cmd mode yet */
}

/* initial local variable and extract pmu setting from input */
int mdla_util_apu_pmu_handle(struct mdla_dev *mdla_info,
	struct apusys_cmd_hnd *apusys_hd, u16 priority)
{
	int i, pmu_mode;
	u32 evt, evt_num;
	struct mdla_pmu_info *pmu;
	u32 core_id;

	core_id = mdla_info->mdla_id;
	pmu = pmu_ops.get_info(core_id, priority);

	if (!pmu) {
		mdla_err("%s: No pmu info device\n", __func__);
		return -1;
	}

	pmu_ops.clr_counter_variable(pmu);
	pmu_ops.clr_cycle_variable(pmu);


	if (mdla_prof_use_dbgfs_pmu_event(mdla_info->mdla_id)) {
		pmu_ops.set_num_evt(core_id,
				priority, MDLA_PMU_COUNTERS);

		for (i = 0; i < MDLA_PMU_COUNTERS; i++) {
			evt = mdla_dbg_read_u32(FS_C1 + i);
			pmu_ops.set_evt_handle(pmu, i,
					(evt & 0x1f0000) | (evt & 0xf));
		}
		return 0;
	}

	if (!apusys_pmu_support)
		return -1;

	if (mdla_util_pmu_addr_is_invalid(apusys_hd)) {
		for (i = 0; i < MDLA_PMU_COUNTERS; i++)
			pmu_ops.set_evt_handle(pmu, i, COUNTER_CLEAR);
		return -1;
	}

	if (pmu_ops.apu_cmd_prepare(mdla_info, apusys_hd, priority))
		return -1;

	pmu_mode = pmu_ops.get_curr_mode(pmu);
	evt_num = pmu_ops.get_hnd_evt_num(pmu);

	/* save to local variable */
	pmu_ops.set_percmd_mode(pmu, pmu_mode);
	pmu_ops.set_num_evt(core_id, priority,
					evt_num);

	if (pmu_ops.apu_pmu_valid(mdla_info, apusys_hd, priority) == false)
		return -1;

	mdla_pmu_debug("PMU number_of_event:%d, mode: %d\n",
			evt_num,
			pmu_mode);

	for (i = 0; i < evt_num; i++) {
		evt = pmu_ops.get_hnd_evt(pmu, i);
		pmu_ops.set_evt_handle(pmu, i,
				((evt & 0x1f00) << 8) | (evt & 0xf));
	}

	return 0;
}

void mdla_util_apu_pmu_update(struct mdla_dev *mdla_info,
	struct apusys_cmd_hnd *apusys_hd, u16 priority)
{
	int i;
	struct mdla_pmu_result result;
	void *base = NULL, *desc = NULL, *src = NULL;
	int sz = 0;
	//int event_num = 0;
	int offset = 0;
	//u16 final_len = 0;
	u16 loop_count;
	u32 core_id;
	struct mdla_pmu_info *pmu;

	if (!apusys_pmu_support)
		return;

	/* FIXME: NOT always upload pmu info to NN? */
	if (mdla_prof_use_dbgfs_pmu_event(mdla_info->mdla_id))
		return;

	if (mdla_util_pmu_addr_is_invalid(apusys_hd))
		return;
	if (pmu_ops.apu_pmu_valid(mdla_info, apusys_hd, priority) == false)
		return;

	core_id = mdla_info->mdla_id;
	pmu = pmu_ops.get_info(core_id, priority);

	if (!pmu) {
		mdla_err("%s: no pmu info device(core:%d, prio:%d)\n",
				__func__, core_id, priority);
		return;
	}

	//loop_count = pmu->pmu_hnd->number_of_event;
	loop_count = pmu_ops.get_hnd_evt_num(pmu);
	if (loop_count == 0)
		return;
	//event_num = loop_count + 1;

	//result.cmd_len = pmu->data.l_cmd_cnt;
	result.cmd_len = pmu_ops.get_perf_cmdcnt(pmu);
	result.cmd_id = pmu_ops.get_perf_cmdid(pmu);

	//sz = sizeof(u16) * 2 + sizeof(u32) * event_num;
	sz = sizeof(struct mdla_pmu_result);
	base = (void *)pmu_ops.get_hnd_buf_addr(pmu);

	mdla_pmu_debug("mode: %d, cmd_len: %d, cmd_id: %d, sz: %d\n",
		       pmu_ops.get_hnd_mode(pmu),
		       result.cmd_len, result.cmd_id, sz);

	if (pmu_ops.get_curr_mode(pmu) == PER_CMD) {
		mdla_util_pmu_cmd_timer(false);
		result.pmu_val[0] = pmu_ops.get_perf_end(pmu);
	} else {
		result.pmu_val[0] = pmu_ops.get_perf_cycle(pmu);
	}

	mdla_pmu_debug("global counter:%08x\n", result.pmu_val[0]);

	for (i = 1; i < loop_count; i++) {
		result.pmu_val[i] =
			pmu_ops.get_counter(pmu, i);
		mdla_pmu_debug("event %d cnt :%08x\n",
			       i, result.pmu_val[i]);
	}

	/* update pmu result buffer */
	if (result.cmd_len == 1) {
		desc = base;
		src = &result;
		memcpy(desc, src, sz);
	} else if (result.cmd_len > 1) {
		offset = sz + (result.cmd_len - 2) * (sz - sizeof(u16));
		desc = (void *)(base + offset);
		src = (void *)&(result.cmd_id);
		memcpy(desc, src, sz - sizeof(u16));
	}

	if (result.cmd_id == mdla_info->max_cmd_id) {
		((struct mdla_pmu_result *)base)->cmd_len = result.cmd_len;
		//final_len = result.cmd_len;
		//desc = base;
		//memcpy(desc, &final_len, sizeof(u16));
		if (unlikely(mdla_dbg_read_u32(FS_KLOG) & MDLA_DBG_PMU)) {
			struct mdla_pmu_result check;

			memcpy(&check, base, sz);

			mdla_pmu_debug("[-] check cmd_len: %d\n",
				check.cmd_len);
			mdla_pmu_debug("[-] check cmd_id: %d\n",
				check.cmd_id);
			mdla_pmu_debug("[-] check cmd_val[1]:  %08x\n",
			       check.pmu_val[1]);

			if (result.cmd_len > 1) {
				offset =
					sz +
					(result.cmd_len - 2) *
					(sz - sizeof(u16));
				desc = (void *)(base + offset);
				memcpy(
					(void *)&(check.cmd_id),
					desc,
					sz - sizeof(u16));

				mdla_pmu_debug("[-] offset: %d\n", offset);
				mdla_pmu_debug("[-] check cmd_id: %d\n",
					check.cmd_id);
				mdla_pmu_debug("[-] check cmd_val[1]:  %08x\n",
				       check.pmu_val[1]);
			}
		}
	}
}

void mdla_util_apusys_pmu_support(bool enable)
{
	apusys_pmu_support = enable;
}

/* IO operation */
static struct mdla_reg_ctl *mdla_reg_control;
static void *apu_conn_top;
static void *infracfg_ao_top;

static u32 mdla_util_dummy_core_r(u32 i, u32 ofs) { return 0; }
static void mdla_util_dummy_core_w(u32 i, u32 ofs, u32 val) {}
static u32 mdla_util_dummy_r(u32 ofs) { return 0; }
static void mdla_util_dummy_w(u32 ofs, u32 val) {}

static struct mdla_util_io_ops io_ops = {
	.cfg = { .read = mdla_util_dummy_core_r,
			.write = mdla_util_dummy_core_w,
			.set_b = mdla_util_dummy_core_w,
			.clr_b = mdla_util_dummy_core_w},
	.cmde = { .read = mdla_util_dummy_core_r,
			.write = mdla_util_dummy_core_w,
			.set_b = mdla_util_dummy_core_w,
			.clr_b = mdla_util_dummy_core_w},
	.biu = { .read = mdla_util_dummy_core_r,
			.write = mdla_util_dummy_core_w,
			.set_b = mdla_util_dummy_core_w,
			.clr_b = mdla_util_dummy_core_w},
	.apu_conn = { .read = mdla_util_dummy_r,
			.write = mdla_util_dummy_w,
			.set_b = mdla_util_dummy_w,
			.clr_b = mdla_util_dummy_w},
	.infra_cfg = { .read = mdla_util_dummy_r,
			.write = mdla_util_dummy_w,
			.set_b = mdla_util_dummy_w,
			.clr_b = mdla_util_dummy_w},
};

const struct mdla_util_io_ops *mdla_util_io_ops_get(void)
{
	return &io_ops;
}

static inline void reg_set(void *base, u32 offset, u32 value)
{
	iowrite32(ioread32(base + offset) | value, base + offset);
}

static inline void reg_clr(void *base, u32 offset, u32 value)
{
	iowrite32(ioread32(base + offset) & (~value), base + offset);
}

static u32 mdla_cfg_read(u32 id, u32 offset)
{
	if (unlikely(core_id_is_invalid(id)))
		return 0;

	return ioread32(mdla_reg_control[id].apu_mdla_config_top + offset);
}

static void mdla_cfg_write(u32 id, u32 offset, u32 value)
{
	if (unlikely(core_id_is_invalid(id)))
		return;

	iowrite32(value, mdla_reg_control[id].apu_mdla_config_top + offset);
}

static void mdla_cfg_set_b(u32 id, u32 offset, u32 value)
{
	if (unlikely(core_id_is_invalid(id)))
		return;

	reg_set(mdla_reg_control[id].apu_mdla_config_top, offset, value);
}

static void mdla_cfg_clr_b(u32 id, u32 offset, u32 value)
{
	if (unlikely(core_id_is_invalid(id)))
		return;

	reg_clr(mdla_reg_control[id].apu_mdla_config_top, offset, value);
}

static u32 mdla_reg_read(u32 id, u32 offset)
{
	if (unlikely(core_id_is_invalid(id)))
		return 0;

	return ioread32(mdla_reg_control[id].apu_mdla_cmde_mreg_top + offset);
}

static void mdla_reg_write(u32 id, u32 offset, u32 value)
{
	if (unlikely(core_id_is_invalid(id)))
		return;

	iowrite32(value, mdla_reg_control[id].apu_mdla_cmde_mreg_top + offset);
}

static void mdla_reg_set_b(u32 id, u32 offset, u32 value)
{
	if (unlikely(core_id_is_invalid(id)))
		return;

	reg_set(mdla_reg_control[id].apu_mdla_cmde_mreg_top, offset, value);
}

static void mdla_reg_clr_b(u32 id, u32 offset, u32 value)
{
	if (unlikely(core_id_is_invalid(id)))
		return;

	reg_clr(mdla_reg_control[id].apu_mdla_cmde_mreg_top, offset, value);
}

static u32 mdla_biu_read(u32 id, u32 offset)
{
	if (unlikely(core_id_is_invalid(id)))
		return 0;

	return ioread32(mdla_reg_control[id].apu_mdla_biu_top + offset);
}

static void mdla_biu_write(u32 id, u32 offset, u32 value)
{
	if (unlikely(core_id_is_invalid(id)))
		return;

	iowrite32(value, mdla_reg_control[id].apu_mdla_biu_top + offset);
}

static void mdla_biu_set_b(u32 id, u32 offset, u32 value)
{
	if (unlikely(core_id_is_invalid(id)))
		return;

	reg_set(mdla_reg_control[id].apu_mdla_biu_top, offset, value);
}

static void mdla_biu_clr_b(u32 id, u32 offset, u32 value)
{
	if (unlikely(core_id_is_invalid(id)))
		return;

	reg_clr(mdla_reg_control[id].apu_mdla_biu_top, offset, value);
}

static u32 mdla_apu_conn_read(u32 offset)
{
	return ioread32(apu_conn_top + offset);
}

static void mdla_apu_conn_write(u32 offset, u32 value)
{
	iowrite32(value, apu_conn_top + offset);
}

static void mdla_apu_conn_set_b(u32 offset, u32 value)
{
	reg_set(apu_conn_top, offset, value);
}

static void mdla_apu_conn_clr_b(u32 offset, u32 value)
{
	reg_clr(apu_conn_top, offset, value);
}

static u32 mdla_infracfg_ao_read(u32 offset)
{
	return ioread32(infracfg_ao_top + offset);
}

static void mdla_infracfg_ao_write(u32 offset, u32 value)
{
	iowrite32(value, infracfg_ao_top + offset);
}

static void mdla_infracfg_ao_set_b(u32 offset, u32 value)
{
	reg_set(infracfg_ao_top, offset, value);
}

static void mdla_infracfg_ao_clr_b(u32 offset, u32 value)
{
	reg_clr(infracfg_ao_top, offset, value);
}

void mdla_util_io_set_addr(struct mdla_reg_ctl *reg_ctl)
{
	if (reg_ctl == NULL)
		return;

	mdla_reg_control = reg_ctl;

	io_ops.cfg.read        = mdla_cfg_read;
	io_ops.cfg.write       = mdla_cfg_write;
	io_ops.cfg.set_b       = mdla_cfg_set_b;
	io_ops.cfg.clr_b       = mdla_cfg_clr_b;
	io_ops.cmde.read       = mdla_reg_read;
	io_ops.cmde.write      = mdla_reg_write;
	io_ops.cmde.set_b      = mdla_reg_set_b;
	io_ops.cmde.clr_b      = mdla_biu_clr_b;
	io_ops.biu.read        = mdla_biu_read;
	io_ops.biu.write       = mdla_biu_write;
	io_ops.biu.set_b       = mdla_biu_set_b;
	io_ops.biu.clr_b       = mdla_reg_clr_b;
}

void mdla_util_io_set_extra_addr(int type,
		void *addr1, void *addr2, void *addr3)
{
	switch (type) {
	case EXTRA_ADDR_V1P0:
		apu_conn_top    = addr1;
		infracfg_ao_top = addr2;
		break;
	case EXTRA_ADDR_V1PX:
		apu_conn_top    = addr1;
		break;
	default:
		return;
	}

	if (apu_conn_top) {
		io_ops.apu_conn.read   = mdla_apu_conn_read;
		io_ops.apu_conn.write  = mdla_apu_conn_write;
		io_ops.apu_conn.set_b  = mdla_apu_conn_set_b;
		io_ops.apu_conn.clr_b  = mdla_apu_conn_clr_b;
	}

	if (infracfg_ao_top) {
		io_ops.infra_cfg.read  = mdla_infracfg_ao_read;
		io_ops.infra_cfg.write = mdla_infracfg_ao_write;
		io_ops.infra_cfg.set_b = mdla_infracfg_ao_set_b;
		io_ops.infra_cfg.clr_b = mdla_infracfg_ao_clr_b;
	}
}
