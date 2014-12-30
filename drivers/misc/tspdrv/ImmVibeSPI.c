/*
** =========================================================================
** Copyright (C) 2014 XiaoMi, Inc.
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
#include "../../staging/android/timed_output.h"


/*
** GPIO to ISA1000_EN pin
*/
#define GPIO_ISA1000_EN 33
#define GPIO_HAPTIC_EN 50
#define ISA1000_VIB_DEFAULT_TIMEOUT	15000

/*
** PWM to ISA1000
*/
static struct pwm_device *pwm;
/* PWM Channel to be requested*/
/* Xiaomi TODO: Please fill in the allocated PWM channel */
#define PWM_CH_ID 1
/* PWM Frequency */
/* Xiaomi TODO: Please fill in the configured PWM frequency */
/* Note: According to ISA1000 spec, the valid input PWM frequency is from
**       10kHz to 50kHz.  The PWM frequency itself should not matter, as the
**       ISA1000 motor output depends on the PWM duty cycle.
**       A usual value we are using is ~22khz
*/
#define PWM_FREQUENCY 25000
static unsigned int pwm_period_ns;

struct isa1000_vib {
	struct hrtimer vib_timer;
	struct timed_output_dev timed_dev;
	struct work_struct work;

	int timeout;
	int state;
	struct mutex lock;
} ;

static struct isa1000_vib *vib_dev;

static int isa1000_vib_set(struct isa1000_vib *vib, int on)
{
	int rc;
	int period_us = pwm_period_ns/1000;

	if (on) {
		rc = pwm_config(pwm,
						(period_us * 80/100),
						period_us);
		if (rc < 0){
			printk( "Unable to config pwm\n");
		}

		rc = pwm_enable(pwm);
		if (rc < 0){
			printk("Unable to enable pwm\n");
		}
		gpio_set_value_cansleep(GPIO_ISA1000_EN, 1);
	} else {
		gpio_set_value_cansleep(GPIO_ISA1000_EN, 0);
		pwm_disable(pwm);
	}

	return rc;
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
        int rc = 0;
        int period_us = pwm_period_ns / 1000;

        if  (level != 0) {

                /* Set PWM duty cycle corresponding to the input 'level' */
                /* Xiaomi TODO: This is only an example.
                **              Please modify for PWM on Hong Mi 2A platform
                */
                rc = pwm_config(pwm,
                                (period_us * (level + 128)) / 256,
                                period_us);
                if (rc < 0) {
                        pr_err("%s: pwm_config fail\n", __func__);
                        goto chip_dwn;
                }

                /* Enable the PWM output */
                /* Xiaomi TODO: This is only an example.
                **              Please modify for PWM on Hong Mi 2A platform
                */
                rc = pwm_enable(pwm);
                if (rc < 0) {
                        pr_err("%s: pwm_enable fail\n", __func__);
                        goto chip_dwn;
                }

                /* Assert the GPIO_ISA1000_EN to enable ISA1000 */
                /* Xiaomi TODO: This is only an example.
                **              Please modify for GPIO on Hong Mi 2A platform
                */
                gpio_set_value_cansleep(GPIO_ISA1000_EN, 1);
        } else {
                /* Deassert the GPIO_ISA1000_EN to disable ISA1000 */
                /* Xiaomi TODO: This is only an example.
                **              Please modify for GPIO on Hong Mi 2A platform
                */
                gpio_set_value_cansleep(GPIO_ISA1000_EN, 0);

                /* Disable the PWM output */
                /* Xiaomi TODO: This is only an example.
                **              Please modify for PWM on Hong Mi 2A platform
                */
                pwm_disable(pwm);
        }

        return;

chip_dwn:
        gpio_set_value_cansleep(GPIO_ISA1000_EN, 0);
}

static int isa1000_setup(void)
{
        int ret;

        ret = gpio_is_valid(GPIO_ISA1000_EN);
        if (ret) {
                ret = gpio_request(GPIO_ISA1000_EN, "gpio_isa1000_en");
                if (ret) {
                        printk(KERN_ERR "%s: gpio %d request failed\n",
                                        __func__, GPIO_ISA1000_EN);
                        return ret;
                }
        } else {
                printk(KERN_ERR "%s: Invalid gpio %d\n", __func__, GPIO_ISA1000_EN);
                return ret;
        }

	ret = gpio_is_valid(GPIO_HAPTIC_EN);
        if (ret) {
                ret = gpio_request(GPIO_HAPTIC_EN, "gpio_haptic_en");
                if (ret) {
                        printk(KERN_ERR "%s: gpio %d request failed\n",
                                        __func__, GPIO_HAPTIC_EN);
                        return ret;
                }
        } else {
                printk(KERN_ERR "%s: Invalid gpio %d\n", __func__, GPIO_ISA1000_EN);
                return ret;
        }

	gpio_direction_output(GPIO_ISA1000_EN,0);
	gpio_direction_output(GPIO_HAPTIC_EN,1);

        /* Request and reserve the PWM module for output TO ISA1000 */
        pwm = pwm_request(PWM_CH_ID, "isa1000");
        if (IS_ERR(pwm)) {
            printk(KERN_ERR "%s: pwm request failed\n", __func__);
            return PTR_ERR(pwm);
        }

        printk(KERN_INFO "%s: %s set up\n", __func__, "isa1000");
        return 0;
}

/************ ISA1000 specific section end **********/

