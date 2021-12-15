/*
 * tfa98xx.c   tfa98xx codec module
 *
 *
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2020 GOODIX
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifdef __KERNEL__
	#ifdef pr_fmt
	#undef pr_fmt
	#endif
	#define pr_fmt(fmt) "[tfa98xx] %s(): " fmt, __func__
#endif

#include <linux/module.h>
#include <linux/i2c.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>
#include <linux/version.h>
#include <linux/input.h>
#include "config.h"
#include "tfa98xx.h"
#include "tfa.h"
#include "tfa_dsp_fw.h"

 /* required for enum tfa9912_irq */
#include "tfa98xx_tfafieldnames.h"

#define TFA98XX_VERSION	TFA98XX_API_REV_STR

#define I2C_RETRIES 50
#define I2C_RETRY_DELAY 5 /* ms */

/* Change volume selection behavior:
 * Uncomment following line to generate a profile change when updating
 * a volume control (also changes to the profile of the modified  volume
 * control)
 */
 /*#define TFA98XX_ALSA_CTRL_PROF_CHG_ON_VOL	1
 */

 /* Supported rates and data formats */
#define TFA98XX_RATES SNDRV_PCM_RATE_8000_96000
#define TFA98XX_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

#define TF98XX_MAX_DSP_START_TRY_COUNT	10

/* data accessible by all instances */
static struct kmem_cache *tfa98xx_cache = NULL;  /* Memory pool used for DSP messages */
/* Mutex protected data */
static DEFINE_MUTEX(tfa98xx_mutex);
static LIST_HEAD(tfa98xx_device_list);
static int tfa98xx_device_count = 0;
static int tfa98xx_sync_count = 0;
static LIST_HEAD(profile_list);        /* list of user selectable profiles */
static int tfa98xx_mixer_profiles = 0; /* number of user selectable profiles */
static int tfa98xx_mixer_profile = 0;  /* current mixer profile */
static struct snd_kcontrol_new *tfa98xx_controls;
static TfaContainer_t *tfa98xx_container = NULL;

static int tfa98xx_kmsg_regs = 0;
static int tfa98xx_ftrace_regs = 0;

static char *fw_name = "tfa98xx.cnt";
module_param(fw_name, charp, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(fw_name, "TFA98xx DSP firmware (container file) name.");

static int trace_level = 0;
module_param(trace_level, int, S_IRUGO);
MODULE_PARM_DESC(trace_level, "TFA98xx debug trace level (0=off, bits:1=verbose,2=regdmesg,3=regftrace,4=timing).");

static char *dflt_prof_name = "";
module_param(dflt_prof_name, charp, S_IRUGO);

static int no_start = 0;
module_param(no_start, int, S_IRUGO);
MODULE_PARM_DESC(no_start, "do not start the work queue; for debugging via user\n");

static int no_reset = 0;
module_param(no_reset, int, S_IRUGO);
MODULE_PARM_DESC(no_reset, "do not use the reset line; for debugging via user\n");

/* we will be using dynamic TDM settings for all Xiaomi project */
static int pcm_sample_format = 0; /*Be carefull:  setting pcm_sample_format to 3 means TDM settings will be dynamically adapted, please do not set the
HW TDM Setting in the container file in case of dynamic sample format seletcion*/
module_param(pcm_sample_format, int, S_IRUGO);
MODULE_PARM_DESC(pcm_sample_format, "PCM sample format: 0=S16_LE, 1=S24_LE, 2=S32_LE, 3=dynamic\n");

static int pcm_no_constraint = 0;
module_param(pcm_no_constraint, int, S_IRUGO);
MODULE_PARM_DESC(pcm_no_constraint, "do not use constraints for PCM parameters\n");

static void tfa98xx_tapdet_check_update(struct tfa98xx *tfa98xx);
static int tfa98xx_get_fssel(unsigned int rate);
static void tfa98xx_interrupt_enable(struct tfa98xx *tfa98xx, bool enable);

static int get_profile_from_list(char *buf, int id);
static int get_profile_id_for_sr(int id, unsigned int rate);

struct tfa98xx_rate {
	unsigned int rate;
	unsigned int fssel;
};

static const struct tfa98xx_rate rate_to_fssel[] = {
	{ 8000, 0 },
	{ 11025, 1 },
	{ 12000, 2 },
	{ 16000, 3 },
	{ 22050, 4 },
	{ 24000, 5 },
	{ 32000, 6 },
	{ 44100, 7 },
	{ 48000, 8 },
	{ 96000, 9 },
};

static inline char *tfa_cont_profile_name(struct tfa98xx *tfa98xx, int prof_idx)
{
	if (tfa98xx->tfa->cnt == NULL)
		return NULL;
	return tfaContProfileName(tfa98xx->tfa->cnt, tfa98xx->tfa->dev_idx, prof_idx);
}

static enum tfa_error tfa98xx_write_re25(struct tfa_device *tfa, int value)
{
	enum tfa_error err;

	/* clear MTPEX */
	err = tfa_dev_mtp_set(tfa, TFA_MTP_EX, 0);
	if (err == tfa_error_ok) {
		/* set RE25 in shadow regiser */
		err = tfa_dev_mtp_set(tfa, TFA_MTP_RE25_PRIM, value);
	}
	if (err == tfa_error_ok) {
		/* set MTPEX to copy RE25 into MTP  */
		err = tfa_dev_mtp_set(tfa, TFA_MTP_EX, 2);
	}

	return err;
}

/* Wrapper for tfa start */
static enum tfa_error tfa98xx_tfa_start(struct tfa98xx *tfa98xx, int next_profile, int vstep)
{
	enum tfa_error err;
	ktime_t start_time, stop_time;
	u64 delta_time;

	pr_debug("%s  next_profile=%d  vstep=%d\n", __func__, next_profile, vstep);
	if (trace_level & 8) {
		start_time = ktime_get_boottime();
	}

	err = tfa_dev_start(tfa98xx->tfa, next_profile, vstep);
	pr_debug("%s  after performed tfa_dev_start return (%d)\n", __func__, err);

	if (trace_level & 8) {
		stop_time = ktime_get_boottime();
		delta_time = ktime_to_ns(ktime_sub(stop_time, start_time));
		do_div(delta_time, 1000);
		dev_dbg(&tfa98xx->i2c->dev, "tfa_dev_start(%d,%d) time = %lld us\n",
			next_profile, vstep, delta_time);
	}

	if ((err == tfa_error_ok) && (tfa98xx->set_mtp_cal)) {
		enum tfa_error err_cal;
		err_cal = tfa98xx_write_re25(tfa98xx->tfa, tfa98xx->cal_data);
		if (err_cal != tfa_error_ok) {
			pr_err("Error, setting calibration value in mtp, err=%d\n", err_cal);
		}
		else {
			tfa98xx->set_mtp_cal = false;
			pr_info("Calibration value (%d) set in mtp\n",
				tfa98xx->cal_data);
		}
	}

	/* Check and update tap-detection state (in case of profile change) */
	tfa98xx_tapdet_check_update(tfa98xx);

	/* Remove sticky bit by reading it once */
	tfa_get_noclk(tfa98xx->tfa);

	/* A cold start erases the configuration, including interrupts setting.
	 * Restore it if required
	 */
	tfa98xx_interrupt_enable(tfa98xx, true);

	return err;
}

static int tfa98xx_input_open(struct input_dev *dev)
{
	struct tfa98xx *tfa98xx = input_get_drvdata(dev);
	dev_dbg(tfa98xx->codec->dev, "opening device file\n");

	/* note: open function is called only once by the framework.
	 * No need to count number of open file instances.
	 */
	if (tfa98xx->dsp_fw_state != TFA98XX_DSP_FW_OK) {
		dev_dbg(&tfa98xx->i2c->dev,
			"DSP not loaded, cannot start tap-detection\n");
		return -EIO;
	}

	/* enable tap-detection service */
	tfa98xx->tapdet_open = true;
	tfa98xx_tapdet_check_update(tfa98xx);

	return 0;
}

static void tfa98xx_input_close(struct input_dev *dev)
{
	struct tfa98xx *tfa98xx = input_get_drvdata(dev);

	dev_dbg(tfa98xx->codec->dev, "closing device file\n");

	/* Note: close function is called if the device is unregistered */

	/* disable tap-detection service */
	tfa98xx->tapdet_open = false;
	tfa98xx_tapdet_check_update(tfa98xx);
}

static int tfa98xx_register_inputdev(struct tfa98xx *tfa98xx)
{
	int err;
	struct input_dev *input;
	input = input_allocate_device();

	if (!input) {
		dev_err(tfa98xx->codec->dev, "Unable to allocate input device\n");
		return -ENOMEM;
	}

	input->evbit[0] = BIT_MASK(EV_KEY);
	input->keybit[BIT_WORD(BTN_0)] |= BIT_MASK(BTN_0);
	input->keybit[BIT_WORD(BTN_1)] |= BIT_MASK(BTN_1);
	input->keybit[BIT_WORD(BTN_2)] |= BIT_MASK(BTN_2);
	input->keybit[BIT_WORD(BTN_3)] |= BIT_MASK(BTN_3);
	input->keybit[BIT_WORD(BTN_4)] |= BIT_MASK(BTN_4);
	input->keybit[BIT_WORD(BTN_5)] |= BIT_MASK(BTN_5);
	input->keybit[BIT_WORD(BTN_6)] |= BIT_MASK(BTN_6);
	input->keybit[BIT_WORD(BTN_7)] |= BIT_MASK(BTN_7);
	input->keybit[BIT_WORD(BTN_8)] |= BIT_MASK(BTN_8);
	input->keybit[BIT_WORD(BTN_9)] |= BIT_MASK(BTN_9);

	input->open = tfa98xx_input_open;
	input->close = tfa98xx_input_close;

	input->name = "tfa98xx-tapdetect";

	input->id.bustype = BUS_I2C;
	input_set_drvdata(input, tfa98xx);

	err = input_register_device(input);
	if (err) {
		dev_err(tfa98xx->codec->dev, "Unable to register input device\n");
		goto err_free_dev;
	}

	dev_dbg(tfa98xx->codec->dev, "Input device for tap-detection registered: %s\n",
		input->name);
	tfa98xx->input = input;
	return 0;

err_free_dev:
	input_free_device(input);
	return err;
}

/*
 * Check if an input device for tap-detection can and shall be registered.
 * Register it if appropriate.
 * If already registered, check if still relevant and remove it if necessary.
 * unregister: true to request inputdev unregistration.
 */
static void __tfa98xx_inputdev_check_register(struct tfa98xx *tfa98xx, bool unregister)
{
	bool tap_profile = false;
	unsigned int i;
	for (i = 0; i < tfa_cnt_get_dev_nprof(tfa98xx->tfa); i++) {
		if (strstr(tfa_cont_profile_name(tfa98xx, i), ".tap")) {
			tap_profile = true;
			tfa98xx->tapdet_profiles |= 1 << i;
			dev_info(tfa98xx->codec->dev,
				"found a tap-detection profile (%d - %s)\n",
				i, tfa_cont_profile_name(tfa98xx, i));
		}
	}

	/* Check for device support:
	 *  - at device level
	 *  - at container (profile) level
	 */
	if (!(tfa98xx->flags & TFA98XX_FLAG_TAPDET_AVAILABLE) ||
		!tap_profile ||
		unregister) {
		/* No input device supported or required */
		if (tfa98xx->input) {
			input_unregister_device(tfa98xx->input);
			tfa98xx->input = NULL;
		}
		return;
	}

	/* input device required */
	if (tfa98xx->input)
		dev_info(tfa98xx->codec->dev, "Input device already registered, skipping\n");
	else
		tfa98xx_register_inputdev(tfa98xx);
}

static void tfa98xx_inputdev_check_register(struct tfa98xx *tfa98xx)
{
	__tfa98xx_inputdev_check_register(tfa98xx, false);
}

static void tfa98xx_inputdev_unregister(struct tfa98xx *tfa98xx)
{
	__tfa98xx_inputdev_check_register(tfa98xx, true);
}

#ifdef TFA_NON_DSP_SOLUTION
extern int send_tfa_cal_apr(void *buf, int cmd_size, bool bRead);
#else
int send_tfa_cal_apr(void *buf, int cmd_size, bool bRead)
{
	pr_err("this is empty function in tfa98xx.c\n");
	return 0;
}
#endif

#ifdef CONFIG_DEBUG_FS
/* OTC reporting
 * Returns the MTP0 OTC bit value
 */
static int tfa98xx_dbgfs_otc_get(void *data, u64 *val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	int value;

	mutex_lock(&tfa98xx->dsp_lock);
	value = tfa_dev_mtp_get(tfa98xx->tfa, TFA_MTP_OTC);
	mutex_unlock(&tfa98xx->dsp_lock);

	if (value < 0) {
		pr_err("[0x%x] Unable to check DSP access: %d\n", tfa98xx->i2c->addr, value);
		return -EIO;
	}

	*val = value;
	pr_debug("[0x%x] OTC : %d\n", tfa98xx->i2c->addr, value);

	return 0;
}

static int tfa98xx_dbgfs_otc_set(void *data, u64 val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	enum tfa_error err;

	if (val != 0 && val != 1) {
		pr_err("[0x%x] Unexpected value %llu\n", tfa98xx->i2c->addr, val);
		return -EINVAL;
	}

	mutex_lock(&tfa98xx->dsp_lock);
	err = tfa_dev_mtp_set(tfa98xx->tfa, TFA_MTP_OTC, val);
	mutex_unlock(&tfa98xx->dsp_lock);

	if (err != tfa_error_ok) {
		pr_err("[0x%x] Unable to check DSP access: %d\n", tfa98xx->i2c->addr, err);
		return -EIO;
	}

	pr_debug("[0x%x] OTC < %llu\n", tfa98xx->i2c->addr, val);

	return 0;
}

static int tfa98xx_dbgfs_mtpex_get(void *data, u64 *val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	int value;

	mutex_lock(&tfa98xx->dsp_lock);
	value = tfa_dev_mtp_get(tfa98xx->tfa, TFA_MTP_EX);
	mutex_unlock(&tfa98xx->dsp_lock);

	if (value < 0) {
		pr_err("[0x%x] Unable to check DSP access: %d\n", tfa98xx->i2c->addr, value);
		return -EIO;
	}


	*val = value;
	pr_debug("[0x%x] MTPEX : %d\n", tfa98xx->i2c->addr, value);

	return 0;
}

static int tfa98xx_dbgfs_mtpex_set(void *data, u64 val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	enum tfa_error err;

	if (val != 0) {
		pr_err("[0x%x] Can only clear MTPEX (0 value expected)\n", tfa98xx->i2c->addr);
		return -EINVAL;
	}

	mutex_lock(&tfa98xx->dsp_lock);
	err = tfa_dev_mtp_set(tfa98xx->tfa, TFA_MTP_EX, val);
	mutex_unlock(&tfa98xx->dsp_lock);

	if (err != tfa_error_ok) {
		pr_err("[0x%x] Unable to check DSP access: %d\n", tfa98xx->i2c->addr, err);
		return -EIO;
	}

	pr_debug("[0x%x] MTPEX < 0\n", tfa98xx->i2c->addr);

	return 0;
}

static int tfa98xx_dbgfs_temp_get(void *data, u64 *val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);

	mutex_lock(&tfa98xx->dsp_lock);
	*val = tfa98xx_get_exttemp(tfa98xx->tfa);
	mutex_unlock(&tfa98xx->dsp_lock);

	pr_debug("[0x%x] TEMP : %llu\n", tfa98xx->i2c->addr, *val);

	return 0;
}

static int tfa98xx_dbgfs_temp_set(void *data, u64 val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);

	mutex_lock(&tfa98xx->dsp_lock);
	tfa98xx_set_exttemp(tfa98xx->tfa, (short)val);
	mutex_unlock(&tfa98xx->dsp_lock);

	pr_debug("[0x%x] TEMP < %llu\n", tfa98xx->i2c->addr, val);

	return 0;
}

static ssize_t tfa98xx_dbgfs_start_set(struct file *file,
	const char __user *user_buf,
	size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	enum tfa_error ret;
	char buf[32];
	const char ref[] = "please calibrate now";
	int buf_size, cal_profile = 0;

	/* check string length, and account for eol */
	if (count > sizeof(ref) + 1 || count < (sizeof(ref) - 1))
		return -EINVAL;

	buf_size = min(count, (size_t)(sizeof(buf) - 1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size] = 0;

	/* Compare string, excluding the trailing \0 and the potentials eol */
	if (strncmp(buf, ref, sizeof(ref) - 1))
		return -EINVAL;

	mutex_lock(&tfa98xx->dsp_lock);
	ret = tfa_calibrate(tfa98xx->tfa);
	if (ret == tfa_error_ok) {
		cal_profile = tfaContGetCalProfile(tfa98xx->tfa);
		if (cal_profile < 0) {
			pr_warn("[0x%x] Calibration profile not found\n",
				tfa98xx->i2c->addr);
		}

		ret = tfa98xx_tfa_start(tfa98xx, cal_profile, tfa98xx->vstep);
	}
	if (ret == tfa_error_ok)
		tfa_dev_set_state(tfa98xx->tfa, TFA_STATE_UNMUTE, 0);
	mutex_unlock(&tfa98xx->dsp_lock);

	if (ret) {
		pr_info("[0x%x] Calibration start failed (%d)\n", tfa98xx->i2c->addr, ret);
		return -EIO;
	}
	else {
		pr_info("[0x%x] Calibration started\n", tfa98xx->i2c->addr);
	}

	return count;
}

static ssize_t tfa98xx_dbgfs_r_read(struct file *file,
	char __user *user_buf, size_t count,
	loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	char *str;
	uint16_t status;
	int ret;

	mutex_lock(&tfa98xx->dsp_lock);

	/* Need to ensure DSP is access-able, use mtp read access for this
	 * purpose
	 */
	ret = tfa98xx_get_mtp(tfa98xx->tfa, &status);
	if (ret) {
		ret = -EIO;
		pr_err("[0x%x] MTP read failed\n", tfa98xx->i2c->addr);
		goto r_c_err;
	}

	ret = tfaRunSpeakerCalibration(tfa98xx->tfa);
	if (ret) {
		ret = -EIO;
		pr_err("[0x%x] calibration failed\n", tfa98xx->i2c->addr);
		goto r_c_err;
	}

	str = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!str) {
		ret = -ENOMEM;
		pr_err("[0x%x] memory allocation failed\n", tfa98xx->i2c->addr);
		goto r_c_err;
	}

	if (tfa98xx->tfa->spkr_count > 1) {
		ret = snprintf(str, PAGE_SIZE,
			"Prim:%d mOhms, Sec:%d mOhms\n",
			tfa98xx->tfa->mohm[0],
			tfa98xx->tfa->mohm[1]);
	}
	else {
		ret = snprintf(str, PAGE_SIZE,
			"Prim:%d mOhms\n",
			tfa98xx->tfa->mohm[0]);
	}

	pr_debug("[0x%x] calib_done: %s", tfa98xx->i2c->addr, str);

	if (ret < 0)
		goto r_err;

	ret = simple_read_from_buffer(user_buf, count, ppos, str, ret);

r_err:
	kfree(str);
r_c_err:
	mutex_unlock(&tfa98xx->dsp_lock);
	return ret;
}

