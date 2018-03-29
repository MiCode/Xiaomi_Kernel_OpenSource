 /*
  * Copyright (C) 2016 MediaTek Inc.
  *
  * drivers/misc/mediatek/pd/mtk_direct_charge_vdm.c
  * MTK Direct Charge Vdm Driver
  * Author: Sakya <jeff_chang@richtek.com>
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


#include <linux/err.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif /* CONFIG_DEBUG_FS */
#include <linux/usb/class-dual-role.h>

#include "inc/mtk_direct_charge_vdm.h"
#include "inc/tcpm.h"

#define MTK_VDM_COUNT	(10)
#define MTK_VDM_DELAY	(50)

static struct tcpc_device *tcpc;
static struct dual_role_phy_instance *dr_usb;
static atomic_t vdm_event_flag;
static struct notifier_block vdm_nb;
static struct mutex vdm_event_lock;
static struct mutex vdm_par_lock;
static struct wake_lock vdm_event_wake_lock;
static bool vdm_inited;
static uint32_t vdm_payload[7];
static bool vdm_success;

static int vdm_tcp_notifier_call(struct notifier_block *nb,
					unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	int i;

	switch (event) {
	case TCP_NOTIFY_RT7207_VDM:
		if (atomic_read(&vdm_event_flag)) {
			mutex_lock(&vdm_par_lock);
			if (noti->rt7207_vdm_success) {
				vdm_success = true;
				for (i = 0; i < 7; i++)
					vdm_payload[i] = noti->payload[i];
			} else
				vdm_success = false;
			mutex_unlock(&vdm_par_lock);
			atomic_dec(&vdm_event_flag); /* set flag = 0 */
		}
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

static void mtk_vdm_lock(void)
{
	wake_lock(&vdm_event_wake_lock);
	mutex_lock(&vdm_event_lock);
}

static void mtk_vdm_unlock(void)
{
	mutex_unlock(&vdm_event_lock);
	wake_unlock(&vdm_event_wake_lock);
}

int mtk_get_ta_id(struct tcpc_device *tcpc)
{
	int count = MTK_VDM_COUNT;
	int id = 0;

	if (!vdm_inited) {
		pr_err("%s vdm not inited\n", __func__);
		return -EINVAL;
	}

	mtk_vdm_lock();
	atomic_inc(&vdm_event_flag); /* set flag = 1 */
	tcpm_vdm_request_rt7207(tcpc, 0x1012, 0);
	while (count) {
		if (atomic_read(&vdm_event_flag) == 0) {
			mutex_lock(&vdm_par_lock);
			if (vdm_success)
				id = vdm_payload[1];
			else
				id = -1;
			pr_err("%s id = 0x%x\n", __func__, id);
			mutex_unlock(&vdm_par_lock);
			mtk_vdm_unlock();
			return id;
		}
		count--;
		mdelay(MTK_VDM_DELAY);
	}
	pr_err("%s Time out\n", __func__);
	atomic_dec(&vdm_event_flag);
	mtk_vdm_unlock();
	return -1;
}

int mtk_get_ta_charger_status(struct tcpc_device *tcpc)
{
	int count = MTK_VDM_COUNT;
	int status;

	if (!vdm_inited) {
		pr_err("%s vdm not inited\n", __func__);
		return -EINVAL;
	}

	mtk_vdm_lock();
	atomic_inc(&vdm_event_flag); /* set flag = 1 */
	tcpm_vdm_request_rt7207(tcpc, 0x101a, 0);
	while (count) {
		if (atomic_read(&vdm_event_flag) == 0) {
			mutex_lock(&vdm_par_lock);
			if (vdm_success)
				status = (vdm_payload[1]&0x80000000) ?
					RT7207_CC_MODE : RT7207_CV_MODE;
			else {
				pr_err("%s Failed\n", __func__);
				status = -1;
			}
			pr_err("%s status = %s\n", __func__, status > 0 ?
					"CC Mode" : "CV Mode");
			mutex_unlock(&vdm_par_lock);
			mtk_vdm_unlock();
			return status;
		}
		count--;
		mdelay(MTK_VDM_DELAY);
	}
	pr_err("%s Time out\n", __func__);
	atomic_dec(&vdm_event_flag);
	mtk_vdm_unlock();
	return -1;
}

int mtk_get_ta_current_cap(struct tcpc_device *tcpc,
					struct mtk_vdm_ta_cap *cap)
{
	int count = MTK_VDM_COUNT;
	int status = MTK_VDM_FAIL;

	if (!vdm_inited) {
		pr_err("%s vdm not inited\n", __func__);
		return -EINVAL;
	}

	mtk_vdm_lock();
	atomic_inc(&vdm_event_flag); /* set flag = 1 */
	tcpm_vdm_request_rt7207(tcpc, 0x101b, 0);
	while (count) {
		if (atomic_read(&vdm_event_flag) == 0) {
			mutex_lock(&vdm_par_lock);
			if (vdm_success) {
				cap->vol = vdm_payload[1]&0x0000ffff;
				cap->cur = (vdm_payload[1]&0xffff0000)>>16;
				status = MTK_VDM_SUCCESS;
				pr_err("%s mv = %dmv,ma = %dma\n",
					__func__, cap->vol, cap->cur);
			} else {
				pr_err("%s Failed\n", __func__);
				status = MTK_VDM_FAIL;
			}
			mutex_unlock(&vdm_par_lock);
			mtk_vdm_unlock();
			return status;
		}
		count--;
		mdelay(MTK_VDM_DELAY);
	}
	pr_err("%s Time out\n", __func__);
	atomic_dec(&vdm_event_flag);
	mtk_vdm_unlock();
	return status;
}

int mtk_get_ta_setting_dac(struct tcpc_device *tcpc,
					struct mtk_vdm_ta_cap *cap)
{
	int count = MTK_VDM_COUNT;
	int status = MTK_VDM_FAIL;

	if (!vdm_inited) {
		pr_err("%s vdm not inited\n", __func__);
		return -EINVAL;
	}

	mtk_vdm_lock();
	atomic_inc(&vdm_event_flag); /* set flag = 1 */
	tcpm_vdm_request_rt7207(tcpc, 0x101c, 0);
	while (count) {
		if (atomic_read(&vdm_event_flag) == 0) {
			mutex_lock(&vdm_par_lock);
			if (vdm_success) {
				cap->vol = (vdm_payload[1]&0x0000ffff) * 26;
				cap->cur = ((vdm_payload[1]&0xffff0000)>>16) * 26;
				status = MTK_VDM_SUCCESS;
				pr_err("%s mv = %dmv,ma = %dma\n",
					__func__, cap->vol, cap->cur);
			} else {
				pr_err("%s Failed\n", __func__);
				status = MTK_VDM_FAIL;
			}
			mutex_unlock(&vdm_par_lock);
			mtk_vdm_unlock();
			return status;
		}
		count--;
		mdelay(MTK_VDM_DELAY);
	}
	pr_err("%s Time out\n", __func__);
	atomic_dec(&vdm_event_flag);
	mtk_vdm_unlock();
	return status;
}

int mtk_get_ta_temperature(struct tcpc_device *tcpc)
{
	return -1;
}

int mtk_show_ta_info(struct tcpc_device *tcpc)
{
	int count = MTK_VDM_COUNT;
	int status = MTK_VDM_FAIL;

	if (!vdm_inited) {
		pr_err("%s vdm not inited\n", __func__);
		return -EINVAL;
	}

	mtk_vdm_lock();
	atomic_inc(&vdm_event_flag);
	tcpm_vdm_request_rt7207(tcpc, 0x101f, 0);
	while (count) {
		if (atomic_read(&vdm_event_flag) == 0) {
			mutex_lock(&vdm_par_lock);
			if (vdm_success) {
				pr_err("%s CUR(%d), VOL(%d), T(%d)\n", __func__,
					(vdm_payload[1]&0xffff0000)>>16,
					(vdm_payload[1]&0x0000ffff),
					vdm_payload[2]),
				status = MTK_VDM_SUCCESS;
			} else {
				pr_err("%s Failed\n", __func__);
				status = MTK_VDM_FAIL;
			}
			mutex_unlock(&vdm_par_lock);
			mtk_vdm_unlock();
			return status;
		}
		count--;
		mdelay(MTK_VDM_DELAY);
	}
	pr_err("%s Time out\n", __func__);
	atomic_dec(&vdm_event_flag);
	return status;
}

int mtk_set_ta_boundary_cap(struct tcpc_device *tcpc,
					struct mtk_vdm_ta_cap *cap)
{
	int count = MTK_VDM_COUNT;
	int status = MTK_VDM_FAIL;
	uint32_t data;

	if (!vdm_inited) {
		pr_err("%s vdm not inited\n", __func__);
		return -EINVAL;
	}

	mtk_vdm_lock();
	data = (cap->cur<<16)|cap->vol;

	pr_err("%s set mv = %dmv,ma = %dma\n",
		__func__, cap->vol, cap->cur);

	atomic_inc(&vdm_event_flag); /* set flag = 1 */
	tcpm_vdm_request_rt7207(tcpc, 0x2021, data);
	while (count) {
		if (atomic_read(&vdm_event_flag) == 0) {
			mutex_lock(&vdm_par_lock);
			if (vdm_success) {
				cap->vol = vdm_payload[1]&0x0000ffff;
				cap->cur = (vdm_payload[1]&0xffff0000)>>16;
				status = MTK_VDM_SUCCESS;
				pr_err("%s mv = %dmv,ma = %dma\n",
					__func__, cap->vol, cap->cur);
			} else {
				pr_err("%s Failed\n", __func__);
				status = MTK_VDM_FAIL;
			}
			mutex_unlock(&vdm_par_lock);
			mtk_vdm_unlock();
			return status;
		}
		count--;
		mdelay(MTK_VDM_DELAY);
	}
	pr_err("%s Time out\n", __func__);
	atomic_dec(&vdm_event_flag);
	mtk_vdm_unlock();
	return status;

}

int mtk_get_ta_boundary_cap(struct tcpc_device *tcpc,
					struct mtk_vdm_ta_cap *cap)
{
	int count = MTK_VDM_COUNT;
	int status = MTK_VDM_FAIL;

	if (!vdm_inited) {
		pr_err("%s vdm not inited\n", __func__);
		return -EINVAL;
	}

	mtk_vdm_lock();
	atomic_inc(&vdm_event_flag); /* set flag = 1 */
	tcpm_vdm_request_rt7207(tcpc, 0x1021, 0);
	while (count) {
		if (atomic_read(&vdm_event_flag) == 0) {
			mutex_lock(&vdm_par_lock);
			if (vdm_success) {
				cap->vol = vdm_payload[1]&0x0000ffff;
				cap->cur = (vdm_payload[1]&0xffff0000)>>16;
				status = MTK_VDM_SUCCESS;
				pr_err("%s mv = %dmv,ma = %dma\n",
					__func__, cap->vol, cap->cur);
			} else {
				pr_err("%s Failed\n", __func__);
				status = MTK_VDM_FAIL;
			}
			mutex_unlock(&vdm_par_lock);
			mtk_vdm_unlock();
			return status;
		}
		count--;
		mdelay(MTK_VDM_DELAY);
	}
	pr_err("%s Time out\n", __func__);
	atomic_dec(&vdm_event_flag);
	mtk_vdm_unlock();
	return status;
}

int mtk_set_ta_cap(struct tcpc_device *tcpc, struct mtk_vdm_ta_cap *cap)
{
	int count = MTK_VDM_COUNT;
	int status = MTK_VDM_FAIL;
	uint32_t data;

	if (!vdm_inited) {
		pr_err("%s vdm not inited\n", __func__);
		return -EINVAL;
	}

	pr_err("%s set mv = %dmv,ma = %dma\n",
		__func__, cap->vol, cap->cur);

	mtk_vdm_lock();
	data = (cap->cur<<16)|cap->vol;
	atomic_inc(&vdm_event_flag); /* set flag = 1 */
	tcpm_vdm_request_rt7207(tcpc, 0x2022, data);
	while (count) {
		if (atomic_read(&vdm_event_flag) == 0) {
			mutex_lock(&vdm_par_lock);
			if (vdm_success) {
				cap->vol = vdm_payload[1]&0x0000ffff;
				cap->cur = (vdm_payload[1]&0xffff0000)>>16;
				status = MTK_VDM_SUCCESS;
				pr_err("%s mv = %dmv,ma = %dma\n",
					__func__, cap->vol, cap->cur);
			} else {
				pr_err("%s Failed\n", __func__);
				status = MTK_VDM_FAIL;
			}
			mutex_unlock(&vdm_par_lock);
			mtk_vdm_unlock();
			return status;
		}
		count--;
		mdelay(MTK_VDM_DELAY);
	}
	pr_err("%s Time out\n", __func__);
	atomic_dec(&vdm_event_flag);
	mtk_vdm_unlock();
	return status;
}

int mtk_get_ta_cap(struct tcpc_device *tcpc,
					struct mtk_vdm_ta_cap *cap)
{
	int count = MTK_VDM_COUNT;
	int status = MTK_VDM_FAIL;

	if (!vdm_inited) {
		pr_err("%s vdm not inited\n", __func__);
		return -EINVAL;
	}

	mtk_vdm_lock();
	atomic_inc(&vdm_event_flag); /* set flag = 1 */
	tcpm_vdm_request_rt7207(tcpc, 0x1022, 0);
	while (count) {
		if (atomic_read(&vdm_event_flag) == 0) {
			mutex_lock(&vdm_par_lock);
			if (vdm_success) {
				cap->vol = vdm_payload[1]&0x0000ffff;
				cap->cur = (vdm_payload[1]&0xffff0000)>>16;
				status = MTK_VDM_SUCCESS;
				pr_err("%s mv = %dmv,ma = %dma\n",
					__func__, cap->vol, cap->cur);
			} else {
				pr_err("%s Failed\n", __func__);
				status = MTK_VDM_FAIL;
			}
			mutex_unlock(&vdm_par_lock);
			mtk_vdm_unlock();
			return status;
		}
		count--;
		mdelay(MTK_VDM_DELAY);
	}
	pr_err("%s Time out\n", __func__);
	atomic_dec(&vdm_event_flag);
	mtk_vdm_unlock();
	return status;
}



int mtk_set_ta_uvlo(struct tcpc_device *tcpc, int mv)
{
	int count = MTK_VDM_COUNT;
	int status = MTK_VDM_FAIL;
	uint32_t data;

	if (!vdm_inited) {
		pr_err("%s vdm not inited\n", __func__);
		return -EINVAL;
	}

	mtk_vdm_lock();
	data = (mv&0x0000ffff);
	atomic_inc(&vdm_event_flag); /* set flag = 1 */
	tcpm_vdm_request_rt7207(tcpc, 0x2023, data);
	while (count) {
		if (atomic_read(&vdm_event_flag) == 0) {
			mutex_lock(&vdm_par_lock);
			if (vdm_success)
				status = MTK_VDM_SUCCESS;
			else {
				pr_err("%s Failed\n", __func__);
				status = MTK_VDM_FAIL;
			}
			pr_err("%s uvlo = %dmv\n", __func__,
				(vdm_payload[1]&0x0000ffff));
			mutex_unlock(&vdm_par_lock);
			mtk_vdm_unlock();
			return status;
		}
		count--;
		mdelay(MTK_VDM_DELAY);
	}
	pr_err("%s Time out\n", __func__);
	atomic_dec(&vdm_event_flag);
	mtk_vdm_unlock();
	return status;
}

int mtk_vdm_config_dfp(void)
{
	int ret = 0;
	unsigned int val;

	ret = dual_role_get_property(dr_usb, DUAL_ROLE_PROP_MODE, &val);
	if (ret < 0) {
		pr_err("%s get property mode fail\n", __func__);
		return -EINVAL;
	}
	if (val == DUAL_ROLE_PROP_MODE_DFP) {
		pr_info("%s Already DFP Mode\n", __func__);
		return ret;
	}

	tcpm_data_role_swap(tcpc);
	mdelay(50);
	ret = dual_role_get_property(dr_usb,
			DUAL_ROLE_PROP_MODE, &val);
	if (ret < 0) {
		pr_err("%s get property mode fail\n", __func__);
		return -EINVAL;
	}
	if (val == DUAL_ROLE_PROP_MODE_DFP) {
		pr_info("%s config DFP Mode Success\n", __func__);
		return ret;
	}
	pr_err("%s config DFP Mode Fail\n", __func__);
	return -EINVAL;
}

#ifdef CONFIG_DEBUG_FS
struct rt_debug_st {
	int id;
};

static struct dentry *debugfs_vdm_dir;
static struct dentry *debugfs_vdm_file[3];
static struct rt_debug_st vdm_dbg_data[3];

enum {
	RT7207_VDM_TEST,
};

static int de_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t de_read(struct file *file,
		char __user *user_buffer, size_t count, loff_t *position)
{
	struct rt_debug_st *db = file->private_data;
	char tmp[200] = {0};

	switch (db->id) {
	case RT7207_VDM_TEST:
		sprintf(tmp, "RT7207 VDM API Test\n");
		break;
	default:
		break;
	}
	return simple_read_from_buffer(user_buffer, count,
					position, tmp, strlen(tmp));
}

static ssize_t de_write(struct file *file,
		const char __user *user_buffer, size_t count, loff_t *position)
{
	struct rt_debug_st *db = file->private_data;
	char buf[200] = {0};
	unsigned long yo;
	struct mtk_vdm_ta_cap cap;
	int ret;

	simple_write_to_buffer(buf, sizeof(buf), position, user_buffer, count);
	ret = kstrtoul(buf, 16, &yo);

	mtk_direct_charge_vdm_init();
	if (mtk_vdm_config_dfp()) {
		pr_err("%s cannot config DFP mode\n", __func__);
		return count;
	}

	switch (db->id) {
	case RT7207_VDM_TEST:
		if (yo == 1)
			mtk_get_ta_id(tcpc);
		else if (yo == 2)
			mtk_get_ta_charger_status(tcpc);
		else if (yo == 3)
			mtk_get_ta_current_cap(tcpc, &cap);
		else if (yo == 4)
			mtk_get_ta_temperature(tcpc);
		else if (yo == 5)
			tcpm_set_direct_charge_en(tcpc, true);
		else if (yo == 6)
			tcpm_set_direct_charge_en(tcpc, false);
		else if (yo == 7) {
			cap.cur = 3000;
			cap.vol = 5000;
			mtk_set_ta_boundary_cap(tcpc, &cap);
		} else if (yo == 8) {
			cap.cur = 1200;
			cap.vol = 3800;
			mtk_set_ta_cap(tcpc, &cap);
		} else if (yo == 9)
			mtk_set_ta_uvlo(tcpc, 3000);
		else if (yo == 10)
			mtk_show_ta_info(tcpc);
		break;
	default:
		break;
	}
	return count;
}

static const struct file_operations debugfs_fops = {
	.open = de_open,
	.read = de_read,
	.write = de_write,
};
#endif /* CONFIG_DEBUG_FS */

int mtk_direct_charge_vdm_init(void)
{
	int ret = 0;

	if (!vdm_inited) {
		tcpc = tcpc_dev_get_by_name("type_c_port0");
		if (!tcpc) {
			pr_err("%s get tcpc device type_c_port0 fail\n",
								__func__);
			return -ENODEV;
		}

		dr_usb = dual_role_phy_instance_get_byname(
						"dual-role-type_c_port0");
		if (!dr_usb) {
			pr_err("%s get dual role instance fail\n", __func__);
			return -EINVAL;
		}

		mutex_init(&vdm_event_lock);
		mutex_init(&vdm_par_lock);
		wake_lock_init(&vdm_event_wake_lock,
			WAKE_LOCK_SUSPEND, "RT7208 VDM Wakelock");
		atomic_set(&vdm_event_flag, 0);

		vdm_nb.notifier_call = vdm_tcp_notifier_call;
		ret = register_tcp_dev_notifier(tcpc, &vdm_nb);
		if (ret < 0) {
			pr_err("%s: register tcpc notifier fail\n", __func__);
			return -EINVAL;
		}

		vdm_inited = true;

#ifdef CONFIG_DEBUG_FS
		debugfs_vdm_dir = debugfs_create_dir("rt7207_vdm_dbg", 0);
		if (!IS_ERR(debugfs_vdm_dir)) {
			vdm_dbg_data[0].id = RT7207_VDM_TEST;
			debugfs_vdm_file[0] = debugfs_create_file("test", 0666,
				debugfs_vdm_dir, (void *)&vdm_dbg_data[0],
				&debugfs_fops);
		}
#endif /* CONFIG_DEBUG_FS */
		pr_info("%s init OK!\n", __func__);
	}
	return 0;
}
EXPORT_SYMBOL(mtk_direct_charge_vdm_init);

int mtk_direct_charge_vdm_deinit(void)
{
	if (vdm_inited) {
		mutex_destroy(&vdm_event_lock);
		vdm_inited = false;
		if (!IS_ERR(debugfs_vdm_dir))
			debugfs_remove_recursive(debugfs_vdm_dir);
	}
	return 0;
}
EXPORT_SYMBOL(mtk_direct_charge_vdm_deinit);
