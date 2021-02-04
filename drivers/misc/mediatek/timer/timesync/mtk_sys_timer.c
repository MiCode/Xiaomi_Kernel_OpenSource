/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */


#include <linux/module.h>
#if defined(CONFIG_MTK_TIMER_TIMESYNC) && !defined(CONFIG_FPGA_EARLY_PORTING)

#include <linux/kernel.h>
#include <linux/clocksource.h>
#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/math64.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/mtk_sys_timer.h>
#include <mtk_sys_timer_typedefs.h>

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#include <mtk_sys_timer_mbox.h>
#include <sspm_define.h>
#include <sspm_ipi.h>
#include <sspm_mbox.h>
#endif

#ifdef CONFIG_MTK_AUDIODSP_SUPPORT
#include <adsp_helper.h>
#endif

#define SYS_TIMER_DEBUG                    (0)

#if SYS_TIMER_DEBUG
#define sys_timer_print(fmt, ...)          pr_debug(fmt, ##__VA_ARGS__)
#else
#define sys_timer_print(fmt, ...)
#endif

#define sys_timer_sysram_write(val, addr) mt_reg_sync_writel(val, addr)

static void __iomem *sys_timer_base;
spinlock_t sys_timer_lock;
static const char sys_timer_node_name[] = "mediatek,sys_timer";
static struct workqueue_struct *sys_timer_workqueue;
static struct sys_timer_timesync_context_t timesync_cxt;

static int sys_timer_device_probe(struct platform_device *pdev);

static inline u64 notrace cyc_to_ns(u64 cyc, u32 mult, u32 shift)
{
	return (cyc * mult) >> shift;
}

void sys_timer_timesync_print_base(void)
{
	u64 temp_u64[2];

	pr_info("timebase:\n");
	pr_info(" enabled  : %d\n", timesync_cxt.enabled);
	pr_info(" base_tick: 0x%llx\n", timesync_cxt.base_tick);

	temp_u64[0] = timesync_cxt.base_ts;
	temp_u64[1] = do_div(temp_u64[0], NSEC_PER_SEC);

	pr_info(" base_ts  : %llu.%llu\n", temp_u64[0], temp_u64[1]);
	pr_info(" base_fz  : %d\n", timesync_cxt.base_fz);
	pr_info(" base_ver : %d\n", timesync_cxt.base_ver);
}

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
static int sys_timer_mbox_write(unsigned int id, unsigned int val)
{
	return sspm_mbox_write(SYS_TIMER_MBOX, id, (void *)&val, 1);
}

static int sys_timer_mbox_read(unsigned int id, unsigned int *val)
{
	return sspm_mbox_read(SYS_TIMER_MBOX, id, val, 1);
}

static u8 sys_timer_timesync_inc_ver(void)
{
	u8 ver;

	if (timesync_cxt.base_ver >= TIMESYNC_MAX_VER)
		ver = 0;
	else
		ver = timesync_cxt.base_ver + 1;

	timesync_cxt.base_ver = ver;

	return ver;
}

static void sys_timer_timesync_update_sspm(int suspended,
	u64 tick, u64 ts)
{
	u32 header, val;

	/* make header: freeze and version */
	header = suspended ? TIMESYNC_HEADER_FREEZE : 0;

	header |= ((timesync_cxt.base_ver << TIMESYNC_HEADER_VER_OFS) &
		TIMESYNC_HEADER_VER_MASK);

	/* update tick, h -> l */
	val = (tick >> 32) & 0xFFFFFFFF;
	val |= header;
	sys_timer_mbox_write(SYS_TIMER_MBOX_TICK_H, val);

	sys_timer_print("%s: tick_h=0x%x\n", __func__, val);

	/* fix update sequence to promise atomicity */
	mb();

	val = tick & 0xFFFFFFFF;
	sys_timer_mbox_write(SYS_TIMER_MBOX_TICK_L, val);

	sys_timer_print("%s: tick_l=0x%x\n", __func__, val);

	/* fix update sequence to promise atomicity */
	mb();

	/* update ts, l -> h */
	val = ts & 0xFFFFFFFF;
	sys_timer_mbox_write(SYS_TIMER_MBOX_TS_L, val);

	sys_timer_print("%s: ts_l=0x%x\n", __func__, val);

	/* fix update sequence to promise atomicity */
	mb();

	val = (ts >> 32) & 0xFFFFFFFF;
	val |= header;
	sys_timer_mbox_write(SYS_TIMER_MBOX_TS_H, val);

	sys_timer_print("%s: ts_h=0x%x\n", __func__, val);

	/* fix update sequence to promise atomicity */
	mb();
}

