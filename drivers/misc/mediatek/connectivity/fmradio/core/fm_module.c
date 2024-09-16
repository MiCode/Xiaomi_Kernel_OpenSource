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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/delay.h>	/* udelay() */
#include <linux/version.h>

#include "fm_config.h"
#include "fm_main.h"
#include "fm_ioctl.h"

#define FM_PROC_FILE		"fm"

unsigned int g_dbg_level = 0xfffffff5;	/* Debug level of FM */

/* fm main data structure */
static struct fm *g_fm;
/* proc file entry */
static struct proc_dir_entry *g_fm_proc;

/* char device interface */
static signed int fm_cdev_setup(struct fm *fm);
static signed int fm_cdev_destroy(struct fm *fm);

static long fm_ops_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
#ifdef CONFIG_COMPAT
static long fm_ops_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
#endif
static loff_t fm_ops_lseek(struct file *filp, loff_t off, signed int whence);
static ssize_t fm_ops_read(struct file *filp, char *buf, size_t len, loff_t *off);
static signed int fm_ops_open(struct inode *inode, struct file *filp);
static signed int fm_ops_release(struct inode *inode, struct file *filp);
static signed int fm_ops_flush(struct file *filp, fl_owner_t Id);
static const struct file_operations fm_ops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = fm_ops_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = fm_ops_compat_ioctl,
#endif
	.llseek = fm_ops_lseek,
	.read = fm_ops_read,
	.open = fm_ops_open,
	.release = fm_ops_release,
	.flush = fm_ops_flush,
};

/* static signed int fm_proc_read(char *page, char **start, off_t off, signed int count,
 *				signed int *eof, void *data);
 */
static ssize_t fm_proc_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t fm_proc_write(struct file *file, const char *buffer, size_t count, loff_t *ppos);

static const struct file_operations fm_proc_ops = {
	.owner = THIS_MODULE,
	.read = fm_proc_read,
	.write = fm_proc_write,
};

#ifdef CONFIG_COMPAT
static long fm_ops_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret;

	WCN_DBG(FM_NTC | MAIN, "COMPAT %s---pid(%d)---cmd(0x%08x)---arg(0x%08x)\n", current->comm,
		current->pid, cmd, (unsigned int) arg);

	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;
	switch (cmd) {
	case COMPAT_FM_IOCTL_GET_AUDIO_INFO: {
		ret = filp->f_op->unlocked_ioctl(filp, FM_IOCTL_GET_AUDIO_INFO, arg);
		break;
		}
	default:
		ret = filp->f_op->unlocked_ioctl(filp, ((cmd & 0xFF) | (FM_IOCTL_POWERUP & 0xFFFFFF00)), arg);
		break;
	}
	return ret;
}
#endif

static long fm_ops_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	signed int ret = 0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	struct fm_platform *plat = container_of(filp->f_path.dentry->d_inode->i_cdev, struct fm_platform, cdev);
#else
	struct fm_platform *plat = container_of(filp->f_dentry->d_inode->i_cdev, struct fm_platform, cdev);
