/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/time.h>

#include <sspm_helper.h>
#include <sspm_mbox.h>
#include <sspm_ipi_id.h>
#include <sspm_define.h>
#include <sspm_reservedmem.h>

#include "pmsr.h"

struct pmsr_cfg cfg;
struct pmsr_tool_ipi_data pmsr_ipi_data;
static int ipi_ack_data;
static unsigned int window_len_pre;
struct hrtimer pmsr_timer;

#define PMSR_COMPATIBLE_STRING	"mediatek,spmtwam"

static char *ch_name[] = {
	"ch0",
	"ch1",
	"ch2",
	"ch3",
};

static void pmsr_cfg_init(void)
{
	int i;

	cfg.enable = false;
	cfg.pmsr_speed_mode = DEFAULT_SPEED_MODE;
	cfg.pmsr_window_len = 0;
	window_len_pre = 0;

	for (i = 0 ; i < SET_CH_MAX; i++) {
		cfg.ch[i].signal = 0;
		cfg.ch[i].id = 0xFFFFFFFF; /* default disabled */
		cfg.ch[i].montype = DEFAULT_MONTYPE;
	}
}

static int pmsr_ipi_init(void)
{
	unsigned int ret = 0;

	/* for AP to SSPM */
	if (is_sspm_ready()) {
		ret = mtk_ipi_register(&sspm_ipidev, IPIS_C_SPM_SUSPEND, NULL, NULL,
				       (void *) &ipi_ack_data);
	}

	return ret;
}

static void pmsr_procfs_exit(void)
{
	remove_proc_entry("pmsr", NULL);
}

static ssize_t remote_data_read(struct file *filp, char __user *userbuf,
				size_t count, loff_t *f_pos)
{
	return 0;
}

static ssize_t remote_data_write(struct file *fp, const char __user *userbuf,
				 size_t count, loff_t *f_pos)
{
	unsigned int *v = PDE_DATA(file_inode(fp));
	int ret;
	int i;
	static unsigned int window_len_pre;

	if (kstrtou32_from_user(userbuf, count, 10, v))
		return -EFAULT;

	if ((void *)v == (void *)&cfg.pause) {
		pmsr_ipi_data.uid = 0x0;
		pmsr_ipi_data.action = (cfg.pause) ?
			PMSR_TOOL_ACT_PAUSE : PMSR_TOOL_ACT_RESUME;
		ret = mtk_ipi_send_compl(&sspm_ipidev,
					 IPIS_C_SPM_SUSPEND,
					 IPI_SEND_POLLING,
					 &pmsr_ipi_data, 8, 2000);
	} else if ((void *)v == (void *)&cfg.enable) {
		if (cfg.enable == true) {
			for (i = 0 ; i < SET_CH_MAX; i++) {
				if (cfg.ch[i].id == 0xFFFFFFFF)
					continue;

				pmsr_ipi_data.uid = 0x0;
				pmsr_ipi_data.action = PMSR_TOOL_ACT_CHSET;
				pmsr_ipi_data.sig = cfg.ch[i].signal;
				pmsr_ipi_data.id = cfg.ch[i].id;
				pmsr_ipi_data.montype = cfg.ch[i].montype;
				//pmsr_ipi_data.window_len = cfg.pmsr_window_len;
				pmsr_ipi_data.speed_mode = cfg.pmsr_speed_mode;
				pmsr_ipi_data.idx = i;

				ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_SPM_SUSPEND,
							 IPI_SEND_POLLING, &pmsr_ipi_data,
							 8, 2000);
			}
			if (window_len_pre != cfg.pmsr_window_len) {
				pmsr_ipi_data.window_len = cfg.pmsr_window_len;
				pmsr_ipi_data.action = PMSR_TOOL_ACT_WINDOW;

				ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_SPM_SUSPEND,
							 IPI_SEND_POLLING, &pmsr_ipi_data,
							 8, 2000);
			}

			window_len_pre = cfg.pmsr_window_len;
			pmsr_ipi_data.action = PMSR_TOOL_ACT_ENABLE;
			if (cfg.fake)
				pmsr_ipi_data.action |= PMSR_TOOL_ACT_TEST_FAKE;

			ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_SPM_SUSPEND,
						 IPI_SEND_POLLING, &pmsr_ipi_data,
						 8, 2000);

			if (!ret) {
				hrtimer_start(&pmsr_timer,
					      ns_to_ktime(pmsr_ipi_data.window_len * NSEC_PER_USEC),
					      HRTIMER_MODE_REL_PINNED);
			}
		} else {
			pmsr_cfg_init();
			pmsr_ipi_data.action = PMSR_TOOL_ACT_DISABLE;

			ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_SPM_SUSPEND,
						 IPI_SEND_POLLING, &pmsr_ipi_data,
						 8, 2000);
			hrtimer_try_to_cancel(&pmsr_timer);
		}
	} else if ((void *)v == (void *)&cfg.output) {
		pmsr_ipi_data.uid = 0x0;
		pmsr_ipi_data.action = PMSR_TOOL_ACT_CH_OUTBUFFER;

		ret = mtk_ipi_send_compl(&sspm_ipidev,
					 IPIS_C_SPM_SUSPEND, IPI_SEND_POLLING,
					 &pmsr_ipi_data, 8, 2000);
	}

	return count;
}

const static struct file_operations remote_data_fops = {
	.owner = THIS_MODULE,
	.read = remote_data_read,
	.write = remote_data_write,
};