static ssize_t tfa98xx_dbgfs_version_read(struct file *file,
	char __user *user_buf, size_t count,
	loff_t *ppos)
{
	char str[] = TFA98XX_VERSION "\n";
	int ret;

	ret = simple_read_from_buffer(user_buf, count, ppos, str, sizeof(str));

	return ret;
}

static ssize_t tfa98xx_dbgfs_dsp_state_get(struct file *file,
	char __user *user_buf, size_t count,
	loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	int ret = 0;
	char *str;

	switch (tfa98xx->dsp_init) {
	case TFA98XX_DSP_INIT_STOPPED:
		str = "Stopped\n";
		break;
	case TFA98XX_DSP_INIT_RECOVER:
		str = "Recover requested\n";
		break;
	case TFA98XX_DSP_INIT_FAIL:
		str = "Failed init\n";
		break;
	case TFA98XX_DSP_INIT_PENDING:
		str = "Pending init\n";
		break;
	case TFA98XX_DSP_INIT_DONE:
		str = "Init complete\n";
		break;
	default:
		str = "Invalid\n";
	}

	pr_debug("[0x%x] dsp_state : %s\n", tfa98xx->i2c->addr, str);

	ret = simple_read_from_buffer(user_buf, count, ppos, str, strlen(str));
	return ret;
}

static ssize_t tfa98xx_dbgfs_dsp_state_set(struct file *file,
	const char __user *user_buf,
	size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	enum tfa_error ret;
	char buf[32];
	const char start_cmd[] = "start";
	const char stop_cmd[] = "stop";
	const char mon_start_cmd[] = "monitor start";
	const char mon_stop_cmd[] = "monitor stop";
	int buf_size;

	buf_size = min(count, (size_t)(sizeof(buf) - 1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size] = 0;

	/* Compare strings, excluding the trailing \0 */
	if (!strncmp(buf, start_cmd, sizeof(start_cmd) - 1)) {
		pr_info("[0x%x] Manual triggering of dsp start...\n", tfa98xx->i2c->addr);
		mutex_lock(&tfa98xx->dsp_lock);
		ret = tfa98xx_tfa_start(tfa98xx, tfa98xx->profile, tfa98xx->vstep);
		mutex_unlock(&tfa98xx->dsp_lock);
		pr_debug("[0x%x] tfa_dev_start complete: %d\n", tfa98xx->i2c->addr, ret);
	}
	else if (!strncmp(buf, stop_cmd, sizeof(stop_cmd) - 1)) {
		pr_info("[0x%x] Manual triggering of dsp stop...\n", tfa98xx->i2c->addr);
		mutex_lock(&tfa98xx->dsp_lock);
		ret = tfa_dev_stop(tfa98xx->tfa);
		mutex_unlock(&tfa98xx->dsp_lock);
		pr_debug("[0x%x] tfa_dev_stop complete: %d\n", tfa98xx->i2c->addr, ret);
	}
	else if (!strncmp(buf, mon_start_cmd, sizeof(mon_start_cmd) - 1)) {
		pr_info("[0x%x] Manual start of monitor thread...\n", tfa98xx->i2c->addr);
		queue_delayed_work(tfa98xx->tfa98xx_wq,
			&tfa98xx->monitor_work, HZ);
	}
	else if (!strncmp(buf, mon_stop_cmd, sizeof(mon_stop_cmd) - 1)) {
		pr_info("[0x%x] Manual stop of monitor thread...\n", tfa98xx->i2c->addr);
		cancel_delayed_work_sync(&tfa98xx->monitor_work);
	}
	else {
		return -EINVAL;
	}

	return count;
}

static ssize_t tfa98xx_dbgfs_fw_state_get(struct file *file,
	char __user *user_buf, size_t count,
	loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	char *str;

	switch (tfa98xx->dsp_fw_state) {
	case TFA98XX_DSP_FW_NONE:
		str = "None\n";
		break;
	case TFA98XX_DSP_FW_PENDING:
		str = "Pending\n";
		break;
	case TFA98XX_DSP_FW_FAIL:
		str = "Fail\n";
		break;
	case TFA98XX_DSP_FW_OK:
		str = "Ok\n";
		break;
	default:
		str = "Invalid\n";
	}

	pr_debug("[0x%x] fw_state : %s", tfa98xx->i2c->addr, str);

	return simple_read_from_buffer(user_buf, count, ppos, str, strlen(str));
}

static ssize_t tfa98xx_dbgfs_rpc_read(struct file *file,
	char __user *user_buf, size_t count,
	loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	int ret = 0;
	uint8_t *buffer;
	enum Tfa98xx_Error error;

	if (tfa98xx->tfa == NULL) {
		pr_debug("[0x%x] dsp is not available\n", tfa98xx->i2c->addr);
		return -ENODEV;
	}

	if (count == 0)
		return 0;

	buffer = kmalloc(count, GFP_KERNEL);
	if (buffer == NULL) {
		pr_debug("[0x%x] can not allocate memory\n", tfa98xx->i2c->addr);
		return -ENOMEM;
	}

	mutex_lock(&tfa98xx->dsp_lock);

	if (tfa98xx->tfa->is_probus_device) {
		error = send_tfa_cal_apr(buffer, count, true);
	} else {
		error = dsp_msg_read(tfa98xx->tfa, count, buffer);
	}
	mutex_unlock(&tfa98xx->dsp_lock);
	if (error != Tfa98xx_Error_Ok) {
		pr_debug("[0x%x] dsp_msg_read error: %d\n", tfa98xx->i2c->addr, error);
		kfree(buffer);
		return -EFAULT;
	}

	ret = copy_to_user(user_buf, buffer, count);
	kfree(buffer);
	if (ret)
		return -EFAULT;

	*ppos += count;
	return count;
}

static ssize_t tfa98xx_dbgfs_rpc_send(struct file *file,
	const char __user *user_buf,
	size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	TfaFileDsc_t *msg_file = NULL;
	enum Tfa98xx_Error error;
	int err = 0;

	if (tfa98xx->tfa == NULL) {
		pr_debug("[0x%x] dsp is not available\n", tfa98xx->i2c->addr);
		return -ENODEV;
	}

	if (count == 0)
		return 0;

	/* msg_file.name is not used */
	msg_file = kmalloc(count + sizeof(TfaFileDsc_t), GFP_KERNEL);
	if (msg_file == NULL) {
		pr_debug("[0x%x] can not allocate memory\n", tfa98xx->i2c->addr);
		return  -ENOMEM;
	}
	msg_file->size = count;

	if (copy_from_user(msg_file->data, user_buf, count)) {
		kfree(msg_file);
		return -EFAULT;
	}

	if (tfa98xx->tfa->is_probus_device) {
		mutex_lock(&tfa98xx->dsp_lock);
		error = send_tfa_cal_apr(msg_file->data, msg_file->size, false);
		if (error != Tfa98xx_Error_Ok) {
			pr_debug("[0x%x] dsp_msg error: %d\n", tfa98xx->i2c->addr, error);
			err = -EIO;
		}
		mutex_unlock(&tfa98xx->dsp_lock);

		mdelay(2);
	} else {
		mutex_lock(&tfa98xx->dsp_lock);

		if ((msg_file->data[0] == 'M') && (msg_file->data[1] == 'G')) {
			error = tfaContWriteFile(tfa98xx->tfa, msg_file, 0, 0); /* int vstep_idx, int vstep_msg_idx both 0 */
			if (error != Tfa98xx_Error_Ok) {
				pr_debug("[0x%x] tfaContWriteFile error: %d\n", tfa98xx->i2c->addr, error);
				err = -EIO;
			}
		} else {
			error = dsp_msg(tfa98xx->tfa, msg_file->size, msg_file->data);
			if (error != Tfa98xx_Error_Ok) {
				pr_debug("[0x%x] dsp_msg error: %d\n", tfa98xx->i2c->addr, error);
				err = -EIO;
    		}
	    }
		mutex_unlock(&tfa98xx->dsp_lock);
	}

	kfree(msg_file);

	if (err)
		return err;
	return count;
}
/* -- RPC */

static int tfa98xx_dbgfs_pga_gain_get(void *data, u64 *val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	unsigned int value;

	value = tfa_get_pga_gain(tfa98xx->tfa);
	if (value < 0)
		return -EINVAL;

	*val = value;
	return 0;
}

static int tfa98xx_dbgfs_pga_gain_set(void *data, u64 val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	uint16_t value;
	int err;

	value = val & 0xffff;
	if (value > 7)
		return -EINVAL;

	err = tfa_set_pga_gain(tfa98xx->tfa, value);
	if (err < 0)
		return -EINVAL;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(tfa98xx_dbgfs_calib_otc_fops, tfa98xx_dbgfs_otc_get,
	tfa98xx_dbgfs_otc_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(tfa98xx_dbgfs_calib_mtpex_fops, tfa98xx_dbgfs_mtpex_get,
	tfa98xx_dbgfs_mtpex_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(tfa98xx_dbgfs_calib_temp_fops, tfa98xx_dbgfs_temp_get,
	tfa98xx_dbgfs_temp_set, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(tfa98xx_dbgfs_pga_gain_fops, tfa98xx_dbgfs_pga_gain_get,
	tfa98xx_dbgfs_pga_gain_set, "%llu\n");

static const struct file_operations tfa98xx_dbgfs_calib_start_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.write = tfa98xx_dbgfs_start_set,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_r_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = tfa98xx_dbgfs_r_read,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_version_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = tfa98xx_dbgfs_version_read,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_dsp_state_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = tfa98xx_dbgfs_dsp_state_get,
	.write = tfa98xx_dbgfs_dsp_state_set,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_fw_state_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = tfa98xx_dbgfs_fw_state_get,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_rpc_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = tfa98xx_dbgfs_rpc_read,
	.write = tfa98xx_dbgfs_rpc_send,
	.llseek = default_llseek,
};

static void tfa98xx_debug_init(struct tfa98xx *tfa98xx, struct i2c_client *i2c)
{
	char name[50];

	scnprintf(name, MAX_CONTROL_NAME, "%s-%x", i2c->name, i2c->addr);
	tfa98xx->dbg_dir = debugfs_create_dir(name, NULL);
	debugfs_create_file("OTC", S_IRUGO | S_IWUGO, tfa98xx->dbg_dir,
		i2c, &tfa98xx_dbgfs_calib_otc_fops);
	debugfs_create_file("MTPEX", S_IRUGO | S_IWUGO, tfa98xx->dbg_dir,
		i2c, &tfa98xx_dbgfs_calib_mtpex_fops);
	debugfs_create_file("TEMP", S_IRUGO | S_IWUGO, tfa98xx->dbg_dir,
		i2c, &tfa98xx_dbgfs_calib_temp_fops);
	debugfs_create_file("calibrate", S_IRUGO | S_IWUGO, tfa98xx->dbg_dir,
		i2c, &tfa98xx_dbgfs_calib_start_fops);
	debugfs_create_file("R", S_IRUGO, tfa98xx->dbg_dir,
		i2c, &tfa98xx_dbgfs_r_fops);
	debugfs_create_file("version", S_IRUGO, tfa98xx->dbg_dir,
		i2c, &tfa98xx_dbgfs_version_fops);
	debugfs_create_file("dsp-state", S_IRUGO | S_IWUGO, tfa98xx->dbg_dir,
		i2c, &tfa98xx_dbgfs_dsp_state_fops);
	debugfs_create_file("fw-state", S_IRUGO | S_IWUGO, tfa98xx->dbg_dir,
		i2c, &tfa98xx_dbgfs_fw_state_fops);
	debugfs_create_file("rpc", S_IRUGO | S_IWUGO, tfa98xx->dbg_dir,
		i2c, &tfa98xx_dbgfs_rpc_fops);

	if (tfa98xx->flags & TFA98XX_FLAG_SAAM_AVAILABLE) {
		dev_dbg(tfa98xx->dev, "Adding pga_gain debug interface\n");
		debugfs_create_file("pga_gain", S_IRUGO, tfa98xx->dbg_dir,
			tfa98xx->i2c,
			&tfa98xx_dbgfs_pga_gain_fops);
	}
}

static void tfa98xx_debug_remove(struct tfa98xx *tfa98xx)
{
	if (tfa98xx->dbg_dir)
		debugfs_remove_recursive(tfa98xx->dbg_dir);
}
#endif


/* copies the profile basename (i.e. part until .) into buf */
static void get_profile_basename(char* buf, char* profile)
{
	int cp_len = 0, idx = 0;
	char *pch;

	pch = strchr(profile, '.');
	idx = pch - profile;
	cp_len = (pch != NULL) ? idx : (int)strlen(profile);
	memcpy(buf, profile, cp_len);
	buf[cp_len] = 0;
}

/* return the profile name accociated with id from the profile list */
static int get_profile_from_list(char *buf, int id)
{
	struct tfa98xx_baseprofile *bprof;

	list_for_each_entry(bprof, &profile_list, list) {
		if (bprof->item_id == id) {
			strcpy(buf, bprof->basename);
			return 0;
		}
	}

	return -1;
}

/* search for the profile in the profile list */
static int is_profile_in_list(char *profile, int len)
{
	struct tfa98xx_baseprofile *bprof;

	list_for_each_entry(bprof, &profile_list, list) {

		if ((len == bprof->len) && (0 == strncmp(bprof->basename, profile, len)))
			return 1;
	}

	return 0;
}

/*
 * for the profile with id, look if the requested samplerate is
 * supported, if found return the (container)profile for this
 * samplerate, on error or if not found return -1
 */
static int get_profile_id_for_sr(int id, unsigned int rate)
{
	int idx = 0;
	struct tfa98xx_baseprofile *bprof;

	list_for_each_entry(bprof, &profile_list, list) {
		if (id == bprof->item_id) {
			idx = tfa98xx_get_fssel(rate);
			if (idx < 0) {
				/* samplerate not supported */
				return -1;
			}

			return bprof->sr_rate_sup[idx];
		}
	}

	/* profile not found */
	return -1;
}

static int get_profile_id_by_name(char *profile, int len)
{
	struct tfa98xx_baseprofile *bprof;
	int prof_index = -1;

	list_for_each_entry(bprof, &profile_list, list) {
		if (strncmp(profile, bprof->basename, len) == 0) {
			prof_index = bprof->item_id;
			break;
		}
	}

	return prof_index;
}

/* check if this profile is a calibration profile */
static int is_calibration_profile(char *profile)
{
	if (strstr(profile, ".cal") != NULL)
		return 1;
	return 0;
}

/*
 * adds the (container)profile index of the samplerate found in
 * the (container)profile to a fixed samplerate table in the (mixer)profile
 */
static int add_sr_to_profile(struct tfa98xx *tfa98xx, char *basename, int len, int profile)
{
	struct tfa98xx_baseprofile *bprof;
	int idx = 0;
	unsigned int sr = 0;

	list_for_each_entry(bprof, &profile_list, list) {
		if ((len == bprof->len) && (0 == strncmp(bprof->basename, basename, len))) {
			/* add supported samplerate for this profile */
			sr = tfa98xx_get_profile_sr(tfa98xx->tfa, profile);
			if (!sr) {
				pr_err("unable to identify supported sample rate for %s\n", bprof->basename);
				return -1;
			}

			/* get the index for this samplerate */
			idx = tfa98xx_get_fssel(sr);
			if (idx < 0 || idx >= TFA98XX_NUM_RATES) {
				pr_err("invalid index for samplerate %d\n", idx);
				return -1;
			}

			/* enter the (container)profile for this samplerate at the corresponding index */
			bprof->sr_rate_sup[idx] = profile;

			pr_debug("added profile:samplerate = [%d:%d] for mixer profile: %s\n", profile, sr, bprof->basename);
		}
	}

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,16,0)
static struct snd_soc_codec *snd_soc_kcontrol_codec(struct snd_kcontrol *kcontrol)
{
	return snd_kcontrol_chip(kcontrol);
}
#endif

static int tfa98xx_get_vstep(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_component_get_drvdata(codec);
#else
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
#endif
	int mixer_profile = kcontrol->private_value;
	int ret = 0;
	int profile;

	profile = get_profile_id_for_sr(mixer_profile, tfa98xx->rate);
	if (profile < 0) {
		pr_err("tfa98xx: tfa98xx_get_vstep: invalid profile %d (mixer_profile=%d, rate=%d)\n", profile, mixer_profile, tfa98xx->rate);
		return -EINVAL;
	}

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		int vstep = tfa98xx->prof_vsteps[profile];

		ucontrol->value.integer.value[tfa98xx->tfa->dev_idx] =
			tfacont_get_max_vstep(tfa98xx->tfa, profile)
			- vstep - 1;
	}
	mutex_unlock(&tfa98xx_mutex);

	return ret;
}

static int tfa98xx_set_vstep(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_component_get_drvdata(codec);
#else
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
#endif
	int mixer_profile = kcontrol->private_value;
	int profile;
	int err = 0;
	int change = 0;

	pr_debug("%s  no_start=%d\n", __func__, no_start);
	if (no_start != 0)
		return 0;

	profile = get_profile_id_for_sr(mixer_profile, tfa98xx->rate);
	if (profile < 0) {
		pr_err("tfa98xx: tfa98xx_set_vstep: invalid profile %d (mixer_profile=%d, rate=%d)\n", profile, mixer_profile, tfa98xx->rate);
		return -EINVAL;
	}

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		int vstep, vsteps;
		int ready = 0;
		int new_vstep;
		int value = ucontrol->value.integer.value[tfa98xx->tfa->dev_idx];

		vstep = tfa98xx->prof_vsteps[profile];
		vsteps = tfacont_get_max_vstep(tfa98xx->tfa, profile);

		if (vstep == vsteps - value - 1)
			continue;

		new_vstep = vsteps - value - 1;

		if (new_vstep < 0)
			new_vstep = 0;

		tfa98xx->prof_vsteps[profile] = new_vstep;

#ifndef TFA98XX_ALSA_CTRL_PROF_CHG_ON_VOL
		if (profile == tfa98xx->profile) {
#endif
			/* this is the active profile, program the new vstep */
			tfa98xx->vstep = new_vstep;
			mutex_lock(&tfa98xx->dsp_lock);
			tfa98xx_dsp_system_stable(tfa98xx->tfa, &ready);

			if (ready) {
				err = tfa98xx_tfa_start(tfa98xx, tfa98xx->profile, tfa98xx->vstep);
				if (err) {
					pr_err("Write vstep error: %d\n", err);
				}
				else {
					pr_debug("Succesfully changed vstep index!\n");
					change = 1;
				}
			}

			mutex_unlock(&tfa98xx->dsp_lock);
#ifndef TFA98XX_ALSA_CTRL_PROF_CHG_ON_VOL
		}
#endif
		pr_debug("%d: vstep:%d, (control value: %d) - profile %d\n",
			tfa98xx->tfa->dev_idx, new_vstep, value, profile);
	}

	if (change) {
		list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
			mutex_lock(&tfa98xx->dsp_lock);
			tfa_dev_set_state(tfa98xx->tfa, TFA_STATE_UNMUTE, 0);
			mutex_unlock(&tfa98xx->dsp_lock);
		}
	}

	mutex_unlock(&tfa98xx_mutex);

	return change;
}

static int tfa98xx_info_vstep(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_component_get_drvdata(codec);
#else
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
#endif

	int mixer_profile = tfa98xx_mixer_profile;
	int profile = get_profile_id_for_sr(mixer_profile, tfa98xx->rate);
	if (profile < 0) {
		pr_err("tfa98xx: tfa98xx_info_vstep: invalid profile %d (mixer_profile=%d, rate=%d)\n", profile, mixer_profile, tfa98xx->rate);
		return -EINVAL;
	}

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	mutex_lock(&tfa98xx_mutex);
	uinfo->count = tfa98xx_device_count;
	mutex_unlock(&tfa98xx_mutex);
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = max(0, tfacont_get_max_vstep(tfa98xx->tfa, profile) - 1);
	pr_debug("vsteps count: %d [prof=%d]\n", tfacont_get_max_vstep(tfa98xx->tfa, profile),
		profile);
	return 0;
}

static int tfa98xx_get_profile(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&tfa98xx_mutex);
	ucontrol->value.integer.value[0] = tfa98xx_mixer_profile;
	mutex_unlock(&tfa98xx_mutex);

	return 0;
}

