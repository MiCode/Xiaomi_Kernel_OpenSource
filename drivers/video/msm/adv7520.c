/* Copyright (c) 2010,2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/adv7520.h>
#include <linux/time.h>
#include <linux/completion.h>
#include <linux/wakelock.h>
#include <linux/clk.h>
#include <asm/atomic.h>
#include "msm_fb.h"

#define DEBUG
#define DEV_DBG_PREFIX "HDMI: "

#include "external_common.h"

/* #define PORT_DEBUG */
/* #define TESTING_FORCE_480p */

#define HPD_DUTY_CYCLE	4 /*secs*/

static struct external_common_state_type hdmi_common;

static struct i2c_client *hclient;
static struct clk *tv_enc_clk;

static bool chip_power_on = FALSE;	/* For chip power on/off */
static bool enable_5v_on = FALSE;
static bool hpd_power_on = FALSE;
static atomic_t comm_power_on;	/* For dtv power on/off (I2C) */
static int suspend_count;

static u8 reg[256];	/* HDMI panel registers */

struct hdmi_data {
	struct msm_hdmi_platform_data *pd;
	struct work_struct isr_work;
};
static struct hdmi_data *dd;
static struct work_struct hpd_timer_work;

#ifdef CONFIG_FB_MSM_HDMI_ADV7520_PANEL_HDCP_SUPPORT
static struct work_struct hdcp_handle_work;
static int hdcp_activating;
static DEFINE_MUTEX(hdcp_state_mutex);
static int has_hdcp_hw_support = true;
#endif

static struct timer_list hpd_timer;
static struct timer_list hpd_duty_timer;
static struct work_struct hpd_duty_work;
static unsigned int monitor_sense;
static boolean hpd_cable_chg_detected;

struct wake_lock wlock;

/* Change HDMI state */
static void change_hdmi_state(int online)
{
	if (!external_common_state)
		return;

	mutex_lock(&external_common_state_hpd_mutex);
	external_common_state->hpd_state = online;
	mutex_unlock(&external_common_state_hpd_mutex);

	if (!external_common_state->uevent_kobj)
		return;

	if (online) {
		kobject_uevent(external_common_state->uevent_kobj,
			KOBJ_ONLINE);
		switch_set_state(&external_common_state->sdev, 1);
	} else {
		kobject_uevent(external_common_state->uevent_kobj,
			KOBJ_OFFLINE);
		switch_set_state(&external_common_state->sdev, 0);
	}
	DEV_INFO("adv7520_uevent: %d [suspend# %d]\n", online, suspend_count);
}


/*
 * Read a value from a register on ADV7520 device
 * If sucessfull returns value read , otherwise error.
 */
static u8 adv7520_read_reg(struct i2c_client *client, u8 reg)
{
	int err;
	struct i2c_msg msg[2];
	u8 reg_buf[] = { reg };
	u8 data_buf[] = { 0 };

	if (!client->adapter)
		return -ENODEV;
	if (!atomic_read(&comm_power_on)) {
		DEV_WARN("%s: WARN: missing GPIO power\n", __func__);
		return -ENODEV;
	}

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = reg_buf;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = data_buf;

	err = i2c_transfer(client->adapter, msg, 2);

	if (err < 0) {
		DEV_INFO("%s: I2C err: %d\n", __func__, err);
		return err;
	}

#ifdef PORT_DEBUG
	DEV_INFO("HDMI[%02x] [R] %02x\n", reg, data);
#endif
	return *data_buf;
}

/*
 * Write a value to a register on adv7520 device.
 * Returns zero if successful, or non-zero otherwise.
 */
static int adv7520_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	int err;
	struct i2c_msg msg[1];
	unsigned char data[2];

	if (!client->adapter)
		return -ENODEV;
	if (!atomic_read(&comm_power_on)) {
		DEV_WARN("%s: WARN: missing GPIO power\n", __func__);
		return -ENODEV;
	}

	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 2;
	msg->buf = data;
	data[0] = reg;
	data[1] = val;

	err = i2c_transfer(client->adapter, msg, 1);
	if (err >= 0)
		return 0;
#ifdef PORT_DEBUG
	DEV_INFO("HDMI[%02x] [W] %02x [%d]\n", reg, val, err);
#endif
	return err;
}

