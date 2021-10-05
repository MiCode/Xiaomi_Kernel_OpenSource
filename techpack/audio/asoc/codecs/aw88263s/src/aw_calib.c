/*
 * aw_calibration.c cali_module
 *
 *
 * Copyright (c) 2020 AWINIC Technology CO., LTD
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 *  Author: Nick Li <liweilei@awinic.com.cn>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
/*#define DEBUG*/
#include <linux/module.h>
#include <asm/ioctls.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "aw882xx.h"
#include "aw_dsp.h"
#include "aw_log.h"
#include "aw_calib.h"

static bool is_single_cali = false; /*if mutli_dev cali false, single dev true*/

static const char *cali_str[CALI_STR_MAX] = {"none", "start_cali", "cali_re",
	"cali_f0", "store_re", "show_re", "show_r0", "show_cali_f0", "show_f0",
	"show_te", "show_st", "dev_sel", "get_ver", "get_dev_num"
};

static char *ch_name[AW_DEV_CH_MAX] = {"pri_l", "pri_r", "sec_l", "sec_r"};
static unsigned int g_cali_re_time = AW_CALI_RE_DEFAULT_TIMER;
static unsigned int g_msic_wr_flag = CALI_STR_NONE;
static unsigned int g_dev_select = AW_DEV_CH_PRI_L;
static unsigned int g_cali_status = false;
static struct miscdevice *g_misc_dev = NULL;
static DEFINE_MUTEX(g_cali_lock);


#ifdef AW_CALI_STORE_EXAMPLE
 /*write cali to persist file example*/
#define AWINIC_CALI_FILE  "/mnt/vendor/persist/factory/audio/aw_cali.bin"
#define AW_INT_DEC_DIGIT 10

static int aw_cali_write_cali_re_to_file(int32_t cali_re, int channel)
{
	struct file *fp = NULL;
	char buf[50] = {0};
	loff_t pos = 0;
	mm_segment_t fs;

	fp = filp_open(AWINIC_CALI_FILE, O_RDWR | O_CREAT, 0644);
	if (IS_ERR(fp)) {
		pr_err("%s:channel:%d open %s failed!\n",
		__func__, channel, AWINIC_CALI_FILE);
		return -EINVAL;
	}

	pos = AW_INT_DEC_DIGIT * channel;

	snprintf(buf, sizeof(buf), "%10d", cali_re);

	fs = get_fs();
	set_fs(KERNEL_DS);

//	kernel_write(fp, buf, strlen(buf), &pos);

	set_fs(fs);

	pr_info("%s: channel:%d buf:%s cali_re:%d\n",
		__func__, channel, buf, cali_re);

	filp_close(fp, NULL);
	return 0;
}

static int aw_cali_get_read_cali_re(int32_t *cali_re, int channel)
{
	struct file *fp = NULL;
	/*struct inode *node;*/
	int f_size;
	char *buf = NULL;
	int32_t int_cali_re = 0;
	loff_t pos = 0;
	mm_segment_t fs;

	fp = filp_open(AWINIC_CALI_FILE, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		pr_err("%s:channel:%d open %s failed!\n",
			__func__, channel, AWINIC_CALI_FILE);
		return -EINVAL;
	}

	pos = AW_INT_DEC_DIGIT * channel;

	/*node = fp->f_dentry->d_inode;*/
	/*f_size = node->i_size;*/
	f_size = AW_INT_DEC_DIGIT;

	buf = kzalloc(f_size + 1, GFP_ATOMIC);
	if (!buf) {
		pr_err("%s: channel:%d malloc mem %d failed!\n",
			 __func__, channel, f_size);
		filp_close(fp, NULL);
		return -EINVAL;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);

//	kernel_read(fp, buf, f_size, &pos);

	set_fs(fs);

	if (sscanf(buf, "%d", &int_cali_re) == 1)
		*cali_re = int_cali_re;
	else
		*cali_re = AW_ERRO_CALI_VALUE;

	pr_info("%s: channel:%d buf:%s int_cali_re: %d\n",
		__func__, channel, buf, int_cali_re);

	kfree(buf);
	buf = NULL;
	filp_close(fp, NULL);

	return  0;
}
#endif

/*custom need add to set/get cali_re form/to nv*/
int aw_cali_write_re_to_nvram(int32_t cali_re, int32_t channel)
{
#ifdef AW_CALI_STORE_EXAMPLE
	if (channel >= AW_DEV_CH_MAX) {
		pr_err("%s: unsupported channel [%d] \n", __func__, channel);
		return -EINVAL;
	}
	return aw_cali_write_cali_re_to_file(cali_re, channel);
#else
	return -EBUSY;
#endif
}

int aw_cali_read_re_from_nvram(int32_t *cali_re, int32_t channel)
{
	/*custom add, if success return value is 0 , else -1*/
#ifdef AW_CALI_STORE_EXAMPLE
	if (channel >= AW_DEV_CH_MAX) {
		pr_err("%s: unsupported channel [%d] \n", __func__, channel);
		return -EINVAL;
	}
	return aw_cali_get_read_cali_re(cali_re, channel);
#else
	return -EBUSY;
#endif
}

int aw_cali_store_cali_re(struct aw_device *aw_dev, int32_t re)
{
	int ret = 0;

	if ((re >= aw_dev->re_min) && (re <= aw_dev->re_max)) {
		aw_dev->cali_desc.cali_re = re;
		ret = aw_cali_write_re_to_nvram(re, aw_dev->channel);
		if (ret)
			aw_dev_err(aw_dev->dev, "write re to nvram failed!");
	} else {
		aw_dev_err(aw_dev->dev, "invalid cali re %d !", re);
		return -EINVAL;
	}

	aw_dev_info(aw_dev->dev, "store cali re is %d", re);
	return ret;
}

/*********************************aw_cali_sevice*********************************************************/
static void aw_cali_svc_set_cali_status(struct aw_device *aw_dev, int status)
{
	if (status)
		g_cali_status = true;
	else
		g_cali_status = false;

	aw_dev_info(aw_dev->dev, "cali %s",
		(status == 0) ? ("disable") : ("enable"));
}

int aw_cali_svc_get_cali_status(void)
{
	return g_cali_status;
}

static int aw_cali_svc_dev_get_re(struct aw_device *aw_dev)
{
	int32_t re[AW_CALI_READ_TIMES];
	int ret, i;
	int32_t sum = 0;

	for (i = 0; i < AW_CALI_READ_TIMES; i++) {
		ret = aw_dsp_read_r0(aw_dev, &re[i]);
		if (ret) {
			aw_dev_err(aw_dev->dev, "get re failed!");
			return ret;
		}
		sum += re[i];
		usleep_range(AW_10000_US, AW_10000_US + 10);
	}
	re[0] = sum / AW_CALI_READ_TIMES;

	aw_dev->cali_desc.cali_re = re[0];

	if ((aw_dev->cali_desc.cali_re >= aw_dev->re_min) &&
			(aw_dev->cali_desc.cali_re <= aw_dev->re_max)) {
		ret = aw_cali_write_re_to_nvram(aw_dev->cali_desc.cali_re, aw_dev->channel);
		if (ret)
			aw_dev_err(aw_dev->dev, "write re to nvram failed!");
	}
	return 0;
}

static int aw_cali_svc_dev_get_f0(struct aw_device *aw_dev)
{
	int32_t f0[AW_CALI_READ_TIMES];
	int ret, i;
	int32_t sum = 0;

	for (i = 0; i < AW_CALI_READ_TIMES; i++) {
		ret = aw_dsp_read_f0(aw_dev, &f0[i]);
		if (ret) {
			aw_dev_err(aw_dev->dev, "get f0 failed!");
			return ret;
		}
		sum += f0[i];
		usleep_range(AW_10000_US, AW_10000_US + 10);
	}
	f0[0] = sum / AW_CALI_READ_TIMES;

	aw_dev->cali_desc.cali_f0 = f0[0];
	return 0;
}

static int aw_cali_svc_dev_get_f0_q(struct aw_device *aw_dev)
{
	int32_t f0[AW_CALI_READ_TIMES], q[AW_CALI_READ_TIMES];
	int ret, i;
	int32_t sum_f0 = 0, sum_q = 0;

	for (i = 0; i < AW_CALI_READ_TIMES; i++) {
		ret = aw_dsp_read_f0_q(aw_dev, &f0[i], &q[i]);
		if (ret) {
			aw_dev_err(aw_dev->dev, "get f0 failed!");
			return ret;
		}
		sum_f0 += f0[i];
		sum_q += q[i];
		usleep_range(AW_10000_US, AW_10000_US + 10);
	}
	f0[0] = sum_f0 / AW_CALI_READ_TIMES;
	q[0] = sum_q / AW_CALI_READ_TIMES;
	aw_dev->cali_desc.cali_f0 = f0[0];
	aw_dev->cali_desc.cali_q = q[0];
	return 0;
}


