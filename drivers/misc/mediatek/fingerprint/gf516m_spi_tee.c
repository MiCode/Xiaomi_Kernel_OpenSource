/* Goodix's GF516M/GF318M fingerprint sensor linux driver for TEE
 *
 * 2010 - 2015 Goodix Technology.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/io.h>
#include <linux/kthread.h>

#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#include <asm/uaccess.h>
#include <linux/ktime.h>

#include <linux/interrupt.h>
#include <linux/irq.h>
//#include <mach/irqs.h>
#include <linux/completion.h>
#include <linux/gpio.h>
//#include <mach/gpio.h>
//#include <plat/gpio-cfg.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#include <linux/pm_qos.h>
#include <linux/cpufreq.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

//#include <mach/hardware.h>
//#include <mach/mt_irq.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#include <mach/mt_spi.h>
#include <mach/upmu_common.h>
#include <mach/eint.h>
#include <mach/gpio_const.h>

#include <mach/memory.h>
#include <mach/mt_clkmgr.h>
#include "mt_spi_hal.h"

#include <cust_eint.h>
#include <cust_gpio_usage.h>

#include "gf516m_spi_tee.h"

#include <mobicore_driver_api.h>
#include "drFp_Api_gf516m.h"
#include "tltest_Api.h"
#include <linux/dev_info.h>
/*
 * This supports access to SPI devices using normal userspace I/O calls.
 * Note that while traditional UNIX/POSIX I/O semantics are half duplex,
 * and often mask message boundaries, full SPI support requires full duplex
 * transfers.  There are several kinds of internal message boundaries to
 * handle chipselect management and other protocol options.
 *
 * SPI has a character major number assigned.  We allocate minor numbers
 * dynamically using a bitmask.  You must use hotplug tools, such as udev
 * (or mdev with busybox) to create and destroy the /dev/spidevB.C device
 * nodes, since there is no fixed association of minor numbers with any
 * particular SPI bus or device.
 */

/*spi device name*/
//#define SPI_DEV_NAME	 "spidev"
#define SPI_DEV_NAME   "fp_spi"
/*device name after register in charater*/
#define DEV_NAME "goodix_fp_spi"

#define CHRD_DRIVER_NAME		"goodix_fp"
#define CLASS_NAME			"goodix_fp_spi"
#define SPIDEV_MAJOR			158	/* assigned */
#define N_SPI_MINORS			32	/* ... up to 256 */
static DECLARE_BITMAP(minors, N_SPI_MINORS);

#define GF516M_SPI_VERSION "gf516m_spi_tee_v0.9"
#define GF_FP_PRODUCT_ID "GFx18M"

/**************************feature control******************************/
//#define GF516M_FW_UPDATE
#define GF516M_WATCHDOG
#define GF516M_PROBE_CHIP
//#define GF516M_SLEEP_FEATURE   //exclusive with GF516M_FF_FEATURE
#define GF516M_FF_FEATURE
//#define GF516M_FF_DRIVER_TEST    //fingerflash feature local driver test only
//#define GF516M_DRIVER_CLEAR_DOWN_UP_IRQ   //enable for local driver test only

/***********************GPIO setting port layer*************************/
/* customer hardware port layer, please change according to customer's hardware */
#ifndef GPIO_FP_INT_PIN
#define GPIO_FP_INT_PIN			(GPIO3 | 0x80000000)  //Jaosn Aug-7-2015 //(GPIO5 | 0x80000000)
#define GPIO_FP_INT_PIN_M_GPIO	GPIO_MODE_00
#define GPIO_FP_INT_PIN_M_EINT	GPIO_FP_INT_PIN_M_GPIO
#endif

#define GPIO_FP_SPICLK_PIN		   (GPIO166 | 0x80000000)
#define GPIO_FP_SPICLK_PIN_M_GPIO  GPIO_MODE_00
#define GPIO_FP_SPICLK_PIN_M_PWM  GPIO_MODE_03
#define GPIO_FP_SPICLK_PIN_M_SPI_CK   GPIO_MODE_01

#define GPIO_FP_SPIMISO_PIN			(GPIO167 | 0x80000000)
#define GPIO_FP_SPIMISO_PIN_M_GPIO	GPIO_MODE_00
#define GPIO_FP_SPIMISO_PIN_M_PWM  GPIO_MODE_03
#define GPIO_FP_SPIMISO_PIN_M_SPI_MI   GPIO_MODE_01

#define GPIO_FP_SPIMOSI_PIN			(GPIO168 | 0x80000000)
#define GPIO_FP_SPIMOSI_PIN_M_GPIO	GPIO_MODE_00
#define GPIO_FP_SPIMOSI_PIN_M_MDEINT  GPIO_MODE_02
#define GPIO_FP_SPIMOSI_PIN_M_PWM  GPIO_MODE_03
#define GPIO_FP_SPIMOSI_PIN_M_SPI_MO   GPIO_MODE_01

#define GPIO_FP_SPICS_PIN		  (GPIO169 | 0x80000000)
#define GPIO_FP_SPICS_PIN_M_GPIO  GPIO_MODE_00
#define GPIO_FP_SPICS_PIN_M_MDEINT	GPIO_MODE_02
#define GPIO_FP_SPICS_PIN_M_PWM  GPIO_MODE_03
#define GPIO_FP_SPICS_PIN_M_SPI_CS	 GPIO_MODE_01

#define GPIO_FP_RESET_PIN		   (GPIO115 | 0x80000000) //Jason Aug-7-2015 //(GPIO122 | 0x80000000)
#define GPIO_FP_RESET_PIN_M_GPIO  GPIO_MODE_00

//Jason Aug-7-2015 
#define GPIO_FP_POWER_PIN		(GPIO94 | 0x80000000)
#define GPIO_FP_POWER_PIN_M_GPIO  GPIO_MODE_00

#define GPIO_FP_SPI_BYPASS_PIN		   (GPIO133 | 0x80000000)
#define GPIO_FP_SPI_BYPASS_M_GPIO  GPIO_MODE_00

#ifndef	CUST_EINT_FP_EINT_NUM
#define CUST_EINT_FP_EINT_NUM			   3    //Jaosn Aug-7-2015  //5
#define CUST_EINT_FP_EINT_DEBOUNCE_CN	   0
#define CUST_EINT_FP_EINT_TYPE			   EINTF_TRIGGER_RISING//EINTF_TRIGGER_LOW//CUST_EINTF_TRIGGER_RISING
#define CUST_EINT_FP_EINT_DEBOUNCE_EN	   0//CUST_EINT_DEBOUNCE_DISABLE
#endif

#ifndef GF516M_INPUT_HOME_KEY
#define GF516M_INPUT_HOME_KEY 353

#define GF516M_INPUT_MENU_KEY  KEY_MENU
#define GF516M_INPUT_BACK_KEY  KEY_BACK

//#define GF516M_INPUT_UP_KEY  KEY_UP
//#define GF516M_INPUT_DOWN_KEY  KEY_DOWN

#define GF516M_FF_KEY  KEY_POWER
#endif



#define GF516M_NVG_DOWN_KEY  KEY_LEFT
#define GF516M_NVG_UP_KEY  KEY_RIGHT
#define GF516M_NVG_LEFT_KEY  KEY_DOWN
#define GF516M_NVG_RIGHT_KEY  KEY_UP
#define GF516M_NVG_TOUCH_KEY  353




/***********************GPIO setting port layer*************************/


struct gf516m_dev *g_gf516m_dev = NULL;

/* align=2, 2 bytes align */
/* align=4, 4 bytes align */
/* align=8, 8 bytes align */
#define ROUND_UP(x, align)		(x+(align-1))&~(align-1)

/**************************debug******************************/
#define ERR_LOG  (0)
#define INFO_LOG (1)
#define DEBUG_LOG (2)

/* debug log setting */
u8 g_debug_level = INFO_LOG;

#define gf516m_debug(level, fmt, args...) do{ \
			if(g_debug_level >= level) {\
				printk("[gf516m] " fmt, ##args); \
			} \
		}while(0)

#define FUNC_ENTRY()  gf516m_debug(DEBUG_LOG, "%s, %d, entry\n", __func__, __LINE__)
#define FUNC_EXIT()  gf516m_debug(DEBUG_LOG, "%s, %d, exit\n", __func__, __LINE__)

/*************************************************************/
static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

static unsigned bufsiz = (150*150);
static int get_algo_result = 0;
static int nav_status = 1000;
static int rec_event = 0;  //avoid to send  nav  event  when  unlocking;

module_param(bufsiz, uint, S_IRUGO);
MODULE_PARM_DESC(bufsiz, "data bytes in biggest supported SPI message");


/******************function declaration************************/

static void gf516m_enable_watchdog(struct gf516m_dev *gf516m_dev);
static void gf516m_disable_watchdog(struct gf516m_dev *gf516m_dev);

#ifdef GF516M_WATCHDOG
#define GF516M_TIMER_EXPIRE (2 * HZ)

static void gf516m_timer_work(struct work_struct *work);
static void gf516m_timer_func(unsigned long arg);
#endif

static void gf516m_enable_irq(struct gf516m_dev *gf516m_dev);
static void gf516m_disable_irq(struct gf516m_dev *gf516m_dev);


/* -------------------------------------------------------------------- */
/* timer function								*/
/* -------------------------------------------------------------------- */
#define TIME_START	   0
#define TIME_STOP	   1

static long int prev_time, cur_time;

long int kernel_time(unsigned int step)
{
	cur_time = ktime_to_us(ktime_get());
	if(step == TIME_START)
	{
		prev_time = cur_time;
		return 0;
	}
	else if(step == TIME_STOP)
	{
		gf516m_debug(DEBUG_LOG, "%s, use: %ld us\n", __func__, (cur_time - prev_time));
		return (cur_time - prev_time);
	}
	else
	{
		prev_time = cur_time;
		return -1;
	}
}

/* -------------------------------------------------------------------- */
/* trustonic TEE related								*/
/* -------------------------------------------------------------------- */
static u32 mc_deviceId = MC_DEVICE_ID_DEFAULT;
static const struct mc_uuid_t ta_uuid = TL_TEST_UUID;	//TEST TA UUID
static const struct mc_uuid_t dr_uuid = DR_FP_UUID;  //fp secure driver UUID

static struct mc_session_handle ta_SessionHandle = {0};
static struct mc_session_handle dr_SessionHandle = {0};

static tciTestMessage_t *pTci = NULL;
static dciMessage_t *pDci  = NULL;

int gf516m_load_TA(void)
{
	enum mc_result mcRet = MC_DRV_OK;

#if 0
	mcRet = mc_open_device(mc_deviceId);
	if(MC_DRV_OK != mcRet){
		gf516m_debug(ERR_LOG, "TEE: mc_open_device failed, mcRet: %d\n", mcRet);
		return -EFAULT;
	}
	gf516m_debug(INFO_LOG, "TEE: mc_open_device success, mcRet: %d\n", mcRet);
#endif

	/* Allocating WSM for TCI */
	mcRet = mc_malloc_wsm(mc_deviceId, 0, sizeof(tciTestMessage_t), (u8 **) &pTci, 0);
	if(MC_DRV_OK != mcRet){
		gf516m_debug(ERR_LOG, "TEE: mc_malloc_wsm allocate TCI failed, mcRet: %d\n", mcRet);
		mc_close_device(mc_deviceId);
		return -EFAULT;
	}
	gf516m_debug(INFO_LOG, "TEE: mc_malloc_wsm success, mcRet: %d\n", mcRet);

	/* Open session to the secure driver*/
	memset(&ta_SessionHandle, 0, sizeof(ta_SessionHandle));
	ta_SessionHandle.device_id = mc_deviceId;
	mcRet = mc_open_session(&ta_SessionHandle, &ta_uuid, (u8 *)pTci, sizeof(tciTestMessage_t));
	if(MC_DRV_OK != mcRet){
		mc_free_wsm(mc_deviceId, (u8 *)pTci);
		mc_close_device(mc_deviceId);
		pTci = NULL;
		gf516m_debug(ERR_LOG, "TEE: mc_open_session failed, mcRet: %d\n", mcRet);
		return -EFAULT;
	}
	gf516m_debug(INFO_LOG, "TEE: mc_open_session success, mcRet: %d\n", mcRet);
	return mcRet;
}

int gf516m_send_cmd_TA(u32 cmd)
{
	enum mc_result mcRet = MC_DRV_OK;

	gf516m_debug(INFO_LOG, "gf516m_send_cmd_TA\n");

	if(NULL == pTci) {
		gf516m_debug(ERR_LOG, "pTci not exist\n");
		return -ENODEV;
	}

	pTci->cmd_test.header.commandId = (tciCommandId_t)cmd;
	pTci->cmd_test.len = 0;

	gf516m_debug(DEBUG_LOG, "mc_notify\n");

	mcRet = mc_notify(&ta_SessionHandle);
	if (MC_DRV_OK != mcRet) {
		gf516m_debug(ERR_LOG, "mc_notify failed: %d", mcRet);
		goto exit;
	}

	gf516m_debug(DEBUG_LOG, "SPI mc_wait_notification\n");
	mcRet = mc_wait_notification(&ta_SessionHandle, -1);
	if (MC_DRV_OK != mcRet) {
		gf516m_debug(ERR_LOG, "SPI mc_wait_notification failed: %d", mcRet);
		goto exit;
	}

	if(RSP_ID(cmd) != pTci->rsp_test.header.responseId){
		gf516m_debug(ERR_LOG, "TEE: ta not send a response %d\n", pTci->rsp_test.header.responseId);
		return -EBUSY;
	}
	if(MC_DRV_OK != pTci->rsp_test.header.returnCode){
		gf516m_debug(ERR_LOG, "TEE: ta not send a valid return code %d\n", pTci->rsp_test.header.returnCode);
		return -EBUSY;
	}

exit:
	if (MC_DRV_OK != mcRet){
		return -ENOSPC;
	}

	return mcRet;
}

int gf516m_close_TA(void)
{
	enum mc_result mcRet = MC_DRV_OK;

	mcRet = mc_close_session(&ta_SessionHandle);
	if(MC_DRV_OK != mcRet){
		 gf516m_debug(ERR_LOG, "TEE: mc_close_session failed, mcRet: %d\n", mcRet);
		 return -EFAULT;
	}
	gf516m_debug(INFO_LOG, "TEE: mc_close_session success, mcRet: %d\n", mcRet);

	mcRet = mc_free_wsm(mc_deviceId, (u8 *)pTci);
	if(MC_DRV_OK != mcRet){
		gf516m_debug(ERR_LOG, "TEE: mc_free_wsm free TCI failed, mcRet: %d\n", mcRet);
		return -EFAULT;
	}
	pTci = NULL;
	gf516m_debug(INFO_LOG, "TEE: mc_free_wsm success, mcRet: %d\n", mcRet);

#if 0
	mcRet = mc_close_device(mc_deviceId);
	if(MC_DRV_OK != mcRet){
		gf516m_debug(ERR_LOG, "TEE: mc_close_device failed, mcRet: %d\n", mcRet);
		return -EFAULT;
	}
	gf516m_debug(INFO_LOG, "TEE: mc_close_device success, mcRet: %d\n", mcRet);
#endif
	return mcRet;
}


int gf516m_load_secdrv(void)
{
    int i = 0;
    enum mc_result mcRet = MC_DRV_OK;

    for (i = 0; i < 30; i++) { // retry 30 times to load finger print secure driver
        mcRet = mc_open_device(mc_deviceId);
        if(MC_DRV_OK != mcRet){
                gf516m_debug(ERR_LOG, "TEE: mc_open_device failed, mcRet: %d\n", mcRet);
            mcRet = -EFAULT;
            continue;
        }
        gf516m_debug(INFO_LOG, "TEE: mc_open_device success, mcRet: %d\n", mcRet);

        /* Allocating WSM for DCI */
        mcRet = mc_malloc_wsm(mc_deviceId, 0, sizeof(dciMessage_t), (u8 **) &pDci, 0);
        if(MC_DRV_OK != mcRet){
                gf516m_debug(ERR_LOG, "TEE: mc_malloc_wsm allocate TCI failed, mcRet: %d\n", mcRet);
                mc_close_device(mc_deviceId);
            mcRet = -EFAULT;
            continue;
        }
        gf516m_debug(INFO_LOG, "TEE: mc_malloc_wsm success, mcRet: %d, pDci address: 0x%p\n", mcRet, (u8 *)pDci);

        msleep(1000);
        gf516m_debug(INFO_LOG, "%s, wait for 1 second and then open session\n", __func__);
        /* Open session to the secure driver*/
        memset(&dr_SessionHandle, 0, sizeof(dr_SessionHandle));
        dr_SessionHandle.device_id = mc_deviceId;
        mcRet = mc_open_session(&dr_SessionHandle, &dr_uuid, (u8 *)pDci, sizeof(dciMessage_t));
        if (MC_DRV_OK == mcRet) {
            gf516m_debug(INFO_LOG, "TEE: mc_open_session success, mcRet: %d\n", mcRet);
            break;
        }
        mc_free_wsm(mc_deviceId, (u8 *) pDci);
        mc_close_device(mc_deviceId);
        pDci = NULL;
        gf516m_debug(ERR_LOG, "TEE: mc_open_session failed, mcRet: %d\n", mcRet);
        mcRet = -EFAULT;
    }

	return mcRet;
}