static int tfa98xx_set_profile(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_component_get_drvdata(codec);
#else
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
#endif
	int change = 0;
	int new_profile;
	int prof_idx;
	int profile_count = tfa98xx_mixer_profiles;
	int profile = tfa98xx_mixer_profile;

	if (no_start != 0)
		return 0;

	new_profile = ucontrol->value.integer.value[0];
	if (new_profile == profile)
		return 0;

	if ((new_profile < 0) || (new_profile >= profile_count)) {
		pr_err("not existing profile (%d)\n", new_profile);
		return -EINVAL;
	}

	/* get the container profile for the requested sample rate */
	prof_idx = get_profile_id_for_sr(new_profile, tfa98xx->rate);
	if (prof_idx < 0) {
		pr_err("tfa98xx: sample rate [%d] not supported for this mixer profile [%d].\n", tfa98xx->rate, new_profile);
		return 0;
	}
	pr_debug("selected container profile [%d]\n", prof_idx);

	/* update mixer profile */
	tfa98xx_mixer_profile = new_profile;

	/* modified by zengjiangtao begin. */
	/* we are updating profile index only if the device is not in operating mode, and will be start in tfa98xx_mute() later.
	if the device in operating mode, we will apply new profile now. */
	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		int err;
		int ready = 0;
	 /* Flag DSP as invalidated as the profile change may invalidate the
	 * current DSP configuration. That way, further stream start can
	 * trigger a tfa_dev_start.*/
		tfa98xx->dsp_init = TFA98XX_DSP_INIT_INVALIDATED;

		/* update 'real' profile (container profile) */
		tfa98xx->profile = prof_idx;
		tfa98xx->vstep = tfa98xx->prof_vsteps[prof_idx];
		if (!tfa98xx->tfa->is_probus_device) {
			/* Don't call tfa_dev_start() if there is no clock. */
			mutex_lock(&tfa98xx->dsp_lock);
			tfa98xx_dsp_system_stable(tfa98xx->tfa, &ready);
			if (ready && (tfa_dev_get_state(tfa98xx->tfa) == TFA_STATE_OPERATING)) {
				/* Also re-enables the interrupts */
				err = tfa98xx_tfa_start(tfa98xx, prof_idx, tfa98xx->vstep);
				if (err) {
					pr_info("Write profile error: %d\n", err);
				} else {
					pr_debug("Changed to profile %d (vstep = %d)\n",
					         prof_idx, tfa98xx->vstep);
					change = 1;
				}
			}
			mutex_unlock(&tfa98xx->dsp_lock);
		}
	}

	if (change) {
		list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
			mutex_lock(&tfa98xx->dsp_lock);
			tfa_dev_set_state(tfa98xx->tfa, TFA_STATE_UNMUTE, 0);
			mutex_unlock(&tfa98xx->dsp_lock);
		}
	}

	mutex_unlock(&tfa98xx_mutex);

	/* modified by zengjiangtao end. */

	return change;
}

static int tfa98xx_info_profile(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	char profile_name[MAX_CONTROL_NAME] = { 0 };
	int count = tfa98xx_mixer_profiles, err = -1;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = count;

	if (uinfo->value.enumerated.item >= count)
		uinfo->value.enumerated.item = count - 1;

	err = get_profile_from_list(profile_name, uinfo->value.enumerated.item);
	if (err != 0)
		return -EINVAL;

	strcpy(uinfo->value.enumerated.name, profile_name);

	return 0;
}

static int tfa98xx_info_stop_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	mutex_lock(&tfa98xx_mutex);
	uinfo->count = tfa98xx_device_count;
	mutex_unlock(&tfa98xx_mutex);
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	return 0;
}

static int tfa98xx_get_stop_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx;

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		ucontrol->value.integer.value[tfa98xx->tfa->dev_idx] = 0;
	}
	mutex_unlock(&tfa98xx_mutex);

	return 0;
}

static int tfa98xx_set_stop_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx;

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		int ready = 0;
		int i = tfa98xx->tfa->dev_idx;

		pr_debug("%d: %ld\n", i, ucontrol->value.integer.value[i]);

		tfa98xx_dsp_system_stable(tfa98xx->tfa, &ready);

		if ((ucontrol->value.integer.value[i] != 0) && ready) {
			cancel_delayed_work_sync(&tfa98xx->monitor_work);

			cancel_delayed_work_sync(&tfa98xx->init_work);
			if (tfa98xx->dsp_fw_state != TFA98XX_DSP_FW_OK)
				continue;

			mutex_lock(&tfa98xx->dsp_lock);
			tfa_dev_stop(tfa98xx->tfa);
			tfa98xx->dsp_init = TFA98XX_DSP_INIT_STOPPED;
			mutex_unlock(&tfa98xx->dsp_lock);
		}

		ucontrol->value.integer.value[i] = 0;
	}
	mutex_unlock(&tfa98xx_mutex);

	return 1;
}

static int tfa98xx_info_PAmute(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	mutex_lock(&tfa98xx_mutex);
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = tfa98xx_device_count;
	uinfo->value.integer.min = TFA98XX_DEVICE_MUTE_OFF;
	uinfo->value.integer.max = TFA98XX_DEVICE_MUTE_ON;
	mutex_unlock(&tfa98xx_mutex);

	return 0;
}

static int tfa98xx_get_PAmute(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx = NULL;

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		ucontrol->value.integer.value[tfa98xx->tfa->dev_idx] = tfa98xx->tfa_mute_mode;
	}
	mutex_unlock(&tfa98xx_mutex);

	return 0;
}

static int tfa98xx_set_PAmute(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx = NULL;

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		if (ucontrol->value.integer.value[tfa98xx->tfa->dev_idx] != TFA98XX_DEVICE_MUTE_OFF) {
			tfa98xx->tfa_mute_mode = TFA98XX_DEVICE_MUTE_ON;
		} else {
			tfa98xx->tfa_mute_mode = TFA98XX_DEVICE_MUTE_OFF;
		}
	}
	mutex_unlock(&tfa98xx_mutex);

	return 1;
}

static int tfa98xx_info_cal_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	mutex_lock(&tfa98xx_mutex);
	uinfo->count = tfa98xx_device_count;
	mutex_unlock(&tfa98xx_mutex);
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0xffff; /* 16 bit value */

	return 0;
}

static int tfa98xx_set_cal_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx;

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		enum tfa_error err;
		int i = tfa98xx->tfa->dev_idx;

		tfa98xx->cal_data = (uint16_t)ucontrol->value.integer.value[i];

		mutex_lock(&tfa98xx->dsp_lock);
		err = tfa98xx_write_re25(tfa98xx->tfa, tfa98xx->cal_data);
		tfa98xx->set_mtp_cal = (err != tfa_error_ok);
		if (tfa98xx->set_mtp_cal == false) {
			pr_info("Calibration value (%d) set in mtp\n",
				tfa98xx->cal_data);
		}
		mutex_unlock(&tfa98xx->dsp_lock);
	}
	mutex_unlock(&tfa98xx_mutex);

	return 1;
}

static int tfa98xx_get_cal_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx;

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		mutex_lock(&tfa98xx->dsp_lock);
		ucontrol->value.integer.value[tfa98xx->tfa->dev_idx] = tfa_dev_mtp_get(tfa98xx->tfa, TFA_MTP_RE25_PRIM);
		mutex_unlock(&tfa98xx->dsp_lock);
	}
	mutex_unlock(&tfa98xx_mutex);

	return 0;
}

#ifdef TFA_NON_DSP_SOLUTION
static atomic_t g_algo_bypass;
static atomic_t g_algo_mute;
static atomic_t g_Tx_enable;
extern int send_tfa_cal_set_bypass(void *buf, int cmd_size);
extern int send_tfa_cal_set_tx_enable(void *buf, int cmd_size);
extern int send_tfa_cal_in_band(void *buf, int cmd_size);

/*************bypass control***************/
static int tfa98xx_send_mute_cmd(int mute)
{
    u8 cmd[9]= {0x00, 0x81, 0x03,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    if (mute != 0) {
        cmd[5] = TFA_KCONTROL_VALUE_ENABLED; //mute left channel
        cmd[8] = TFA_KCONTROL_VALUE_ENABLED; //mute right channel
    }
	pr_info("send mute command to host DSP.\n");
	return send_tfa_cal_in_band(&cmd[0], sizeof(cmd));
}

static int tfa987x_algo_get_status(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int32_t ret = 0;
	ucontrol->value.integer.value[0] = atomic_read(&g_algo_bypass);
	return ret;
}

static int tfa987x_algo_set_status(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int32_t ret = 0;
	u8 buff[56] = {0}, *ptr = buff;
	((int32_t *)buff)[0] = ucontrol->value.integer.value[0];
	pr_err("%s:status data %d\n", __func__, ((int32_t *)buff)[0]);
	atomic_set(&g_algo_bypass, ((int32_t *)buff)[0]);
	ret = send_tfa_cal_set_bypass(ptr, 4);
	return ret;
}

static int tfa987x_algo_set_mute(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
    pr_err("%s:  mute=%ld\n", __func__, ucontrol->value.integer.value[0]);

    atomic_set(&g_algo_mute, ucontrol->value.integer.value[0]);
    return tfa98xx_send_mute_cmd(ucontrol->value.integer.value[0]);
}

static int tfa987x_algo_get_mute(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = atomic_read(&g_algo_mute);
	return 0;
}

static int tfa987x_algo_set_tx_enable(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int32_t ret = 0;
	u8 buff[56] = {0}, *ptr = buff;
	((int32_t *)buff)[0] = ucontrol->value.integer.value[0];
	pr_err("%s:set_tx_enable %d\n", __func__, ((int32_t *)buff)[0]);
	atomic_set(&g_Tx_enable, ((int32_t *)buff)[0]);
	ret = send_tfa_cal_set_tx_enable(ptr, 4);
	return ret;
}

static int tfa987x_algo_get_tx_status(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int32_t ret = 0;
	ucontrol->value.integer.value[0] = atomic_read(&g_Tx_enable);
	return ret;
}

static const char *tfa987x_algo_text[] = {
	"ENABLED", "DISABLED"
};

static const char *tfa987x_tx_text[] = {
	"DISABLE", "ENABLE"
};

static const char *tfa987x_algo_mute_text[] = {
	"DISABLED", "ENABLED"
};

static const struct soc_enum tfa987x_algo_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tfa987x_algo_text),tfa987x_algo_text)
};

static const struct soc_enum tfa987x_algo_mute_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tfa987x_algo_mute_text),tfa987x_algo_mute_text)
};

static const struct soc_enum tfa987x_tx_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tfa987x_tx_text),tfa987x_tx_text)
};

const struct snd_kcontrol_new tfa987x_algo_controls[] = {
	SOC_ENUM_EXT("TFA987X_ALGO_STATUS", tfa987x_algo_enum[0], tfa987x_algo_get_status, tfa987x_algo_set_status),
    SOC_ENUM_EXT("TFA987X_ALGO_MUTE", tfa987x_algo_mute_enum[0], tfa987x_algo_get_mute, tfa987x_algo_set_mute),
	SOC_ENUM_EXT("TFA987X_TX_ENABLE", tfa987x_tx_enum[0], tfa987x_algo_get_tx_status, tfa987x_algo_set_tx_enable)
};
#endif

