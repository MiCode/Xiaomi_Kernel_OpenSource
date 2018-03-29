/*
 * Copyright (C) 2016 MediaTek Inc.
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

#define pr_fmt(fmt) "["KBUILD_MODNAME"]" fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <mach/emi_mpu.h>
#include <mt-plat/mtk_meminfo.h>
#include "mtkdcs_drv.h"
#include <mt_spm_vcorefs.h>
#include <mt_vcorefs_manager.h>

static enum dcs_status sys_dcs_status = DCS_NORMAL;
static bool dcs_initialized;
static int normal_channel_num;
static int lowpower_channel_num;
static struct rw_semaphore dcs_rwsem;

static char * const __dcs_status_name[DCS_NR_STATUS] = {
	"normal",
	"low power",
	"busy",
};

enum dcs_sysfs_mode {
	DCS_SYSFS_MODE_START,
	DCS_SYSFS_ALWAYS_NORMAL = DCS_SYSFS_MODE_START,
	DCS_SYSFS_ALWAYS_LOWPOWER,
	DCS_SYSFS_FREERUN,
	DCS_SYSFS_FREERUN_NORMAL,
	DCS_SYSFS_FREERUN_LOWPOWER,
	DCS_SYSFS_NR_MODE,
};

enum dcs_sysfs_mode dcs_sysfs_mode = DCS_SYSFS_FREERUN;

static char * const dcs_sysfs_mode_name[DCS_SYSFS_NR_MODE] = {
	"always normal",
	"always lowpower",
	"freerun only",
	"freerun normal",
	"freerun lowpower",
};

/*
 * dcs_status_name
 * return the status name for the given dcs status
 * @status: the dcs status
 *
 * return the pointer of the name or NULL for invalid status
 */
char * const dcs_status_name(enum dcs_status status)
{
	if (status < DCS_NR_STATUS)
		return __dcs_status_name[status];
	else
		return NULL;
}

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#include "sspm_ipi.h"
static unsigned int dcs_recv_data[4];

static int dcs_get_status_ipi(enum dcs_status *sys_dcs_status)
{
	int ipi_data_ret = 0, err;
	unsigned int ipi_buf[32];

	ipi_buf[0] = IPI_DCS_GET_MODE;

	err = sspm_ipi_send_sync(IPI_ID_DCS, 1, (void *)ipi_buf, 0, &ipi_data_ret);

	if (err) {
		pr_err("[%s:%d]ipi_write error: %d\n", __func__, __LINE__, err);
		return -EBUSY;
	}

	*sys_dcs_status = (ipi_data_ret) ? DCS_LOWPOWER : DCS_LOWPOWER;

	return 0;
}

static int dcs_migration_ipi(enum migrate_dir dir)
{
	int ipi_data_ret = 0, err;
	unsigned int ipi_buf[32];

	ipi_buf[0] = IPI_DCS_MIGRATION;
	ipi_buf[1] = dir;

	err = sspm_ipi_send_sync(IPI_ID_DCS, 1, (void *)ipi_buf, 0, &ipi_data_ret);

	if (err) {
		pr_err("[%s:%d]ipi_write error: %d\n", __func__, __LINE__, err);
		return -EBUSY;
	}

	return 0;
}

static int dcs_set_dummy_write_ipi(void)
{
	int ipi_data_ret = 0, err;
	unsigned int ipi_buf[32];

	ipi_buf[0] = IPI_DCS_SET_DUMMY_WRITE;
	ipi_buf[1] = 0;
	ipi_buf[2] = 0x200000;
	ipi_buf[3] = 0x000000;
	ipi_buf[4] = 0x300000;
	ipi_buf[5] = 0x000000;

	err = sspm_ipi_send_sync(IPI_ID_DCS, 1, (void *)ipi_buf, 0, &ipi_data_ret);

	if (err) {
		pr_err("[%s:%d]ipi_write error: %d\n", __func__, __LINE__, err);
		return -EBUSY;
	}

	return 0;
}

static int dcs_dump_reg_ipi(void)
{
	int ipi_data_ret = 0, err;
	unsigned int ipi_buf[32];

	ipi_buf[0] = IPI_DCS_DUMP_REG;

	err = sspm_ipi_send_sync(IPI_ID_DCS, 1, (void *)ipi_buf, 0, &ipi_data_ret);

	if (err) {
		pr_err("[%s:%d]ipi_write error: %d\n", __func__, __LINE__, err);
		return -EBUSY;
	}

	return 0;
}

static int dcs_ipi_register(void)
{
	int ret;
	int retry = 0;
	struct ipi_action dcs_isr;

	dcs_isr.data = (void *)dcs_recv_data;

	do {
		ret = sspm_ipi_recv_registration(IPI_ID_DCS, &dcs_isr);
	} while ((ret != IPI_REG_OK) && (retry++ < 10));

	if (ret != IPI_REG_OK) {
		pr_err("dcs_ipi_register fail\n");
		return -EBUSY;
	}

	return 0;
}