int gf516m_close_secdrv(void)
{
	enum mc_result mcRet = MC_DRV_OK;

	mcRet = mc_close_session(&dr_SessionHandle);
	if(MC_DRV_OK != mcRet){
		gf516m_debug(ERR_LOG, "TEE: mc_close_session failed, mcRet: %d\n", mcRet);
		return -EFAULT;
	}
	gf516m_debug(INFO_LOG, "TEE: mc_close_session success, mcRet: %d\n", mcRet);

	mcRet = mc_free_wsm(mc_deviceId, (u8 *)pDci);
	if(MC_DRV_OK != mcRet){
		gf516m_debug(ERR_LOG, "TEE: mc_free_wsm free TCI failed, mcRet: %d\n", mcRet);
		return -EFAULT;
	}
	pDci = NULL;
	gf516m_debug(INFO_LOG, "TEE: mc_free_wsm success, mcRet: %d\n", mcRet);

	mcRet = mc_close_device(mc_deviceId);
	if(MC_DRV_OK != mcRet){
		gf516m_debug(ERR_LOG, "TEE: mc_close_device failed, mcRet: %d\n", mcRet);
		return -EFAULT;
	}
	gf516m_debug(INFO_LOG, "TEE: mc_close_device success, mcRet: %d\n", mcRet);

	return mcRet;
}

int gf516m_send_cmd_secdrv(u32 cmd)
{
	enum mc_result mcRet;

	if(cmd != GF516M_SECDEV_CMD_RW_ADDR){
		gf516m_debug(DEBUG_LOG, "%s, gf516m_send_cmd_secdrv, cmd=%d\n", __func__, cmd);
	}

	if(NULL == pDci) {
		gf516m_debug(ERR_LOG, "%s, pDci not exist\n", __func__);
		return -ENODEV;
	}

	//Jason Aug-24-2015 enable the spi clk when start the spi transfer for mi.
	enable_clock(MT_CG_PERI_SPI0, "spi");

	pDci->command.header.commandId = (dciCommandId_t)cmd;

	//gf516m_debug(DEBUG_LOG, "%s, mc_notify\n", __func__);
	mcRet = mc_notify(&dr_SessionHandle);
	if (MC_DRV_OK != mcRet) {
		gf516m_debug(ERR_LOG, "%s, mc_notify failed: %d", __func__, mcRet);
		goto exit;
	}

	//gf516m_debug(DEBUG_LOG,"%s, SPI mc_wait_notification\n", __func__);
	mcRet = mc_wait_notification(&dr_SessionHandle, -1);
	if (MC_DRV_OK != mcRet) {
		gf516m_debug(ERR_LOG, "%s, SPI mc_wait_notification failed: %d", __func__, mcRet);
		goto exit;
	}

	if(RSP_ID(cmd) != pDci->response.header.responseId){
		gf516m_debug(ERR_LOG, "TEE: secdr not send a response %d\n", pDci->response.header.responseId);
		goto exit;
	}
	if(MC_DRV_OK != pDci->response.header.returnCode){
		gf516m_debug(ERR_LOG, "TEE: secdr not send a valid return code %d\n", pDci->response.header.returnCode);
		goto exit;;
	}

	//Jason Aug-24-2015 Disable the spi clk after  spi transfer for mi.
	//disable_clock(MT_CG_PERI_SPI0, "spi");

exit:
	disable_clock(MT_CG_PERI_SPI0, "spi");
	if (MC_DRV_OK != mcRet){
		return -ENOSPC;
	}else{
		return mcRet;
	}
}


/* -------------------------------------------------------------------- */
/* fingerprint chip hardware configuration								  */
/* -------------------------------------------------------------------- */
static void gf516m_hw_power(u8 bonoff)
{
	//TODO: LDO configure
#if 0
	if(bonoff)
		hwPowerOn(MT6331_POWER_LDO_VIBR, VOL_2800, "fingerprint");
	else
		hwPowerDown(MT6331_POWER_LDO_VIBR, "fingerprint");
#endif

//Jason Aug-7-2015
	if (bonoff) {
		mt_set_gpio_mode(GPIO_FP_POWER_PIN, GPIO_FP_POWER_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_FP_POWER_PIN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_FP_POWER_PIN, GPIO_OUT_ONE);
	} else {
		mt_set_gpio_out(GPIO_FP_POWER_PIN, GPIO_OUT_ZERO);
	}

}

static void gf516m_bypass_flash_cfg(void)
{
	//TODO: by pass flash IO config, default connect to GND
	return;
}

/*Confure the IRQ pin for GF516M irq if necessary*/
inline static void gf516m_irq_cfg(void)
{
	/*Config IRQ pin, referring to platform.*/
	mt_set_gpio_mode(GPIO_FP_INT_PIN,GPIO_FP_INT_PIN_M_EINT);
	mt_set_gpio_pull_enable(GPIO_FP_INT_PIN, GPIO_PULL_DISABLE);
	//mt_set_gpio_pull_select(GPIO_FP_INT_PIN, GPIO_PULL_DOWN);
	mt_set_gpio_dir(GPIO_FP_INT_PIN, GPIO_DIR_IN);

	mt_eint_set_hw_debounce(CUST_EINT_FP_EINT_NUM, CUST_EINT_FP_EINT_DEBOUNCE_CN);
#if 0
	mt_eint_registration(CUST_EINT_FP_EINT_NUM, CUST_EINT_FP_EINT_TYPE, gf516m_irq, 0);
	mt_eint_unmask(CUST_EINT_FP_EINT_NUM);
#endif

	return;
}

/*Only could be called before switch SPI to TEE */
inline static int gf516m_pull_miso_high(struct gf516m_dev *gf516m_dev)
{


	mt_set_gpio_mode(GPIO_FP_SPIMISO_PIN, GPIO_FP_SPIMISO_PIN_M_GPIO);
	//mt_set_gpio_dir(GPIO_FP_SPIMISO_PIN, GPIO_DIR_OUT);

	//mt_set_gpio_out(GPIO_FP_SPIMISO_PIN, GPIO_OUT_ONE);
	mt_set_gpio_dir(GPIO_FP_SPIMISO_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_FP_SPIMISO_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_FP_SPIMISO_PIN, GPIO_PULL_UP);

        //mdelay(20);

	gf516m_debug(INFO_LOG, "%s, gf516m_pull_miso_high success\n", __func__);

        /* init GPIO for fp RESET and INTTERUPT */
        mt_set_gpio_mode(GPIO_FP_RESET_PIN, GPIO_FP_RESET_PIN_M_GPIO);
        mt_set_gpio_dir(GPIO_FP_RESET_PIN, GPIO_DIR_OUT);

        mt_set_gpio_out(GPIO_FP_RESET_PIN, GPIO_OUT_ZERO);
        mdelay(5);
        mt_set_gpio_out(GPIO_FP_RESET_PIN, GPIO_OUT_ONE);

	gf516m_debug(INFO_LOG, "%s, gf516m_pull_miso_high set reset pin\n", __func__);
	return EACCES;
}

/*Confure the IRQ pin for GF516M irq if necessary*/
inline static void gf516m_gpio_cfg(void)
{
        mt_set_gpio_mode(GPIO_FP_SPIMISO_PIN, GPIO_FP_SPIMISO_PIN_M_GPIO);
        mt_set_gpio_dir(GPIO_FP_SPIMISO_PIN, GPIO_DIR_IN);
        mt_set_gpio_pull_enable(GPIO_FP_SPIMISO_PIN, GPIO_PULL_DISABLE);

        /* init GPIO for SPI use */
        mt_set_gpio_mode(GPIO_FP_SPICLK_PIN, GPIO_FP_SPICLK_PIN_M_SPI_CK);
        mt_set_gpio_mode(GPIO_FP_SPIMISO_PIN, GPIO_FP_SPIMISO_PIN_M_SPI_MI);
        mt_set_gpio_mode(GPIO_FP_SPIMOSI_PIN, GPIO_FP_SPIMOSI_PIN_M_SPI_MO);
        mt_set_gpio_mode(GPIO_FP_SPICS_PIN, GPIO_FP_SPICS_PIN_M_SPI_CS);

        /* init GPIO for fp RESET and INTTERUPT */
        mt_set_gpio_mode(GPIO_FP_RESET_PIN, GPIO_FP_RESET_PIN_M_GPIO);
        mt_set_gpio_dir(GPIO_FP_RESET_PIN, GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_FP_RESET_PIN, GPIO_OUT_ONE);

	//TODO: consider place to enable SPI clock!!!
	//enable_clock(MT_CG_PERI_SPI0, "spi");
	gf516m_debug(INFO_LOG, "%s, gf516m_gpio_cfg success\n", __func__);
	return;
}

/********************************************************************
*CPU output low level in RST pin to reset GF516M. This is the MUST action for GF516M.
*Take care of this function. IO Pin driver strength / glitch and so on.
********************************************************************/
inline static void gf516m_hw_reset(u8 delay)
{
        mt_set_gpio_out(GPIO_FP_RESET_PIN, GPIO_OUT_ZERO);
        mdelay(5);
        mt_set_gpio_out(GPIO_FP_RESET_PIN, GPIO_OUT_ONE);
        if(delay){
                mdelay(delay);
        }

        gf516m_debug(INFO_LOG, "[%s] : gf516m reset success.\n", __func__);
}

/* -------------------------------------------------------------------- */
/* spi read/write functions, only for debug use																	  */
/* -------------------------------------------------------------------- */
int gf516m_spi_write_byte(struct gf516m_dev *gf516m_dev, u16 addr, u8 value)
{
	int ret = 0;

	if(gf516m_dev->secspi_init_ok){
		mutex_lock(&gf516m_dev->dci_lock);
		pDci->rw_data.len = 1;
		pDci->rw_data.direction = 1;
		pDci->rw_data.speed = 0;
		pDci->rw_data.addr = addr;

		pDci->rw_data.data[0] = value;
		ret = gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_RW_ADDR);

		mutex_unlock(&gf516m_dev->dci_lock);
		//gf516m_debug(DEBUG_LOG, "%s: write address %x: %d, ret:%d", __func__, addr, value, ret);
	}else{
		ret = EACCES;
	}
	return ret;
}

/******************************************************************
*gf516m_addr: SPI device start address
*data_len: byte length to write
*tx_buf: write buffer used for SPI transfer, no offset for true data, big endian!!!!
*******************************************************************/
int gf516m_spi_write_bytes(struct gf516m_dev *gf516m_dev,
				u16 addr, u32 data_len, u8 *tx_buf)
{
	int ret = 0;
#if 1
	gf516m_debug(ERR_LOG, "Not support batch write in normal driver\n");
#else
	if(gf516m_dev->secspi_init_ok){
		mutex_lock(&gf516m_dev->dci_lock);
		pDci->rw_data.len = data_len;
		pDci->rw_data.direction = 1;
		pDci->rw_data.speed = 0;
		pDci->rw_data.addr = addr;

		memcpy(pDci->rw_data.data, tx_buf, data_len);

		ret = gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_RW_ADDR);
		mutex_unlock(&gf516m_dev->dci_lock);
		//gf516m_debug(DEBUG_LOG, "%s: write address %x: %d, ret:%d", __func__, addr, value, ret);
	}else{
		ret = EACCES;
	}
#endif
	return ret;
}

/* -------------------------------------------------------------------- */
/* spi read/write functions, only for debug use																	  */
/* -------------------------------------------------------------------- */
int gf516m_spi_read_byte(struct gf516m_dev *gf516m_dev, u16 addr, u8 *value)
{
	int ret = 0;

	if(gf516m_dev->secspi_init_ok){
		mutex_lock(&gf516m_dev->dci_lock);
		pDci->rw_data.len = 1;
		pDci->rw_data.direction = 0;
		pDci->rw_data.speed = 0;
		pDci->rw_data.addr = addr;

		ret = gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_RW_ADDR);
		*value = pDci->rw_data.data[0];
		mutex_unlock(&gf516m_dev->dci_lock);
		//gf516m_debug(DEBUG_LOG, "%s: read address %x: %d", __func__, addr, *value);
	}else{
		ret = EACCES;
	}
	return ret;
}

/******************************************************************
*gf516m_addr: SPI device start address
*data_len: byte length to read
*rx_buf: read buffer used for SPI transfer, no offset for true data, big endian!!!
*******************************************************************/
int gf516m_spi_read_bytes(struct gf516m_dev *gf516m_dev,
				u16 addr, u32 data_len, u8 *rx_buf)
{
	int ret = 0;
#if 1
	gf516m_debug(ERR_LOG, "Not support batch read in normal driver\n");
#else
	if(gf516m_dev->secspi_init_ok){
		mutex_lock(&gf516m_dev->dci_lock);
		pDci->rw_data.len = data_len;
		pDci->rw_data.direction = 0;
		pDci->rw_data.speed = 0;
		pDci->rw_data.addr = addr;

		ret = gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_RW_ADDR);
		memcpy(rx_buf, pDci->rw_data.data, data_len);

		mutex_unlock(&gf516m_dev->dci_lock);
	}else{
		ret = EACCES;
	}
#endif
	return ret;
}

#ifdef GF516M_PROBE_CHIP
static u8 tmp_buf[24];
/* read out data start from offset 4!! */
static int gf516m_spi_read_bytes_nwd(struct gf516m_dev *gf516m_dev,
				u16 addr, u32 data_len, u8 *rx_buf)
{
	struct spi_message msg;
	struct spi_transfer *xfer;

	if(gf516m_dev->probe_finish){
		gf516m_debug(ERR_LOG, "%s, SPI is unreachable after probe finished!\n", __func__);
		return -ENOMEM;
	}

	xfer = kzalloc(sizeof(*xfer)*2, GFP_KERNEL);
	if( xfer == NULL){
		gf516m_debug(ERR_LOG, "%s, No memory for spi_transfer\n", __func__);
		return -ENOMEM;
	}

	/*send gf516m command to device.*/
	spi_message_init(&msg);
	rx_buf[0] = GF516M_W;
	rx_buf[1] = (u8)((addr >> 8)&0xFF);
	rx_buf[2] = (u8)(addr & 0xFF);
	xfer[0].tx_buf = rx_buf;
	xfer[0].len = 3;
	xfer[0].delay_usecs = 5;
	spi_message_add_tail(&xfer[0], &msg);

	/*if wanted to read data from gf516m.
	*Should write Read command to device
	*before read any data from device.
	*/
	//memset(rx_buf, 0xff, data_len);
	spi_sync(gf516m_dev->spi, &msg);
	disable_clock(MT_CG_PERI_SPI0, "spi");
	spi_message_init(&msg);

	memset(rx_buf,0xff,data_len + GF516M_RDATA_OFFSET);

        rx_buf[0] = GF516M_R;
        xfer[1].tx_buf = &rx_buf[0];

        xfer[1].rx_buf = &rx_buf[0];
        xfer[1].len = data_len + 1;
        xfer[1].delay_usecs = 5;
        spi_message_add_tail(&xfer[1], &msg);

	spi_sync(gf516m_dev->spi, &msg);
	disable_clock(MT_CG_PERI_SPI0, "spi");
	gf516m_debug(DEBUG_LOG, "%s finished\n", __func__);

	kfree(xfer);
	if(xfer != NULL){
		xfer = NULL;
	}

	return EACCES;
}

static int gf516m_spi_write_byte_nwd(struct gf516m_dev *gf516m_dev,
				u16 addr, u8 value)
{
	struct spi_message msg;
	struct spi_transfer *xfer;

	if(gf516m_dev->probe_finish){
		gf516m_debug(ERR_LOG, "%s, SPI is unreachable after probe finished!\n", __func__);
		return -ENOMEM;
	}

	xfer = kzalloc(sizeof(*xfer), GFP_KERNEL);
	if( xfer == NULL){
		gf516m_debug(ERR_LOG, "%s, No memory for spi_transfer\n", __func__);
		return -ENOMEM;
	}

	/*send gf516m command to device.*/
	spi_message_init(&msg);
	tmp_buf[0] = GF516M_W;
	tmp_buf[1] = (u8)((addr >> 8)&0xFF);
	tmp_buf[2] = (u8)(addr & 0xFF);
	tmp_buf[3] = value;

	xfer[0].tx_buf = tmp_buf;
	xfer[0].len = 3+1;
	xfer[0].delay_usecs = 5;
	spi_message_add_tail(&xfer[0], &msg);
	spi_sync(gf516m_dev->spi, &msg);
	gf516m_debug(DEBUG_LOG, "%s finished\n", __func__);

	kfree(xfer);
	if(xfer != NULL){
		xfer = NULL;
	}

	return EACCES;
}

static int gf516m_check_9p_chip(struct gf516m_dev *gf516m_dev)
{
	u32 time_out = 0;
	u8 reg_value[10] = {0};

	if(gf516m_dev->probe_finish){
		gf516m_debug(ERR_LOG, "%s, SPI is unreachable after probe finished!\n", __func__);
		return -ENOMEM;
	}

        do{
                memset(reg_value, 0xFF, 10);
                /* read data start from offset 4 */
                gf516m_spi_read_bytes_nwd(gf516m_dev, 0x4220, 4, tmp_buf);
                gf516m_debug(INFO_LOG, "%s, 9p chip version is 0x%x, 0x%x, 0x%x, 0x%x\n", __func__, 
                                        tmp_buf[GF516M_RDATA_OFFSET], tmp_buf[GF516M_RDATA_OFFSET+1],
                                        tmp_buf[GF516M_RDATA_OFFSET+2], tmp_buf[GF516M_RDATA_OFFSET+3]);

                time_out++;
                /* 9P MP chip version is 0x00900802*/
                if ((0x00 == tmp_buf[GF516M_RDATA_OFFSET+3]) && (0x90 == tmp_buf[GF516M_RDATA_OFFSET+2]) && (0x08 == tmp_buf[GF516M_RDATA_OFFSET+1])) {
                        gf516m_debug(INFO_LOG, "%s, 9p chip version check pass, time_out=%d\n", __func__, time_out);
                        return 0;
                }
                mdelay(2);
	}while(time_out<40);

	gf516m_debug(INFO_LOG, "%s, 9p chip version read failed, time_out=%d\n", __func__, time_out);
	return -ENOMEM;
}