static int aw_cali_svc_dev_cali_mode_en(struct aw_device *aw_dev, int type, bool is_enable, unsigned int flag)
{
	int ret;

	/* open cali mode */
	if (is_enable) {
		aw_cali_svc_set_cali_status(aw_dev, true);

		if (type == CALI_TYPE_RE) {
			if (flag & CALI_OPS_HMUTE) {
				ret = aw_dsp_hmute_en(aw_dev, true);
				if (ret < 0)
					return ret;
			}
		} else {
			if (flag & CALI_OPS_NOISE) {
				ret = aw_dsp_noise_en(aw_dev, true);
				if (ret < 0)
					return ret;
			}
		}

		ret = aw_dsp_cali_en(aw_dev, true);
		if (ret < 0)
			return ret;
	} else {
		aw_dsp_cali_en(aw_dev, false);

		if (type == CALI_TYPE_RE) {
			/*close mute*/
			if (flag & CALI_OPS_HMUTE)
				aw_dsp_hmute_en(aw_dev, false);
		} else {
			/*close mute*/
			if (flag & CALI_OPS_NOISE)
				aw_dsp_noise_en(aw_dev, false);
		}

		/*close cali mode*/
		aw_cali_svc_set_cali_status(aw_dev, false);
	}
	return 0;
}

static int aw_cali_svc_dev_cali_re(struct aw_device *aw_dev, unsigned int flag)
{
	int ret;

	ret = aw_cali_svc_dev_cali_mode_en(aw_dev, CALI_TYPE_RE, true, flag);
	if (ret < 0)
		goto exit;

	/*wait time*/
	msleep(g_cali_re_time);

	/*dev get echo cali re*/
	ret = aw_cali_svc_dev_get_re(aw_dev);
	if (ret < 0)
		goto exit;
exit:
	aw_cali_svc_dev_cali_mode_en(aw_dev, CALI_TYPE_RE, false, flag);

	return ret;
}

static int aw_cali_svc_devs_cali_re(struct aw_device *aw_dev, unsigned int flag)
{
	struct list_head *dev_list;
	struct list_head *pos;
	struct aw_device *local_dev;
	int ret;

	/* get dev list */
	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		aw_dev_err(aw_dev->dev, "get dev list failed");
		return ret;
	}

	/* enable all dev cali mode */
	list_for_each (pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		ret = aw_cali_svc_dev_cali_mode_en(local_dev, CALI_TYPE_RE, true, flag);
		if (ret < 0)
			goto exit;
	}

	/* wait time */
	msleep(g_cali_re_time);

	/* dev get echo cali re */
	list_for_each(pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		ret = aw_cali_svc_dev_get_re(local_dev);
		if (ret < 0)
			goto exit;
	}

exit:
	list_for_each (pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		/* close cali mode */
		aw_cali_svc_dev_cali_mode_en(local_dev, CALI_TYPE_RE, false, flag);
	}

	return ret;
}

static int aw_cali_svc_cali_re(struct aw_device *aw_dev, bool is_single, unsigned int flag)
{
	if (is_single)
		return aw_cali_svc_dev_cali_re(aw_dev, flag);
	else
		return aw_cali_svc_devs_cali_re(aw_dev, flag);
	return 0;
}

static int aw_cali_svc_dev_cali_f0(struct aw_device *aw_dev, unsigned int flag)
{
	int ret;

	ret = aw_cali_svc_dev_cali_mode_en(aw_dev, CALI_TYPE_F0, true, flag);
	if (ret < 0)
		goto exit;

	/*wait time*/
	msleep(5 * 1000);

	/*dev get echo cali re*/
	ret = aw_cali_svc_dev_get_f0(aw_dev);
	if (ret < 0)
		goto exit;

exit:
	aw_cali_svc_dev_cali_mode_en(aw_dev, CALI_TYPE_F0, false, flag);

	return ret;
}

static int aw_cali_svc_devs_cali_f0(struct aw_device *aw_dev, unsigned int flag)
{
	struct list_head *dev_list;
	struct list_head *pos = NULL;
	struct aw_device *local_dev;
	int ret;

	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		aw_dev_err(aw_dev->dev, " get dev list failed");
		return ret;
	}

	list_for_each (pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		/* set mode status */
		ret = aw_cali_svc_dev_cali_mode_en(local_dev, CALI_TYPE_F0, true, flag);
		if (ret < 0)
			goto exit;
	}

	/*wait time*/
	msleep(5 * 1000);

	/*dev get echo cali re*/
	list_for_each(pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		ret = aw_cali_svc_dev_get_f0(local_dev);
		if (ret < 0)
			goto exit;
	}

exit:
	list_for_each (pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		/*close cali*/
		aw_cali_svc_dev_cali_mode_en(local_dev, CALI_TYPE_F0, false, flag);
	}

	return ret;
}

static int aw_cali_svc_cali_f0(struct aw_device *aw_dev, bool is_single, unsigned int flag)
{
	if (is_single)
		return aw_cali_svc_dev_cali_f0(aw_dev, flag);
	else
		return aw_cali_svc_devs_cali_f0(aw_dev, flag);
	return 0;
}

static int aw_cali_svc_dev_cali_f0_q(struct aw_device *aw_dev, unsigned int flag)
{
	int ret;

	ret = aw_cali_svc_dev_cali_mode_en(aw_dev, CALI_TYPE_F0, true, flag);
	if (ret < 0)
		goto exit;


	/*wait time*/
	msleep(5 * 1000);

	/*dev get echo cali re*/
	ret = aw_cali_svc_dev_get_f0_q(aw_dev);
	if (ret < 0)
		goto exit;

exit:
	aw_cali_svc_dev_cali_mode_en(aw_dev, CALI_TYPE_F0, false, flag);

	return ret;

}

static int aw_cali_svc_devs_cali_f0_q(struct aw_device *aw_dev, unsigned int flag)
{
	struct list_head *dev_list;
	struct list_head *pos = NULL;
	struct aw_device *local_dev;
	int ret;

	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		aw_dev_err(aw_dev->dev, "get dev list failed ");
		return ret;
	}


	list_for_each (pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		/*set mode status*/
		ret = aw_cali_svc_dev_cali_mode_en(local_dev, CALI_TYPE_F0, true, flag);
		if (ret < 0)
			goto exit;
	}

	/*wait time*/
	msleep(5 * 1000);

	/*dev get echo cali re*/
	list_for_each(pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		ret = aw_cali_svc_dev_get_f0_q(local_dev);
		if (ret < 0)
			goto exit;
	}

exit:
	list_for_each (pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		aw_cali_svc_dev_cali_mode_en(local_dev, CALI_TYPE_F0, false, flag);
	}

	return ret;
}

static int aw_cali_svc_cali_f0_q(struct aw_device *aw_dev, bool is_single, unsigned int flag)
{
	if (is_single)
		return aw_cali_svc_dev_cali_f0_q(aw_dev, flag);
	else
		return aw_cali_svc_devs_cali_f0_q(aw_dev, flag);

	return 0;
}

static int aw_cali_svc_cali_re_f0(struct aw_device *aw_dev, bool is_single, unsigned int flag)
{
	int ret;

	ret = aw_cali_svc_cali_re(aw_dev, is_single, flag);
	if (ret)
		return ret;

	ret = aw_cali_svc_cali_f0(aw_dev, is_single, flag);
	if (ret)
		return ret;

	return 0;
}

static int aw_cali_svc_cali_re_f0_q(struct aw_device *aw_dev, bool is_single, unsigned int flag)
{
	int ret;

	ret = aw_cali_svc_cali_re(aw_dev, is_single, flag);
	if (ret)
		return ret;

	ret = aw_cali_svc_cali_f0_q(aw_dev, is_single, flag);
	if (ret)
		return ret;

	return 0;
}

int aw_cali_svc_cali_cmd(struct aw_device *aw_dev, int cali_cmd, bool is_single, unsigned int flag)
{
	switch (cali_cmd) {
	case AW_CALI_CMD_RE:
		return aw_cali_svc_cali_re(aw_dev, is_single, flag);
	case AW_CALI_CMD_F0:
		return aw_cali_svc_cali_f0(aw_dev, is_single, flag);
	case AW_CALI_CMD_F0_Q:
		return aw_cali_svc_cali_f0_q(aw_dev, is_single, flag);
	case AW_CALI_CMD_RE_F0:
		return aw_cali_svc_cali_re_f0(aw_dev, is_single, flag);
	case AW_CALI_CMD_RE_F0_Q:
		return aw_cali_svc_cali_re_f0_q(aw_dev, is_single, flag);
	default:
		aw_dev_err(aw_dev->dev, "unsupported cmd %d", cali_cmd);
		return -EINVAL;
	}
	return 0;
}