static int tfa98xx_create_controls(struct tfa98xx *tfa98xx)
{
	int prof, nprof, mix_index = 0;
	int  nr_controls = 0, id = 0;
	char *name;
	struct tfa98xx_baseprofile *bprofile;
	int ret = 0;

	/* Create the following controls:
	 *  - enum control to select the active profile
	 *  - one volume control for each profile hosting a vstep
	 *  - Stop control on TFA1 devices
	 */

	nr_controls = 2; /* Profile and stop control */

	if (tfa98xx->flags & TFA98XX_FLAG_CALIBRATION_CTL)
		nr_controls += 1; /* calibration */

	/* allocate the tfa98xx_controls base on the nr of profiles */
	nprof = tfa_cnt_get_dev_nprof(tfa98xx->tfa);
	for (prof = 0; prof < nprof; prof++) {
		if (tfacont_get_max_vstep(tfa98xx->tfa, prof))
			nr_controls++; /* Playback Volume control */
	}
	/* xiaomi's requirement */
	if (tfa98xx->tfa->tfa_family == 2) {
		nr_controls++; /* SmartPA Mute control */
	}

	tfa98xx_controls = devm_kzalloc(tfa98xx->codec->dev,
		nr_controls * sizeof(tfa98xx_controls[0]), GFP_KERNEL);
	if (!tfa98xx_controls)
		return -ENOMEM;

	/* Create a mixer item for selecting the active profile */
	name = devm_kzalloc(tfa98xx->codec->dev, MAX_CONTROL_NAME, GFP_KERNEL);
	if (!name)
		return -ENOMEM;
	scnprintf(name, MAX_CONTROL_NAME, "%s Profile", tfa98xx->fw.name);
	tfa98xx_controls[mix_index].name = name;
	tfa98xx_controls[mix_index].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	tfa98xx_controls[mix_index].info = tfa98xx_info_profile;
	tfa98xx_controls[mix_index].get = tfa98xx_get_profile;
	tfa98xx_controls[mix_index].put = tfa98xx_set_profile;
	// tfa98xx_controls[mix_index].private_value = profs; /* save number of profiles */
	mix_index++;

	/* xiaomi's requirement */
	/* add new mixer control item 'SmartPA Mute' for MAX2 device. */
	if (tfa98xx->tfa->tfa_family == 2) {
		tfa98xx_controls[mix_index].name = "SmartPA Mute";
		tfa98xx_controls[mix_index].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
		tfa98xx_controls[mix_index].info = tfa98xx_info_PAmute;
		tfa98xx_controls[mix_index].get	= tfa98xx_get_PAmute;
		tfa98xx_controls[mix_index].put	= tfa98xx_set_PAmute;
		mix_index++;
	}

	/* create mixer items for each profile that has volume */
	for (prof = 0; prof < nprof; prof++) {
		/* create an new empty profile */
		bprofile = devm_kzalloc(tfa98xx->codec->dev, sizeof(*bprofile), GFP_KERNEL);
		if (!bprofile)
			return -ENOMEM;

		bprofile->len = 0;
		bprofile->item_id = -1;
		INIT_LIST_HEAD(&bprofile->list);

		/* copy profile name into basename until the . */
		get_profile_basename(bprofile->basename, tfa_cont_profile_name(tfa98xx, prof));
		bprofile->len = strlen(bprofile->basename);

		/*
		 * search the profile list for a profile with basename, if it is not found then
		 * add it to the list and add a new mixer control (if it has vsteps)
		 * also, if it is a calibration profile, do not add it to the list
		 */
		if ((is_profile_in_list(bprofile->basename, bprofile->len) == 0) &&
			is_calibration_profile(tfa_cont_profile_name(tfa98xx, prof)) == 0) {
			/* the profile is not present, add it to the list */
			list_add(&bprofile->list, &profile_list);
			bprofile->item_id = id++;

			pr_debug("profile added [%d]: %s\n", bprofile->item_id, bprofile->basename);

			if (tfacont_get_max_vstep(tfa98xx->tfa, prof)) {
				name = devm_kzalloc(tfa98xx->codec->dev, MAX_CONTROL_NAME, GFP_KERNEL);
				if (!name)
					return -ENOMEM;

				scnprintf(name, MAX_CONTROL_NAME, "%s %s Playback Volume",
					tfa98xx->fw.name, bprofile->basename);

				tfa98xx_controls[mix_index].name = name;
				tfa98xx_controls[mix_index].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
				tfa98xx_controls[mix_index].info = tfa98xx_info_vstep;
				tfa98xx_controls[mix_index].get = tfa98xx_get_vstep;
				tfa98xx_controls[mix_index].put = tfa98xx_set_vstep;
				tfa98xx_controls[mix_index].private_value = bprofile->item_id; /* save profile index */
				mix_index++;
			}
		}

		/* look for the basename profile in the list of mixer profiles and add the
		   container profile index to the supported samplerates of this mixer profile */
		add_sr_to_profile(tfa98xx, bprofile->basename, bprofile->len, prof);
	}

	/* set the number of user selectable profiles in the mixer */
	tfa98xx_mixer_profiles = id;

	/* Create a mixer item for stop control on TFA1 */
	name = devm_kzalloc(tfa98xx->codec->dev, MAX_CONTROL_NAME, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	scnprintf(name, MAX_CONTROL_NAME, "%s Stop", tfa98xx->fw.name);
	tfa98xx_controls[mix_index].name = name;
	tfa98xx_controls[mix_index].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	tfa98xx_controls[mix_index].info = tfa98xx_info_stop_ctl;
	tfa98xx_controls[mix_index].get = tfa98xx_get_stop_ctl;
	tfa98xx_controls[mix_index].put = tfa98xx_set_stop_ctl;
	mix_index++;

	if (tfa98xx->flags & TFA98XX_FLAG_CALIBRATION_CTL) {
		name = devm_kzalloc(tfa98xx->codec->dev, MAX_CONTROL_NAME, GFP_KERNEL);
		if (!name)
			return -ENOMEM;

		scnprintf(name, MAX_CONTROL_NAME, "%s Calibration", tfa98xx->fw.name);
		tfa98xx_controls[mix_index].name = name;
		tfa98xx_controls[mix_index].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
		tfa98xx_controls[mix_index].info = tfa98xx_info_cal_ctl;
		tfa98xx_controls[mix_index].get = tfa98xx_get_cal_ctl;
		tfa98xx_controls[mix_index].put = tfa98xx_set_cal_ctl;
		mix_index++;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
	ret = snd_soc_add_component_controls(tfa98xx->codec,
		tfa98xx_controls, mix_index);
	pr_info("create tfa98xx_controls  ret=%d", ret);

#ifdef TFA_NON_DSP_SOLUTION
	ret = snd_soc_add_component_controls(tfa98xx->codec,
		tfa987x_algo_controls, ARRAY_SIZE(tfa987x_algo_controls));
	pr_info("create tfa987x_algo_controls  ret=%d", ret);
	/* reset kcontrol flag once power down tfa device. */
	atomic_set(&g_algo_bypass, TFA_KCONTROL_VALUE_DISABLED);
	atomic_set(&g_algo_mute, TFA_KCONTROL_VALUE_DISABLED);
	atomic_set(&g_Tx_enable, TFA_KCONTROL_VALUE_ENABLED);
#endif

#else
	ret = snd_soc_add_codec_controls(tfa98xx->codec,
		tfa98xx_controls, mix_index);
	pr_info("create tfa98xx_controls  ret=%d", ret);

	ret = snd_soc_add_codec_controls(tfa98xx->codec,
		nxp_spk_id_controls, ARRAY_SIZE(nxp_spk_id_controls));
	pr_info("create nxp_spk_id_controls  ret=%d", ret);

#ifdef TFA_NON_DSP_SOLUTION
	ret = snd_soc_add_codec_controls(tfa98xx->codec,
		tfa987x_algo_controls,
		ARRAY_SIZE(tfa987x_algo_controls));
	pr_info("create tfa987x_algo_controls  ret=%d", ret);
#endif
#endif

	return ret;
}

static void *tfa98xx_devm_kstrdup(struct device *dev, char *buf)
{
	char *str = devm_kzalloc(dev, strlen(buf) + 1, GFP_KERNEL);
	if (!str)
		return str;
	memcpy(str, buf, strlen(buf));
	return str;
}

static int tfa98xx_append_i2c_address(struct device *dev,
	struct i2c_client *i2c,
	struct snd_soc_dapm_widget *widgets,
	int num_widgets,
	struct snd_soc_dai_driver *dai_drv,
	int num_dai)
{
	char buf[50];
	int i;
	int i2cbus = i2c->adapter->nr;
	int addr = i2c->addr;
	if (dai_drv && num_dai > 0)
		for (i = 0; i < num_dai; i++) {
			memset(buf, 0x00, sizeof(buf));
			snprintf(buf, 50, "%s-%x-%x", dai_drv[i].name, i2cbus,
				addr);
			dai_drv[i].name = tfa98xx_devm_kstrdup(dev, buf);
			pr_info("tfa98xx_append_i2c_address()  dai_drv[%d].name = [%s]\n", i, dai_drv[i].name);

			memset(buf, 0x00, sizeof(buf));
			snprintf(buf, 50, "%s-%x-%x",
				dai_drv[i].playback.stream_name,
				i2cbus, addr);
			dai_drv[i].playback.stream_name = tfa98xx_devm_kstrdup(dev, buf);
			pr_info("tfa98xx_append_i2c_address()  dai_drv[%d].playback.stream_name = [%s]\n", i, dai_drv[i].playback.stream_name);

			memset(buf, 0x00, sizeof(buf));
			snprintf(buf, 50, "%s-%x-%x",
				dai_drv[i].capture.stream_name,
				i2cbus, addr);
			dai_drv[i].capture.stream_name = tfa98xx_devm_kstrdup(dev, buf);
			pr_info("tfa98xx_append_i2c_address()  dai_drv[%d].capture.stream_name = [%s]\n", i, dai_drv[i].capture.stream_name);
		}

	/* the idea behind this is convert:
	 * SND_SOC_DAPM_AIF_IN("AIF IN", "AIF Playback", 0, SND_SOC_NOPM, 0, 0),
	 * into:
	 * SND_SOC_DAPM_AIF_IN("AIF IN", "AIF Playback-2-36", 0, SND_SOC_NOPM, 0, 0),
	 */
	if (widgets && num_widgets > 0)
		for (i = 0; i < num_widgets; i++) {
			if (!widgets[i].sname)
				continue;
			if ((widgets[i].id == snd_soc_dapm_aif_in)
				|| (widgets[i].id == snd_soc_dapm_aif_out)) {
				snprintf(buf, 50, "%s-%x-%x", widgets[i].sname,
					i2cbus, addr);
				widgets[i].sname = tfa98xx_devm_kstrdup(dev, buf);
			}
		}

	return 0;
}

static struct snd_soc_dapm_widget tfa98xx_dapm_widgets_common[] = {
	/* Stream widgets */
	SND_SOC_DAPM_AIF_IN("AIF IN", "AIF Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF OUT", "AIF Capture", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_OUTPUT("OUTL"),
	SND_SOC_DAPM_INPUT("AEC Loopback"),
};

static struct snd_soc_dapm_widget tfa98xx_dapm_widgets_stereo[] = {
	SND_SOC_DAPM_OUTPUT("OUTR"),
};

static struct snd_soc_dapm_widget tfa98xx_dapm_widgets_saam[] = {
	SND_SOC_DAPM_INPUT("SAAM MIC"),
};

static struct snd_soc_dapm_widget tfa9888_dapm_inputs[] = {
	SND_SOC_DAPM_INPUT("DMIC1"),
	SND_SOC_DAPM_INPUT("DMIC2"),
	SND_SOC_DAPM_INPUT("DMIC3"),
	SND_SOC_DAPM_INPUT("DMIC4"),
};

static const struct snd_soc_dapm_route tfa98xx_dapm_routes_common[] = {
	{ "OUTL", NULL, "AIF IN" },
	{ "AIF OUT", NULL, "AEC Loopback" },
};

static const struct snd_soc_dapm_route tfa98xx_dapm_routes_saam[] = {
	{ "AIF OUT", NULL, "SAAM MIC" },
};

static const struct snd_soc_dapm_route tfa98xx_dapm_routes_stereo[] = {
	{ "OUTR", NULL, "AIF IN" },
};

static const struct snd_soc_dapm_route tfa9888_input_dapm_routes[] = {
	{ "AIF OUT", NULL, "DMIC1" },
	{ "AIF OUT", NULL, "DMIC2" },
	{ "AIF OUT", NULL, "DMIC3" },
	{ "AIF OUT", NULL, "DMIC4" },
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,2,0)
static struct snd_soc_dapm_context *snd_soc_codec_get_dapm(struct snd_soc_codec *codec)
{
	return &codec->dapm;
}
#endif

static void tfa98xx_add_widgets(struct tfa98xx *tfa98xx)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
	struct snd_soc_dapm_context *dapm = snd_soc_component_get_dapm(tfa98xx->codec);
#else
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(tfa98xx->codec);
#endif
	struct snd_soc_dapm_widget *widgets;
	unsigned int num_dapm_widgets = ARRAY_SIZE(tfa98xx_dapm_widgets_common);

	widgets = devm_kzalloc(&tfa98xx->i2c->dev,
		sizeof(struct snd_soc_dapm_widget) *
		ARRAY_SIZE(tfa98xx_dapm_widgets_common),
		GFP_KERNEL);
	if (!widgets)
		return;
	memcpy(widgets, tfa98xx_dapm_widgets_common,
		sizeof(struct snd_soc_dapm_widget) *
		ARRAY_SIZE(tfa98xx_dapm_widgets_common));

	tfa98xx_append_i2c_address(&tfa98xx->i2c->dev,
		tfa98xx->i2c,
		widgets,
		num_dapm_widgets,
		NULL,
		0);

	snd_soc_dapm_new_controls(dapm, widgets,
		ARRAY_SIZE(tfa98xx_dapm_widgets_common));
	snd_soc_dapm_add_routes(dapm, tfa98xx_dapm_routes_common,
		ARRAY_SIZE(tfa98xx_dapm_routes_common));

	if (tfa98xx->flags & TFA98XX_FLAG_STEREO_DEVICE) {
		snd_soc_dapm_new_controls(dapm, tfa98xx_dapm_widgets_stereo,
			ARRAY_SIZE(tfa98xx_dapm_widgets_stereo));
		snd_soc_dapm_add_routes(dapm, tfa98xx_dapm_routes_stereo,
			ARRAY_SIZE(tfa98xx_dapm_routes_stereo));
	}

	if (tfa98xx->flags & TFA98XX_FLAG_MULTI_MIC_INPUTS) {
		snd_soc_dapm_new_controls(dapm, tfa9888_dapm_inputs,
			ARRAY_SIZE(tfa9888_dapm_inputs));
		snd_soc_dapm_add_routes(dapm, tfa9888_input_dapm_routes,
			ARRAY_SIZE(tfa9888_input_dapm_routes));
	}

	if (tfa98xx->flags & TFA98XX_FLAG_SAAM_AVAILABLE) {
		snd_soc_dapm_new_controls(dapm, tfa98xx_dapm_widgets_saam,
			ARRAY_SIZE(tfa98xx_dapm_widgets_saam));
		snd_soc_dapm_add_routes(dapm, tfa98xx_dapm_routes_saam,
			ARRAY_SIZE(tfa98xx_dapm_routes_saam));
	}
}

/* I2C wrapper functions */
enum Tfa98xx_Error tfa98xx_write_register16(struct tfa_device *tfa,
	unsigned char subaddress,
	unsigned short value)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	struct tfa98xx *tfa98xx;
	int ret;
	int retries = I2C_RETRIES;

	if (tfa == NULL) {
		pr_err("No device available\n");
		return Tfa98xx_Error_Fail;
	}

	tfa98xx = (struct tfa98xx *)tfa->data;
	if (!tfa98xx || !tfa98xx->regmap) {
		pr_err("No tfa98xx regmap available\n");
		return Tfa98xx_Error_Bad_Parameter;
	}
retry:
	ret = regmap_write(tfa98xx->regmap, subaddress, value);
	if (ret < 0) {
		pr_warn("i2c error, retries left: %d\n", retries);
		if (retries) {
			retries--;
			msleep(I2C_RETRY_DELAY);
			goto retry;
		}
		return Tfa98xx_Error_Fail;
	}
	if (tfa98xx_kmsg_regs)
		dev_dbg(&tfa98xx->i2c->dev, "  WR reg=0x%02x, val=0x%04x %s\n",
			subaddress, value,
			ret < 0 ? "Error!!" : "");

	if (tfa98xx_ftrace_regs)
		tfa98xx_trace_printk("\tWR     reg=0x%02x, val=0x%04x %s\n",
			subaddress, value,
			ret < 0 ? "Error!!" : "");
	return error;
}

enum Tfa98xx_Error tfa98xx_read_register16(struct tfa_device *tfa,
	unsigned char subaddress,
	unsigned short *val)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	struct tfa98xx *tfa98xx;
	unsigned int value;
	int retries = I2C_RETRIES;
	int ret;

	if (tfa == NULL) {
		pr_err("No device available\n");
		return Tfa98xx_Error_Fail;
	}

	tfa98xx = (struct tfa98xx *)tfa->data;
	if (!tfa98xx || !tfa98xx->regmap) {
		pr_err("No tfa98xx regmap available\n");
		return Tfa98xx_Error_Bad_Parameter;
	}
retry:
	ret = regmap_read(tfa98xx->regmap, subaddress, &value);
	if (ret < 0) {
		pr_warn("i2c error at subaddress 0x%x, retries left: %d\n", subaddress, retries);
		if (retries) {
			retries--;
			msleep(I2C_RETRY_DELAY);
			goto retry;
		}
		return Tfa98xx_Error_Fail;
	}
	*val = value & 0xffff;

	if (tfa98xx_kmsg_regs)
		dev_dbg(&tfa98xx->i2c->dev, "RD   reg=0x%02x, val=0x%04x %s\n",
			subaddress, *val,
			ret < 0 ? "Error!!" : "");
	if (tfa98xx_ftrace_regs)
		tfa98xx_trace_printk("\tRD     reg=0x%02x, val=0x%04x %s\n",
			subaddress, *val,
			ret < 0 ? "Error!!" : "");

	return error;
}


/*
 * init external dsp
 */
enum Tfa98xx_Error
	tfa98xx_init_dsp(struct tfa_device *tfa)
{
	return Tfa98xx_Error_Not_Supported;
}

int tfa98xx_get_dsp_status(struct tfa_device *tfa)
{
	return 0;
}

/*
 * write external dsp message
 */
enum Tfa98xx_Error
	tfa98xx_write_dsp(struct tfa_device *tfa, int num_bytes, const char *command_buffer)
{
	return Tfa98xx_Error_Not_Supported;
}

/*
 * read external dsp message
 */
enum Tfa98xx_Error
	tfa98xx_read_dsp(struct tfa_device *tfa, int num_bytes, unsigned char *result_buffer)
{
	return Tfa98xx_Error_Not_Supported;
}
/*
 * write/read external dsp message
 */
enum Tfa98xx_Error
	tfa98xx_writeread_dsp(struct tfa_device *tfa, int command_length, void *command_buffer,
		int result_length, void *result_buffer)
{
	return Tfa98xx_Error_Not_Supported;
}

enum Tfa98xx_Error tfa98xx_read_data(struct tfa_device *tfa,
	unsigned char reg,
	int len, unsigned char value[])
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	struct tfa98xx *tfa98xx;
	struct i2c_client *tfa98xx_client;
	int err;
	int tries = 0;
	unsigned char *reg_buf = NULL;
	struct i2c_msg msgs[] = {
		{
			.flags = 0,
			.len = 1,
			.buf = NULL,
		}, {
			.flags = I2C_M_RD,
			.len = len,
			.buf = value,
		},
	};
	reg_buf = (unsigned char *)kmalloc(sizeof(reg), GFP_DMA);     //GRP_KERNEL  also works,
	if (!reg_buf) {
		return -ENOMEM;;
	}

	*reg_buf = reg;
	msgs[0].buf = reg_buf;

	if (tfa == NULL) {
		pr_err("No device available\n");
		return Tfa98xx_Error_Fail;
	}

	tfa98xx = (struct tfa98xx *)tfa->data;
	if (tfa98xx->i2c) {
		tfa98xx_client = tfa98xx->i2c;
		msgs[0].addr = tfa98xx_client->addr;
		msgs[1].addr = tfa98xx_client->addr;

		do {
			err = i2c_transfer(tfa98xx_client->adapter, msgs,
				ARRAY_SIZE(msgs));
			if (err != ARRAY_SIZE(msgs))
				msleep_interruptible(I2C_RETRY_DELAY);
		} while ((err != ARRAY_SIZE(msgs)) && (++tries < I2C_RETRIES));

		if (err != ARRAY_SIZE(msgs)) {
			dev_err(&tfa98xx_client->dev, "read transfer error %d\n",
				err);
			error = Tfa98xx_Error_Fail;
		}

		if (tfa98xx_kmsg_regs)
			dev_dbg(&tfa98xx_client->dev, "RD-DAT reg=0x%02x, len=%d\n",
				reg, len);
		if (tfa98xx_ftrace_regs)
			tfa98xx_trace_printk("\t\tRD-DAT reg=0x%02x, len=%d\n",
				reg, len);
	}
	else {
		pr_err("No device available\n");
		error = Tfa98xx_Error_Fail;
	}
	kfree(reg_buf);
	return error;
}

enum Tfa98xx_Error tfa98xx_write_raw(struct tfa_device *tfa,
	int len,
	const unsigned char data[])
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	struct tfa98xx *tfa98xx;
	int ret;
	int retries = I2C_RETRIES;


	if (tfa == NULL) {
		pr_err("No device available\n");
		return Tfa98xx_Error_Fail;
	}

	tfa98xx = (struct tfa98xx *)tfa->data;

retry:
	ret = i2c_master_send(tfa98xx->i2c, data, len);
	if (ret < 0) {
		pr_warn("i2c error, retries left: %d\n", retries);
		if (retries) {
			retries--;
			msleep(I2C_RETRY_DELAY);
			goto retry;
		}
	}

	if (ret == len) {
		if (tfa98xx_kmsg_regs)
			dev_dbg(&tfa98xx->i2c->dev, "  WR-RAW len=%d\n", len);
		if (tfa98xx_ftrace_regs)
			tfa98xx_trace_printk("\t\tWR-RAW len=%d\n", len);
		return Tfa98xx_Error_Ok;
	}
	pr_err("  WR-RAW (len=%d) Error I2C send size mismatch %d\n", len, ret);
	error = Tfa98xx_Error_Fail;

	return error;
}

/* Interrupts management */

