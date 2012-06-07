/* arch/arm/mach-msm/htc_35mm_jack.c
 *
 * Copyright (C) 2009 HTC, Inc.
 * Author: Arec Kao <Arec_Kao@htc.com>
 * Copyright (C) 2009 Google, Inc.
 * Author: Eric Olsen <eolsen@android.com>
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

#include <linux/module.h>
#include <linux/sysdev.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/debugfs.h>
#include <linux/jiffies.h>
#include <linux/switch.h>
#include <linux/input.h>
#include <linux/wakelock.h>
#include <asm/gpio.h>
#include <asm/atomic.h>
#include <mach/board.h>
#include <mach/vreg.h>
#include <asm/mach-types.h>
#include <mach/htc_acoustic_qsd.h>
#include <mach/htc_35mm_jack.h>
#include <mach/htc_headset.h>

#ifdef CONFIG_HTC_AUDIOJACK
#include <mach/audio_jack.h>
#endif

/* #define CONFIG_DEBUG_H2W */

#define H2WI(fmt, arg...) \
	printk(KERN_INFO "[H2W] %s " fmt "\r\n", __func__, ## arg)
#define H2WE(fmt, arg...) \
	printk(KERN_ERR "[H2W] %s " fmt "\r\n", __func__, ## arg)

#ifdef CONFIG_DEBUG_H2W
#define H2W_DBG(fmt, arg...) \
	printk(KERN_INFO "[H2W] %s " fmt "\r\n", __func__, ## arg)
#else
#define H2W_DBG(fmt, arg...) do {} while (0)
#endif

void detect_h2w_do_work(struct work_struct *w);

static struct workqueue_struct *detect_wq;
static struct workqueue_struct *button_wq;

static DECLARE_DELAYED_WORK(detect_h2w_work, detect_h2w_do_work);

static void insert_35mm_do_work(struct work_struct *work);
static DECLARE_WORK(insert_35mm_work, insert_35mm_do_work);
static void remove_35mm_do_work(struct work_struct *work);
static DECLARE_WORK(remove_35mm_work, remove_35mm_do_work);
static void button_35mm_do_work(struct work_struct *work);
static DECLARE_WORK(button_35mm_work, button_35mm_do_work);

struct h35_info {
	struct mutex mutex_lock;
	struct switch_dev hs_change;
	unsigned long insert_jiffies;
	int ext_35mm_status;
	int is_ext_insert;
	int key_code;
	int mic_bias_state;
	int *is_hpin_stable;
	struct input_dev *input;

	struct wake_lock headset_wake_lock;
};

static struct h35mm_platform_data *pd;
static struct h35_info *hi;

static ssize_t h35mm_print_name(struct switch_dev *sdev, char *buf)
{
	return sprintf(buf, "Headset\n");
}

static void button_35mm_do_work(struct work_struct *work)
{
	int key = 0;
	int pressed = 0;

	if (!hi->is_ext_insert) {
		/* no headset ignor key event */
		H2WI("3.5mm headset is plugged out, skip report key event");
		return;
	}

	switch (hi->key_code) {
	case 0x1: /* Play/Pause */
		H2WI("3.5mm RC: Play Pressed");
		key = KEY_MEDIA;
		pressed = 1;
		break;
	case 0x2:
		H2WI("3.5mm RC: BACKWARD Pressed");
		key = KEY_PREVIOUSSONG;
		pressed = 1;
		break;
	case 0x3:
		H2WI("3.5mm RC: FORWARD Pressed");
		key = KEY_NEXTSONG;
		pressed = 1;
		break;
	case 0x81: /* Play/Pause */
		H2WI("3.5mm RC: Play Released");
		key = KEY_MEDIA;
		pressed = 0;
		break;
	case 0x82:
		H2WI("3.5mm RC: BACKWARD Released");
		key = KEY_PREVIOUSSONG;
		pressed = 0;
		break;
	case 0x83:
		H2WI("3.5mm RC: FORWARD Released");
		key = KEY_NEXTSONG;
		pressed = 0;
		break;
	default:
		H2WI("3.5mm RC: Unknown Button (0x%x) Pressed", hi->key_code);
		return;
	}
	input_report_key(hi->input, key, pressed);
	input_sync(hi->input);

	wake_lock_timeout(&hi->headset_wake_lock, 1.5*HZ);
}

static void remove_35mm_do_work(struct work_struct *work)
{
	wake_lock_timeout(&hi->headset_wake_lock, 2.5*HZ);

	H2W_DBG("");
	/*To solve the insert, remove, insert headset problem*/
	if (time_before_eq(jiffies, hi->insert_jiffies))
		msleep(800);

	if (hi->is_ext_insert) {
		H2WI("Skip 3.5mm headset plug out!!!");
		if (hi->is_hpin_stable)
			*(hi->is_hpin_stable) = 1;
		return;
	}

	pr_info("3.5mm_headset plug out\n");

	if (pd->key_event_disable != NULL)
		pd->key_event_disable();

	if (hi->mic_bias_state) {
		turn_mic_bias_on(0);
		hi->mic_bias_state = 0;
	}
	hi->ext_35mm_status = 0;
	if (hi->is_hpin_stable)
		*(hi->is_hpin_stable) = 0;

	/* Notify framework via switch class */
	mutex_lock(&hi->mutex_lock);
	switch_set_state(&hi->hs_change, hi->ext_35mm_status);
	mutex_unlock(&hi->mutex_lock);
}

static void insert_35mm_do_work(struct work_struct *work)
{
	H2W_DBG("");
	hi->insert_jiffies = jiffies + 1*HZ;

	wake_lock_timeout(&hi->headset_wake_lock, 1.5*HZ);

	if (hi->is_ext_insert) {
		pr_info("3.5mm_headset plug in\n");

	if (pd->key_event_enable != NULL)
		pd->key_event_enable();

		/* Turn On Mic Bias */
		if (!hi->mic_bias_state) {
			turn_mic_bias_on(1);
			hi->mic_bias_state = 1;
			/* Wait for pin stable */
			msleep(300);
		}

		/* Detect headset with or without microphone */
		if(pd->headset_has_mic) {
			if (pd->headset_has_mic() == 0) {
				/* without microphone */
				pr_info("3.5mm without microphone\n");
				hi->ext_35mm_status = BIT_HEADSET_NO_MIC;
			} else { /* with microphone */
				pr_info("3.5mm with microphone\n");
				hi->ext_35mm_status = BIT_HEADSET;
			}
		} else {
			/* Assume no mic */
			pr_info("3.5mm without microphone\n");
			hi->ext_35mm_status = BIT_HEADSET_NO_MIC;
		}
		hi->ext_35mm_status |= BIT_35MM_HEADSET;

		/* Notify framework via switch class */
		mutex_lock(&hi->mutex_lock);
		switch_set_state(&hi->hs_change, hi->ext_35mm_status);
		mutex_unlock(&hi->mutex_lock);

		if (hi->is_hpin_stable)
			*(hi->is_hpin_stable) = 1;
	}
}

int htc_35mm_key_event(int keycode, int *hpin_stable)
{
	hi->key_code = keycode;
	hi->is_hpin_stable = hpin_stable;

	if ((hi->ext_35mm_status & BIT_HEADSET) == 0) {
		*(hi->is_hpin_stable) = 0;

		pr_info("Key press with no mic.  Retrying detection\n");
		queue_work(detect_wq, &insert_35mm_work);
	} else
		queue_work(button_wq, &button_35mm_work);

	return 0;
}

int htc_35mm_jack_plug_event(int insert, int *hpin_stable)
{
	if (!hi) {
		pr_err("Plug event before driver init\n");
		return -1;
	}

	mutex_lock(&hi->mutex_lock);
	hi->is_ext_insert = insert;
	hi->is_hpin_stable = hpin_stable;
	mutex_unlock(&hi->mutex_lock);

	H2WI(" %d", hi->is_ext_insert);
	if (!hi->is_ext_insert)
		queue_work(detect_wq, &remove_35mm_work);
	else
		queue_work(detect_wq, &insert_35mm_work);
	return 1;
}

static int htc_35mm_probe(struct platform_device *pdev)
{
	int ret;

	pd = pdev->dev.platform_data;

	pr_info("H2W: htc_35mm_jack driver register\n");

	hi = kzalloc(sizeof(struct h35_info), GFP_KERNEL);
	if (!hi)
		return -ENOMEM;

	hi->ext_35mm_status = 0;
	hi->is_ext_insert = 0;
	hi->mic_bias_state = 0;

	mutex_init(&hi->mutex_lock);

	wake_lock_init(&hi->headset_wake_lock, WAKE_LOCK_SUSPEND, "headset");

	hi->hs_change.name = "h2w";
	hi->hs_change.print_name = h35mm_print_name;
	ret = switch_dev_register(&hi->hs_change);
	if (ret < 0)
		goto err_switch_dev_register;

	detect_wq = create_workqueue("detection");
	if (detect_wq  == NULL) {
		ret = -ENOMEM;
		goto err_create_detect_work_queue;
	}

	button_wq = create_workqueue("button");
	if (button_wq  == NULL) {
			ret = -ENOMEM;
			goto err_create_button_work_queue;
	}

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

	/* Enable plug events*/
	if (pd->plug_event_enable == NULL) {
		ret = -ENOMEM;
		goto err_enable_plug_event;
	}
	if (pd->plug_event_enable() != 1)  {
		ret = -ENOMEM;
		goto err_enable_plug_event;
	}

	return 0;

err_enable_plug_event:
err_register_input_dev:
	input_free_device(hi->input);
err_request_input_dev:
	destroy_workqueue(button_wq);
err_create_button_work_queue:
	destroy_workqueue(detect_wq);
err_create_detect_work_queue:
	switch_dev_unregister(&hi->hs_change);
err_switch_dev_register:
	kzfree(hi);
	pr_err("H2W: Failed to register driver\n");

	return ret;
}

static int htc_35mm_remove(struct platform_device *pdev)
{
	H2W_DBG("");
	switch_dev_unregister(&hi->hs_change);
	kzfree(hi);

#if 0 /* Add keys later */
	input_unregister_device(hi->input);
#endif
	return 0;
}

static struct platform_driver htc_35mm_driver = {
	.probe		= htc_35mm_probe,
	.remove		= htc_35mm_remove,
	.driver		= {
		.name		= "htc_headset",
		.owner		= THIS_MODULE,
	},
};

static int __init htc_35mm_init(void)
{
	H2W_DBG("");
	return platform_driver_register(&htc_35mm_driver);
}

static void __exit htc_35mm_exit(void)
{
	platform_driver_unregister(&htc_35mm_driver);
}

module_init(htc_35mm_init);
module_exit(htc_35mm_exit);

MODULE_AUTHOR("Eric Olsen <eolsen@android.com>");
MODULE_DESCRIPTION("HTC 3.5MM Driver");
MODULE_LICENSE("GPL");