void sys_timer_timesync_verify_sspm(void)
{
	struct plt_ipi_data_s ipi_data;
	int ackdata = 0;
	u32 ts_h = 0, ts_l = 0;
	u64 ts_sspm, ts_ap1, ts_ap2, temp_u64[2];

	/* reset debug mbox before test */
	sys_timer_mbox_write(SYS_TIMER_MBOX_DEBUG_TS_L, 0);
	sys_timer_mbox_write(SYS_TIMER_MBOX_DEBUG_TS_H, 0);

	ts_ap1 = sched_clock();

	temp_u64[0] = ts_ap1;
	temp_u64[1] = do_div(temp_u64[0], NSEC_PER_SEC);
	pr_info("verify-sspm:ts-ap-1=%llu.%llu\n", temp_u64[0], temp_u64[1]);

	/* send ipi to sspm to trigger test */
	ipi_data.cmd = PLT_TIMESYNC_SRAM_TEST;

	sspm_ipi_send_sync(IPI_ID_PLATFORM, IPI_OPT_WAIT,
		&ipi_data, sizeof(ipi_data) / MBOX_SLOT_SIZE, &ackdata, 1);

	/* wait until sspm writes sspm-view timestamp to sram */
	while (1) {
		sys_timer_mbox_read(SYS_TIMER_MBOX_DEBUG_TS_L, &ts_l);
		sys_timer_mbox_read(SYS_TIMER_MBOX_DEBUG_TS_H, &ts_h);

		if (ts_l || ts_h)
			break;

		pr_info("verify-sspm:polling sspm mbox ...\n");
		cpu_relax();
	}

	ts_ap2 = sched_clock();

	ts_sspm = ((u64)ts_h << 32) | (u64)ts_l;

	temp_u64[0] = ts_sspm;
	temp_u64[1] = do_div(temp_u64[0], NSEC_PER_SEC);
	pr_info("verify-sspm:ts-sspm=%llu.%llu\n", temp_u64[0], temp_u64[1]);

	temp_u64[0] = ts_ap2;
	temp_u64[1] = do_div(temp_u64[0], NSEC_PER_SEC);
	pr_info("verify-sspm:ts-ap-2=%llu.%llu\n", temp_u64[0], temp_u64[1]);

	if (ts_ap1 >= ts_sspm || ts_ap2 <= ts_sspm)
		pr_info("verify-sspm:ERROR!");

	sys_timer_timesync_print_base();
}

#else
#define sys_timer_mbox_read(id, val)
#define sys_timer_mbox_write(id, val)
#define sys_timer_timesync_inc_ver(void)
#define sys_timer_timesync_update_sspm(suspended, tick, ts)
#define sys_timer_timesync_verify_sspm(void)
#endif

static void sys_timer_timesync_update_ram(void __iomem *base,
	int freeze, u64 tick, u64 ts)
{
	sys_timer_sysram_write((tick >> 32) & 0xFFFFFFFF,
		base + TIMESYNC_BASE_TICK);
	sys_timer_sysram_write(tick & 0xFFFFFFFF,
		base + TIMESYNC_BASE_TICK + 4);

	sys_timer_sysram_write((ts >> 32) & 0xFFFFFFFF,
		base + TIMESYNC_BASE_TS);
	sys_timer_sysram_write(ts & 0xFFFFFFFF,
		base + TIMESYNC_BASE_TS + 4);

	sys_timer_sysram_write(freeze,
		base + TIMESYNC_BASE_FREEZE);
}