static void tfa98xx_interrupt_enable_tfa2(struct tfa98xx *tfa98xx, bool enable)
{
	/* Only for 0x72 we need to enable NOCLK interrupts */
	if (tfa98xx->flags & TFA98XX_FLAG_REMOVE_PLOP_NOISE)
		tfa_irq_ena(tfa98xx->tfa, tfa9912_irq_stnoclk, enable);

	if (tfa98xx->flags & TFA98XX_FLAG_LP_MODES) {
		tfa_irq_ena(tfa98xx->tfa, 36, enable); /* FIXME: IELP0 does not excist for 9912 */
		tfa_irq_ena(tfa98xx->tfa, tfa9912_irq_stclpr, enable);
	}

	#if 0
	if ((tfa98xx->rev == 0x73) || (tfa98xx->rev == 0x74)) {
		tfa987x_irq_clear(tfa98xx->tfa, tfa9874_irq_all);
		//tfa987x_irq_enable(tfa98xx->tfa, tfa9874_irq_stvdds, enable);	/* enable POR */
		//tfa987x_irq_enable(tfa98xx->tfa, tfa9874_irq_stocpr, enable);	/* enable OCP */
		//tfa987x_irq_enable(tfa98xx->tfa, tfa9874_irq_stnoclk, enable);	/* enable NOCLK */
	}
	#endif
}

/* Check if tap-detection can and shall be enabled.
 * Configure SPK interrupt accordingly or setup polling mode
 * Tap-detection shall be active if:
 *  - the service is enabled (tapdet_open), AND
 *  - the current profile is a tap-detection profile
 * On TFA1 familiy of devices, activating tap-detection means enabling the SPK
 * interrupt if available.
 * We also update the tapdet_enabled and tapdet_poll variables.
 */
static void tfa98xx_tapdet_check_update(struct tfa98xx *tfa98xx)
{
	unsigned int enable = false;

	/* Support tap-detection on TFA1 family of devices */
	if ((tfa98xx->flags & TFA98XX_FLAG_TAPDET_AVAILABLE) == 0)
		return;

	if (tfa98xx->tapdet_open &&
		(tfa98xx->tapdet_profiles & (1 << tfa98xx->profile)))
		enable = true;

	if (!gpio_is_valid(tfa98xx->irq_gpio)) {
		/* interrupt not available, setup polling mode */
		tfa98xx->tapdet_poll = true;
		if (enable)
			queue_delayed_work(tfa98xx->tfa98xx_wq,
				&tfa98xx->tapdet_work, HZ / 10);
		else
			cancel_delayed_work_sync(&tfa98xx->tapdet_work);
		dev_dbg(tfa98xx->codec->dev,
			"Polling for tap-detection: %s (%d; 0x%x, %d)\n",
			enable ? "enabled" : "disabled",
			tfa98xx->tapdet_open, tfa98xx->tapdet_profiles,
			tfa98xx->profile);

	}
	else {
		dev_dbg(tfa98xx->codec->dev,
			"Interrupt for tap-detection: %s (%d; 0x%x, %d)\n",
			enable ? "enabled" : "disabled",
			tfa98xx->tapdet_open, tfa98xx->tapdet_profiles,
			tfa98xx->profile);
		/*  enabled interrupt */
		tfa_irq_ena(tfa98xx->tfa, tfa9912_irq_sttapdet, enable);
	}

	/* check disabled => enabled transition to clear pending events */
	if (!tfa98xx->tapdet_enabled && enable) {
		/* clear pending event if any */
		tfa_irq_clear(tfa98xx->tfa, tfa9912_irq_sttapdet);
	}

	if (!tfa98xx->tapdet_poll)
		tfa_irq_ena(tfa98xx->tfa, tfa9912_irq_sttapdet, 1); /* enable again */
}

/* global enable / disable interrupts */
static void tfa98xx_interrupt_enable(struct tfa98xx *tfa98xx, bool enable)
{
	if (tfa98xx->flags & TFA98XX_FLAG_SKIP_INTERRUPTS)
		return;

	if (tfa98xx->tfa->tfa_family == 2)
		tfa98xx_interrupt_enable_tfa2(tfa98xx, enable);
}

/* Firmware management */
static void tfa98xx_container_loaded(const struct firmware *cont, void *context)
{
	TfaContainer_t *container;
	struct tfa98xx *tfa98xx = context;
	enum tfa_error tfa_err;
	int container_size;
	int ret;

	tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_FAIL;

	printk(KERN_ERR "%s-1\n",__func__);//

	if (!cont) {
		pr_err("Failed to read %s\n", fw_name);
		return;
	}

	pr_info("loaded %s - size: %zu\n", fw_name, cont->size);

	if (tfa98xx_container == NULL) {
		container = kzalloc(cont->size, GFP_KERNEL);
		if (container == NULL) {
			pr_err("Error allocating memory\n");
			return;
		}

		container_size = cont->size;
		memcpy(container, cont->data, container_size);

		pr_info("%.2s%.2s\n", container->version, container->subversion);
		pr_info("%.8s\n", container->customer);
		pr_info("%.8s\n", container->application);
		pr_info("%.8s\n", container->type);
		pr_info("%d ndev\n", container->ndev);
		pr_info("%d nprof\n", container->nprof);

		tfa_err = tfa_load_cnt(container, container_size);
		if (tfa_err != tfa_error_ok) {
			kfree(container);
			dev_err(tfa98xx->dev, "Cannot load container file, aborting\n");
			return;
		}

		tfa98xx_container = container;
	}
	else {
		pr_debug("container file already loaded...\n");
		container = tfa98xx_container;
	}

	tfa98xx->tfa->cnt = container;

	/*
		i2c transaction limited to 64k
		(Documentation/i2c/writing-clients)
	*/
	tfa98xx->tfa->buffer_size = 65536;

	if (tfa_dev_probe(tfa98xx->i2c->addr, tfa98xx->tfa) != 0) {
		dev_err(tfa98xx->dev, "Failed to probe TFA98xx @ 0x%.2x\n", tfa98xx->i2c->addr);
		return;
	}

	tfa98xx->tfa->dev_idx = tfa_cont_get_idx(tfa98xx->tfa);
	if (tfa98xx->tfa->dev_idx < 0) {
		dev_err(tfa98xx->dev, "Failed to find TFA98xx @ 0x%.2x in container file\n", tfa98xx->i2c->addr);
		return;
	}

	/* Enable debug traces */
	tfa98xx->tfa->verbose = trace_level & 1;

	/* prefix is the application name from the cnt */
	tfa_cnt_get_app_name(tfa98xx->tfa, tfa98xx->fw.name);

	/* set default profile/vstep */
	tfa98xx->profile = 0;
	tfa98xx->vstep = 0;

	/* Override default profile if requested */
	if (strcmp(dflt_prof_name, "")) {
		unsigned int i;
		int nprof = tfa_cnt_get_dev_nprof(tfa98xx->tfa);
		for (i = 0; i < nprof; i++) {
			if (strcmp(tfa_cont_profile_name(tfa98xx, i),
				dflt_prof_name) == 0) {
				tfa98xx->profile = i;
				dev_info(tfa98xx->dev,
					"changing default profile to %s (%d)\n",
					dflt_prof_name, tfa98xx->profile);
				break;
			}
		}
		if (i >= nprof)
			dev_info(tfa98xx->dev,
				"Default profile override failed (%s profile not found)\n",
				dflt_prof_name);
	}

	tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_OK;
	pr_debug("Firmware init complete\n");

	if (no_start != 0)
		return;

	/* Only controls for master device */
	if (tfa98xx->tfa->dev_idx == 0)
		tfa98xx_create_controls(tfa98xx);

	tfa98xx_inputdev_check_register(tfa98xx);

	if (tfa_is_cold(tfa98xx->tfa) == 0) {
		pr_debug("Warning: device 0x%.2x is still warm\n", tfa98xx->i2c->addr);
		tfa_reset(tfa98xx->tfa);
	}

	if (tfa98xx->flags & TFA98XX_FLAG_TDM_DEVICE) {
		return;
	}

	/* Preload settings using internal clock on TFA2 */
	if ((tfa98xx->tfa->tfa_family == 2) && (0 == tfa98xx->tfa->is_probus_device)) {
		mutex_lock(&tfa98xx->dsp_lock);
		pr_info("will be using internal clock to preload MAX2 TFA settings.\n");
		ret = tfa98xx_tfa_start(tfa98xx, tfa98xx->profile, tfa98xx->vstep);
		if (ret == Tfa98xx_Error_Not_Supported)
			tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_FAIL;

		/* we should be power-down device when parameter is loaded. */
		tfa_dev_stop(tfa98xx->tfa);
		ret = tfa98xx->dsp_init = TFA98XX_DSP_INIT_STOPPED;

		mutex_unlock(&tfa98xx->dsp_lock);
	}
	tfa98xx_interrupt_enable(tfa98xx, true);
}

static int tfa98xx_load_container(struct tfa98xx *tfa98xx)
{
	const struct firmware *firmware;
	int rc = 0;

	tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_PENDING;
	mutex_lock(&tfa98xx_mutex);
	rc = request_firmware(&firmware, fw_name, tfa98xx->dev);
	if (rc <0) {
		mutex_unlock(&tfa98xx_mutex);
		return rc;
	}
	tfa98xx_container_loaded(firmware, tfa98xx);
	release_firmware(firmware);
	mutex_unlock(&tfa98xx_mutex);

	return rc;
}


static void tfa98xx_tapdet(struct tfa98xx *tfa98xx)
{
	unsigned int tap_pattern;
	int btn;

	/* check tap pattern (BTN_0 is "error" wrong tap indication */
	tap_pattern = tfa_get_tap_pattern(tfa98xx->tfa);
	switch (tap_pattern) {
	case 0xffffffff:
		pr_info("More than 4 taps detected! (flagTapPattern = -1)\n");
		btn = BTN_0;
		break;
	case 0xfffffffe:
	case 0xfe:
		pr_info("Illegal tap detected!\n");
		btn = BTN_0;
		break;
	case 0:
		pr_info("Unrecognized pattern! (flagTapPattern = 0)\n");
		btn = BTN_0;
		break;
	default:
		pr_info("Detected pattern: %d\n", tap_pattern);
		btn = BTN_0 + tap_pattern;
		break;
	}

	input_report_key(tfa98xx->input, btn, 1);
	input_report_key(tfa98xx->input, btn, 0);
	input_sync(tfa98xx->input);

	/* acknowledge event done by clearing interrupt */

}

static void tfa98xx_tapdet_work(struct work_struct *work)
{
	struct tfa98xx *tfa98xx;

	//TODO check is this is still needed for tap polling
	tfa98xx = container_of(work, struct tfa98xx, tapdet_work.work);

	if (tfa_irq_get(tfa98xx->tfa, tfa9912_irq_sttapdet))
		tfa98xx_tapdet(tfa98xx);

	queue_delayed_work(tfa98xx->tfa98xx_wq, &tfa98xx->tapdet_work, HZ / 10);
}
static void tfa98xx_nmode_update_work(struct work_struct *work)
{
/* the AP will be wake up by workQ, we should be disabled it to save power. */
/* you can enable it for debug purpose. */
#if 0
	struct tfa98xx *tfa98xx;

	//MCH_TO_TEST, checking if noise mode update is required or not
	tfa98xx = container_of(work, struct tfa98xx, nmodeupdate_work.work);
	mutex_lock(&tfa98xx->dsp_lock);
	tfa_adapt_noisemode(tfa98xx->tfa);
	mutex_unlock(&tfa98xx->dsp_lock);
	queue_delayed_work(tfa98xx->tfa98xx_wq, &tfa98xx->nmodeupdate_work,5 * HZ);
#endif
}
static void tfa98xx_monitor(struct work_struct *work)
{
/* the AP will be wake up by workQ, we should be disabled it to save power. */
/* you can enable it for debug purpose. */
#if 0
	struct tfa98xx *tfa98xx;
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;

	tfa98xx = container_of(work, struct tfa98xx, monitor_work.work);

	/* Check for tap-detection - bypass monitor if it is active */
	if (!tfa98xx->input) {
		mutex_lock(&tfa98xx->dsp_lock);
		error = tfa_status(tfa98xx->tfa);
		mutex_unlock(&tfa98xx->dsp_lock);
		if (error == Tfa98xx_Error_DSP_not_running) {
			if (tfa98xx->dsp_init == TFA98XX_DSP_INIT_DONE) {
				tfa98xx->dsp_init = TFA98XX_DSP_INIT_RECOVER;
				queue_delayed_work(tfa98xx->tfa98xx_wq, &tfa98xx->init_work, 0);
			}
		}
	}

	/* reschedule */
	queue_delayed_work(tfa98xx->tfa98xx_wq, &tfa98xx->monitor_work, 5 * HZ);
#endif
}

static void tfa98xx_dsp_init(struct tfa98xx *tfa98xx)
{
	int ret;
	bool failed = false;
	bool reschedule = false;
	bool sync = false;

	if (tfa98xx->dsp_fw_state != TFA98XX_DSP_FW_OK) {
		pr_debug("Skipping tfa_dev_start (no FW: %d)\n", tfa98xx->dsp_fw_state);
		return;
	}

	if (tfa98xx->dsp_init == TFA98XX_DSP_INIT_DONE) {
		pr_debug("Stream already started, skipping DSP power-on\n");
		return;
	}

	mutex_lock(&tfa98xx->dsp_lock);

	tfa98xx->dsp_init = TFA98XX_DSP_INIT_PENDING;

	pr_debug("[goodix] %s  init_count=%d\n", __func__, tfa98xx->init_count);
	if (tfa98xx->init_count < TF98XX_MAX_DSP_START_TRY_COUNT) {
		/* directly try to start DSP */
		ret = tfa98xx_tfa_start(tfa98xx, tfa98xx->profile, tfa98xx->vstep);
		if (ret == Tfa98xx_Error_Not_Supported) {
			tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_FAIL;
			dev_err(&tfa98xx->i2c->dev, "Failed starting device\n");
			failed = true;
		}
		else if (ret != Tfa98xx_Error_Ok) {
			/* It may fail as we may not have a valid clock at that
			 * time, so re-schedule and re-try later.
			 */
			dev_err(&tfa98xx->i2c->dev,
				"tfa_dev_start failed! (err %d) - %d\n",
				ret, tfa98xx->init_count);
			reschedule = true;
		}
		else {
			sync = true;

			/* Subsystem ready, tfa init complete */
			tfa98xx->dsp_init = TFA98XX_DSP_INIT_DONE;
			dev_dbg(&tfa98xx->i2c->dev,
				"tfa_dev_start success (%d)\n",
				tfa98xx->init_count);
			/* cancel other pending init works */
			cancel_delayed_work(&tfa98xx->init_work);
			tfa98xx->init_count = 0;
		}
	}
	else {
		/* exceeded max number ot start tentatives, cancel start */
		dev_err(&tfa98xx->i2c->dev,
			"Failed starting device (%d)\n",
			tfa98xx->init_count);
		failed = true;
	}
	if (reschedule) {
		/* reschedule this init work for later */
		queue_delayed_work(tfa98xx->tfa98xx_wq,
			&tfa98xx->init_work,
			msecs_to_jiffies(5));
		tfa98xx->init_count++;
	}
	if (failed) {
		tfa98xx->dsp_init = TFA98XX_DSP_INIT_FAIL;
		/* cancel other pending init works */
		cancel_delayed_work(&tfa98xx->init_work);
		tfa98xx->init_count = 0;
	}
	mutex_unlock(&tfa98xx->dsp_lock);

	if (sync) {
		/* check if all devices have started */
		bool do_sync;
		mutex_lock(&tfa98xx_mutex);

		if (tfa98xx_sync_count < tfa98xx_device_count)
			tfa98xx_sync_count++;

		do_sync = (tfa98xx_sync_count >= tfa98xx_device_count);
		mutex_unlock(&tfa98xx_mutex);

		/* when all devices have started then unmute */
		if (do_sync) {
			tfa98xx_sync_count = 0;
			list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
				mutex_lock(&tfa98xx->dsp_lock);
				tfa_dev_set_state(tfa98xx->tfa, TFA_STATE_UNMUTE, 0);

				/*
				 * start monitor thread to check IC status bit
				 * periodically, and re-init IC to recover if
				 * needed.
				 */
				if (tfa98xx->tfa->tfa_family == 1)
					queue_delayed_work(tfa98xx->tfa98xx_wq,
						&tfa98xx->monitor_work,
						1 * HZ);
				mutex_unlock(&tfa98xx->dsp_lock);
			}

		}
	}


	return;
}


static void tfa98xx_dsp_init_work(struct work_struct *work)
{
	struct tfa98xx *tfa98xx = container_of(work, struct tfa98xx, init_work.work);

	tfa98xx_dsp_init(tfa98xx);
}

static void tfa98xx_interrupt(struct work_struct *work)
{
	struct tfa98xx *tfa98xx = container_of(work, struct tfa98xx, interrupt_work.work);
	if ((tfa98xx->rev == 0x73) || (tfa98xx->rev == 0x74)) {
		mutex_lock(&tfa98xx->dsp_lock);
		tfa987x_irq_handle(tfa98xx->tfa);
		tfa987x_irq_clear(tfa98xx->tfa, tfa9874_irq_all); /* clear interrupt */
		tfa987x_irq_unmask(tfa98xx->tfa);	/* restore interrupt */
		mutex_unlock(&tfa98xx->dsp_lock);
		return;
	}

	if (tfa98xx->flags & TFA98XX_FLAG_TAPDET_AVAILABLE) {
		/* check for tap interrupt */
		if (tfa_irq_get(tfa98xx->tfa, tfa9912_irq_sttapdet)) {
			tfa98xx_tapdet(tfa98xx);

			/* clear interrupt */
			tfa_irq_clear(tfa98xx->tfa, tfa9912_irq_sttapdet);
		}
	} /* TFA98XX_FLAG_TAPDET_AVAILABLE */

	if (tfa98xx->flags & TFA98XX_FLAG_REMOVE_PLOP_NOISE) {
		int start_triggered;

		mutex_lock(&tfa98xx->dsp_lock);
		start_triggered = tfa_plop_noise_interrupt(tfa98xx->tfa, tfa98xx->profile, tfa98xx->vstep);
		/* Only enable when the return value is 1, otherwise the interrupt is triggered twice */
		if (start_triggered)
			tfa98xx_interrupt_enable(tfa98xx, true);
		mutex_unlock(&tfa98xx->dsp_lock);
	} /* TFA98XX_FLAG_REMOVE_PLOP_NOISE */

	if (tfa98xx->flags & TFA98XX_FLAG_LP_MODES) {
		tfa_lp_mode_interrupt(tfa98xx->tfa);
	} /* TFA98XX_FLAG_LP_MODES */

	/* unmask interrupts masked in IRQ handler */
	tfa_irq_unmask(tfa98xx->tfa);
}

