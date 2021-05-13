// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/of_device.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/debugfs.h>
#include <linux/sched/clock.h>

#include <common/mdla_device.h>
#include <common/mdla_cmd_proc.h>
#include <common/mdla_power_ctrl.h>

#include <utilities/mdla_debug.h>
#include <utilities/mdla_profile.h>
#include <utilities/mdla_util.h>
#include <utilities/mdla_trace.h>

#include <interface/mdla_intf.h>
#include <interface/mdla_cmd_data_v1_x.h>

#include <platform/mdla_plat_api.h>

#include "mdla_hw_reg_v1_x.h"
#include "mdla_irq_v1_x.h"
#include "mdla_pmu_v1_x.h"
#include "mdla_sched_v1_x.h"

#define DBGFS_PMU_NAME      "pmu_reg"
#define DBGFS_USAGE_NAME    "help"

static struct mdla_reg_ctl *mdla_reg_control;
static void *apu_conn_top;
static void *apu_mdla_gsm_top;
static void *apu_mdla_gsm_base;

static struct mdla_dev *mdla_plat_devices;

static u32 fs_zero_skip_count;

static int mdla_dbgfs_u64_create[NF_MDLA_DEBUG_FS_U64] = {
	[FS_CFG_PMU_PERIOD] = 1,
};

static bool mdla_dbgfs_u32_create[NF_MDLA_DEBUG_FS_U32] = {
	[FS_C1]                = 1,
	[FS_C2]                = 1,
	[FS_C3]                = 1,
	[FS_C4]                = 1,
	[FS_C5]                = 1,
	[FS_C6]                = 1,
	[FS_C7]                = 1,
	[FS_C8]                = 1,
	[FS_C9]                = 1,
	[FS_C10]               = 1,
	[FS_C11]               = 1,
	[FS_C12]               = 1,
	[FS_C13]               = 1,
	[FS_C14]               = 1,
	[FS_C15]               = 1,
	[FS_CFG_ENG0]          = 1,
	[FS_CFG_ENG1]          = 1,
	[FS_CFG_ENG2]          = 1,
	[FS_CFG_ENG11]         = 1,
	[FS_POLLING_CMD_DONE]  = 1,
	[FS_DUMP_CMDBUF]       = 1,
	[FS_DVFS_RAND]         = 1,
	[FS_PMU_EVT_BY_APU]    = 1,
	[FS_KLOG]              = 1,
	[FS_POWEROFF_TIME]     = 1,
	[FS_TIMEOUT]           = 1,
	[FS_TIMEOUT_DBG]       = 1,
	[FS_BATCH_NUM]         = 1,
	[FS_PREEMPTION_TIMES]  = 1,
	[FS_PREEMPTION_DBG]    = 1,
};

static struct lock_class_key hw_lock_key[MAX_CORE_NUM];

/* platform static functions */

static int mdla_plat_zero_skip_detect(u32 core_id)
{
	const struct mdla_util_io_ops *io = mdla_util_io_ops_get();
	u32 debug_intf, front_c_invalid;

	if (io->cmde.read(core_id, 0x0DB0) != 0x6)
		return 0;

	debug_intf =
		io->cmde.read(core_id, 0x0DB8);
	front_c_invalid =
		io->cmde.read(core_id, 0x0D74);

	if ((debug_intf == front_c_invalid) ||
		(debug_intf == (front_c_invalid/2))) {
		mdla_timeout_debug("core:%d, %s: match zero skip issue\n",
			core_id,
			__func__);
		fs_zero_skip_count++;
		return -1;
	}
	return 0;
}

static void mdla_plat_print_post_cmd_info(u32 core_id)
{
	const struct mdla_util_io_ops *io = mdla_util_io_ops_get();

	mdla_verbose("%s: C:%d,FIN1: %.8x\n",
			__func__,
			core_id,
			io->cmde.read(core_id, MREG_TOP_G_FIN1));
	mdla_verbose("dst addr:%.8x\n", io->cmde.read(core_id, 0xE3C));

	mdla_pmu_debug("%s: CFG_PMCR: %8x, pmu_clk_cnt: %.8x\n",
		__func__,
		io->biu.read(core_id, CFG_PMCR),
		io->biu.read(core_id, PMU_CLK_CNT));
}