#endif
	struct fm *fm = container_of(plat, struct fm, platform);

	WCN_DBG(FM_INF | MAIN, "%s---pid(%d)---cmd(0x%08x)---arg(0x%08x)\n", current->comm,
		current->pid, cmd, (unsigned int) arg);

	if (fm_sys_state_get(fm) != FM_SUBSYS_RST_OFF) {
		WCN_DBG(FM_ALT | MAIN, "FM subsys is resetting, retry later\n");
		ret = -FM_ESRST;
		return ret;
	}

	switch (cmd) {
	case FM_IOCTL_POWERUP:{
			struct fm_tune_parm parm;

			WCN_DBG(FM_NTC | MAIN, "FM_IOCTL_POWERUP:0\n");
			if (copy_from_user(&parm, (void *)arg, sizeof(struct fm_tune_parm))) {
				ret = -EFAULT;
				goto out;
			}

			if (parm.freq <= 0 || parm.freq > FM_FREQ_MAX) {
				struct fm_tune_parm_old *old_parm =
					(struct fm_tune_parm_old *)&parm;

				if (old_parm->freq > 0 && old_parm->freq <= FM_FREQ_MAX) {
					WCN_DBG(FM_WAR | MAIN,
						"convert to old version fm_tune_parm [%u]->[%u]\n",
						parm.freq, old_parm->freq);
					parm.freq = old_parm->freq;
					parm.deemphasis = 0;
				}
			}

			if (parm.deemphasis == 1)
				fm_config.rx_cfg.deemphasis = 1;
			else
				fm_config.rx_cfg.deemphasis = 0;

			ret = fm_powerup(fm, &parm);
			if (ret < 0) {
				WCN_DBG(FM_NTC | MAIN, "FM_IOCTL_POWERUP:fail in fm_powerup, return\n");
				goto out;
			}
			ret = fm_tune(fm, &parm);
			if (ret < 0) {
				WCN_DBG(FM_NTC | MAIN, "FM_IOCTL_POWERUP:fail in fm_tune, return\n");
				goto out;
			}

			if (copy_to_user((void *)arg, &parm, sizeof(struct fm_tune_parm))) {
				ret = -EFAULT;
				goto out;
			}
			WCN_DBG(FM_NTC | MAIN, "FM_IOCTL_POWERUP:1\n");

			break;
		}

	case FM_IOCTL_POWERDOWN:{
			int powerdwn_type = 0;

			WCN_DBG(FM_NTC | MAIN, "FM_IOCTL_POWERDOWN:0\n");

			if (copy_from_user(&powerdwn_type, (void *)arg, sizeof(int))) {
				ret = -EFAULT;
				goto out;
			}

			ret = fm_powerdown(fm, powerdwn_type);	/* 0: RX 1: TX */
			WCN_DBG(FM_NTC | MAIN, "FM_IOCTL_POWERDOWN:1\n");
			break;
		}

	case FM_IOCTL_TUNE:{
			struct fm_tune_parm parm;

			WCN_DBG(FM_NTC | MAIN, "FM_IOCTL_TUNE:0\n");

			if (copy_from_user(&parm, (void *)arg, sizeof(struct fm_tune_parm))) {
				ret = -EFAULT;
				goto out;
			}

			if (parm.freq <= 0 || parm.freq > FM_FREQ_MAX) {
				struct fm_tune_parm_old *old_parm =
					(struct fm_tune_parm_old *)&parm;

				if (old_parm->freq > 0 && old_parm->freq <= FM_FREQ_MAX) {
					WCN_DBG(FM_WAR | MAIN,
						"convert to old version fm_tune_parm [%u]->[%u]\n",
						parm.freq, old_parm->freq);
					parm.freq = old_parm->freq;
					parm.deemphasis = 0;
				}
			}

			ret = fm_tune(fm, &parm);
			if (ret < 0)
				goto out;

			if (copy_to_user((void *)arg, &parm, sizeof(struct fm_tune_parm))) {
				ret = -EFAULT;
				goto out;
			}

			WCN_DBG(FM_NTC | MAIN, "FM_IOCTL_TUNE:1\n");
			break;
		}

	case FM_IOCTL_SOFT_MUTE_TUNE:
		{
			struct fm_softmute_tune_t parm;

			fm_cqi_log();	/* cqi log tool */
			if (copy_from_user(&parm, (void *)arg, sizeof(struct fm_softmute_tune_t))) {
				ret = -EFAULT;
				goto out;
			}

			ret = fm_soft_mute_tune(fm, &parm);
			if (ret < 0)
				goto out;

			if (copy_to_user((void *)arg, &parm, sizeof(struct fm_softmute_tune_t))) {
				ret = -EFAULT;
				goto out;
			}
			break;
		}
	case FM_IOCTL_PRE_SEARCH:
		{
			ret = fm_pre_search(fm);
			break;
		}
	case FM_IOCTL_RESTORE_SEARCH:
		{
			ret = fm_restore_search(fm);
			break;
		}
	case FM_IOCTL_CQI_GET:{
			struct fm_cqi_req cqi_req;
			signed char *buf = NULL;
			signed int tmp;
			unsigned int cpy_size = 0;

			WCN_DBG(FM_INF | MAIN, "FM_IOCTL_CQI_GET\n");

			if (copy_from_user(&cqi_req, (void *)arg, sizeof(struct fm_cqi_req))) {
				WCN_DBG(FM_ALT | MAIN, "copy_from_user failed\n");
				ret = -EFAULT;
				goto out;
			}

			if ((cqi_req.ch_num * sizeof(struct fm_cqi) > cqi_req.buf_size)
			    || !cqi_req.cqi_buf) {
				ret = -FM_EPARA;
				goto out;
			}

			tmp = cqi_req.ch_num / 16 + ((cqi_req.ch_num % 16) ? 1 : 0);
			tmp = tmp * 16 * sizeof(struct fm_cqi);
			buf = fm_zalloc(tmp);

			if (!buf) {
				ret = -FM_ENOMEM;
				goto out;
			}

			ret = fm_cqi_get(fm, cqi_req.ch_num, buf, tmp);

			if (ret) {
				fm_free(buf);
				WCN_DBG(FM_ALT | MAIN, "get cqi failed\n");
				goto out;
			}

			cpy_size = cqi_req.ch_num * sizeof(struct fm_cqi);
			if (cpy_size > tmp)
				cpy_size = tmp;
			if (copy_to_user((void *)cqi_req.cqi_buf, buf, cpy_size)) {
				fm_free(buf);
				ret = -EFAULT;
				goto out;
			}

			fm_free(buf);
			break;
		}

	case FM_IOCTL_GET_HW_INFO:{
			struct fm_hw_info info;

			WCN_DBG(FM_NTC | MAIN, "FM_IOCTL_GET_HW_INFO\n");

			ret = fm_get_hw_info(fm, &info);

			if (ret) {
				WCN_DBG(FM_ALT | MAIN, "get hw info failed\n");
				goto out;
			}

			if (copy_to_user((void *)arg, &info, sizeof(struct fm_hw_info))) {
				ret = -EFAULT;
				goto out;
			}

			break;
		}

	case FM_IOCTL_GET_I2S_INFO:{
			struct fm_i2s_info i2sinfo;

			WCN_DBG(FM_NTC | MAIN, "FM_IOCTL_GET_I2S_INFO\n");

			ret = fm_get_i2s_info(fm, &i2sinfo);

			if (ret) {
				WCN_DBG(FM_ALT | MAIN, "get i2s info failed\n");
				goto out;
			}

			if (copy_to_user((void *)arg, &i2sinfo, sizeof(struct fm_i2s_info))) {
				ret = -EFAULT;
				goto out;
			}

			break;
		}

	case FM_IOCTL_SETVOL:{
			unsigned int vol;

			WCN_DBG(FM_NTC | MAIN, "FM_IOCTL_SETVOL start\n");
			if (copy_from_user(&vol, (void *)arg, sizeof(unsigned int))) {
				WCN_DBG(FM_ALT | MAIN, "copy_from_user failed\n");
				ret = -EFAULT;
				goto out;
			}

			ret = fm_setvol(fm, vol);
			WCN_DBG(FM_NTC | MAIN, "FM_IOCTL_SETVOL end:%d\n", vol);
			break;
		}
	case FM_IOCTL_GETVOL:{
			unsigned int vol;

			WCN_DBG(FM_NTC | MAIN, "FM_IOCTL_GETVOL start\n");
			ret = fm_getvol(fm, &vol);
			if (ret < 0)
				goto out;

			if (copy_to_user((void *)arg, &vol, sizeof(unsigned int))) {
				ret = -EFAULT;
				goto out;
			}

			WCN_DBG(FM_NTC | MAIN, "FM_IOCTL_GETVOL end=%d\n", vol);
			break;
		}

	case FM_IOCTL_MUTE:{
			unsigned int bmute;

			if (copy_from_user(&bmute, (void *)arg, sizeof(unsigned int))) {
				ret = -EFAULT;
				goto out;
			}

			ret = fm_mute(fm, bmute);
			WCN_DBG(FM_INF | MAIN, "FM_IOCTL_MUTE end-%d\n", bmute);
			break;
		}

	case FM_IOCTL_GETRSSI:{
			signed int rssi = 0;

			ret = fm_getrssi(fm, &rssi);
			if (ret < 0)
				goto out;

			if (copy_to_user((void *)arg, &rssi, sizeof(signed int))) {
				ret = -EFAULT;
				goto out;
			}

			WCN_DBG(FM_NTC | MAIN, "FM_IOCTL_GETRSSI:%d\n", rssi);
			break;
		}

	case FM_IOCTL_RW_REG:{
			struct fm_ctl_parm parm_ctl;

			WCN_DBG(FM_INF | MAIN, "FM_IOCTL_RW_REG\n");
			if (fm->chipon == false || fm_pwr_state_get(fm) == FM_PWR_OFF) {
				WCN_DBG(FM_ERR | MAIN, "ERROR, FM chip is OFF\n");
				ret = -EFAULT;
				goto out;
			}

			if (copy_from_user(&parm_ctl, (void *)arg, sizeof(struct fm_ctl_parm))) {
				WCN_DBG(FM_ALT | MAIN, "copy from user error\n");
				ret = -EFAULT;
				goto out;
			}

			if (parm_ctl.rw_flag == 0)
				ret = fm_write(fm, parm_ctl.addr, parm_ctl.val);
			else
				ret = fm_read(fm, parm_ctl.addr, &parm_ctl.val);

			if (ret < 0)
				goto out;

			if ((parm_ctl.rw_flag == 0x01) && (!ret)) {
				if (copy_to_user((void *)arg, &parm_ctl, sizeof(struct fm_ctl_parm))) {
					ret = -EFAULT;
					goto out;
				}
			}

			break;
		}
	case FM_IOCTL_TOP_RDWR:
		{
			struct fm_top_rw_parm parm_ctl;

			WCN_DBG(FM_INF | MAIN, "FM_IOCTL_TOP_RDWR\n");

#ifdef CONFIG_MTK_USER_BUILD
			WCN_DBG(FM_ERR | MAIN, "Not support FM_IOCTL_TOP_RDWR\n");
			ret = -EFAULT;
			goto out;
#endif

			if (g_dbg_level != 0xfffffff7) {
				WCN_DBG(FM_ERR | MAIN, "Not support FM_IOCTL_TOP_RDWR\n");
				ret = -EFAULT;
				goto out;
			}

			if (fm->chipon == false || fm_pwr_state_get(fm) == FM_PWR_OFF) {
				WCN_DBG(FM_ERR | MAIN, "ERROR, FM chip is OFF\n");
				ret = -EFAULT;
				goto out;
			}

			if (copy_from_user(&parm_ctl, (void *)arg, sizeof(struct fm_top_rw_parm))) {
				WCN_DBG(FM_ALT | MAIN, "copy from user error\n");
				ret = -EFAULT;
				goto out;
			}

			if (parm_ctl.rw_flag == 0)
				ret = fm_top_write(fm, parm_ctl.addr, parm_ctl.val);
			else
				ret = fm_top_read(fm, parm_ctl.addr, &parm_ctl.val);

			if (ret < 0)
				goto out;

			if ((parm_ctl.rw_flag == 0x01) && (!ret)) {
				if (copy_to_user((void *)arg, &parm_ctl, sizeof(struct fm_top_rw_parm))) {
					ret = -EFAULT;
					goto out;
				}
			}

			break;
		}
	case FM_IOCTL_HOST_RDWR:
		{
			struct fm_host_rw_parm parm_ctl;

			WCN_DBG(FM_INF | MAIN, "FM_IOCTL_HOST_RDWR\n");

#ifdef CONFIG_MTK_USER_BUILD
			WCN_DBG(FM_ERR | MAIN, "Not support FM_IOCTL_HOST_RDWR\n");
			ret = -EFAULT;
			goto out;
#endif

			if (g_dbg_level != 0xfffffff7) {
				WCN_DBG(FM_ERR | MAIN, "Not support FM_IOCTL_HOST_RDWR\n");
				ret = -EFAULT;
				goto out;
			}

			if (fm->chipon == false || fm_pwr_state_get(fm) == FM_PWR_OFF) {
				WCN_DBG(FM_ERR | MAIN, "ERROR, FM chip is OFF\n");
				ret = -EFAULT;
				goto out;
			}

			if (copy_from_user(&parm_ctl, (void *)arg, sizeof(struct fm_host_rw_parm))) {
				WCN_DBG(FM_ALT | MAIN, "copy from user error\n");
				ret = -EFAULT;
				goto out;
			}

			/* 4 bytes alignment and illegal address */
			if (parm_ctl.addr % 4 != 0 || parm_ctl.addr >= 0x90000000) {
				ret = -FM_EPARA;
				goto out;
			}
			WCN_DBG(FM_NTC | MAIN, "rw? (%d), addr: %x\n", parm_ctl.rw_flag, parm_ctl.addr);
			if (parm_ctl.rw_flag == 0)
				ret = fm_host_write(fm, parm_ctl.addr, parm_ctl.val);
			else
				ret = fm_host_read(fm, parm_ctl.addr, &parm_ctl.val);

			if (ret < 0)
				goto out;

			if ((parm_ctl.rw_flag == 0x01) && (!ret)) {
				if (copy_to_user((void *)arg, &parm_ctl, sizeof(struct fm_host_rw_parm))) {
					ret = -EFAULT;
					goto out;
				}
			}

			break;
		}
	case FM_IOCTL_PMIC_RDWR:
		{
			struct fm_pmic_rw_parm parm_ctl;

			WCN_DBG(FM_INF | MAIN, "FM_IOCTL_PMIC_RDWR\n");
			if (fm->chipon == false || fm_pwr_state_get(fm) == FM_PWR_OFF) {
				WCN_DBG(FM_ERR | MAIN, "ERROR, FM chip is OFF\n");
				ret = -EFAULT;
				goto out;
			}

			if (copy_from_user(&parm_ctl, (void *)arg, sizeof(struct fm_pmic_rw_parm))) {
				WCN_DBG(FM_ALT | MAIN, "copy from user error\n");
				ret = -EFAULT;
				goto out;
			}

			if (parm_ctl.rw_flag == 0)
				ret = fm_pmic_write(fm, parm_ctl.addr, parm_ctl.val);
			else
				ret = fm_pmic_read(fm, parm_ctl.addr, &parm_ctl.val);

			if (ret < 0)
				goto out;

			if ((parm_ctl.rw_flag == 0x01) && (!ret)) {
				if (copy_to_user((void *)arg, &parm_ctl, sizeof(struct fm_pmic_rw_parm))) {
					ret = -EFAULT;
					goto out;
				}
			}

			break;
		}
	case FM_IOCTL_GETCHIPID:{
			unsigned short chipid = 0;

			ret = fm_chipid_get(fm, &chipid);
			WCN_DBG(FM_INF | MAIN, "FM_IOCTL_GETCHIPID:%04x\n", chipid);
			if (ret < 0)
				goto out;

			if (copy_to_user((void *)arg, &chipid, sizeof(unsigned short))) {
				ret = -EFAULT;
				goto out;
			}
			break;
		}

	case FM_IOCTL_GETMONOSTERO:{
			unsigned short usStereoMono = 0;

			ret = fm_monostereo_get(fm, &usStereoMono);
			WCN_DBG(FM_INF | MAIN, "FM_IOCTL_GETMONOSTERO:%04x\n", usStereoMono);
			if (ret < 0)
				goto out;

			if (copy_to_user((void *)arg, &usStereoMono, sizeof(unsigned short))) {
				ret = -EFAULT;
				goto out;
			}
			break;
		}

	case FM_IOCTL_SETMONOSTERO:{
			signed int monostero;

			WCN_DBG(FM_INF | MAIN, "FM_IOCTL_SETMONOSTERO\n");
			if (copy_from_user(&monostero, (void *)arg, sizeof(signed int))) {
				WCN_DBG(FM_ALT | MAIN, "copy from user error\n");
				ret = -EFAULT;
				goto out;
			}
			ret = fm_monostereo_set(fm, monostero);
			break;
		}

	case FM_IOCTL_GETCURPAMD:{
			unsigned short PamdLevl = 0;

			ret = fm_pamd_get(fm, &PamdLevl);
			WCN_DBG(FM_INF | MAIN, "FM_IOCTL_GETCURPAMD:%d\n", PamdLevl);
			if (ret < 0)
				goto out;

			if (copy_to_user((void *)arg, &PamdLevl, sizeof(unsigned short)))
				ret = -EFAULT;
			goto out;

			break;
		}

	case FM_IOCTL_GETCAPARRAY:{
			signed int ca = 0;

			ret = fm_caparray_get(fm, &ca);
			WCN_DBG(FM_INF | MAIN, "FM_IOCTL_GETCAPARRAY:%d\n", ca);
			if (ret < 0)
				goto out;

			if (copy_to_user((void *)arg, &ca, sizeof(signed int))) {
				ret = -EFAULT;
				goto out;
			}
			break;
		}

	case FM_IOCTL_EM_TEST:{
			struct fm_em_parm parm_em;

			WCN_DBG(FM_DBG | MAIN, "FM_IOCTL_EM_TEST\n");

			if (copy_from_user(&parm_em, (void *)arg, sizeof(struct fm_em_parm))) {
				ret = -EFAULT;
				goto out;
			}
			ret = fm_em_test(fm, parm_em.group_idx, parm_em.item_idx, parm_em.item_value);
			break;
		}

	case FM_IOCTL_RDS_SUPPORT:{
			signed int support = FM_RDS_ENABLE;

			WCN_DBG(FM_NTC | MAIN, "FM_IOCTL_RDS_SUPPORT\n");

			if (copy_to_user((void *)arg, &support, sizeof(signed int))) {
				ret = -EFAULT;
				goto out;
			}
			break;
		}

	case FM_IOCTL_IS_FM_POWERED_UP:{
			unsigned int powerup;

			WCN_DBG(FM_DBG | MAIN, "FM_IOCTL_IS_FM_POWERED_UP");

			if (fm->chipon && fm_pwr_state_get(fm))
				powerup = 1;
			else
				powerup = 0;

			if (copy_to_user((void *)arg, &powerup, sizeof(unsigned int))) {
				ret = -EFAULT;
				goto out;
			}
			break;
		}

	case FM_IOCTL_FM_SET_STATUS:{
			struct fm_status_t fm_stat;

			WCN_DBG(FM_DBG | MAIN, "FM_IOCTL_FM_SET_STATUS");

			if (copy_from_user(&fm_stat, (void *)arg, sizeof(struct fm_status_t))) {
				ret = -EFAULT;
				goto out;
			}

			fm_set_stat(fm, fm_stat.which, fm_stat.stat);

			break;
		}

	case FM_IOCTL_FM_GET_STATUS:{
			struct fm_status_t fm_stat;

			WCN_DBG(FM_DBG | MAIN, "FM_IOCTL_FM_GET_STATUS");

			if (copy_from_user(&fm_stat, (void *)arg, sizeof(struct fm_status_t))) {
				ret = -EFAULT;
				goto out;
			}

			fm_get_stat(fm, fm_stat.which, &fm_stat.stat);

			if (copy_to_user((void *)arg, &fm_stat, sizeof(struct fm_status_t))) {
				ret = -EFAULT;
				goto out;
			}

			break;
		}

	case FM_IOCTL_RDS_ONOFF:{
			unsigned short rdson_off = 0;

			WCN_DBG(FM_NTC | MAIN, "FM_IOCTL_RDS_ONOFF start\n");

			if (copy_from_user(&rdson_off, (void *)arg, sizeof(unsigned short))) {
				ret = -EFAULT;
				goto out;
			}
			ret = fm_rds_onoff(fm, rdson_off);
			WCN_DBG(FM_NTC | MAIN, "FM_IOCTL_RDS_ONOFF end:%d\n", rdson_off);
			break;
		}

	case FM_IOCTL_GETGOODBCNT:{
			unsigned short uGBLCNT = 0;

			ret = fm_rds_good_bc_get(fm, &uGBLCNT);
			WCN_DBG(FM_DBG | MAIN, "FM_IOCTL_GETGOODBCNT:%d\n", uGBLCNT);
			if (ret < 0)
				goto out;

			if (copy_to_user((void *)arg, &uGBLCNT, sizeof(unsigned short))) {
				ret = -EFAULT;
				goto out;
			}
			break;
		}

	case FM_IOCTL_GETBADBNT:{
			unsigned short uBadBLCNT = 0;

			ret = fm_rds_bad_bc_get(fm, &uBadBLCNT);
			WCN_DBG(FM_DBG | MAIN, "FM_IOCTL_GETBADBNT:%d\n", uBadBLCNT);
			if (ret < 0)
				goto out;

			if (copy_to_user((void *)arg, &uBadBLCNT, sizeof(unsigned short))) {
				ret = -EFAULT;
				goto out;
			}
			break;
		}

	case FM_IOCTL_GETBLERRATIO:{
			unsigned short uBlerRatio = 0;

			ret = fm_rds_bler_ratio_get(fm, &uBlerRatio);
			WCN_DBG(FM_DBG | MAIN, "FM_IOCTL_GETBLERRATIO:%d\n", uBlerRatio);
			if (ret < 0)
				goto out;

			if (copy_to_user((void *)arg, &uBlerRatio, sizeof(unsigned short))) {
				ret = -EFAULT;
				goto out;
			}
			break;
		}

	case FM_IOCTL_ANA_SWITCH:{
			signed int antenna = -1;

			WCN_DBG(FM_DBG | MAIN, "FM_IOCTL_ANA_SWITCH\n");

			if (copy_from_user(&antenna, (void *)arg, sizeof(signed int))) {
				WCN_DBG(FM_ALT | MAIN, "copy from user error\n");
				ret = -EFAULT;
				goto out;
			}

			ret = fm_ana_switch(fm, antenna);
			break;
		}

	case FM_IOCTL_RDS_GROUPCNT:{
			struct rds_group_cnt_req_t gc_req;

			WCN_DBG(FM_DBG | MAIN, "......FM_IOCTL_RDS_GROUPCNT......\n");

			if (copy_from_user(&gc_req, (void *)arg, sizeof(struct rds_group_cnt_req_t))) {
				WCN_DBG(FM_ALT | MAIN, "copy_from_user error\n");
				ret = -EFAULT;
				goto out;
			}
			/* handle group counter request */
			switch (gc_req.op) {
			case RDS_GROUP_CNT_READ:
				ret = fm_rds_group_cnt_get(fm, &gc_req.gc);
				break;
			case RDS_GROUP_CNT_WRITE:
				break;
			case RDS_GROUP_CNT_RESET:
				ret = fm_rds_group_cnt_reset(fm);
				break;
			default:
				break;
			}

			if (copy_to_user((void *)arg, &gc_req, sizeof(struct rds_group_cnt_req_t))) {
				WCN_DBG(FM_ALT | MAIN, "copy_to_user error\n");
				ret = -EFAULT;
				goto out;
			}

			break;
		}

	case FM_IOCTL_RDS_GET_LOG:{
			struct rds_raw_t rds_log;
			signed int len = 0;

			memset(rds_log.data, 0, sizeof(rds_log.data));
			WCN_DBG(FM_DBG | MAIN, "......FM_IOCTL_RDS_GET_LOG......\n");
			/* fetch a record form RDS log buffer */
			ret = fm_rds_log_get(fm, (struct rds_rx_t *)&(rds_log.data), &len);
			rds_log.dirty = true;
			rds_log.len = (len < sizeof(rds_log.data)) ? len : sizeof(rds_log.data);
			if (ret < 0)
				goto out;

			if (copy_to_user((void *)arg, &rds_log, rds_log.len + 2 * sizeof(signed int))) {
				WCN_DBG(FM_ALT | MAIN, "copy_to_user error\n");
				ret = -EFAULT;
				goto out;
			}

			break;
		}

	case FM_IOCTL_RDS_BC_RST:{
			WCN_DBG(FM_DBG | MAIN, "FM_IOCTL_RDS_BC_RST\n");
			ret = fm_rds_block_cnt_reset(fm);
			break;
		}

	case FM_IOCTL_I2S_SETTING:{
			struct fm_i2s_setting i2s_cfg;

			WCN_DBG(FM_NTC | MAIN, "FM_IOCTL_I2S_SETTING\n");

			if (copy_from_user(&i2s_cfg, (void *)arg, sizeof(struct fm_i2s_setting))) {
				WCN_DBG(FM_ALT | MAIN, "i2s set, copy_from_user err\n");
				ret = -EFAULT;
				goto out;
			}

			ret = fm_i2s_set(fm, i2s_cfg.onoff, i2s_cfg.mode, i2s_cfg.sample);

			if (ret) {
				WCN_DBG(FM_ALT | MAIN, "Set i2s err\n");
				goto out;
			}

			break;
		}

	case FM_IOCTL_IS_DESE_CHAN:{
			signed int tmp;

			WCN_DBG(FM_INF | MAIN, "FM_IOCTL_IS_DESE_CHAN\n");

			if (copy_from_user(&tmp, (void *)arg, sizeof(signed int))) {
				WCN_DBG(FM_ALT | MAIN, "is dese chan, copy_from_user err\n");
				ret = -EFAULT;
				goto out;
			}

			tmp = fm_is_dese_chan(fm, (unsigned short) tmp);

			if (copy_to_user((void *)arg, &tmp, sizeof(signed int))) {
				WCN_DBG(FM_ALT | MAIN, "is dese chan, copy_to_user err\n");
				ret = -EFAULT;
				goto out;
			}

			break;
		}
	case FM_IOCTL_DESENSE_CHECK:
		{
			struct fm_desense_check_t tmp;

			WCN_DBG(FM_INF | MAIN, "FM_IOCTL_DESENSE_CHECK\n");

			if (copy_from_user(&tmp, (void *)arg, sizeof(struct fm_desense_check_t))) {
				WCN_DBG(FM_ALT | MAIN, "desene check, copy_from_user err\n");
				ret = -EFAULT;
				goto out;
			}
			ret = fm_desense_check(fm, (unsigned short) tmp.freq, tmp.rssi);

			/*if (copy_to_user((void*)arg, &tmp, sizeof(struct fm_desense_check_t))) {
			*  WCN_DBG(FM_ALT | MAIN, "desene check, copy_to_user err\n");
			*  ret = -EFAULT;
			*  goto out;
			*  }
			*/

			break;
		}
	case FM_IOCTL_SCAN_GETRSSI:
		{
			WCN_DBG(FM_ALT | MAIN, "FM_IOCTL_SCAN_GETRSSI:not support\n");
			break;
		}

	case FM_IOCTL_DUMP_REG:
		{
			WCN_DBG(FM_NTC | MAIN, "......FM_IOCTL_DUMP_REG......\n");
			if (g_dbg_level != 0xfffffff7) {
				WCN_DBG(FM_ERR | MAIN, "Not support FM_IOCTL_HOST_RDWR\n");
				ret = -EFAULT;
				goto out;
			}
			if (fm->chipon == false || fm_pwr_state_get(fm) == FM_PWR_OFF) {
				WCN_DBG(FM_ERR | MAIN, "ERROR, FM chip is OFF\n");
				ret = -EFAULT;
				goto out;
			}
			ret = fm_dump_reg();
			if (ret)
				WCN_DBG(FM_ALT | MAIN, "fm_dump_reg err\n");
			break;
		}
	case FM_IOCTL_GPS_RTC_DRIFT:{
			struct fm_gps_rtc_info rtc_info;

			WCN_DBG(FM_NTC | MAIN, "......FM_IOCTL_GPS_RTC_DRIFT......\n");

			if (false == fm->chipon) {
				WCN_DBG(FM_ERR | MAIN, "ERROR, FM chip is OFF\n");
				ret = -EFAULT;
				goto out;
			}
			if (copy_from_user(&rtc_info, (void *)arg, sizeof(struct fm_gps_rtc_info))) {
				WCN_DBG(FM_ERR | MAIN, "copy_from_user error\n");
				ret = -EFAULT;
				goto out;
			}

			ret = fm_get_gps_rtc_info(&rtc_info);
			if (ret) {
				WCN_DBG(FM_ERR | MAIN, "fm_get_gps_rtc_info error\n");
				goto out;
			}
			break;
		}
	case FM_IOCTL_OVER_BT_ENABLE:
		{
			signed int fm_via_bt = -1;

			WCN_DBG(FM_NTC | MAIN, "......FM_IOCTL_OVER_BT_ENABLE......\n");

			if (copy_from_user(&fm_via_bt, (void *)arg, sizeof(int32_t))) {
				WCN_DBG(FM_ERR | MAIN, "copy_from_user error\n");
				ret = -EFAULT;
				goto out;
			}

			ret = fm_over_bt(fm, fm_via_bt);
			if (ret)
				WCN_DBG(FM_ERR | MAIN, "fm_over_bt err\n");
			break;
		}

	case FM_IOCTL_SET_SEARCH_THRESHOLD:
		{
			struct fm_search_threshold_t parm;

			WCN_DBG(FM_NTC | MAIN, "......FM_IOCTL_SET_SEARCH_THRESHOLD......\n");

			if (copy_from_user(&parm, (void *)arg, sizeof(struct fm_search_threshold_t))) {
				WCN_DBG(FM_ALT | MAIN, "copy_from_user error\n");
				ret = -EFAULT;
				goto out;
			}
			ret = fm_set_search_th(fm, parm);
			if (ret < 0)
				WCN_DBG(FM_ERR | MAIN, "FM_IOCTL_SET_SEARCH_THRESHOLD not supported\n");
			break;
		}
	case FM_IOCTL_GET_AUDIO_INFO:
		{
			struct fm_audio_info_t aud_data;

			ret = fm_get_aud_info(&aud_data);
			if (ret)
				WCN_DBG(FM_ERR | MAIN, "fm_get_aud_info err\n");

			if (copy_to_user((void *)arg, &aud_data, sizeof(struct fm_audio_info_t))) {
				WCN_DBG(FM_ERR | MAIN, "copy_to_user error\n");
				ret = -EFAULT;
				goto out;
			}

			WCN_DBG(FM_INF | MAIN, "fm_get_aud_info ret=%d\n", ret);
			break;
		}
    /***************************FM Tx function************************************/
	case FM_IOCTL_TX_SUPPORT:
		{
			signed int tx_support = -1;

			WCN_DBG(FM_NTC | MAIN, "......FM_IOCTL_TX_SUPPORT......\n");

			ret = fm_tx_support(fm, &tx_support);
			if (ret)
				WCN_DBG(FM_ERR | MAIN, "fm_tx_support err\n");

			if (copy_to_user((void *)arg, &tx_support, sizeof(signed int))) {
				WCN_DBG(FM_ERR | MAIN, "copy_to_user error\n");
				ret = -EFAULT;
				goto out;
			}
			break;
		}
	case FM_IOCTL_POWERUP_TX:
		{
			struct fm_tune_parm parm;

			WCN_DBG(FM_NTC | MAIN, "FM_IOCTL_POWERUP_TX:0\n");
			if (copy_from_user(&parm, (void *)arg, sizeof(struct fm_tune_parm))) {
				ret = -EFAULT;
				goto out;
			}

			ret = fm_powerup_tx(fm, &parm);
			if (ret < 0)
				goto out;

			ret = fm_tune_tx(fm, &parm);
			if (ret < 0)
				goto out;

			if (copy_to_user((void *)arg, &parm, sizeof(struct fm_tune_parm))) {
				ret = -EFAULT;
				goto out;
			}
			WCN_DBG(FM_NTC | MAIN, "FM_IOCTL_POWERUP_TX:1\n");
			break;
		}

	case FM_IOCTL_TUNE_TX:
		{
			struct fm_tune_parm parm;

			WCN_DBG(FM_NTC | MAIN, "FM_IOCTL_TUNE_TX:0\n");

			if (copy_from_user(&parm, (void *)arg, sizeof(struct fm_tune_parm))) {
				ret = -EFAULT;
				goto out;
			}

			ret = fm_tune_tx(fm, &parm);
			if (ret < 0)
				goto out;

			if (copy_to_user((void *)arg, &parm, sizeof(struct fm_tune_parm))) {
				ret = -EFAULT;
				goto out;
			}

			WCN_DBG(FM_NTC | MAIN, "FM_IOCTL_TUNE_TX:1\n");
			break;
		}
	case FM_IOCTL_RDSTX_SUPPORT:
		{
			signed int rds_tx_support = -1;

			WCN_DBG(FM_NTC | MAIN, "......FM_IOCTL_RDSTX_SUPPORT......\n");

			ret = fm_rdstx_support(fm, &rds_tx_support);
			if (ret)
				WCN_DBG(FM_ERR | MAIN, "fm_rdstx_support err\n");

			if (copy_to_user((void *)arg, &rds_tx_support, sizeof(signed int))) {
				WCN_DBG(FM_ERR | MAIN, "copy_to_user error\n");
				ret = -EFAULT;
				goto out;
			}
			break;
		}

	case FM_IOCTL_RDSTX_ENABLE:
		{
			signed int onoff = -1;

			WCN_DBG(FM_NTC | MAIN, "......FM_IOCTL_RDSTX_ENABLE......\n");

			if (copy_from_user(&onoff, (void *)arg, sizeof(signed int))) {
				WCN_DBG(FM_ALT | MAIN, "FM_IOCTL_RDSTX_ENABLE, copy_from_user err\n");
				ret = -EFAULT;
				goto out;
			}

			ret = fm_rdstx_enable(fm, onoff);
			if (ret)
				WCN_DBG(FM_ERR | MAIN, "fm_rdstx_enable err\n");

			break;
		}

	case FM_IOCTL_RDS_TX:
		{
			struct fm_rds_tx_parm parm;

			WCN_DBG(FM_NTC | MAIN, "......FM_IOCTL_RDS_TX......\n");

			if (copy_from_user(&parm, (void *)arg, sizeof(struct fm_rds_tx_parm))) {
				WCN_DBG(FM_ALT | MAIN, "RDS Tx, copy_from_user err\n");
				ret = -EFAULT;
				goto out;
			}

			ret = fm_rds_tx(fm, &parm);
			if (ret)
				WCN_DBG(FM_ALT | MAIN, "fm_rds_tx err\n");

			if (copy_to_user((void *)arg, &parm, sizeof(struct fm_rds_tx_parm))) {
				WCN_DBG(FM_ALT | MAIN, "RDS Tx, copy_to_user err\n");
				ret = -EFAULT;
				goto out;
			}
			break;
		}

	case FM_IOCTL_TX_SCAN:
		{
			struct fm_tx_scan_parm parm;

			WCN_DBG(FM_NTC | MAIN, "......FM_IOCTL_TX_SCAN......\n");

			if (copy_from_user(&parm, (void *)arg, sizeof(struct fm_tx_scan_parm))) {
				WCN_DBG(FM_ALT | MAIN, "copy_from_user error\n");
				ret = -EFAULT;
				goto out;
			}
			ret = fm_tx_scan(fm, &parm);
			if (ret < 0)
				WCN_DBG(FM_ERR | MAIN, "FM_IOCTL_TX_SCAN failed\n");

			if (copy_to_user((void *)arg, &parm, sizeof(struct fm_tx_scan_parm))) {
				WCN_DBG(FM_ALT | MAIN, "copy_to_user error\n");
				ret = -EFAULT;
				goto out;
			}
			break;
		}

	default:
		ret = -EPERM;
	}