static int tfa98xx_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
	struct snd_soc_component *codec = dai->component;
	struct tfa98xx *tfa98xx = snd_soc_component_get_drvdata(codec);
#else
	struct snd_soc_codec *codec = dai->codec;
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
#endif
	unsigned int sr;
	int len, prof, nprof, idx = 0;
	char *basename;

	/*
	 * Support CODEC to CODEC links,
	 * these are called with a NULL runtime pointer.
	 */
	if (!substream->runtime)
		return 0;

	if (pcm_no_constraint != 0)
		return 0;

	if (no_start != 0)
		return 0;

	if (tfa98xx->dsp_fw_state != TFA98XX_DSP_FW_OK) {
		dev_info(codec->dev, "Container file not loaded\n");
		return -EINVAL;
	}

	basename = kzalloc(MAX_CONTROL_NAME, GFP_KERNEL);
	if (!basename)
		return -ENOMEM;

	/* copy profile name into basename until the . */
	get_profile_basename(basename, tfa_cont_profile_name(tfa98xx, tfa98xx->profile));
	len = strlen(basename);

	/* loop over all profiles and get the supported samples rate(s) from
	 * the profiles with the same basename
	 */
	nprof = tfa_cnt_get_dev_nprof(tfa98xx->tfa);
	tfa98xx->rate_constraint.list = &tfa98xx->rate_constraint_list[0];
	tfa98xx->rate_constraint.count = 0;
	for (prof = 0; prof < nprof; prof++) {
		if (0 == strncmp(basename, tfa_cont_profile_name(tfa98xx, prof), len)) {
			/* Check which sample rate is supported with current profile,
			 * and enforce this.
			 */
			sr = tfa98xx_get_profile_sr(tfa98xx->tfa, prof);
			if (!sr)
				dev_info(codec->dev, "Unable to identify supported sample rate\n");

			if (tfa98xx->rate_constraint.count >= TFA98XX_NUM_RATES) {
				dev_err(codec->dev, "too many sample rates\n");
			}
			else {
				tfa98xx->rate_constraint_list[idx++] = sr;
				tfa98xx->rate_constraint.count += 1;
			}
		}
	}

	kfree(basename);

/* as QUALCOMM FAE suggested, we don't need to calling 'snd_pcm_hw_constraint_list' on QUALCOMM platform. */
	return 0;
}

static int tfa98xx_set_dai_sysclk(struct snd_soc_dai *codec_dai,
	int clk_id, unsigned int freq, int dir)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
	struct tfa98xx *tfa98xx = snd_soc_component_get_drvdata(codec_dai->component);
#else
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec_dai->codec);
#endif
	tfa98xx->sysclk = freq;
	return 0;
}

static int tfa98xx_set_tdm_slot(struct snd_soc_dai *dai, unsigned int tx_mask,
	unsigned int rx_mask, int slots, int slot_width)
{
	pr_debug("\n");
	return 0;
}

static int tfa98xx_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
	struct tfa98xx *tfa98xx = snd_soc_component_get_drvdata(dai->component);
	struct snd_soc_component *codec = dai->component;
#else
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(dai->codec);
	struct snd_soc_codec *codec = dai->codec;
#endif
	pr_debug("fmt=0x%x\n", fmt);

	/* Supported mode: regular I2S, slave, or PDM */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		if ((fmt & SND_SOC_DAIFMT_MASTER_MASK) != SND_SOC_DAIFMT_CBS_CFS) {
			dev_err(codec->dev, "Invalid Codec master mode\n");
			return -EINVAL;
		}
		break;
	case SND_SOC_DAIFMT_PDM:
		break;
	default:
		dev_err(codec->dev, "Unsupported DAI format %d\n",
			fmt & SND_SOC_DAIFMT_FORMAT_MASK);
		return -EINVAL;
	}

	tfa98xx->audio_mode = fmt & SND_SOC_DAIFMT_FORMAT_MASK;

	return 0;
}

static int tfa98xx_get_fssel(unsigned int rate)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(rate_to_fssel); i++) {
		if (rate_to_fssel[i].rate == rate) {
			return rate_to_fssel[i].fssel;
		}
	}
	return -EINVAL;
}

static int tfa98xx_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
	struct snd_soc_component *codec = dai->component;
	struct tfa98xx *tfa98xx = snd_soc_component_get_drvdata(codec);
#else
	struct snd_soc_codec *codec = dai->codec;
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
#endif
	unsigned int rate;
	int prof_idx;

	printk(KERN_ERR "%s-1\n",__func__);//

	/* Supported */
	rate = params_rate(params);
    tfa98xx->tfa->bitwidth = params_width(params);
    tfa98xx->tfa->dynamicTDMmode = pcm_sample_format;
	pr_debug("Requested rate: %d, sample size: %d, physical size: %d\n",
		rate, snd_pcm_format_width(params_format(params)),
		snd_pcm_format_physical_width(params_format(params)));

	if (no_start != 0)
		return 0;
	/* set TDM bit width */
	pr_debug("%s: Requested width: %d\n", __func__,
			params_width(params));
	if ((tfa98xx->tfa->dynamicTDMmode == 3) && tfa_dev_set_tdm_bitwidth(tfa98xx->tfa,tfa98xx->tfa->bitwidth))
		return -EINVAL;
	/* check if samplerate is supported for this mixer profile */
	prof_idx = get_profile_id_for_sr(tfa98xx_mixer_profile, rate);
	if (prof_idx < 0) {
		pr_err("tfa98xx: invalid sample rate %d.\n", rate);
		return -EINVAL;
	}
	pr_debug("mixer profile:container profile = [%d:%d]\n", tfa98xx_mixer_profile, prof_idx);


	/* update 'real' profile (container profile) */
	tfa98xx->profile = prof_idx;

	/* update to new rate */
	tfa98xx->rate = rate;

	return 0;
}

#ifdef TFA_NON_DSP_SOLUTION
static uint8_t bytes[3*3+1] = {0};
enum Tfa98xx_Error tfa98xx_adsp_send_calib_values(void)
{
	struct tfa98xx *tfa98xx;
	int ret = 0;
	int value = 0, dsp_cal_value = 0;

	/* if the calibration value was sent to host DSP, we clear flag only (stereo case). */
	if ((tfa98xx_device_count > 1) && (tfa98xx_device_count == bytes[0])) {
		pr_info("The calibration value was sent to host DSP.\n");
		bytes[0] = 0;
		return Tfa98xx_Error_Ok;
	}

	/* read calibrated impendance from all devices. */
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		struct tfa_device *tfa = tfa98xx->tfa;

		if (TFA_GET_BF(tfa, MTPEX) == 1) {
			value = tfa_dev_mtp_get(tfa, TFA_MTP_RE25);
			dsp_cal_value = (value * 65536) / 1000;
			pr_info("Device 0x%x impendance:%d, cal value is 0x%x\n", tfa98xx->i2c->addr, value, dsp_cal_value);

			if (2 == tfa98xx_device_count) {
				/*stereo case*/
				/* we should makesure left device calibration value into primary channel,
							   and right device calibration value into secondary channel. */
				if (TFA_LEFT_DEVICE_ADDRESS == tfa98xx->i2c->addr) {
					bytes[4] = (uint8_t)((dsp_cal_value >> 16) & 0xff);
					bytes[5] = (uint8_t)((dsp_cal_value >> 8) & 0xff);
					bytes[6] = (uint8_t)(dsp_cal_value & 0xff);
				} else if (TFA_RIGHT_DEVICE_ADDRESS == tfa98xx->i2c->addr) {
					bytes[7] = (uint8_t)((dsp_cal_value >> 16) & 0xff);
					bytes[8] = (uint8_t)((dsp_cal_value >> 8) & 0xff);
					bytes[9] = (uint8_t)(dsp_cal_value & 0xff);
				}
			} else {
				/*mono case*/
				bytes[4] = (uint8_t)((dsp_cal_value >> 16) & 0xff);
				bytes[5] = (uint8_t)((dsp_cal_value >> 8) & 0xff);
				bytes[6] = (uint8_t)(dsp_cal_value & 0xff);

				memcpy(&bytes[7], &bytes[4], sizeof(char)*3);

			}

			bytes[0] += 1;
		}
	}

	pr_info("tfa98xx_device_count=%d  bytes[0]=%d\n", tfa98xx_device_count, bytes[0]);

	/* we will send it to host DSP algorithm once calibraion value loaded from all device. */
	if (tfa98xx_device_count == bytes[0]) {
		bytes[1] = 0x00;
		bytes[2] = 0x81;
		bytes[3] = 0x05;

		pr_info("calibration value send to host DSP.\n");
		ret = send_tfa_cal_in_band(&bytes[1], sizeof(bytes) - 1);
		msleep(10);

		/* for mono case, we should clear flag here. */
		if (1 == tfa98xx_device_count)
			bytes[0] = 0;

	} else {
		pr_err("load calibration data from device failed.\n");
		ret = Tfa98xx_Error_Bad_Parameter;
	}

	return ret;
}
#endif

static int tfa98xx_mute(struct snd_soc_dai *dai, int mute, int stream)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
	struct snd_soc_component *codec = dai->component;
	struct tfa98xx *tfa98xx = snd_soc_component_get_drvdata(codec);
#else
	struct snd_soc_codec *codec = dai->codec;
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
#endif
	dev_info(&tfa98xx->i2c->dev, "%s: state: %d\n", __func__, mute);

	printk(KERN_ERR "%s-1 mute:\n",__func__,mute);//

	if (no_start) {
		pr_debug("no_start parameter set no tfa_dev_start or tfa_dev_stop, returning\n");
		return 0;
	}
	if (TFA98XX_DEVICE_MUTE_ON == tfa98xx->tfa_mute_mode) {
		pr_debug("%s: if Mute mode is enalbed, we don't need to power-on device. \n", __func__);
		return 0;
	}
#ifdef TFA_NON_DSP_SOLUTION
		/* reset kcontrol flag once power down tfa device. */
		atomic_set(&g_algo_bypass, TFA_KCONTROL_VALUE_DISABLED);
		atomic_set(&g_algo_mute, TFA_KCONTROL_VALUE_DISABLED);
		atomic_set(&g_Tx_enable, TFA_KCONTROL_VALUE_ENABLED);
#endif

	if (mute) {
		/* stop DSP only when both playback and capture streams
		 * are deactivated
		 */
		if (stream == SNDRV_PCM_STREAM_PLAYBACK){
			tfa98xx->pstream = 0;
		}
		else
			tfa98xx->cstream = 0;
		if (tfa98xx->pstream != 0 || tfa98xx->cstream != 0)
			return 0;

		mutex_lock(&tfa98xx_mutex);
		tfa98xx_sync_count = 0;
		mutex_unlock(&tfa98xx_mutex);

		cancel_delayed_work_sync(&tfa98xx->monitor_work);

		cancel_delayed_work_sync(&tfa98xx->init_work);
		if (tfa98xx->dsp_fw_state != TFA98XX_DSP_FW_OK)
			return 0;
		mutex_lock(&tfa98xx->dsp_lock);
#ifdef TFA_NON_DSP_SOLUTION
		if (strcmp (tfa_cont_profile_name (tfa98xx, tfa98xx_mixer_profile), "handset") != 0
				&& !(strstr(tfaContProfileName(tfa98xx->tfa->cnt, tfa98xx->tfa->dev_idx, tfa98xx_mixer_profile), ".standby") != NULL)) {
			tfa98xx_send_mute_cmd(TFA_KCONTROL_VALUE_ENABLED);
			msleep(60);
		}
#endif
		tfa_dev_stop(tfa98xx->tfa);
#if defined(CONFIG_TARGET_PRODUCT_RENOIR) || defined(CONFIG_TARGET_PRODUCT_LISA)
		if (stream == SNDRV_PCM_STREAM_PLAYBACK){
			if(gpio_is_valid(tfa98xx->spk_sw_gpio)){
				gpio_direction_output(tfa98xx->spk_sw_gpio,0);
			}
		}
#endif
		tfa98xx->dsp_init = TFA98XX_DSP_INIT_STOPPED;
		mutex_unlock(&tfa98xx->dsp_lock);
        if(tfa98xx->flags & TFA98XX_FLAG_ADAPT_NOISE_MODE)
        	cancel_delayed_work_sync(&tfa98xx->nmodeupdate_work);
	}
	else {
		if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
			tfa98xx->pstream = 1;
#if defined(CONFIG_TARGET_PRODUCT_RENOIR) || defined(CONFIG_TARGET_PRODUCT_LISA)
			if(strcmp (tfa_cont_profile_name (tfa98xx, tfa98xx_mixer_profile), "handset")== 0){
				if(gpio_is_valid(tfa98xx->spk_sw_gpio)){
					gpio_direction_output(tfa98xx->spk_sw_gpio,1);
				}
			}
			else{
				if(gpio_is_valid(tfa98xx->spk_sw_gpio)){
					gpio_direction_output(tfa98xx->spk_sw_gpio,0);
				}
			}
#endif
#ifdef TFA_NON_DSP_SOLUTION
			if (tfa98xx->tfa->is_probus_device
					&& (strcmp (tfa_cont_profile_name (tfa98xx, tfa98xx_mixer_profile), "handset") != 0)
					&& !(strstr(tfaContProfileName(tfa98xx->tfa->cnt, tfa98xx->tfa->dev_idx, tfa98xx_mixer_profile), ".standby") != NULL)) {
				tfa98xx_adsp_send_calib_values();
			}
#endif
		} else {
			tfa98xx->cstream = 1;
		}
		/* Start DSP */
#if 0
		/* Start DSP with async mode.*/
		if (tfa98xx->dsp_init != TFA98XX_DSP_INIT_PENDING)
			queue_delayed_work(tfa98xx->tfa98xx_wq,
			                   &tfa98xx->init_work, 0);
#else
		/* Start DSP with sync mode.*/
		pr_debug("[goodix] %s dsp_init=%d\n", __func__, tfa98xx->dsp_init);
		if (tfa98xx->dsp_init != TFA98XX_DSP_INIT_PENDING)
			tfa98xx_dsp_init(tfa98xx);
#endif
	     if(tfa98xx->flags & TFA98XX_FLAG_ADAPT_NOISE_MODE)
		 	queue_delayed_work(tfa98xx->tfa98xx_wq,
						&tfa98xx->nmodeupdate_work,
						0);
	}

	return 0;
}

static const struct snd_soc_dai_ops tfa98xx_dai_ops = {
	.startup = tfa98xx_startup,
	.set_fmt = tfa98xx_set_fmt,
	.set_sysclk = tfa98xx_set_dai_sysclk,
	.set_tdm_slot = tfa98xx_set_tdm_slot,
	.hw_params = tfa98xx_hw_params,
	.mute_stream = tfa98xx_mute,
};

static struct snd_soc_dai_driver tfa98xx_dai[] = {
	{
		.name = "tfa98xx-aif",
		.id = 1,
		.playback = {
			.stream_name = "AIF Playback",
			.channels_min = 1,
			.channels_max = 4,
			.rates = TFA98XX_RATES,
			.formats = TFA98XX_FORMATS,
		},
		.capture = {
			 .stream_name = "AIF Capture",
			 .channels_min = 1,
			 .channels_max = 4,
			 .rates = TFA98XX_RATES,
			 .formats = TFA98XX_FORMATS,
		 },
		.ops = &tfa98xx_dai_ops,
/*		.symmetric_rates = 1,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
		.symmetric_channels = 1,
		.symmetric_samplebits = 1,
#endif
*/
	},
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
static int tfa98xx_probe(struct snd_soc_component *codec)
{
	struct tfa98xx *tfa98xx = snd_soc_component_get_drvdata(codec);
	struct snd_soc_dapm_context *dapm = snd_soc_component_get_dapm(codec);
#else
static int tfa98xx_probe(struct snd_soc_codec *codec)
{
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);
#endif
	int ret;

	printk(KERN_ERR "tfa98xx_probe enter\n");///

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
	snd_soc_component_init_regmap(codec, tfa98xx->regmap);
#endif
	/* setup work queue, will be used to initial DSP on first boot up */
	tfa98xx->tfa98xx_wq = create_singlethread_workqueue("tfa98xx");
	if (!tfa98xx->tfa98xx_wq)
		return -ENOMEM;

	INIT_DELAYED_WORK(&tfa98xx->init_work, tfa98xx_dsp_init_work);
	INIT_DELAYED_WORK(&tfa98xx->monitor_work, tfa98xx_monitor);
	INIT_DELAYED_WORK(&tfa98xx->interrupt_work, tfa98xx_interrupt);
	INIT_DELAYED_WORK(&tfa98xx->tapdet_work, tfa98xx_tapdet_work);
	INIT_DELAYED_WORK(&tfa98xx->nmodeupdate_work, tfa98xx_nmode_update_work);

	tfa98xx->codec = codec;

	ret = tfa98xx_load_container(tfa98xx);
	printk(KERN_ERR "Container loading requested: %d\n", ret);//

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,16,0)
	codec->control_data = tfa98xx->regmap;
	ret = snd_soc_codec_set_cache_io(codec, 8, 16, SND_SOC_REGMAP);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}
#endif
	tfa98xx_add_widgets(tfa98xx);

	snd_soc_dapm_ignore_suspend(dapm, "AIF IN");
	snd_soc_dapm_ignore_suspend(dapm, "AIF OUT");
	snd_soc_dapm_ignore_suspend(dapm, "OUTL");
	snd_soc_dapm_ignore_suspend(dapm, "AEC Loopback");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC1");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC2");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC3");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC4");
	snd_soc_dapm_ignore_suspend(dapm, "AIF Playback-1-34");
	snd_soc_dapm_ignore_suspend(dapm, "AIF Capture-1-34");

	dev_info(codec->dev, "tfa98xx codec registered (%s)",
		tfa98xx->fw.name);

	return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