static inline unsigned long mdla_plat_get_wait_time(u32 core_id)
{
	unsigned long time;

	if (mdla_prof_pmu_tmr_en(core_id))
		time = usecs_to_jiffies(mdla_dbg_read_u64(FS_CFG_PMU_PERIOD));
	else
		time = msecs_to_jiffies(mdla_dbg_read_u32(FS_POLLING_CMD_DONE));

	return time;
}

static void mdla_plat_destroy_dump_cmdbuf(struct mdla_dev *mdla_device)
{
	mutex_lock(&mdla_device->cmd_buf_dmp_lock);

	if (mdla_device->cmd_buf_len) {
		kvfree(mdla_device->cmd_buf_dmp);
		mdla_device->cmd_buf_len = 0;
	}

	mutex_unlock(&mdla_device->cmd_buf_dmp_lock);
}

static int mdla_plat_create_dump_cmdbuf(struct mdla_dev *mdla_device, struct command_entry *ce)
{
	int ret = 0;

	if (ce->kva == NULL)
		return ret;

	mutex_lock(&mdla_device->cmd_buf_dmp_lock);

	if (mdla_device->cmd_buf_len)
		kvfree(mdla_device->cmd_buf_dmp);

	mdla_device->cmd_buf_dmp = kvmalloc(ce->count * MREG_CMD_SIZE, GFP_KERNEL);

	if (!mdla_device->cmd_buf_dmp) {
		ret = -ENOMEM;
		mdla_err("%s: kvmalloc: failed\n", __func__);
		goto out;
	}
	mdla_device->cmd_buf_len = ce->count * MREG_CMD_SIZE;
	memcpy(mdla_device->cmd_buf_dmp, ce->kva, mdla_device->cmd_buf_len);

out:
	mutex_unlock(&mdla_device->cmd_buf_dmp_lock);
	return ret;
}

static int mdla_plat_pre_cmd_handle(u32 core_id, struct command_entry *ce)
{
	if (ce->multicore_total > 1)
		mdla_plat_devices[core_id].sched->set_smp_deadline(ce->priority, ce->deadline_t);

	if (mdla_dbg_read_u32(FS_DUMP_CMDBUF))
		mdla_plat_create_dump_cmdbuf(mdla_get_device(core_id), ce);
	return 0;
}

static int mdla_plat_wait_cmd_handle(u32 core_id, struct command_entry *ce)
{
	/* update SMP CMD deadline */
	if (ce->multicore_total > 1) {
		u64 deadline = mdla_plat_devices[core_id].sched->get_smp_deadline(ce->priority);

		if (deadline > ce->deadline_t)
			ce->deadline_t = deadline;
	}

	return 0;
}

/* NOTE: hw_lock should be acquired by caller */
static void mdla_plat_raw_process_command(u32 core_id, u32 evt_id, dma_addr_t addr, u32 count)
{
	const struct mdla_util_io_ops *io = mdla_util_io_ops_get();

	/* set command address */
	io->cmde.write(core_id, MREG_TOP_G_CDMA1, addr);
	/* set command number */
	io->cmde.write(core_id, MREG_TOP_G_CDMA2, count);

	mdla_plat_devices[core_id].sched->pro_ce->req_start_t = sched_clock();
	/* trigger hw */
	io->cmde.write(core_id, MREG_TOP_G_CDMA3, evt_id);
}

static int mdla_plat_process_command(u32 core_id, struct command_entry *ce)
{
	dma_addr_t addr;
	u32 evt_id, count;
	unsigned long flags;

	addr = ce->mva;
	count = ce->count;
	evt_id = ce->count;

	mdla_verbose("%s: count: %d, addr: %lx\n",
		__func__, ce->count,
		(unsigned long)addr);

	spin_lock_irqsave(&mdla_get_device(core_id)->hw_lock, flags);

	mdla_util_pmu_ops_get()->reset(core_id);
	mdla_util_pmu_ops_get()->write_evt_exec(core_id, 0);

	ce->state = CE_RUN;
	mdla_plat_raw_process_command(core_id, evt_id, addr, count);

	spin_unlock_irqrestore(&mdla_get_device(core_id)->hw_lock, flags);

	return 0;
}