static int gf516m_fw_upgrade_prepare(struct gf516m_dev *gf516m_dev)
{
        gf516m_spi_write_byte_nwd(gf516m_dev, 0x5081, 0x00);
        /* hold mcu and DSP first */
        gf516m_spi_write_byte_nwd(gf516m_dev, 0x4180, 0x0c);
        gf516m_spi_read_bytes_nwd(gf516m_dev, 0x4180, 1, tmp_buf);
        if(tmp_buf[GF516M_RDATA_OFFSET] == 0x0c) {
                /* 0. enable power supply for DSP and MCU */
                gf516m_spi_write_byte_nwd(gf516m_dev, 0x4010, 0x0);

                /*1.Close watch-dog,  clear cache enable(write 0 to 0x40B0)*/
                gf516m_spi_write_byte_nwd(gf516m_dev, 0x40B0, 0x00);
                gf516m_spi_write_byte_nwd(gf516m_dev, 0x404B, 0x00);
        } else {
                gf516m_debug(ERR_LOG, "%s, Reg = 0x%x, expect 0x0c\n", __func__, tmp_buf[4]);
                return -1;
        }

        gf516m_debug(INFO_LOG, "%s, fw upgrade prepare finished\n", __func__);
        return 0;
}
#endif

/* functions add for read fw version, return byte 0 of fw version */
static u8 gf516m_read_fw_version(struct gf516m_dev *gf516m_dev)
{
	u8 value;
	u8 vendor_id = 0;

	mutex_lock(&gf516m_dev->dci_lock);
	//u8 version = 0;
	pDci->fw_version.len = 10;
	pDci->fw_version.addr = 0x8000;
	memset(pDci->fw_version.fw, 0x00, 10);

	/*make sure gf516m_dev->secspi_init_ok is 1 before call*/
	gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_READ_FW_VERSION);
	gf516m_debug(INFO_LOG, "%s: through DCI 0x%x:0x%x:0x%x:0x%x:0x%x:0x%x, %02d.%02d.%02d\n", __func__,
		pDci->fw_version.fw[0], pDci->fw_version.fw[1], pDci->fw_version.fw[2],
		pDci->fw_version.fw[3], pDci->fw_version.fw[4], pDci->fw_version.fw[5],
		pDci->fw_version.fw[7], pDci->fw_version.fw[8], pDci->fw_version.fw[9]);

	value = pDci->fw_version.fw[0];
	vendor_id = pDci->fw_version.vendor_id[0];
	gf516m_dev->vendor_id = vendor_id;
	memcpy(gf516m_dev->chip_version, pDci->fw_version.fw, 10);
	
	gf516m_debug(INFO_LOG, "[%s]: pDci->fw_version.vendor_id[0] is 0x%x,  gf516m_dev->vendor_id = 0x%x.\n", __func__,  pDci->fw_version.vendor_id[0], gf516m_dev->vendor_id);
	mutex_unlock(&gf516m_dev->dci_lock);

#if 0
	if(value != 0x47)
	{
		gf516m_debug(ERR_LOG, "[ERR] %s device detect error!! version = 0x%x\n", __func__, version);
		return -1;
	}
		gf516m_debug(INFO_LOG, "%s read fw version through spi_read_function success!\n", __func__);
#endif

	return value;
}

static int gf516m_get_mode(struct gf516m_dev *gf516m_dev, u8 *mode)
{
	mutex_lock(&gf516m_dev->dci_lock);
	pDci->mode.rw_flag = 0;
	gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_GET_SET_MODE);
	*mode = pDci->mode.mode;
	mutex_unlock(&gf516m_dev->dci_lock);
	return 0;
}

static int gf516m_set_mode(struct gf516m_dev *gf516m_dev, u8 mode)
{
        mutex_lock(&gf516m_dev->dci_lock);
        pDci->mode.rw_flag = 1;
        pDci->mode.mode = mode;
        gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_GET_SET_MODE);
        mutex_unlock(&gf516m_dev->dci_lock);
        gf516m_debug(INFO_LOG, "[%s]:set mode = 0x%x.\n", __func__, mode);
        return 0;
}

static int gf516m_clear_status(struct gf516m_dev *gf516m_dev, u8 mode, u8 status)
{
	mutex_lock(&gf516m_dev->dci_lock);
	pDci->clear_irq.mode = mode;
	pDci->clear_irq.status = status;
	gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_CLEAR_IRQ);
	mutex_unlock(&gf516m_dev->dci_lock);
	return 0;
}


/*-------------------------------------------------------------------------*/

#ifdef CONFIG_HAS_EARLYSUSPEND
static void gf516m_early_suspend (struct early_suspend *handler)
{
	struct gf516m_dev *gf516m_dev;

        gf516m_dev = container_of(handler, struct gf516m_dev, early_suspend);

        mutex_lock(&gf516m_dev->mode_lock);
        gf516m_debug(INFO_LOG, "[%s] enter\n", __func__);

        /* disable esd timer feature until resume finished */
        gf516m_disable_watchdog(gf516m_dev);
        //gf516m_dev->has_esd_watchdog = 0;

	#if defined(GF516M_FF_FEATURE)
	gf516m_dev->is_ff_mode = 1;
	#endif

	gf516m_dev->device_available = 0;
	
#if defined(GF516M_FF_FEATURE)
        if ( 1 == gf516m_dev->enable_sleep_mode) {
                disable_irq(gf516m_dev->spi->irq);
                gf516m_set_mode(gf516m_dev, GF516M_SLEEP_MODE);
                gf516m_debug(INFO_LOG, "[%s] : fp enter sleep mode.\n", __func__);
        } else {
                gf516m_set_mode(gf516m_dev, GF516M_FF_MODE);
	       gf516m_debug(INFO_LOG, "[%s] : fp enter ff mode, gf516m_dev->mode = 0x%x.\n", __func__, gf516m_dev->mode);
        }

#elif defined(GF516M_SLEEP_FEATURE)
	gf516m_disable_irq(gf516m_dev);
	gf516m_set_mode(gf516m_dev, GF516M_SLEEP_MODE);
#endif

    mutex_unlock(&gf516m_dev->mode_lock);
}

static void gf516m_late_resume (struct early_suspend *handler)
{
	struct gf516m_dev *gf516m_dev;

        gf516m_dev = container_of(handler, struct gf516m_dev, early_suspend);

        mutex_lock(&gf516m_dev->mode_lock);
        gf516m_debug(INFO_LOG, "[%s]: mode=0x%x, enable_sleep_mode = 0x%x.\n", __func__, gf516m_dev->mode, gf516m_dev->enable_sleep_mode);

#if defined(GF516M_FF_FEATURE)
        /* 1. screen on by sensor fingerflash feature(older mode is image) */
        /* 2. screen on by power key or other methods */
        if (gf516m_dev->device_available == 0) {
                if ( 1 == gf516m_dev->enable_sleep_mode ) {
                        gf516m_hw_reset(70);
		      //clear reset irq status after reset fp.
		      gf516m_spi_write_byte(gf516m_dev, GF516M_BUFFER_STATUS_L, 0);
                        gf516m_set_mode(gf516m_dev, gf516m_dev->mode);
                        gf516m_dev->device_available = 1;
                        gf516m_dev->is_ff_mode = 0;
                        enable_irq(gf516m_dev->spi->irq);
                        gf516m_debug(INFO_LOG, "[%s]: reset and resume  fp after screen on from sleep, mode=0x%x, enable_sleep_mode =0x%x.\n", 
                                                __func__, gf516m_dev->mode, gf516m_dev->enable_sleep_mode);
                } else {
                        gf516m_set_mode(gf516m_dev, gf516m_dev->mode);
                        gf516m_dev->device_available = 1;
                        gf516m_dev->is_ff_mode = 0;
                        gf516m_debug(INFO_LOG, "[%s]: resume fp sensor after screen on from ff, mode=0x%x\n", __func__, gf516m_dev->mode);
                }
        }
        /* enable esd watchdog */
        //gf516m_dev->has_esd_watchdog = 1;
        gf516m_enable_watchdog(gf516m_dev);

#elif defined(GF516M_SLEEP_FEATURE)
        /* 1. phone has enter deepsleep, and sensor has already wakeup in resume function */
        /* 2. phone not enter deepsleep, so need wakeup sensor in screen on callback function */
        if(gf516m_dev->device_available == 0){
                gf516m_hw_reset(70);
	       //clear reset irq status after reset fp.
	       gf516m_spi_write_byte(gf516m_dev, GF516M_BUFFER_STATUS_L, 0);
                gf516m_dev->device_available = 1;
        }

	if(GF516M_IMAGE_MODE != gf516m_dev->mode){
		gf516m_set_mode(gf516m_dev, gf516m_dev->mode);
	}
	gf516m_debug(INFO_LOG, "[%s]: resume fp sensor after screen on from sleep, mode=0x%x.\n", __func__, gf516m_dev->mode);

        /* enable watchdog before enable irq!!! */
        //gf516m_dev->has_esd_watchdog = 1;
        gf516m_enable_watchdog(gf516m_dev);

	gf516m_enable_irq(gf516m_dev);
#endif

        mutex_unlock(&gf516m_dev->mode_lock);

}
#endif


static ssize_t gf516m_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	gf516m_debug(ERR_LOG, "Not support read opertion in TEE version\n");
	return -EFAULT;
}

/* Write-only message with current device setup */
static ssize_t gf516m_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	gf516m_debug(ERR_LOG, "Not support write opertion in TEE version\n");
	return -EFAULT;
}

