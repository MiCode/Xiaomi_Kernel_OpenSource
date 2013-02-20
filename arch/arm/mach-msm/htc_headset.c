/*
 *  H2W device detection driver.
 *
 *  Copyright (C) 2008 Google, Inc.
 *  Copyright (C) 2008 HTC, Inc.
 *
 *  Authors:
 *      Laurence Chen <Laurence_Chen@htc.com>
 *      Nick Pelly <npelly@google.com>
 *      Thomas Tsai <thomas_tsai@htc.com>
 *      Farmer Tseng <farmer_tseng@htc.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 */

/*  For detecting HTC 2 Wire devices, such as wired headset.

    Logically, the H2W driver is always present, and H2W state (hi->state)
    indicates what is currently plugged into the H2W interface.

    When the headset is plugged in, CABLE_IN1 is pulled low. When the headset
    button is pressed, CABLE_IN2 is pulled low. These two lines are shared with
    the TX and RX (respectively) of UART3 - used for serial debugging.

    This headset driver keeps the CPLD configured as UART3 for as long as
    possible, so that we can do serial FIQ debugging even when the kernel is
    locked and this driver no longer runs. So it only configures the CPLD to
    GPIO while the headset is plugged in, and for 10ms during detection work.

    Unfortunately we can't leave the CPLD as UART3 while a headset is plugged
    in, UART3 is pullup on TX but the headset is pull-down, causing a 55 mA
    drain on trout.

    The headset detection work involves setting CPLD to GPIO, and then pulling
    CABLE_IN1 high with a stronger pullup than usual. A H2W headset will still
    pull this line low, whereas other attachments such as a serial console
    would get pulled up by this stronger pullup.

    Headset insertion/removal causes UEvent's to be sent, and
    /sys/class/switch/h2w/state to be updated.

    Button presses are interpreted as input event (KEY_MEDIA). Button presses
    are ignored if the headset is plugged in, so the buttons on 11 pin -> 3.5mm
    jack adapters do not work until a headset is plugged into the adapter. This
    is to avoid serial RX traffic causing spurious button press events.

    We tend to check the status of CABLE_IN1 a few more times than strictly
    necessary during headset detection, to avoid spurious headset insertion
    events caused by serial debugger TX traffic.
*/

#include <linux/module.h>
#include <linux/sysdev.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/switch.h>
#include <linux/input.h>
#include <linux/debugfs.h>
#include <asm/gpio.h>
#include <asm/atomic.h>
#include <mach/board.h>
#include <mach/vreg.h>
#include <asm/mach-types.h>

#include <mach/htc_headset.h>

