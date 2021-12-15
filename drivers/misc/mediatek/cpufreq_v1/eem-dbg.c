// SPDX-License-Identifier: GPL-2.0
/*
 * eem-dbg.c - eem debug Driver
 *
 * Copyright (c) 2020 MediaTek Inc.
 * Chienwei Chang <chienwei.chang@mediatek.com>
 */

/* system includes */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/cpufreq.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include "mtk_cpu_dbg.h"
#include "eem-dbg-v1.h"
#include "../mcupm/include/mcupm_driver.h"
#include "../mcupm/include/mcupm_ipi_id.h"


int ipi_ackdata;
static struct eemsn_log *eemsn_log;
uint32_t eem_log_size;
static int eem_log_en;


void __iomem *eem_csram_base;

static unsigned int eem_to_up(unsigned int cmd,
	struct eemsn_ipi_data *eem_data)
{
	unsigned int ret;

	eem_data->cmd = cmd;
	ret = mtk_ipi_send_compl(get_mcupm_ipidev(), CH_S_EEMSN,
		/*IPI_SEND_WAIT*/IPI_SEND_POLLING, eem_data,
		sizeof(struct eemsn_ipi_data)/MBOX_SLOT_SIZE, 2000);
	return ret;
}


/**
 * ===============================================
 * PROCFS interface for debugging
 * ===============================================
 */
static int eem_debug_proc_show(struct seq_file *m, void *v)
{

	return 0;
}
/*
 * set EEM status by procfs interface
 */
static ssize_t eem_debug_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	int disable = 0;
	char *buf = (char *) __get_free_page(GFP_USER);
	struct eemsn_ipi_data eem_data;
	int bank_id = 0;
	char *cmd_str = NULL;

	if (!buf)
		return -ENOMEM;
	ret = -EINVAL;
	if (count >= PAGE_SIZE)
		goto out;
	ret = -EFAULT;
	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	cmd_str = strsep(&buf, " ");
	if (cmd_str == NULL)
		return -EINVAL;

	if (!kstrtoint(cmd_str, 10, &bank_id))
		if (bank_id >= NR_EEMSN_DET)
			goto out;

	if (buf == NULL)
		return -EINVAL;

	if (!kstrtoint(buf, 10, &disable)) {
		ret = 0;
		if ((disable < 0) || (disable > 1))
			goto out;

		memset(&eem_data, 0, sizeof(struct eemsn_ipi_data));
		eem_data.u.data.arg[0] = bank_id;
		eem_data.u.data.arg[1] = disable;
		eem_to_up(IPI_EEMSN_DEBUG_PROC_WRITE, &eem_data);

	} else
		ret = -EINVAL;
	pr_debug("eem_debug bank_id:%d, eem disable:%d\n", bank_id, disable);
out:

	free_page((unsigned long)buf);
	return (ret < 0) ? ret : count;
}

/*
 * show current aging margin
 */

static int eem_setclamp_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

/*
 * remove aging margin
 */
static ssize_t eem_setclamp_proc_write(struct file *file,
			const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	char *buf = (char *) __get_free_page(GFP_USER);
	int volt_clamp = 0;
	unsigned int  bank_id = 0;
	char *cmd_str = NULL;
	struct eemsn_ipi_data eem_data;

	if (!buf)
		return -ENOMEM;
	ret = -EINVAL;
	if (count >= PAGE_SIZE)
		goto out;
	ret = -EFAULT;
	if (copy_from_user(buf, buffer, count))
		goto out;
	buf[count] = '\0';

	cmd_str = strsep(&buf, " ");
	if (cmd_str == NULL)
		return -EINVAL;

	if (!kstrtoint(cmd_str, 10, &bank_id))
		if (bank_id >= NR_EEMSN_DET)
			goto out;

	if (buf == NULL)
		return -EINVAL;

	if (!kstrtoint(buf, 10, &volt_clamp)) {
		ret = 0;
		if ((volt_clamp < -30) || (volt_clamp > 30))
			goto out;

		memset(&eem_data, 0, sizeof(struct eemsn_ipi_data));
		eem_data.u.data.arg[0] = bank_id;
		eem_data.u.data.arg[1] = volt_clamp;
		eem_to_up(IPI_EEMSN_SETCLAMP_PROC_WRITE, &eem_data);

		pr_debug("set volt_offset %d\n", volt_clamp);
	} else {
		ret = -EINVAL;
		pr_debug("bad argument_1!! argument should be \"0\"\n");
	}
out:
	free_page((unsigned long)buf);
	return (ret < 0) ? ret : count;

}