//void gf516m_irq(void)
static irqreturn_t gf516m_irq(int irq, void* handle)
{
        struct gf516m_dev *gf516m_dev = (struct gf516m_dev *)handle;
        //struct gf516m_dev *gf516m_dev = g_gf516m_dev;
        u8 status, frame_num = 0;
        u8 status_l;
        u8 mode;
        u8 ff_mode;
        u8 notify_application = 0;

	FUNC_ENTRY();

	//gf516m_disable_watchdog(gf516m_dev);

	mutex_lock(&gf516m_dev->dci_lock);
	//read out status and mode register
	pDci->irq_status.reg_group = 0;
	gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_IRQ_STATUS);

	/* TODO: only detect image and home key temporary!! */
	ff_mode = 0;
	//status = (pDci->irq_status.status)&0xF0;
	//Jason Aug-12-2015
	status = pDci->irq_status.status;
	status_l = pDci->irq_status.status_l;	
	mode = pDci->irq_status.mode;
	mutex_unlock(&gf516m_dev->dci_lock);

	gf516m_debug(INFO_LOG, "%s: buffer status=0x%x, status_l = 0x%x, mode=0x%x\n", __func__, pDci->irq_status.status, status_l, mode);

	#if 0
	/* TODO: sanity check needed */
	if((0xa0 != status) && (0xb0 != status) && (0xc0 != status)){
		gf516m_debug(ERR_LOG, "%s: Invalid IRQ = [0x%x][0x%x], mode=0x%x****\n", __func__, pDci->irq_status.status, status, mode);
		gf516m_clear_status(gf516m_dev, mode, status);
		goto err;
	}
	#endif

        if(status_l & 0x80)
        {
//                gf516m_enable_irq(gf516m_dev);
                gf516m_spi_write_byte(gf516m_dev, 0x8141, 0);
                mutex_lock(&gf516m_dev->mode_lock);
	       if ((1 == gf516m_dev->is_ff_mode) && (GF516M_IMAGE_MODE == mode)) {
			gf516m_set_mode(gf516m_dev, GF516M_FF_MODE);
			gf516m_debug(ERR_LOG, "[%s] :there is reset int in ff mode,set the fp to ff mode again.\n", __func__);
	       } else {
			gf516m_set_mode(gf516m_dev, gf516m_dev->mode);
	       }
	       mutex_unlock(&gf516m_dev->mode_lock);
                gf516m_debug(INFO_LOG, "[%s] :reset int and resume fp to origin mode.\n", __func__);
                return IRQ_HANDLED;
        }
        if(status_l == 0x46)
        {
//                gf516m_enable_irq(gf516m_dev);
                gf516m_spi_write_byte(gf516m_dev, 0x8140, 0);
                gf516m_debug(INFO_LOG, "[%s] :LONG PRESS NOT SUPPORT.\n", __func__);
                return IRQ_HANDLED;
        }

        if ((!(status & GF516M_BUF_STA_MASK)) || (!((status & GF516M_KEY_MASK) ||(status & GF516M_IMAGE_MASK) || (status_l & GF516M_NVG_MASK) || (status_l & GF516M_LONG_PRESS_MASK)))) {
                gf516m_debug(INFO_LOG, "[%s]:Invalid IRQ = 0x%x\n", __func__, status);
//              gf516m_enable_irq(gf516m_dev);
                gf516m_spi_write_byte(gf516m_dev, GF516M_BUFFER_STATUS, (status & 0x7F));
                 
                return IRQ_HANDLED;
        }

        switch(mode)
        {
                case GF516M_KEY_MODE:
                        gf516m_debug(INFO_LOG, "%s receives irq in key mode, status = 0x%x, status_l = 0x%x. \n", __func__, status, status_l);
                        if((status & GF516M_KEY_MASK) && (status & GF516M_BUF_STA_MASK)) {
                                gf516m_debug(INFO_LOG, "%s: home key events [0x%x][%s]=====\n", __func__, status, ((0xa0 == status)?"UP":"DOWN"));
                                input_report_key(gf516m_dev->input, GF516M_INPUT_HOME_KEY, (status & GF516M_KEY_STA)>>4);
                                input_sync(gf516m_dev->input);
                        }
#if 0
                        //Jason Aug-12-2015
                        if ((status_l & GF516M_NVG_MASK) && (status & GF516M_BUF_STA_MASK)) {

                                if (GF516M_NVG_EVENT_LEFT == ((status_l & GF516M_NVG_STA) >> 4)) {
                                        #if 0
                                        input_report_key(gf516m_dev->input, GF516M_INPUT_UP_KEY, 1);
                                        input_sync(gf516m_dev->input);
                                        input_report_key(gf516m_dev->input, GF516M_INPUT_UP_KEY, 0);
                                        input_sync(gf516m_dev->input);
                                        #endif
                                        gf516m_dev->nvg_event_type = GF516M_NVG_EVENT_LEFT;
                                        gf516m_debug(INFO_LOG, "%s: NVG key events left. status_l = 0x%x.\n", __func__, status_l);
                                } else if (GF516M_NVG_EVENT_RIGHT == ((status_l & GF516M_NVG_STA) >> 4)) {
                                        #if 0
                                        input_report_key(gf516m_dev->input, GF516M_INPUT_DOWN_KEY, 1);
                                        input_sync(gf516m_dev->input);
                                        input_report_key(gf516m_dev->input, GF516M_INPUT_DOWN_KEY, 0);
                                        input_sync(gf516m_dev->input);
                                        #endif
                                        gf516m_dev->nvg_event_type = GF516M_NVG_EVENT_RIGHT;
                                        gf516m_debug(INFO_LOG, "%s: NVG key events right. status_l = 0x%x.\n", __func__, status_l);
                                } else if (GF516M_NVG_EVENT_UP == ((status_l & GF516M_NVG_STA) >> 4)) {
                                        gf516m_debug(INFO_LOG, "%s: NVG key events up. status_l = 0x%x.\n", __func__, status_l);
                                } else if (GF516M_NVG_EVENT_DOWN == ((status_l & GF516M_NVG_STA) >> 4)) {
                                        gf516m_debug(INFO_LOG, "%s: NVG key events down. status_l = 0x%x.\n", __func__, status_l);
                                }
                        }
#endif
                        //gf516m_dev->event_type = ((GF_HOMEKEY_EVENT<<16) | (u32)(status&0x00FF));
                        gf516m_clear_status(gf516m_dev, mode, status);
                        //gf516m_dev->lastEvent = GF_HOMEKEY_EVENT;
                        break;
                case GF516M_NVG_MODE:
//                        if(rec_event){
//                                gf516m_debug(INFO_LOG, " Switch mode avoid send event ===== \n");
//                                rec_event = 0;
//                                break;
//                        }        
                        gf516m_debug(INFO_LOG, "%s receives irq in navigation mode, status = 0x%x, status_l = 0x%x. \n", __func__, status, status_l);
                        if((status_l & GF516M_NVG_MASK) && (status & GF516M_BUF_STA_MASK) && (!(status & GF516M_KEY_MASK))) {
                                gf516m_debug(ERR_LOG, " NAV IMAGE EVENT ===== \n");

                                if ((GF_KEYDOWN_EVENT == gf516m_dev->lastEvent) || (GF_FP_DATA_EVENT == gf516m_dev->lastEvent)){

                                        gf516m_debug(INFO_LOG, "%s read nvg data\n", __func__);
                                        mutex_lock(&gf516m_dev->dci_lock);
                                        pDci->forward_irq.event_type= GF_NVG_DATA_EVENT;
                                        pDci->forward_irq.status = status_l;
                                        pDci->forward_irq.mode= mode;
				     pDci->forward_irq.notify_application = 0;
                                        gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_READ_NVG_DATA);
				     notify_application = pDci->forward_irq.notify_application;
                                        mutex_unlock(&gf516m_dev->dci_lock);
                                        
                                        gf516m_dev->lastEvent = GF_FP_DATA_EVENT;
                                        if (1 == notify_application) {
                                                gf516m_debug(INFO_LOG, "%s: finished allocate nvg data\n", __func__);
                                                //TODO: notify uppler level, not needed?
                                        #ifdef GF516M_FASYNC
                                                if(gf516m_dev->async) {
                                                        gf516m_dev->event_type = ((gf516m_dev->sig_count<<24) | (GF_NVG_DATA_EVENT<<16) | (u32)(status&0x00FF));
                                                        gf516m_dev->sig_count++;
                                                        kill_fasync(&gf516m_dev->async, SIGIO, POLL_IN);
                                                }
                                        #endif
                                        }else{
                                                gf516m_debug(ERR_LOG, "%s: failed to allocate nvg data, notify_application is 0x%x.\n", __func__, notify_application);
                                                break;   //TODO :break, do not clear irq to get the same irq;
                                        }
                                }
                        }else if(status & GF516M_KEY_MASK){
                                if(status & GF516M_KEY_STA){
                                        gf516m_dev->lastEvent = GF_KEYDOWN_EVENT;
                                        nav_status = 1000;
                                        gf516m_debug(ERR_LOG, " touch key down event ===== %d\n",nav_status);
                                        gf516m_clear_status(gf516m_dev, mode, status);

                                        break;
                                }else{
                                        if (GF_FP_DATA_EVENT != gf516m_dev->lastEvent) {
                                                gf516m_debug(DEBUG_LOG, "%s: reset sample status when receive UP event\n", __func__);
                                                mutex_lock(&gf516m_dev->dci_lock);
                                                pDci->reset_sample.do_reset_sample = 1;
                                                gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_RESET_SAMPLE);
                                                mutex_unlock(&gf516m_dev->dci_lock);

                                                gf516m_clear_status(gf516m_dev, mode, status);

                                                break;

                                        }
                                        if(nav_status == GF_NVG_TOUCH_EVENT)
                                        {
                                                mutex_lock(&gf516m_dev->dci_lock);
                                                pDci->forward_irq.event_type= GF_NVG_DATA_EVENT;
                                                pDci->forward_irq.status = status_l;
                                                pDci->forward_irq.mode= mode;
					    pDci->forward_irq.notify_application = 0;
                                                gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_READ_NVG_DATA);
					    notify_application = pDci->forward_irq.notify_application;
                                                mutex_unlock(&gf516m_dev->dci_lock);
                                                
                                                if (1 == notify_application) {
                                                        gf516m_debug(INFO_LOG, "%s: finished allocate nvg data\n", __func__);
                                                        //TODO: notify uppler level, not needed?
                                                #ifdef GF516M_FASYNC
                                                        if(gf516m_dev->async) {
                                                                gf516m_dev->event_type = ((gf516m_dev->sig_count<<24) | (GF_NVG_DATA_EVENT<<16) | (u32)(status&0x00FF));
                                                                gf516m_dev->sig_count++;
                                                                kill_fasync(&gf516m_dev->async, SIGIO, POLL_IN);
                                                        }
                                                #endif
                                                }else{
                                                        gf516m_debug(ERR_LOG, "%s: failed to allocate nvg data, notify_application is 0x%x.\n", __func__, notify_application);
                                                }
                                        }
                                        gf516m_dev->lastEvent = GF_KEYUP_EVENT;
                                        //gf516m_spi_read_byte(gf516m_dev, 0x8146, &frame_num);
                                        gf516m_debug(ERR_LOG, " touch key up event ===== %d   --  frame_num 0x%x\n",nav_status, frame_num);
                                        if(get_algo_result == 1){
                                                if(nav_status == GF_NVG_TOUCH_EVENT){
                                                        gf516m_debug(ERR_LOG, " NOT SUPPORT ===== %d\n",nav_status);
                                                        //input_report_key(gf516m_dev->input, GF516M_NVG_TOUCH_KEY, 0);
                                                        //input_sync(gf516m_dev->input);
                                                }else if(nav_status == GF_NVG_LEFT_EVENT){
                                                        gf516m_debug(ERR_LOG, " key left event ===== %d\n",nav_status);         
                                                        input_report_key(gf516m_dev->input, GF516M_NVG_LEFT_KEY, 0);
                                                        input_sync(gf516m_dev->input);

                                                }else if(nav_status == GF_NVG_RIGHT_EVENT){
                                                        gf516m_debug(ERR_LOG, " key right event ===== %d\n",nav_status);                
                                                        input_report_key(gf516m_dev->input, GF516M_NVG_RIGHT_KEY, 0);
                                                        input_sync(gf516m_dev->input);

                                                }else if(nav_status == GF_NVG_UP_EVENT){
                                                        gf516m_debug(ERR_LOG, " key up event ===== %d\n",nav_status);           
                                                        input_report_key(gf516m_dev->input, GF516M_NVG_UP_KEY, 0);
                                                        input_sync(gf516m_dev->input);

                                                }else if(nav_status == GF_NVG_DOWN_EVENT){
                                                        gf516m_debug(ERR_LOG, " key down event ===== %d\n",nav_status);         
                                                        input_report_key(gf516m_dev->input, GF516M_NVG_DOWN_KEY, 0);
                                                        input_sync(gf516m_dev->input);

                                                }else if(nav_status == GF_NVG_WEIGHT_PRESS_EVENT){
                                                        gf516m_debug(ERR_LOG, " key weight press event ===== %d\n",nav_status);         
                                                        input_report_key(gf516m_dev->input, GF516M_NVG_TOUCH_KEY, 0);
                                                        input_sync(gf516m_dev->input);

                                                }else if(nav_status == GF_NVG_LIGHT_PRESS_EVENT){
                                                        gf516m_debug(ERR_LOG, " NOT SUPPORT  ===== %d\n",nav_status);          
                                                        //input_report_key(gf516m_dev->input, GF516M_NVG_TOUCH_KEY, 0);
                                                        //input_sync(gf516m_dev->input);

                                                }else{
                                                        gf516m_debug(ERR_LOG, " ERROR key event ===== %d\n",nav_status);
                                                }
                                                gf516m_clear_status(gf516m_dev, mode, status);
                                        }
                                        get_algo_result = 0;
                                }
                        }

                        

                        break;

                case GF516M_NVG_MODE_NOTX:
                        gf516m_debug(INFO_LOG, "%s receives irq in navigation base mode, status = 0x%x, status_l = 0x%x. \n", __func__, status, status_l);
                        mutex_lock(&gf516m_dev->dci_lock);
                        pDci->forward_irq.event_type= GF_NVG_REF_DATA_EVENT;
                        pDci->forward_irq.mode= mode;
		      pDci->forward_irq.notify_application = 0;
                        gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_READ_NVG_DATA);
		       notify_application = pDci->forward_irq.notify_application;
                        mutex_unlock(&gf516m_dev->dci_lock);


                        if (3 == notify_application) {
                                gf516m_debug(INFO_LOG, "%s: finished allocate nvg base data in nvg notx mode\n", __func__);
                                gf516m_dev->lastEvent = GF_REF_DATA_EVENT;
                        #ifdef GF516M_FASYNC
                                if(gf516m_dev->async) {
                                        gf516m_dev->event_type = ((gf516m_dev->sig_count<<24) | (GF_NVG_REF_DATA_EVENT<<16) | (u32)(status & 0x00FF));
                                        gf516m_dev->sig_count++;
                                        kill_fasync(&gf516m_dev->async, SIGIO, POLL_IN);
                                }
                        #endif
                        
                        }else{
                                gf516m_debug(ERR_LOG, "%s: failed to allocate nvg baed data in nvg notx mode, notify_application=0x%x.\n", __func__, notify_application);
                        }


                        gf516m_clear_status(gf516m_dev, mode, status);
                        //gf516m_dev->lastEvent = GF_HOMEKEY_EVENT;
                        break;
                case GF516M_FF_MODE:
                        ff_mode = 1;
                        gf516m_debug(INFO_LOG, "%s receives irq in ff mode [0x%x], gf516m_dev->mode[0x%x]\n", __func__, status, gf516m_dev->mode);
                        #if 0
                        if(GF516M_IMAGE_MODE != gf516m_dev->mode){
                                
                                if(0 == gf516m_dev->device_available){
                                        /* user not set fingerprint unlock, send power key event directly and restore mode */
                                        gf516m_set_mode(gf516m_dev, gf516m_dev->mode);
                                        input_report_key(gf516m_dev->input, GF516M_FF_KEY, 1);
                                        input_sync(gf516m_dev->input);
                                        input_report_key(gf516m_dev->input, GF516M_FF_KEY, 0);
                                        input_sync(gf516m_dev->input);

					gf516m_clear_status(gf516m_dev, mode, status);
					gf516m_dev->device_available = 1;
					gf516m_dev->is_ff_mode = 0;

					gf516m_debug(INFO_LOG, "%s: send power key event, status[0x%x]\n", __func__, status);
				}else{
					gf516m_debug(ERR_LOG, "%s: devices already available in ff mode, status[0x%x]\n", __func__, status);
				}
				
				gf516m_debug(INFO_LOG, "%s: Don't send power key event, status[0x%x]\n", __func__, status);
				gf516m_clear_status(gf516m_dev, mode, status);
				break;
			}
			#endif
			/* if gf516m_dev->mode is IMAGE, continue run and not break!!! */
			/* TODO: */

                case GF516M_IMAGE_MODE:
                        if(status & GF516M_IMAGE_MASK){
                                /* image event */
                                if ((GF_KEYDOWN_EVENT == gf516m_dev->lastEvent) || (GF_FP_DATA_EVENT == gf516m_dev->lastEvent)){
                                        if(gf516m_dev->secspi_init_ok){
                                                mutex_lock(&gf516m_dev->dci_lock);
                                                pDci->forward_irq.event_type = GF_FP_DATA_EVENT;
                                                pDci->forward_irq.status = status;
                                                pDci->forward_irq.mode = mode;
                                                pDci->forward_irq.notify_application = 0;
                                                gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_FORWARD_IRQ);
					    notify_application = pDci->forward_irq.notify_application;
                                                mutex_unlock(&gf516m_dev->dci_lock);
                                                gf516m_dev->lastEvent = GF_FP_DATA_EVENT;

                                                if (1 == notify_application) {
                                                        gf516m_debug(INFO_LOG, "%s: finished allocate fp data, notify upper, count=%d=====\n", __func__, gf516m_dev->sig_count);
                                                        //TODO: notify uppler level, not needed?
                                                        #ifdef GF516M_FASYNC
                                                        if(gf516m_dev->async) {
                                                                gf516m_dev->event_type = ((gf516m_dev->sig_count<<24) | (GF_FP_DATA_EVENT<<16) | (u32)(status&0x00FF));
                                                                gf516m_dev->sig_count++;
                                                                kill_fasync(&gf516m_dev->async, SIGIO, POLL_IN);
                                                        }
                                                        #endif
                                                }else{
                                                        gf516m_debug(ERR_LOG, "%s: failed to allocate fp data, notify_application is 0x%x.\n", __func__, notify_application);
                                                }
                                                //TODO: clear interrupt in linux driver after receviced result from secure driver
                                                gf516m_clear_status(gf516m_dev, mode, status);
                                        }
                                } else {
                                        gf516m_debug(ERR_LOG, "%s: without key down event, so discard the image event, gf516m_dev->lastEvent=%d\n", __func__, gf516m_dev->lastEvent);
                                        gf516m_clear_status(gf516m_dev, mode, status);
                                }
                        }else if(status & GF516M_KEY_MASK){
                                if(status & GF516M_KEY_STA){
                                        gf516m_debug(INFO_LOG, "%s: key DOWN event in image mode, count=%d=====\n", __func__, gf516m_dev->sig_count);
                                        if (GF_KEYDOWN_EVENT != gf516m_dev->lastEvent) {

                                                #ifdef GF516M_FF_DRIVER_TEST
                                                /* for driver local test only, send power key event */
                                                if(ff_mode){
                                                        input_report_key(gf516m_dev->input, GF516M_FF_KEY, 1);
                                                        input_sync(gf516m_dev->input);
                                                        input_report_key(gf516m_dev->input, GF516M_FF_KEY, 0);
                                                        input_sync(gf516m_dev->input);
                                                        gf516m_debug(INFO_LOG, "%s: in ff mode, driver local test, send power key event\n", __func__);
                                                }
                                                #endif
                                                //TODO: send to upper level
                                                gf516m_dev->lastEvent = GF_KEYDOWN_EVENT;
                                                #ifdef GF516M_FASYNC
                                                if(gf516m_dev->async) {
                                                        gf516m_dev->event_type = ((gf516m_dev->sig_count<<24) | (GF_KEYDOWN_EVENT<<16) | (u32)(status&0x00FF));
                                                        gf516m_dev->sig_count++;
                                                        kill_fasync(&gf516m_dev->async, SIGIO, POLL_IN);
                                                }
                                                #endif
                                        } else {
                                                gf516m_debug(INFO_LOG, "%s: duplicate key DOWN event\n", __func__);
                                                gf516m_clear_status(gf516m_dev, mode, status);
                                        }
                                }else{
                                        gf516m_debug(INFO_LOG, "%s: key UP event in image mode, count=%d=====\n", __func__, gf516m_dev->sig_count);
                                        if (GF_KEYUP_EVENT != gf516m_dev->lastEvent) {
                                                if (GF_FP_DATA_EVENT != gf516m_dev->lastEvent) {
                                                        gf516m_debug(DEBUG_LOG, "%s: reset sample status when receive UP event\n", __func__);
                                                        mutex_lock(&gf516m_dev->dci_lock);
                                                        pDci->reset_sample.do_reset_sample = 1;
                                                        gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_RESET_SAMPLE);
                                                        mutex_unlock(&gf516m_dev->dci_lock);
                                                }
                                                //TODO: send to upper level
                                                gf516m_dev->lastEvent = GF_KEYUP_EVENT;
                                                #ifdef GF516M_FASYNC
                                                if(gf516m_dev->async) {
                                                        gf516m_dev->event_type = ((gf516m_dev->sig_count<<24) | (GF_KEYUP_EVENT<<16) | (u32)(status & 0x00FF));
                                                        gf516m_dev->sig_count++;
                                                        kill_fasync(&gf516m_dev->async, SIGIO, POLL_IN);
                                                }
                                                #endif
                                        } else {
                                                gf516m_debug(INFO_LOG, "%s: duplicate key UP event\n", __func__);
                                                gf516m_clear_status(gf516m_dev, mode, status);
                                        }
                                }
                                #ifdef GF516M_DRIVER_CLEAR_DOWN_UP_IRQ
                                /* TODO: clear key down/on interrupt in Hal!! */
                                gf516m_clear_status(gf516m_dev, mode, status);
                                #endif
                        }
                        break;
                case GF516M_DEBUG_MODE:
                        gf516m_debug(INFO_LOG, "%s receives irq in debug mode status = 0x%x.\n", __func__, status);
                        if ((status & GF516M_IMAGE_MASK) && (status & GF516M_BUF_STA_MASK)) {
                                if (gf516m_dev->secspi_init_ok) {
                                        mutex_lock(&gf516m_dev->dci_lock);
                                        pDci->forward_irq.event_type = GF_FP_DATA_EVENT;
                                        pDci->forward_irq.status = status;
                                        pDci->forward_irq.mode = mode;
                                        pDci->forward_irq.notify_application = 0;
                                        gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_FORWARD_IRQ);
				     notify_application = pDci->forward_irq.notify_application;
                                        mutex_unlock(&gf516m_dev->dci_lock);
                                        gf516m_dev->lastEvent = GF_FP_DATA_EVENT;

                                        if (1 == notify_application) {
                                                gf516m_debug(INFO_LOG, "%s: finished allocate fp data in debug mode, notify upper, count=%d=====\n", __func__, gf516m_dev->sig_count);
                                                //TODO: notify uppler level, not needed?
                                                #ifdef GF516M_FASYNC
                                                if (gf516m_dev->async) {
                                                        gf516m_dev->event_type = ((gf516m_dev->sig_count<<24) | (GF_FP_DATA_EVENT<<16) | (u32)(status&0x00FF));
                                                        gf516m_dev->sig_count++;
                                                        kill_fasync(&gf516m_dev->async, SIGIO, POLL_IN);
                                                }
                                                #endif
                                        } else {
                                                gf516m_debug(ERR_LOG, "%s: failed to allocate fp data in debug mode, notify_application is 0x%x.\n", __func__, notify_application);
                                        }
                                        //TODO: clear interrupt in linux driver after receviced result from secure driver
                                        gf516m_clear_status(gf516m_dev, mode, status);
                                }
                        }
                        break;
                case GF516M_DEBUG_MODE_NOTX:
                        gf516m_debug(INFO_LOG, "%s receives irq in debug  no tx mode[0x%x], status[0x%x]\n", __func__, mode, status);

                        if(0xC0 == status){
                                gf516m_debug(INFO_LOG, "%s begin collect fp data in debug  no tx mode\n", __func__);
                                if(gf516m_dev->secspi_init_ok){
                                        mutex_lock(&gf516m_dev->dci_lock);
                                        pDci->forward_irq.event_type = GF_REF_DATA_EVENT;
                                        pDci->forward_irq.status = status;
                                        pDci->forward_irq.mode = mode;
                                        pDci->forward_irq.notify_application = 0;
                                        gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_FORWARD_IRQ);
				     notify_application = pDci->forward_irq.notify_application;
                                        mutex_unlock(&gf516m_dev->dci_lock);

                                        if (3 == notify_application) {
                                                gf516m_debug(INFO_LOG, "%s: finished allocate fp data in debug no tx mode\n", __func__);
                                                gf516m_dev->lastEvent = GF_REF_DATA_EVENT;
                                                #ifdef GF516M_FASYNC
                                                if(gf516m_dev->async) {
                                                        gf516m_dev->event_type = ((gf516m_dev->sig_count<<24) | (GF_REF_DATA_EVENT<<16) | (u32)(status & 0x00FF));
                                                        gf516m_dev->sig_count++;
                                                        kill_fasync(&gf516m_dev->async, SIGIO, POLL_IN);
                                                }
                                                #endif

                                        }else{
                                                gf516m_debug(ERR_LOG, "%s: failed to allocate fp data in debug no tx mode, notify_application=0x%x.\n", __func__, notify_application);
                                        }
                                        //TODO: clear interrupt in linux driver after receviced result from secure driver
                                        gf516m_clear_status(gf516m_dev, mode, status);

					/* sevices need make sure switch back to KEY or IMAGE mode!!!! */
					/* driver will not restore to original mode */
					//gf516m_set_mode(gf516m_dev, gf516m_dev->mode);
				}
			}
			break;
		default:
			gf516m_debug(ERR_LOG, "%s error mode[0x%x] [0x%x]\n", __func__, mode, status);
	}

	//gf516m_enable_watchdog(gf516m_dev);
	FUNC_EXIT();
	return IRQ_HANDLED;
}