int aw_cali_svc_get_devs_cali_re(struct aw_device *aw_dev, int32_t *re_buf, int num)
{
	struct list_head *dev_list;
	struct list_head *pos = NULL;
	struct aw_device *local_dev;
	int ret, cnt = 0;

	/* get dev list */
	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		aw_dev_err(aw_dev->dev, "get dev list failed");
		return ret;
	}

	list_for_each(pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		if (local_dev->channel < num) {
			re_buf[local_dev->channel] = local_dev->cali_desc.cali_re;
			cnt++;
		} else {
			aw_dev_err(aw_dev->dev, "channel num[%d] overflow buf num[%d]",
						local_dev->channel, num);
			return -EINVAL;
		}
	}

	return cnt;
}

int aw_cali_svc_get_devs_r0(struct aw_device *aw_dev, int32_t *re_buf, int num)
{
	struct list_head *dev_list;
	struct list_head *pos = NULL;
	struct aw_device *local_dev;
	int ret, cnt = 0;

	/* get dev list */
	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		aw_dev_err(aw_dev->dev, "get dev list failed");
		return ret;
	}

	list_for_each(pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		if (local_dev->channel < num) {
			ret = aw_dsp_read_r0(local_dev, &re_buf[local_dev->channel]);
			if (ret) {
				aw_dev_err(local_dev->dev, "get re failed!");
				return ret;
			}
			cnt++;
		} else {
			aw_dev_err(aw_dev->dev, "channel num[%d] overflow buf num[%d] ",
						 local_dev->channel, num);
		}
	}
	return cnt;
}

int aw_cali_svc_get_devs_te(struct aw_device *aw_dev, int32_t *te_buf, int num)
{
	struct list_head *dev_list;
	struct list_head *pos = NULL;
	struct aw_device *local_dev;
	int ret, cnt = 0;

	/* get dev list */
	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		aw_dev_err(aw_dev->dev, "get dev list failed");
		return ret;
	}

	list_for_each (pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		if (local_dev->channel < num) {
			ret = aw_dsp_read_te(local_dev, &te_buf[local_dev->channel]);
			if (ret) {
				aw_dev_err(local_dev->dev, "get re failed!");
				return ret;
			}
			cnt++;
		} else {
			aw_dev_err(aw_dev->dev, "channel num[%d] overflow buf num[%d]",
					local_dev->channel, num);
		}
	}

	return cnt;
}

int aw_cali_svc_get_devs_st(struct aw_device *aw_dev, int32_t *st_buf, int num)
{
	struct list_head *dev_list;
	struct list_head *pos = NULL;
	struct aw_device *local_dev;
	int ret, cnt = 0;

	/*get dev list*/
	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		aw_dev_err(aw_dev->dev, "get dev list failed");
		return ret;
	}

	list_for_each(pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		if (local_dev->channel < num) {
			ret = aw_dsp_read_st(local_dev, &st_buf[local_dev->channel << 1], &st_buf[(local_dev->channel << 1) + 1]);
			if (ret) {
				aw_dev_err(local_dev->dev, "get re failed!");
				return ret;
			}
			cnt++;
		} else {
			aw_dev_err(aw_dev->dev, "channel num[%d] overflow buf num[%d]",
						local_dev->channel, num);
		}
	}
	return cnt;
}

int aw_cali_svc_get_devs_cali_f0(struct aw_device *aw_dev, int32_t *f0_buf, int num)
{
	struct list_head *dev_list;
	struct list_head *pos = NULL;
	struct aw_device *local_dev;
	int ret, cnt = 0;

	/*get dev list*/
	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		aw_dev_err(aw_dev->dev, "get dev list failed");
		return ret;
	}

	list_for_each(pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		if (local_dev->channel < num) {
			f0_buf[local_dev->channel] = local_dev->cali_desc.cali_f0;
			cnt++;
		} else {
			aw_dev_err(aw_dev->dev, "channel num[%d] overflow buf num[%d]",
						local_dev->channel, num);
		}
	}

	return cnt;
}

int aw_cali_svc_get_devs_f0(struct aw_device *aw_dev, int32_t *f0_buf, int num)
{
	struct list_head *dev_list;
	struct list_head *pos = NULL;
	struct aw_device *local_dev;
	int ret, cnt = 0;

	/* get dev list */
	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		aw_dev_err(aw_dev->dev, "get dev list failed");
		return ret;
	}

	list_for_each(pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		if (local_dev->channel < num) {
			ret = aw_dsp_read_f0(local_dev, &f0_buf[local_dev->channel]);
			if (ret) {
				aw_dev_err(local_dev->dev, "get re failed!");
				return ret;
			}
			cnt++;
		} else {
			aw_dev_err(aw_dev->dev, "channel num[%d] overflow buf num[%d]",
					local_dev->channel, num);
		}
	}
	return cnt;
}

int aw_cali_svc_get_devs_cali_f0_q(struct aw_device *aw_dev,
			int32_t *f0_buf, int32_t *q_buf, int num)
{
	struct list_head *dev_list;
	struct list_head *pos = NULL;
	struct aw_device *local_dev;
	int ret, cnt = 0;

	/*get dev list*/
	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		aw_dev_err(aw_dev->dev, "get dev list failed");
		return ret;
	}

	list_for_each(pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		if (local_dev->channel < num) {
			f0_buf[local_dev->channel] = local_dev->cali_desc.cali_f0;
			q_buf[local_dev->channel] = local_dev->cali_desc.cali_q;
			cnt++;
		} else {
			aw_dev_err(aw_dev->dev, "channel num[%d] overflow buf num[%d]",
					local_dev->channel, num);
		}
	}
	return cnt;
}

static int aw_cali_svc_set_devs_re_str(struct aw_device *aw_dev, const char *re_str)
{
	struct list_head *dev_list, *pos = NULL;
	struct aw_device *local_dev;
	int ret, cnt = 0;
	int re_data[AW_DEV_CH_MAX] = {0};

	/* get dev list */
	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		aw_dev_err(aw_dev->dev, "get dev list failed");
		return ret;
	}

	ret = sscanf(re_str, "pri_l:%d pri_r:%d sec_l:%d sec_r:%d",
					&re_data[AW_DEV_CH_PRI_L],
					&re_data[AW_DEV_CH_PRI_R],
					&re_data[AW_DEV_CH_SEC_L],
					&re_data[AW_DEV_CH_SEC_R]);

	if (ret <= 0) {
		aw_dev_err(aw_dev->dev, "unsupport str[%s]", re_str);
		return ret;
	}

	aw_dev_dbg(aw_dev->dev, "pri_l:%d pri_r:%d sec_l:%d sec_r:%d",
					re_data[AW_DEV_CH_PRI_L],
					re_data[AW_DEV_CH_PRI_R],
					re_data[AW_DEV_CH_SEC_L],
					re_data[AW_DEV_CH_SEC_R]);

	list_for_each (pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		if (local_dev->channel < AW_DEV_CH_MAX) {
			aw_cali_store_cali_re(local_dev, re_data[local_dev->channel]);
			cnt++;
		}
	}
	return cnt;

}

static int aw_cali_svc_get_cmd_form_str(struct aw_device *aw_dev, const char *buf)
{
	int i;

	for (i = 0; i < CALI_STR_MAX; i++) {
		if (!strncmp(cali_str[i], buf, strlen(cali_str[i]))) {
			break;
		}
	}

	if (i == CALI_STR_MAX) {
		aw_dev_err(aw_dev->dev, "supported cmd [%s]!", buf);
		return -EINVAL;
	}

	aw_dev_info(aw_dev->dev, "find str [%s]", cali_str[i]);
	return i;
}

/*********************************aw_cali_sevice*********************************************************/


/*****************************attr   start***************************************************/
static ssize_t aw_cali_attr_time_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	uint32_t time;
	struct aw882xx *aw882xx = dev_get_drvdata(dev);
	struct aw_device *aw_dev = aw882xx->aw_pa;

	ret = kstrtoint(buf, 0, &time);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read buf %s failed\n",
			buf);
		return ret;
	}

	if (time < 400) {
		aw_dev_err(aw_dev->dev, "time:%d is too short, no set",
			time);
		return -EINVAL;
	}

	g_cali_re_time = time;
	aw_dev_dbg(aw_dev->dev, "time:%d", time);

	return count;
}

static ssize_t aw_cali_attr_time_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf+len, PAGE_SIZE-len,
		"time: %d \n", g_cali_re_time);

	return len;
}

static ssize_t aw_cali_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct aw882xx *aw882xx = dev_get_drvdata(dev);
	struct aw_device *aw_dev = aw882xx->aw_pa;
	int ret;

	ret = aw_cali_svc_get_cmd_form_str(aw_dev, buf);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "supported cmd [%s]!", buf);
		return -EPERM;
	}

	if (ret == CALI_STR_CALI_RE_F0) {
		aw_cali_svc_cali_cmd(aw_dev, AW_CALI_CMD_RE_F0,
			is_single_cali, CALI_OPS_HMUTE|CALI_OPS_NOISE);
		return count;
	} else if (ret == CALI_STR_CALI_RE) {
		aw_cali_svc_cali_cmd(aw_dev, AW_CALI_CMD_RE,
			is_single_cali, CALI_OPS_HMUTE);
		return count;
	} else if (ret == CALI_STR_CALI_F0) {
		aw_cali_svc_cali_cmd(aw_dev, AW_CALI_CMD_F0,
			is_single_cali, CALI_OPS_NOISE);
		return count;
	} else {
		aw_dev_err(aw_dev->dev, "supported cmd [%s]!", buf);
	}

	return -EPERM;

}

