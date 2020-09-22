// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "md_cooling.h"

#define MD_COOLING_DEBUGFS_ENTRY_RO(name) \
static int md_cooling_##name##_open(struct inode *i, struct file *file) \
{ \
	return single_open(file, md_cooling_##name##_show, i->i_private); \
} \
\
static const struct file_operations md_cooling_##name##_fops = { \
	.owner = THIS_MODULE, \
	.open = md_cooling_##name##_open, \
	.read = seq_read, \
	.llseek = seq_lseek, \
	.release = single_release, \
}

#define MD_COOLING_DEBUGFS_ENTRY_RW(name) \
static int md_cooling_##name##_open(struct inode *i, struct file *file) \
{ \
	return single_open(file, md_cooling_##name##_show, i->i_private); \
} \
\
static const struct file_operations md_cooling_##name##_fops = { \
	.owner = THIS_MODULE, \
	.open = md_cooling_##name##_open, \
	.read = seq_read, \
	.write = md_cooling_##name##_write, \
	.llseek = seq_lseek, \
	.release = single_release, \
}

static struct dentry *mdc_debug_dir;
static int ca_ctrl;
static int pa_ctrl;
static int duty_ctrl;

static int md_cooling_status_show(struct seq_file *m, void *unused)
{
	struct md_cooling_device *md_cdev;
	int i, j;
	unsigned int pa_num = get_pa_num();

	seq_printf(m, "PA num = %d\n", pa_num);
	seq_printf(m, "MD status = %d\n", get_md_cooling_status());

	for (i = 0; i < NR_MD_COOLING_TYPE; i++) {
		for (j = 0; j < pa_num; j++) {
			md_cdev = get_md_cdev((enum md_cooling_type)i, j);
			if (!md_cdev)
				continue;

			seq_printf(m, "\n[%s, %d]\n", md_cdev->name,
				md_cdev->pa_id);
			seq_printf(m, "target/max = %ld/%ld\n",
				md_cdev->target_state, md_cdev->max_state);

			if (md_cdev->type == MD_COOLING_TYPE_TX_PWR)
				seq_printf(m, "throttle tx_pwr = %d/%d/%d\n",
					md_cdev->throttle_tx_power[0],
					md_cdev->throttle_tx_power[1],
					md_cdev->throttle_tx_power[2]);
		}
	}

	return 0;
}
MD_COOLING_DEBUGFS_ENTRY_RO(status);

static int md_cooling_duty_ctrl_show(struct seq_file *m, void *unused)
{
	seq_printf(m, "duty ctrl is %s\n", duty_ctrl ? "ON" : "OFF");

	return 0;
}