/*
** This function is necessary for the TSP Designer Bridge.
** Called to allocate the specified amount of space in the DIAG output buffer.
** OEM must review this function and verify if the proposed function is used in their software package.
*/
void* ImmVibeSPI_Diag_BufPktAlloc(int nLength)
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
    gpio_set_value_cansleep(GPIO_ISA1000_EN, 0);

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
    gpio_set_value_cansleep(GPIO_ISA1000_EN, 1);

    return VIBE_S_SUCCESS;
}

/*
** Called at initialization time to set PWM freq, disable amp, etc...
*/
VibeStatus ImmVibeSPI_ForceOut_Initialize(void)
{
    int Ret;

    /* Kick start the ISA1200 set up */
    Ret = isa1000_setup();
    if(Ret < 0)
    {
        printk(KERN_ERR "%s, ISA1000 initialization failed\n", __func__);
        return VIBE_E_FAIL;
    }

    /* Set PWM duty cycle to 50%, i.e. output level to 0 */
    isa1000_vib_set_level(0);

    /* Set PWM frequency */
    /* Xiaomi TODO: Please change to the PWM configuration procedures for
    **              setting the PWM output frequency on
    **              the Hong Mi 2A platform
    */

    /* Compute the period of PWM cycle */
    pwm_period_ns = NSEC_PER_SEC / PWM_FREQUENCY;

    /*add google interface*/
    vib_dev = kmalloc(sizeof(struct isa1000_vib),GFP_KERNEL);
    mutex_init(&vib_dev->lock);
    INIT_WORK(&vib_dev->work, isa1000_vib_update);

    hrtimer_init(&vib_dev->vib_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    vib_dev->vib_timer.function = isa1000_vib_timer_func;
    vib_dev->timeout = ISA1000_VIB_DEFAULT_TIMEOUT;
    vib_dev->timed_dev.name = "vibrator";
    vib_dev->timed_dev.get_time = isa1000_vib_get_time;
    vib_dev->timed_dev.enable = isa1000_vib_enable;
    Ret = timed_output_dev_register(&vib_dev->timed_dev);
    if (Ret < 0){
	printk(KERN_ERR "%s, ISA1000 register failed\n", __func__);
        return VIBE_E_FAIL;
    }
    /**/

    /* Disable amp */
    ImmVibeSPI_ForceOut_AmpDisable(0);

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
VibeStatus ImmVibeSPI_ForceOut_SetSamples(VibeUInt8 nActuatorIndex, VibeUInt16 nOutputSignalBitDepth, VibeUInt16 nBufferSizeInBytes, VibeInt8* pForceOutputBuffer)
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

    if(nOutputSignalBitDepth == 8) {
        if( nBufferSizeInBytes != 1) {
	    printk("%s: Only support single sample for ERM\n",__func__);
	    return VIBE_E_FAIL;
        } else {
	    level = (signed char)(*pForceOutputBuffer);
	}
    } else if(nOutputSignalBitDepth == 16) {
        if( nBufferSizeInBytes != 2) {
	    printk("%s: Only support single sample for ERM\n",__func__);
	    return VIBE_E_FAIL;
        } else {
	    level = (signed short)(*((signed short*)(pForceOutputBuffer)));
	    /* Quantize it to 8-bit value as ISA1200 only support 8-bit levels */
	    level >>= 8;
	}
    } else {
	printk("%s: Invalid Output Force Bit Depth\n",__func__);
	return VIBE_E_FAIL;
    }

    printk(KERN_DEBUG "%s: level = %d\n", __func__, level);

    isa1000_vib_set_level( level);

    return VIBE_S_SUCCESS;
}

/*
** Called to set force output frequency parameters
*/
VibeStatus ImmVibeSPI_ForceOut_SetFrequency(VibeUInt8 nActuatorIndex, VibeUInt16 nFrequencyParameterID, VibeUInt32 nFrequencyParameterValue)
{
    /* Don't needed to be implemented on Hong Mi 2A*/
#if 0
    /*
    ** The following code is provided as sample. If enabled, it will allow device
    ** frequency parameters tuning via the ImmVibeSetDeviceKernelParameter API.
    ** Please modify as required.
    */
    switch (nFrequencyParameterID)
    {
        case VIBE_KP_CFG_FREQUENCY_PARAM1:
            /* Update frequency parameter 1 */
            break;

        case VIBE_KP_CFG_FREQUENCY_PARAM2:
            /* Update frequency parameter 2 */
            break;

        case VIBE_KP_CFG_FREQUENCY_PARAM3:
            /* Update frequency parameter 3 */
            break;

        case VIBE_KP_CFG_FREQUENCY_PARAM4:
            /* Update frequency parameter 4 */
            break;

        case VIBE_KP_CFG_FREQUENCY_PARAM5:
            /* Update frequency parameter 5 */
            break;

        case VIBE_KP_CFG_FREQUENCY_PARAM6:
            /* Update frequency parameter 6 */
            break;
    }
#endif

    return VIBE_S_SUCCESS;
}

/*
** Called to save an IVT data file (pIVT) to a file (szPathName)
*/
VibeStatus ImmVibeSPI_IVTFile_Save(const VibeUInt8 *pIVT, VibeUInt32 nIVTSize, const char *szPathname)
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
    if ((!szDevName) || (nSize < 1)) return VIBE_E_FAIL;

    strncpy(szDevName, "ISA1000", nSize-1);
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
