/*
 * Copyright (C) 2015 MediaTek Inc.
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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <mt-plat/aee.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <asm/uaccess.h>
#include <asm/string.h>
#include <linux/spinlock.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include "mt-plat/mtk_mdm_monitor.h"
#include <mt-plat/mt_boot_common.h>
#include <linux/uidgid.h>
#include <linux/slab.h>

#if defined(CONFIG_MTK_THERMAL_PA_VIA_ATCMD)
#define MTK_TS_PA_THPUT_VIA_ATCMD   (1)
#else
#define MTK_TS_PA_THPUT_VIA_ATCMD   (0)
#endif

#define mtk_mdm_debug_log 0
#define mtk_mdm_dprintk(fmt, args...)   \
do {                                    \
	if (mtk_mdm_debug_log)                \
		pr_debug("[MDM_TxPower/PA_Thermal]" fmt, ##args); \
} while (0)

#define DEFINE_MDM_CB(index)	\
static int fill_mdm_cb_##index(int md_id, int data)	\
{\
	if (data > 500)	\
		data = -127;	\
	g_pinfo_list[index].value = data*1000;	\
	return 0;	\
}

#define MDM_CB(index)	(fill_mdm_cb_##index)

#define MAX_LEN	256
#define ID_REG_TXPOWER_CB	(MD_TX_POWER)
#define ID_REG_RFTEMP_CB	(MD_RF_TEMPERATURE)
#define ID_REG_RFTEMP_3G_CB	(MD_RF_TEMPERATURE_3G)
#define MTK_THERMAL_GET_TX_POWER	0
#define MTK_THERMAL_GET_RF_TEMP_2G	1
#define MTK_THERMAL_GET_RF_TEMP_3G	2

#if defined(CONFIG_MTK_THERMAL_PA_VIA_ATCMD)
#define MAX_MDINFOEX_OPCODE (16)
#endif

#if MTK_TS_PA_THPUT_VIA_ATCMD == 1
static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);
#endif

static bool mdm_sw;
static int mtk_mdm_enable(void);
static int mtk_mdm_disable(void);
static int signal_period = 60;	/* 1s */

#if defined(CONFIG_MTK_THERMAL_PA_VIA_ATCMD)
static int mdinfoex[MAX_MDINFOEX_OPCODE] = { 0 };
static int mdinfoex_threshold[MAX_MDINFOEX_OPCODE] = { 0 };
#endif

struct md_info g_pinfo_list[6] = { {"TXPWR_MD1", -127, "db", -127, 0},
{"TXPWR_MD2", -127, "db", -127, 1},
{"RFTEMP_2G_MD1", -127000, "m¢XC", -127000, 2},
{"RFTEMP_2G_MD2", -127000, "m¢XC", -127000, 3},
{"RFTEMP_3G_MD1", -127000, "m¢XC", -127000, 4},
{"RFTEMP_3G_MD2", -127000, "m¢XC", -127000, 5}
};

int mtk_mdm_get_tx_power(void)
{
	return 0;
}
EXPORT_SYMBOL(mtk_mdm_get_tx_power);

int mtk_mdm_get_rf_temp(void)
{
	return 0;
}
EXPORT_SYMBOL(mtk_mdm_get_rf_temp);

int mtk_mdm_get_md_info(struct md_info **p_inf, int *size)
{
	mtk_mdm_dprintk("mtk_mdm_get_md_info+\n");

	*p_inf = g_pinfo_list;
	*size = sizeof(g_pinfo_list) / sizeof(g_pinfo_list[0]);

	mtk_mdm_dprintk("mtk_mdm_get_md_info-\n");

	return 0;
}
EXPORT_SYMBOL(mtk_mdm_get_md_info);

int mtk_mdm_get_mdinfoex(int opcode, int *value)
{
	mtk_mdm_dprintk("%s\n", __func__);

#if defined(CONFIG_MTK_THERMAL_PA_VIA_ATCMD)
	if (opcode >= 0 && opcode < MAX_MDINFOEX_OPCODE && value != NULL) {
		*value = mdinfoex[opcode];
		return 0;
	} else {
		return -1;
	}
#else
	return -1;
#endif
}
EXPORT_SYMBOL(mtk_mdm_get_mdinfoex);

int mtk_mdm_set_mdinfoex_threshold(int opcode, int threshold)
{
	mtk_mdm_dprintk("%s\n", __func__);

#if defined(CONFIG_MTK_THERMAL_PA_VIA_ATCMD)
	if (opcode >= 0 && opcode < MAX_MDINFOEX_OPCODE) {
		mdinfoex_threshold[opcode] = threshold;
		return 0;
	} else {
		return -1;
	}
#else
	return -1;
#endif
}
EXPORT_SYMBOL(mtk_mdm_set_mdinfoex_threshold);