static ssize_t aw_cali_attr_re_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct aw882xx *aw882xx = dev_get_drvdata(dev);
	struct aw_device *aw_dev = aw882xx->aw_pa;
	int ret;
	int data;

	if (is_single_cali) {
		ret = kstrtoint(buf, 0, &data);
		if (ret < 0) {
			aw_dev_err(aw882xx->dev, " read buf %s failed", buf);
			return ret;
		}
		aw_cali_store_cali_re(aw_dev, data);
	} else {
		ret = aw_cali_svc_set_devs_re_str(aw_dev, buf);
		if (ret <= 0) {
			aw_dev_err(aw_dev->dev, "set re str %s failed", buf);
			return -EPERM;
		}
	}

	return count;
}

static ssize_t aw_cali_attr_re_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret, i;
	struct aw882xx *aw882xx = dev_get_drvdata(dev);
	struct aw_device *aw_dev = aw882xx->aw_pa;
	ssize_t len = 0;
	int32_t cali_re[AW_DEV_CH_MAX] = {0};

	ret = aw_cali_svc_cali_cmd(aw_dev, AW_CALI_CMD_RE, is_single_cali, CALI_OPS_HMUTE);
	if (ret < 0) {
		len += snprintf(buf+len, PAGE_SIZE-len, "cali Re cmd failed\n");
		return len;
	}

	if (is_single_cali) {
		len += snprintf(buf+len, PAGE_SIZE-len, "%d\n", aw_dev->cali_desc.cali_re);
	} else {
		ret = aw_cali_svc_get_devs_cali_re(aw_dev, cali_re, AW_DEV_CH_MAX);
		if (ret <= 0) {
			aw_dev_err(aw_dev->dev, "get re failed ");
			len += snprintf(buf+len, PAGE_SIZE-len, "get re failed\n");
		} else {
			for (i = 0; i < ret; i++)
				len += snprintf(buf+len, PAGE_SIZE-len, "%s:%d mOhms ", ch_name[i], cali_re[i]);
			len += snprintf(buf+len, PAGE_SIZE-len, " \n");
		}
	}

	return len;
}

static ssize_t aw_cali_attr_f0_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct aw882xx *aw882xx = dev_get_drvdata(dev);
	struct aw_device *aw_dev = aw882xx->aw_pa;

	aw_cali_svc_cali_cmd(aw_dev, AW_CALI_CMD_F0, is_single_cali, CALI_OPS_NOISE);
	return count;
}

static ssize_t aw_cali_attr_f0_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret, i;
	struct aw882xx *aw882xx = dev_get_drvdata(dev);
	struct aw_device *aw_dev = aw882xx->aw_pa;
	ssize_t len = 0;
	int32_t cali_f0[AW_DEV_CH_MAX] = {0};

	ret = aw_cali_svc_cali_cmd(aw_dev, AW_CALI_CMD_F0, is_single_cali, CALI_OPS_NOISE);
	if (ret < 0) {
		len += snprintf(buf+len, PAGE_SIZE-len, "cali f0 cmd failed\n");
		return len;
	}

	if (is_single_cali) {
		len += snprintf(buf+len, PAGE_SIZE-len, "%d\n", aw_dev->cali_desc.cali_f0);
	} else {
		ret = aw_cali_svc_get_devs_cali_f0(aw_dev, cali_f0, AW_DEV_CH_MAX);
		if (ret <= 0) {
			aw_dev_err(aw_dev->dev, "get f0 failed ");
			len += snprintf(buf+len, PAGE_SIZE-len, "get f0 failed \n");
		} else {
			for (i = 0; i < ret; i++)
				len += snprintf(buf+len, PAGE_SIZE-len, "%s:%d ", ch_name[i], cali_f0[i]);
			len += snprintf(buf+len, PAGE_SIZE-len, " \n");
		}
	}

	return len;
}

static ssize_t aw_cali_attr_show_re(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret, i;
	struct aw882xx *aw882xx = dev_get_drvdata(dev);
	struct aw_device *aw_dev = aw882xx->aw_pa;
	ssize_t len = 0;
	int32_t cali_re[AW_DEV_CH_MAX] = {0};

	if (is_single_cali) {
		len += snprintf(buf+len, PAGE_SIZE-len, "%d\n", aw_dev->cali_desc.cali_re);
	} else {
		ret = aw_cali_svc_get_devs_cali_re(aw_dev, cali_re, AW_DEV_CH_MAX);
		if (ret <= 0) {
			aw_dev_err(aw_dev->dev, "get re failed ");
		} else {
			for (i = 0; i < ret; i++)
				len += snprintf(buf+len, PAGE_SIZE-len, "%s:%d mOhms ", ch_name[i], cali_re[i]);
		}
		len += snprintf(buf+len, PAGE_SIZE-len, " \n");
	}

	return len;
}

static ssize_t aw_cali_attr_show_f0(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret, i;
	struct aw882xx *aw882xx = dev_get_drvdata(dev);
	struct aw_device *aw_dev = aw882xx->aw_pa;
	ssize_t len = 0;
	int32_t cali_f0[AW_DEV_CH_MAX] = {0};

	if (is_single_cali) {
		len += snprintf(buf+len, PAGE_SIZE-len, "%d\n", aw_dev->cali_desc.cali_f0);
	} else {
		ret = aw_cali_svc_get_devs_cali_f0(aw_dev, cali_f0, AW_DEV_CH_MAX);
		if (ret <= 0) {
			aw_dev_err(aw_dev->dev, "get f0 failed ");
		} else {
			for (i = 0; i < ret; i++)
				len += snprintf(buf+len, PAGE_SIZE-len, "%s:%d ", ch_name[i], cali_f0[i]);
			len += snprintf(buf+len, PAGE_SIZE-len, " \n");
		}
	}

	return len;
}

/*set cali time*/
static DEVICE_ATTR(cali_time, S_IWUSR | S_IRUGO,
	aw_cali_attr_time_show, aw_cali_attr_time_store);
/*start cali*/
static DEVICE_ATTR(cali, S_IWUSR,
	NULL, aw_cali_attr_store);
/*cali re*/
static DEVICE_ATTR(cali_re, S_IRUGO | S_IWUSR,
	aw_cali_attr_re_show, aw_cali_attr_re_store);
/*cali_f0*/
static DEVICE_ATTR(cali_f0, S_IRUGO | S_IWUSR,
	aw_cali_attr_f0_show, aw_cali_attr_f0_store);
/*show cali_re*/
static DEVICE_ATTR(re_show, S_IRUGO,
	aw_cali_attr_show_re, NULL);
/*show cali_f0*/
static DEVICE_ATTR(f0_show, S_IRUGO,
	aw_cali_attr_show_f0, NULL);


static struct attribute *aw_cali_attr[] = {
	&dev_attr_cali_time.attr,
	&dev_attr_cali.attr,
	&dev_attr_cali_re.attr,
	&dev_attr_cali_f0.attr,
	&dev_attr_re_show.attr,
	&dev_attr_f0_show.attr,
	NULL
};

static struct attribute_group aw_cali_attr_group = {
	.attrs = aw_cali_attr
};

static void aw_cali_attr_init(struct aw_device *aw_dev)
{
	int ret;

	aw_dev_info(aw_dev->dev, "enter");

	ret = sysfs_create_group(&aw_dev->dev->kobj, &aw_cali_attr_group);
	if (ret < 0)
		aw_dev_info(aw_dev->dev, "error creating sysfs cali attr files");
}

static void aw_cali_attr_deinit(struct aw_device *aw_dev)
{
	sysfs_remove_group(&aw_dev->dev->kobj, &aw_cali_attr_group);
	aw_dev_info(aw_dev->dev, "attr files deinit");
}

/*****************************attr   end***************************************************/

/*****************************class node******************************************************/
static ssize_t aw_cali_class_time_show(struct  class *class, struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf+len, PAGE_SIZE-len,
		"time: %d \n", g_cali_re_time);

	return len;
}

static ssize_t aw_cali_class_time_store(struct class *class,
					struct class_attribute *attr, const char *buf, size_t len)
{
	int ret;
	uint32_t time;

	ret = kstrtoint(buf, 0, &time);
	if (ret < 0) {
		pr_err("[Awinic] %s, read buf %s failed\n",
			__func__, buf);
		return ret;
	}

	if (time < 400) {
		pr_err("[Awinic] %s:time:%d is too short, no set\n",
			__func__, time);
		return -EINVAL;
	}

	g_cali_re_time = time;
	pr_debug("%s:time:%d\n", __func__, time);

	return len;
}

