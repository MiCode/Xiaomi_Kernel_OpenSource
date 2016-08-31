/* drivers/i2c/chips/a2220.c - a2220 voice processor driver
 *
 * Copyright (C) 2009 HTC Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/freezer.h>
#include <linux/a2220.h>
#include <linux/a2220_fw.h>
#include <linux/kthread.h>
#include <linux/clk.h>

#include <mach/iomap.h>
#include <linux/io.h>


#define PMC_CLK_OUT			0x1a8
#define CLK3_SRC_SEL	(0x3 << 22)
#define CLK3_FORCE_EN	(0x1 << 18)

#define MODULE_NAME "audience_a2220"
#define DEBUG			(0)
#define ENABLE_DIAG_IOCTLS	(0)
#define WAKEUP_GPIO_NUM_HERCULES_REV01 33
#define WAKEUP_GPIO_NUM_CELOX_ATT_REV05 33

/* MAGIC NUMBERS! Fixme */
#define VP_RESET 118
#define AUDIO_LD0_EN  60
#define AMP_SHUTDOWN_N  139

static struct i2c_client *this_client;
static struct a2220_platform_data *pdata;
static struct task_struct *task;
static int execute_cmdmsg(unsigned int);

static struct mutex a2220_lock;
static int a2220_opened;
static int a2220_suspended;
static int control_a2220_clk = 0;
struct clk *extern3_clk;
static unsigned int a2220_NS_state = A2220_NS_STATE_AUTO;
static int a2220_current_config = A2220_PATH_SUSPEND;
static int a2220_param_ID;

struct vp_ctxt {
	unsigned char *data;
	unsigned int img_size;
};

struct vp_ctxt the_vp;

unsigned int get_hw_rev(void)
{
	return 0x05;
}

static int a2220_i2c_read(char *rxData, int length)
{
	int rc;
	struct i2c_msg msgs[] = {
		{
		 .addr = this_client->addr,
		 .flags = I2C_M_RD,
		 .len = length,
		 .buf = rxData,
		 },
	};

	rc = i2c_transfer(this_client->adapter, msgs, 1);
	if (rc < 0) {
		printk(KERN_ERR "%s: transfer error %d\n", __func__, rc);
		return rc;
	}
#if DEBUG
	{
		int i = 0;
		for (i = 0; i < length; i++)
			pr_info("%s: rx[%d] = %2x\n", __func__, i, rxData[i]);
	}
#endif

	return 0;
}

static int a2220_i2c_write(char *txData, int length)
{
	int rc;
	struct i2c_msg msg[] = {
		{
		 .addr = this_client->addr,
		 .flags = 0,
		 .len = length,
		 .buf = txData,
		 },
	};

	rc = i2c_transfer(this_client->adapter, msg, 1);
	if (rc < 0) {
		printk(KERN_ERR "%s: transfer error %d\n", __func__, rc);
		return rc;
	}
#if DEBUG
	{
		int i = 0;
		for (i = 0; i < length; i++)
			pr_info("%s: tx[%d] = %2x\n", __func__, i, txData[i]);
	}
#endif

	return 0;
}

static int a2220_open(struct inode *inode, struct file *file)
{
	int rc = 0;
	struct vp_ctxt *vp = &the_vp;

	mutex_lock(&a2220_lock);

	if (a2220_opened) {
		printk(KERN_ERR "%s: busy\n", __func__);
		rc = -EBUSY;
		goto done;
	}

	file->private_data = vp;
	vp->img_size = 0;
	a2220_opened = 1;
 done:
	mutex_unlock(&a2220_lock);
	return rc;
}

static int a2220_release(struct inode *inode, struct file *file)
{
	mutex_lock(&a2220_lock);
	a2220_opened = 0;
	mutex_unlock(&a2220_lock);

	return 0;
}

#ifdef AUDIENCE_BYPASS			  /*(+)dragonball Multimedia mode */
#define A100_msg_mutimedia1   0x801C0000  /*VoiceProcessingOn, 0x0000:off */
#define A100_msg_mutimedia2   0x8026001F  /*SelectRouting, 0x001A:(26) */
#define A100_msg_mutimedia3   0x800C0B03  /* ; PCM B Din delay 1bit */
#define A100_msg_mutimedia4   0x800D0001
#define A100_msg_mutimedia5   0x800C0A03  /* ; PCM A Din delay 1bit */
#define A100_msg_mutimedia6   0x800D0001
#endif

static void a2220_i2c_sw_reset(unsigned int reset_cmd)
{
	int rc = 0;
	unsigned char msgbuf[4];

	msgbuf[0] = (reset_cmd >> 24) & 0xFF;
	msgbuf[1] = (reset_cmd >> 16) & 0xFF;
	msgbuf[2] = (reset_cmd >> 8) & 0xFF;
	msgbuf[3] = reset_cmd & 0xFF;

	pr_info("%s: %08x\n", __func__, reset_cmd);

	rc = a2220_i2c_write(msgbuf, 4);
	if (!rc)
		msleep(20);
}