out:
	if (ret == -FM_EFW) {
		if (fm_sys_state_get(fm) == FM_SUBSYS_RST_OFF) {
			fm->wholechiprst = false;
			/* subsystem reset */
			WCN_DBG(FM_NTC | MAIN, "fm_subsys_reset START\n");
			fm_subsys_reset(fm);
			WCN_DBG(FM_NTC | MAIN, "fm_subsys_reset END\n");
		}
	}

	return ret;
}

static loff_t fm_ops_lseek(struct file *filp, loff_t off, signed int whence)
{
	struct fm *fm = filp->private_data;

	if (whence == SEEK_END)
		fm_hwscan_stop(fm);
	else if (whence == SEEK_SET)
		FM_EVENT_SEND(fm->rds_event, FM_RDS_DATA_READY);

	return off;
}

static ssize_t fm_ops_read(struct file *filp, char *buf, size_t len, loff_t *off)
{
	struct fm *fm = filp->private_data;
	signed int copy_len = 0;

	if (!fm) {
		WCN_DBG(FM_ALT | MAIN, "fm_read invalid fm pointer\n");
		return 0;
	}

	WCN_DBG(FM_DBG | MAIN, "rds buf len=%zu\n", len);
	WCN_DBG(FM_DBG | MAIN, "sizeof(struct rds_t)=%zu\n", sizeof(struct rds_t));

	if (!buf || len < sizeof(struct rds_t)) {
		WCN_DBG(FM_NTC | MAIN, "fm_read invliad buf\n");
		return 0;
	}
	/* return if FM is resetting */
	if (fm_sys_state_get(fm) != FM_SUBSYS_RST_OFF) {
		WCN_DBG(FM_ALT | MAIN, "fm subsys underring reset\n");
		return 0;
	}

	copy_len = sizeof(struct rds_t);

	return fm_rds_read(fm, buf, copy_len);
}