static void tfa98xx_remove(struct snd_soc_component *codec)
{
	struct tfa98xx *tfa98xx = snd_soc_component_get_drvdata(codec);
#else
static int tfa98xx_remove(struct snd_soc_codec *codec)
{
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
#endif
	pr_debug("\n");

	tfa98xx_interrupt_enable(tfa98xx, false);

	tfa98xx_inputdev_unregister(tfa98xx);

	cancel_delayed_work_sync(&tfa98xx->interrupt_work);
	cancel_delayed_work_sync(&tfa98xx->monitor_work);
	cancel_delayed_work_sync(&tfa98xx->init_work);
	cancel_delayed_work_sync(&tfa98xx->tapdet_work);
	cancel_delayed_work_sync(&tfa98xx->nmodeupdate_work);

	if (tfa98xx->tfa98xx_wq)
		destroy_workqueue(tfa98xx->tfa98xx_wq);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
	return;
#else
	return 0;
#endif
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(4,18,0))
static struct regmap *tfa98xx_get_regmap(struct device *dev)
{
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);

	return tfa98xx->regmap;
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
static struct snd_soc_component_driver soc_codec_dev_tfa98xx = {
#else
static struct snd_soc_codec_driver soc_codec_dev_tfa98xx = {
#endif
	.probe =	tfa98xx_probe,
	.remove =	tfa98xx_remove,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(4,18,0))
	.get_regmap = tfa98xx_get_regmap,
#endif
};


static bool tfa98xx_writeable_register(struct device *dev, unsigned int reg)
{
	/* enable read access for all registers */
	return 1;
}

static bool tfa98xx_readable_register(struct device *dev, unsigned int reg)
{
	/* enable read access for all registers */
	return 1;
}

static bool tfa98xx_volatile_register(struct device *dev, unsigned int reg)
{
	/* enable read access for all registers */
	return 1;
}

static const struct regmap_config tfa98xx_regmap = {
	.reg_bits = 8,
	.val_bits = 16,

	.max_register = TFA98XX_MAX_REGISTER,
	.writeable_reg = tfa98xx_writeable_register,
	.readable_reg = tfa98xx_readable_register,
	.volatile_reg = tfa98xx_volatile_register,
	.cache_type = REGCACHE_NONE,
};

#if 0
static void tfa98xx_irq_tfa2(struct tfa98xx *tfa98xx)
{
	pr_info("\n");

	/*
	 * mask interrupts
	 * will be unmasked after handling interrupts in workqueue
	 */
	if ((tfa98xx->rev == 0x73) || (tfa98xx->rev == 0x74))
		tfa987x_irq_mask(tfa98xx->tfa);
	else
		tfa_irq_mask(tfa98xx->tfa);

	queue_delayed_work(tfa98xx->tfa98xx_wq, &tfa98xx->interrupt_work, 0);
}


static irqreturn_t tfa98xx_irq(int irq, void *data)
{
	struct tfa98xx *tfa98xx = data;

	if (tfa98xx->tfa->tfa_family == 2)
		tfa98xx_irq_tfa2(tfa98xx);

	return IRQ_HANDLED;
}
#endif

static int tfa98xx_ext_reset(struct tfa98xx *tfa98xx)
{
	if (tfa98xx && gpio_is_valid(tfa98xx->reset_gpio)) {
		int reset = tfa98xx->reset_polarity;
		gpio_set_value_cansleep(tfa98xx->reset_gpio, reset);
		mdelay(10);
		gpio_set_value_cansleep(tfa98xx->reset_gpio, !reset);
		mdelay(10);
	}
	return 0;
}

static int tfa98xx_parse_dt(struct device *dev, struct tfa98xx *tfa98xx,
	struct device_node *np) {
	u32 value;
	int ret;
	tfa98xx->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (tfa98xx->reset_gpio < 0)
		dev_dbg(dev, "No reset GPIO provided, will not HW reset device\n");

	tfa98xx->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
	if (tfa98xx->irq_gpio < 0)
		dev_dbg(dev, "No IRQ GPIO provided.\n");
	ret = of_property_read_u32(np,"reset-polarity",&value);
	if(ret< 0)
	{
		tfa98xx->reset_polarity = HIGH;
    } else {
		tfa98xx->reset_polarity = (value == 0) ? LOW : HIGH;
	}

	dev_dbg(dev, "reset-polarity:%d\n",tfa98xx->reset_polarity);

	tfa98xx->spk_sw_gpio = of_get_named_gpio(np, "spk-sw-gpio", 0);
	if (tfa98xx->spk_sw_gpio < 0)
		printk(KERN_ERR  "No spk_sw_gpio GPIO provided\n");
	return 0;
}

static ssize_t tfa98xx_reg_write(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr,
	char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);

	if (count != 1) {
		pr_debug("invalid register address");
		return -EINVAL;
	}

	tfa98xx->reg = buf[0];

	return 1;
}

static ssize_t tfa98xx_rw_write(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr,
	char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	u8 *data;
	int ret;
	int retries = I2C_RETRIES;

	data = kmalloc(count + 1, GFP_KERNEL);
	if (data == NULL) {
		pr_debug("can not allocate memory\n");
		return  -ENOMEM;
	}

	data[0] = tfa98xx->reg;
	memcpy(&data[1], buf, count);

retry:
	ret = i2c_master_send(tfa98xx->i2c, data, count + 1);
	if (ret < 0) {
		pr_warn("i2c error, retries left: %d\n", retries);
		if (retries) {
			retries--;
			msleep(I2C_RETRY_DELAY);
			goto retry;
		}
	}

	kfree(data);

	/* the number of data bytes written without the register address */
	return ((ret > 1) ? count : -EIO);
}

static ssize_t tfa98xx_rw_read(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr,
	char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	struct i2c_msg msgs[] = {
		{
			.addr = tfa98xx->i2c->addr,
			.flags = 0,
			.len = 1,
			.buf = &tfa98xx->reg,
		},
		{
			.addr = tfa98xx->i2c->addr,
			.flags = I2C_M_RD,
			.len = count,
			.buf = buf,
		},
	};
	int ret;
	int retries = I2C_RETRIES;
retry:
	ret = i2c_transfer(tfa98xx->i2c->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret < 0) {
		pr_warn("i2c error, retries left: %d\n", retries);
		if (retries) {
			retries--;
			msleep(I2C_RETRY_DELAY);
			goto retry;
		}
		return ret;
	}
	/* ret contains the number of i2c transaction */
	/* return the number of bytes read */
	return ((ret > 1) ? count : -EIO);
}

static struct bin_attribute dev_attr_rw = {
	.attr = {
		.name = "rw",
		.mode = S_IRUSR | S_IWUSR,
	},
	.size = 0,
	.read = tfa98xx_rw_read,
	.write = tfa98xx_rw_write,
};

static struct bin_attribute dev_attr_reg = {
	.attr = {
		.name = "reg",
		.mode = S_IWUSR,
	},
	.size = 0,
	.read = NULL,
	.write = tfa98xx_reg_write,
};

static uint16_t gCurrentAddress = 0;
static ssize_t tfa98xx_misc_device_profile_write(struct file *file, const char __user *user_buf,
						size_t count, loff_t *ppos)
{
	struct tfa98xx *tfa98xx = NULL;
	char name[100] = { 0 };
	int ret = 0;
	int profileID = 0;

	pr_info("entry count=%d\n", (int)count);

	memset(name, 0x00, sizeof(name));
	ret = copy_from_user(name, user_buf, count);
	pr_info("profile name=%s\n", name);

	/* search profile name and return ID. */
	profileID = get_profile_id_by_name(name, strlen(name));
	if (profileID < 0) {
		pr_err("didn't find profile from list.\n");
		return 0;
	} else {
		tfa98xx_mixer_profile = profileID;
		/* update profile id for all TFA devices. */
		list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
			int prof_idx = get_profile_id_for_sr(profileID, tfa98xx->rate);
			if (prof_idx < 0) {
				pr_err("sample rate [%d] not supported for this profile(%d).\n", tfa98xx->rate, profileID);
				return 0;
			} else {
				tfa98xx->profile = prof_idx;
				tfa98xx->vstep = tfa98xx->prof_vsteps[prof_idx];
				pr_info("update profile index (%d:%d) succeeded\n", profileID, prof_idx);
			}
		}
	}

	return count;
}

static ssize_t tfa98xx_misc_device_reg_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct tfa98xx *tfa98xx = NULL;
	u8 address[2] = {0, 0};
	int ret = 0;

	pr_info("entry    count=%d\n", (int)count);
	if (count != 2) {
		pr_err("invalid register address\n");
		return -EINVAL;
	}

	ret = copy_from_user(address, user_buf, count);
	if (ret) {
		pr_err("copy data from user space failed.\n");
	}

    gCurrentAddress = (uint16_t)address[0];  /* device address */
    list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
        if (gCurrentAddress == tfa98xx->i2c->addr) {
            break;
        }
    }
    tfa98xx->reg = address[1]; /* sub address */
	pr_info("gCurrentAddress=0x%x tfa98xx->reg=0x%x\n", gCurrentAddress, tfa98xx->reg);

	return count;
}

static ssize_t tfa98xx_misc_device_rw_read(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct tfa98xx *tfa98xx = NULL;
	struct i2c_msg msgs[] = {
		{
			.addr = 0,
			.flags = 0,
			.len = 1,
			.buf = NULL,
		},
		{
			.addr = 0,
			.flags = I2C_M_RD,
			.len = count,
			.buf = NULL,
		},
	};

	u8 *data;
	int ret;
	int retries = I2C_RETRIES;

	pr_info("entry    count=%d\n", (int)count);
    list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
        if (gCurrentAddress == tfa98xx->i2c->addr) {
            msgs[0].addr = tfa98xx->i2c->addr;
            msgs[0].buf = &tfa98xx->reg;
            msgs[1].addr = tfa98xx->i2c->addr;
            break;
        }
    }

	data = kmalloc(count+1, GFP_KERNEL);
	if (data == NULL) {
		pr_debug("can not allocate memory\n");
		return  -ENOMEM;
	}

	msgs[1].buf = data;

retry:
	ret = i2c_transfer(tfa98xx->i2c->adapter, msgs, ARRAY_SIZE(msgs));
	pr_info("i2c_transfer  ret=%d\n", ret);

	if (ret < 0) {
		pr_warn("i2c error, retries left: %d\n", retries);
		if (retries) {
			retries--;
			msleep(I2C_RETRY_DELAY);
			goto retry;
		}

		kfree(data);
		return ret;
	} else if (ret > 1) {
		int ret_cpy = copy_to_user(user_buf, data, count);
		if (ret_cpy) {
			pr_err("copy to user space failed.\n");
		}
	}

	/* ret contains the number of i2c transaction */
	/* return the number of bytes read */
	kfree(data);
	return ((ret > 1) ? count : -EIO);
}

static ssize_t tfa98xx_misc_device_rw_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct tfa98xx *tfa98xx = NULL;
	u8 *data;
	int ret;
	int retries = I2C_RETRIES;
	pr_info("entry    count=%d\n", (int)count);

    data = kmalloc(count+1, GFP_KERNEL);
	if (data == NULL) {
		pr_debug("can not allocate memory\n");
		return  -ENOMEM;
	}
    list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
        if (gCurrentAddress == tfa98xx->i2c->addr) {
            break;
        }
    }

	pr_info("tfa98xx->reg=0x%x\n", tfa98xx->reg);

	data[0] = tfa98xx->reg;
	if (copy_from_user(&data[1], user_buf, count)) {
		pr_err("copy to user space failed.\n");
	}


retry:
	ret = i2c_master_send(tfa98xx->i2c, data, count+1);
	if (ret < 0) {
		pr_warn("i2c error, retries left: %d\n", retries);
		if (retries) {
			retries--;
			msleep(I2C_RETRY_DELAY);
			goto retry;
		}
	}

	kfree(data);

	/* the number of data bytes written without the register address */
	return ((ret > 1) ? count : -EIO);
}

static int tfa98xx_misc_device_rpc_open(struct inode *inode, struct file *file)
{
	struct tfa98xx *tfa98xx = container_of(file->private_data,
					struct tfa98xx, tfa98xx_rpc);
	if (tfa98xx) {
		file->private_data = tfa98xx;
		return 0;
	} else {
		file->private_data = NULL;
		return -EINVAL;
	}
}

static ssize_t tfa98xx_misc_device_rpc_read(struct file *file, char __user *user_buf,
											size_t count, loff_t *ppos)
{
	struct tfa98xx *tfa98xx = file->private_data;
	uint8_t *buffer = NULL;
	int ret = 0;

	buffer = kmalloc(count, GFP_KERNEL);
	if (buffer == NULL) {
		pr_err("can not allocate memory\n");
		return -ENOMEM;
	}

	mutex_lock(&tfa98xx->dsp_lock);

	ret = send_tfa_cal_apr(buffer, count, true);

	mutex_unlock(&tfa98xx->dsp_lock);
	if (ret) {
		pr_err("dsp_msg_read error: %d\n", ret);
		kfree(buffer);
		return -EFAULT;
	}

	ret = copy_to_user(user_buf, buffer, count);
	kfree(buffer);
	if (ret)
		return -EFAULT;

	*ppos += count;
	return count;
}

static ssize_t tfa98xx_misc_device_rpc_write(struct file *file, const char __user *user_buf,
											size_t count, loff_t *ppos)
{
	struct tfa98xx *tfa98xx = file->private_data;
	uint8_t *buffer;
	int err = 0;

	if (count == 0)
		return 0;

	/* msg_file.name is not used */
	buffer = kmalloc(count, GFP_KERNEL);
	if ( buffer == NULL ) {
		pr_err("can not allocate memory\n");
		return  -ENOMEM;
	}
	if (copy_from_user(buffer, user_buf, count)) {
		kfree(buffer);
		pr_err("copy_from_user failed!!\n");
		return -EFAULT;
	}
	mutex_lock(&tfa98xx->dsp_lock);

	err = send_tfa_cal_apr(buffer, count, false);
	if (err) {
		pr_err("dsp_msg error: %d\n", err);
	}

	mdelay(2);

	mutex_unlock(&tfa98xx->dsp_lock);

	kfree(buffer);

	if (err)
		return err;
	return count;
}

static long tfa98xx_misc_device_control_ioctl(struct file *file,
	unsigned int cmd,
	unsigned long arg)
{
	struct tfa98xx *tfa98xx = NULL;
	int result = 0;

	pr_info("entry  cmd=%d    arg=%p\n", cmd, (void*)arg);
	if (!arg) {
		pr_err("arg is NULL!\n");
		return -EINVAL;
	}

	switch (cmd) {
		case IOCTL_CMD_GET_MEMTRACK_DATA: {
			list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
				if (tfa98xx->tfa->is_probus_device) {
					void *pUserData = (void*)arg;
					int livedata[6];
					int error = 0;
					uint8_t memtrack[] = {
						0x00, 0x80, 0x0b, 0x00, 0x00, 0x06,
						0x22, 0x00, 0x00, 0x22, 0x00, 0x01, /* Re0 */
						0x22, 0x00, 0x04, 0x22, 0x00, 0x05, /* Speaker Temperature */
						0x22, 0x00, 0x13, 0x22, 0x00, 0x14  /* F0 */
					};
					uint8_t data[21] = { 0 };

					mutex_lock(&tfa98xx->dsp_lock);
					/* send livedata configuration. */
					error = send_tfa_cal_apr(memtrack, sizeof(memtrack), false);
					if (error) {
						mutex_unlock(&tfa98xx->dsp_lock);
						return Tfa98xx_Error_Bad_Parameter;
					}
					mdelay(2);

					/* send query command */
					data[0] = 0x00;
					data[1] = 0x80;
					data[2] = 0x8b;
					error = send_tfa_cal_apr(data, sizeof(data), false);
					if (error) {
						mutex_unlock(&tfa98xx->dsp_lock);
						return Tfa98xx_Error_Bad_Parameter;
					}
					mdelay(5);

					/* read livedata from host DSP. */
					error = send_tfa_cal_apr(data, sizeof(data), true);
					if (error) {
						mutex_unlock(&tfa98xx->dsp_lock);
						return Tfa98xx_Error_Bad_Parameter;
					}
					mdelay(5);

					/* primary */
					livedata[0] = (data[3] << 16) + (data[4] << 8) + data[5];		/* Re0: should be dividing 65536 in userspace */
					livedata[1] = ((data[9] << 16) + (data[10] << 8) + data[11]);	/* Temperature: should be dividing 16384 in userspace */
					livedata[2] = (data[15] << 16) + (data[16] << 8) + data[17];	/* F0 */

					/* secondary */
					livedata[3] = (data[6] << 16) + (data[7] << 8) + data[8];		/* Re0: should be dividing 65536 in userspace */
					livedata[4] = ((data[12] << 16) + (data[13] << 8) + data[14]);	/* Temperature: should be dividing 16384 in userspace */
					livedata[5] = (data[18] << 16) + (data[19] << 8) + data[20];	/* F0 */

					if (1 == tfa98xx_device_count) {
						result	= copy_to_user((void __user *)pUserData, &livedata[0], sizeof(int) * 3);
					} else {
						result	= copy_to_user((void __user *)pUserData, &livedata[0], sizeof(int) * 6);
					}
					if (result) {
						pr_err("copy to user space failed(%d).\n", result);
						result = -EINVAL;
					}
					mutex_unlock(&tfa98xx->dsp_lock);
					break;
				}
			}
			break;
		}
		case IOCTL_CMD_GET_CNT_VERSION: {
			void *pUserData = (void*)arg;

			if (NULL != tfa98xx_container) {
				result = copy_to_user((void __user *)pUserData, tfa98xx_container->type, strlen(tfa98xx_container->type));
				if (result) {
					pr_err("copy to user space failed(%d).\n", result);
				} else {
					result = strlen(tfa98xx_container->type);
				}
			} else {
				pr_err("get cnt version failed.\n");
				result = -EINVAL;
			}
			break;
		}
		default:
			result = -EINVAL;
			pr_err("un-supported command. (%d)\n", cmd);
			break;
	}

	pr_info("exit  result=%d\n", result);
	return result;
}

#ifdef CONFIG_COMPAT
static long tfa98xx_misc_device_control_compat_ioctl(struct file *file,
													unsigned int cmd,
													unsigned long arg)
{
	pr_info("%s entry  cmd=%d    arg=%p\n", __func__, cmd, (void*)arg);

	if (!arg) {
		pr_err("%s No data send to driver!\n", __func__);
		return -EINVAL;
	}

	return tfa98xx_misc_device_control_ioctl(file, cmd, arg);
}
#endif