static void mdla_plat_dump_reg(u32 core_id, struct seq_file *s)
{
	dump_reg_cfg(core_id, MDLA_CG_CON);
	dump_reg_cfg(core_id, MDLA_SW_RST);
	dump_reg_cfg(core_id, MDLA_MBIST_MODE0);
	dump_reg_cfg(core_id, MDLA_MBIST_MODE1);
	dump_reg_cfg(core_id, MDLA_MBIST_CTL);
	dump_reg_cfg(core_id, MDLA_MBIST_DEFAULT_DELSEL);
	dump_reg_cfg(core_id, MDLA_RP_RST);
	dump_reg_cfg(core_id, MDLA_RP_CON);
	dump_reg_cfg(core_id, MDLA_AXI_CTRL);
	dump_reg_cfg(core_id, MDLA_AXI1_CTRL);

	dump_reg_top(core_id, MREG_TOP_G_REV);
	dump_reg_top(core_id, MREG_TOP_G_INTP0);
	dump_reg_top(core_id, MREG_TOP_G_INTP1);
	dump_reg_top(core_id, MREG_TOP_G_INTP2);
	dump_reg_top(core_id, MREG_TOP_G_CDMA0);
	dump_reg_top(core_id, MREG_TOP_G_CDMA1);
	dump_reg_top(core_id, MREG_TOP_G_CDMA2);
	dump_reg_top(core_id, MREG_TOP_G_CDMA3);
	dump_reg_top(core_id, MREG_TOP_G_CDMA4);
	dump_reg_top(core_id, MREG_TOP_G_CDMA5);
	dump_reg_top(core_id, MREG_TOP_G_CDMA6);
	dump_reg_top(core_id, MREG_TOP_G_CUR0);
	dump_reg_top(core_id, MREG_TOP_G_CUR1);
	dump_reg_top(core_id, MREG_TOP_G_FIN0);
	dump_reg_top(core_id, MREG_TOP_G_FIN1);
	dump_reg_top(core_id, MREG_TOP_G_FIN3);
	dump_reg_top(core_id, MREG_TOP_G_IDLE);

}

static void mdla_plat_memory_show(struct seq_file *s)
{
	struct mdla_dev *mdla_device;
	int i;
	u32 core_id;
	u32 *cmd_addr;

	seq_puts(s, "------- dump MDLA code buf -------\n");

	for_each_mdla_core(core_id) {
		mdla_device = mdla_get_device(core_id);
		if (mdla_device->cmd_buf_len == 0)
			continue;
		seq_printf(s, "mdla %d code buf:\n", mdla_device->mdla_id);
		mutex_lock(&mdla_device->cmd_buf_dmp_lock);
		cmd_addr = mdla_device->cmd_buf_dmp;
		for (i = 0; i < (mdla_device->cmd_buf_len/4); i++)
			seq_printf(s, "count: %d, offset: %.8x, val: %.8x\n",
				(i * 4) / MREG_CMD_SIZE,
				(i * 4) % MREG_CMD_SIZE,
				cmd_addr[i]);
		mutex_unlock(&mdla_device->cmd_buf_dmp_lock);
	}
}

static bool mdla_plat_dbgfs_u64_enable(int node)
{
	return node >= 0 && node < NF_MDLA_DEBUG_FS_U64
			? mdla_dbgfs_u64_create[node] : false;
}

static bool mdla_plat_dbgfs_u32_enable(int node)
{
	return node >= 0 && node < NF_MDLA_DEBUG_FS_U32
			? mdla_dbgfs_u32_create[node] : false;
}

static int mdla_plat_register_show(struct seq_file *s, void *data)
{
	mdla_v1_x_pmu_info_show(s);

	return 0;
}