#ifdef CONFIG_FB_MSM_HDMI_ADV7520_PANEL_HDCP_SUPPORT
static void adv7520_close_hdcp_link(void)
{
	if (!external_common_state->hdcp_active && !hdcp_activating)
		return;

	DEV_INFO("HDCP: Close link\n");

	reg[0xD5] = adv7520_read_reg(hclient, 0xD5);
	reg[0xD5] &= 0xFE;
	adv7520_write_reg(hclient, 0xD5, (u8)reg[0xD5]);

	reg[0x16] = adv7520_read_reg(hclient, 0x16);
	reg[0x16] &= 0xFE;
	adv7520_write_reg(hclient, 0x16, (u8)reg[0x16]);

	/* UnMute Audio */
	adv7520_write_reg(hclient, 0x0C, (u8)0x84);

	external_common_state->hdcp_active = FALSE;
	mutex_lock(&hdcp_state_mutex);
	hdcp_activating = FALSE;
	mutex_unlock(&hdcp_state_mutex);
}

static void adv7520_comm_power(int on, int show);
static void adv7520_hdcp_enable(struct work_struct *work)
{
	DEV_INFO("HDCP: Start reg[0xaf]=%02x (mute audio)\n", reg[0xaf]);

	adv7520_comm_power(1, 1);

	/* Mute Audio */
	adv7520_write_reg(hclient, 0x0C, (u8)0xC3);

	msleep(200);
	/* Wait for BKSV ready interrupt */
	/* Read BKSV's keys from HDTV */
	reg[0xBF] = adv7520_read_reg(hclient, 0xBF);
	reg[0xC0] = adv7520_read_reg(hclient, 0xC0);
	reg[0xC1] = adv7520_read_reg(hclient, 0xC1);
	reg[0xC2] = adv7520_read_reg(hclient, 0xC2);
	reg[0xc3] = adv7520_read_reg(hclient, 0xC3);

	DEV_DBG("HDCP: BKSV={%02x,%02x,%02x,%02x,%02x}\n", reg[0xbf], reg[0xc0],
		reg[0xc1], reg[0xc2], reg[0xc3]);

	/* Is SINK repeater */
	reg[0xBE] = adv7520_read_reg(hclient, 0xBE);
	if (~(reg[0xBE] & 0x40)) {
		; /* compare with revocation list */
		/* Check 20 1's and 20 zero's */
	} else {
		/* Don't implement HDCP if sink as a repeater */
		adv7520_write_reg(hclient, 0x0C, (u8)0x84);
		mutex_lock(&hdcp_state_mutex);
		hdcp_activating = FALSE;
		mutex_unlock(&hdcp_state_mutex);
		DEV_WARN("HDCP: Sink Repeater (%02x), (unmute audio)\n",
			reg[0xbe]);

		adv7520_comm_power(0, 1);
		return;
	}

	msleep(200);
	reg[0xB8] = adv7520_read_reg(hclient, 0xB8);
	DEV_INFO("HDCP: Status reg[0xB8] is %02x\n", reg[0xb8]);
	if (reg[0xb8] & 0x40) {
		/* UnMute Audio */
		adv7520_write_reg(hclient, 0x0C, (u8)0x84);
		DEV_INFO("HDCP: A/V content Encrypted (unmute audio)\n");
		external_common_state->hdcp_active = TRUE;
	}
	adv7520_comm_power(0, 1);

	mutex_lock(&hdcp_state_mutex);
	hdcp_activating = FALSE;
	mutex_unlock(&hdcp_state_mutex);
}
#endif

static int adv7520_read_edid_block(int block, uint8 *edid_buf)
{
	u8 r = 0;
	int ret;
	struct i2c_msg msg[] = {
		{ .addr = reg[0x43] >> 1,
		  .flags = 0,
		  .len = 1,
		  .buf = &r },
		{ .addr = reg[0x43] >> 1,
		  .flags = I2C_M_RD,
		  .len = 0x100,
		  .buf = edid_buf } };

	if (block > 0)
		return 0;
	ret = i2c_transfer(hclient->adapter, msg, 2);
	DEV_DBG("EDID block: addr=%02x, ret=%d\n", reg[0x43] >> 1, ret);
	return (ret < 2) ? -ENODEV : 0;
}

static void adv7520_read_edid(void)
{
	external_common_state->read_edid_block = adv7520_read_edid_block;
	if (hdmi_common_read_edid()) {
		u8 timeout;
		DEV_INFO("%s: retry\n", __func__);
		adv7520_write_reg(hclient, 0xc9, 0x13);
		msleep(500);
		timeout = (adv7520_read_reg(hclient, 0x96) & (1 << 2));
		if (timeout) {
			hdmi_common_read_edid();
		}
	}
}