atomic_t g_counter = ATOMIC_INIT(0);
static signed int fm_ops_open(struct inode *inode, struct file *filp)
{
	signed int ret = 0;
	struct fm_platform *plat = container_of(inode->i_cdev, struct fm_platform, cdev);
	struct fm *fm = container_of(plat, struct fm, platform);

	if (fm_sys_state_get(fm) != FM_SUBSYS_RST_OFF) {
		WCN_DBG(FM_ALT | MAIN, "FM subsys is resetting, retry later\n");
		ret = -FM_ESRST;
		return ret;
	}

	ret = fm_open(fm);
	filp->private_data = fm;
	atomic_inc(&g_counter);

	WCN_DBG(FM_NTC | MAIN, "fm_ops_open:%d [%d]\n", current->pid, atomic_read(&g_counter));
	return ret;
}

static signed int fm_ops_release(struct inode *inode, struct file *filp)
{
/* signed int ret = 0; */
/* struct fm_platform *plat = container_of(inode->i_cdev, struct fm_platform, cdev); */
/* struct fm *fm = container_of(plat, struct fm, platform); */
	struct fm *fm = filp->private_data;

	WCN_DBG(FM_NTC | MAIN, "fm_ops_release:%d [%d]\n", current->pid, atomic_read(&g_counter));

	if (atomic_dec_and_test(&g_counter)) {
		WCN_DBG(FM_ALT | MAIN, "FM power down... [%d]\n", atomic_read(&g_counter));
		if (-FM_ELOCK == fm_powerdown(fm, 0)) {
			WCN_DBG(FM_ALT | MAIN, "FM power down fail. Do it later.\n");
			fm->timer_wkthd->add_work(fm->timer_wkthd, fm->pwroff_wk);
		}
	}

	filp->private_data = NULL;
	return 0;
}