static ssize_t aw_cali_class_cali_re_show(struct  class *class, struct class_attribute *attr, char *buf)
{
	struct list_head *dev_list;
	struct aw_device *local_dev;
	int ret, i;
	ssize_t len = 0;
	int32_t cali_re[AW_DEV_CH_MAX] = {0};

	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		pr_err("[Awinic] %s: get dev list failed \n", __func__);
		return ret;
	}

	local_dev = list_first_entry(dev_list, struct aw_device, list_node);

	ret = aw_cali_svc_cali_cmd(local_dev, AW_CALI_CMD_RE, false, CALI_OPS_HMUTE);
	if (ret < 0) {
		len += snprintf(buf+len, PAGE_SIZE-len, "cali Re cmd failed\n");
		return len;
	}

	ret = aw_cali_svc_get_devs_cali_re(local_dev, cali_re, AW_DEV_CH_MAX);
	if (ret <= 0) {
		aw_dev_err(local_dev->dev, "get re failed ");
		len += snprintf(buf+len, PAGE_SIZE-len, "get re failed \n");
	} else {
		for (i = 0; i < ret; i++)
			len += snprintf(buf+len, PAGE_SIZE-len, "%s:%d mOhms ", ch_name[i], cali_re[i]);
		len += snprintf(buf+len, PAGE_SIZE-len, " \n");
	}

	return len;
}

static ssize_t aw_cali_class_cali_re_store(struct class *class,
					struct class_attribute *attr, const char *buf, size_t len)
{
	struct list_head *dev_list;
	struct aw_device *local_dev;
	int ret;

	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		pr_err("[Awinic] %s: get dev list failed \n", __func__);
		return ret;
	}

	local_dev = list_first_entry(dev_list, struct aw_device, list_node);

	ret = aw_cali_svc_set_devs_re_str(local_dev, buf);
	if (ret <= 0) {
		aw_dev_err(local_dev->dev, "set re str %s failed", buf);
		return -EPERM;
	}

	return len;
}

static ssize_t aw_cali_class_cali_f0_show(struct  class *class, struct class_attribute *attr, char *buf)
{
	struct list_head *dev_list;
	struct aw_device *local_dev;
	int ret, i;
	ssize_t len = 0;
	int32_t cali_f0[AW_DEV_CH_MAX] = {0};

	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		pr_err("[Awinic] %s: get dev list failed \n", __func__);
		return ret;
	}

	local_dev = list_first_entry(dev_list, struct aw_device, list_node);

	ret = aw_cali_svc_cali_cmd(local_dev, AW_CALI_CMD_F0, false, CALI_OPS_NOISE);
	if (ret < 0) {
		len += snprintf(buf+len, PAGE_SIZE-len, "cali f0 cmd failed\n");
		return len;
	}

	ret = aw_cali_svc_get_devs_cali_f0(local_dev, cali_f0, AW_DEV_CH_MAX);
	if (ret <= 0) {
		aw_dev_err(local_dev->dev, "get f0 failed ");
		len += snprintf(buf+len, PAGE_SIZE-len, "get f0 failed \n");
	} else {
		for (i = 0; i < ret; i++)
			len += snprintf(buf+len, PAGE_SIZE-len, "%s:%d ", ch_name[i], cali_f0[i]);
		len += snprintf(buf+len, PAGE_SIZE-len, " \n");
	}

	return len;
}

static ssize_t aw_cali_class_cali_f0_store(struct class *class,
				struct class_attribute *attr, const char *buf, size_t len)
{
	struct list_head *dev_list;
	struct aw_device *local_dev;
	int ret;

	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		pr_err("[Awinic] %s: get dev list failed \n", __func__);
		return ret;
	}

	local_dev = list_first_entry(dev_list, struct aw_device, list_node);

	aw_cali_svc_cali_cmd(local_dev, AW_CALI_CMD_F0, false, CALI_OPS_NOISE);

	return len;
}

static ssize_t aw_cali_class_f0_show(struct  class *class, struct class_attribute *attr, char *buf)
{
	struct list_head *dev_list;
	struct aw_device *local_dev;
	int ret, i;
	ssize_t len = 0;
	int32_t cali_f0[AW_DEV_CH_MAX] = {0};

	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		pr_err("[Awinic] %s: get dev list failed \n", __func__);
		return ret;
	}

	local_dev = list_first_entry(dev_list, struct aw_device, list_node);

	ret = aw_cali_svc_get_devs_cali_f0(local_dev, cali_f0, AW_DEV_CH_MAX);
	if (ret <= 0) {
		aw_dev_err(local_dev->dev, "get re failed ");
	} else {
		for (i = 0; i < ret; i++)
			len += snprintf(buf+len, PAGE_SIZE-len, "%s:%d ", ch_name[i], cali_f0[i]);
		len += snprintf(buf+len, PAGE_SIZE-len, " \n");
	}

	return len;
}

static ssize_t aw_cali_class_re_show(struct  class *class, struct class_attribute *attr, char *buf)
{
	struct list_head *dev_list;
	struct aw_device *local_dev;
	int ret, i;
	ssize_t len = 0;
	int32_t cali_re[AW_DEV_CH_MAX] = {0};

	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		pr_err("[Awinic] %s: get dev list failed \n", __func__);
		return ret;
	}

	local_dev = list_first_entry(dev_list, struct aw_device, list_node);

	ret = aw_cali_svc_get_devs_cali_re(local_dev, cali_re, AW_DEV_CH_MAX);
	if (ret <= 0) {
		aw_dev_err(local_dev->dev, "get re failed ");
	} else {
		for (i = 0; i < ret; i++)
			len += snprintf(buf+len, PAGE_SIZE-len, "%s:%d mOhms ", ch_name[i], cali_re[i]);
		len += snprintf(buf+len, PAGE_SIZE-len, " \n");
	}

	return len;
}

static struct class_attribute class_attr_cali_time = \
		__ATTR(cali_time, S_IWUSR | S_IRUGO, \
		aw_cali_class_time_show, aw_cali_class_time_store);

static struct class_attribute class_attr_re25_calib =  \
		__ATTR(re25_calib, S_IWUSR | S_IRUGO,	\
		aw_cali_class_cali_re_show, aw_cali_class_cali_re_store);

static struct class_attribute class_attr_f0_calib = \
		__ATTR(f0_calib, S_IWUSR | S_IRUGO, \
		aw_cali_class_cali_f0_show, aw_cali_class_cali_f0_store);

static struct class_attribute class_attr_re_show = \
		__ATTR(re_show, S_IRUGO, \
		aw_cali_class_re_show, NULL);

static struct class_attribute class_attr_f0_show = \
		__ATTR(f0_show, S_IRUGO, \
		aw_cali_class_f0_show, NULL);


static struct class aw_cali_class = {
	.name = "smartpa",
	.owner = THIS_MODULE,
};

static void aw_cali_class_attr_init(struct aw_device *aw_dev)
{
	int ret;

	if (aw_dev->index != 0) {
		aw_dev_info(aw_dev->dev, "class node already register");
		return;
	}

	ret = class_register(&aw_cali_class);
	if (ret < 0) {
		aw_dev_info(aw_dev->dev, "error creating class node");
		return;
	}
	ret = class_create_file(&aw_cali_class, &class_attr_re25_calib);
	if (ret) {
		aw_dev_info(aw_dev->dev, "creat class_attr_re25_calib fail");
	}
	ret = class_create_file(&aw_cali_class, &class_attr_f0_calib);
	if (ret) {
		aw_dev_info(aw_dev->dev, "creat class_attr_re25_calib fail");
	}
	ret = class_create_file(&aw_cali_class, &class_attr_cali_time);
	if (ret) {
		aw_dev_info(aw_dev->dev, "creat class_attr_cali_time fail");
	}
	ret = class_create_file(&aw_cali_class, &class_attr_re_show);
	if (ret) {
		aw_dev_info(aw_dev->dev, "creat class_attr_re_show fail");
	}
	ret = class_create_file(&aw_cali_class, &class_attr_f0_show);
	if (ret) {
		aw_dev_info(aw_dev->dev, "creat class_attr_f0_show fail");
	}
}

static void aw_cali_class_attr_deinit(struct aw_device *aw_dev)
{
	class_remove_file(&aw_cali_class, &class_attr_re25_calib);
	class_remove_file(&aw_cali_class, &class_attr_f0_calib);
	class_remove_file(&aw_cali_class, &class_attr_cali_time);
	class_remove_file(&aw_cali_class, &class_attr_re_show);
	class_remove_file(&aw_cali_class, &class_attr_f0_show);

	class_unregister(&aw_cali_class);
	aw_dev_info(aw_dev->dev, "unregister class node");
}
/*****************************class node******************************************************/