static void adv7520_chip_on(void)
{
	if (!chip_power_on) {
		/* Get the current register holding the power bit. */
		unsigned long reg0xaf = adv7520_read_reg(hclient, 0xaf);

		dd->pd->core_power(1, 1);

		/* Set the HDMI select bit. */
		set_bit(1, &reg0xaf);
		DEV_INFO("%s: turn on chip power\n", __func__);
		adv7520_write_reg(hclient, 0x41, 0x10);
		adv7520_write_reg(hclient, 0xaf, (u8)reg0xaf);
		chip_power_on = TRUE;
	} else
		DEV_INFO("%s: chip already has power\n", __func__);
}

static void adv7520_chip_off(void)
{
	if (chip_power_on) {
#ifdef CONFIG_FB_MSM_HDMI_ADV7520_PANEL_HDCP_SUPPORT
		if (has_hdcp_hw_support)
			adv7520_close_hdcp_link();
#endif

		DEV_INFO("%s: turn off chip power\n", __func__);
		adv7520_write_reg(hclient, 0x41, 0x50);
		dd->pd->core_power(0, 1);
		chip_power_on = FALSE;
	} else
		DEV_INFO("%s: chip is already off\n", __func__);

	monitor_sense = 0;
	hpd_cable_chg_detected = FALSE;

	if (enable_5v_on) {
		dd->pd->enable_5v(0);
		enable_5v_on = FALSE;
	}
}

/*  Power ON/OFF  ADV7520 chip */
static void adv7520_isr_w(struct work_struct *work);
static void adv7520_comm_power(int on, int show)
{
	if (!on)
		atomic_dec(&comm_power_on);
	dd->pd->comm_power(on, 0/*show*/);
	if (on)
		atomic_inc(&comm_power_on);
}

#ifdef CONFIG_FB_MSM_HDMI_ADV7520_PANEL_HDCP_SUPPORT
static void adv7520_start_hdcp(void);
#endif
static int adv7520_power_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd = platform_get_drvdata(pdev);

	clk_enable(tv_enc_clk);
	external_common_state->dev = &pdev->dev;
	if (mfd != NULL) {
		DEV_INFO("adv7520_power: ON (%dx%d %d)\n",
			mfd->var_xres, mfd->var_yres, mfd->var_pixclock);
		hdmi_common_get_video_format_from_drv_data(mfd);
	}

	adv7520_comm_power(1, 1);
	/* Check if HPD is signaled */
	if (adv7520_read_reg(hclient, 0x42) & (1 << 6)) {
		DEV_INFO("power_on: cable detected\n");
		monitor_sense = adv7520_read_reg(hclient, 0xC6);
#ifdef CONFIG_FB_MSM_HDMI_ADV7520_PANEL_HDCP_SUPPORT
		if (has_hdcp_hw_support) {
			if (!hdcp_activating)
				adv7520_start_hdcp();
		}
#endif
	} else
		DEV_INFO("power_on: cable NOT detected\n");
	adv7520_comm_power(0, 1);
	wake_lock(&wlock);

	return 0;
}

static int adv7520_power_off(struct platform_device *pdev)
{
	DEV_INFO("power_off\n");
	adv7520_comm_power(1, 1);
	adv7520_chip_off();
	wake_unlock(&wlock);
	adv7520_comm_power(0, 1);
	clk_disable(tv_enc_clk);
	return 0;
}