static signed int fm_ops_flush(struct file *filp, fl_owner_t Id)
{
	signed int ret = 0;
	struct fm *fm = filp->private_data;

	WCN_DBG(FM_NTC | MAIN, "fm_ops_flush:%d\n", current->pid);
	fm_close(fm);
	filp->private_data = fm;
	return ret;
}

static ssize_t fm_proc_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	struct fm *fm = g_fm;
	ssize_t length = 0;
	char tmpbuf[3];
	unsigned long pos = *ppos;

	WCN_DBG(FM_NTC | MAIN, "Enter fm_proc_read.\n");
	/* WCN_DBG(FM_NTC | MAIN, "count = %d\n", count); */
	/* WCN_DBG(FM_NTC | MAIN, "ppos = %d\n", pos); */

	if (pos != 0)
		return 0;

	if (!fm) {
		WCN_DBG(FM_ALT | MAIN, "para err\n");
		return 0;
	}

	if (fm->chipon && (fm_pwr_state_get(fm) == FM_PWR_RX_ON)) {
		length = snprintf(tmpbuf, sizeof(tmpbuf), "1\n");
		WCN_DBG(FM_NTC | MAIN, " FM_PWR_RX_ON\n");
	} else if (fm->chipon && (fm_pwr_state_get(fm) == FM_PWR_TX_ON)) {
		length = snprintf(tmpbuf, sizeof(tmpbuf), "2\n");
		WCN_DBG(FM_NTC | MAIN, " FM_PWR_TX_ON\n");
	} else {
		length = snprintf(tmpbuf, sizeof(tmpbuf), "0\n");
		WCN_DBG(FM_NTC | MAIN, " FM POWER OFF\n");
	}

	if (copy_to_user(buf, tmpbuf, length)) {
		WCN_DBG(FM_NTC | MAIN, " Read FM status fail!\n");
		return 0;
	}

	pos += length;
	*ppos = pos;
	WCN_DBG(FM_NTC | MAIN, "Leave fm_proc_read. length = %zu\n", length);

	return length;
}