static ssize_t md_cooling_duty_ctrl_write(struct file *flip,
			const char *ubuf, size_t cnt, loff_t *data)
{
	int ret = cnt, msg, no_ims, active, suspend, enable;
	char *buf;

	if (cnt > 32)
		return -ENOMEM;

	buf = kmalloc(cnt + 1, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	if (copy_from_user(buf, ubuf, cnt)) {
		ret = -EFAULT;
		goto err;
	}
	buf[cnt] = '\0';

	if ((sscanf(buf, "%d %d %d", &no_ims, &active, &suspend) != 3)) {
		ret = -EINVAL;
		goto err;
	}

	if (get_md_cooling_status() != MD_LV_THROTTLE_DISABLED) {
		ret = -EBUSY;
		goto err;
	}

	if (no_ims) {
		/* set active/suspend=1/255 for no IMS case */
		msg = duty_ctrl_to_tmc_msg(1, 255, 0);
		enable = 1;
	} else if (active >= 1 && active <= 255
		&& suspend >= 1 && suspend <= 255) {
		msg = duty_ctrl_to_tmc_msg(active, suspend, 1);
		enable = 1;
	} else {
		msg = TMC_THROTTLE_DISABLE_MSG;
		enable = 0;
	}

	if (send_throttle_msg(msg)) {
		ret = -EBUSY;
		goto err;
	}

	duty_ctrl = enable;

err:
	kfree(buf);

	return ret;
}
MD_COOLING_DEBUGFS_ENTRY_RW(duty_ctrl);

static int md_cooling_ca_ctrl_show(struct seq_file *m, void *unused)
{
	seq_printf(m, "CA ctrl is %s\n", ca_ctrl ? "ON" : "OFF");

	return 0;
}

static ssize_t md_cooling_ca_ctrl_write(struct file *flip,
			const char *ubuf, size_t cnt, loff_t *data)
{
	int val;

	if (kstrtoint_from_user(ubuf, cnt, 0, &val))
		return -EFAULT;

	if (get_md_cooling_status() != MD_LV_THROTTLE_DISABLED)
		return -EBUSY;

	if (send_throttle_msg(ca_ctrl_to_tmc_msg(val)))
		return -EBUSY;

	ca_ctrl = val;

	return cnt;
}
MD_COOLING_DEBUGFS_ENTRY_RW(ca_ctrl);

static int md_cooling_pa_ctrl_show(struct seq_file *m, void *unused)
{
	seq_printf(m, "PA ctrl is %s\n", pa_ctrl ? "ON" : "OFF");

	return 0;
}

static ssize_t md_cooling_pa_ctrl_write(struct file *flip,
			const char *ubuf, size_t cnt, loff_t *data)
{
	int val;

	if (kstrtoint_from_user(ubuf, cnt, 0, &val))
		return -EFAULT;

	if (get_md_cooling_status() != MD_LV_THROTTLE_DISABLED)
		return -EBUSY;

	if (send_throttle_msg(pa_ctrl_to_tmc_msg(val)))
		return -EBUSY;

	pa_ctrl = val;

	return cnt;
}
MD_COOLING_DEBUGFS_ENTRY_RW(pa_ctrl);

static int md_cooling_update_tx_pwr_show(struct seq_file *m, void *unused)
{
	struct md_cooling_device *md_cdev;
	int i;

	for (i = 0; i < get_pa_num(); i++) {
		md_cdev = get_md_cdev(MD_COOLING_TYPE_TX_PWR, i);
		if (!md_cdev)
			continue;

		seq_printf(m, "PA%d throttle tx_pwr = %d/%d/%d\n",
			i,
			md_cdev->throttle_tx_power[0],
			md_cdev->throttle_tx_power[1],
			md_cdev->throttle_tx_power[2]);
	}

	return 0;
}

static ssize_t md_cooling_update_tx_pwr_write(struct file *flip,
			const char *ubuf, size_t cnt, loff_t *data)
{
	int ret = cnt, id, tx_pwr_lv1, tx_pwr_lv2, tx_pwr_lv3;
	unsigned int tx_pwr[MAX_NUM_TX_PWR_STATE];
	char *buf;

	if (cnt > 32)
		return -ENOMEM;

	buf = kmalloc(cnt + 1, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	if (copy_from_user(buf, ubuf, cnt)) {
		ret = -EFAULT;
		goto err;
	}
	buf[cnt] = '\0';

	if ((sscanf(buf, "%d %d %d %d", &id, &tx_pwr_lv1,
		&tx_pwr_lv2, &tx_pwr_lv3) != (MAX_NUM_TX_PWR_STATE + 1))) {
		ret = -EINVAL;
		goto err;
	}

	if (id >= get_pa_num()) {
		pr_err("Invalid PA id(%d)\n", id);
		ret = -EINVAL;
		goto err;
	}

	tx_pwr[0] = tx_pwr_lv1;
	tx_pwr[1] = tx_pwr_lv2;
	tx_pwr[2] = tx_pwr_lv3;

	update_throttle_power(id, tx_pwr);

err:
	kfree(buf);

	return ret;
}
MD_COOLING_DEBUGFS_ENTRY_RW(update_tx_pwr);

int md_cooling_debugfs_init(void)
{
	mdc_debug_dir = debugfs_create_dir("md_cooling", NULL);
	if (!mdc_debug_dir) {
		pr_err("failed to create mdc_debug_dir\n");
		goto failed;
	}

	if (!debugfs_create_file("status", 0440,
		mdc_debug_dir, NULL, &md_cooling_status_fops))
		goto failed;
	if (!debugfs_create_file("duty_ctrl", 0640,
		mdc_debug_dir, NULL, &md_cooling_duty_ctrl_fops))
		goto failed;
	if (!debugfs_create_file("ca_ctrl", 0640,
		mdc_debug_dir, NULL, &md_cooling_ca_ctrl_fops))
		goto failed;
	if (!debugfs_create_file("pa_ctrl", 0640,
		mdc_debug_dir, NULL, &md_cooling_pa_ctrl_fops))
		goto failed;
	if (!debugfs_create_file("update_tx_pwr", 0640,
		mdc_debug_dir, NULL, &md_cooling_update_tx_pwr_fops))
		goto failed;

	return 0;

failed:
	return -ENODEV;
}

void md_cooling_debugfs_exit(void)
{
	debugfs_remove_recursive(mdc_debug_dir);
}