static long gf516m_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
        struct gf516m_dev *gf516m_dev = NULL;
        struct gf_ioc_transfer *ioc = NULL;
        u32 tmp = 0;
        int retval = 0;
        GF516M_MODE mode;
        u32 mode2 = 0xFFFFFFFF;
        u8 value = 0;
        int fw_update_flag = 0;
        u8 fw_update_cnt = 0;
        u32 sleep_mode = 0;
        u8 notify_application = 0;
        u8 irq_status_l = 0;

	FUNC_ENTRY();
	if (_IOC_TYPE(cmd) != GF_IOC_MAGIC){
		return -ENOTTY;
	}
	/* Check access direction once here; don't repeat below.
	* IOC_DIR is from the user perspective, while access_ok is
	* from the kernel perspective; so they look reversed.
	*/
	if (_IOC_DIR(cmd) & _IOC_READ){
		retval = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	}
	if (retval == 0 && _IOC_DIR(cmd) & _IOC_WRITE){
		retval = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	}
	if (retval){
		return -EFAULT;
	}

	gf516m_dev = (struct gf516m_dev *)filp->private_data;

	switch(cmd){
		case GF_IOC_CMD:
		/* the use of ioc->buf is different with old implementation, NO OFFSET NEEDED now!!!!! */
		ioc = kzalloc(sizeof(*ioc), GFP_KERNEL);
		/*copy command data from user to kernel.*/
		if(copy_from_user(ioc, (struct gf_ioc_transfer*)arg, sizeof(*ioc))){
			gf516m_debug(ERR_LOG, "Failed to copy command from user to kernel\n");
			retval = -EFAULT;
			break;
		}

                if((ioc->len > bufsiz)||(ioc->len == 0)) {
                        gf516m_debug(ERR_LOG, "Request length[%d] longer than supported maximum buf length[%d]\n",
                                        ioc->len, bufsiz);
                        retval = -EMSGSIZE;
                        break;
                }
                mutex_lock(&gf516m_dev->buf_lock);
                if(ioc->cmd == GF516M_R) {
                        /*if want to read data from hardware.*/
                        gf516m_debug(INFO_LOG, "Read data from 0x%x, len = 0x%x buf = 0x%p\n", ioc->addr, ioc->len, ioc->buf);
                        gf516m_spi_read_bytes(gf516m_dev, ioc->addr, ioc->len, gf516m_dev->buffer);
                        if(copy_to_user(ioc->buf, gf516m_dev->buffer, ioc->len)) {
                                gf516m_debug(ERR_LOG, "Failed to copy data from kernel to user\n");
                                retval = -EFAULT;
                                mutex_unlock(&gf516m_dev->buf_lock);
                                break;
                        }
                } else if (ioc->cmd == GF516M_W) {
                        /*if want to read data from hardware.*/
                        gf516m_debug(INFO_LOG, "Write data from 0x%x, len = 0x%x\n", ioc->addr, ioc->len);
                        if(copy_from_user(gf516m_dev->buffer, ioc->buf, ioc->len)){
                                gf516m_debug(ERR_LOG, "Failed to copy data from user to kernel\n");
                                retval = -EFAULT;
                                mutex_unlock(&gf516m_dev->buf_lock);
                                break;
                        }
                        gf516m_spi_write_bytes(gf516m_dev, ioc->addr, ioc->len, gf516m_dev->buffer);
                } else {
                        gf516m_debug(ERR_LOG, "Error command for gf516m\n");
                }
                if(ioc != NULL) {
                        kfree(ioc);
                        ioc = NULL;
                }
                mutex_unlock(&gf516m_dev->buf_lock);
                break;
        case GF_IOC_REINIT:
                /* make sure secure SPI init finished first */
                gf516m_disable_irq(gf516m_dev);
                gf516m_gpio_cfg();
                gf516m_hw_reset(80);
	       //clear reset irq status after reset fp.
	       gf516m_spi_write_byte(gf516m_dev, GF516M_BUFFER_STATUS_L, 0);

		//init fp chip, FW & cfg
		gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_FP_INIT);

                gf516m_enable_irq(gf516m_dev);
                //msleep(200); //waiting the hardware to wake-up.
                break;
        case GF_IOC_SPI_SETSPEED:
                /* setspeed ioctl for test only, please do NOT call outside normal driver!!! */
                /* SPI speed is controlled by normal driver only */
                /* not work for MTK platform */
                retval = __get_user(tmp, (u32 __user*)arg);
                //spi_setup(gf516m_dev->spi);
                gf516m_debug(ERR_LOG, "Not support GF516M_IOC_SPI_SETSPEED in normal world, request speed %d\n", tmp);
                break;
        case GF_IOC_INIT_SPI_SPEED_VARIABLE:
                //set speed global variable
                retval = copy_from_user(&gf516m_dev->spi_speed, (void __user *)arg, sizeof(struct spi_speed_setting));
                if (retval) {
                        retval = -EFAULT;
                        break;
                }
                /* NULL */
                gf516m_debug(INFO_LOG, "init SPI speed global variable, [%d, %d]\n",
                gf516m_dev->spi_speed.high_speed, gf516m_dev->spi_speed.low_speed);
                break;
        case GF_IOC_STOPTIMER:
                gf516m_disable_watchdog(gf516m_dev);
                break;
        case GF_IOC_STARTTIMER:
                gf516m_enable_watchdog(gf516m_dev);
                break;
        case GF_IOC_SETMODE:
                retval = __get_user(mode2, (u32 __user*)arg);
                if(GF_MODE_IMAGE == mode2){
                        mode = GF516M_IMAGE_MODE;
                }else if(GF_MODE_KEY == mode2){
                        mode = GF516M_KEY_MODE;
                }else if(GF_MODE_BLANK_FRAME_WITH_TX == mode2){
                        mode = GF516M_DEBUG_MODE;
                }else if(GF_MODE_BLANK_FRAME_WITHOUT_TX == mode2){
                        mode = GF516M_DEBUG_MODE_NOTX;
                }else if(GF_MODE_NAVIGATION == mode2){
                        mode = GF516M_NVG_MODE;
                }else if(GF_MODE_NAVIGATION_NO_TX == mode2){
                        mode = GF516M_NVG_MODE_NOTX;
                }else{
                        gf516m_debug(ERR_LOG, "wrong request mode[0x%x]!\n", mode2);
                        mode = GF516M_UNKNOWN_MODE;
                        break;
                }
	       mutex_lock(&gf516m_dev->mode_lock);
	       gf516m_debug(INFO_LOG, "[%s] :set sensor mode,mode = 0x%x,device_available = 0x%x.\n",
					__func__, mode, gf516m_dev->device_available);
                if(gf516m_dev->device_available == 0) {
                        gf516m_debug(ERR_LOG, " request mode[0x%x], saved mode[0x%x], device_available = 0x%x!\n", 
							mode, gf516m_dev->mode, gf516m_dev->device_available);
                        #if defined(GF516M_FF_FEATURE)
                        if(gf516m_dev->is_ff_mode){
                                gf516m_dev->mode = mode;
                                gf516m_debug(INFO_LOG, "restore to mode[0x%x], mode will be set after wake up from ff mode.\n", mode);
                        }
                        gf516m_dev->lastEvent = GF_NOEVENT;
                        #elif defined(GF516M_SLEEP_FEATURE)
                        gf516m_dev->mode = mode;
                        gf516m_debug(INFO_LOG, "restore to mode[0x%x] after wakeup from sleep mode\n", mode);
                        #endif
                } else if(gf516m_dev->device_available == 1) {

                        if((mode == GF516M_DEBUG_MODE_NOTX) || (mode == GF516M_NVG_MODE_NOTX)){
                                mutex_lock(&gf516m_dev->dci_lock);
                                pDci->send_sensor.cmd_num = SENSOR_CMD_SAMPLE_RESUME;
                                gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_SEND_SENSOR);
                                mutex_unlock(&gf516m_dev->dci_lock);
                                gf516m_debug(INFO_LOG, "resume sample for notx mode\n");
                        }

                       gf516m_dev->lastEvent = GF_NOEVENT;
                       gf516m_dev->mode = mode;
                       gf516m_set_mode(gf516m_dev, mode);
                       gf516m_debug(INFO_LOG, "[%s]. set sensor to mode 0x%x.\n", __func__, mode);

                } else {
                        gf516m_debug(ERR_LOG, "Abnornal value: device_available value[0x%x]\n", gf516m_dev->device_available);
                }

	       mutex_unlock(&gf516m_dev->mode_lock);

                break;
        case GF_IOC_GETMODE:
                if(GF516M_IMAGE_MODE == gf516m_dev->mode){
                        mode2 = GF_MODE_IMAGE;
                }else if(GF516M_KEY_MODE == gf516m_dev->mode){
                        mode2 = GF_MODE_KEY;
                }else if(GF516M_DEBUG_MODE == gf516m_dev->mode){
                        mode2 = GF_MODE_BLANK_FRAME_WITH_TX;
                }else if(GF516M_DEBUG_MODE_NOTX == gf516m_dev->mode){
                        mode2 = GF_MODE_BLANK_FRAME_WITHOUT_TX;
                }else{
                        gf516m_debug(ERR_LOG, "wrong gf516m_dev->mode[0x%x]!\n", gf516m_dev->mode);
                        mode2 = GF_MODE_UNKNOWN;
                }
                retval = __put_user(mode2, (u32 __user*)arg);
                break;

	case GF_IOC_CONNECT_SECURE_DRIVER:
		retval = gf516m_load_secdrv();
		break;
	case GF_IOC_CLOSE_SECURE_DRIVER:
		gf516m_dev->secspi_init_ok = 0;
		gf516m_disable_watchdog(gf516m_dev);

                /* TRUSTONIC suggest not close secure driver session, it may lead to t-base hung */
                retval = gf516m_close_secdrv();
                break;
        case GF_IOC_SECURE_INIT:
                mdelay(100);
		//read fw version
		gf516m_debug(INFO_LOG, "[%s]: ready to read the chip id from TEE.\n", __func__);
		gf516m_read_fw_version(gf516m_dev);
                
                //add GF516M special init for secure driver
                //SPI has transfered from normal to secure, no more access in normal driver
                gf516m_debug(INFO_LOG, "%s: secure SPI init started======\n", __func__);

                /* check whether need upgrade fw */
	do {
	       fw_update_flag = 0;
                if (1 == gf516m_dev->fw_need_upgrade) {
                        gf516m_debug(INFO_LOG, "[%s]: fw has been damaged, need upgrade.\n", __func__);

                        mutex_lock(&gf516m_dev->dci_lock);
                        pDci->upgrade_fw_cfg.cmd = GF_UPDATE_FW_DAMAGE_CMD;
                        gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_UPGRADE_FW_CFG);
                        if (GF_UPDATE_FW_RESULT_OK == pDci->upgrade_fw_cfg.ret) {
                                gf516m_debug(INFO_LOG, "[%s] :update firmware success.\n", __func__);
                                gf516m_hw_reset(100);
                                pDci->upgrade_fw_cfg.cmd =GF_UPDATE_CFG_CMD;
                                gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_UPGRADE_FW_CFG);
                                if (GF_UPDATE_CFG_RESULT_OK == pDci->upgrade_fw_cfg.ret) {
                                        gf516m_debug(INFO_LOG, "[%s]: update config success.\n", __func__);
                                } else {
                                        gf516m_debug(INFO_LOG, "[%s]: update config failed.\n", __func__);
				    fw_update_flag = 1;
                                }
                        } else {
                                /* upgrade failed */
                                gf516m_debug(ERR_LOG, "[%s]: update firmware failed. ret =%d.\n", __func__, pDci->upgrade_fw_cfg.ret);
			     fw_update_flag = 1;
                        }
                        mutex_unlock(&gf516m_dev->dci_lock);

                }else {
                        gf516m_debug(INFO_LOG, "[%s]: check whether fw need update.\n", __func__);
                        mutex_lock(&gf516m_dev->dci_lock);
                        pDci->upgrade_fw_cfg.cmd = GF_UPDATE_FW_CHECK_CMD;
                        gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_UPGRADE_FW_CFG);
                        if (GF_UPDATE_FW_RESULT_NO_NEED_UPDATE == pDci->upgrade_fw_cfg.ret) {
                                gf516m_debug(INFO_LOG, "[%s]: no need update firmware.\n", __func__);
                        } else if (GF_UPDATE_FW_RESULT_OK == pDci->upgrade_fw_cfg.ret) {
                                gf516m_debug(INFO_LOG, "[%s] :update firmware success.\n", __func__);
                                gf516m_hw_reset(100);
                                pDci->upgrade_fw_cfg.cmd =GF_UPDATE_CFG_CMD;
                                gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_UPGRADE_FW_CFG);
                                if (GF_UPDATE_CFG_RESULT_OK == pDci->upgrade_fw_cfg.ret) {
                                        gf516m_debug(INFO_LOG, "[%s]: update config success.\n", __func__);
                                } else {
                                        gf516m_debug(INFO_LOG, "[%s]: update config failed.\n", __func__);
				     fw_update_flag = 1;
                                }
                        } else {
                                /* upgrade failed */
                                gf516m_debug(ERR_LOG, "[%s]: update firmware failed. ret =%d.\n", __func__, pDci->upgrade_fw_cfg.ret);
			     fw_update_flag = 1;
                        }
                        mutex_unlock(&gf516m_dev->dci_lock);
                }
	   fw_update_cnt ++;
	   gf516m_debug(INFO_LOG, "[%s]:fw_update_flag =%d,fw_update_cnt = %d.\n", __func__, fw_update_flag, fw_update_cnt);

	   if (fw_update_flag && (fw_update_cnt < 5)) {
		gf516m_hw_power(0);
		mdelay(100);
		gf516m_hw_power(1);
		gf516m_pull_miso_high(gf516m_dev);
		udelay(100);
		gf516m_gpio_cfg();
             }
 }while(fw_update_flag && (fw_update_cnt < 5));

	if (fw_update_flag) {
		retval = -ENODEV;
		gf516m_debug(INFO_LOG, "[%s]: fw update failed.fw_update_flag = %d.\n", __func__, fw_update_flag);
		break;
	}
                //gf516m_gpio_cfg();
                //gf516m_hw_reset(80);

                //init fp chip, FW & cfg
                mutex_lock(&gf516m_dev->dci_lock);
                gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_FP_INIT);
                mutex_unlock(&gf516m_dev->dci_lock);

                gf516m_dev->secspi_init_ok = 1;
                gf516m_dev->device_available = 1;
                /* default mode is key after init */
                gf516m_dev->mode = GF516M_IMAGE_MODE;
	       //clear reset irq status after reset fp.
	       gf516m_spi_write_byte(gf516m_dev, GF516M_BUFFER_STATUS_L, 0);
#if 0
                gf516m_debug(INFO_LOG, "%s: init nvg timer.\n", __func__);
                init_timer(&gf516m_dev->gf516m_nvg_timer);
                gf516m_dev->gf516m_nvg_timer.function = gf516m_nvg_timer_func;
                gf516m_dev->gf516m_nvg_timer.data = (unsigned long)gf516m_dev;
#endif
                gf516m_irq_cfg();
                retval = request_threaded_irq(gf516m_dev->spi->irq, NULL, gf516m_irq,
                                IRQF_TRIGGER_RISING | IRQF_ONESHOT, dev_name(&(gf516m_dev->spi->dev)), gf516m_dev);
                if(!retval) {
                        gf516m_debug(INFO_LOG, "%s irq thread request success!\n", __func__);
                }else{
                        gf516m_debug(ERR_LOG, "%s irq thread request failed, retval=%d\n", __func__, retval);
		       break;
                }
                //gf516m_dev->irq_count = 1;
                gf516m_disable_irq(gf516m_dev);

#ifdef GF516M_WATCHDOG
                //moved after secure SPI finished init
                INIT_WORK(&gf516m_dev->spi_work, gf516m_timer_work);
                init_timer(&gf516m_dev->gf516m_timer);
                gf516m_dev->gf516m_timer.function = gf516m_timer_func;
                gf516m_dev->gf516m_timer.data = (unsigned long)gf516m_dev;
                //gf516m_dev->has_esd_watchdog = 1;
                gf516m_enable_watchdog(gf516m_dev);
#endif

#if defined(CONFIG_HAS_EARLYSUSPEND)
		gf516m_debug(INFO_LOG ,"[%s] : register_early_suspend\n", __func__);
		gf516m_dev->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1;
		gf516m_dev->early_suspend.suspend = gf516m_early_suspend,
		gf516m_dev->early_suspend.resume = gf516m_late_resume,
		register_early_suspend (&gf516m_dev->early_suspend);