static void
sys_timer_timesync_sync_base_internal(unsigned int flag)
{
	u64 tick, ts;
	unsigned long irq_flags = 0;
	int freeze, unfreeze;

	spin_lock_irqsave(&timesync_cxt.lock, irq_flags);

	ts = sched_clock_get_cyc(&tick);

	timesync_cxt.base_tick = tick;
	timesync_cxt.base_ts = ts;
	sys_timer_timesync_inc_ver();

	freeze = (flag & SYS_TIMER_TIMESYNC_FLAG_FREEZE) ? 1 : 0;
	unfreeze = (flag & SYS_TIMER_TIMESYNC_FLAG_UNFREEZE) ? 1 : 0;

	/* sync with sysram */
	if (timesync_cxt.support_sysram) {
		sys_timer_timesync_update_ram(timesync_cxt.ram_base,
			freeze, tick, ts);
	}

#ifdef CONFIG_MTK_AUDIODSP_SUPPORT
	if (freeze == 0 && unfreeze == 0) {
	/* sync with adsp */
		adsp_enable_dsp_clk(true);

		sys_timer_timesync_update_ram(ADSP_A_OSTIMER_BUFFER,
			freeze, tick, ts);

		adsp_enable_dsp_clk(false);
	}
#endif

	/* sync with sspm */
	sys_timer_timesync_update_sspm(freeze, tick, ts);

	spin_unlock_irqrestore(&timesync_cxt.lock, irq_flags);

	pr_debug("update base: ts=%llu, tick=0x%llx, fz=%d, ver=%d\n",
		ts, tick, freeze, timesync_cxt.base_ver);
}

void sys_timer_timesync_sync_adsp(unsigned int flag)
{
	u64 tick, ts;
	unsigned long irq_flags = 0;
	int freeze;

	spin_lock_irqsave(&timesync_cxt.lock, irq_flags);

	ts = sched_clock_get_cyc(&tick);

	timesync_cxt.base_tick = tick;
	timesync_cxt.base_ts = ts;
	sys_timer_timesync_inc_ver();

	freeze = (flag & SYS_TIMER_TIMESYNC_FLAG_FREEZE) ? 1 : 0;
#ifdef CONFIG_MTK_AUDIODSP_SUPPORT
	/* sync with adsp */
	sys_timer_timesync_update_ram(ADSP_A_OSTIMER_BUFFER,
		freeze, tick, ts);
#endif
	spin_unlock_irqrestore(&timesync_cxt.lock, irq_flags);

	pr_debug("update base (adsp): ts=%llu, tick=0x%llx, fz=%d, ver=%d\n",
		ts, tick, freeze, timesync_cxt.base_ver);
}
EXPORT_SYMBOL(sys_timer_timesync_sync_adsp);

u64 sys_timer_timesync_tick_to_sched_clock(u64 tick)
{
	u64 ret;
	unsigned long flags = 0;

	if (tick < timesync_cxt.base_tick)
		return 0;

	spin_lock_irqsave(&timesync_cxt.lock, flags);

	if (timesync_cxt.base_fz)
		ret = timesync_cxt.base_ts;
	else {
		ret = cyc_to_ns(tick - timesync_cxt.base_tick,
			timesync_cxt.mult, timesync_cxt.shift) +
			timesync_cxt.base_ts;
	}

	spin_unlock_irqrestore(&timesync_cxt.lock, flags);

	return ret;
}

void sys_timer_timesync_sync_base(unsigned int flag)
{
	if (!timesync_cxt.enabled)
		return;

	if (flag & SYS_TIMER_TIMESYNC_FLAG_ASYNC)
		queue_work(sys_timer_workqueue, &(timesync_cxt.work));
	else
		sys_timer_timesync_sync_base_internal(flag);
}

u64 notrace sys_timer_read_tick(void)
{
	unsigned long flags;
	u32 tick_l, tick_h;

	spin_lock_irqsave(&sys_timer_lock, flags);

	tick_l = __raw_readl(sys_timer_base + SYS_TIMER_CNTCV_L);
	tick_h = __raw_readl(sys_timer_base + SYS_TIMER_CNTCV_H);

	spin_unlock_irqrestore(&sys_timer_lock, flags);

	return (((u64)tick_h << 32) & 0xFFFFFFFF00000000) |
		((u64)tick_l & 0x00000000FFFFFFFF);
}

static const struct platform_device_id sys_timer_id_table[] = {
	{ "sys_timer", 0},
	{ },
};

static const struct of_device_id sys_timer_of_match[] = {
	{ .compatible = "mediatek,sys_timer", },
	{},
};