static ssize_t a2220_hw_reset(struct a2220img *img)
{
	struct a2220img *vp = img;
	int rc, i, pass = 0;
	int remaining;
	int retry = RETRY_CNT;
	unsigned char *index;
	char buf[2];

	while (retry--) {
		/* Reset A2220 chip */
		if (pdata->gpio_a2220_reset)
			gpio_set_value(pdata->gpio_a2220_reset, 0);
		else
			gpio_set_value(VP_RESET, 1);

		/* Enable A2220 clock */
		if (control_a2220_clk)
			gpio_set_value(pdata->gpio_a2220_clk, 1);
		mdelay(1);

		/* Take out of reset */
		if (pdata->gpio_a2220_reset)
			gpio_set_value(pdata->gpio_a2220_reset, 1);
		else
			gpio_set_value(VP_RESET, 0);

		msleep(50);	/* Delay before send I2C command */

		/* Boot Cmd to A2220 */
		buf[0] = A2220_msg_BOOT >> 8;
		buf[1] = A2220_msg_BOOT & 0xff;

		rc = a2220_i2c_write(buf, 2);
		if (rc < 0) {
			printk(KERN_ERR "%s: set boot mode error (%d retries left)\n",
			       __func__, retry);
			continue;
		}

		mdelay(1);
		rc = a2220_i2c_read(buf, 1);

		if (rc < 0) {
			printk(KERN_ERR "%s: boot mode ack error (%d retries left)\n",
			       __func__, retry);
			continue;
		}

		remaining = vp->img_size / 32;
		index = vp->buf;

		for (; remaining; remaining--, index += 32) {
			rc = a2220_i2c_write(index, 32);
			if (rc < 0)
				break;
		}

		if (rc >= 0 && vp->img_size % 32)
			rc = a2220_i2c_write(index, vp->img_size % 32);

		if (rc < 0) {
			printk(KERN_ERR "%s: fw load error %d (%d retries left)\n",
			       __func__, rc, retry);
			continue;
		}

		msleep(20);	/* Delay time before issue a Sync Cmd */

		for (i = 0; i < 10; i++)
			msleep(20);

		rc = execute_cmdmsg(A100_msg_Sync);
		if (rc < 0) {
			printk(KERN_ERR "%s: sync command error %d (%d retries left)\n",
			       __func__, rc, retry);
			continue;
		}

		pass = 1;
		break;
	}
	return rc;
}

/* eS305B HPT  */
#ifdef CONFIG_USA_MODEL_SGH_I717
static int hpt_longCmd_execute(unsigned char *i2c_cmds, int size)
{

	int i = 0, rc = 0;
	int retry = 4;
/*      unsigned int sw_reset = 0; */
	unsigned int msg;
	unsigned char *pMsg;

	pMsg = (unsigned char *)&msg;

	for (i = 0; i < size; i += 4) {
		pMsg[3] = i2c_cmds[i];
		pMsg[2] = i2c_cmds[i + 1];
		pMsg[1] = i2c_cmds[i + 2];
		pMsg[0] = i2c_cmds[i + 3];

		do {
			rc = execute_cmdmsg(msg);
		} while ((rc < 0) && --retry);

	}
	return 0;
}
#endif

static int a2220_set_boot_mode(void)
{
	int rc;
	int retry = RETRY_CNT;
	char buf[2];

	mdelay(100);

	while (retry--) {
		/* Reset A2220 chip */
		gpio_set_value(VP_RESET, 1);

		/* Enable A2220 clock */
		if (control_a2220_clk)
			gpio_set_value(pdata->gpio_a2220_clk, 1);
		mdelay(1);

		/* Take out of reset */
		gpio_set_value(VP_RESET, 0);

		msleep(150); /* Delay before send I2C command */

		/* Boot Cmd to A2220 */
		buf[0] = A2220_msg_BOOT >> 8;
		buf[1] = A2220_msg_BOOT & 0xff;
		rc = a2220_i2c_write(buf, 2);
		if (rc < 0) {
			printk(KERN_ERR "%s: write error (%d retries left)\n",
			       __func__, retry);
			if (retry > 0)
				continue;
			else
				return rc;
		}

		mdelay(1);
		rc = a2220_i2c_read(buf, 1);

		if (rc < 0) {
			printk(KERN_ERR "%s: ack error (%d retries left)\n",
			       __func__, retry);
			continue;
		}
	}

	return rc;
}

static ssize_t a2220_bootup_init(struct a2220img *pImg)
{
	struct a2220img *vp = pImg;
	int rc = 0, pass = 0;
	int remaining;
	int retry = RETRY_CNT;
	unsigned char *index;

	mdelay(10);

	while (retry--) {
		remaining = vp->img_size / 32;
		index = vp->buf;
		pr_info("%s: starting to load image (%d passes)...\n",
			__func__, remaining + !!(vp->img_size % 32));

		for (; remaining; remaining--, index += 32) {
			rc = a2220_i2c_write(index, 32);
			if (rc < 0)
				break;
		}

		if (rc >= 0 && vp->img_size % 32)
			rc = a2220_i2c_write(index, vp->img_size % 32);

		if (rc < 0) {
			printk(KERN_ERR "%s: fw load error %d (%d retries left)\n",
			       __func__, rc, retry);
			continue;
		}

		msleep(150);	/* Delay time before issue a Sync Cmd */

		rc = execute_cmdmsg(A100_msg_Sync);
		if (rc < 0) {
			printk(KERN_ERR "%s: sync command error %d (%d retries left)\n",
			       __func__, rc, retry);
			continue;
		}

		pass = 1;
		break;
	}

	rc = execute_cmdmsg(A100_msg_ReadPortA);
	if (rc < 0)
		printk(KERN_ERR "%s: suspend error\n", __func__);

	rc = execute_cmdmsg(A100_msg_PortD_C_PASS);
	if (rc < 0)
		printk(KERN_ERR "%s: suspend error\n", __func__);

	rc = execute_cmdmsg(A100_msg_PortB_A_PASS);
	if (rc < 0)
		printk(KERN_ERR "%s: suspend error\n", __func__);

	a2220_suspended = 0;
	rc = execute_cmdmsg(A100_msg_Sleep);
	if (rc < 0)
		printk(KERN_ERR "%s: suspend error\n", __func__);

	msleep(30);

	if (control_a2220_clk)
		clk_disable(extern3_clk);

	return rc;
}