int mtk_mdm_start_query(void)
{
/* #if  defined(CONFIG_MTK_ENABLE_MD1) || defined(CONFIG_MTK_ENABLE_MD2) */
	mtk_mdm_dprintk("mtk_mdm_start_query\n");

	mdm_sw = true;
	mtk_mdm_enable();
/* #endif */
	return 0;
}
EXPORT_SYMBOL(mtk_mdm_start_query);

int mtk_mdm_stop_query(void)
{
	mtk_mdm_dprintk("mtk_mdm_stop_query\n");

	mdm_sw = false;
	mtk_mdm_disable();
	return 0;
}
EXPORT_SYMBOL(mtk_mdm_stop_query);

int mtk_mdm_set_signal_period(int second)
{
	signal_period = second;
	return 0;
}
EXPORT_SYMBOL(mtk_mdm_set_signal_period);

static int md1_signal_period;
static int md2_signal_period;
static int mdm_signal_period;

static void set_mdm_signal_period(void)
{
	int new_mdm_signal_period = 0;

	if ((0 == md2_signal_period) && (0 == md1_signal_period))
		;
	else if (0 == md2_signal_period)
		new_mdm_signal_period = md1_signal_period;
	else if (0 == md1_signal_period)
		new_mdm_signal_period = md2_signal_period;
	else
		new_mdm_signal_period = (md1_signal_period <= md2_signal_period) ?
			md1_signal_period : md2_signal_period;

	if (new_mdm_signal_period != mdm_signal_period) {
		if (0 == new_mdm_signal_period) {
			mtk_mdm_stop_query();
		} else {
			if (0 == mdm_signal_period) {
				mtk_mdm_set_signal_period(new_mdm_signal_period);
				mtk_mdm_start_query();
			} else {
				mtk_mdm_set_signal_period(new_mdm_signal_period);
			}
		}
		mdm_signal_period = new_mdm_signal_period;
	}
}

int mtk_mdm_set_md1_signal_period(int second)
{
	md1_signal_period = second;
	set_mdm_signal_period();
	return 0;
}
EXPORT_SYMBOL(mtk_mdm_set_md1_signal_period);

int mtk_mdm_set_md2_signal_period(int second)
{
	md2_signal_period = second;
	set_mdm_signal_period();
	return 0;
}
EXPORT_SYMBOL(mtk_mdm_set_md2_signal_period);



static int mtk_mdm_enable(void)
{
	mtk_mdm_dprintk("ENABLE MDM_TxPower Function\n");
	return 0;
}

static int mtk_mdm_disable(void)
{
	mtk_mdm_dprintk("DISABLE MDM_TxPower Function\n");
	return 0;
}

static int mtk_mdm_value_read(struct seq_file *m, void *v)
{
	struct md_info *p_md;
	int size;
	int i;

	mtk_mdm_get_md_info(&p_md, &size);
	seq_printf(m, "%s:%d %s\n", p_md[0].attribute, p_md[0].value, p_md[0].unit);
	for (i = 1; i < size; i++)
		seq_printf(m, "%s:%d %s\n", p_md[i].attribute, p_md[i].value, p_md[i].unit);

	return 0;
}

static int mtk_mdm_value_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_mdm_value_read, NULL);
}

static const struct file_operations mtk_mdm_value_fops = {
	.owner = THIS_MODULE,
	.open = mtk_mdm_value_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int mtk_mdm_sw_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", mdm_sw ? "on" : "off");
	mtk_mdm_dprintk("[%s] %s", __func__, mdm_sw ? "on" : "off");
	return 0;
}

static ssize_t mtk_mdm_sw_write(struct file *file, const char __user *buf, size_t len,
				loff_t *data)
{
	struct mtktsmdm_data {
		char desc[MAX_LEN];
		char temp[MAX_LEN];
	};

	struct mtktsmdm_data *ptr_mtktsmdm_data = kmalloc(sizeof(*ptr_mtktsmdm_data), GFP_KERNEL);

	if (ptr_mtktsmdm_data == NULL)
		return -ENOMEM;

	len = (len < (sizeof(ptr_mtktsmdm_data->desc) - 1)) ? len : (sizeof(ptr_mtktsmdm_data->desc) - 1);

	/* write data to the buffer */
	if (copy_from_user(ptr_mtktsmdm_data->desc, buf, len)) {
		kfree(ptr_mtktsmdm_data);
		return -EFAULT;
	}

	if (sscanf(ptr_mtktsmdm_data->desc, "%255s", ptr_mtktsmdm_data->temp) == 1) {
		if (strncmp(ptr_mtktsmdm_data->temp, "on", 2) == 0 || strncmp(ptr_mtktsmdm_data->temp, "1", 1) == 0)
			mdm_sw = true;
		else if (strncmp(ptr_mtktsmdm_data->temp, "off", 3) == 0 ||
			strncmp(ptr_mtktsmdm_data->temp, "0", 1) == 0)
			mdm_sw = false;
		else
			mtk_mdm_dprintk("[%s] bad argument:%s\n", __func__, ptr_mtktsmdm_data->temp);

		if (mdm_sw)
			mtk_mdm_enable();
		else
			mtk_mdm_disable();


		kfree(ptr_mtktsmdm_data);
		return len;
	}

	mtk_mdm_dprintk("[%s] bad argument\n", __func__);
	kfree(ptr_mtktsmdm_data);
	return -EINVAL;
}