/* AV7520 chip specific initialization */
static void adv7520_chip_init(void)
{
	/* Initialize the variables used to read/write the ADV7520 chip. */
	memset(&reg, 0xff, sizeof(reg));

	/* Get the values from the "Fixed Registers That Must Be Set". */
	reg[0x98] = adv7520_read_reg(hclient, 0x98);
	reg[0x9c] = adv7520_read_reg(hclient, 0x9c);
	reg[0x9d] = adv7520_read_reg(hclient, 0x9d);
	reg[0xa2] = adv7520_read_reg(hclient, 0xa2);
	reg[0xa3] = adv7520_read_reg(hclient, 0xa3);
	reg[0xde] = adv7520_read_reg(hclient, 0xde);

	/* Get the "HDMI/DVI Selection" register. */
	reg[0xaf] = adv7520_read_reg(hclient, 0xaf);

	/* Read Packet Memory I2C Address */
	reg[0x45] = adv7520_read_reg(hclient, 0x45);

	/* Hard coded values provided by ADV7520 data sheet. */
	reg[0x98] = 0x03;
	reg[0x9c] = 0x38;
	reg[0x9d] = 0x61;
	reg[0xa2] = 0x94;
	reg[0xa3] = 0x94;
	reg[0xde] = 0x88;

	/* Set the HDMI select bit. */
	reg[0xaf] |= 0x16;

	/* Set the audio related registers. */
	reg[0x01] = 0x00;
	reg[0x02] = 0x2d;
	reg[0x03] = 0x80;
	reg[0x0a] = 0x4d;
	reg[0x0b] = 0x0e;
	reg[0x0c] = 0x84;
	reg[0x0d] = 0x10;
	reg[0x12] = 0x00;
	reg[0x14] = 0x00;
	reg[0x15] = 0x20;
	reg[0x44] = 0x79;
	reg[0x73] = 0x01;
	reg[0x76] = 0x00;

	/* Set 720p display related registers */
	reg[0x16] = 0x00;

	reg[0x18] = 0x46;
	reg[0x55] = 0x00;
	reg[0x3c] = 0x04;

	/* Set Interrupt Mask register for HPD/HDCP */
	reg[0x94] = 0xC0;
#ifdef CONFIG_FB_MSM_HDMI_ADV7520_PANEL_HDCP_SUPPORT
	if (has_hdcp_hw_support)
		reg[0x95] = 0xC0;
	else
		reg[0x95] = 0x00;
#else
	reg[0x95] = 0x00;
#endif
	adv7520_write_reg(hclient, 0x94, reg[0x94]);
	adv7520_write_reg(hclient, 0x95, reg[0x95]);

	/* Set Packet Memory I2C Address */
	reg[0x45] = 0x74;

	/* Set the values from the "Fixed Registers That Must Be Set". */
	adv7520_write_reg(hclient, 0x98, reg[0x98]);
	adv7520_write_reg(hclient, 0x9c, reg[0x9c]);
	adv7520_write_reg(hclient, 0x9d, reg[0x9d]);
	adv7520_write_reg(hclient, 0xa2, reg[0xa2]);
	adv7520_write_reg(hclient, 0xa3, reg[0xa3]);
	adv7520_write_reg(hclient, 0xde, reg[0xde]);

	/* Set the "HDMI/DVI Selection" register. */
	adv7520_write_reg(hclient, 0xaf, reg[0xaf]);

	/* Set EDID Monitor address */
	reg[0x43] = 0x7E;
	adv7520_write_reg(hclient, 0x43, reg[0x43]);

	/* Enable the i2s audio input. */
	adv7520_write_reg(hclient, 0x01, reg[0x01]);
	adv7520_write_reg(hclient, 0x02, reg[0x02]);
	adv7520_write_reg(hclient, 0x03, reg[0x03]);
	adv7520_write_reg(hclient, 0x0a, reg[0x0a]);
	adv7520_write_reg(hclient, 0x0b, reg[0x0b]);
	adv7520_write_reg(hclient, 0x0c, reg[0x0c]);
	adv7520_write_reg(hclient, 0x0d, reg[0x0d]);
	adv7520_write_reg(hclient, 0x12, reg[0x12]);
	adv7520_write_reg(hclient, 0x14, reg[0x14]);
	adv7520_write_reg(hclient, 0x15, reg[0x15]);
	adv7520_write_reg(hclient, 0x44, reg[0x44]);
	adv7520_write_reg(hclient, 0x73, reg[0x73]);
	adv7520_write_reg(hclient, 0x76, reg[0x76]);

	/* Enable 720p display */
	adv7520_write_reg(hclient, 0x16, reg[0x16]);
	adv7520_write_reg(hclient, 0x18, reg[0x18]);
	adv7520_write_reg(hclient, 0x55, reg[0x55]);
	adv7520_write_reg(hclient, 0x3c, reg[0x3c]);

	/* Set Packet Memory address to avoid conflict
	with Bosch Accelerometer */
	adv7520_write_reg(hclient, 0x45, reg[0x45]);

	/* Ensure chip is in low-power state */
	adv7520_write_reg(hclient, 0x41, 0x50);
}