/*****************************misc node******************************************************/
static int aw_cali_misc_open(struct inode *inode, struct file *file)
{
	struct list_head *dev_list;
	struct list_head *pos = NULL;
	struct aw_device *local_dev;
	int ret;

	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		pr_err("[Awinic] %s: get dev list failed \n", __func__);
		file->private_data = NULL;
		return -EINVAL;
	}

	/* find select dev */
	list_for_each (pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		if (local_dev->channel == g_dev_select) {
			break;
		}
	}

	/* cannot find sel dev, use list first dev */
	if (local_dev->channel != g_dev_select) {
		local_dev = list_first_entry(dev_list, struct aw_device, list_node);
		aw_dev_dbg(local_dev->dev, "can not find [%s], use default", ch_name[g_dev_select]);
	}

	file->private_data = (void *)local_dev;

	aw_dev_dbg(local_dev->dev, "misc open success\n");
	return 0;
}

static int aw_cali_misc_release(struct inode *inode, struct file *file)
{
	file->private_data = (void *)NULL;

	pr_debug("misc release successi\n");
	return 0;
}

static int aw_cali_misc_params_ptr(struct aw_device *aw_dev, struct ptr_params_data *p_params)
{
	char *p_data = NULL;
	int ret = 0;

	if (p_params->data == NULL || (!p_params->len)) {
		aw_dev_err(aw_dev->dev, "p_params error");
		ret = -EFAULT;
		return ret;
	}

	p_data = kzalloc(p_params->len, GFP_KERNEL);
	if (p_data == NULL) {
		aw_dev_err(aw_dev->dev, "error allocating memory");
		ret = -ENOMEM;
		goto exit;
	}

	if (copy_from_user(p_data,
			(void __user *)p_params->data,
			p_params->len)) {
		ret = -EFAULT;
		goto exit;
	}

	ret = aw_dsp_write_params(aw_dev, p_data, p_params->len);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "write params failed ");
		ret = -EFAULT;
		goto exit;
	}

exit:
	if (p_data != NULL) {
		kfree(p_data);
		p_data = NULL;
	}

	return ret;
}

static int aw_cali_misc_ops_write(struct aw_device *aw_dev,
			unsigned int cmd, unsigned long arg)
{
	unsigned int data_len = _IOC_SIZE(cmd);
	char *data_ptr = NULL;
	int32_t data = 0;
	int ret = 0;

	data_ptr = kzalloc(data_len, GFP_KERNEL);
	if (!data_ptr) {
		aw_dev_err(aw_dev->dev, "malloc failed !");
		return -ENOMEM;
	}

	if (copy_from_user(data_ptr, (void __user *)arg, data_len)) {
		ret = -EFAULT;
		goto exit;
	}

	switch (cmd) {
		case AW_IOCTL_ENABLE_CALI: {
			aw_cali_svc_set_cali_status(aw_dev, data_ptr[0]);
		} break;
		case AW_IOCTL_SET_CALI_CFG: {
			ret = aw_dsp_write_cali_cfg(aw_dev, data_ptr, data_len);
		} break;
		case AW_IOCTL_SET_NOISE: {
			data = *(int32_t *)data_ptr;
			ret = aw_dsp_noise_en(aw_dev, data);
		} break;
		case AW_IOCTL_SET_VMAX: {
			ret = aw_dsp_write_vmax(aw_dev, data_ptr, data_len);
		} break;
		case AW_IOCTL_SET_PARAM: {
			ret = aw_dsp_write_params(aw_dev, data_ptr, data_len);
		} break;
		case AW_IOCTL_SET_PTR_PARAM_NUM: {
			ret = aw_cali_misc_params_ptr(aw_dev, (struct ptr_params_data *)data_ptr);
		} break;
		case AW_IOCTL_SET_CALI_RE: {
			aw_cali_store_cali_re(aw_dev, *((int32_t *)data_ptr));
			/*ret = aw_dsp_write_cali_re(aw_dev, *(int32_t *)data_ptr);*/
		} break;
		case AW_IOCTL_SET_DSP_HMUTE: {
			data = *(int32_t *)data_ptr;
			ret = aw_dsp_hmute_en(aw_dev, data);
		} break;
		case AW_IOCTL_SET_CALI_CFG_FLAG: {
			data = *(int32_t *)data_ptr;
			/*aw_cali_svc_set_cali_status(aw_dev, data);*/
			ret = aw_dsp_cali_en(aw_dev, data);
		} break;
		default:{
			aw_dev_err(aw_dev->dev, "unsupported  cmd %d", cmd);
			ret = -EINVAL;
		} break;
	}

exit:
	kfree(data_ptr);
	return ret;
}

static int aw_cali_misc_ops_read(struct aw_device *aw_dev,
			unsigned int cmd, unsigned long arg)
{
	int16_t data_len = _IOC_SIZE(cmd);
	char *data_ptr = NULL;
	int32_t *data_32_ptr = NULL;
	int ret = 0;

	data_ptr = kzalloc(data_len, GFP_KERNEL);
	if (!data_ptr) {
		aw_dev_err(aw_dev->dev, "malloc failed !");
		return -ENOMEM;
	}

	switch (cmd) {
		case AW_IOCTL_GET_CALI_CFG: {
			ret = aw_dsp_read_cali_cfg(aw_dev, data_ptr, data_len);
		} break;
		case AW_IOCTL_GET_CALI_DATA: {
			ret = aw_dsp_read_cali_data(aw_dev, data_ptr, data_len);
		} break;
		case AW_IOCTL_GET_F0: {
			data_32_ptr = (int32_t *)data_ptr;
			ret = aw_dsp_read_f0(aw_dev, data_32_ptr);
		} break;
		case AW_IOCTL_GET_CALI_RE: {
			data_32_ptr = (int32_t *)data_ptr;
			ret = aw_dsp_read_cali_re(aw_dev, data_32_ptr);
		} break;
		case AW_IOCTL_GET_VMAX: {
			ret = aw_dsp_read_vmax(aw_dev, data_ptr, data_len);
		} break;
		case AW_IOCTL_GET_F0_Q: {
			data_32_ptr = (int32_t *)data_ptr;
			ret = aw_dsp_read_f0_q(aw_dev, &data_32_ptr[0], &data_32_ptr[1]);
		} break;
		default:{
			aw_dev_err(aw_dev->dev, "unsupported  cmd %d", cmd);
			ret = -EINVAL;
		} break;
	}

	if (copy_to_user((void __user *)arg,
		data_ptr, data_len)) {
		ret = -EFAULT;
	}

	kfree(data_ptr);
	return ret;
}

static int aw_cali_misc_read_dsp(struct aw_device *aw_dev, aw_ioctl_msg_t *msg)
{
	char __user *user_data = (char __user *)msg->data_buf;
	uint32_t dsp_msg_id = (uint32_t)msg->opcode_id;
	int data_len = msg->data_len;
	int ret;
	char *data_ptr;

	data_ptr = kzalloc(data_len, GFP_KERNEL);
	if (!data_ptr) {
		aw_dev_err(aw_dev->dev, "malloc failed !");
		return -ENOMEM;
	}

	ret = aw_dsp_read_msg(aw_dev, dsp_msg_id, data_ptr, data_len);
	if (ret) {
		aw_dev_err(aw_dev->dev, " write failed");
		goto exit;
	}

	if (copy_to_user((void __user *)user_data,
		data_ptr, data_len))
		ret = -EFAULT;
exit:
	kfree(data_ptr);
	return ret;
}

static int aw_cali_misc_write_dsp(struct aw_device *aw_dev, aw_ioctl_msg_t *msg)
{
	char __user *user_data = (char __user *)msg->data_buf;
	uint32_t dsp_msg_id = (uint32_t)msg->opcode_id;
	int data_len = msg->data_len;
	int ret;
	char *data_ptr;

	data_ptr = kzalloc(data_len, GFP_KERNEL);
	if (!data_ptr) {
		aw_dev_err(aw_dev->dev, "malloc failed !\n");
		return -ENOMEM;
	}

	if (copy_from_user(data_ptr, (void __user *)user_data, data_len)) {
		aw_dev_err(aw_dev->dev, "copy data failed");
		ret = -EFAULT;
		goto exit;
	}

	ret = aw_dsp_write_msg(aw_dev, dsp_msg_id, data_ptr, data_len);
	if (ret)
		aw_dev_err(aw_dev->dev, "write failed");

exit:
	kfree(data_ptr);
	return ret;
}


static int aw_cali_misc_ops(struct aw_device *aw_dev,
			unsigned int cmd, unsigned long arg);
static int aw_cali_misc_ops_msg(struct aw_device *aw_dev, unsigned long arg)
{
	aw_ioctl_msg_t ioctl_msg;

	if (copy_from_user(&ioctl_msg, (void __user *)arg, sizeof(aw_ioctl_msg_t)))
		return -EFAULT;

	if (ioctl_msg.version != AW_IOCTL_MSG_VERSION) {
		aw_dev_err(aw_dev->dev, "unsupported msg version %d", ioctl_msg.version);
		return -EINVAL;
	}

	if (ioctl_msg.type == AW_IOCTL_MSG_RD_DSP) {
		return aw_cali_misc_read_dsp(aw_dev, &ioctl_msg);
	} else if (ioctl_msg.type == AW_IOCTL_MSG_WR_DSP) {
		return aw_cali_misc_write_dsp(aw_dev, &ioctl_msg);
	} else if (ioctl_msg.type == AW_IOCTL_MSG_IOCTL) {
		return aw_cali_misc_ops(aw_dev, ioctl_msg.opcode_id, (unsigned long)ioctl_msg.data_buf);
	} else {
		aw_dev_err(aw_dev->dev, "unsupported msg type %d", ioctl_msg.type);
		return -EINVAL;
	}
}