static ssize_t chk_wakeup_a2220(void)
{
	int i, rc = 0, retry = 4;

	if (a2220_suspended == 1) {
		/* Enable A2220 clock */
		if (control_a2220_clk) {
			gpio_set_value(pdata->gpio_a2220_clk, 1);
			mdelay(1);
		}

		if (pdata->gpio_a2220_wakeup) {
			printk(MODULE_NAME
			       "%s : chk_wakeup_a2220  --> get_hw_rev of Target = %d\n",
			       __func__, get_hw_rev());
#ifdef CONFIG_USA_MODEL_SGH_T989
			if (get_hw_rev() >= 0x05)
				gpio_set_value(WAKEUP_GPIO_NUM_HERCULES_REV01,
					       0);
			else
				gpio_set_value(pdata->gpio_a2220_wakeup, 0);
#elif CONFIG_USA_MODEL_SGH_I727
			qweqwewq if (get_hw_rev() >= 0x05)
				gpio_set_value(WAKEUP_GPIO_NUM_CELOX_ATT_REV05,
					       0);
			else
				gpio_set_value(pdata->gpio_a2220_wakeup, 0);
#elif CONFIG_USA_MODEL_SGH_I717
			gpio_set_value(WAKEUP_GPIO_NUM_CELOX_ATT_REV05, 0);
#else
			gpio_set_value(pdata->gpio_a2220_wakeup, 0);
#endif
		}
#ifdef CONFIG_USA_MODEL_SGH_I717
		for (i = 0; i < 5; i++)
			msleep(20);
#else
		msleep(30);
#endif

		do {
			rc = execute_cmdmsg(A100_msg_Sync);
		} while ((rc < 0) && --retry);

		/* Audience not responding to execute_cmdmsg ,
		* doing HW reset of the chipset */
		if ((retry == 0) && (rc < 0)) {
			struct a2220img img;
			img.buf = a2220_firmware_buf;
			img.img_size = sizeof(a2220_firmware_buf);
			rc = a2220_hw_reset(&img);	/* Call if the Audience
			chipset is not responding after retrying 12 times */
		}
		if (rc < 0)
			printk(MODULE_NAME "%s ::  Audience HW Reset Failed\n",
			       __func__);

#ifdef CONFIG_USA_MODEL_SGH_I717
		rc = hpt_longCmd_execute(hpt_init_macro,
					 sizeof(hpt_init_macro));
		if (rc < 0)
			printk(MODULE_NAME "%s: htp init error\n", __func__);
#endif

		if (pdata->gpio_a2220_wakeup) {
#ifdef CONFIG_USA_MODEL_SGH_T989
			if (get_hw_rev() >= 0x05)
				gpio_set_value(WAKEUP_GPIO_NUM_HERCULES_REV01,
					       1);
			else
				gpio_set_value(pdata->gpio_a2220_wakeup, 1);
#elif CONFIG_USA_MODEL_SGH_I727
			if (get_hw_rev() >= 0x05)
				gpio_set_value(WAKEUP_GPIO_NUM_CELOX_ATT_REV05,
					       1);
			else
				gpio_set_value(pdata->gpio_a2220_wakeup, 1);
#elif CONFIG_USA_MODEL_SGH_I717
			gpio_set_value(WAKEUP_GPIO_NUM_CELOX_ATT_REV05, 1);
#else
			gpio_set_value(pdata->gpio_a2220_wakeup, 1);
#endif
		}

		if (rc < 0) {
			printk(KERN_ERR "%s: failed (%d)\n", __func__, rc);
			goto wakeup_sync_err;
		}

		a2220_suspended = 0;
	}
 wakeup_sync_err:
	return rc;
}

/* Filter commands according to noise suppression state forced by
 * A2220_SET_NS_STATE ioctl.
 *
 * For this function to operate properly, all configurations must include
 * both A100_msg_Bypass and Mic_Config commands even if default values
 * are selected or if Mic_Config is useless because VP is off
 */
int a2220_filter_vp_cmd(int cmd, int mode)
{
	int msg = (cmd >> 16) & 0xFFFF;
	int filtered_cmd = cmd;

	if (a2220_NS_state == A2220_NS_STATE_AUTO)
		return cmd;

	switch (msg) {
	case A100_msg_Bypass:
		if (a2220_NS_state == A2220_NS_STATE_OFF)
			filtered_cmd = A2220_msg_VP_OFF;
		else
			filtered_cmd = A2220_msg_VP_ON;
		break;
	case A100_msg_SetAlgorithmParmID:
		a2220_param_ID = cmd & 0xFFFF;
		break;
	case A100_msg_SetAlgorithmParm:
		if (a2220_param_ID == Mic_Config) {
			if (a2220_NS_state == A2220_NS_STATE_CT)
				filtered_cmd = (msg << 16);
			else if (a2220_NS_state == A2220_NS_STATE_FT)
				filtered_cmd = (msg << 16) | 0x0002;
		}
		break;
	default:
		if (mode == A2220_CONFIG_VP)
			filtered_cmd = -1;
		break;
	}

	pr_info("%s: %x filtered = %x, a2220_NS_state %d, mode %d\n", __func__,
		cmd, filtered_cmd, a2220_NS_state, mode);

	return filtered_cmd;
}