/*
 * show current voltage
 */
static int eem_cur_volt_proc_show(struct seq_file *m, void *v)
{
	unsigned char lock;
	unsigned int locklimit = 0;
	struct eemsn_ipi_data eem_data;
	unsigned int ipi_ret = 0, bank_id = 0;
	int i;

	/* update volt_tbl_pmic info from mcupm */
	memset(&eem_data, 0, sizeof(struct eemsn_ipi_data));
	ipi_ret = eem_to_up(IPI_EEMSN_GET_EEM_VOLT, &eem_data);

	while (1) {
		lock = eemsn_log->lock;
		locklimit++;
		mdelay(5); /* wait 5 ms */
		/* pr_info("1 lock=0x%X\n", lock); */
		lock = eemsn_log->lock;
		/* pr_info("2 lock=0x%X\n", lock); */
		if ((lock & 0x1) && (locklimit < 5))
			continue; /* if lock, read dram again */
		else
			break;
		/* if unlock, break out while loop, read next det*/
	}

	for (bank_id = 0; bank_id < NR_EEMSN_DET; bank_id++) {
		seq_printf(m, "id:%d, DVFS_TABLE\n", bank_id);
		for (i = 0; i < NR_FREQ; i++) {
			if (eemsn_log->det_log[bank_id].freq_tbl[i] == 0)
				break;
			seq_printf(m, "[%d],freq = [%hu], eem = [%x], pmic = [%x]\n",
			i,
			eemsn_log->det_log[bank_id].freq_tbl[i],
			eemsn_log->det_log[bank_id].volt_tbl_init2[i],
			eemsn_log->det_log[bank_id].volt_tbl_pmic[i]);
		}
	}
	seq_printf(m, "ipi_ret:%d\n", ipi_ret);

	return 0;

}

static int eem_log_en_proc_show(struct seq_file *m, void *v)
{
	struct eemsn_ipi_data eem_data;
	unsigned int ipi_ret = 0;

	memset(&eem_data, 0, sizeof(struct eemsn_ipi_data));
	ipi_ret = eem_to_up(IPI_EEMSN_LOGEN_PROC_SHOW, &eem_data);
	seq_printf(m, "log_en:%d, ipi_ret:%d\n", eemsn_log->eemsn_log_en, ipi_ret);

	return 0;
}

static ssize_t eem_log_en_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	char *buf = (char *) __get_free_page(GFP_USER);
	struct eemsn_ipi_data eem_data;


	if (!buf)
		return -ENOMEM;

	ret = -EINVAL;
	if (count >= PAGE_SIZE)
		goto out;
	ret = -EFAULT;
	if (copy_from_user(buf, buffer, count))
		goto out;
	buf[count] = '\0';
	ret = -EINVAL;
	if (kstrtoint(buf, 10, &eem_log_en))
		goto out;


	ret = 0;
	memset(&eem_data, 0, sizeof(struct eemsn_ipi_data));
	eem_data.u.data.arg[0] = eem_log_en;
	eem_to_up(IPI_EEMSN_LOGEN_PROC_WRITE, &eem_data);
out:
	free_page((unsigned long)buf);

	return (ret < 0) ? ret : count;
}

static int eem_disable_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "eemsn_disable:%d\n", eemsn_log->eemsn_disable);

	return 0;
}