static int aw_cali_misc_ops(struct aw_device *aw_dev,
			unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	switch (cmd) {
	case AW_IOCTL_ENABLE_CALI:
	case AW_IOCTL_SET_CALI_CFG:
	case AW_IOCTL_SET_NOISE:
	case AW_IOCTL_SET_VMAX:
	case AW_IOCTL_SET_PARAM:
	case AW_IOCTL_SET_PTR_PARAM_NUM:
	case AW_IOCTL_SET_CALI_RE:
	case AW_IOCTL_SET_DSP_HMUTE:
	case AW_IOCTL_SET_CALI_CFG_FLAG:
		ret = aw_cali_misc_ops_write(aw_dev, cmd, arg);
		break;
	case AW_IOCTL_GET_CALI_CFG:
	case AW_IOCTL_GET_CALI_DATA:
	case AW_IOCTL_GET_F0:
	case AW_IOCTL_GET_CALI_RE:
	case AW_IOCTL_GET_VMAX:
	case AW_IOCTL_GET_F0_Q:
		ret = aw_cali_misc_ops_read(aw_dev, cmd, arg);
		break;
	case AW_IOCTL_MSG: {
		ret = aw_cali_misc_ops_msg(aw_dev, arg);
	} break;
	default:
		aw_dev_err(aw_dev->dev, "unsupported  cmd %d", cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static long aw_cali_misc_unlocked_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct aw_device *aw_dev = NULL;

	if (((_IOC_TYPE(cmd)) != (AW_IOCTL_MAGIC))) {
		aw_dev_err(aw_dev->dev, " cmd magic err");
		return -EINVAL;
	}
	aw_dev = (struct aw_device *)file->private_data;
	ret = aw_cali_misc_ops(aw_dev, cmd, arg);
	if (ret)
		return -EINVAL;

	return 0;
}

#ifdef CONFIG_COMPAT
static long aw_cali_misc_compat_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct aw_device *aw_dev = NULL;

	if (((_IOC_TYPE(cmd)) != (AW_IOCTL_MAGIC))) {
		aw_dev_err(aw_dev->dev, "cmd magic err");
		return -EINVAL;
	}
	aw_dev = (struct aw_device *)file->private_data;
	ret = aw_cali_misc_ops(aw_dev, cmd, arg);
	if (ret)
		return -EINVAL;

	return 0;
}
#endif

static ssize_t aw_cali_misc_read(struct file *filp, char __user *buf, size_t size, loff_t *pos)
{
	int len = 0;
	int i, ret;
	struct aw_device *aw_dev = (struct aw_device *)filp->private_data;
	char local_buf[128] = { 0 };
	unsigned int dev_num;
	int32_t temp_data[AW_DEV_CH_MAX << 1] = {0};

	aw_dev_info(aw_dev->dev, "enter");

	if (*pos) {
		*pos = 0;
		return 0;
	}

	switch (g_msic_wr_flag) {
	case CALI_STR_SHOW_RE: {
		ret = aw_cali_svc_get_devs_cali_re(aw_dev, temp_data, AW_DEV_CH_MAX);
		if (ret <= 0) {
			aw_dev_err(aw_dev->dev, "get cali_re failed");
			return ret;
		} else {
			for (i = 0; i < ret; i++)
				len += snprintf(local_buf+len, sizeof(local_buf)-len,
							"%s:%d mOhms ", ch_name[i], temp_data[i]);

			len += snprintf(local_buf+len, sizeof(local_buf)-len, "\n");
		}
	} break;
	case CALI_STR_SHOW_CALI_F0: {
		ret = aw_cali_svc_get_devs_cali_f0(aw_dev, temp_data, AW_DEV_CH_MAX);
		if (ret <= 0) {
			aw_dev_err(aw_dev->dev, "get cali f0 failed");
			return ret;
		} else {
			for (i = 0; i < ret; i++)
				len += snprintf(local_buf+len, sizeof(local_buf)-len,
							"%s:%d ", ch_name[i], temp_data[i]);

			len += snprintf(local_buf+len, sizeof(local_buf)-len, "\n");
		}
	} break;
	case CALI_STR_SHOW_R0: {
		ret = aw_cali_svc_get_devs_r0(aw_dev, temp_data, AW_DEV_CH_MAX);
		if (ret <= 0) {
			aw_dev_err(aw_dev->dev, "get r0 failed");
			return ret;
		} else {
			for (i = 0; i < ret; i++)
				len += snprintf(local_buf+len, sizeof(local_buf)-len,
							"%s:%d mOhms ", ch_name[i], temp_data[i]);
			len += snprintf(local_buf+len, sizeof(local_buf)-len, "\n");
		}
	} break;
	case CALI_STR_SHOW_TE: {
		ret = aw_cali_svc_get_devs_te(aw_dev, temp_data, AW_DEV_CH_MAX);
		if (ret <= 0) {
			aw_dev_err(aw_dev->dev, "get te failed");
			return ret;
		} else {
			for (i = 0; i < ret; i++)
				len += snprintf(local_buf+len, sizeof(local_buf)-len,
							"%s:%d ", ch_name[i], temp_data[i]);
			len += snprintf(local_buf+len, sizeof(local_buf)-len, "\n");
		}
	} break;
	case CALI_STR_SHOW_ST: {
		ret = aw_cali_svc_get_devs_st(aw_dev, temp_data, AW_DEV_CH_MAX);
		if (ret <= 0) {
			aw_dev_err(aw_dev->dev, "get spkr status failed");
			return ret;
		} else {
			for (i = 0; i < ret; i++) {
				len += snprintf(local_buf+len, sizeof(local_buf)-len,
							"%s:R0 %d mOhms Te %d ",
						ch_name[i], temp_data[i << 1], temp_data[(i << 1) + 1]);
			}
			len += snprintf(local_buf+len, sizeof(local_buf)-len, "\n");
		}
	} break;
	case CALI_STR_SHOW_F0: {
		ret = aw_cali_svc_get_devs_f0(aw_dev, temp_data, AW_DEV_CH_MAX);
		if (ret <= 0) {
			aw_dev_err(aw_dev->dev, "get f0 failed");
			return ret;
		} else {
			for (i = 0; i < ret; i++)
				len += snprintf(local_buf+len, sizeof(local_buf)-len,
							"%s:%d ", ch_name[i], temp_data[i]);

			len += snprintf(local_buf+len, sizeof(local_buf) - len, "\n");
		}
	} break;
	case CALI_STR_VER: {
		if (aw_dev->ops.aw_get_version) {
			len = aw_dev->ops.aw_get_version(local_buf, sizeof(local_buf));
			if (len < 0) {
				aw_dev_err(aw_dev->dev, "get version failed");
				return -EINVAL;
			}
			len += snprintf(local_buf+len, sizeof(local_buf) - len, "\n");
		} else {
			aw_dev_err(aw_dev->dev, "get version is NULL");
			return -EINVAL;
		}
	} break;
	case CALI_STR_DEV_NUM: {
		if (aw_dev->ops.aw_get_dev_num) {
			dev_num = aw_dev->ops.aw_get_dev_num();
			len += snprintf(local_buf + len, sizeof(local_buf) - len, "dev_num:%d\n", dev_num);
		} else {
			aw_dev_err(aw_dev->dev, "get dev num is NULL");
			return -EINVAL;
		}
		break;
	}
	default: {
		if (g_msic_wr_flag == CALI_STR_NONE) {
			aw_dev_info(aw_dev->dev, "please write cmd first");
			return -EINVAL;
		} else {
			aw_dev_err(aw_dev->dev, "unsupported flag [%d]", g_msic_wr_flag);
			g_msic_wr_flag = CALI_STR_NONE;
			return -EINVAL;
		}
	} break;
	}

	if (copy_to_user((void __user *)buf, local_buf, len)) {
		aw_dev_err(aw_dev->dev, "copy_to_user error");
		g_msic_wr_flag = CALI_STR_NONE;
		return -EFAULT;
	}

	g_msic_wr_flag = CALI_STR_NONE;
	*pos += len;
	return len;
}

static int aw_cali_misc_switch_dev(struct file *filp, struct aw_device *aw_dev, char *cmd_buf)
{
	int i;
	char dev_select[50];
	struct list_head *dev_list;
	struct list_head *pos = NULL;
	struct aw_device *local_dev;
	int ret;

	/* get sel dev str */
	sscanf(cmd_buf, "dev_sel:%s", dev_select);

	for (i = 0; i < AW_DEV_CH_MAX; i++) {
		if (strnstr(dev_select,	ch_name[i], strlen(ch_name[i])))
			break;
	}

	if (i == AW_DEV_CH_MAX) {
		aw_dev_err(aw_dev->dev, "unsupport dev [%s]", dev_select);
		return -EINVAL;
	}

	/* get dev list */
	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		aw_dev_err(aw_dev->dev, "get dev list failed ");
		return ret;
	}

	/* find sel dev */
	list_for_each(pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		if (local_dev->channel == i) {
			filp->private_data = (void *)local_dev;
			g_dev_select = i;
			aw_dev_info(local_dev->dev, "switch dev to [%s]", ch_name[i]);
			return 0;
		}
	}
	aw_dev_err(aw_dev->dev, " unsupport [%s]", dev_select);
	return -EINVAL;
}