static char dbgbuf[1024] = {0};
#define log2buf(p, s, fmt, args...) \
	(p += scnprintf(p, sizeof(s) - strlen(s), fmt, ##args))
#undef log
#define log(fmt, args...)   log2buf(p, dbgbuf, fmt, ##args)

static ssize_t local_ipi_read(struct file *fp, char __user *userbuf,
			      size_t count, loff_t *f_pos)
{
	int i, len = 0;
	char *p = dbgbuf;

	p[0] = '\0';

	log("pmsr state:\n");
	log("enable %d\n", cfg.enable ? 1 : 0);
	log("speed_mode %d (0: low, 1: high)\n",
	    cfg.pmsr_speed_mode ? 1 : 0);
	log("window_len %u (0x%x)\n",
	    cfg.pmsr_window_len, cfg.pmsr_window_len);
	for (i = 0; i < SET_CH_MAX; i++)
		if (cfg.ch[i].id < 32)
			log("ch%d: signal %u id %u montype %u (%s)\n",
			    i,
			    cfg.ch[i].signal,
			    cfg.ch[i].id,
			    cfg.ch[i].montype,
			    cfg.ch[i].montype == 0 ? "rising" :
			    cfg.ch[i].montype == 1 ? "falling" :
			    cfg.ch[i].montype == 2 ? "high level" :
			    cfg.ch[i].montype == 3 ? "low level" :
			    "unknown");
		else
			log("ch%d: off\n", i);

	log("pause %d\n", cfg.pause ? 1 : 0);
	log("fake %d\n", cfg.fake ? 1 : 0);
	len = p - dbgbuf;
	return simple_read_from_buffer(userbuf, count, f_pos, dbgbuf, len);
}

static ssize_t local_ipi_write(struct file *fp, const char __user *userbuf,
			       size_t count, loff_t *f_pos)
{
	unsigned int *v = PDE_DATA(file_inode(fp));

	if (v == NULL)
		return -EFAULT;

	if (kstrtou32_from_user(userbuf, count, 10, v))
		return -EFAULT;

	return count;
}

const static struct file_operations local_ipi_fops = {
	.owner = THIS_MODULE,
	.read = local_ipi_read,
	.write = local_ipi_write,
};

static struct proc_dir_entry *pmsr_droot;

static int pmsr_procfs_init(void)
{
	int i;
	struct proc_dir_entry *ch[4];

	pmsr_cfg_init();

	pmsr_droot = proc_mkdir("pmsr", NULL);
	if (pmsr_droot) {
		proc_create("state", 0444, pmsr_droot, &local_ipi_fops);
		proc_create_data("speed_mode", 0644, pmsr_droot, &local_ipi_fops,
				 (void *) &(cfg.pmsr_speed_mode));
		proc_create_data("window_len", 0644, pmsr_droot, &local_ipi_fops,
				 (void *) &(cfg.pmsr_window_len));
		proc_create_data("enable", 0644, pmsr_droot, &remote_data_fops,
				 (void *) &(cfg.enable));
		proc_create_data("pause", 0644, pmsr_droot, &remote_data_fops,
				 (void *) &(cfg.pause));
		proc_create_data("output", 0644, pmsr_droot, &remote_data_fops,
				 (void *) &(cfg.output));
		proc_create_data("fake", 0644, pmsr_droot, &remote_data_fops,
				 (void *) &(cfg.fake));

		for (i = 0 ; i < SET_CH_MAX; i++) {
			ch[i] = proc_mkdir(ch_name[i], pmsr_droot);

			if (ch[i]) {
				proc_create_data("signal",
						 0644, ch[i], &local_ipi_fops,
						 (void *)&(cfg.ch[i].signal));
				proc_create_data("id",
						 0644, ch[i], &local_ipi_fops,
						 (void *)&(cfg.ch[i].id));
				proc_create_data("montype",
						 0644, ch[i], &local_ipi_fops,
						 (void *)&(cfg.ch[i].montype));
			}
		}
	}

	return 0;
}

static enum hrtimer_restart pmsr_timer_handle(struct hrtimer *timer)
{
	hrtimer_forward(timer, timer->base->get_time(),
			ns_to_ktime(pmsr_ipi_data.window_len * NSEC_PER_USEC));

	return HRTIMER_RESTART;
}

static int pmsr_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *node;
	void __iomem *base;

	node = of_find_compatible_node(NULL, NULL, PMSR_COMPATIBLE_STRING);
	if (!node) {
		pr_info("failed to get spmtwam_pmsr node\n");
		return -ENOENT;
	}

	base = of_iomap(node, 0);
	if (!base) {
		pr_info("failed to get spmtwam_pmsr base\n");
		return -ENOENT;
	}

	return ret;
}

static const struct of_device_id pmsr_of_ids[] = {
	{.compatible = PMSR_COMPATIBLE_STRING,},
	{}
};

static struct platform_driver pmsr_drv = {
	.probe = pmsr_probe,
	.driver = {
		.name = "pmsr",
		.owner = THIS_MODULE,
		.of_match_table = pmsr_of_ids,
	},
};

static int __init pmsr_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&pmsr_drv);

	/* create debugfs node */
	pmsr_procfs_init();

	/* register ipi for AP2SSPM communication */
	pmsr_ipi_init();

	hrtimer_init(&pmsr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pmsr_timer.function = pmsr_timer_handle;

	return ret;
}

module_init(pmsr_init);

static void __exit pmsr_exit(void)
{
	/* remove debugfs node */
	pmsr_procfs_exit();
	hrtimer_try_to_cancel(&pmsr_timer);

	return platform_driver_unregister(&pmsr_drv);
}

module_exit(pmsr_exit);

MODULE_DESCRIPTION("Mediatek MT68XX pmsr driver");
MODULE_AUTHOR("SHChen <Show-Hong.Chen@mediatek.com>");
MODULE_LICENSE("GPL");