#ifdef CONFIG_FB_MSM_HDMI_ADV7520_PANEL_HDCP_SUPPORT
static void adv7520_start_hdcp(void)
{
	mutex_lock(&hdcp_state_mutex);
	if (hdcp_activating) {
		DEV_WARN("adv7520_timer: HDCP already"
			" activating, skipping\n");
		mutex_unlock(&hdcp_state_mutex);
		return;
	}
	hdcp_activating = TRUE;
	mutex_unlock(&hdcp_state_mutex);

	del_timer(&hpd_duty_timer);

	adv7520_comm_power(1, 1);

	if (!enable_5v_on) {
		dd->pd->enable_5v(1);
		enable_5v_on = TRUE;
		adv7520_chip_on();
	}

	/* request for HDCP */
	reg[0xaf] = adv7520_read_reg(hclient, 0xaf);
	reg[0xaf] |= 0x90;
	adv7520_write_reg(hclient, 0xaf, reg[0xaf]);
	reg[0xaf] = adv7520_read_reg(hclient, 0xaf);

	reg[0xba] = adv7520_read_reg(hclient, 0xba);
	reg[0xba] |= 0x10;
	adv7520_write_reg(hclient, 0xba, reg[0xba]);
	reg[0xba] = adv7520_read_reg(hclient, 0xba);
	adv7520_comm_power(0, 1);

	DEV_INFO("HDCP: reg[0xaf]=0x%02x, reg[0xba]=0x%02x, waiting for BKSV\n",
				reg[0xaf], reg[0xba]);

	/* will check for HDCP Error or BKSV ready */
	mod_timer(&hpd_duty_timer, jiffies + HZ/2);
}
#endif

static void adv7520_hpd_timer_w(struct work_struct *work)
{
	if (!external_common_state->hpd_feature_on) {
		DEV_INFO("adv7520_timer: skipping, feature off\n");
		return;
	}

	if ((monitor_sense & 0x4) && !external_common_state->hpd_state) {
		int timeout;
		DEV_DBG("adv7520_timer: Cable Detected\n");
		adv7520_comm_power(1, 1);
		adv7520_chip_on();

		if (hpd_cable_chg_detected) {
			hpd_cable_chg_detected = FALSE;
			/* Ensure 5V to read EDID */
			if (!enable_5v_on) {
				dd->pd->enable_5v(1);
				enable_5v_on = TRUE;
			}
			msleep(500);
			timeout = (adv7520_read_reg(hclient, 0x96) & (1 << 2));
			if (timeout) {
				DEV_DBG("adv7520_timer: EDID-Ready..\n");
				adv7520_read_edid();
			} else
				DEV_DBG("adv7520_timer: EDID TIMEOUT (C9=%02x)"
					"\n", adv7520_read_reg(hclient, 0xC9));
		}
#ifdef TESTING_FORCE_480p
		external_common_state->disp_mode_list.num_of_elements = 1;
		external_common_state->disp_mode_list.disp_mode_list[0] =
			HDMI_VFRMT_720x480p60_16_9;
#endif
		adv7520_comm_power(0, 1);
#ifndef CONFIG_FB_MSM_HDMI_ADV7520_PANEL_HDCP_SUPPORT
		/* HDMI_5V_EN not needed anymore */
		if (enable_5v_on) {
			DEV_DBG("adv7520_timer: EDID done, no HDCP, 5V not "
				"needed anymore\n");
			dd->pd->enable_5v(0);
			enable_5v_on = FALSE;
		}
#endif
		change_hdmi_state(1);
	} else if (external_common_state->hpd_state) {
		adv7520_comm_power(1, 1);
		adv7520_chip_off();
		adv7520_comm_power(0, 1);
		DEV_DBG("adv7520_timer: Cable Removed\n");
		change_hdmi_state(0);
	}
}

static void adv7520_hpd_timer_f(unsigned long data)
{
	schedule_work(&hpd_timer_work);
}

