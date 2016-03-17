/*
 ** =========================================================================
 ** Copyright (c) 2007-2013  Immersion Corporation.  All rights reserved.
 ** Copyright (C) 2016 XiaoMi, Inc.
 **                          Immersion Corporation Confidential and Proprietary
 **
 ** File:
 **     ImmVibeSPI.c
 **
 ** Description:
 **     Device-dependent functions called by Immersion TSP API
 **     to control PWM duty cycle, amp enable/disable, save IVT file, etc...
 **
 **     This file is provided for Generic Project
 **
 ** =========================================================================
 */
#include <linux/string.h>     /* for strncpy */

/*
 ** This SPI supports only one actuator.
 */
#define NUM_ACTUATORS 1

/************ ISA1000 specific section begin **********/

/*
 ** Name of the ISA1000 board
 */
#define ISA1000_BOARD_NAME   "ISA1000"

/*
 ** Necessities
 */
/* Xiaomi TODO: Please add includes as necessary */
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/time.h>       /* for NSEC_PER_SEC */
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/qpnp/pwm.h>
#include <linux/regulator/consumer.h>
#include <asm/bootinfo.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include "../staging/android/timed_output.h"

#define ISA1000_VIB_DEFAULT_TIMEOUT	15000
#define ISA1000_DEFAULT_PWM_FREQ	30000

static const unsigned int pwm_period_ns = NSEC_PER_SEC / ISA1000_DEFAULT_PWM_FREQ;

struct isa1000_pwm_info {
	struct pwm_device *pwm_dev;
	u32 pwm_channel;
	u32 duty_us;
	u32 period_us;
};

struct isa1000_vib {
	struct platform_device *pdev;
	struct hrtimer vib_timer;
	struct timed_output_dev timed_dev;
	struct work_struct work;
	struct isa1000_pwm_info pwm_info;
	struct regulator *regulator_vdd;

	u16 base;
	int enable_gpio;
	int timeout;
	int state;
	struct mutex lock;
} ;

static struct isa1000_vib *vib_dev;

static int isa1000_vib_set(struct isa1000_vib *vib, int on)
{
	int rc;

	if (on) {
		rc = pwm_enable(vib->pwm_info.pwm_dev);
		if (rc < 0) {
			dev_err(&vib->pdev->dev, "Unable to enable pwm\n");
			return rc;
		}
		gpio_set_value(vib->enable_gpio, 1);
	} else {
		pwm_disable(vib->pwm_info.pwm_dev);
		gpio_set_value(vib->enable_gpio, 0);
	}

	return 0;
}

static void isa1000_vib_enable(struct timed_output_dev *dev, int value)
{
	struct isa1000_vib *vib = container_of(dev, struct isa1000_vib,
			timed_dev);

	mutex_lock(&vib->lock);
	hrtimer_cancel(&vib->vib_timer);

	if (value == 0)
		vib->state = 0;
	else {
		value = (value > vib->timeout ?
				vib->timeout : value);
		vib->state = 1;
		hrtimer_start(&vib->vib_timer,
				ktime_set(value / 1000, (value % 1000) * 1000000),
				HRTIMER_MODE_REL);
	}
	mutex_unlock(&vib->lock);
	schedule_work(&vib->work);
}

static void isa1000_vib_update(struct work_struct *work)
{
	struct isa1000_vib *vib = container_of(work, struct isa1000_vib,
			work);
	isa1000_vib_set(vib, vib->state);
}

static int isa1000_vib_get_time(struct timed_output_dev *dev)
{
	struct isa1000_vib *vib = container_of(dev, struct isa1000_vib,
			timed_dev);

	if (hrtimer_active(&vib->vib_timer)) {
		ktime_t r = hrtimer_get_remaining(&vib->vib_timer);
		return (int)ktime_to_us(r);
	} else
		return 0;
}

static enum hrtimer_restart isa1000_vib_timer_func(struct hrtimer *timer)
{
	struct isa1000_vib *vib = container_of(timer, struct isa1000_vib,
			vib_timer);

	vib->state = 0;
	schedule_work(&vib->work);

	return HRTIMER_NORESTART;
}

/* This function assumes an input of [-127, 127] and mapped to [1,255] */
/* If your PWM module does not take an input of [1,255] and mapping to [1%,99%]
 **    Please modify accordingly
 **/