static const struct tfa98xx_miscdevice_info miscdevice_info[MISC_DEVICE_MAX] = {
	{
		.devicename = "tfa_reg",
		.operations.owner = THIS_MODULE,
		.operations.write = tfa98xx_misc_device_reg_write,
	},
	{
		.devicename = "tfa_rw",
		.operations.owner = THIS_MODULE,
		.operations.read = tfa98xx_misc_device_rw_read,
		.operations.write = tfa98xx_misc_device_rw_write,
	},
	{
		.devicename = "tfa_rpc",
		.operations.owner = THIS_MODULE,
		.operations.open = tfa98xx_misc_device_rpc_open,
		.operations.read = tfa98xx_misc_device_rpc_read,
		.operations.write = tfa98xx_misc_device_rpc_write,
	},
	{
		.devicename = "tfa_profile",
		.operations.owner = THIS_MODULE,
		.operations.write = tfa98xx_misc_device_profile_write,
	},
	{
		.devicename = "tfa_control",
		.operations.owner = THIS_MODULE,
		.operations.unlocked_ioctl = tfa98xx_misc_device_control_ioctl,
#ifdef CONFIG_COMPAT
		.operations.compat_ioctl = tfa98xx_misc_device_control_compat_ioctl,
#endif
	},
};

int tfa98xx_init_misc_device(struct tfa98xx *tfa98xx)
{
	int ret = 0;

	pr_info("entry\n");
	if (NULL == tfa98xx) {
		pr_err("tfa98xx is NULL.\n");
		return -EINVAL;
	}

	pr_info("I2C bus=0x%x  address=0x%x\n", tfa98xx->i2c->adapter->nr, tfa98xx->i2c->addr);
	/* create device node "tfa_reg" for write sub address. */
	tfa98xx->tfa98xx_reg.minor = MISC_DYNAMIC_MINOR;
	tfa98xx->tfa98xx_reg.name = miscdevice_info[MISC_DEVICE_TFA98XX_REG].devicename;
	tfa98xx->tfa98xx_reg.fops = &miscdevice_info[MISC_DEVICE_TFA98XX_REG].operations;
	ret = misc_register(&tfa98xx->tfa98xx_reg);
	if (ret) {
		pr_err("tfa98xx_init_misc_device: register misc device [%s] failed\n", tfa98xx->tfa98xx_reg.name);
	}

	/* create device node "tfa_rw" for read/write i2c device. */
	tfa98xx->tfa98xx_rw.minor = MISC_DYNAMIC_MINOR;
	tfa98xx->tfa98xx_rw.name = miscdevice_info[MISC_DEVICE_TFA98XX_RW].devicename;
	tfa98xx->tfa98xx_rw.fops = &miscdevice_info[MISC_DEVICE_TFA98XX_RW].operations;
	ret = misc_register(&tfa98xx->tfa98xx_rw);
	if (ret) {
		pr_err("tfa98xx_init_misc_device: register misc device [%s] failed\n", tfa98xx->tfa98xx_rw.name);
	}

	/* create device node "tfa_rpc" for switching profile. */
	tfa98xx->tfa98xx_rpc.minor = MISC_DYNAMIC_MINOR;
	tfa98xx->tfa98xx_rpc.name = miscdevice_info[MISC_DEVICE_TFA98XX_RPC].devicename;
	tfa98xx->tfa98xx_rpc.fops = &miscdevice_info[MISC_DEVICE_TFA98XX_RPC].operations;
	ret = misc_register(&tfa98xx->tfa98xx_rpc);
	if (ret) {
		pr_err("tfa98xx_init_misc_device: register misc device [%s] failed\n", tfa98xx->tfa98xx_rpc.name);
	}

	/* create device node "tfa_profile" for switching profile. */
	tfa98xx->tfa98xx_profile.minor = MISC_DYNAMIC_MINOR;
	tfa98xx->tfa98xx_profile.name = miscdevice_info[MISC_DEVICE_TFA98XX_PROFILE].devicename;
	tfa98xx->tfa98xx_profile.fops = &miscdevice_info[MISC_DEVICE_TFA98XX_PROFILE].operations;
	ret = misc_register(&tfa98xx->tfa98xx_profile);
	if (ret) {
		pr_err("tfa98xx_init_misc_device: register misc device [%s] failed\n", tfa98xx->tfa98xx_profile.name);
	}

	/* create device node "tfa_control" for IO control. */
	tfa98xx->tfa98xx_control.minor = MISC_DYNAMIC_MINOR;
	tfa98xx->tfa98xx_control.name = miscdevice_info[MISC_DEVICE_TFA98XX_IOCTL].devicename;
	tfa98xx->tfa98xx_control.fops = &miscdevice_info[MISC_DEVICE_TFA98XX_IOCTL].operations;
	ret = misc_register(&tfa98xx->tfa98xx_control);
	if (ret) {
		pr_err("tfa98xx_init_misc_device: register misc device [%s] failed\n", tfa98xx->tfa98xx_control.name);
	}

	if (0 == ret)
		pr_info("register misc device successed.\n");
	return ret;
}

void tfa98xx_remove_misc_device(struct tfa98xx *tfa98xx)
{
	pr_info("entry\n");
	if (NULL == tfa98xx) {
		pr_err("tfa98xx is NULL.\n");
		return;
	}

	misc_deregister(&tfa98xx->tfa98xx_reg);
	misc_deregister(&tfa98xx->tfa98xx_rw);
	misc_deregister(&tfa98xx->tfa98xx_rpc);
	misc_deregister(&tfa98xx->tfa98xx_profile);
	misc_deregister(&tfa98xx->tfa98xx_control);
	return;
}

static int tfa98xx_i2c_probe(struct i2c_client *i2c,
	const struct i2c_device_id *id)
{
	struct snd_soc_dai_driver *dai;
	struct tfa98xx *tfa98xx;
	struct device_node *np = i2c->dev.of_node;
	//int irq_flags;
	unsigned int reg;
	int ret;

	pr_info("addr=0x%x\n", i2c->addr);

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		dev_err(&i2c->dev, "check_functionality failed\n");
		return -EIO;
	}

	tfa98xx = devm_kzalloc(&i2c->dev, sizeof(struct tfa98xx), GFP_KERNEL);
	if (tfa98xx == NULL)
		return -ENOMEM;

	tfa98xx->dev = &i2c->dev;
	tfa98xx->i2c = i2c;
	tfa98xx->dsp_init = TFA98XX_DSP_INIT_STOPPED;
	tfa98xx->rate = 48000; /* init to the default sample rate (48kHz) */
	tfa98xx->tfa = NULL;
	tfa98xx->tfa_mute_mode = TFA98XX_DEVICE_MUTE_OFF; /* the mute mode is disabled by default. */
	tfa98xx->regmap = devm_regmap_init_i2c(i2c, &tfa98xx_regmap);
	if (IS_ERR(tfa98xx->regmap)) {
		ret = PTR_ERR(tfa98xx->regmap);
		dev_err(&i2c->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	i2c_set_clientdata(i2c, tfa98xx);
	mutex_init(&tfa98xx->dsp_lock);
	init_waitqueue_head(&tfa98xx->wq);

	if (np) {
		ret = tfa98xx_parse_dt(&i2c->dev, tfa98xx, np);
		if (ret) {
			dev_err(&i2c->dev, "Failed to parse DT node\n");
			return ret;
		}
		if (no_start)
			tfa98xx->irq_gpio = -1;
		if (no_reset)
			tfa98xx->reset_gpio = -1;
	}
	else {
		tfa98xx->reset_gpio = -1;
		tfa98xx->irq_gpio = -1;
	}

	if (gpio_is_valid(tfa98xx->reset_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, tfa98xx->reset_gpio,
			GPIOF_OUT_INIT_LOW, "TFA98XX_RST");
		if (ret)
			return ret;
	}

	if (gpio_is_valid(tfa98xx->irq_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, tfa98xx->irq_gpio,
			GPIOF_DIR_IN, "TFA98XX_INT");
		if (ret)
			return ret;
	}

	if (gpio_is_valid(tfa98xx->spk_sw_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, tfa98xx->spk_sw_gpio,
			GPIOF_OUT_INIT_LOW, "TFA98XX_SPK_SW");
		if (ret)
			return ret;
	}

	tfa98xx->spk_id_gpio_p = of_parse_phandle(np,
				"nxp,spk-id-pin", 0);

	if (!tfa98xx->spk_id_gpio_p) {
		dev_err(&i2c->dev, "property %s not detected in node %s",
				"nxp,spk-id-pin", np->full_name);
	} else {
		dev_err(&i2c->dev, "fw_name =%s\n", fw_name);
	}

	/* Power up! */
    /* we should reset chip only 1 times if all reset pin connected to 1 GPIO. */
    if (0 == tfa98xx_device_count)
    	tfa98xx_ext_reset(tfa98xx);

	if ((no_start == 0) && (no_reset == 0)) {
		ret = regmap_read(tfa98xx->regmap, 0x03, &reg);
		if (ret < 0) {
			dev_err(&i2c->dev, "Failed to read Revision register: %d\n",
				ret);
			return -EIO;
		}

		tfa98xx->rev = reg & 0xff;
		switch (tfa98xx->rev) {
		case 0x72: /* tfa9872 */
			pr_info("TFA9872 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_MULTI_MIC_INPUTS;
			tfa98xx->flags |= TFA98XX_FLAG_CALIBRATION_CTL;
			tfa98xx->flags |= TFA98XX_FLAG_REMOVE_PLOP_NOISE;
			/* tfa98xx->flags |= TFA98XX_FLAG_LP_MODES; */
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			break;
		case 0x73: /* tfa9873 */
			pr_info("TFA9873 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_MULTI_MIC_INPUTS;
			tfa98xx->flags |= TFA98XX_FLAG_CALIBRATION_CTL;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			tfa98xx->flags |= TFA98XX_FLAG_ADAPT_NOISE_MODE; /***MCH_TO_TEST***/
			break;
		case 0x74: /* tfa9874 */
			pr_info("TFA9874 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_MULTI_MIC_INPUTS;
			tfa98xx->flags |= TFA98XX_FLAG_CALIBRATION_CTL;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			break;
		case 0x78: /* tfa9878 */
			pr_info("TFA9878 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_MULTI_MIC_INPUTS;
			tfa98xx->flags |= TFA98XX_FLAG_CALIBRATION_CTL;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			break;
		case 0x88: /* tfa9888 */
			pr_info("TFA9888 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_STEREO_DEVICE;
			tfa98xx->flags |= TFA98XX_FLAG_MULTI_MIC_INPUTS;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			break;
		case 0x13: /* tfa9912 */
			pr_info("TFA9912 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_MULTI_MIC_INPUTS;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			/* tfa98xx->flags |= TFA98XX_FLAG_TAPDET_AVAILABLE; */
			break;
		case 0x94: /* tfa9894 */
			pr_info("TFA9894 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_MULTI_MIC_INPUTS;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
			break;
		case 0x80: /* tfa9890 */
		case 0x81: /* tfa9890 */
			pr_info("TFA9890 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
			break;
		case 0x92: /* tfa9891 */
			pr_info("TFA9891 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_SAAM_AVAILABLE;
			tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
			break;
		case 0x12: /* tfa9895 */
			pr_info("TFA9895 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
			break;
		case 0x97:
			pr_info("TFA9897 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			break;
		case 0x96:
			pr_info("TFA9896 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			break;
		default:
			pr_info("Unsupported device revision (0x%x)\n", reg & 0xff);
			return -EINVAL;
		}
	}

	tfa98xx->tfa = devm_kzalloc(&i2c->dev, sizeof(struct tfa_device), GFP_KERNEL);
	if (tfa98xx->tfa == NULL)
		return -ENOMEM;

	tfa98xx->tfa->data = (void *)tfa98xx;
	tfa98xx->tfa->cachep = tfa98xx_cache;

	/* Modify the stream names, by appending the i2c device address.
	 * This is used with multicodec, in order to discriminate the devices.
	 * Stream names appear in the dai definition and in the stream  	 .
	 * We create copies of original structures because each device will
	 * have its own instance of this structure, with its own address.
	 */
	dai = devm_kzalloc(&i2c->dev, sizeof(tfa98xx_dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;
	memcpy(dai, tfa98xx_dai, sizeof(tfa98xx_dai));

	tfa98xx_append_i2c_address(&i2c->dev,
		i2c,
		NULL,
		0,
		dai,
		ARRAY_SIZE(tfa98xx_dai));

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
	ret = devm_snd_soc_register_component(&i2c->dev,
				&soc_codec_dev_tfa98xx, dai,
				ARRAY_SIZE(tfa98xx_dai));
#else
	ret = snd_soc_register_codec(&i2c->dev,
				&soc_codec_dev_tfa98xx, dai,
				ARRAY_SIZE(tfa98xx_dai));
#endif
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to register TFA98xx: %d\n", ret);
		return ret;
	}
#if 0
	if (gpio_is_valid(tfa98xx->irq_gpio) &&
		!(tfa98xx->flags & TFA98XX_FLAG_SKIP_INTERRUPTS)) {
		/* register irq handler */
		irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
		ret = devm_request_threaded_irq(&i2c->dev,
			gpio_to_irq(tfa98xx->irq_gpio),
			NULL, tfa98xx_irq, irq_flags,
			"tfa98xx", tfa98xx);
		if (ret != 0) {
			dev_err(&i2c->dev, "Failed to request IRQ %d: %d\n",
				gpio_to_irq(tfa98xx->irq_gpio), ret);
			return ret;
		}
	}
	else {
		dev_info(&i2c->dev, "Skipping IRQ registration\n");
		/* disable feature support if gpio was invalid */
		tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
	}
#endif
#ifdef CONFIG_DEBUG_FS
	if (no_start == 0)
		tfa98xx_debug_init(tfa98xx, i2c);
#endif
	/* Register the sysfs files for climax backdoor access */
	ret = device_create_bin_file(&i2c->dev, &dev_attr_rw);
	if (ret)
		dev_info(&i2c->dev, "error creating sysfs files\n");
	ret = device_create_bin_file(&i2c->dev, &dev_attr_reg);
	if (ret)
		dev_info(&i2c->dev, "error creating sysfs files\n");

    if (0 == tfa98xx_device_count)
    	tfa98xx_init_misc_device(tfa98xx);
	pr_info("%s Probe completed successfully!\n", __func__);

	INIT_LIST_HEAD(&tfa98xx->list);

	mutex_lock(&tfa98xx_mutex);
	tfa98xx_device_count++;
	list_add(&tfa98xx->list, &tfa98xx_device_list);
	mutex_unlock(&tfa98xx_mutex);

	return 0;
}

static int tfa98xx_i2c_remove(struct i2c_client *i2c)
{
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);

	pr_debug("addr=0x%x\n", i2c->addr);

	tfa98xx_interrupt_enable(tfa98xx, false);

	cancel_delayed_work_sync(&tfa98xx->interrupt_work);
	cancel_delayed_work_sync(&tfa98xx->monitor_work);
	cancel_delayed_work_sync(&tfa98xx->init_work);
	cancel_delayed_work_sync(&tfa98xx->tapdet_work);
	cancel_delayed_work_sync(&tfa98xx->nmodeupdate_work);

	device_remove_bin_file(&i2c->dev, &dev_attr_reg);
	device_remove_bin_file(&i2c->dev, &dev_attr_rw);
#ifdef CONFIG_DEBUG_FS
	tfa98xx_debug_remove(tfa98xx);
#endif

	tfa98xx_remove_misc_device(tfa98xx);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
	snd_soc_unregister_component(&i2c->dev);
#else
	snd_soc_unregister_codec(&i2c->dev);
#endif
	if (gpio_is_valid(tfa98xx->irq_gpio))
		devm_gpio_free(&i2c->dev, tfa98xx->irq_gpio);
	if (gpio_is_valid(tfa98xx->reset_gpio))
		devm_gpio_free(&i2c->dev, tfa98xx->reset_gpio);
	if (gpio_is_valid(tfa98xx->spk_sw_gpio))
		devm_gpio_free(&i2c->dev, tfa98xx->spk_sw_gpio);
	mutex_lock(&tfa98xx_mutex);
	list_del(&tfa98xx->list);
	tfa98xx_device_count--;
	if (tfa98xx_device_count == 0) {
		kfree(tfa98xx_container);
		tfa98xx_container = NULL;
	}
	mutex_unlock(&tfa98xx_mutex);

	return 0;
}

static const struct i2c_device_id tfa98xx_i2c_id[] = {
	{ "tfa98xx", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tfa98xx_i2c_id);

#ifdef CONFIG_OF
static struct of_device_id tfa98xx_dt_match[] = {
	{.compatible = "tfa,tfa98xx" },
	{.compatible = "tfa,tfa9872" },
	{.compatible = "tfa,tfa9873" },
	{.compatible = "tfa,tfa9874" },
	{.compatible = "tfa,tfa9878" },
	{.compatible = "tfa,tfa9888" },
	{.compatible = "tfa,tfa9890" },
	{.compatible = "tfa,tfa9891" },
	{.compatible = "tfa,tfa9894" },
	{.compatible = "tfa,tfa9895" },
	{.compatible = "tfa,tfa9896" },
	{.compatible = "tfa,tfa9897" },
	{.compatible = "tfa,tfa9912" },
	{ },
};
#endif

static struct i2c_driver tfa98xx_i2c_driver = {
	.driver = {
		.name = "tfa98xx",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tfa98xx_dt_match),
	},
	.probe = tfa98xx_i2c_probe,
	.remove = tfa98xx_i2c_remove,
	.id_table = tfa98xx_i2c_id,
};

static int __init tfa98xx_i2c_init(void)
{
	int ret = 0;

	pr_info("TFA98XX driver version %s\n", TFA98XX_VERSION);

	/* Enable debug traces */
	tfa98xx_kmsg_regs = trace_level & 2;
	tfa98xx_ftrace_regs = trace_level & 4;

	/* Initialize kmem_cache */
	tfa98xx_cache = kmem_cache_create("tfa98xx_cache", /* Cache name /proc/slabinfo */
		PAGE_SIZE, /* Structure size, we should fit in single page */
		0, /* Structure alignment */
		(SLAB_HWCACHE_ALIGN | SLAB_RECLAIM_ACCOUNT |
			SLAB_MEM_SPREAD), /* Cache property */
		NULL); /* Object constructor */
	if (!tfa98xx_cache) {
		pr_err("tfa98xx can't create memory pool\n");
		ret = -ENOMEM;
	}

	ret = i2c_add_driver(&tfa98xx_i2c_driver);

	return ret;
}
module_init(tfa98xx_i2c_init);

static void __exit tfa98xx_i2c_exit(void)
{
	i2c_del_driver(&tfa98xx_i2c_driver);
	kmem_cache_destroy(tfa98xx_cache);
}
module_exit(tfa98xx_i2c_exit);

MODULE_DESCRIPTION("ASoC TFA98XX driver");
MODULE_LICENSE("GPL");