static void adv7520_isr_w(struct work_struct *work)
{
	static int state_count;
	static u8 last_reg0x96;
	u8 reg0xc8;
	u8 reg0x96;
#ifdef CONFIG_FB_MSM_HDMI_ADV7520_PANEL_HDCP_SUPPORT
	static u8 last_reg0x97;
	u8 reg0x97 = 0;
#endif
	if (!external_common_state->hpd_feature_on) {
		DEV_DBG("adv7520_irq: skipping, hpd off\n");
		return;
	}

	adv7520_comm_power(1, 1);
	reg0x96 = adv7520_read_reg(hclient, 0x96);
#ifdef CONFIG_FB_MSM_HDMI_ADV7520_PANEL_HDCP_SUPPORT
	if (has_hdcp_hw_support) {
		reg0x97 = adv7520_read_reg(hclient, 0x97);
		/* Clearing the Interrupts */
		adv7520_write_reg(hclient, 0x97, reg0x97);
	}
#endif
	/* Clearing the Interrupts */
	adv7520_write_reg(hclient, 0x96, reg0x96);

	if ((reg0x96 == 0xC0) || (reg0x96 & 0x40)) {
#ifdef DEBUG
		unsigned int hpd_state = adv7520_read_reg(hclient, 0x42);
#endif
		monitor_sense = adv7520_read_reg(hclient, 0xC6);
		DEV_DBG("adv7520_irq: reg[0x42]=%02x && reg[0xC6]=%02x\n",
			hpd_state, monitor_sense);

		if (!enable_5v_on) {
			dd->pd->enable_5v(1);
			enable_5v_on = TRUE;
		}
		if (!hpd_power_on) {
			dd->pd->core_power(1, 1);
			hpd_power_on = TRUE;
		}

		/* Timer for catching interrupt debouning */
		DEV_DBG("adv7520_irq: Timer in .5sec\n");
		hpd_cable_chg_detected = TRUE;
		mod_timer(&hpd_timer, jiffies + HZ/2);
	}
#ifdef CONFIG_FB_MSM_HDMI_ADV7520_PANEL_HDCP_SUPPORT
	if (has_hdcp_hw_support) {
		if (hdcp_activating) {
			/* HDCP controller error Interrupt */
			if (reg0x97 & 0x80) {
				DEV_ERR("adv7520_irq: HDCP_ERROR\n");
				state_count = 0;
				adv7520_close_hdcp_link();
			/* BKSV Ready interrupts */
			} else if (reg0x97 & 0x40) {
				DEV_INFO("adv7520_irq: BKSV keys ready, Begin"
					" HDCP encryption\n");
				state_count = 0;
				schedule_work(&hdcp_handle_work);
			} else if (++state_count > 2 && (monitor_sense & 0x4)) {
				DEV_INFO("adv7520_irq: Still waiting for BKSV,"
				"restart HDCP\n");
				hdcp_activating = FALSE;
				state_count = 0;
				adv7520_chip_off();
				adv7520_start_hdcp();
			}
			reg0xc8 = adv7520_read_reg(hclient, 0xc8);
			DEV_INFO("adv7520_irq: DDC controller reg[0xC8]=0x%02x,"
				"state_count=%d, monitor_sense=%x\n",
				reg0xc8, state_count, monitor_sense);
		} else if (!external_common_state->hdcp_active
			&& (monitor_sense & 0x4)) {
			DEV_INFO("adv7520_irq: start HDCP with"
				" monitor sense\n");
			state_count = 0;
			adv7520_start_hdcp();
		} else
			state_count = 0;
		if (last_reg0x97 != reg0x97 || last_reg0x96 != reg0x96)
			DEV_DBG("adv7520_irq: reg[0x96]=%02x "
				"reg[0x97]=%02x: HDCP: %d\n", reg0x96, reg0x97,
				external_common_state->hdcp_active);
		last_reg0x97 = reg0x97;
	} else {
		if (last_reg0x96 != reg0x96)
			DEV_DBG("adv7520_irq: reg[0x96]=%02x\n", reg0x96);
	}
#else
	if (last_reg0x96 != reg0x96)
		DEV_DBG("adv7520_irq: reg[0x96]=%02x\n", reg0x96);
#endif
	last_reg0x96 = reg0x96;
	adv7520_comm_power(0, 1);
}

static void adv7520_hpd_duty_work(struct work_struct *work)
{
	if (!external_common_state->hpd_feature_on) {
		DEV_WARN("%s: hpd feature is off, skipping\n", __func__);
		return;
	}

	dd->pd->core_power(1, 0);
	msleep(10);
	adv7520_isr_w(NULL);
	dd->pd->core_power(0, 0);
}

static void adv7520_hpd_duty_timer_f(unsigned long data)
{
	if (!external_common_state->hpd_feature_on) {
		DEV_WARN("%s: hpd feature is off, skipping\n", __func__);
		return;
	}

	mod_timer(&hpd_duty_timer, jiffies + HPD_DUTY_CYCLE*HZ);
	schedule_work(&hpd_duty_work);
}

static const struct i2c_device_id adv7520_id[] = {
	{ ADV7520_DRV_NAME , 0},
	{}
};

static struct msm_fb_panel_data hdmi_panel_data = {
	.on  = adv7520_power_on,
	.off = adv7520_power_off,
};

static struct platform_device hdmi_device = {
	.name = ADV7520_DRV_NAME ,
	.id   = 2,
	.dev  = {
		.platform_data = &hdmi_panel_data,
		}
};

static void adv7520_ensure_init(void)
{
	static boolean init_done;
	if (!init_done) {
		int rc = dd->pd->init_irq();
		if (rc) {
			DEV_ERR("adv7520_init: init_irq: %d\n", rc);
			return;
		}

		init_done = TRUE;
	}
	DEV_INFO("adv7520_init: chip init\n");
	adv7520_comm_power(1, 1);
	adv7520_chip_init();
	adv7520_comm_power(0, 1);
}