static ssize_t fm_proc_write(struct file *file, const char *buffer, size_t count, loff_t *ppos)
{
	struct fm *fm = g_fm;
	signed char tmp_buf[51] = { 0 };
	unsigned int copysize;

	WCN_DBG(FM_NTC | MAIN, "fm_proc_write:0 count = %zu\n", count);
	if (count <= 0 || buffer == NULL) {
		WCN_DBG(FM_ERR | MAIN, "failed parameter not accept\n");
		return -EFAULT;
	}

	copysize = (count < (sizeof(tmp_buf) - 1)) ? count : (sizeof(tmp_buf) - 1);

	memset(tmp_buf, 0, sizeof(tmp_buf));
	if (copy_from_user(tmp_buf, buffer, copysize)) {
		WCN_DBG(FM_ERR | MAIN, "failed copy_from_user\n");
		return -EFAULT;
	}

	if (strncmp(tmp_buf, "subsys reset", strlen("subsys reset")) == 0) {
		fm_subsys_reset(fm);
		return count;
	}

	if (!fm->chipon || (fm_pwr_state_get(fm) != FM_PWR_RX_ON)) {
		WCN_DBG(FM_ERR | MAIN, "FM is off.\n");
		return -EFAULT;
	}

	if (kstrtouint(tmp_buf, 0, &g_dbg_level)) {
		tmp_buf[50] = '\0';
		WCN_DBG(FM_ERR | MAIN, "Not a valid dbg_level: %s\n", tmp_buf);
		if (!fm_cust_config_setup(tmp_buf)) {
			WCN_DBG(FM_NTC | MAIN, "get config form %s ok\n", tmp_buf);
			return count;
		}
		return -EFAULT;
	}

	WCN_DBG(FM_NTC | MAIN, "fm_proc_write:1 g_dbg_level = 0x%x\n", g_dbg_level);
	return count;
}