static struct platform_driver sys_timer_driver = {
	.remove = NULL,
	.shutdown = NULL,
	.suspend = NULL,
	.resume = NULL,
	.probe = sys_timer_device_probe,
	.driver = {
		.name = "sys_timer",
		.owner = THIS_MODULE,
		.of_match_table = sys_timer_of_match,
	},
	.id_table = sys_timer_id_table,
};

static int sys_timer_work_init(void)
{
	sys_timer_workqueue = create_workqueue("sys_timer_wq");

	if (!sys_timer_workqueue) {
		pr_info("workqueue create failed\n");
		return -1;
	}

	return 0;
}

static void sys_timer_timesync_ws(struct work_struct *ws)
{
	sys_timer_timesync_sync_base(SYS_TIMER_TIMESYNC_FLAG_SYNC);
}

#ifdef SYS_TIMER_TIMESYNC_REGULAR
static void sys_timer_timesync_timeout(unsigned long data)
{
	sys_timer_timesync_sync_base(SYS_TIMER_TIMESYNC_FLAG_ASYNC);

	timesync_cxt.timer.expires = jiffies + TIMESYNC_REGULAR_SYNC_SEC;
	add_timer(&(timesync_cxt.timer));
}

static int sys_timer_timesync_cfg_regular_sync(void)
{
	/*
	 * init work for
	 * 1. regular sync by sys_timer itself.
	 * 2. regular sync by sched_clock_poll.
	 */
	INIT_WORK(&(timesync_cxt.work), sys_timer_timesync_ws);

	/* timer for regular sync by sys_timer itself */
	setup_timer(&(timesync_cxt.timer), &sys_timer_timesync_timeout, 0);
	timesync_cxt.timer.expires = jiffies + TIMESYNC_REGULAR_SYNC_SEC;
	add_timer(&(timesync_cxt.timer));

	return 0;
}
#else
static int sys_timer_timesync_cfg_regular_sync(void)
{
	/*
	 * init work for regular sync by sched_clock_poll only
	 */
	INIT_WORK(&(timesync_cxt.work), sys_timer_timesync_ws);

	return 0;
}
#endif

static int sys_timer_timesync_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct resource *res;
	int sysram_size = -1;
	int ret = 0;

	spin_lock_init(&timesync_cxt.lock);

	/* get sysram base */

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sysram_base");
	timesync_cxt.ram_base = devm_ioremap_resource(dev, res);

	if (IS_ERR((void const *)timesync_cxt.ram_base)) {

		pr_info("unable to ioremap sysram base, might be disabled\n");

		/* ensure sysram support is disabled */
		timesync_cxt.support_sysram = 0;
	} else {
		/* get sysram size */

		if (of_property_read_u32(node,
			"mediatek,sysram-size", &sysram_size)) {
			pr_info("unable to get sysram-size\n");
			goto fail_out;
		}

		/* check if we have enough sysram size */

		if (sysram_size < 20) {
			pr_info("not enough sram size %d\n", sysram_size);
			goto fail_out;
		}

		timesync_cxt.support_sysram = 1;
	}

	/* init mult and shift */

	clocks_calc_mult_shift(&timesync_cxt.mult, &timesync_cxt.shift,
		SYS_TIMER_CLK_RATE, NSEC_PER_SEC, TIMESYNC_MAX_SEC);

	pr_info("mult=%u, shift=%u, maxsec=%u\n",
		timesync_cxt.mult, timesync_cxt.shift, TIMESYNC_MAX_SEC);

	timesync_cxt.enabled = 1;

	sys_timer_timesync_cfg_regular_sync();

	/* add the 1st base */
	sys_timer_timesync_sync_base(SYS_TIMER_TIMESYNC_FLAG_SYNC);

	goto out;

fail_out:

	/* ensure disabled */
	timesync_cxt.enabled = 0;

	ret = -1;

out:
	pr_info("enabled: %d, support_sysram: %d\n",
		timesync_cxt.enabled, timesync_cxt.support_sysram);

	return ret;
}