static int adv7520_hpd_feature(int on)
{
	int rc = 0;

	if (!on) {
		if (enable_5v_on) {
			dd->pd->enable_5v(0);
			enable_5v_on = FALSE;
		}
		if (hpd_power_on) {
			dd->pd->core_power(0, 1);
			hpd_power_on = FALSE;
		}

		DEV_DBG("adv7520_hpd: %d: stop duty timer\n", on);
		del_timer(&hpd_timer);
		del_timer(&hpd_duty_timer);
		external_common_state->hpd_state = 0;
	}

	if (on) {
		dd->pd->core_power(1, 0);
		adv7520_ensure_init();

		adv7520_comm_power(1, 1);
		monitor_sense = adv7520_read_reg(hclient, 0xC6);
		DEV_DBG("adv7520_irq: reg[0xC6]=%02x\n", monitor_sense);
		adv7520_comm_power(0, 1);
		dd->pd->core_power(0, 0);

		if (monitor_sense & 0x4) {
			if (!enable_5v_on) {
				dd->pd->enable_5v(1);
				enable_5v_on = TRUE;
			}
			if (!hpd_power_on) {
				dd->pd->core_power(1, 1);
				hpd_power_on = TRUE;
			}

			hpd_cable_chg_detected = TRUE;
			mod_timer(&hpd_timer, jiffies + HZ/2);
		}

		DEV_DBG("adv7520_hpd: %d start duty timer\n", on);
		mod_timer(&hpd_duty_timer, jiffies + HZ/100);
	}

	DEV_INFO("adv7520_hpd: %d\n", on);
	return rc;
}

static int __devinit
	adv7520_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int rc;
	struct platform_device *fb_dev;

	dd = kzalloc(sizeof *dd, GFP_KERNEL);
	if (!dd) {
		rc = -ENOMEM;
		goto probe_exit;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;

	external_common_state->dev = &client->dev;

	/* Init real i2c_client */
	hclient = client;

	i2c_set_clientdata(client, dd);
	dd->pd = client->dev.platform_data;
	if (!dd->pd) {
		rc = -ENODEV;
		goto probe_free;
	}

	INIT_WORK(&dd->isr_work, adv7520_isr_w);
	INIT_WORK(&hpd_timer_work, adv7520_hpd_timer_w);
#ifdef CONFIG_FB_MSM_HDMI_ADV7520_PANEL_HDCP_SUPPORT
	if (dd->pd->check_hdcp_hw_support)
		has_hdcp_hw_support = dd->pd->check_hdcp_hw_support();

	if (has_hdcp_hw_support)
		INIT_WORK(&hdcp_handle_work, adv7520_hdcp_enable);
	else
		DEV_INFO("%s: no hdcp hw support.\n", __func__);
#endif

	init_timer(&hpd_timer);
	hpd_timer.function = adv7520_hpd_timer_f;
	hpd_timer.data = (unsigned long)NULL;
	hpd_timer.expires = 0xffffffff;
	add_timer(&hpd_timer);

	external_common_state->hpd_feature = adv7520_hpd_feature;
	DEV_INFO("adv7520_probe: HPD detection on request\n");
	init_timer(&hpd_duty_timer);
	hpd_duty_timer.function = adv7520_hpd_duty_timer_f;
	hpd_duty_timer.data = (unsigned long)NULL;
	hpd_duty_timer.expires = 0xffffffff;
	add_timer(&hpd_duty_timer);
	INIT_WORK(&hpd_duty_work, adv7520_hpd_duty_work);
	DEV_INFO("adv7520_probe: HPD detection ON (duty)\n");

	fb_dev = msm_fb_add_device(&hdmi_device);

	if (fb_dev) {
		rc = external_common_state_create(fb_dev);
		if (rc)
			goto probe_free;
	} else
		DEV_ERR("adv7520_probe: failed to add fb device\n");

#ifdef CONFIG_FB_MSM_HDMI_AS_PRIMARY
	external_common_state->sdev.name = "hdmi_as_primary";
#else
	external_common_state->sdev.name = "hdmi";
#endif
	if (switch_dev_register(&external_common_state->sdev) < 0)
		DEV_ERR("Hdmi switch registration failed\n");

	return 0;

probe_free:
	kfree(dd);
	dd = NULL;
probe_exit:
	return rc;

}