static int mtk_mdm_sw_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_mdm_sw_read, NULL);
}

static const struct file_operations mtk_mdm_sw_fops = {
	.owner = THIS_MODULE,
	.open = mtk_mdm_sw_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtk_mdm_sw_write,
	.release = single_release,
};

static int mtk_mdm_proc_timeout_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", signal_period);
	mtk_mdm_dprintk("[%s] %d", __func__, signal_period);
	return 0;
}

static ssize_t mtk_mdm_proc_timeout_write(struct file *file, const char __user *buf, size_t len,
					  loff_t *data)
{
	char desc[MAX_LEN] = { 0 };
	int temp_value;

	len = (len < (sizeof(desc) - 1)) ? len : (sizeof(desc) - 1);

	/* write data to the buffer */
	if (copy_from_user(desc, buf, len))
		return -EFAULT;

	if (kstrtoint(desc, 10, &temp_value) == 0) {
		signal_period = temp_value;
		mtk_mdm_dprintk("[%s] Set Timeout:%d\n", __func__, temp_value);
		return len;
	}

	mtk_mdm_dprintk("[%s] bad argument\n", __func__);
	return -EINVAL;
}

static int mtk_mdm_proc_timeout_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_mdm_proc_timeout_read, NULL);
}

static const struct file_operations mtk_mdm_proc_timeout_fops = {
	.owner = THIS_MODULE,
	.open = mtk_mdm_proc_timeout_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtk_mdm_proc_timeout_write,
	.release = single_release,
};

#if MTK_TS_PA_THPUT_VIA_ATCMD == 1
static int mtk_mdm_proc_mdinfo_read(struct seq_file *m, void *v)
{
	seq_printf(m, "md 1 T2g %d T3g %d tx %d\nmd 2 T2g %d T3g %d tx %d\n",
		   g_pinfo_list[2].value,
		   g_pinfo_list[4].value,
		   g_pinfo_list[0].value,
		   g_pinfo_list[3].value, g_pinfo_list[5].value, g_pinfo_list[1].value);

	mtk_mdm_dprintk("[%s] %d", __func__, signal_period);
	return 0;
}

static ssize_t mtk_mdm_proc_mdinfo_write(struct file *file, const char *buf, size_t len,
					 loff_t *data)
{
	char desc[MAX_LEN] = { 0 };
	int sim;
	int rat;
	int rf_temp;
	int tx_power;

	len = (len < (sizeof(desc) - 1)) ? len : (sizeof(desc) - 1);

	/* write data to the buffer */
	if (copy_from_user(desc, buf, len))
		return -EFAULT;

	if (sscanf(desc, "%d,%d,%d,%d", &sim, &rat, &rf_temp, &tx_power) >= 3) {
		mtk_mdm_dprintk("[%s] %d,%d,%d,%d\n", __func__, sim, rat, rf_temp, tx_power);

		rf_temp = (rf_temp >= 32767) ? -127 : rf_temp;	/* 32767 means invalid temp */

		/* fill into g_pinfo_list */
		if (0 == sim) {	/* MD1 */
			if (1 == rat) {
				g_pinfo_list[2].value = rf_temp * 1000;
			} else if (2 == rat || 3 == rat) {
				g_pinfo_list[4].value = rf_temp * 1000;
				g_pinfo_list[0].value = tx_power;
			}
		} else if (1 == sim) {	/* MD2 */
			if (1 == rat) {
				g_pinfo_list[3].value = rf_temp * 1000;
			} else if (2 == rat || 3 == rat) {
				g_pinfo_list[5].value = rf_temp * 1000;
				g_pinfo_list[1].value = tx_power;
			}
		}

		return len;
	}

	mtk_mdm_dprintk("[%s] insufficient input %d,%d,%d,%d\n", __func__, sim, rat, rf_temp, tx_power);
	return -EINVAL;
}