#endif

		gf516m_dev->lastEvent = GF_NOEVENT;
		gf516m_dev->sig_count = 0;
		gf516m_debug(INFO_LOG, "%s: secure SPI init finished======\n", __func__);

                break;
        case GF_IOC_SET_IRQ:
                retval = __get_user(tmp, (u32 __user*)arg);
                if(tmp) {
                        if(gf516m_dev->secspi_init_ok){
                                //enable irq by ioctl later
                                gf516m_enable_irq(gf516m_dev);
                        }
                }else{
                        if(gf516m_dev->secspi_init_ok){
                                //disable irq by ioctl later
                                gf516m_disable_irq(gf516m_dev);
                        }
                }
                break;
        case GF_IOC_HAS_ESD_WD:
                retval = __get_user(tmp, (u32 __user*)arg);
                if(tmp) {
                        gf516m_debug(INFO_LOG, "fingerpint chip can enter idle\n");
                        //gf516m_dev->has_esd_watchdog = 1;
                }else{
                        gf516m_debug(INFO_LOG, "fingerpint chip will NOT enter idle\n");
                        //gf516m_dev->has_esd_watchdog = 0;
                }
                break;
        case GF_IOC_RESET_SAMPLE_STATUS:
                retval = __get_user(tmp, (u32 __user*)arg);
                if(tmp) {
                        gf516m_debug(INFO_LOG, "GF516M_IOC_RESET_SAMPLE_STATUS: reset sample status\n");
                        mutex_lock(&gf516m_dev->dci_lock);
                        pDci->reset_sample.do_reset_sample = 1;
                        gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_RESET_SAMPLE);
                        mutex_unlock(&gf516m_dev->dci_lock);
                }else{
                        //pr_info("GF_IOC_RESET_SAMPLE_STATUS: TODO\n");
                }
                break;
        case GF_IOC_GET_EVENT_TYPE:
                retval = __put_user(gf516m_dev->event_type, (u32 __user*)arg);
                break;
        case GF_IOC_CLEAR_IRQ_STATUS:
                retval = __get_user(tmp, (u32 __user*)arg);
                gf516m_clear_status(gf516m_dev, (u8)(tmp>>16), (u8)(tmp&0xff));
                break;
        case GF_IOC_SEND_SENSOR_CMD:
		if (NULL == pDci) {
			gf516m_debug(ERR_LOG, "[%s] :secure init is not ok,pDci is NULL.\n", __func__);
			retval = -ENODEV;
			break;
		}
                retval = __get_user(tmp, (u32 __user*)arg);
                gf516m_debug(INFO_LOG, "GF516M_IOC_SEND_SENSOR_CMD: command[%d]\n", tmp);
                if ((tmp == SENSOR_CMD_SET_FACTORY_TEST_A_CONFIG)
                                || (tmp == SENSOR_CMD_SET_FACTORY_TEST_B_CONFIG)
                                || (tmp == SENSOR_CMD_RESUME_NORMAL_CONFIG)) {
                        gf516m_disable_irq(gf516m_dev);
                        gf516m_disable_watchdog(gf516m_dev);
                }

		mutex_lock(&gf516m_dev->dci_lock);
		pDci->send_sensor.cmd_num = tmp;
		gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_SEND_SENSOR);
		if ( pDci->send_sensor.ret < 0) {
			gf516m_debug(ERR_LOG, " set factory config failed,ret = %d.\n", pDci->send_sensor.ret);
			retval = -EIO;
		}
		mutex_unlock(&gf516m_dev->dci_lock);

                //Jason July-24-2015 need reset the sensor after set the config.
                if ((tmp == SENSOR_CMD_SET_FACTORY_TEST_A_CONFIG)
                                        || (tmp == SENSOR_CMD_SET_FACTORY_TEST_B_CONFIG)
                                        || (tmp == SENSOR_CMD_RESUME_NORMAL_CONFIG)) {
                mdelay(100);
                gf516m_enable_watchdog(gf516m_dev);
                gf516m_enable_irq(gf516m_dev);
                }
                break;
        case GF_IOC_INIT_FP_EINT:
                gf516m_irq_cfg();
                break;
#if 1                
        case GF_IOC_SET_SLEEP_MODE:
                if (copy_from_user(&sleep_mode, (u32 *)arg, sizeof(u32))) {
                        gf516m_debug(ERR_LOG, "[%s]: failed to copy from user.\n", __func__);
                        break;
                }
                gf516m_dev->enable_sleep_mode = sleep_mode;
                gf516m_debug(INFO_LOG, "[%s]: enable_sleep_mode =0x%x.\n", __func__, gf516m_dev->enable_sleep_mode);
                break;
#endif          
        case GF_IOC_NVG_EVENT:
                retval = __get_user(tmp, (u32 __user*)arg);
                get_algo_result = 1;
                gf516m_debug(ERR_LOG, "gf516m nvg key event ===== %d\n",tmp);
                break;
                if((nav_status > 0) && (nav_status < 7))
                {       
                        gf516m_debug(INFO_LOG, "BREAK MORE EVENT %d\n",tmp);
                        break;
                }
                nav_status = tmp;

                if(tmp != GF_NVG_TOUCH_EVENT)
                        gf516m_spi_write_byte(gf516m_dev, 0x8044, 0x01);
                if(tmp == GF_NVG_TOUCH_EVENT){
                        gf516m_debug(ERR_LOG, "nvg touch key event ===== %d\n",tmp);
                        if(gf516m_dev->lastEvent == GF_KEYUP_EVENT){
                                mutex_lock(&gf516m_dev->dci_lock);
                                pDci->forward_irq.event_type= GF_NVG_DATA_EVENT;
                                pDci->forward_irq.status = 0xff;
                                pDci->forward_irq.mode= 0x10;
			     pDci->forward_irq.notify_application = 0;
                                gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_READ_NVG_DATA);
			     notify_application = pDci->forward_irq.notify_application;
                                mutex_unlock(&gf516m_dev->dci_lock);
                                
                                if (1 == notify_application) {
                                        gf516m_debug(INFO_LOG, "%s: finished allocate nvg data\n", __func__);
                                        //TODO: notify uppler level, not needed?
                                #ifdef GF516M_FASYNC
                                        if(gf516m_dev->async) {
                                                gf516m_dev->event_type = ((gf516m_dev->sig_count<<24) | (GF_NVG_DATA_EVENT<<16) | 0x00FF);
                                                gf516m_dev->sig_count++;
                                                kill_fasync(&gf516m_dev->async, SIGIO, POLL_IN);
                                        }
                                #endif
                                }else{
                                        gf516m_debug(ERR_LOG, "%s: failed to allocate nvg data, notify_application is 0x%x.\n", __func__, notify_application);
                                }
                        }else{
                        //        input_report_key(gf516m_dev->input, GF516M_NVG_TOUCH_KEY, 1);
                        //      input_sync(gf516m_dev->input);
                                gf516m_debug(ERR_LOG, "receive down event ===== %d\n",tmp);
                        }
                        
                }else if(tmp == GF_NVG_LEFT_EVENT){
                        gf516m_debug(ERR_LOG, "nvg key left event ===== %d\n",tmp);
                        if(gf516m_dev->lastEvent == GF_KEYUP_EVENT){
                                input_report_key(gf516m_dev->input, GF516M_NVG_LEFT_KEY, 1);
                                input_sync(gf516m_dev->input);
                                input_report_key(gf516m_dev->input, GF516M_NVG_LEFT_KEY, 0);
                                input_sync(gf516m_dev->input);
                                gf516m_debug(ERR_LOG, "receive left down-up event ===== %d\n",tmp);
                        }else{
                                input_report_key(gf516m_dev->input, GF516M_NVG_LEFT_KEY, 1);
                                input_sync(gf516m_dev->input);
                                gf516m_debug(ERR_LOG, "receive left down event ===== %d\n",tmp);
                        }
                }else if(tmp == GF_NVG_RIGHT_EVENT){
                        gf516m_debug(ERR_LOG, "nvg key right event ===== %d\n",tmp);
                        if(gf516m_dev->lastEvent == GF_KEYUP_EVENT){
                                input_report_key(gf516m_dev->input, GF516M_NVG_RIGHT_KEY, 1);
                                input_sync(gf516m_dev->input);
                                input_report_key(gf516m_dev->input, GF516M_NVG_RIGHT_KEY, 0);
                                input_sync(gf516m_dev->input);
                                gf516m_debug(ERR_LOG, "receive right down-up event ===== %d\n",tmp);
                        }else{
                                input_report_key(gf516m_dev->input, GF516M_NVG_RIGHT_KEY, 1);
                                input_sync(gf516m_dev->input);
                                gf516m_debug(ERR_LOG, "receive right down event ===== %d\n",tmp);
                        }
                }else if(tmp == GF_NVG_UP_EVENT){
                        gf516m_debug(ERR_LOG, "nvg key up event ===== %d\n",tmp);
                        if(gf516m_dev->lastEvent == GF_KEYUP_EVENT){
                                input_report_key(gf516m_dev->input, GF516M_NVG_UP_KEY, 1);
                                input_sync(gf516m_dev->input);
                                input_report_key(gf516m_dev->input, GF516M_NVG_UP_KEY, 0);
                                input_sync(gf516m_dev->input);
                                gf516m_debug(ERR_LOG, "receive up down-up event ===== %d\n",tmp);
                        }else{
                                input_report_key(gf516m_dev->input, GF516M_NVG_UP_KEY, 1);
                                input_sync(gf516m_dev->input);
                                gf516m_debug(ERR_LOG, "receive up down event ===== %d\n",tmp);
                        }
                }else if(tmp == GF_NVG_DOWN_EVENT){
                        gf516m_debug(ERR_LOG, "nvg key down event ===== %d\n",tmp);
                        if(gf516m_dev->lastEvent == GF_KEYUP_EVENT){
                                input_report_key(gf516m_dev->input, GF516M_NVG_DOWN_KEY, 1);
                                input_sync(gf516m_dev->input);
                                input_report_key(gf516m_dev->input, GF516M_NVG_DOWN_KEY, 0);
                                input_sync(gf516m_dev->input);
                                gf516m_debug(ERR_LOG, "receive down down-up event ===== %d\n",tmp);
                        }else{
                                input_report_key(gf516m_dev->input, GF516M_NVG_DOWN_KEY, 1);
                                input_sync(gf516m_dev->input);
                                gf516m_debug(ERR_LOG, "receive down down event ===== %d\n",tmp);
                        }
                }else if(tmp == GF_NVG_WEIGHT_PRESS_EVENT){
                        gf516m_debug(ERR_LOG, "nvg key weight press event ===== %d\n",tmp);
                        if(gf516m_dev->lastEvent == GF_KEYUP_EVENT){
                                input_report_key(gf516m_dev->input, GF516M_NVG_TOUCH_KEY, 1);
                                input_sync(gf516m_dev->input);
                                input_report_key(gf516m_dev->input, GF516M_NVG_TOUCH_KEY, 0);
                                input_sync(gf516m_dev->input);
                        }else{
                                input_report_key(gf516m_dev->input, GF516M_NVG_TOUCH_KEY, 1);
                                input_sync(gf516m_dev->input);
                        }
                }else if(tmp == GF_NVG_LIGHT_PRESS_EVENT){

                        gf516m_debug(ERR_LOG, "NO SUPPORT ===== %d\n",tmp);

                        /*

                        if(gf516m_dev->lastEvent == GF_KEYUP_EVENT){
                                input_report_key(gf516m_dev->input, GF516M_NVG_TOUCH_KEY, 1);
                                input_sync(gf516m_dev->input);
                                input_report_key(gf516m_dev->input, GF516M_NVG_TOUCH_KEY, 0);
                                input_sync(gf516m_dev->input);
                        }else{
                                input_report_key(gf516m_dev->input, GF516M_NVG_TOUCH_KEY, 1);
                                input_sync(gf516m_dev->input);
                        }
                        */
                }else{
                        gf516m_debug(ERR_LOG, "ERROR nvg key event ===== %d\n",tmp);
                }
                
                break;
	case GF_IOC_RESET_PIN_TEST:
		if (NULL == pDci) {
			gf516m_debug(ERR_LOG, "[%s] :secure init is not ok,pDci is NULL.\n", __func__);
			retval = -ENODEV;
			break;
		}
		mutex_lock(&gf516m_dev->mode_lock);
		gf516m_debug(INFO_LOG, "[%s]: reset test.\n", __func__);
		gf516m_disable_irq(gf516m_dev);
		gf516m_disable_watchdog(gf516m_dev);

		gf516m_spi_write_byte(gf516m_dev, GF516M_BUFFER_STATUS_L, 0);
	         gf516m_hw_reset(130);	// make sure has enough time to get reset irq status.
		gf516m_spi_read_byte(gf516m_dev, GF516M_BUFFER_STATUS_L, &irq_status_l);
		gf516m_debug(INFO_LOG, "[%s]: irq_status_l=0x%x.\n", __func__,  irq_status_l);
		gf516m_spi_write_byte(gf516m_dev, GF516M_BUFFER_STATUS_L, 0);

		gf516m_spi_write_byte(gf516m_dev, GF516M_MODE_STATUS, gf516m_dev->mode);
		gf516m_debug(INFO_LOG, "[%s] : Change the sensor to saved mode ater reset test. mode = 0x%x.\n", __func__, gf516m_dev->mode);

		gf516m_enable_watchdog(gf516m_dev);
		gf516m_enable_irq(gf516m_dev);
		mutex_unlock(&gf516m_dev->mode_lock);

		if (irq_status_l & (0x1 << 7)) {
			gf516m_debug(INFO_LOG, "[%s]: reset test success.\n", __func__);
			retval = 0;
		} else {
			gf516m_debug(INFO_LOG, "[%s]: reset test failed.\n", __func__);
			retval = -ENODEV;
		}
		break;
        default:
                gf516m_debug(ERR_LOG, "gf516m doesn't support this command(%d)\n", cmd);
                break;
        }

	FUNC_EXIT();
	return retval;
}

static unsigned int gf516m_poll(struct file *filp, struct poll_table_struct *wait)
{
#if 1
	gf516m_debug(ERR_LOG, "Not support poll operation in normal driver\n");
#else
	struct gf516m_dev *gf516m_dev = filp->private_data;
	//debug for GF516M
	gf516m_spi_read_byte(gf516m_dev, GF516M_BUFFER_STATUS, &gf516m_dev->buf_status);
	if((gf516m_dev->buf_status & (0x1<<7)) == GF516M_BUF_STA_READY) {
		return (POLLIN|POLLRDNORM);
	} else {
		gf516m_debug(INFO_LOG, "%s: Poll no data\n", __func__);
	}
#endif
	return 0;
}

static void gf516m_enable_watchdog(struct gf516m_dev *gf516m_dev)
{
#ifdef GF516M_WATCHDOG
        //if((gf516m_dev->device_available == 1) && (gf516m_dev->has_esd_watchdog == 1)){
         if ( 0 == timer_pending(&gf516m_dev->gf516m_timer)) {
                gf516m_dev->gf516m_timer.expires = jiffies + GF516M_TIMER_EXPIRE;
                add_timer(&gf516m_dev->gf516m_timer);
         } else {
		gf516m_debug(INFO_LOG, "[%s]:timer already running.\n", __func__);
	}
#endif
	return;
}

static void gf516m_disable_watchdog(struct gf516m_dev *gf516m_dev)
{
#ifdef GF516M_WATCHDOG
        //if((gf516m_dev->device_available == 1) && (gf516m_dev->has_esd_watchdog == 1)){
        if ( 1 == timer_pending(&gf516m_dev->gf516m_timer)) {
                del_timer_sync(&gf516m_dev->gf516m_timer);
        } else {
		gf516m_debug(INFO_LOG, "[%s]:timer already stopping.\n", __func__);
	}
#endif
	return;
}

static void gf516m_enable_irq(struct gf516m_dev *gf516m_dev)
{
	enable_irq(gf516m_dev->spi->irq);
	gf516m_debug(INFO_LOG, "[%s] :enable interrupt!\n", __func__);
}

static void gf516m_disable_irq(struct gf516m_dev *gf516m_dev)
{
	disable_irq(gf516m_dev->spi->irq);
	gf516m_debug(INFO_LOG, "[%s] :disable interrupt!\n", __func__);
}