static int __devexit adv7520_remove(struct i2c_client *client)
{
	if (!client->adapter) {
		DEV_ERR("%s: No HDMI Device\n", __func__);
		return -ENODEV;
	}
	switch_dev_unregister(&external_common_state->sdev);
	wake_lock_destroy(&wlock);
	kfree(dd);
	dd = NULL;
	return 0;
}

#ifdef CONFIG_SUSPEND
static int adv7520_i2c_suspend(struct device *dev)
{
	DEV_INFO("%s\n", __func__);

	++suspend_count;

	if (external_common_state->hpd_feature_on) {
		DEV_DBG("%s: stop duty timer\n", __func__);
		del_timer(&hpd_duty_timer);
		del_timer(&hpd_timer);
	}

	/* Turn off LDO8 and go into low-power state */
	if (chip_power_on) {
		DEV_DBG("%s: turn off power\n", __func__);
		adv7520_comm_power(1, 1);
		adv7520_write_reg(hclient, 0x41, 0x50);
		adv7520_comm_power(0, 1);
		dd->pd->core_power(0, 1);
	}

	return 0;
}

static int adv7520_i2c_resume(struct device *dev)
{
	DEV_INFO("%s\n", __func__);

	/* Turn on LDO8 and go into normal-power state */
	if (chip_power_on) {
		DEV_DBG("%s: turn on power\n", __func__);
		dd->pd->core_power(1, 1);
		adv7520_comm_power(1, 1);
		adv7520_write_reg(hclient, 0x41, 0x10);
		adv7520_comm_power(0, 1);
	}

	if (external_common_state->hpd_feature_on) {
		DEV_DBG("%s: start duty timer\n", __func__);
		mod_timer(&hpd_duty_timer, jiffies + HPD_DUTY_CYCLE*HZ);
	}

	return 0;
}
#else
#define adv7520_i2c_suspend	NULL
#define adv7520_i2c_resume	NULL
#endif

static const struct dev_pm_ops adv7520_device_pm_ops = {
	.suspend = adv7520_i2c_suspend,
	.resume = adv7520_i2c_resume,
};

static struct i2c_driver hdmi_i2c_driver = {
	.driver		= {
		.name   = ADV7520_DRV_NAME,
		.owner  = THIS_MODULE,
		.pm     = &adv7520_device_pm_ops,
	},
	.probe		= adv7520_probe,
	.id_table	= adv7520_id,
	.remove		= __devexit_p(adv7520_remove),
};

static int __init adv7520_init(void)
{
	int rc;

	pr_info("%s\n", __func__);
	external_common_state = &hdmi_common;
	external_common_state->video_resolution = HDMI_VFRMT_1280x720p60_16_9;

	tv_enc_clk = clk_get(NULL, "tv_enc_clk");
	if (IS_ERR(tv_enc_clk)) {
		printk(KERN_ERR "error: can't get tv_enc_clk!\n");
		return IS_ERR(tv_enc_clk);
	}

	HDMI_SETUP_LUT(640x480p60_4_3);		/* 25.20MHz */
	HDMI_SETUP_LUT(720x480p60_16_9);	/* 27.03MHz */
	HDMI_SETUP_LUT(1280x720p60_16_9);	/* 74.25MHz */

	HDMI_SETUP_LUT(720x576p50_16_9);	/* 27.00MHz */
	HDMI_SETUP_LUT(1280x720p50_16_9);	/* 74.25MHz */

	hdmi_common_init_panel_info(&hdmi_panel_data.panel_info);

	rc = i2c_add_driver(&hdmi_i2c_driver);
	if (rc) {
		pr_err("hdmi_init FAILED: i2c_add_driver rc=%d\n", rc);
		goto init_exit;
	}

	if (machine_is_msm7x30_surf() || machine_is_msm8x55_surf()) {
		short *hdtv_mux = (short *)ioremap(0x8e000170 , 0x100);
		*hdtv_mux++ = 0x020b;
		*hdtv_mux = 0x8000;
		iounmap(hdtv_mux);
	}
	wake_lock_init(&wlock, WAKE_LOCK_IDLE, "hdmi_active");

	return 0;

init_exit:
	if (tv_enc_clk)
		clk_put(tv_enc_clk);
	return rc;
}

static void __exit adv7520_exit(void)
{
	i2c_del_driver(&hdmi_i2c_driver);
}

module_init(adv7520_init);
module_exit(adv7520_exit);
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.1");
MODULE_AUTHOR("Qualcomm Innovation Center, Inc.");
MODULE_DESCRIPTION("ADV7520 HDMI driver");