static int mtk_mdm_proc_mdinfo_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_mdm_proc_mdinfo_read, NULL);
}

static const struct file_operations mtk_mdm_proc_mdinfo_fops = {
	.owner = THIS_MODULE,
	.open = mtk_mdm_proc_mdinfo_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtk_mdm_proc_mdinfo_write,
	.release = single_release,
};

static int mtk_mdm_proc_mdinfoex_read(struct seq_file *m, void *v)
{
	int i = 0;

	for (; i < MAX_MDINFOEX_OPCODE; i++)
		seq_printf(m, "%03d %d\n", i, mdinfoex[i]);

	mtk_mdm_dprintk("[%s]\n", __func__);
	return 0;
}

static ssize_t mtk_mdm_proc_mdinfoex_write(struct file *file, const char *buf, size_t len,
					   loff_t *data)
{

	char desc[MAX_LEN] = { 0 };
	int opcode;
	int value;

	len = (len < (sizeof(desc) - 1)) ? len : (sizeof(desc) - 1);

	/* write data to the buffer */
	if (copy_from_user(desc, buf, len))
		return -EFAULT;

	if (sscanf(desc, "%d,%d", &opcode, &value) >= 2) {
		mtk_mdm_dprintk("[%s] %d,%d\n", __func__, opcode, value);

		/* fill mdinfoex */
		if (opcode >= 0 && opcode < MAX_MDINFOEX_OPCODE)
			mdinfoex[opcode] = value;
		else
			mtk_mdm_dprintk("[%s] invalid input %d,%d\n", __func__, opcode, value);

		return len;
	}

	mtk_mdm_dprintk("[%s] invalid input %d,%d\n", __func__, opcode, value);
	return -EINVAL;
}

static int mtk_mdm_proc_mdinfoex_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_mdm_proc_mdinfoex_read, NULL);
}

static const struct file_operations mtk_mdm_proc_mdinfoex_fops = {
	.owner = THIS_MODULE,
	.open = mtk_mdm_proc_mdinfoex_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtk_mdm_proc_mdinfoex_write,
	.release = single_release,
};

static int mtk_mdm_proc_mdinfoex_threshold_read(struct seq_file *m, void *v)
{
	int i = 0;

	for (; i < MAX_MDINFOEX_OPCODE; i++)
		seq_printf(m, "%d,", mdinfoex_threshold[i]);

	seq_puts(m, "\n");

	mtk_mdm_dprintk("[%s]\n", __func__);
	return 0;
}

static int mtk_mdm_proc_mdinfoex_threshold_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_mdm_proc_mdinfoex_threshold_read, NULL);
}

static const struct file_operations mtk_mdm_proc_mdinfoex_threshold_fops = {
	.owner = THIS_MODULE,
	.open = mtk_mdm_proc_mdinfoex_threshold_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

static int __init mtk_mdm_txpwr_init(void)
{
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mdtxpwr_dir = NULL;

	mtk_mdm_dprintk("[%s]\n", __func__);

	mdtxpwr_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mdtxpwr_dir) {
		mtk_mdm_dprintk("[%s]: mkdir /driver/thermal failed\n", __func__);
	} else {
		entry =
		    proc_create("mdm_sw", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, mdtxpwr_dir,
				&mtk_mdm_sw_fops);
		entry =
		    proc_create("mdm_value", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, mdtxpwr_dir,
				&mtk_mdm_value_fops);
		entry =
		    proc_create("mdm_timeout", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, mdtxpwr_dir,
				&mtk_mdm_proc_timeout_fops);

#if MTK_TS_PA_THPUT_VIA_ATCMD == 1
		entry =
		    proc_create("mdm_mdinfo", S_IRUGO | S_IWUSR | S_IWGRP, mdtxpwr_dir,
				&mtk_mdm_proc_mdinfo_fops);
		if (entry)
			proc_set_user(entry, uid, gid);

		entry =
		    proc_create("mdm_mdinfoex", S_IRUGO | S_IWUSR | S_IWGRP, mdtxpwr_dir,
				&mtk_mdm_proc_mdinfoex_fops);
		if (entry)
			proc_set_user(entry, uid, gid);

		entry =
		    proc_create("mdm_mdinfoex_thre", S_IRUGO, mdtxpwr_dir,
				&mtk_mdm_proc_mdinfoex_threshold_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
#endif
	}
	/* Add for thermal all on scenary */
	/* mtk_mdm_start_query(); */

	return 0;
}

static void __exit mtk_mdm_txpwr_exit(void)
{
	mtk_mdm_dprintk("[mtk_mdm_txpwr_exit]\n");
}
module_init(mtk_mdm_txpwr_init);
module_exit(mtk_mdm_txpwr_exit);