static int mdla_plat_dbgfs_usage(struct seq_file *s, void *data)
{
	seq_printf(s, "---- Kernel debug log mask (current = 0x%x) ----\n",
				mdla_dbg_read_u32(FS_KLOG));
	seq_printf(s, "echo [mask(hex)] > /d/mdla/%s\n",
				mdla_dbg_get_u32_node_str(FS_KLOG));
	mdla_dbg_show_klog_info(s, "\t");

	seq_puts(s, "\n---- Dump MDLA HW register ----\n");
	seq_printf(s, "cat /d/mdla/%s\n", DBGFS_HW_REG_NAME);

	seq_puts(s, "\n---- Dump the last code buffer ----\n");
	seq_printf(s, "echo [1|0] > /d/mdla/%s\n",
				mdla_dbg_get_u32_node_str(FS_DUMP_CMDBUF));
	seq_printf(s, "cat /d/mdla/%s\n", DBGFS_CMDBUF_NAME);

	seq_puts(s, "\n---- Command timeout setting ----\n");
	seq_printf(s, "echo [ms(dec)] > /d/mdla/%s\n",
				mdla_dbg_get_u32_node_str(FS_TIMEOUT));

	seq_puts(s, "\n---- Set delay time of power off (default = 2s) ----\n");
	seq_printf(s, "echo [ms(dec)] > /d/mdla/%s\n",
				mdla_dbg_get_u32_node_str(FS_POWEROFF_TIME));

	seq_puts(s, "\n---- Show power usage ----\n");
	seq_printf(s, "cat /d/mdla/%s\n", DBGFS_PWR_NAME);

	seq_puts(s, "\n---- Dump PMU data ----\n");
	seq_printf(s, "cat /d/mdla/%s\n", DBGFS_PMU_NAME);

	seq_puts(s, "\n");

	return 0;
}

static void mdla_plat_dbgfs_init(struct device *dev, struct dentry *parent)
{
	if (!dev || !parent)
		return;

	debugfs_create_devm_seqfile(dev, DBGFS_PMU_NAME, parent,
				mdla_plat_register_show);
	debugfs_create_u32("zero_skip_count", 0660,
			parent, &fs_zero_skip_count);
	debugfs_create_devm_seqfile(dev, DBGFS_USAGE_NAME, parent,
				mdla_plat_dbgfs_usage);
}

static int mdla_plat_get_base_addr(struct platform_device *pdev, void **reg, int num)
{
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, num);
	if (!res) {
		dev_info(&pdev->dev, "invalid address (num = %d)\n", num);
		return -ENODEV;
	}

	*reg = ioremap_wc(res->start, res->end - res->start + 1);
	if (*reg == 0) {
		dev_info(&pdev->dev,
			"could not allocate iomem (num = %d)\n", num);
		return -EIO;
	}

	dev_info(&pdev->dev,
		"IORESOURCE_MEM (num = %d) at 0x%08lx mapped to 0x%08lx\n",
		num,
		(unsigned long __force)res->start,
		(unsigned long __force)res->end);

	return 0;
}

static int mdla_dts_map(struct platform_device *pdev)
{
	int i, mdla_idx, gsm_idx, apu_conn_idx;
	struct device *dev = &pdev->dev;
	u32 nr_core_ids = mdla_util_get_core_num();

	dev_info(dev, "Device Tree Probing\n");

	mdla_reg_control = kcalloc(nr_core_ids, sizeof(struct mdla_reg_ctl), GFP_KERNEL);

	if (!mdla_reg_control)
		return -1;

	mdla_idx     = 0;
	gsm_idx	     = 3 * nr_core_ids;
	apu_conn_idx = 3 * nr_core_ids + 1;

	for (i = mdla_idx;  i < nr_core_ids; i++) {
		if (mdla_plat_get_base_addr(pdev,
				&mdla_reg_control[i].apu_mdla_config_top,
				3 * i))
			goto err;

		if (mdla_plat_get_base_addr(pdev,
				&mdla_reg_control[i].apu_mdla_cmde_mreg_top,
				3 * i + 1)) {
			iounmap(mdla_reg_control[i].apu_mdla_config_top);
			goto err;
		}

		if (mdla_plat_get_base_addr(pdev,
				&mdla_reg_control[i].apu_mdla_biu_top,
				3 * i + 2)) {
			iounmap(mdla_reg_control[i].apu_mdla_cmde_mreg_top);
			iounmap(mdla_reg_control[i].apu_mdla_config_top);
			goto err;
		}
	}

	if (mdla_plat_get_base_addr(pdev, &apu_mdla_gsm_top, gsm_idx))
		goto err;
	else
		apu_mdla_gsm_base = (void *)platform_get_resource(pdev,
					IORESOURCE_MEM, gsm_idx)->start;

	dev_info(dev, "apu_mdla_gsm_top: %p, apu_mdla_gsm_base: %p\n",
			apu_mdla_gsm_top, apu_mdla_gsm_base);

	if (mdla_plat_get_base_addr(pdev, &apu_conn_top, apu_conn_idx))
		goto err_conn;

	if (mdla_v1_x_irq_request(dev, nr_core_ids))
		goto err_irq;

	return 0;

err_irq:
	iounmap(apu_conn_top);
err_conn:
	iounmap(apu_mdla_gsm_top);
err:
	for (i = i - 1;  i >= mdla_idx; i--) {
		iounmap(mdla_reg_control[i].apu_mdla_config_top);
		iounmap(mdla_reg_control[i].apu_mdla_cmde_mreg_top);
		iounmap(mdla_reg_control[i].apu_mdla_biu_top);
	}
	kfree(mdla_reg_control);
	return -1;
}