int a2220_set_config(char newid, int mode)
{
	int i = 0, rc = 0, size = 0;
	int retry = 4;
	unsigned int sw_reset = 0;
	unsigned char *i2c_cmds;
	unsigned int msg;
	unsigned char *pMsg;

	if ((a2220_suspended) && (newid == A2220_PATH_SUSPEND))
		return rc;

#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_I727)\
		|| defined(CONFIG_USA_MODEL_SGH_I717)
	if (a2220_current_config == newid) {
		printk(KERN_DEBUG "already configured this path!!!\n");
		return rc;
	}
#endif

	rc = chk_wakeup_a2220();
	if (rc < 0)
		return rc;

	sw_reset = ((A100_msg_Reset << 16) | RESET_IMMEDIATE);

	switch (newid) {
	case A2220_PATH_INCALL_RECEIVER_NSON:
		i2c_cmds = phonecall_receiver_nson;
		size = sizeof(phonecall_receiver_nson);
		break;

	case A2220_PATH_INCALL_RECEIVER_NSOFF:
		i2c_cmds = phonecall_receiver_nsoff;
		size = sizeof(phonecall_receiver_nsoff);
		break;

		/* (+) ysseo 20110420 : to use a2220 bypass mode */
#ifdef AUDIENCE_BYPASS		/*(+)dragonball Multimedia bypass mode */
	case A2220_PATH_BYPASS_MULTIMEDIA:
		printk(KERN_DEBUG "%s : setting A2220_PATH_BYPASS_MULTIMEDIA\n",
			 __func__);
		i2c_cmds = bypass_multimedia;
		size = sizeof(bypass_multimedia);
		break;
#endif
	case A2220_PATH_INCALL_HEADSET:
		i2c_cmds = phonecall_headset;
		size = sizeof(phonecall_headset);
		break;
	case A2220_PATH_INCALL_SPEAKER:
		i2c_cmds = phonecall_speaker;
		size = sizeof(phonecall_speaker);
		break;
	case A2220_PATH_INCALL_BT:
		i2c_cmds = phonecall_bt;
		size = sizeof(phonecall_bt);
		break;
	case A2220_PATH_INCALL_TTY:
		i2c_cmds = phonecall_tty;
		size = sizeof(phonecall_tty);
		break;
	case A2220_PATH_VR_NO_NS_RECEIVER:
		i2c_cmds = vr_no_ns_receiver;
		size = sizeof(vr_no_ns_receiver);
		break;
	case A2220_PATH_VR_NO_NS_HEADSET:
		i2c_cmds = vr_no_ns_headset;
		size = sizeof(vr_no_ns_headset);
		break;
	case A2220_PATH_VR_NO_NS_SPEAKER:
		i2c_cmds = vr_no_ns_speaker;
		size = sizeof(vr_no_ns_speaker);
		break;
	case A2220_PATH_VR_NO_NS_BT:
		i2c_cmds = vr_no_ns_bt;
		size = sizeof(vr_no_ns_bt);
		break;
	case A2220_PATH_VR_NS_RECEIVER:
		i2c_cmds = vr_ns_receiver;
		size = sizeof(vr_ns_receiver);
		break;
	case A2220_PATH_VR_NS_HEADSET:
		i2c_cmds = vr_ns_headset;
		size = sizeof(vr_ns_headset);
		break;
	case A2220_PATH_VR_NS_SPEAKER:
		i2c_cmds = vr_ns_speaker;
		size = sizeof(vr_ns_speaker);
		break;
	case A2220_PATH_VR_NS_BT:
		i2c_cmds = vr_ns_bt;
		size = sizeof(vr_ns_bt);
		break;
	case A2220_PATH_RECORD_RECEIVER:
		i2c_cmds = INT_MIC_recording_receiver;
		size = sizeof(INT_MIC_recording_receiver);
		break;
	case A2220_PATH_RECORD_HEADSET:
		i2c_cmds = EXT_MIC_recording;
		size = sizeof(EXT_MIC_recording);
		break;
	case A2220_PATH_RECORD_SPEAKER:
		i2c_cmds = INT_MIC_recording_speaker;
		size = sizeof(INT_MIC_recording_speaker);
		break;
	case A2220_PATH_RECORD_BT:
		i2c_cmds = phonecall_bt;
		size = sizeof(phonecall_bt);
		break;
	case A2220_PATH_SUSPEND:
		i2c_cmds = (unsigned char *)suspend_mode;
		size = sizeof(suspend_mode);
		break;
	case A2220_PATH_CAMCORDER:
		i2c_cmds = BACK_MIC_recording;
		size = sizeof(BACK_MIC_recording);
		break;
	default:
		printk(KERN_ERR "%s: invalid cmd %d\n", __func__, newid);
		rc = -1;
		goto input_err;
		break;
	}

	a2220_current_config = newid;

#if DEBUG
	pr_info("%s: change to mode %d\n", __func__, newid);
	pr_info("%s: block write start (size = %d)\n", __func__, size);
	for (i = 1; i <= size; i++) {
		pr_info("%x ", *(i2c_cmds + i - 1));
		if (!(i % 4))
			pr_info("\n");
	}
#endif

#if 1

	pMsg = (unsigned char *)&msg;

	for (i = 0; i < size; i += 4) {
		pMsg[3] = i2c_cmds[i];
		pMsg[2] = i2c_cmds[i + 1];
		pMsg[1] = i2c_cmds[i + 2];
		pMsg[0] = i2c_cmds[i + 3];

		do {
			rc = execute_cmdmsg(msg);
		} while ((rc < 0) && --retry);

		/* Audience not responding to execute_cmdmsg ,
		* doing HW reset of the chipset */
		if ((retry == 0) && (rc < 0)) {
			struct a2220img img;
			img.buf = a2220_firmware_buf;
			img.img_size = sizeof(a2220_firmware_buf);
			rc = a2220_hw_reset(&img); /* Call if the Audience
			chipset is not responding after retrying 12 times */
			if (rc < 0) {
				printk(MODULE_NAME
				       "%s ::  Audience HW Reset Failed\n",
				       __func__);
				return rc;
			}
		}

	}

#else
	rc = a2220_i2c_write(i2c_cmds, size);
	if (rc < 0) {
		printk(KERN_ERR "A2220 CMD block write error!\n");
		a2220_i2c_sw_reset(sw_reset);
		return rc;
	}
	pr_info("%s: block write end\n", __func__);

	/* Don't need to get Ack after sending out a suspend command */
	if (*i2c_cmds == 0x80 && *(i2c_cmds + 1) == 0x10
	    && *(i2c_cmds + 2) == 0x00 && *(i2c_cmds + 3) == 0x01) {
		a2220_suspended = 1;
		/* Disable A2220 clock */
		msleep(120);
		if (control_a2220_clk)
			gpio_set_value(pdata->gpio_a2220_clk, 0);
		return rc;
	}

	memset(ack_buf, 0, sizeof(ack_buf));
	msleep(20);
	pr_info("%s: CMD ACK block read start\n", __func__);
	rc = a2220_i2c_read(ack_buf, size);
	if (rc < 0) {
		printk(KERN_ERR "%s: CMD ACK block read error\n", __func__);
		a2220_i2c_sw_reset(sw_reset);
		return rc;
	} else {
		pr_info("%s: CMD ACK block read end\n", __func__);
#if DEBUG
		for (i = 1; i <= size; i++) {
			pr_info("%x ", ack_buf[i - 1]);
			if (!(i % 4))
				pr_info("\n");
		}
#endif
		index = ack_buf;
		number_of_cmd_sets = size / 4;
		do {
			if (*index == 0x00) {
				rd_retry_cnt = POLLING_RETRY_CNT;
 rd_retry:
				if (rd_retry_cnt--) {
					memset(rdbuf, 0, sizeof(rdbuf));
					rc = a2220_i2c_read(rdbuf, 4);
					if (rc < 0)
						return rc;
#if DEBUG
					for (i = 0; i < sizeof(rdbuf); i++)
						pr_info("0x%x\n", rdbuf[i]);
					pr_info("-----------------\n");
#endif
					if (rdbuf[0] == 0x00) {
						msleep(20);
						goto rd_retry;
					}
				} else {
					printk(KERN_ERR "%s: CMD ACK Not Ready\n",
					       __func__);
					return -EBUSY;
				}
			} else if (*index == 0xff) {	/* illegal cmd */
				return -ENOEXEC;
			} else if (*index == 0x80) {
				index += 4;
			}
		} while (--number_of_cmd_sets);
	}
#endif

 input_err:
	return rc;
}