/*
 * __dcs_dram_channel_switch
 *
 * Do the channel switch operation
 * Callers must hold write semaphore: dcs_rwsem
 *
 * return 0 on success or error code
 */
static int __dcs_dram_channel_switch(enum dcs_status status)
{
	if ((sys_dcs_status < DCS_BUSY) &&
		(status < DCS_BUSY) &&
		(sys_dcs_status != status)) {
		/* speed up lpdma, use max DRAM frequency */
		vcorefs_request_dvfs_opp(KIR_DCS, OPP_0);
		dcs_migration_ipi(status == DCS_NORMAL ? NORMAL : LOWPWR);
		/* release DRAM frequency */
		vcorefs_request_dvfs_opp(KIR_DCS, OPP_UNREQ);
		/* update DVFSRC setting */
		spm_dvfsrc_set_channel_bw(status == DCS_NORMAL ?
				DVFSRC_CHANNEL_4 : DVFSRC_CHANNEL_2);
		sys_dcs_status = status;
		pr_info("sys_dcs_status=%s\n", dcs_status_name(sys_dcs_status));
	} else {
		pr_info("sys_dcs_status not changed\n");
	}

	return 0;
}
#else /* !CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
static int dcs_get_status_ipi(enum dcs_status *sys_dcs_status)
{
	*sys_dcs_status = DCS_LOWPOWER;
	return 0;
}
static int dcs_set_dummy_write_ipi(void) { return 0; }
static int dcs_dump_reg_ipi(void) { return 0; }
static int dcs_ipi_register(void) { return 0; }
static int __dcs_dram_channel_switch(enum dcs_status status) { return 0; }
#endif /* end of CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

/*
 * dcs_dram_channel_switch
 *
 * Send a IPI call to SSPM to perform dynamic channel switch.
 * The dynamic channel switch only performed in stable status.
 * i.e., DCS_NORMAL or DCS_LOWPOWER.
 * @status: channel mode
 *
 * return 0 on success or error code
 */
int dcs_dram_channel_switch(enum dcs_status status)
{
	int ret = 0;

	if (!dcs_initialized)
		return -ENODEV;

	down_write(&dcs_rwsem);

	if (dcs_sysfs_mode == DCS_SYSFS_FREERUN)
		ret = __dcs_dram_channel_switch(status);

	up_write(&dcs_rwsem);

	return ret;
}

/*
 * dcs_dram_channel_switch_by_sysfs_mode
 *
 * Update dcs_sysfs_mode and send a IPI call to SSPM to perform
 * dynamic channel switch by the sysfs mode.
 * The dynamic channel switch only performed in stable status.
 * i.e., DCS_NORMAL or DCS_LOWPOWER.
 * @mode: sysfs mode
 *
 * return 0 on success or error code
 */
static int dcs_dram_channel_switch_by_sysfs_mode(enum dcs_sysfs_mode mode)
{
	int ret = 0;

	if (!dcs_initialized)
		return -ENODEV;

	down_write(&dcs_rwsem);

	dcs_sysfs_mode = mode;
	switch (mode) {
	case DCS_SYSFS_FREERUN_NORMAL:
		dcs_sysfs_mode = DCS_SYSFS_FREERUN;
		/* fallthrough */
	case DCS_SYSFS_ALWAYS_NORMAL:
		ret = __dcs_dram_channel_switch(DCS_NORMAL);
		break;
	case DCS_SYSFS_FREERUN_LOWPOWER:
		dcs_sysfs_mode = DCS_SYSFS_FREERUN;
		/* fallthrough */
	case DCS_SYSFS_ALWAYS_LOWPOWER:
		ret = __dcs_dram_channel_switch(DCS_LOWPOWER);
		break;
	default:
		pr_alert("unknown sysfs mode: %d\n", mode);
		break;
	}

	up_write(&dcs_rwsem);

	return ret;
}

/*
 * dcs_get_dcs_status_lock
 * return the number of DRAM channels and status and get the dcs lock
 * @ch: address storing the number of DRAM channels.
 * @dcs_status: address storing the system dcs status
 *
 * return 0 on success or error code
 */
int dcs_get_dcs_status_lock(int *ch, enum dcs_status *dcs_status)
{
	if (!dcs_initialized)
		return -ENODEV;

	down_read(&dcs_rwsem);

	*dcs_status = sys_dcs_status;

	switch (sys_dcs_status) {
	case DCS_NORMAL:
		*ch = normal_channel_num;
		break;
	case DCS_LOWPOWER:
		*ch = lowpower_channel_num;
		break;
	default:
		pr_err("%s:%d, incorrect DCS status=%s\n",
				__func__,
				__LINE__,
				dcs_status_name(sys_dcs_status));
		goto BUSY;
	}
	return 0;
BUSY:
	*ch = -1;
	return -EBUSY;
}