#ifdef GF516M_WATCHDOG
static void gf516m_timer_work(struct work_struct *work)
{
        struct gf516m_dev *gf516m_dev;
        u8 value, value2;
        u8 esd_ret;
        int fw_update_flag = 0;
        u8 fw_update_cnt = 0;

	if(work == NULL)
	{
		gf516m_debug(INFO_LOG, "%s work paramter is NULL\n", __func__);
		return;
	}
	gf516m_dev = container_of(work, struct gf516m_dev, spi_work);

	disable_irq(gf516m_dev->spi->irq);
	//TODO: ESD check inside TEE
	mutex_lock(&gf516m_dev->dci_lock);
	pDci->esd_check.cmd = 1;
	gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_ESD_CHECK);
	esd_ret = pDci->esd_check.ret;
	mutex_unlock(&gf516m_dev->dci_lock);
	enable_irq(gf516m_dev->spi->irq);

        if(1 == esd_ret){
                gf516m_debug(ERR_LOG, "[%s] :esd check failed, reset sensor.\n", __func__);
                gf516m_dev->esd_err_cnt++;
                if (gf516m_dev->esd_err_cnt <=  3) {
                        disable_irq(gf516m_dev->spi->irq);
		      gf516m_disable_watchdog(gf516m_dev);
                        gf516m_hw_reset(100);
		      //clear reset irq status after reset fp.
		      gf516m_spi_write_byte(gf516m_dev, GF516M_BUFFER_STATUS_L, 0);
		       mutex_lock(&gf516m_dev->mode_lock);
                        gf516m_set_mode(gf516m_dev, gf516m_dev->mode);
		       mutex_unlock(&gf516m_dev->mode_lock);
                        gf516m_debug(ERR_LOG, "[%s] :esd check failed, change sensor to origin mode. mode = 0x%x.\n", __func__, gf516m_dev->mode);
		      gf516m_enable_watchdog(gf516m_dev);
                        enable_irq(gf516m_dev->spi->irq);
                } else {
                        gf516m_dev->esd_err_cnt = 0;
                        disable_irq(gf516m_dev->spi->irq);
                        gf516m_disable_watchdog(gf516m_dev);

		do {
		      fw_update_flag = 0;
                        mutex_lock(&gf516m_dev->dci_lock);

		      gf516m_hw_power(0);
		      mdelay(100);
		      gf516m_hw_power(1);
                        gf516m_pull_miso_high(gf516m_dev);
                        udelay(100);
                        gf516m_gpio_cfg();

                        gf516m_debug(INFO_LOG, "[%s]: fw has been damaged, need upgrade.\n", __func__);

                        pDci->upgrade_fw_cfg.cmd = GF_UPDATE_FW_DAMAGE_CMD;
                        gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_UPGRADE_FW_CFG);
                        if (GF_UPDATE_FW_RESULT_OK == pDci->upgrade_fw_cfg.ret) {
                                gf516m_debug(INFO_LOG, "[%s] :update firmware success.\n", __func__);
                                gf516m_hw_reset(100);
                                pDci->upgrade_fw_cfg.cmd =GF_UPDATE_CFG_CMD;
                                gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_UPGRADE_FW_CFG);
                                if (GF_UPDATE_CFG_RESULT_OK == pDci->upgrade_fw_cfg.ret) {
                                        gf516m_debug(INFO_LOG, "[%s]: update config success.\n", __func__);
                                } else {
                                        gf516m_debug(ERR_LOG, "[%s]: update config failed.\n", __func__);
				     fw_update_flag = 1;
                                }
                        } else {
                                gf516m_debug(ERR_LOG, "[%s]: update firmware failed. ret =%d.\n", __func__, pDci->upgrade_fw_cfg.ret);
			      fw_update_flag = 1;
                        }
                        mutex_unlock(&gf516m_dev->dci_lock);

		      fw_update_cnt ++;
		      gf516m_debug(INFO_LOG, "[%s]:fw_update_flag =%d,fw_update_cnt = %d.\n", __func__, fw_update_flag, fw_update_cnt);
		} while (fw_update_flag && (fw_update_cnt < 5));

		      //clear reset irq status after reset fp.
		      gf516m_spi_write_byte(gf516m_dev, GF516M_BUFFER_STATUS_L, 0);
                        mutex_lock(&gf516m_dev->mode_lock);
                        gf516m_set_mode(gf516m_dev, gf516m_dev->mode);
                        gf516m_debug(INFO_LOG, "[%s] :resume fp to origin mode 0x%x.\n", __func__, gf516m_dev->mode);
		      mutex_unlock(&gf516m_dev->mode_lock);
                        gf516m_enable_watchdog(gf516m_dev);
                        enable_irq(gf516m_dev->spi->irq);
                }
        } else if (0 == esd_ret) {
                gf516m_dev->esd_err_cnt = 0;
        }
        return;
}

static void gf516m_timer_func(unsigned long arg)
{
	struct gf516m_dev *gf516m_dev = (struct gf516m_dev*)arg;
	if(gf516m_dev == NULL)
	{
		gf516m_debug(ERR_LOG, "%s can't get the gf516m_dev\n",__func__);
		return;
	}
	schedule_work(&gf516m_dev->spi_work);
	//change to 20s temporary, formal version is 2s
	mod_timer(&gf516m_dev->gf516m_timer, jiffies + GF516M_TIMER_EXPIRE);
}
#endif


/* -------------------------------------------------------------------- */
/* devfs								*/
/* -------------------------------------------------------------------- */
static ssize_t gf516m_debug_show(struct device *dev, 
		struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct gf516m_dev *gf516m_dev =  dev_get_drvdata(dev);

	gf516m_debug(INFO_LOG, "%s: Show debug_level = 0x%x\n", __func__, g_debug_level);
	len += sprintf(buf, "VendorID:0x%x.\nDebug level:0x%x.\n",  gf516m_dev->vendor_id, g_debug_level);
	len += snprintf(buf+len, 18, " ProductID:%s.\n", gf516m_dev->chip_version);
	len += sprintf(buf +len , " FW Version: %x.%x.%x.\n", gf516m_dev->chip_version[7],  gf516m_dev->chip_version[8], gf516m_dev->chip_version[9]);
	//len += sprintf(buf +len, "VendorID:0x%x.\nDebug level:0x%x.\n",  gf516m_dev->vendor_id, g_debug_level);

	return len;
	//return (sprintf(buf, "Debug level:0x%x.\nVendorID:0x%x.\n", g_debug_level, gf516m_dev->vendor_id));
}
# if 1
static ssize_t gf516m_debug_store(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}
#else
static ssize_t gf516m_debug_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval = 0;
	struct gf516m_dev *gf516m_dev =  dev_get_drvdata(dev);

	if (!strncmp(buf, "-10", 3)){
		gf516m_debug(INFO_LOG, "%s: parameter is -10, load secure driver===============\n", __func__);

		gf516m_load_secdrv();
		//gf516m_close_secdrv();
		gf516m_debug(INFO_LOG, "gf516m_load_secdrv load secure driver finished!!!\n");

		gf516m_dev->secspi_init_ok = 1;

	}else if (!strncmp(buf, "-11", 3)){
		u8 temp_str[32] = "Hello, secure world!";
		gf516m_debug(INFO_LOG, "%s: parameter is -11, test DCI interface============\n", __func__);

		pDci->testcase.len = strlen(temp_str);
		memcpy(pDci->testcase.data, temp_str, 32);
		gf516m_debug(INFO_LOG, "%s: before call DCI interface, string is %s\n", __func__, pDci->testcase.data);
		gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_TESTCASE);   //SECDEV_CMD_TESTCASE
		gf516m_debug(INFO_LOG, "%s: after call DCI interface, string is %s\n", __func__, pDci->testcase.data);

	}else if (!strncmp(buf, "-12", 3)){
		gf516m_debug(INFO_LOG, "%s: parameter is -12, init fingerprint chip============\n", __func__);

		gf516m_gpio_cfg();
		gf516m_hw_reset(80);

		mutex_lock(&gf516m_dev->dci_lock);
		gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_FP_INIT);
		mutex_unlock(&gf516m_dev->dci_lock);

		gf516m_dev->secspi_init_ok = 1;
		gf516m_dev->device_available = 1;
		gf516m_dev->mode = GF516M_IMAGE_MODE;

		#ifdef GF516M_WATCHDOG
		INIT_WORK(&gf516m_dev->spi_work, gf516m_timer_work);
		init_timer(&gf516m_dev->gf516m_timer);
		gf516m_dev->gf516m_timer.function = gf516m_timer_func;
		gf516m_dev->gf516m_timer.data = (unsigned long)gf516m_dev;
		gf516m_enable_watchdog(gf516m_dev);
		#endif

		#if defined(CONFIG_HAS_EARLYSUSPEND)
		gf516m_debug(INFO_LOG ,"[%s] : register_early_suspend\n", __func__);
		gf516m_dev->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1;
		gf516m_dev->early_suspend.suspend = gf516m_early_suspend,
		gf516m_dev->early_suspend.resume = gf516m_late_resume,
		register_early_suspend (&gf516m_dev->early_suspend);
		#endif

		gf516m_dev->lastEvent = GF_NOEVENT;
		gf516m_dev->sig_count = 0;
		gf516m_debug(INFO_LOG, "%s: secure SPI init finished======\n", __func__);

	}else if (!strncmp(buf, "-13", 3)){
		gf516m_debug(INFO_LOG, "%s: parameter is -13, read fw version============\n", __func__);
		pDci->fw_version.len = 10;
		pDci->fw_version.addr = GF516M_FW_VERSION;
		memset(pDci->fw_version.fw, 0x00, 10);
		gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_READ_FW_VERSION);
		gf516m_debug(INFO_LOG, "%s: through DCI 0x%x:0x%x:0x%x:0x%x:0x%x:0x%x, %02d.%02d.%02d\n", __func__,
			pDci->fw_version.fw[0], pDci->fw_version.fw[1], pDci->fw_version.fw[2],
			pDci->fw_version.fw[3], pDci->fw_version.fw[4], pDci->fw_version.fw[5],
			pDci->fw_version.fw[7], pDci->fw_version.fw[8], pDci->fw_version.fw[9]);

		memset(gf516m_dev->buffer, 0x55, 10);
		gf516m_spi_read_bytes(gf516m_dev, GF516M_FW_VERSION, 10, gf516m_dev->buffer);
		gf516m_debug(INFO_LOG, "%s: through read method 0x%x:0x%x:0x%x:0x%x:0x%x:0x%x, %02d.%02d.%02d\n", __func__,
			gf516m_dev->buffer[0], gf516m_dev->buffer[1], gf516m_dev->buffer[2], 
			gf516m_dev->buffer[3], gf516m_dev->buffer[4], gf516m_dev->buffer[5],
			gf516m_dev->buffer[7], gf516m_dev->buffer[8], gf516m_dev->buffer[9]);

	}else if (!strncmp(buf, "-14", 3)){
		gf516m_debug(INFO_LOG, "%s: parameter is -14, test spi read/write function============\n", __func__);

        }else if (!strncmp(buf, "-15", 3)){
                gf516m_debug(INFO_LOG, "%s: parameter is -15, configure fingerprint interrupt============\n", __func__);
                gf516m_irq_cfg();
                retval = request_threaded_irq(gf516m_dev->spi->irq, NULL, gf516m_irq,
                                IRQF_TRIGGER_RISING | IRQF_ONESHOT, dev_name(&(gf516m_dev->spi->dev)), gf516m_dev);
                if(!retval) {
                        gf516m_debug(INFO_LOG, "%s irq thread request success!\n", __func__);
                }else{
                        gf516m_debug(ERR_LOG, "%s irq thread request failed, retval=%d\n", __func__, retval);
                }
                //gf516m_dev->irq_count = 1;
                gf516m_disable_irq(gf516m_dev);


	}else if (!strncmp(buf, "-16", 3)){
		gf516m_debug(INFO_LOG, "%s: parameter is -16, enable fingerprint interrupt============\n", __func__);
		//gf516m_irq_cfg();

		gf516m_enable_irq(gf516m_dev);
		gf516m_debug(INFO_LOG, "%s enable interrupt!\n", __func__);

	}else if (!strncmp(buf, "-17", 3)){
		gf516m_debug(INFO_LOG, "%s: parameter is -17, disable fingerprint interrupt============\n", __func__);
		gf516m_disable_irq(gf516m_dev);

	}else if (!strncmp(buf, "-18", 3)){
		gf516m_debug(INFO_LOG, "%s: parameter is -18, upgrade fw and cfg============\n", __func__);

		gf516m_disable_irq(gf516m_dev);
		gf516m_disable_watchdog(gf516m_dev);
		gf516m_hw_reset(80);

		mutex_lock(&gf516m_dev->dci_lock);
		pDci->upgrade_fw_cfg.cmd = 0;
		gf516m_send_cmd_secdrv(GF516M_SECDEV_CMD_UPGRADE_FW_CFG);
		mutex_unlock(&gf516m_dev->dci_lock);

		gf516m_enable_irq(gf516m_dev);
		gf516m_enable_watchdog(gf516m_dev);

	}else if (!strncmp(buf, "-19", 3)){
		gf516m_debug(INFO_LOG, "%s: parameter is -19, gpio test============\n", __func__);

		gf516m_pull_miso_high(gf516m_dev);

        }else if (!strncmp(buf, "-20", 3)){
                gf516m_debug(INFO_LOG, "%s: parameter is -20, start ESD check timer============\n", __func__);
                gf516m_enable_watchdog(gf516m_dev);
                //gf516m_dev->has_esd_watchdog = 1;

        }else if (!strncmp(buf, "-21", 3)){
                gf516m_debug(INFO_LOG, "%s: parameter is -21, stop ESD check timer============\n", __func__);
                gf516m_disable_watchdog(gf516m_dev);
                //gf516m_dev->has_esd_watchdog = 0;

	}else if (!strncmp(buf, "-50", 3)){
		gf516m_debug(INFO_LOG, "%s: parameter is -50, load test TA, and send cmd============\n", __func__);
		gf516m_load_TA();
		//gf516m_close_TA();
		gf516m_send_cmd_TA(TLTEST_CMD_1);
	} else if (!strncmp(buf, "-51", 3)) {
		gf516m_dev->enable_sleep_mode = 1;
		gf516m_debug(INFO_LOG, "[%s] :,set sleep mode after sreen off.enable_sleep_mode = 0x%x.\n", __func__, gf516m_dev->enable_sleep_mode);
	} else if (!strncmp(buf, "-52", 3)) {
		gf516m_dev->enable_sleep_mode = 0;
		gf516m_debug(INFO_LOG, "[%s] :set ff mode after sreen off.enable_sleep_mode = 0x%x.\n", __func__, gf516m_dev->enable_sleep_mode);
	}

	return count;
}
#endif

static ssize_t gf516m_mode_show(struct device *dev, 
		struct device_attribute *attr, char *buf)
{
	struct gf516m_dev *gf516m_dev =  dev_get_drvdata(dev);

	u8 mode = 0;

	gf516m_get_mode(gf516m_dev, &mode);
	gf516m_debug(INFO_LOG, "gf516m_mode_show, mode is 0x%02x\n", mode);

	return (sprintf(buf, "0x%02x\n", mode));
}
static ssize_t gf516m_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct gf516m_dev *gf516m_dev =  dev_get_drvdata(dev);

        gf516m_debug(INFO_LOG, "%s, gf516m set mode = %d\n", __func__, (buf[0]-48));
        mutex_lock(&gf516m_dev->mode_lock);
        if (!strncmp(buf, "0", 1)){
                //GF516M_IMAGE_MODE
                gf516m_set_mode(gf516m_dev, GF516M_IMAGE_MODE);
                gf516m_dev->mode = GF516M_IMAGE_MODE;

	}else if(!strncmp(buf, "1", 1)){
		//GF516M_KEY_MODE
		gf516m_set_mode(gf516m_dev, GF516M_KEY_MODE);
		gf516m_dev->mode = GF516M_KEY_MODE;

	}else if(!strncmp(buf, "2", 1)){
		//GF516M_SLEEP_MODE
		//gf516m_set_mode(gf516m_dev, GF516M_SLEEP_MODE);
		//gf516m_dev->mode = GF516M_SLEEP_MODE;
		gf516m_debug(ERR_LOG, "%s: disable sleep mode for gf516m temporary\n", __func__);

	}else if(!strncmp(buf, "3", 1)){
		//GF516M_FF_MODE
		gf516m_set_mode(gf516m_dev, GF516M_FF_MODE);
		//gf516m_dev->mode = GF516M_FF_MODE;

	}else if(!strncmp(buf, "4", 1)){
		//GF516M_DEBUG_MODE
		gf516m_set_mode(gf516m_dev, GF516M_DEBUG_MODE);
		gf516m_dev->mode = GF516M_DEBUG_MODE;
		gf516m_debug(ERR_LOG, "%s: change to debug mode for fp data collect\n", __func__);

	}else if(!strncmp(buf, "5", 1)){
		//GF516M_DEBUG_MODE
		gf516m_set_mode(gf516m_dev, GF516M_DEBUG_MODE_NOTX);
		gf516m_dev->mode = GF516M_DEBUG_MODE_NOTX;
		gf516m_debug(ERR_LOG, "%s: change to debug mode without tx for fp data collect\n", __func__);

        }else if(!strncmp(buf, "6", 1)){
                //GF516M_DEBUG_MODE
                gf516m_set_mode(gf516m_dev, GF516M_NVG_MODE_NOTX);
                gf516m_dev->mode = GF516M_NVG_MODE_NOTX;
                gf516m_debug(ERR_LOG, "%s: change to nvg mode without tx for fp data collect\n", __func__);

        }else if(!strncmp(buf, "7", 1)){
                //GF516M_DEBUG_MODE
                gf516m_set_mode(gf516m_dev, GF516M_NVG_MODE);
                gf516m_dev->mode = GF516M_NVG_MODE;
                gf516m_debug(ERR_LOG, "%s: change to nvg mode for fp data collect\n", __func__);

        }else{
                //error mode
                gf516m_debug(ERR_LOG, "%s, set wrong mode %d\n",__func__, (buf[0]-'1'));

        }
	mutex_unlock(&gf516m_dev->mode_lock);
        return strnlen(buf, count);
}


static DEVICE_ATTR(debug, S_IRUGO|S_IWUSR, gf516m_debug_show, gf516m_debug_store);
static DEVICE_ATTR(mode, S_IRUGO|S_IWUSR, gf516m_mode_show, gf516m_mode_store);

static struct attribute *gf516m_debug_attrs[] = {
	&dev_attr_debug.attr,
	&dev_attr_mode.attr,
	NULL
};

static const struct attribute_group gf516m_debug_attr_group = {
	.attrs = gf516m_debug_attrs,
	.name = "debug"
};

/* -------------------------------------------------------------------- */
/* device function								  */
/* -------------------------------------------------------------------- */
static int gf516m_open(struct inode *inode, struct file *filp)
{
	struct gf516m_dev *gf516m_dev;
	int status = -ENXIO;

	FUNC_ENTRY();
	mutex_lock(&device_list_lock);

	list_for_each_entry(gf516m_dev, &device_list, device_entry) {
		if(gf516m_dev->devt == inode->i_rdev) {
			gf516m_debug(INFO_LOG, "%s, Found\n", __func__);
			status = 0;
			break;
		}
	}

	if(status == 0){
		mutex_lock(&gf516m_dev->buf_lock);
		if( gf516m_dev->buffer == NULL) {
			gf516m_dev->buffer = kzalloc(bufsiz + GF516M_RDATA_OFFSET, GFP_KERNEL);
			if(gf516m_dev->buffer == NULL) {
				gf516m_debug(ERR_LOG, "%s, allocate gf516m_dev->buffer failed\n", __func__);
				status = -ENOMEM;
			}
		}
		mutex_unlock(&gf516m_dev->buf_lock);

		if(status == 0) {
			gf516m_dev->users++;
			filp->private_data = gf516m_dev;
			nonseekable_open(inode, filp);
			gf516m_debug(INFO_LOG, "%s, Succeed to open device. irq = %d\n", __func__, gf516m_dev->spi->irq);
			/*enale irq  at the first open*/
			if(gf516m_dev->users == 1){
				//enable irq by ioctl later
				//gf516m_enable_irq(gf516m_dev);
			}
		}
	} else {
		gf516m_debug(ERR_LOG, "%s, No device for minor %d\n", __func__, iminor(inode));
	}
	mutex_unlock(&device_list_lock);
	FUNC_EXIT();
	return status;
}