int execute_cmdmsg(unsigned int msg)
{
	int rc = 0;
	int retries, pass = 0;
	unsigned char msgbuf[4];
	unsigned char chkbuf[4];
	unsigned int sw_reset = 0;

	sw_reset = ((A100_msg_Reset << 16) | RESET_IMMEDIATE);

	msgbuf[0] = (msg >> 24) & 0xFF;
	msgbuf[1] = (msg >> 16) & 0xFF;
	msgbuf[2] = (msg >> 8) & 0xFF;
	msgbuf[3] = msg & 0xFF;

#if DEBUG
	printk(KERN_DEBUG "%s : execute_cmdmsg :: %x %x %x %x\n",
		__func__, msgbuf[0], msgbuf[1], msgbuf[2], msgbuf[3]);
#endif
	memcpy(chkbuf, msgbuf, 4);

	rc = a2220_i2c_write(msgbuf, 4);
	if (rc < 0) {
		a2220_i2c_sw_reset(sw_reset);

		if (msg == A100_msg_Sleep) {
			printk(MODULE_NAME
			       "%s : execute_cmdmsg ...go to suspend first\n",
			       __func__);
			a2220_suspended = 1;/*(+)dragonball test for audience */
			msleep(120);

		}
		return rc;
	}

	/* We don't need to get Ack after sending out a suspend command */
	if (msg == A100_msg_Sleep) {
		printk(MODULE_NAME "%s : ...go to suspend first\n", __func__);
		a2220_suspended = 1;	/*(+)dragonball test for audience */

		return rc;
	}

	retries = POLLING_RETRY_CNT;
	while (retries--) {
		rc = 0;
		memset(msgbuf, 0, sizeof(msgbuf));
		rc = a2220_i2c_read(msgbuf, 4);
		if (rc < 0) {
			printk(KERN_ERR "%s: ...........ack-read error %d (%d retries)\n",
			     __func__, rc, retries);
			continue;
		}

		if (msgbuf[0] == 0x80 && msgbuf[1] == chkbuf[1]) {
			pass = 1;
			break;
		} else if (msgbuf[0] == 0xff && msgbuf[1] == 0xff) {
			printk(KERN_ERR "%s: illegal cmd %08x\n",
				__func__, msg);
			rc = -EINVAL;
			/*break; */
		} else if (msgbuf[0] == 0x00 && msgbuf[1] == 0x00) {
			pr_info("%s: not ready (%d retries)\n", __func__,
				retries);
			rc = -EBUSY;
		} else {
			pr_info("%s: cmd/ack mismatch: (%d retries left)\n",
				__func__, retries);
#if DEBUG
			pr_info("%s: msgbuf[0] = %x\n", __func__, msgbuf[0]);
			pr_info("%s: msgbuf[1] = %x\n", __func__, msgbuf[1]);
			pr_info("%s: msgbuf[2] = %x\n", __func__, msgbuf[2]);
			pr_info("%s: msgbuf[3] = %x\n", __func__, msgbuf[3]);
#endif
			rc = -EBUSY;
		}
		msleep(20);	/* use polling */
	}

	if (!pass) {
		printk(KERN_ERR "%s: failed execute cmd %08x (%d)\n",
			__func__, msg, rc);
		a2220_i2c_sw_reset(sw_reset);
	}

	return rc;
}

#if ENABLE_DIAG_IOCTLS
static int a2220_set_mic_state(char miccase)
{
	int rc = 0;
	unsigned int cmd_msg = 0;

	switch (miccase) {
	case 1:		/* Mic-1 ON / Mic-2 OFF */
		cmd_msg = 0x80260007;
		break;
	case 2:		/* Mic-1 OFF / Mic-2 ON */
		cmd_msg = 0x80260015;
		break;
	case 3:		/* both ON */
		cmd_msg = 0x80260001;
		break;
	case 4:		/* both OFF */
		cmd_msg = 0x80260006;
		break;
	default:
		pr_info("%s: invalid input %d\n", __func__, miccase);
		rc = -EINVAL;
		break;
	}
	rc = execute_cmdmsg(cmd_msg);
	return rc;
}

static int exe_cmd_in_file(unsigned char *incmd)
{
	int rc = 0;
	int i = 0;
	unsigned int cmd_msg = 0;
	unsigned char tmp = 0;

	for (i = 0; i < 4; i++) {
		tmp = *(incmd + i);
		cmd_msg |= (unsigned int)tmp;
		if (i != 3)
			cmd_msg = cmd_msg << 8;
	}
	rc = execute_cmdmsg(cmd_msg);
	if (rc < 0)
		printk(KERN_ERR "%s: cmd %08x error %d\n",
			__func__, cmd_msg, rc);
	return rc;
}
#endif	/* ENABLE_DIAG_IOCTLS */