static void mdla_dts_unmap(struct platform_device *pdev)
{
	int i;
	u32 nr_core_ids = mdla_util_get_core_num();

	mdla_v1_x_irq_release(&pdev->dev);

	iounmap(apu_conn_top);
	iounmap(apu_mdla_gsm_top);

	for (i = 0; i < nr_core_ids; i++) {
		iounmap(mdla_reg_control[i].apu_mdla_config_top);
		iounmap(mdla_reg_control[i].apu_mdla_cmde_mreg_top);
		iounmap(mdla_reg_control[i].apu_mdla_biu_top);
	}

	kfree(mdla_reg_control);
}

static int mdla_sw_multi_devices_init(struct device *dev)
{
	int i;
	u32 nr_core_ids = mdla_util_get_core_num();

	mdla_plat_devices = devm_kzalloc(dev, nr_core_ids * sizeof(struct mdla_dev), GFP_KERNEL);

	if (!mdla_plat_devices)
		return -1;

	mdla_set_device(mdla_plat_devices, nr_core_ids);

	for (i = 0; i < nr_core_ids; i++) {

		mdla_plat_devices[i].mdla_id = i;
		mdla_plat_devices[i].dev = dev;
		mdla_plat_devices[i].cmd_buf_dmp = NULL;
		mdla_plat_devices[i].cmd_buf_len = 0;

		INIT_LIST_HEAD(&mdla_plat_devices[i].cmd_list);
		init_completion(&mdla_plat_devices[i].command_done);

		spin_lock_init(&mdla_plat_devices[i].hw_lock);
		lockdep_set_class(&mdla_plat_devices[i].hw_lock,
						&hw_lock_key[i]);

		mutex_init(&mdla_plat_devices[i].cmd_lock);
		mutex_init(&mdla_plat_devices[i].cmd_list_lock);
		mutex_init(&mdla_plat_devices[i].cmd_buf_dmp_lock);

		mdla_v1_x_pmu_init(&mdla_plat_devices[i]);
	}

	return 0;
}

static void mdla_sw_multi_devices_deinit(void)
{
	int i;
	u32 nr_core_ids = mdla_util_get_core_num();

	mdla_set_device(NULL, 0);

	for (i = 0; i < nr_core_ids; i++) {

		mdla_v1_x_pmu_deinit(&mdla_plat_devices[i]);

		/* TODO: Need to kill completion and wait it finished ? */
		mutex_destroy(&mdla_plat_devices[i].cmd_lock);
		mutex_destroy(&mdla_plat_devices[i].cmd_list_lock);
		mutex_destroy(&mdla_plat_devices[i].cmd_buf_dmp_lock);
	}
}

static void mdla_v1_x_reset(u32 core_id, const char *str)
{
	unsigned long flags;
	const struct mdla_util_io_ops *io = mdla_util_io_ops_get();
	struct mdla_dev *dev = mdla_get_device(core_id);

	if (unlikely(!dev)) {
		mdla_err("%s(): No mdla device (%d)\n", __func__, core_id);
		return;
	}

	/* use power down==>power on apis insted bus protect init */
	mdla_pwr_debug("%s: MDLA RESET: %s\n", __func__,
		str);

	spin_lock_irqsave(&dev->hw_lock, flags);
	io->cfg.write(core_id, MDLA_CG_CLR, 0xffffffff);
	io->cmde.write(core_id, MREG_TOP_G_INTP2, MDLA_IRQ_MASK & ~(MDLA_IRQ_SWCMD_DONE));

	/* for DCM and CG */
	io->cmde.write(core_id, MREG_TOP_ENG0, mdla_dbg_read_u32(FS_CFG_ENG0));
	io->cmde.write(core_id, MREG_TOP_ENG1, mdla_dbg_read_u32(FS_CFG_ENG1));
	io->cmde.write(core_id, MREG_TOP_ENG2, mdla_dbg_read_u32(FS_CFG_ENG2));
	io->cmde.write(core_id, MREG_TOP_ENG11, mdla_dbg_read_u32(FS_CFG_ENG11));

	if (mdla_plat_iommu_enable()) {
		io->cfg.set_b(core_id, MDLA_AXI_CTRL, MDLA_AXI_CTRL_MASK);
		io->cfg.set_b(core_id, MDLA_AXI1_CTRL, MDLA_AXI_CTRL_MASK);
	}

	spin_unlock_irqrestore(&dev->hw_lock, flags);

	mdla_trace_reset(core_id, str);
}