/* #define FM_DEV_STATIC_ALLOC */
/* #define FM_DEV_MAJOR    193 */
/* static int FM_major = FM_DEV_MAJOR;  *//* dynamic allocation */

static signed int fm_cdev_setup(struct fm *fm)
{
	signed int ret = 0;
	struct fm_platform *plat = &fm->platform;

#ifdef FM_DEV_STATIC_ALLOC
	/*static allocate chrdev */
	plat->dev_t = MKDEV(FM_major, 0);
	ret = register_chrdev_region(plat->dev_t, 1, FM_NAME);

	if (ret) {
		WCN_DBG(FM_ERR | MAIN, "%s():fail to register chrdev\n", __func__);
		return ret;
	}
#endif

#ifndef FM_DEV_STATIC_ALLOC
	ret = alloc_chrdev_region(&plat->dev_t, 0, 1, FM_NAME);

	if (ret) {
		WCN_DBG(FM_ALT | MAIN, "alloc dev_t failed\n");
		return ret;
	}
#endif

	WCN_DBG(FM_NTC | MAIN, "alloc %s:%d:%d\n", FM_NAME, MAJOR(plat->dev_t), MINOR(plat->dev_t));

	cdev_init(&plat->cdev, &fm_ops);

	plat->cdev.owner = THIS_MODULE;
	plat->cdev.ops = &fm_ops;

	ret = cdev_add(&plat->cdev, plat->dev_t, 1);

	if (ret) {
		WCN_DBG(FM_ALT | MAIN, "add dev_t failed\n");
		return ret;
	}
#ifndef FM_DEV_STATIC_ALLOC
	plat->cls = class_create(THIS_MODULE, FM_NAME);

	if (IS_ERR(plat->cls)) {
		ret = PTR_ERR(plat->cls);
		WCN_DBG(FM_ALT | MAIN, "class_create err:%d\n", ret);
		return ret;
	}

	plat->dev = device_create(plat->cls, NULL, plat->dev_t, NULL, FM_NAME);
#endif

	return ret;
}