/* Thread does the init process of Audience Chip */
static int a2220_init_thread(void *data)
{
	int rc = 0;
	struct a2220img img;
	img.buf = a2220_firmware_buf;
	img.img_size = sizeof(a2220_firmware_buf);
	rc = a2220_bootup_init(&img);
	return rc;
}

static int
a2220_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	    unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct a2220img img;
	int rc = 0;
#if ENABLE_DIAG_IOCTLS
	char msg[4];
	int mic_cases = 0;
	int mic_sel = 0;
#endif
	unsigned int ns_state;

	switch (cmd) {
	case A2220_BOOTUP_INIT:
		img.buf = a2220_firmware_buf;
		img.img_size = sizeof(a2220_firmware_buf);
		printk(MODULE_NAME "%s : a2220_firmware_buf = %d\n", __func__,
		       sizeof(a2220_firmware_buf));
		task =
		    kthread_run(a2220_init_thread, NULL, "a2220_init_thread");
		if (IS_ERR(task)) {
			rc = PTR_ERR(task);
			task = NULL;
		}
		break;
	case A2220_SET_CONFIG:
		rc = a2220_set_config(arg, A2220_CONFIG_FULL);
		if (rc < 0)
			printk(KERN_ERR "%s: A2220_SET_CONFIG (%lu) error %d!\n",
			       __func__, arg, rc);
		break;
	case A2220_SET_NS_STATE:
		if (copy_from_user(&ns_state, argp, sizeof(ns_state)))
			return -EFAULT;
		pr_info("%s: set noise suppression %d\n", __func__, ns_state);
		if (ns_state < 0 || ns_state >= A2220_NS_NUM_STATES)
			return -EINVAL;
		a2220_NS_state = ns_state;
		if (!a2220_suspended)
			a2220_set_config(a2220_current_config, A2220_CONFIG_VP);
		break;
#if ENABLE_DIAG_IOCTLS
	case A2220_SET_MIC_ONOFF:
		rc = chk_wakeup_a2220();
		if (rc < 0)
			return rc;
		if (copy_from_user(&mic_cases, argp, sizeof(mic_cases)))
			return -EFAULT;
		rc = a2220_set_mic_state(mic_cases);
		if (rc < 0)
			printk(KERN_ERR "%s: A2220_SET_MIC_ONOFF %d error %d!\n",
			       __func__, mic_cases, rc);
		break;
	case A2220_SET_MICSEL_ONOFF:
		rc = chk_wakeup_a2220();
		if (rc < 0)
			return rc;
		if (copy_from_user(&mic_sel, argp, sizeof(mic_sel)))
			return -EFAULT;
		rc = 0;
		break;
	case A2220_READ_DATA:
		rc = chk_wakeup_a2220();
		if (rc < 0)
			return rc;
		rc = a2220_i2c_read(msg, 4);
		if (copy_to_user(argp, &msg, 4))
			return -EFAULT;
		break;
	case A2220_WRITE_MSG:
		rc = chk_wakeup_a2220();
		if (rc < 0)
			return rc;
		if (copy_from_user(msg, argp, sizeof(msg)))
			return -EFAULT;
		rc = a2220_i2c_write(msg, 4);
		break;
	case A2220_SYNC_CMD:
		rc = chk_wakeup_a2220();
		if (rc < 0)
			return rc;
		msg[0] = 0x80;
		msg[1] = 0x00;
		msg[2] = 0x00;
		msg[3] = 0x00;
		rc = a2220_i2c_write(msg, 4);
		break;
	case A2220_SET_CMD_FILE:
		rc = chk_wakeup_a2220();
		if (rc < 0)
			return rc;
		if (copy_from_user(msg, argp, sizeof(msg)))
			return -EFAULT;
		rc = exe_cmd_in_file(msg);
		break;
#endif				/* ENABLE_DIAG_IOCTLS */
	default:
		printk(KERN_ERR "%s: invalid command %d\n",
			__func__, _IOC_NR(cmd));
		rc = -EINVAL;
		break;
	}

	return rc;
}

int a2220_ioctl2(unsigned int cmd, unsigned long arg)
{
	a2220_ioctl(NULL, NULL, cmd, arg);
	return 0;
}
EXPORT_SYMBOL(a2220_ioctl2);

int a2220_port_path_change(unsigned int msg)
{
	switch (msg) {
	case A100_msg_PortC_D_PASS:
	case A100_msg_PortD_C_PASS:
	case A100_msg_PortB_A_PASS:
	case A100_msg_PortA_B_PASS:
	case A100_msg_PortC_A_PASS:
	case A100_msg_PortA_C_PASS:
		break;
	default:
		printk(KERN_ERR "Not support [0x%X] for port change\n", msg);
		return -EINVAL;
	}
	/* Default set to PORTD -> PORTC and
	   PORTB -> PORTA in pass through) */
	/* return execute_cmdmsg(msg); */
	return 0;
}
EXPORT_SYMBOL(a2220_port_path_change);

static const struct file_operations a2220_fops = {
	.owner = THIS_MODULE,
	.open = a2220_open,
	.release = a2220_release,
/*      .ioctl = a2220_ioctl, */
};

static struct miscdevice a2220_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "audience_a2220",
	.fops = &a2220_fops,
};