static void isa1000_vib_set_level(int level)
{
	int rc;
	struct pwm_device *pwm;
	struct device *dev;
	static int last_level = 128;

	if (!vib_dev)
		return;

	if (last_level == level)
		return;

	pwm = vib_dev->pwm_info.pwm_dev;
	dev = &vib_dev->pdev->dev;

	if  (level != 0) {

		/* Set PWM duty cycle corresponding to the input 'level' */
		/* Xiaomi TODO: This is only an example.
		 **              Please modify for PWM on Hong Mi 2A platform
		 */
		rc = pwm_config(pwm,
				(pwm_period_ns * (level + 128)) / 256,
				pwm_period_ns);
		if (rc < 0) {
			dev_err(dev, "[%s] pwm_config fail\n", __func__);
			goto chip_dwn;
		}

		/* Enable the PWM output */
		/* Xiaomi TODO: This is only an example.
		 **              Please modify for PWM on Hong Mi 2A platform
		 */
		rc = pwm_enable(pwm);
		if (rc < 0) {
			dev_err(dev, "[%s] pwm_enable fail\n", __func__);
			goto chip_dwn;
		}

		/* Assert the GPIO_ISA1000_EN to enable ISA1000 */
		/* Xiaomi TODO: This is only an example.
		 **              Please modify for GPIO on Hong Mi 2A platform
		 */
		gpio_set_value_cansleep(vib_dev->enable_gpio, 1);
	} else {
		/* Deassert the GPIO_ISA1000_EN to disable ISA1000 */
		/* Xiaomi TODO: This is only an example.
		 **              Please modify for GPIO on Hong Mi 2A platform
		 */
		gpio_set_value_cansleep(vib_dev->enable_gpio, 0);

		/* Disable the PWM output */
		/* Xiaomi TODO: This is only an example.
		 **              Please modify for PWM on Hong Mi 2A platform
		 */
		pwm_disable(pwm);
	}

	last_level = level;
	return;

chip_dwn:
	gpio_set_value_cansleep(vib_dev->enable_gpio, 0);
}

static int isa1000_setup(struct isa1000_vib *vib)
{
	int rc;

	/* Configure PWM */
	rc = pwm_config_us(vib->pwm_info.pwm_dev, vib->pwm_info.duty_us,
			vib->pwm_info.period_us);
	if (rc < 0) {
		dev_err(&vib->pdev->dev, "vib pwm config failed %d\n", rc);
		pwm_free(vib->pwm_info.pwm_dev);
		return -ENODEV;
	}

	/* Enable regulator */
	rc = regulator_enable(vib->regulator_vdd);
	if (rc < 0) {
		dev_err(&vib->pdev->dev, "vib regulator enable failed %d\n", rc);
		return rc;
	}

	/* Request enable gpio */
	rc = gpio_request(vib->enable_gpio, "vibrator_en");
	if (rc < 0) {
		dev_err(&vib->pdev->dev, "vibrator enable gpio request failed %d\n", rc);
		return rc;
	}

	return 0;
}

static int isa1000_parse_dt(struct isa1000_vib *vib)
{
	struct platform_device *pdev = vib->pdev;
	int rc;
	u32 temp_val;

	vib->timeout = ISA1000_VIB_DEFAULT_TIMEOUT;
	rc = of_property_read_u32(pdev->dev.of_node,
			"isa1000,timeout-ms", &temp_val);
	if (!rc)
		vib->timeout = temp_val;
	else {
		dev_err(&pdev->dev, "Unable to read vib timeout\n");
		return rc;
	}

	vib->pwm_info.pwm_dev = of_pwm_get(pdev->dev.of_node, NULL);
	if (vib->pwm_info.pwm_dev < 0) {
		dev_err(&pdev->dev, "Unable to get pwm device\n");
		return PTR_ERR(vib->pwm_info.pwm_dev);
	}

	rc = of_property_read_u32(pdev->dev.of_node,
			"isa1000,period-us", &temp_val);
	if (!rc)
		vib->pwm_info.period_us = temp_val;
	else {
		dev_err(&pdev->dev, "Unable to get perios us\n");
		return rc;
	}

	rc = of_property_read_u32(pdev->dev.of_node,
			"isa1000,duty-us", &temp_val);
	if (!rc)
		vib->pwm_info.duty_us = temp_val;
	else {
		dev_err(&pdev->dev, "Unable to get duty us\n");
		return rc;
	}

	vib->enable_gpio = of_get_named_gpio_flags(pdev->dev.of_node,
			"isa1000,enable-gpio", 0, NULL);
	if (vib->enable_gpio < 0) {
		dev_err(&pdev->dev, "Unable to get enable gpio\n");
		return -ENODEV;
	}

	vib->regulator_vdd = devm_regulator_get(&pdev->dev, "vdd");
	if (IS_ERR(vib->regulator_vdd)) {
		rc = PTR_ERR(vib->regulator_vdd);
		dev_err(&pdev->dev, "Unable to get regulator\n");
		return rc;
	}

	return 0;
}