static ssize_t eem_disable_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	char *buf = (char *) __get_free_page(GFP_USER);
	struct eemsn_ipi_data eem_data;
	unsigned int ctrl_EEMSN_disable;

	if (!buf)
		return -ENOMEM;


	ret = -EINVAL;
	if (count >= PAGE_SIZE)
		goto out;
	ret = -EFAULT;
	if (copy_from_user(buf, buffer, count))
		goto out;
	buf[count] = '\0';
	ret = -EINVAL;
	if (kstrtoint(buf, 10, &ctrl_EEMSN_disable))
		goto out;


	ret = 0;
	memset(&eem_data, 0, sizeof(struct eemsn_ipi_data));
	eem_data.u.data.arg[0] = ctrl_EEMSN_disable;

	eem_to_up(IPI_EEMSN_EN_PROC_WRITE, &eem_data);


out:
	free_page((unsigned long)buf);

	return (ret < 0) ? ret : count;
}

static int eem_sn_disable_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "sn_disable:%d\n", eemsn_log->sn_disable);

	return 0;
}

static ssize_t eem_sn_disable_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	char *buf = (char *) __get_free_page(GFP_USER);
	struct eemsn_ipi_data eem_data;
	unsigned int ipi_ret = 0;
	unsigned int ctrl_SN_disable;

	if (!buf)
		return -ENOMEM;

	ret = -EINVAL;
	if (count >= PAGE_SIZE)
		goto out;
	ret = -EFAULT;
	if (copy_from_user(buf, buffer, count))
		goto out;
	buf[count] = '\0';
	ret = -EINVAL;

	if (kstrtoint(buf, 10, &ctrl_SN_disable))
		goto out;



	ret = 0;
	memset(&eem_data, 0, sizeof(struct eemsn_ipi_data));
	eem_data.u.data.arg[0] = ctrl_SN_disable;
	ipi_ret = eem_to_up(IPI_EEMSN_SNEN_PROC_WRITE, &eem_data);


out:
	free_page((unsigned long)buf);

	return (ret < 0) ? ret : count;
}

static int eem_pull_data_proc_show(struct seq_file *m, void *v)
{
	struct eemsn_ipi_data eem_data;
	unsigned char lock;
	unsigned int locklimit = 0;

	memset(&eem_data, 0, sizeof(struct eemsn_ipi_data));
	eem_to_up(IPI_EEMSN_PULL_DATA, &eem_data);


	while (1) {
		lock = eemsn_log->lock;
		locklimit++;
		mdelay(5); /* wait 5 ms */
		/* pr_info("1 lock=0x%X\n", lock); */
		lock = eemsn_log->lock;
		/* pr_info("2 lock=0x%X\n", lock); */
		if ((lock & 0x1) && (locklimit < 5))
			continue; /* if lock, read dram again */
		else
			break;
		/* if unlock, break out while loop, read next det*/
	}


	return 0;
}


static int eem_offset_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static ssize_t eem_offset_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	char *buf = (char *) __get_free_page(GFP_USER);
	int offset = 0;
	unsigned int ipi_ret = 0, bank_id = 0;
	char *cmd_str = NULL;
	struct eemsn_ipi_data eem_data;

	if (!buf)
		return -ENOMEM;
	ret = -EINVAL;
	if (count >= PAGE_SIZE)
		goto out;
	ret = -EFAULT;
	if (copy_from_user(buf, buffer, count))
		goto out;
	buf[count] = '\0';

	cmd_str = strsep(&buf, " ");
	if (cmd_str == NULL)
		return -EINVAL;

	if (!kstrtoint(cmd_str, 10, &bank_id))
		if (bank_id >= NR_EEMSN_DET)
			goto out;

	if (buf == NULL)
		return -EINVAL;

	if (!kstrtoint(buf, 10, &offset)) {
		ret = 0;
		if ((offset < -30) || (offset > 30))
			goto out;

		memset(&eem_data, 0, sizeof(struct eemsn_ipi_data));
		eem_data.u.data.arg[0] = bank_id;
		eem_data.u.data.arg[1] = offset;
		ipi_ret = eem_to_up(IPI_EEMSN_OFFSET_PROC_WRITE, &eem_data);
		/* to show in eem_offset_proc_show */

		pr_debug("set volt_offset %d\n", offset);
	} else {
		ret = -EINVAL;
		pr_debug("bad argument_1!! argument should be \"0\"\n");
	}