static int a2220_probe(struct i2c_client *client,
		       const struct i2c_device_id *id)
{
	int rc = 0, ret;
	unsigned long val;
	void __iomem *pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);

	extern3_clk = clk_get_sys("extern3", NULL);
	if (IS_ERR(extern3_clk)) {
		printk(KERN_ERR "%s: Can't retrieve extern3\n", __func__);
		goto err_clk_get_failed;
	}

	ret = clk_enable(extern3_clk);
	if (ret) {
		printk(KERN_ERR "Can't enable clk extern3");
		goto err_clk_enable_failed;
	}

	control_a2220_clk = 1;
	/* disable master enable in PMC */
	val = readl(pmc_base + PMC_CLK_OUT);
	val |= CLK3_SRC_SEL;

	writel(val, pmc_base + PMC_CLK_OUT);

	val = readl(pmc_base + PMC_CLK_OUT);
	val |= CLK3_FORCE_EN;
	writel(val, pmc_base + PMC_CLK_OUT);

	pdata = client->dev.platform_data;

	if (pdata == NULL) {
		pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
		printk(KERN_ERR "%s : a2220_probe - pdata NULL so allocating ...\n",
		       __func__);
		if (pdata == NULL) {
			rc = -ENOMEM;
			printk(KERN_ERR "%s: platform data is NULL\n",
				__func__);

			goto err_alloc_data_failed;
		}
	}