#ifdef GF516M_FASYNC
static int gf516m_fasync(int fd, struct file *filp, int mode)
{
	struct gf516m_dev *gf516m_dev = filp->private_data;
	int ret;

	FUNC_ENTRY();
	ret = fasync_helper(fd, filp, mode, &gf516m_dev->async);
	FUNC_EXIT();
	return ret;
}
#endif

static int gf516m_release(struct inode *inode, struct file *filp)
{
	struct gf516m_dev *gf516m_dev;
	int    status = 0;

	FUNC_ENTRY();
	mutex_lock(&device_list_lock);
	gf516m_dev = filp->private_data;
	filp->private_data = NULL;

        /*last close??*/
        gf516m_dev->users --;
        if(!gf516m_dev->users) {
                gf516m_debug(INFO_LOG, "%s, disble_irq. irq = %d\n", __func__, gf516m_dev->spi->irq);
                gf516m_disable_irq(gf516m_dev);
                gf516m_disable_watchdog(gf516m_dev);
                //gf516m_dev->has_esd_watchdog = 0;
        }
        mutex_unlock(&device_list_lock);
        gf516m_dev->device_available = 0;
        FUNC_EXIT();
        return status;
}


static struct class *gf516m_spi_class;

static const struct file_operations gf516m_fops = {
	.owner =	THIS_MODULE,
	/* REVISIT switch to aio primitives, so that userspace
	* gets more complete API coverage.	It'll simplify things
	* too, except for the locking.
	*/
	.write =	gf516m_write,
	.read =		gf516m_read,
	.unlocked_ioctl = gf516m_ioctl,
	//.compat_ioctl = gf516m_compat_ioctl,
	.open =		gf516m_open,
	.release =	gf516m_release,
	.poll	= gf516m_poll,
#ifdef GF516M_FASYNC
	.fasync = gf516m_fasync,
#endif
};
/* The main reason to have this class is to make mdev/udev create the
 * /dev/spidevB.C character device nodes exposing our userspace API.
 * It also simplifies memory management.
 */

/*-------------------------------------------------------------------------*/

static int gf516m_probe(struct spi_device *spi)
{
	struct gf516m_dev	 *gf516m_dev;
	unsigned long minor;
	int status = -EINVAL;
	struct devinfo_struct *dev_goodix;
	FUNC_ENTRY();
	/* Allocate driver data */
	gf516m_dev = kzalloc(sizeof(*gf516m_dev), GFP_KERNEL);
	if (!gf516m_dev){
		gf516m_debug(ERR_LOG,"%s, Failed to alloc memory for gf516m device.\n", __func__);
		FUNC_EXIT();
		return -ENOMEM;
	}

        g_gf516m_dev = gf516m_dev;
        /* Initialize the driver data */
        gf516m_dev->spi = spi;
        spin_lock_init(&gf516m_dev->spi_lock);
        mutex_init(&gf516m_dev->buf_lock);
        mutex_init(&gf516m_dev->dci_lock);
        mutex_init(&gf516m_dev->mode_lock);

        INIT_LIST_HEAD(&gf516m_dev->device_entry);
        gf516m_dev->cs_gpio    = -EINVAL;
        gf516m_dev->irq_gpio   = -EINVAL;
        gf516m_dev->reset_gpio = -EINVAL;
        gf516m_dev->device_available = 0;
        gf516m_dev->is_ff_mode = 0;
        //gf516m_dev->has_esd_watchdog = 0;
        gf516m_dev->secspi_init_ok = 0;
        gf516m_dev->event_type = GF_NOEVENT;
        gf516m_dev->spi->irq = 0;
        gf516m_dev->probe_finish = 0;
        gf516m_dev->fw_need_upgrade = 0;
        gf516m_dev->enable_sleep_mode = 0;

	/*setup gf516m configurations.*/
	gf516m_debug(INFO_LOG, "%s, Setting gf516m device configuration.\n", __func__);
	/*SPI parameters.*/
	gf516m_dev->spi->mode = SPI_MODE_0; //CPOL=CPHA=0
        gf516m_dev->spi->max_speed_hz = 1*1000*1000; //1MHZ
	gf516m_dev->spi->irq = mt_gpio_to_irq(CUST_EINT_FP_EINT_NUM);//mt_gpio_to_irq(GPIO_FP_INT_PIN);//GF516M_IRQ_NUM;//gpio_to_irq(gf516m_IRQ);//
	//gf516m_dev->spi->irq = CUST_EINT_FP_EINT_NUM;//mt_gpio_to_irq(GPIO_FP_INT_PIN);//GF516M_IRQ_NUM;//gpio_to_irq(gf516m_IRQ);//
	gf516m_dev->spi->bits_per_word = 8;
	spi_setup(gf516m_dev->spi);   //not used for MTK platform
	spi_set_drvdata(spi, gf516m_dev);
	gf516m_debug(INFO_LOG, "%s, gf516m interrupt NO. = %d\n", __func__, gf516m_dev->spi->irq);

	/*enable the power*/
	gf516m_hw_power(1);
	gf516m_bypass_flash_cfg();

	/* check 9p chip version, if not pass, then not create device */
	gf516m_pull_miso_high(gf516m_dev);
        udelay(100);
	gf516m_gpio_cfg();

	dev_goodix = (struct devinfo_struct*)kmalloc(sizeof(struct devinfo_struct), GFP_KERNEL);
    
    dev_goodix->device_type = "Fingerprint";
    dev_goodix->device_vendor = "Goodix";
    dev_goodix->device_ic = "GF318M";
    dev_goodix->device_version = DEVINFO_NULL;
    dev_goodix->device_module = DEVINFO_NULL;
    dev_goodix->device_info = DEVINFO_NULL;

#ifdef GF516M_PROBE_CHIP
        enable_clock(MT_CG_PERI_SPI0, "spi");
        memset(tmp_buf, 0x00, 24);
        status = gf516m_check_9p_chip(gf516m_dev);
        if (status != 0) {
                gf516m_debug(ERR_LOG, "%s, 9p chip version not detect\n", __func__);
                goto err2;
        }

	/* check fp chip ID through SPI */
	/* read fw version or chip ID through SPI interface */
        mdelay(80);
        memset(tmp_buf, 0x00, 24);
        gf516m_spi_read_bytes_nwd(gf516m_dev, 0x8000, 10, tmp_buf);
        gf516m_debug(INFO_LOG, "%s: read data 0x%x:0x%x:0x%x:0x%x:0x%x:0x%x, %02d.%02d.%02d\n", __func__,
                                        tmp_buf[GF516M_RDATA_OFFSET], tmp_buf[GF516M_RDATA_OFFSET+1], tmp_buf[GF516M_RDATA_OFFSET+2], 
                                        tmp_buf[GF516M_RDATA_OFFSET+3], tmp_buf[GF516M_RDATA_OFFSET+4], tmp_buf[GF516M_RDATA_OFFSET+5],
                                        tmp_buf[GF516M_RDATA_OFFSET+7], tmp_buf[GF516M_RDATA_OFFSET+ 8], tmp_buf[GF516M_RDATA_OFFSET+9]);
        tmp_buf[7] = '\0';

        gf516m_debug(INFO_LOG, "[%s]: PID is %s.\n", __func__, &tmp_buf[GF516M_RDATA_OFFSET]);
        gf516m_debug(INFO_LOG, "[%s]: fw version is %d.%d.%d.\n", __func__, tmp_buf[GF516M_RDATA_OFFSET+7], tmp_buf[GF516M_RDATA_OFFSET+8], tmp_buf[GF516M_RDATA_OFFSET+9]);
        if (memcmp(&tmp_buf[GF516M_RDATA_OFFSET], GF_FP_PRODUCT_ID, 6)) {
                gf516m_debug(ERR_LOG, "%s, fw version error, need upgrade, reset chip again\n", __func__);
                gf516m_dev->fw_need_upgrade = 1;

		/* reset sensor again */
		gf516m_pull_miso_high(gf516m_dev);
                udelay(100);
		gf516m_gpio_cfg();

                memset(tmp_buf, 0x00, 24);
                status = gf516m_check_9p_chip(gf516m_dev);
                if(status != 0){
                        gf516m_debug(ERR_LOG, "%s, 9p chip version not detect\n", __func__);
                        goto err2;
                }
                mdelay(10);

		status = gf516m_fw_upgrade_prepare(gf516m_dev);
                if (status != 0) {
                        gf516m_debug(ERR_LOG, "%s, fw upgrade prepare failed\n", __func__);
                        goto err2;
                }
        }
        disable_clock(MT_CG_PERI_SPI0, "spi");
#endif

	/* If we can allocate a minor number, hook up this device.
	* Reusing minors is fine so long as udev or mdev is working.
	*/
	mutex_lock(&device_list_lock);
	minor = find_first_zero_bit(minors, N_SPI_MINORS);
	if (minor < N_SPI_MINORS) {
		struct device *dev;

		status = sysfs_create_group(&spi->dev.kobj,&gf516m_debug_attr_group);
		if(status){
			gf516m_debug(ERR_LOG,"%s, Failed to create sysfs file.\n", __func__);
			status = -ENODEV;
			goto err;
		}
		gf516m_debug(INFO_LOG,"%s, Success create sysfs file.\n", __func__);

		gf516m_dev->devt = MKDEV(SPIDEV_MAJOR, minor);
		dev = device_create(gf516m_spi_class, &spi->dev, gf516m_dev->devt, gf516m_dev, DEV_NAME);
		status = IS_ERR(dev) ? PTR_ERR(dev) : 0;
	} else {
		gf516m_debug(ERR_LOG, "%s, no minor number available!\n", __func__);
		goto err;
	}
	if (status == 0) {
		set_bit(minor, minors);
		list_add(&gf516m_dev->device_entry, &device_list);
	}
	mutex_unlock(&device_list_lock);
	if (status == 0){

		gf516m_dev->buffer = kzalloc(bufsiz + GF516M_RDATA_OFFSET, GFP_KERNEL);
		if(gf516m_dev->buffer == NULL) {
			kfree(gf516m_dev);
			status = -ENOMEM;
			goto err;
		}

		/*register device within input system.*/
		gf516m_dev->input = input_allocate_device();
		if(gf516m_dev->input == NULL) {
			gf516m_debug(ERR_LOG, "%s, Failed to allocate input device.\n", __func__);
			status = -ENOMEM;
			kfree(gf516m_dev->buffer);
			kfree(gf516m_dev);
			goto err;
		}

		__set_bit(EV_KEY, gf516m_dev->input->evbit);
		__set_bit(GF516M_INPUT_HOME_KEY, gf516m_dev->input->keybit);

                __set_bit(GF516M_INPUT_MENU_KEY, gf516m_dev->input->keybit);
                __set_bit(GF516M_INPUT_BACK_KEY, gf516m_dev->input->keybit);
//              __set_bit(GF516M_INPUT_UP_KEY, gf516m_dev->input->keybit);
//              __set_bit(GF516M_INPUT_DOWN_KEY, gf516m_dev->input->keybit);
                __set_bit(GF516M_FF_KEY, gf516m_dev->input->keybit);

                __set_bit(GF516M_NVG_DOWN_KEY, gf516m_dev->input->keybit);
                __set_bit(GF516M_NVG_LEFT_KEY, gf516m_dev->input->keybit);
                __set_bit(GF516M_NVG_UP_KEY, gf516m_dev->input->keybit);
                __set_bit(GF516M_NVG_RIGHT_KEY, gf516m_dev->input->keybit);
                __set_bit(GF516M_NVG_TOUCH_KEY, gf516m_dev->input->keybit);





		gf516m_dev->input->name = "fp-keys";
		if(input_register_device(gf516m_dev->input)) {
			gf516m_debug(ERR_LOG, "%s, Failed to register input device.\n", __func__);
			goto err;
		}
	}else{
		gf516m_debug(ERR_LOG, "%s, device create failed", __func__);
		goto err;
	}

        gf516m_dev->probe_finish = 1;
        //gf516m_dev->has_esd_watchdog = 1;
	
    //LOG_INF("<%s:%d>devinfo_add[%d]dev[%x]\n", __func__, __LINE__, devinfo_add, dev_ofilm);
	dev_goodix->device_used = DEVINFO_USED;
	DEVINFO_CHECK_ADD_DEVICE(dev_goodix);
	gf516m_debug(INFO_LOG, "%s probe finished, normal driver version: %s\n", __func__, GF516M_SPI_VERSION);

	FUNC_EXIT();
	return 0;

err:
	device_destroy(gf516m_spi_class, gf516m_dev->devt);

#ifdef GF516M_PROBE_CHIP
err2:
#endif
	dev_goodix->device_used = DEVINFO_UNUSED;
	DEVINFO_CHECK_ADD_DEVICE(dev_goodix);
	kfree(gf516m_dev);
	gf516m_hw_power(0);
	disable_clock(MT_CG_PERI_SPI0, "spi");
	FUNC_EXIT();
	return status;
}

static int gf516m_remove(struct spi_device *spi)
{
	struct gf516m_dev *gf516m_dev = spi_get_drvdata(spi);
	FUNC_ENTRY();

	/* make sure ops on existing fds can abort cleanly */
	if(gf516m_dev->spi->irq) {
		free_irq(gf516m_dev->spi->irq, gf516m_dev);
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	if (gf516m_dev->early_suspend.suspend) {
		unregister_early_suspend(&gf516m_dev->early_suspend);
	}
#endif

	spin_lock_irq(&gf516m_dev->spi_lock);
	gf516m_dev->spi = NULL;
	spi_set_drvdata(spi, NULL);
	spin_unlock_irq(&gf516m_dev->spi_lock);

	/* prevent new opens */
	mutex_lock(&device_list_lock);
	sysfs_remove_group(&spi->dev.kobj, &gf516m_debug_attr_group);
	list_del(&gf516m_dev->device_entry);
	device_destroy(gf516m_spi_class, gf516m_dev->devt);
	clear_bit(MINOR(gf516m_dev->devt), minors);
	if (gf516m_dev->users == 0) {
		if(gf516m_dev->input != NULL){
			input_unregister_device(gf516m_dev->input);
		}
	if(gf516m_dev->buffer != NULL)
		kfree(gf516m_dev->buffer);
		kfree(gf516m_dev);
	}
	mutex_unlock(&device_list_lock);

	gf516m_debug(INFO_LOG, "%s remove finished\n", __func__);

	FUNC_EXIT();
	return 0;
}

static int gf516m_suspend(struct spi_device *spi, pm_message_t msg)
{
	gf516m_debug(INFO_LOG, "%s: enter\n", __func__);
	//TODO:
	return 0;
}

static int gf516m_resume(struct spi_device *spi)
{
#if defined(GF516M_SLEEP_FEATURE)
	struct gf516m_dev	 *gf516m_dev;
	gf516m_dev = spi_get_drvdata(spi);

	if(gf516m_dev->device_available == 0){
		/* wake up sensor... */
		gf516m_hw_reset(50);
		gf516m_dev->device_available = 1;
	}
#endif

	gf516m_debug(INFO_LOG, "%s: enter\n", __func__);
	return 0;
}

static struct spi_driver gf516m_spi_driver = {
	.driver = {
		.name = SPI_DEV_NAME,
		.owner = THIS_MODULE,
	},
	.probe = gf516m_probe,
	.remove = gf516m_remove,
	.suspend = gf516m_suspend,
	.resume = gf516m_resume,
};

/*-------------------------------------------------------------------------*/
static int __init gf516m_init(void)
{
	int status;
	FUNC_ENTRY();

	/* Claim our 256 reserved device numbers.  Then register a class
	 * that will key udev/mdev to add/remove /dev nodes.  Last, register
	 * the driver which manages those device numbers.
	*/
	BUILD_BUG_ON(N_SPI_MINORS > 256);
	status = register_chrdev(SPIDEV_MAJOR, CHRD_DRIVER_NAME, &gf516m_fops);
	if (status < 0){
		gf516m_debug(ERR_LOG, "%s, Failed to register char device!\n", __func__);
		FUNC_EXIT();
		return status;
	}
	gf516m_spi_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(gf516m_spi_class)) {
		unregister_chrdev(SPIDEV_MAJOR, gf516m_spi_driver.driver.name);
		gf516m_debug(ERR_LOG, "%s, Failed to create class.\n", __func__);
		FUNC_EXIT();
		return PTR_ERR(gf516m_spi_class);
	}
	status = spi_register_driver(&gf516m_spi_driver);
	if (status < 0) {
		class_destroy(gf516m_spi_class);
		unregister_chrdev(SPIDEV_MAJOR, gf516m_spi_driver.driver.name);
		gf516m_debug(ERR_LOG, "%s, Failed to register SPI driver.\n", __func__);
	}
	FUNC_EXIT();
	return status;
}
module_init(gf516m_init);

static void __exit gf516m_exit(void)
{
	FUNC_ENTRY();
	spi_unregister_driver(&gf516m_spi_driver);
	class_destroy(gf516m_spi_class);
	unregister_chrdev(SPIDEV_MAJOR, gf516m_spi_driver.driver.name);
	FUNC_EXIT();
}
module_exit(gf516m_exit);


MODULE_AUTHOR("Ke Yang<yangke@goodix.com>");
MODULE_DESCRIPTION("Goodix Fingerprint chip GF516M TEE driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("spi:gf516m-spi");