static signed int fm_cdev_destroy(struct fm *fm)
{
	if (fm == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	device_destroy(fm->platform.cls, fm->platform.dev_t);
	class_destroy(fm->platform.cls);
	cdev_del(&fm->platform.cdev);
	unregister_chrdev_region(fm->platform.dev_t, 1);

	return 0;
}

static signed int fm_mod_init(unsigned int arg)
{
	signed int ret = 0;
	struct fm *fm = NULL;

	fm = fm_dev_init(0);

	if (!fm) {
		ret = -ENOMEM;
		goto ERR_EXIT;
	}

	ret = fm_cdev_setup(fm);
	if (ret)
		goto ERR_EXIT;

	/* fm proc file create "/proc/fm" */
	g_fm_proc = proc_create(FM_PROC_FILE, 0444, NULL, &fm_proc_ops);

	if (g_fm_proc == NULL) {
		WCN_DBG(FM_ALT | MAIN, "create_proc_entry failed\n");
		ret = -ENOMEM;
		goto ERR_EXIT;
	} else {
		WCN_DBG(FM_NTC | MAIN, "create_proc_entry success\n");
	}

	g_fm = fm;
	return 0;

ERR_EXIT:

	if (fm) {
		fm_cdev_destroy(fm);
		fm_dev_destroy(fm);
	}

	remove_proc_entry(FM_PROC_FILE, NULL);
	return ret;
}

static signed int fm_mod_destroy(struct fm *fm)
{
	signed int ret = 0;

	if (fm == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	WCN_DBG(FM_NTC | MAIN, "%s\n", __func__);
	remove_proc_entry(FM_PROC_FILE, NULL);
	fm_cdev_destroy(fm);
	fm_dev_destroy(fm);

	return ret;
}

static signed int mt_fm_probe(struct platform_device *pdev)
{
	signed int ret = 0;

	WCN_DBG(FM_NTC | MAIN, "%s\n", __func__);

	ret = fm_mod_init(0);

	if (ret) {
		WCN_DBG(FM_ERR | MAIN, "fm mod init err\n");
		return -ENOMEM;
	}

	return ret;
}

static signed int mt_fm_remove(struct platform_device *pdev)
{
	WCN_DBG(FM_NTC | MAIN, "%s\n", __func__);

	fm_mod_destroy(g_fm);
	g_fm = NULL;
	return 0;
}

static struct platform_device *pr_fm_device;
/*
static struct platform_device mt_fm_device = {
	.name = FM_NAME,
	.id = -1,
};
*/
/* platform driver entry */
static struct platform_driver mt_fm_dev_drv = {
	.probe = mt_fm_probe,
	.remove = mt_fm_remove,
	.driver = {
		   .name = FM_NAME,
		   .owner = THIS_MODULE,
		   }
};

static signed int mt_fm_init(void)
{
	signed int ret = 0;

	ret = fm_env_setup();

	if (ret) {
		fm_env_destroy();
		return ret;
	}
	/* register fm device to platform bus */
	/* ret = platform_device_register(&mt_fm_device);
	 *
	 * if (ret)
	 *	return ret;
	 */

	pr_fm_device = platform_device_alloc(FM_NAME, 0);
	if (!pr_fm_device) {
		WCN_DBG(FM_ERR | MAIN, "fm platform device alloc fail\n");
		return -ENOMEM;
	}

	ret = platform_device_add(pr_fm_device);
	if (ret) {
		WCN_DBG(FM_ERR | MAIN, "fm platform device add failed(%d)\n", ret);
		platform_device_put(pr_fm_device);
		return ret;
	}

	/* register fm driver to platform bus */
	ret = platform_driver_register(&mt_fm_dev_drv);

	if (ret) {
		WCN_DBG(FM_ERR | MAIN, "fm platform driver register fail(%d)\n", ret);
		platform_device_unregister(pr_fm_device);
		return ret;
	}

	fm_register_irq(&mt_fm_dev_drv);

	WCN_DBG(FM_NTC | MAIN, "6. fm platform driver registered\n");
	return ret;
}

static void mt_fm_exit(void)
{
	WCN_DBG(FM_NTC | MAIN, "%s\n", __func__);
	platform_driver_unregister(&mt_fm_dev_drv);
	/* platform_device_unregister(&mt_fm_device); */
	platform_device_unregister(pr_fm_device);
	fm_env_destroy();
}

#ifdef MTK_WCN_REMOVE_KERNEL_MODULE
int mtk_wcn_fm_init(void)
{
	return mt_fm_init();
}
EXPORT_SYMBOL(mtk_wcn_fm_init);

void mtk_wcn_fm_exit(void)
{
	mt_fm_exit();
}
EXPORT_SYMBOL(mtk_wcn_fm_exit);
#else
module_init(mt_fm_init);
module_exit(mt_fm_exit);
#endif
EXPORT_SYMBOL(g_dbg_level);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek FM Driver");
MODULE_AUTHOR("Hongcheng <hongcheng.xia@MediaTek.com>");