#ifdef CONFIG_DEBUG_FS
static ssize_t sys_timer_dbgfs_debug_write(struct file *filp,
				      const char __user *ubuf, size_t cnt,
				       loff_t *ppos)
{
	int ret;
	int val = 0;

	ret = kstrtoint_from_user(ubuf, cnt, 0, &val);
	if (ret) {
		pr_info("invalid argument\n");
		return ret;
	}

	if (val == 1) {
		/*
		 * send IPI to request SSPM to write its timestamp in
		 * SRAM for verification
		 */
		sys_timer_timesync_verify_sspm();
	} else if (val == 2) {
		/* synchronous timesync test */
		sys_timer_timesync_sync_base(SYS_TIMER_TIMESYNC_FLAG_SYNC);
	} else if (val == 3) {
		/* synchronous timesync test with suspend flag set */
		sys_timer_timesync_sync_base(SYS_TIMER_TIMESYNC_FLAG_SYNC |
			SYS_TIMER_TIMESYNC_FLAG_FREEZE);
	} else if (val == 4) {
		/* asynchronous timesync test */
		sys_timer_timesync_sync_base(SYS_TIMER_TIMESYNC_FLAG_ASYNC);
	} else if (val == 5) {
		/* asynchronous timesync test with suspend flag set */
		sys_timer_timesync_sync_base(SYS_TIMER_TIMESYNC_FLAG_ASYNC |
			SYS_TIMER_TIMESYNC_FLAG_FREEZE);
	} else
		pr_info("unsupported value %d\n", val);

	return cnt;
}

static int
sys_timer_dbgfs_debug_show(struct seq_file *file, void *data)
{
	sys_timer_timesync_print_base();

	return 0;
}

static int
sys_timer_dbgfs_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, sys_timer_dbgfs_debug_show, inode->i_private);
}

static const struct file_operations sys_timer_dbgfs_debug_fops = {
	.open		= sys_timer_dbgfs_debug_open,
	.read		= seq_read,
	.write		= sys_timer_dbgfs_debug_write,
};

static void sys_timer_init_debugfs(void)
{
	if (timesync_cxt.dbgfs_root)
		return;

	timesync_cxt.dbgfs_root = debugfs_create_dir("sys_timer", NULL);

	if (IS_ERR(timesync_cxt.dbgfs_root))
		/* don't complain -- debugfs just isn't enabled */
		goto err_no_root;

	if (!timesync_cxt.dbgfs_root) {
		/*
		 * complain -- debugfs is enabled, but it failed to
		 * create the directory
		 */
		pr_info("null debugfs root directory, exiting\n");
		goto err_no_root;
	}

	timesync_cxt.dbgfs_debug =
		debugfs_create_file("debug", 0600,
			timesync_cxt.dbgfs_root, &timesync_cxt,
				&sys_timer_dbgfs_debug_fops);
	if (!timesync_cxt.dbgfs_debug) {
		pr_info("null err_stats file, exiting\n");
		goto err;
	}

	return;

err:
	debugfs_remove_recursive(timesync_cxt.dbgfs_root);
	timesync_cxt.dbgfs_root = NULL;

err_no_root:
	pr_info("failed to initialize debugfs\n");
}
#else
#define sys_timer_init_debugfs()
#endif

static int sys_timer_device_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;
	int ret = 0;

	spin_lock_init(&sys_timer_lock);

	/* get sys timer base */

	res = platform_get_resource_byname(pdev,
			IORESOURCE_MEM, "sys_timer_base");
	sys_timer_base = devm_ioremap_resource(dev, res);

	if (IS_ERR((void const *)sys_timer_base)) {
		pr_info("unable to ioremap sys timer base\n");
		ret = -1;
		goto out;
	}

	ret = sys_timer_work_init();
	if (ret)
		goto out;

	ret = sys_timer_timesync_init(pdev);
	if (ret)
		goto out;

	sys_timer_init_debugfs();

out:
	return ret;
}

static int __init sys_timer_device_init(void)
{
	if (platform_driver_register(&sys_timer_driver)) {
		pr_info("device register fail\n");
		return -1;
	}

	return 0;
}

#else

static int __init sys_timer_device_init(void)
{
	return 0;
}

#endif

/*
 * shall not be prior than initialization of target sub-sys,
 * for example,
 *   sspm: mbox shall be ready.
 *   adsp: io-remapped ram address for timesync base shall be ready.
 */
late_initcall(sys_timer_device_init);

MODULE_AUTHOR("Stanley Chu <stanley.chu@mediatek.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mediatek Sys Timer");