#ifdef CONFIG_USA_MODEL_SGH_T989
	if (get_hw_rev() >= 0x05)
		gpio_tlmm_config(GPIO_CFG(33, 0, GPIO_CFG_OUTPUT,
				GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
				GPIO_CFG_ENABLE);/* 2MIC_PWDN */
	else
		gpio_tlmm_config(GPIO_CFG(34, 0, GPIO_CFG_OUTPUT,
				GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
				GPIO_CFG_ENABLE);/* 2MIC_PWDN */
#elif CONFIG_USA_MODEL_SGH_I727
	if (get_hw_rev() >= 0x05) {
		printk(KERN_DEBUG " %s : GPIO 33\n", __func__);
		gpio_tlmm_config(GPIO_CFG(33, 0, GPIO_CFG_OUTPUT,
				GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
				GPIO_CFG_ENABLE);/* 2MIC_PWDN */
	} else {
		printk(KERN_DEBUG "%s : get_hw_rev() == %d\n",
			__func__, get_hw_rev());

		gpio_tlmm_config(GPIO_CFG(34, 0, GPIO_CFG_OUTPUT,
				GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
				GPIO_CFG_ENABLE);/* 2MIC_PWDN */
	}
#elif CONFIG_USA_MODEL_SGH_I717

#else
	gpio_tlmm_config(GPIO_CFG(34, 0, GPIO_CFG_OUTPUT,
			GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
			GPIO_CFG_ENABLE);	/* 2MIC_PWDN */
#endif

#if !defined(CONFIG_USA_MODEL_SGH_I727) && !defined(CONFIG_USA_MODEL_SGH_T989)\
	&&  !defined(CONFIG_USA_MODEL_SGH_I717)	/*qup_a2220 */
	gpio_tlmm_config(GPIO_CFG(35, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL,
			GPIO_CFG_2MA), GPIO_CFG_ENABLE);/* 2MIC_SDA_1.8V */
	gpio_tlmm_config(GPIO_CFG(36, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL,
			GPIO_CFG_2MA), GPIO_CFG_ENABLE);/* 2MIC_SCL_1.8V */
#endif

	/*VP  reset  */
	gpio_set_value(VP_RESET, 1);
	gpio_set_value(AUDIO_LD0_EN, 0);

	this_client = client;

	gpio_set_value(VP_RESET, 0);

	if (pdata->gpio_a2220_clk) {
		rc = gpio_request(pdata->gpio_a2220_clk, "a2220");
		if (rc < 0) {
			control_a2220_clk = 0;
			goto chk_gpio_micsel;
		}
		control_a2220_clk = 1;

		rc = gpio_direction_output(pdata->gpio_a2220_clk, 1);
		if (rc < 0) {
			printk(KERN_ERR "%s: request clk gpio direction failed\n",
			       __func__);
			goto err_free_gpio_clk;
		}
	}
 chk_gpio_micsel:
	if (pdata->gpio_a2220_micsel) {
		rc = gpio_request(pdata->gpio_a2220_micsel, "a2220");
		if (rc < 0) {
			printk(KERN_ERR "%s: gpio request mic_sel pin failed\n",
			       __func__);
			goto err_free_gpio_micsel;
		}

		rc = gpio_direction_output(pdata->gpio_a2220_micsel, 1);
		if (rc < 0) {
			printk(KERN_ERR "%s: request mic_sel gpio direction failed\n",
			       __func__);
			goto err_free_gpio_micsel;
		}
	}

	if (pdata->gpio_a2220_wakeup) {
#ifdef CONFIG_USA_MODEL_SGH_T989
		if (get_hw_rev() >= 0x05)
			rc = gpio_request(WAKEUP_GPIO_NUM_HERCULES_REV01,
					  "a2220");
		else
			rc = gpio_request(pdata->gpio_a2220_wakeup, "a2220");
#elif CONFIG_USA_MODEL_SGH_I727
		if (get_hw_rev() >= 0x05)
			rc = gpio_request(WAKEUP_GPIO_NUM_CELOX_ATT_REV05,
					  "a2220");
		else
			rc = gpio_request(pdata->gpio_a2220_wakeup, "a2220");
#elif CONFIG_USA_MODEL_SGH_I717
		rc = gpio_request(WAKEUP_GPIO_NUM_CELOX_ATT_REV05, "a2220");
#else
		rc = gpio_request(pdata->gpio_a2220_wakeup, "a2220");
#endif
		if (rc < 0) {
			printk(KERN_ERR "%s: gpio request wakeup pin failed\n",
			       __func__);
			goto err_free_gpio;
		}
#ifdef CONFIG_USA_MODEL_SGH_T989
		if (get_hw_rev() >= 0x05)
			rc = gpio_direction_output
			    (WAKEUP_GPIO_NUM_HERCULES_REV01, 1);
		else
			rc = gpio_direction_output(pdata->gpio_a2220_wakeup, 1);
#elif CONFIG_USA_MODEL_SGH_I727
		if (get_hw_rev() >= 0x05)
			rc = gpio_direction_output
			    (WAKEUP_GPIO_NUM_CELOX_ATT_REV05, 1);
		else
			rc = gpio_direction_output(pdata->gpio_a2220_wakeup, 1);
#elif CONFIG_USA_MODEL_SGH_I717
		rc = gpio_direction_output(WAKEUP_GPIO_NUM_CELOX_ATT_REV05, 1);
#else
		rc = gpio_direction_output(pdata->gpio_a2220_wakeup, 1);
#endif


		if (rc < 0) {
			printk(KERN_ERR "%s: request wakeup gpio direction failed\n",
			       __func__);
			goto err_free_gpio;
		}
	}

	if (pdata->gpio_a2220_reset) {
		rc = gpio_request(pdata->gpio_a2220_reset, "a2220");
		if (rc < 0) {
			printk(KERN_ERR "%s: gpio request reset pin failed\n",
			__func__);
			goto err_free_gpio;
		}

		rc = gpio_direction_output(pdata->gpio_a2220_reset, 1);
		if (rc < 0) {
			printk(KERN_ERR "%s: request reset gpio direction failed\n",
			__func__);
			goto err_free_gpio_all;
		}
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "%s: i2c check functionality error\n",
		__func__);
		rc = -ENODEV;
		goto err_free_gpio_all;
	}

	if (control_a2220_clk)
		gpio_set_value(pdata->gpio_a2220_clk, 1);
	if (pdata->gpio_a2220_micsel)
		gpio_set_value(pdata->gpio_a2220_micsel, 0);

	if (pdata->gpio_a2220_wakeup) {
#ifdef CONFIG_USA_MODEL_SGH_T989
		if (get_hw_rev() >= 0x05)
			gpio_set_value(WAKEUP_GPIO_NUM_HERCULES_REV01, 1);
		else
			gpio_set_value(pdata->gpio_a2220_wakeup, 1);
#elif CONFIG_USA_MODEL_SGH_I727
		if (get_hw_rev() >= 0x05)
			gpio_set_value(WAKEUP_GPIO_NUM_CELOX_ATT_REV05, 1);
		else
			gpio_set_value(pdata->gpio_a2220_wakeup, 1);
#elif CONFIG_USA_MODEL_SGH_I717
		gpio_set_value(WAKEUP_GPIO_NUM_CELOX_ATT_REV05, 1);
#else
		gpio_set_value(pdata->gpio_a2220_wakeup, 1);
#endif
	}

	if (pdata->gpio_a2220_reset)
		gpio_set_value(pdata->gpio_a2220_reset, 1);

	if (pdata->gpio_a2220_audience_chip_sel)
		gpio_set_value(pdata->gpio_a2220_audience_chip_sel, 1);

	rc = misc_register(&a2220_device);
	if (rc) {
		printk(KERN_ERR "%s: a2220_device register failed\n", __func__);
		goto err_free_gpio_all;
	}

	/* send boot msg */
	rc = a2220_set_boot_mode();
	if (rc < 0) {
		printk(KERN_ERR "%s: failed %d\n", __func__, rc);
		goto err_free_gpio_all;
	}

	/* A2220 firmware download start .. */
	a2220_ioctl2(A2220_BOOTUP_INIT, 0);

	return 0;

 err_free_gpio_all:
	if (pdata->gpio_a2220_reset)
		gpio_free(pdata->gpio_a2220_reset);
 err_free_gpio:
	if (pdata->gpio_a2220_wakeup) {
#ifdef CONFIG_USA_MODEL_SGH_T989
		if (get_hw_rev() >= 0x05)
			gpio_free(WAKEUP_GPIO_NUM_HERCULES_REV01);
		else
			gpio_free(pdata->gpio_a2220_wakeup);
#elif CONFIG_USA_MODEL_SGH_I727
		if (get_hw_rev() >= 0x05)
			gpio_free(WAKEUP_GPIO_NUM_CELOX_ATT_REV05);
		else
			gpio_free(pdata->gpio_a2220_wakeup);
#elif CONFIG_USA_MODEL_SGH_I717
		gpio_free(WAKEUP_GPIO_NUM_CELOX_ATT_REV05);
#else
		gpio_free(pdata->gpio_a2220_wakeup);
#endif
	}
 err_free_gpio_micsel:
	if (pdata->gpio_a2220_micsel)
		gpio_free(pdata->gpio_a2220_micsel);
 err_free_gpio_clk:
	if (pdata->gpio_a2220_clk)
		gpio_free(pdata->gpio_a2220_clk);
 err_alloc_data_failed:
	clk_disable(extern3_clk);
 err_clk_enable_failed:
	clk_put(extern3_clk);
 err_clk_get_failed:

	return rc;
}

static int a2220_remove(struct i2c_client *client)
{
	struct a2220_platform_data *p1026data = i2c_get_clientdata(client);
	kfree(p1026data);

	return 0;
}

static int a2220_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return 0;
}

static int a2220_resume(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id a2220_id[] = {
	{"audience_a2220", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, a2220_id);

static struct i2c_driver a2220_driver = {
	.probe = a2220_probe,
	.remove = a2220_remove,
	.suspend = a2220_suspend,
	.resume = a2220_resume,
	.id_table = a2220_id,
	.driver = {
		   .name = "audience_a2220",
		   },
};


static int __init a2220_init(void)
{
#ifdef CONFIG_BATTERY_SEC
	if (is_lpcharging_state()) {
		pr_info("%s : LPM Charging Mode! return 0\n", __func__);
		return 0;
	}
#endif
	mutex_init(&a2220_lock);

	return i2c_add_driver(&a2220_driver);
}

static void __exit a2220_exit(void)
{
	i2c_del_driver(&a2220_driver);
}

module_init(a2220_init);
module_exit(a2220_exit);

MODULE_DESCRIPTION("A2220 voice processor driver");
MODULE_LICENSE("GPL");