#define H2WI(fmt, arg...) \
	printk(KERN_INFO "[H2W] %s " fmt "\r\n", __func__, ## arg)
#define H2WE(fmt, arg...) \
	printk(KERN_ERR "[H2W] %s " fmt "\r\n", __func__, ## arg)

#ifdef CONFIG_DEBUG_H2W
#define H2W_DBG(fmt, arg...) printk(KERN_INFO "[H2W] %s " fmt "\r\n", __func__, ## arg)
#else
#define H2W_DBG(fmt, arg...) do {} while (0)
#endif

static struct workqueue_struct *g_detection_work_queue;
static void detection_work(struct work_struct *work);
static DECLARE_WORK(g_detection_work, detection_work);

struct h2w_info {
	struct switch_dev sdev;
	struct input_dev *input;
	struct mutex mutex_lock;

	atomic_t btn_state;
	int ignore_btn;

	unsigned int irq;
	unsigned int irq_btn;

	int cable_in1;
	int cable_in2;
	int h2w_clk;
	int h2w_data;
	int debug_uart;

	void (*config_cpld) (int);
	void (*init_cpld) (void);
	/* for h2w */
	void (*set_dat)(int);
	void (*set_clk)(int);
	void (*set_dat_dir)(int);
	void (*set_clk_dir)(int);
	int (*get_dat)(void);
	int (*get_clk)(void);

	int htc_headset_flag;

	struct hrtimer timer;
	ktime_t debounce_time;

	struct hrtimer btn_timer;
	ktime_t btn_debounce_time;

	H2W_INFO h2w_info;
	H2W_SPEED speed;
	struct vreg *vreg_h2w;
};
static struct h2w_info *hi;

static ssize_t h2w_print_name(struct switch_dev *sdev, char *buf)
{
	switch (switch_get_state(&hi->sdev)) {
	case H2W_NO_DEVICE:
		return sprintf(buf, "No Device\n");
	case H2W_HTC_HEADSET:
		return sprintf(buf, "Headset\n");
	}
	return -EINVAL;
}

static void button_pressed(void)
{
	H2W_DBG("button_pressed \n");
	atomic_set(&hi->btn_state, 1);
	input_report_key(hi->input, KEY_MEDIA, 1);
	input_sync(hi->input);
}

static void button_released(void)
{
	H2W_DBG("button_released \n");
	atomic_set(&hi->btn_state, 0);
	input_report_key(hi->input, KEY_MEDIA, 0);
	input_sync(hi->input);
}

/*****************
 * H2W proctocol *
 *****************/
static inline void h2w_begin_command(void)
{
	/* Disable H2W interrupt */
	set_irq_type(hi->irq_btn, IRQF_TRIGGER_HIGH);
	disable_irq(hi->irq);
	disable_irq(hi->irq_btn);

	/* Set H2W_CLK as output low */
	hi->set_clk(0);
	hi->set_clk_dir(1);
}

static inline void h2w_end_command(void)
{
	/* Set H2W_CLK as input */
	hi->set_clk_dir(0);

	/* Enable H2W interrupt */
	enable_irq(hi->irq);
	enable_irq(hi->irq_btn);
	set_irq_type(hi->irq_btn, IRQF_TRIGGER_RISING);
}

/*
 * One bit write data
 *                     ________
 *       SCLK O ______|        |______O(L)
 *
 *
 *	     SDAT I <XXXXXXXXXXXXXXXXXXXX>
 */
static inline void one_clock_write(unsigned short flag)
{
	if (flag)
		hi->set_dat(1);
	else
		hi->set_dat(0);

	udelay(hi->speed);
	hi->set_clk(1);
	udelay(hi->speed);
	hi->set_clk(0);
}

/*
 * One bit write data R/W bit
 *                   ________
 *       SCLK ______|        |______O(L)
 *            1---->         1----->
 *                  2-------> ______
 *	 SDAT <XXXXXXXXXXXXXX>      I
 *                         O(H/L)
 */
static inline void one_clock_write_RWbit(unsigned short flag)
{
	if (flag)
		hi->set_dat(1);
	else
		hi->set_dat(0);

	udelay(hi->speed);
	hi->set_clk(1);
	udelay(hi->speed);
	hi->set_clk(0);
	hi->set_dat_dir(0);
	udelay(hi->speed);
}

/*
 * H2W Reset
 *                       ___________
 *       SCLK O(L)______|           |___O(L)
 *                1---->
 *                      4-->1-->1-->1us-->
 *                          ____
 *       SDAT O(L)________ |    |_______O(L)
 *
 * H2w reset command needs to be issued before every access
 */
static inline void h2w_reset(void)
{
	/* Set H2W_DAT as output low */
	hi->set_dat(0);
	hi->set_dat_dir(1);

	udelay(hi->speed);
	hi->set_clk(1);
	udelay(4 * hi->speed);
	hi->set_dat(1);
	udelay(hi->speed);
	hi->set_dat(0);
	udelay(hi->speed);
	hi->set_clk(0);
	udelay(hi->speed);
}

/*
 * H2W Start
 *                       ___________
 *       SCLK O(L)______|           |___O(L)
 *                1---->
 *                      2----------->1-->
 *
 *       SDAT O(L)______________________O(L)
 */
static inline void h2w_start(void)
{
	udelay(hi->speed);
	hi->set_clk(1);
	udelay(2 * hi->speed);
	hi->set_clk(0);
	udelay(hi->speed);
}

/*
 * H2W Ack
 *                  __________
 *       SCLK _____|          |_______O(L)
 *            1---->	      1------>
 *		           2--------->
 *            ________________________
 *	 SDAT  become Input mode here I
 */
static inline int h2w_ack(void)
{
	int retry_times = 0;

ack_resend:
	if (retry_times == MAX_ACK_RESEND_TIMES)
		return -1;

	udelay(hi->speed);
	hi->set_clk(1);
	udelay(2 * hi->speed);

	if (!hi->get_dat()) {
		retry_times++;
		hi->set_clk(0);
		udelay(hi->speed);
		goto ack_resend;
	}

	hi->set_clk(0);
	udelay(hi->speed);
	return 0;
}

/*
 * One bit read data
 *                   ________
 *       SCLK ______|        |______O(L)
 *            2---->         2----->
 *                  2------->
 *	 SDAT <XXXXXXXXXXXXXXXXXXXX>I
 */
static unsigned char h2w_readc(void)
{
	unsigned char h2w_read_data = 0x0;
	int index;

	for (index = 0; index < 8; index++) {
		hi->set_clk(0);
		udelay(hi->speed);
		hi->set_clk(1);
		udelay(hi->speed);
		if (hi->get_dat())
			h2w_read_data |= (1 << (7 - index));
	}
	hi->set_clk(0);
	udelay(hi->speed);

	return h2w_read_data;
}

static int h2w_readc_cmd(H2W_ADDR address)
{
	int ret = -1, retry_times = 0;
	unsigned char read_data;

read_resend:
	if (retry_times == MAX_HOST_RESEND_TIMES)
		goto err_read;

	h2w_reset();
	h2w_start();
	/* Write address */
	one_clock_write(address & 0x1000);
	one_clock_write(address & 0x0800);
	one_clock_write(address & 0x0400);
	one_clock_write(address & 0x0200);
	one_clock_write(address & 0x0100);
	one_clock_write(address & 0x0080);
	one_clock_write(address & 0x0040);
	one_clock_write(address & 0x0020);
	one_clock_write(address & 0x0010);
	one_clock_write(address & 0x0008);
	one_clock_write(address & 0x0004);
	one_clock_write(address & 0x0002);
	one_clock_write(address & 0x0001);
	one_clock_write_RWbit(1);
	if (h2w_ack() < 0) {
		H2W_DBG("Addr NO ACK(%d).\n", retry_times);
		retry_times++;
		hi->set_clk(0);
		mdelay(RESEND_DELAY);
		goto read_resend;
	}

	read_data = h2w_readc();

	if (h2w_ack() < 0) {
		H2W_DBG("Data NO ACK(%d).\n", retry_times);
		retry_times++;
		hi->set_clk(0);
		mdelay(RESEND_DELAY);
		goto read_resend;
	}
	ret = (int)read_data;

err_read:
	if (ret < 0)
		H2WE("NO ACK.\n");

	return ret;
}

static int h2w_writec_cmd(H2W_ADDR address, unsigned char data)
{
	int ret = -1;
	int retry_times = 0;

write_resend:
	if (retry_times == MAX_HOST_RESEND_TIMES)
		goto err_write;

	h2w_reset();
	h2w_start();

	/* Write address */
	one_clock_write(address & 0x1000);
	one_clock_write(address & 0x0800);
	one_clock_write(address & 0x0400);
	one_clock_write(address & 0x0200);
	one_clock_write(address & 0x0100);
	one_clock_write(address & 0x0080);
	one_clock_write(address & 0x0040);
	one_clock_write(address & 0x0020);
	one_clock_write(address & 0x0010);
	one_clock_write(address & 0x0008);
	one_clock_write(address & 0x0004);
	one_clock_write(address & 0x0002);
	one_clock_write(address & 0x0001);
	one_clock_write_RWbit(0);
	if (h2w_ack() < 0) {
		H2W_DBG("Addr NO ACK(%d).\n", retry_times);
		retry_times++;
		hi->set_clk(0);
		mdelay(RESEND_DELAY);
		goto write_resend;
	}

	/* Write data */
	hi->set_dat_dir(1);
	one_clock_write(data & 0x0080);
	one_clock_write(data & 0x0040);
	one_clock_write(data & 0x0020);
	one_clock_write(data & 0x0010);
	one_clock_write(data & 0x0008);
	one_clock_write(data & 0x0004);
	one_clock_write(data & 0x0002);
	one_clock_write_RWbit(data & 0x0001);
	if (h2w_ack() < 0) {
		H2W_DBG("Data NO ACK(%d).\n", retry_times);
		retry_times++;
		hi->set_clk(0);
		mdelay(RESEND_DELAY);
		goto write_resend;
	}
	ret = 0;

err_write:
	if (ret < 0)
		H2WE("NO ACK.\n");

	return ret;
}

static int h2w_get_fnkey(void)
{
	int ret;
	h2w_begin_command();
	ret = h2w_readc_cmd(H2W_FNKEY_UPDOWN);
	h2w_end_command();
	return ret;
}

static int h2w_dev_init(H2W_INFO *ph2w_info)
{
	int ret = -1;
	unsigned char ascr0 = 0;
	int h2w_sys = 0, maxgpadd = 0, maxadd = 0, key = 0;

	hi->speed = H2W_50KHz;
	h2w_begin_command();

	/* read H2W_SYSTEM */
	h2w_sys = h2w_readc_cmd(H2W_SYSTEM);
	if (h2w_sys == -1) {
		H2WE("read H2W_SYSTEM(0x0000) failed.\n");
		goto err_plugin;
	}
	ph2w_info->ACC_CLASS = (h2w_sys & 0x03);
	ph2w_info->AUDIO_DEVICE  = (h2w_sys & 0x04) > 0 ? 1 : 0;
	ph2w_info->HW_REV = (h2w_sys & 0x18) >> 3;
	ph2w_info->SLEEP_PR  = (h2w_sys & 0x20) >> 5;
	ph2w_info->CLK_SP = (h2w_sys & 0xC0) >> 6;

	/* enter init mode */
	if (h2w_writec_cmd(H2W_ASCR0, H2W_ASCR_DEVICE_INI) < 0) {
		H2WE("write H2W_ASCR0(0x0002) failed.\n");
		goto err_plugin;
	}
	udelay(10);

	/* read H2W_MAX_GP_ADD */
	maxgpadd = h2w_readc_cmd(H2W_MAX_GP_ADD);
	if (maxgpadd == -1) {
		H2WE("write H2W_MAX_GP_ADD(0x0001) failed.\n");
		goto err_plugin;
	}
	ph2w_info->CLK_SP += (maxgpadd & 0x60) >> 3;
	ph2w_info->MAX_GP_ADD = (maxgpadd & 0x1F);

	/* read key group */
	if (ph2w_info->MAX_GP_ADD >= 1) {
		ph2w_info->KEY_MAXADD = h2w_readc_cmd(H2W_KEY_MAXADD);
		if (ph2w_info->KEY_MAXADD == -1)
			goto err_plugin;
		if (ph2w_info->KEY_MAXADD >= 1) {
			key = h2w_readc_cmd(H2W_ASCII_DOWN);
			if (key < 0)
				goto err_plugin;
			ph2w_info->ASCII_DOWN = (key == 0xFF) ? 1 : 0;
		}
		if (ph2w_info->KEY_MAXADD >= 2) {
			key = h2w_readc_cmd(H2W_ASCII_UP);
			if (key == -1)
				goto err_plugin;
			ph2w_info->ASCII_UP = (key == 0xFF) ? 1 : 0;
		}
		if (ph2w_info->KEY_MAXADD >= 3) {
			key = h2w_readc_cmd(H2W_FNKEY_UPDOWN);
			if (key == -1)
				goto err_plugin;
			ph2w_info->FNKEY_UPDOWN = (key == 0xFF) ? 1 : 0;
		}
		if (ph2w_info->KEY_MAXADD >= 4) {
			key = h2w_readc_cmd(H2W_KD_STATUS);
			if (key == -1)
				goto err_plugin;
			ph2w_info->KD_STATUS = (key == 0x01) ? 1 : 0;
		}
	}

	/* read led group */
	if (ph2w_info->MAX_GP_ADD >= 2) {
		ph2w_info->LED_MAXADD = h2w_readc_cmd(H2W_LED_MAXADD);
		if (ph2w_info->LED_MAXADD == -1)
			goto err_plugin;
		if (ph2w_info->LED_MAXADD >= 1) {
			key = h2w_readc_cmd(H2W_LEDCT0);
			if (key == -1)
				goto err_plugin;
			ph2w_info->LEDCT0 = (key == 0x02) ? 1 : 0;
		}
	}

	/* read group 3, 4, 5 */
	if (ph2w_info->MAX_GP_ADD >= 3) {
		maxadd = h2w_readc_cmd(H2W_CRDL_MAXADD);
		if (maxadd == -1)
			goto err_plugin;
	}
	if (ph2w_info->MAX_GP_ADD >= 4) {
		maxadd = h2w_readc_cmd(H2W_CARKIT_MAXADD);
		if (maxadd == -1)
			goto err_plugin;
	}
	if (ph2w_info->MAX_GP_ADD >= 5) {
		maxadd = h2w_readc_cmd(H2W_USBHOST_MAXADD);
		if (maxadd == -1)
			goto err_plugin;
	}

	/* read medical group */
	if (ph2w_info->MAX_GP_ADD >= 6) {
		ph2w_info->MED_MAXADD = h2w_readc_cmd(H2W_MED_MAXADD);
		if (ph2w_info->MED_MAXADD == -1)
			goto err_plugin;
		if (ph2w_info->MED_MAXADD >= 1) {
			key = h2w_readc_cmd(H2W_MED_CONTROL);
			if (key == -1)
				goto err_plugin;
		ph2w_info->DATA_EN = (key & 0x01);
		ph2w_info->AP_EN = (key & 0x02) >> 1;
		ph2w_info->AP_ID = (key & 0x1c) >> 2;
		}
		if (ph2w_info->MED_MAXADD >= 2) {
			key = h2w_readc_cmd(H2W_MED_IN_DATA);
			if (key == -1)
				goto err_plugin;
		}
	}

	if (ph2w_info->AUDIO_DEVICE)
		ascr0 = H2W_ASCR_AUDIO_IN | H2W_ASCR_ACT_EN;
	else
		ascr0 = H2W_ASCR_ACT_EN;

	if (h2w_writec_cmd(H2W_ASCR0, ascr0) < 0)
		goto err_plugin;
	udelay(10);

	ret = 0;

	/* adjust speed */
	if (ph2w_info->MAX_GP_ADD == 2) {
		/* Remote control */
		hi->speed = H2W_250KHz;
	} else if (ph2w_info->MAX_GP_ADD == 6) {
		if (ph2w_info->MED_MAXADD >= 1) {
			key = h2w_readc_cmd(H2W_MED_CONTROL);
			if (key == -1)
				goto err_plugin;
			ph2w_info->DATA_EN   = (key & 0x01);
			ph2w_info->AP_EN = (key & 0x02) >> 1;
			ph2w_info->AP_ID = (key & 0x1c) >> 2;
		}
	}

err_plugin:
	h2w_end_command();

	return ret;
}

static inline void h2w_dev_power_on(int on)
{
	if (!hi->vreg_h2w)
		return;

	if (on)
		vreg_enable(hi->vreg_h2w);
	else
		vreg_disable(hi->vreg_h2w);
}

static int h2w_dev_detect(void)
{
	int ret = -1;
	int retry_times;

	for (retry_times = 5; retry_times; retry_times--) {
		/* Enable H2W Power */
		h2w_dev_power_on(1);
		msleep(100);
		memset(&hi->h2w_info, 0, sizeof(H2W_INFO));
		if (h2w_dev_init(&hi->h2w_info) < 0) {
			h2w_dev_power_on(0);
			msleep(100);
		} else if (hi->h2w_info.MAX_GP_ADD == 2) {
			ret = 0;
			break;
		} else {
			printk(KERN_INFO "h2w_detect: detect error(%d)\n"
				, hi->h2w_info.MAX_GP_ADD);
			h2w_dev_power_on(0);
			msleep(100);
		}
		printk(KERN_INFO "h2w_detect(%d)\n"
				, hi->h2w_info.MAX_GP_ADD);
	}
	H2W_DBG("h2w_detect:(%d)\n", retry_times);
	return ret;
}

static void remove_headset(void)
{
	unsigned long irq_flags;

	H2W_DBG("");

	mutex_lock(&hi->mutex_lock);
	switch_set_state(&hi->sdev, switch_get_state(&hi->sdev) &
			~(BIT_HEADSET | BIT_HEADSET_NO_MIC));
	mutex_unlock(&hi->mutex_lock);
	hi->init_cpld();

	/* Disable button */
	switch (hi->htc_headset_flag) {
	case H2W_HTC_HEADSET:
		local_irq_save(irq_flags);
		disable_irq(hi->irq_btn);
		local_irq_restore(irq_flags);

		if (atomic_read(&hi->btn_state))
			button_released();
		break;
	case H2W_DEVICE:
		h2w_dev_power_on(0);
		set_irq_type(hi->irq_btn, IRQF_TRIGGER_LOW);
		disable_irq(hi->irq_btn);
		/* 10ms (5-15 with 10ms tick) */
		hi->btn_debounce_time = ktime_set(0, 10000000);
		hi->set_clk_dir(0);
		hi->set_dat_dir(0);
		break;
	}

	hi->htc_headset_flag = 0;
	hi->debounce_time = ktime_set(0, 100000000);  /* 100 ms */

}

#ifdef CONFIG_MSM_SERIAL_DEBUGGER
extern void msm_serial_debug_enable(int);
#endif

static void insert_headset(int type)
{
	unsigned long irq_flags;
	int state;

	H2W_DBG("");

	hi->htc_headset_flag = type;
	state = BIT_HEADSET | BIT_HEADSET_NO_MIC;

	state = switch_get_state(&hi->sdev);
	state &= ~(BIT_HEADSET_NO_MIC | BIT_HEADSET);
	switch (type) {
	case H2W_HTC_HEADSET:
		printk(KERN_INFO "insert_headset H2W_HTC_HEADSET\n");
		state |= BIT_HEADSET;
		hi->ignore_btn = !gpio_get_value(hi->cable_in2);
		/* Enable button irq */
		local_irq_save(irq_flags);
		enable_irq(hi->irq_btn);
		local_irq_restore(irq_flags);
		hi->debounce_time = ktime_set(0, 200000000); /* 20 ms */
		break;
	case H2W_DEVICE:
		if (h2w_dev_detect() < 0) {
			printk(KERN_INFO "H2W_DEVICE -- Non detect\n");
			remove_headset();
		} else {
			printk(KERN_INFO "H2W_DEVICE -- detect\n");
			hi->btn_debounce_time = ktime_set(0, 0);
			local_irq_save(irq_flags);
			enable_irq(hi->irq_btn);
			set_irq_type(hi->irq_btn, IRQF_TRIGGER_RISING);
			local_irq_restore(irq_flags);
			state |= BIT_HEADSET;
		}
		break;
	case H2W_USB_CRADLE:
		state |= BIT_HEADSET_NO_MIC;
		break;
	case H2W_UART_DEBUG:
		hi->config_cpld(hi->debug_uart);
		printk(KERN_INFO "switch to H2W_UART_DEBUG\n");
	default:
		return;
	}
	mutex_lock(&hi->mutex_lock);
	switch_set_state(&hi->sdev, state);
	mutex_unlock(&hi->mutex_lock);

#ifdef CONFIG_MSM_SERIAL_DEBUGGER
	msm_serial_debug_enable(false);
#endif

}
#if 0
static void remove_headset(void)
{
	unsigned long irq_flags;

	H2W_DBG("");

	switch_set_state(&hi->sdev, H2W_NO_DEVICE);

	hi->init_cpld();

	/* Disable button */
	local_irq_save(irq_flags);
	disable_irq(hi->irq_btn);
	local_irq_restore(irq_flags);

	if (atomic_read(&hi->btn_state))
		button_released();

	hi->debounce_time = ktime_set(0, 100000000);  /* 100 ms */
}
#endif
static int is_accessary_pluged_in(void)
{
	int type = 0;
	int clk1 = 0, dat1 = 0, clk2 = 0, dat2 = 0, clk3 = 0, dat3 = 0;

	/* Step1: save H2W_CLK and H2W_DAT */
	/* Delay 10ms for pin stable. */
	msleep(10);
	clk1 = gpio_get_value(hi->h2w_clk);
	dat1 = gpio_get_value(hi->h2w_data);

	/*
	 * Step2: set GPIO_CABLE_IN1 as output high and GPIO_CABLE_IN2 as
	 * input
	 */
	gpio_direction_output(hi->cable_in1, 1);
	gpio_direction_input(hi->cable_in2);
	/* Delay 10ms for pin stable. */
	msleep(10);
	/* Step 3: save H2W_CLK and H2W_DAT */
	clk2 = gpio_get_value(hi->h2w_clk);
	dat2 = gpio_get_value(hi->h2w_data);

	/*
	 * Step 4: set GPIO_CABLE_IN1 as input and GPIO_CABLE_IN2 as output
	 * high
	 */
	gpio_direction_input(hi->cable_in1);
	gpio_direction_output(hi->cable_in2, 1);
	/* Delay 10ms for pin stable. */
	msleep(10);
	/* Step 5: save H2W_CLK and H2W_DAT */
	clk3 = gpio_get_value(hi->h2w_clk);
	dat3 = gpio_get_value(hi->h2w_data);

	/* Step 6: set both GPIO_CABLE_IN1 and GPIO_CABLE_IN2 as input */
	gpio_direction_input(hi->cable_in1);
	gpio_direction_input(hi->cable_in2);

	H2W_DBG("(%d,%d) (%d,%d) (%d,%d)\n",
		clk1, dat1, clk2, dat2, clk3, dat3);

	if ((clk1 == 0) && (dat1 == 1) &&
	    (clk2 == 0) && (dat2 == 1) &&
	    (clk3 == 0) && (dat3 == 1))
		type = H2W_HTC_HEADSET;
	else if ((clk1 == 0) && (dat1 == 0) &&
		 (clk2 == 0) && (dat2 == 0) &&
		 (clk3 == 0) &&  (dat3 == 0))
		type = NORMAL_HEARPHONE;
	else if ((clk1 == 0) && (dat1 == 0) &&
		 (clk2 == 1) && (dat2 == 0) &&
		 (clk3 == 0) && (dat3 == 1))
		type = H2W_DEVICE;
	else if ((clk1 == 0) && (dat1 == 0) &&
		 (clk2 == 1) && (dat2 == 1) &&
		 (clk3 == 1) && (dat3 == 1))
		type = H2W_USB_CRADLE;
	else if ((clk1 == 0) && (dat1 == 1) &&
		 (clk2 == 1) && (dat2 == 1) &&
		 (clk3 == 0) && (dat3 == 1))
		type = H2W_UART_DEBUG;
	else
		type = H2W_NO_DEVICE;

	return type;
}


static void detection_work(struct work_struct *work)
{
	unsigned long irq_flags;
	int type;

	H2W_DBG("");

	if (gpio_get_value(hi->cable_in1) != 0) {
		/* Headset not plugged in */
		if (switch_get_state(&hi->sdev) != H2W_NO_DEVICE)
			remove_headset();
		return;
	}

	/* Something plugged in, lets make sure its a headset */

	/* Switch CPLD to GPIO to do detection */
	hi->config_cpld(H2W_GPIO);

	/* Disable headset interrupt while detecting.*/
	local_irq_save(irq_flags);
	disable_irq(hi->irq);
	local_irq_restore(irq_flags);

	/* Something plugged in, lets make sure its a headset */
	type = is_accessary_pluged_in();

	/* Restore IRQs */
	local_irq_save(irq_flags);
	enable_irq(hi->irq);
	local_irq_restore(irq_flags);

	insert_headset(type);
}

static enum hrtimer_restart button_event_timer_func(struct hrtimer *data)
{
	int key, press, keyname, h2w_key = 1;

	H2W_DBG("");

	if (switch_get_state(&hi->sdev) == H2W_HTC_HEADSET) {
		switch (hi->htc_headset_flag) {
		case H2W_HTC_HEADSET:
			if (gpio_get_value(hi->cable_in2)) {
				if (hi->ignore_btn)
					hi->ignore_btn = 0;
				else if (atomic_read(&hi->btn_state))
					button_released();
			} else {
				if (!hi->ignore_btn &&
				    !atomic_read(&hi->btn_state))
					button_pressed();
			}
			break;
		case H2W_DEVICE:
			if ((hi->get_dat() == 1) && (hi->get_clk() == 1)) {
				/* Don't do anything because H2W pull out. */
				H2WE("Remote Control pull out.\n");
			} else {
				key = h2w_get_fnkey();
				press = (key > 0x7F) ? 0 : 1;
				keyname = key & 0x7F;
				 /* H2WI("key = %d, press = %d,
					 keyname = %d \n",
					 key, press, keyname); */
				switch (keyname) {
				case H2W_KEY_PLAY:
					H2WI("H2W_KEY_PLAY");
					key = KEY_PLAYPAUSE;
					break;
				case H2W_KEY_FORWARD:
					H2WI("H2W_KEY_FORWARD");
					key = KEY_NEXTSONG;
					break;
				case H2W_KEY_BACKWARD:
					H2WI("H2W_KEY_BACKWARD");
					key = KEY_PREVIOUSSONG;
					break;
				case H2W_KEY_VOLUP:
					H2WI("H2W_KEY_VOLUP");
					key = KEY_VOLUMEUP;
					break;
				case H2W_KEY_VOLDOWN:
					H2WI("H2W_KEY_VOLDOWN");
					key = KEY_VOLUMEDOWN;
					break;
				case H2W_KEY_PICKUP:
					H2WI("H2W_KEY_PICKUP");
					key = KEY_SEND;
					break;
				case H2W_KEY_HANGUP:
					H2WI("H2W_KEY_HANGUP");
					key = KEY_END;
					break;
				case H2W_KEY_MUTE:
					H2WI("H2W_KEY_MUTE");
					key = KEY_MUTE;
					break;
				case H2W_KEY_HOLD:
					H2WI("H2W_KEY_HOLD");
					break;
				default:
				H2WI("default");
					h2w_key = 0;
				}
				if (h2w_key) {
					if (press)
						H2WI("Press\n");
					else
						H2WI("Release\n");
					input_report_key(hi->input, key, press);
				}
			}
			break;
		} /* end switch */
	}

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart detect_event_timer_func(struct hrtimer *data)
{
	H2W_DBG("");

	queue_work(g_detection_work_queue, &g_detection_work);
	return HRTIMER_NORESTART;
}

static irqreturn_t detect_irq_handler(int irq, void *dev_id)
{
	int value1, value2;
	int retry_limit = 10;

	H2W_DBG("");
	set_irq_type(hi->irq_btn, IRQF_TRIGGER_LOW);
	do {
		value1 = gpio_get_value(hi->cable_in1);
		set_irq_type(hi->irq, value1 ?
				IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH);
		value2 = gpio_get_value(hi->cable_in1);
	} while (value1 != value2 && retry_limit-- > 0);

	H2W_DBG("value2 = %d (%d retries), device=%d",
		value2, (10-retry_limit), switch_get_state(&hi->sdev));

	if ((switch_get_state(&hi->sdev) == H2W_NO_DEVICE) ^ value2) {
		if (switch_get_state(&hi->sdev) == H2W_HTC_HEADSET)
			hi->ignore_btn = 1;
		/* Do the rest of the work in timer context */
		hrtimer_start(&hi->timer, hi->debounce_time, HRTIMER_MODE_REL);
	}

	return IRQ_HANDLED;
}

static irqreturn_t button_irq_handler(int irq, void *dev_id)
{
	int value1, value2;
	int retry_limit = 10;

	H2W_DBG("");
	do {
		value1 = gpio_get_value(hi->cable_in2);
		if (hi->htc_headset_flag != H2W_DEVICE)
		set_irq_type(hi->irq_btn, value1 ?
				IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH);
		value2 = gpio_get_value(hi->cable_in2);
	} while (value1 != value2 && retry_limit-- > 0);

	H2W_DBG("value2 = %d (%d retries)", value2, (10-retry_limit));

	hrtimer_start(&hi->btn_timer, hi->btn_debounce_time, HRTIMER_MODE_REL);

	return IRQ_HANDLED;
}

#if defined(CONFIG_DEBUG_FS)
static int h2w_debug_set(void *data, u64 val)
{
	mutex_lock(&hi->mutex_lock);
	switch_set_state(&hi->sdev, (int)val);
	mutex_unlock(&hi->mutex_lock);
	return 0;
}

static int h2w_debug_get(void *data, u64 *val)
{
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(h2w_debug_fops, h2w_debug_get, h2w_debug_set, "%llu\n");
static int __init h2w_debug_init(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("h2w", 0);
	if (IS_ERR(dent))
		return PTR_ERR(dent);

	debugfs_create_file("state", 0644, dent, NULL, &h2w_debug_fops);

	return 0;
}

device_initcall(h2w_debug_init);
#endif

static int h2w_probe(struct platform_device *pdev)
{
	int ret;
	struct h2w_platform_data *pdata = pdev->dev.platform_data;

	printk(KERN_INFO "H2W: Registering H2W (headset) driver\n");
	hi = kzalloc(sizeof(struct h2w_info), GFP_KERNEL);
	if (!hi)
		return -ENOMEM;

	atomic_set(&hi->btn_state, 0);
	hi->ignore_btn = 0;

	hi->debounce_time = ktime_set(0, 100000000);  /* 100 ms */
	hi->btn_debounce_time = ktime_set(0, 10000000); /* 10 ms */

	hi->htc_headset_flag = 0;
	hi->cable_in1 = pdata->cable_in1;
	hi->cable_in2 = pdata->cable_in2;
	hi->h2w_clk = pdata->h2w_clk;
	hi->h2w_data = pdata->h2w_data;
	hi->debug_uart = pdata->debug_uart;
	hi->config_cpld = pdata->config_cpld;
	hi->init_cpld = pdata->init_cpld;
	hi->set_dat = pdata->set_dat;
	hi->set_clk = pdata->set_clk;
	hi->set_dat_dir	= pdata->set_dat_dir;
	hi->set_clk_dir	= pdata->set_clk_dir;
	hi->get_dat = pdata->get_dat;
	hi->get_clk = pdata->get_clk;
	hi->speed = H2W_50KHz;
	/* obtain needed VREGs */
	if (pdata->power_name)
		hi->vreg_h2w = vreg_get(0, pdata->power_name);

	mutex_init(&hi->mutex_lock);

	hi->sdev.name = "h2w";
	hi->sdev.print_name = h2w_print_name;

	ret = switch_dev_register(&hi->sdev);
	if (ret < 0)
		goto err_switch_dev_register;

	g_detection_work_queue = create_workqueue("detection");
	if (g_detection_work_queue == NULL) {
		ret = -ENOMEM;
		goto err_create_work_queue;
	}

	ret = gpio_request(hi->cable_in1, "h2w_detect");
	if (ret < 0)
		goto err_request_detect_gpio;

	ret = gpio_request(hi->cable_in2, "h2w_button");
	if (ret < 0)
		goto err_request_button_gpio;

	ret = gpio_direction_input(hi->cable_in1);
	if (ret < 0)
		goto err_set_detect_gpio;

	ret = gpio_direction_input(hi->cable_in2);
	if (ret < 0)
		goto err_set_button_gpio;

	hi->irq = gpio_to_irq(hi->cable_in1);
	if (hi->irq < 0) {
		ret = hi->irq;
		goto err_get_h2w_detect_irq_num_failed;
	}

	hi->irq_btn = gpio_to_irq(hi->cable_in2);
	if (hi->irq_btn < 0) {
		ret = hi->irq_btn;
		goto err_get_button_irq_num_failed;
	}

	/* Set CPLD MUX to H2W <-> CPLD GPIO */
	hi->init_cpld();

	hrtimer_init(&hi->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hi->timer.function = detect_event_timer_func;
	hrtimer_init(&hi->btn_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hi->btn_timer.function = button_event_timer_func;

	ret = request_irq(hi->irq, detect_irq_handler,
			  IRQF_TRIGGER_LOW, "h2w_detect", NULL);
	if (ret < 0)
		goto err_request_detect_irq;

	/* Disable button until plugged in */
	set_irq_flags(hi->irq_btn, IRQF_VALID | IRQF_NOAUTOEN);
	ret = request_irq(hi->irq_btn, button_irq_handler,
			  IRQF_TRIGGER_LOW, "h2w_button", NULL);
	if (ret < 0)
		goto err_request_h2w_headset_button_irq;

	ret = set_irq_wake(hi->irq, 1);
	if (ret < 0)
		goto err_request_input_dev;

	ret = set_irq_wake(hi->irq_btn, 1);
	if (ret < 0)
		goto err_request_input_dev;



	hi->input = input_allocate_device();
	if (!hi->input) {
		ret = -ENOMEM;
		goto err_request_input_dev;
	}

	hi->input->name = "h2w headset";
	set_bit(EV_SYN, hi->input->evbit);
	set_bit(EV_KEY, hi->input->evbit);
	set_bit(KEY_MEDIA, hi->input->keybit);
	set_bit(KEY_NEXTSONG, hi->input->keybit);
	set_bit(KEY_PLAYPAUSE, hi->input->keybit);
	set_bit(KEY_PREVIOUSSONG, hi->input->keybit);
	set_bit(KEY_MUTE, hi->input->keybit);
	set_bit(KEY_VOLUMEUP, hi->input->keybit);
	set_bit(KEY_VOLUMEDOWN, hi->input->keybit);
	set_bit(KEY_END, hi->input->keybit);
	set_bit(KEY_SEND, hi->input->keybit);

	ret = input_register_device(hi->input);
	if (ret < 0)
		goto err_register_input_dev;

	return 0;

err_register_input_dev:
	input_free_device(hi->input);
err_request_input_dev:
	free_irq(hi->irq_btn, 0);
err_request_h2w_headset_button_irq:
	free_irq(hi->irq, 0);
err_request_detect_irq:
err_get_button_irq_num_failed:
err_get_h2w_detect_irq_num_failed:
err_set_button_gpio:
err_set_detect_gpio:
	gpio_free(hi->cable_in2);
err_request_button_gpio:
	gpio_free(hi->cable_in1);
err_request_detect_gpio:
	destroy_workqueue(g_detection_work_queue);
err_create_work_queue:
	switch_dev_unregister(&hi->sdev);
err_switch_dev_register:
	printk(KERN_ERR "H2W: Failed to register driver\n");

	return ret;
}

static int h2w_remove(struct platform_device *pdev)
{
	H2W_DBG("");
	if (switch_get_state(&hi->sdev))
		remove_headset();
	input_unregister_device(hi->input);
	gpio_free(hi->cable_in2);
	gpio_free(hi->cable_in1);
	free_irq(hi->irq_btn, 0);
	free_irq(hi->irq, 0);
	destroy_workqueue(g_detection_work_queue);
	switch_dev_unregister(&hi->sdev);

	return 0;
}


static struct platform_driver h2w_driver = {
	.probe		= h2w_probe,
	.remove		= h2w_remove,
	.driver		= {
		.name		= "h2w",
		.owner		= THIS_MODULE,
	},
};

static int __init h2w_init(void)
{
	H2W_DBG("");
	return platform_driver_register(&h2w_driver);
}

static void __exit h2w_exit(void)
{
	platform_driver_unregister(&h2w_driver);
}

module_init(h2w_init);
module_exit(h2w_exit);

MODULE_AUTHOR("Laurence Chen <Laurence_Chen@htc.com>");
MODULE_DESCRIPTION("HTC 2 Wire detection driver");
MODULE_LICENSE("GPL");