static ssize_t aw_cali_misc_write(struct file *filp, const char __user *buf, size_t size, loff_t *pos)
{
	char *kernel_buf = NULL;
	struct aw_device *aw_dev = (struct aw_device *)filp->private_data;
	int ret = 0;

	aw_dev_info(aw_dev->dev, "enter, write size:%d", (int)size);
	kernel_buf = kzalloc(size, GFP_KERNEL);
	if (kernel_buf == NULL) {
		aw_dev_err(aw_dev->dev, "kzalloc failed !");
		return -ENOMEM;
	}

	if (copy_from_user(kernel_buf,
			(void __user *)buf,
			size)) {
		ret = -EFAULT;
		goto exit;
	}

	ret = aw_cali_svc_get_cmd_form_str(aw_dev, kernel_buf);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "upported cmd [%s]! ", kernel_buf);
		ret = -EINVAL;
		goto exit;
	}

	switch (ret) {
	case CALI_STR_CALI_RE_F0: {
		ret = aw_cali_svc_cali_cmd(aw_dev, AW_CALI_CMD_RE_F0,
					is_single_cali, CALI_OPS_HMUTE|CALI_OPS_NOISE);
	} break;
	case CALI_STR_CALI_RE: {
		ret = aw_cali_svc_cali_cmd(aw_dev, AW_CALI_CMD_RE,
					is_single_cali, CALI_OPS_HMUTE);
	} break;
	case CALI_STR_CALI_F0: {
		ret = aw_cali_svc_cali_cmd(aw_dev, AW_CALI_CMD_F0,
					is_single_cali, CALI_OPS_HMUTE|CALI_OPS_NOISE);
	} break;
	case CALI_STR_SET_RE: {
		/*skip store_re*/
		ret = aw_cali_svc_set_devs_re_str(aw_dev,
				kernel_buf + strlen(cali_str[CALI_STR_SET_RE]) + 1);
	} break;
	case CALI_STR_DEV_SEL: {
		ret = aw_cali_misc_switch_dev(filp, aw_dev, kernel_buf);
	} break;
	case CALI_STR_SHOW_RE:			/*show cali_re*/
	case CALI_STR_SHOW_R0:			/*show real r0*/
	case CALI_STR_SHOW_CALI_F0:		/*GET DEV CALI_F0*/
	case CALI_STR_SHOW_F0:			/*SHOW REAL F0*/
	case CALI_STR_SHOW_TE:
	case CALI_STR_SHOW_ST:
	case CALI_STR_VER:
	case CALI_STR_DEV_NUM: {
		g_msic_wr_flag = ret;
		ret = 0;
	} break;
	default: {
		aw_dev_err(aw_dev->dev, "unsupported [%s]! ", kernel_buf);
		ret = -EINVAL;
	} break;
	};

exit:
	aw_dev_info(aw_dev->dev, "cmd [%s]! ", kernel_buf);
	if (kernel_buf) {
		kfree(kernel_buf);
		kernel_buf = NULL;
	}
	if (ret < 0)
		return -EINVAL;
	else
		return size;
}

static const struct file_operations aw_cali_misc_fops = {
	.owner = THIS_MODULE,
	.open = aw_cali_misc_open,
	.read = aw_cali_misc_read,
	.write = aw_cali_misc_write,
	.release = aw_cali_misc_release,
	.unlocked_ioctl = aw_cali_misc_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = aw_cali_misc_compat_ioctl,
#endif

};

struct miscdevice misc_cali = {
	.name = "aw882xx_smartpa",
	.minor = MISC_DYNAMIC_MINOR,
	.fops  = &aw_cali_misc_fops,
};

static int aw_cali_misc_init(struct aw_device *aw_dev)
{
	int ret;

	mutex_lock(&g_cali_lock);
	if (g_misc_dev == NULL) {
		ret = misc_register(&misc_cali);
		if (ret) {
			aw_dev_err(aw_dev->dev, "misc register fail: %d\n", ret);
			mutex_unlock(&g_cali_lock);
			return -EINVAL;
		}
		g_misc_dev = &misc_cali;
		aw_dev_dbg(aw_dev->dev, "misc register success");
	} else {
		aw_dev_dbg(aw_dev->dev, "misc already register");
	}
	mutex_unlock(&g_cali_lock);

	return 0;
}

static void aw_cali_misc_deinit(struct aw_device *aw_dev)
{
	mutex_lock(&g_cali_lock);
	if (g_misc_dev) {
		misc_deregister(g_misc_dev);
		g_misc_dev = NULL;
	}
	mutex_unlock(&g_cali_lock);
	aw_dev_dbg(aw_dev->dev, " misc unregister done");
}

/*****************************misc node******************************************************/
static void aw_cali_parse_dt(struct aw_device *aw_dev)
{
	struct device_node *np = aw_dev->dev->of_node;
	int ret = -1;
	const char *cali_mode_str;
	struct aw_cali_desc *desc = &aw_dev->cali_desc;

	ret = of_property_read_string(np, "aw-cali-mode", &cali_mode_str);
	if (ret < 0) {
		aw_dev_info(aw_dev->dev, " aw-cali-mode get failed ,user default misc way");
		desc->mode = AW_CALI_MODE_MISC;
		return;
	}

	if (!strcmp(cali_mode_str, "none"))
		desc->mode = AW_CALI_MODE_NONE;
	else if (!strcmp(cali_mode_str, "aw_class"))
		desc->mode = AW_CALI_MODE_CLASS;
	else if (!strcmp(cali_mode_str, "aw_attr"))
		desc->mode = AW_CALI_MODE_ATTR;
	else
		desc->mode = AW_CALI_MODE_MISC; /*default misc*/

	aw_dev_info(aw_dev->dev, "cali mode str:%s num:%d",
			cali_mode_str, desc->mode);
}

int aw_cali_parse_re_dt(struct aw_device *aw_dev)
{
	int ret;

	ret = of_property_read_u32(aw_dev->dev->of_node, "aw-re-min", &aw_dev->re_min);
	if (ret < 0) {
		aw_dev->re_min = AW_CALI_RE_DEFAULT_MIN;
		aw_dev_info(aw_dev->dev, "read aw-re-min failed, use default");
	}

	ret = of_property_read_u32(aw_dev->dev->of_node, "aw-re-max", &aw_dev->re_max);
	if (ret < 0) {
		aw_dev->re_max = AW_CALI_RE_DEFAULT_MAX;
		aw_dev_info(aw_dev->dev, "read aw-re-max failed, use default");
	}

	if (aw_dev->re_min >= aw_dev->re_max) {
		aw_dev_err(aw_dev->dev, "re max must be greater than re min");
		return -EINVAL;
	}

	aw_dev_info(aw_dev->dev, "re min: %d, re max: %d",
					aw_dev->re_min, aw_dev->re_max);
	return 0;
}

int aw_cali_init(struct aw_cali_desc *cali_desc)
{
	int ret;

	struct aw_device *aw_dev =
		container_of(cali_desc, struct aw_device, cali_desc);

	cali_desc->cali_f0 = 0;
	cali_desc->cali_re = 0;
	cali_desc->cali_q = 0;
	cali_desc->status = 0;

	aw_cali_parse_dt(aw_dev);

	if (cali_desc->mode == AW_CALI_MODE_NONE)
		return 0;
	else if (cali_desc->mode == AW_CALI_MODE_ATTR)
		aw_cali_attr_init(aw_dev);
	else if (cali_desc->mode == AW_CALI_MODE_CLASS)
		aw_cali_class_attr_init(aw_dev);

	aw_cali_misc_init(aw_dev);

	ret = aw_cali_parse_re_dt(aw_dev);

	return ret;
}

void aw_cali_deinit(struct aw_cali_desc *cali_desc)
{
	struct aw_device *aw_dev =
		container_of(cali_desc, struct aw_device, cali_desc);

	if (cali_desc->mode == AW_CALI_MODE_ATTR)
		aw_cali_attr_deinit(aw_dev);
	else if (cali_desc->mode == AW_CALI_MODE_CLASS)
		aw_cali_class_attr_deinit(aw_dev);

	aw_cali_misc_deinit(aw_dev);
}