static int isa1000_probe(struct platform_device *pdev)
{
	struct isa1000_vib *vib;
	int rc;

	vib = devm_kzalloc(&pdev->dev, sizeof(struct isa1000_vib), GFP_KERNEL);
	if (!vib)
		return -ENOMEM;

	vib->pdev = pdev;

	rc = isa1000_parse_dt(vib);
	if (rc) {
		dev_err(&pdev->dev, "DT parsing failed\n");
		return rc;
	}

	rc = isa1000_setup(vib);
	if (rc) {
		dev_err(&pdev->dev, "isa1000 setup failed\n");
		return rc;
	}

	mutex_init(&vib->lock);
	INIT_WORK(&vib->work, isa1000_vib_update);

	hrtimer_init(&vib->vib_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vib->vib_timer.function = isa1000_vib_timer_func;

	vib->timed_dev.name = "vibrator";
	vib->timed_dev.get_time = isa1000_vib_get_time;
	vib->timed_dev.enable = isa1000_vib_enable;

	dev_set_drvdata(&pdev->dev, vib);

	rc = timed_output_dev_register(&vib->timed_dev);
	if (rc < 0)
		return rc;

	vib_dev = vib;

	/* Disable output */
	isa1000_vib_set_level(0);

	dev_info(&pdev->dev, "ISA1000 probe successfully\n");

	return rc;
}

static int isa1000_remove(struct platform_device *pdev)
{
	struct isa1000_vib *vib = dev_get_drvdata(&pdev->dev);

	cancel_work_sync(&vib->work);
	hrtimer_cancel(&vib->vib_timer);
	timed_output_dev_unregister(&vib->timed_dev);
	mutex_destroy(&vib->lock);

	return 0;
}

static struct of_device_id vibrator_match_table[] = {
	{	.compatible = "vibrator,isa1000",
	},
	{}
};

static struct platform_driver isa1000_drv = {
	.driver = {
		.name	= "vibrator,isa1000",
		.of_match_table = vibrator_match_table,
	},
	.probe		= isa1000_probe,
	.remove		= isa1000_remove,
};

/************ ISA1000 specific section end **********/

/*
 ** This function is necessary for the TSP Designer Bridge.
 ** Called to allocate the specified amount of space in the DIAG output buffer.
 ** OEM must review this function and verify if the proposed function is used in their software package.
 */
void *ImmVibeSPI_Diag_BufPktAlloc(int nLength)
{
	/* No need to be implemented */

	/* return allocate_a_buffer(nLength); */
	return NULL;
}

/*
 ** Called to disable amp (disable output force)
 */
VibeStatus ImmVibeSPI_ForceOut_AmpDisable(VibeUInt8 nActuatorIndex)
{
	/* Disable amp */
	/* Disable the ISA1000 output */
	/* Xiaomi TODO: This is only an example.
	 **              Please change to the GPIO procedures for
	 **              the Hong Mi 2A platform
	 */
	if (vib_dev)
		gpio_set_value_cansleep(vib_dev->enable_gpio, 0);

	return VIBE_S_SUCCESS;
}

/*
 ** Called to enable amp (enable output force)
 */
VibeStatus ImmVibeSPI_ForceOut_AmpEnable(VibeUInt8 nActuatorIndex)
{
	/* Reset PWM frequency (only if necessary on Hong Mi 2A)*/
	/* To be implemented with appropriate hardware access macros */

	/* Set duty cycle to 50% (which correspond to the output level 0) */
	isa1000_vib_set_level(0);

	/* Enable amp */
	/* Enable the ISA1000 output */
	/* Xiaomi TODO: This is only an example.
	 **              Please change to the GPIO procedures for
	 **              the Hong Mi 2A platform
	 */

	return VIBE_S_SUCCESS;
}

/*
 ** Called at initialization time to set PWM freq, disable amp, etc...
 */
VibeStatus ImmVibeSPI_ForceOut_Initialize(void)
{
	int Ret;

	Ret = platform_driver_register(&isa1000_drv);
	if (Ret < 0) {
		pr_err("isa1000 driver register failed %d\n", Ret);
		return VIBE_E_FAIL;
	}

	return VIBE_S_SUCCESS;
}

/*
 ** Called at termination time to set PWM freq, disable amp, etc...
 */
VibeStatus ImmVibeSPI_ForceOut_Terminate(void)
{
	/* Disable amp */
	ImmVibeSPI_ForceOut_AmpDisable(0);

	/* Set PWM frequency (only if necessary on Hong Mi 2A) */
	/* To be implemented with appropriate hardware access macros */

	/* Set duty cycle to 50% */
	/* i.e. output level to 0 */
	isa1000_vib_set_level(0);

	return VIBE_S_SUCCESS;
}

/*
 ** Called by the real-time loop to set force output, and enable amp if required
 */
VibeStatus ImmVibeSPI_ForceOut_SetSamples(VibeUInt8 nActuatorIndex, VibeUInt16 nOutputSignalBitDepth, VibeUInt16 nBufferSizeInBytes, VibeInt8 *pForceOutputBuffer)
{
	/*
	 ** For ERM:
	 **      nBufferSizeInBytes should be equal to 1 if nOutputSignalBitDepth is equal to 8
	 **      nBufferSizeInBytes should be equal to 2 if nOutputSignalBitDepth is equal to 16
	 */

	/* Below based on assumed 8 bit PWM, other implementation are possible */

	/* M = 1, N = 256, 1 <= nDutyCycle <= (N-M) */

	/* Output force: nForce is mapped from [-127, 127] to [1, 255] */
	int level;

	if (nOutputSignalBitDepth == 8) {
		if (nBufferSizeInBytes != 1) {
			pr_info("%s: Only support single sample for ERM\n", __func__);
			return VIBE_E_FAIL;
		} else {
			level = (signed char)(*pForceOutputBuffer);
		}
	} else if (nOutputSignalBitDepth == 16) {
		if (nBufferSizeInBytes != 2) {
			pr_info("%s: Only support single sample for ERM\n", __func__);
			return VIBE_E_FAIL;
		} else {
			level = (signed short)(*((signed short *)(pForceOutputBuffer)));
			/* Quantize it to 8-bit value as ISA1200 only support 8-bit levels */
			level >>= 8;
		}
	} else {
		pr_info("%s: Invalid Output Force Bit Depth\n", __func__);
		return VIBE_E_FAIL;
	}

	pr_debug("%s: level = %d\n", __func__, level);

	isa1000_vib_set_level(level);

	return VIBE_S_SUCCESS;
}

/*
 ** Called to set force output frequency parameters
 */
VibeStatus ImmVibeSPI_ForceOut_SetFrequency(VibeUInt8 nActuatorIndex, VibeUInt16 nFrequencyParameterID, VibeUInt32 nFrequencyParameterValue)
{
	/* Don't needed to be implemented on Hong Mi 2A*/
	return VIBE_S_SUCCESS;
}

/*
 ** Called to save an IVT data file (pIVT) to a file (szPathName)
 */
VibeStatus ImmVibeSPI_IVTFile_Save(const VibeUInt8 * pIVT, VibeUInt32 nIVTSize, const char *szPathname)
{
	/* Don't needed to be implemented on Hong Mi 2A*/

	return VIBE_S_SUCCESS;
}

/*
 ** Called to delete an IVT file
 */
VibeStatus ImmVibeSPI_IVTFile_Delete(const char *szPathname)
{
	/* Don't needed to be implemented on Hong Mi 2A*/

	return VIBE_S_SUCCESS;
}

/*
 ** Called to get the device name (device name must be returned as ANSI char)
 */
VibeStatus ImmVibeSPI_Device_GetName(VibeUInt8 nActuatorIndex, char *szDevName, int nSize)
{
	if ((!szDevName) || (nSize < 1))
		return VIBE_E_FAIL;

	strncpy(szDevName, ISA1000_BOARD_NAME, nSize-1);
	szDevName[nSize - 1] = '\0';

	return VIBE_S_SUCCESS;
}

/*
 ** Called at initialization time to get the number of actuators
 */
VibeStatus ImmVibeSPI_Device_GetNum(void)
{
	return NUM_ACTUATORS;
}