/*
 * dcs_get_dcs_status_trylock
 * return the number of DRAM channels and status and get the dcs lock
 * @ch: address storing the number of DRAM channels, -1 if lock failed
 * @dcs_status: address storing the system dcs status, DCS_BUSY if lock failed
 *
 * return 0 on success or error code
 */
int dcs_get_dcs_status_trylock(int *ch, enum dcs_status *dcs_status)
{
	if (!dcs_initialized)
		return -ENODEV;

	if (!down_read_trylock(&dcs_rwsem)) {
		/* lock failed */
		*dcs_status = DCS_BUSY;
		goto BUSY;
	}

	*dcs_status = sys_dcs_status;

	switch (sys_dcs_status) {
	case DCS_NORMAL:
		*ch = normal_channel_num;
		break;
	case DCS_LOWPOWER:
		*ch = lowpower_channel_num;
		break;
	default:
		pr_err("%s:%d, incorrect DCS status=%s\n",
				__func__,
				__LINE__,
				dcs_status_name(sys_dcs_status));
		goto BUSY;
	}
	return 0;
BUSY:
	*ch = -1;
	return -EBUSY;
}

/*
 * dcs_get_dcs_status_unlock
 * unlock the dcs lock
 */
void dcs_get_dcs_status_unlock(void)
{
	if (!dcs_initialized)
		return;

	up_read(&dcs_rwsem);
}

/*
 * dcs_initialied
 *
 * return true if dcs is initialized
 */
bool dcs_initialied(void)
{
	return dcs_initialized;
}

static ssize_t mtkdcs_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	enum dcs_status dcs_status;
	int n = 0, ch, ret;

	ret = dcs_get_dcs_status_lock(&ch, &dcs_status);

	if (!ret) {
		/*
		 * we're holding the rw_sem, so it's safe to use
		 * dcs_sysfs_mode
		 */
		n = sprintf(buf, "dcs_status=%s, channel=%d, dcs_sysfs_mode=%s\n",
				dcs_status_name(dcs_status),
				ch,
				dcs_sysfs_mode_name[dcs_sysfs_mode]);
		dcs_get_dcs_status_unlock();
	}

	/* call debug ipi */
	dcs_set_dummy_write_ipi();
	dcs_dump_reg_ipi();

	return n;
}

static ssize_t mtkdcs_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	enum dcs_sysfs_mode mode;
	int n = 0;

	n += sprintf(buf + n, "available modes:\n");
	for (mode = DCS_SYSFS_MODE_START; mode < DCS_SYSFS_NR_MODE; mode++)
		n += sprintf(buf + n, "%s\n", dcs_sysfs_mode_name[mode]);

	return n;
}

static ssize_t mtkdcs_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t n)
{
	enum dcs_sysfs_mode mode;
	char *name;

	for (mode = DCS_SYSFS_MODE_START; mode < DCS_SYSFS_NR_MODE; mode++) {
		name = dcs_sysfs_mode_name[mode];
		if (!strncmp(buf, name, strlen(name)))
			goto apply_mode;
	}

	pr_alert("%s:unknown command: %s\n", __func__, buf);
	return n;

apply_mode:

	pr_info("mtkdcs_mode_store cmd=%s", buf);
	dcs_dram_channel_switch_by_sysfs_mode(mode);
	return n;
}

static DEVICE_ATTR(status, S_IRUGO, mtkdcs_status_show, NULL);
static DEVICE_ATTR(mode, S_IRUGO | S_IWUSR, mtkdcs_mode_show, mtkdcs_mode_store);

static struct attribute *mtkdcs_attrs[] = {
	&dev_attr_status.attr,
	&dev_attr_mode.attr,
	NULL,
};

struct attribute_group mtkdcs_attr_group = {
	.attrs = mtkdcs_attrs,
	.name = "mtkdcs",
};

static int __init mtkdcs_init(void)
{
	int ret;

	/* init rwsem */
	init_rwsem(&dcs_rwsem);

	/* register IPI */
	ret = dcs_ipi_register();
	if (ret)
		return -EBUSY;

	/* read system dcs status */
	ret = dcs_get_status_ipi(&sys_dcs_status);
	if (!ret)
		pr_info("get init dcs status: %s\n",
			dcs_status_name(sys_dcs_status));
	else
		return ret;

	/* Create SYSFS interface */
	ret = sysfs_create_group(power_kobj, &mtkdcs_attr_group);
	if (ret)
		return ret;

	/* read number of dram channels */
	normal_channel_num = get_emi_channel_number();

	/* the channel number must be multiple of 2 */
	if (normal_channel_num % 2) {
		pr_err("%s fail, incorrect normal channel num=%d\n",
				__func__, normal_channel_num);
		return -EINVAL;
	}

	lowpower_channel_num = (normal_channel_num / 2);

	dcs_initialized = true;

	return 0;
}

static void __exit mtkdcs_exit(void) { }

late_initcall(mtkdcs_init);
module_exit(mtkdcs_exit);