/* platform public functions */

int mdla_v1_x_init(struct platform_device *pdev)
{
	struct mdla_cmd_cb_func *cmd_cb = mdla_cmd_plat_cb();
	struct mdla_dbg_cb_func *dbg_cb = mdla_dbg_plat_cb();

	dev_info(&pdev->dev, "%s()\n", __func__);

	if (mdla_sw_multi_devices_init(&pdev->dev))
		return -1;

	if (mdla_dts_map(pdev))
		goto err;

	if (mdla_plat_pwr_drv_ready()) {
		if (mdla_pwr_device_register(pdev, mdla_pwr_on_v1_x, mdla_pwr_off_v1_x))
			goto err_pwr;
	}

	mdla_pwr_reset_setup(mdla_v1_x_reset);

	if (mdla_v1_x_sched_init())
		goto err_sched;

	/* set command strategy */
	if (mdla_plat_sw_preemption_support())
		mdla_cmd_setup(mdla_cmd_run_sync_v1_x_sched, NULL);
	else
		mdla_cmd_setup(mdla_cmd_run_sync_v1_x, NULL);

	/* set command callback */
	cmd_cb->pre_cmd_handle      = mdla_plat_pre_cmd_handle;
	cmd_cb->post_cmd_hw_detect  = mdla_plat_zero_skip_detect;
	cmd_cb->post_cmd_info       = mdla_plat_print_post_cmd_info;
	cmd_cb->wait_cmd_handle     = mdla_plat_wait_cmd_handle;
	cmd_cb->get_irq_num         = mdla_v1_x_get_irq_num;
	cmd_cb->get_wait_time       = mdla_plat_get_wait_time;
	cmd_cb->process_command     = mdla_plat_process_command;

	/* set debug callback */
	dbg_cb->destroy_dump_cmdbuf = mdla_plat_destroy_dump_cmdbuf;
	dbg_cb->create_dump_cmdbuf  = mdla_plat_create_dump_cmdbuf;
	dbg_cb->dump_reg            = mdla_plat_dump_reg;
	dbg_cb->memory_show         = mdla_plat_memory_show;
	dbg_cb->dbgfs_u64_enable    = mdla_plat_dbgfs_u64_enable;
	dbg_cb->dbgfs_u32_enable    = mdla_plat_dbgfs_u32_enable;
	dbg_cb->dbgfs_plat_init     = mdla_plat_dbgfs_init;

	/* set base address */
	mdla_util_io_set_addr(mdla_reg_control);
	mdla_util_io_set_extra_addr(EXTRA_ADDR_V1PX, apu_conn_top, NULL, NULL);

	return 0;

err_sched:
	if (mdla_plat_pwr_drv_ready())
		mdla_pwr_device_unregister(pdev);
err_pwr:
	dev_info(&pdev->dev, "register mdla power fail\n");
	mdla_dts_unmap(pdev);
err:
	mdla_sw_multi_devices_deinit();
	return -1;
}

void mdla_v1_x_deinit(struct platform_device *pdev)
{
	int i;

	dev_info(&pdev->dev, "%s()\n", __func__);

	mdla_v1_x_sched_deinit();

	for_each_mdla_core(i) {
		mdla_pwr_ops_get()->off(i, 0, true);
		mdla_plat_destroy_dump_cmdbuf(&mdla_plat_devices[i]);
	}

	if (mdla_plat_pwr_drv_ready()
			&& mdla_pwr_device_unregister(pdev))
		dev_info(&pdev->dev, "unregister mdla power fail\n");

	mdla_dts_unmap(pdev);
	mdla_sw_multi_devices_deinit();
}