out:
	free_page((unsigned long)buf);
	return (ret < 0) ? ret : count;
}

static int eem_dbg_repo_proc_show(struct seq_file *m, void *v)
{
	void __iomem *addr_ptr;
	int counter = 0;
	struct eemsn_ipi_data eem_data;
	unsigned int ipi_ret = 0;
	unsigned char lock;
	unsigned int locklimit = 0;


	memset(&eem_data, 0, sizeof(struct eemsn_ipi_data));
	ipi_ret = eem_to_up(IPI_EEMSN_PULL_DATA, &eem_data);

	while (1) {
		lock = eemsn_log->lock;
		locklimit++;
		mdelay(5); /* wait 5 ms */
		/* pr_info("1 lock=0x%X\n", lock); */
		lock = eemsn_log->lock;
		/* pr_info("2 lock=0x%X\n", lock); */
		if ((lock & 0x1) && (locklimit < 5))
			continue; /* if lock, read dram again */
		else
			break;
		/* if unlock, break out while loop, read next det*/
	}

	if ((void __iomem *)(eem_csram_base) != NULL) {
		for (addr_ptr = (void __iomem *)(eem_csram_base)
			, counter = 0; counter <
			eem_log_size;
			(addr_ptr += 4), counter += 4)
			seq_printf(m, "%08X",
				(unsigned int)__raw_readl(addr_ptr));
	}
	return 0;
}

PROC_FOPS_RO(eem_dbg_repo);
PROC_FOPS_RW(eem_debug);
PROC_FOPS_RO(eem_cur_volt);
PROC_FOPS_RW(eem_offset);
PROC_FOPS_RW(eem_log_en);
PROC_FOPS_RW(eem_disable);
PROC_FOPS_RW(eem_sn_disable);
PROC_FOPS_RO(eem_pull_data);
PROC_FOPS_RW(eem_setclamp);

static int create_debug_fs(void)
{
	int i;
	struct proc_dir_entry *eem_dir = NULL;

	struct pentry {
		const char *name;
		const struct proc_ops *fops;
		void *data;
	};


	struct pentry eem_entries[] = {
		PROC_ENTRY(eem_debug),
		PROC_ENTRY(eem_cur_volt),
		PROC_ENTRY(eem_offset),
		PROC_ENTRY(eem_setclamp),
		PROC_ENTRY(eem_log_en),
		PROC_ENTRY(eem_disable),
		PROC_ENTRY(eem_sn_disable),
		PROC_ENTRY(eem_pull_data),
		PROC_ENTRY(eem_dbg_repo),
	};

	eem_dir = proc_mkdir("eem", NULL);
	for (i = 0; i < ARRAY_SIZE(eem_entries); i++) {
		if (!proc_create(eem_entries[i].name, 0664,
					eem_dir, eem_entries[i].fops)) {
			pr_info("[%s]: create /proc/eem/%s failed\n",
					__func__,
					eem_entries[i].name);
		}
	}

	return 0;
}

int mtk_eem_init(struct platform_device *pdev)
{
	int err = 0;
	struct resource *eem_res;

	eem_res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (eem_res) {
		eem_log_size = resource_size(eem_res);
		eemsn_log = ioremap(eem_res->start, resource_size(eem_res));
	} else
		eemsn_log = ioremap(EEM_LOG_BASE, EEM_LOG_SIZE);

	eem_csram_base = eemsn_log;

	err = mtk_ipi_register(get_mcupm_ipidev(), CH_S_EEMSN, NULL, NULL,
		(void *)&ipi_ackdata);
	if (err != 0) {
		pr_info("%s error ret:%d\n", __func__, err);
		return 0;
	}

	return create_debug_fs();
}

MODULE_DESCRIPTION("MTK CPU DVFS Platform Driver v0.1.1");
MODULE_AUTHOR("Chienwei Chang <chiewei.chang@mediatek.com>");
MODULE_LICENSE("GPL v2");

