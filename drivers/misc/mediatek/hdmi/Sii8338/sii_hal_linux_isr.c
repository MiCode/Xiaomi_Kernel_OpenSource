#define SII_HAL_LINUX_ISR_C
#include "sii_hal.h"
#include "sii_hal_priv.h"
#include "si_drvisrconfig.h"
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <mach/irqs.h>
#include "mach/eint.h"
#include "mach/irqs.h"
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#include <cust_eint.h>
#if !defined GPIO_MHL_EINT_PIN
/* #error GPIO_MHL_EINT_PIN not defined */
#endif
#if !defined CUST_EINT_MHL_NUM
/* /#error CUST_EINT_MHL_NUM not defined */
#endif

#if 0
static irqreturn_t HalThreadedIrqHandler(int irq, void *data)
{
	pMhlDeviceContext pMhlDevContext = (pMhlDeviceContext) data;
	if (HalAcquireIsrLock() == HAL_RET_SUCCESS) {
		if (pMhlDevContext->CheckDevice && !pMhlDevContext->CheckDevice(0)) {
			SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "mhl device errror\n");
			HalReleaseIsrLock();
			return IRQ_HANDLED;
		}
		if (pMhlDevContext->irqHandler) {
			(pMhlDevContext->irqHandler) ();
		}
		HalReleaseIsrLock();
	} else {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "------------- irq missing! -------------\n");
	}
	return IRQ_HANDLED;
}
#endif

static struct task_struct *mhl_irq_task;

static wait_queue_head_t mhl_irq_wq;

#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
extern int smartbook_kthread(void *data);
extern wait_queue_head_t smartbook_wq;
static struct task_struct *smartbook_task;	/* add by kirby */
#endif

static atomic_t mhl_irq_event = ATOMIC_INIT(0);



static void mhl8338_irq_handler(void)
{
	atomic_set(&mhl_irq_event, 1);
	wake_up_interruptible(&mhl_irq_wq);
	/* mt65xx_eint_unmask(CUST_EINT_HDMI_HPD_NUM); */
}


static int mhl_irq_kthread(void *data)
{
	struct sched_param param = {.sched_priority = RTPM_PRIO_SCRN_UPDATE };
	sched_setscheduler(current, SCHED_RR, &param);

	for (;;) {
		set_current_state(TASK_INTERRUPTIBLE);
		wait_event_interruptible(mhl_irq_wq, atomic_read(&mhl_irq_event));
		set_current_state(TASK_RUNNING);
		printk("mhl_irq_kthread, mhl irq received\n");
		/* hdmi_update_impl(); */

		atomic_set(&mhl_irq_event, 0);
		SiiMhlTxDeviceIsr();
		if (kthread_should_stop())
			break;
#ifdef CUST_EINT_MHL_NUM
		mt_eint_unmask(CUST_EINT_MHL_NUM);
#endif
	}

	return 0;
}

halReturn_t HalInstallIrqHandler(fwIrqHandler_t irqHandler)
{
	/* int                           retStatus; */
	halReturn_t halRet;

	init_waitqueue_head(&mhl_irq_wq);

	mhl_irq_task = kthread_create(mhl_irq_kthread, NULL, "mhl_irq_kthread");
	wake_up_process(mhl_irq_task);

#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
	/* add by kirby */
	init_waitqueue_head(&smartbook_wq);
	smartbook_task = kthread_create(smartbook_kthread, NULL, "smartbook_kthread");
	wake_up_process(smartbook_task);
#endif

	if (irqHandler == NULL) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
				"HalInstallIrqHandler: irqHandler cannot be NULL!\n");
		return HAL_RET_PARAMETER_ERROR;
	}
	halRet = I2cAccessCheck();
	if (halRet != HAL_RET_SUCCESS) {
		return halRet;
	}
#if 0
	if (gMhlDevice.pI2cClient->irq == 0) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
				"HalInstallIrqHandler: No IRQ assigned to I2C device!\n");
		return HAL_RET_FAILURE;
	}
#endif


#if 0
	mt_set_gpio_mode(GPIO_MHL_EINT_PIN, GPIO_MODE_01);
	mt_set_gpio_dir(GPIO_MHL_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_select(GPIO_MHL_EINT_PIN, GPIO_PULL_UP);
	mt_set_gpio_pull_enable(GPIO_MHL_EINT_PIN, true);
#endif

#ifdef CUST_EINT_MHL_NUM
	/* /mt_eint_set_sens(CUST_EINT_MHL_NUM, MT_LEVEL_SENSITIVE); */
	/* /mt_eint_set_hw_debounce(CUST_EINT_MHL_NUM, CUST_EINT_MHL_DEBOUNCE_CN); */
	mt_eint_registration(CUST_EINT_MHL_NUM, CUST_EINT_MHL_TYPE, &mhl8338_irq_handler, 0);
	mt_eint_unmask(CUST_EINT_MHL_NUM);
#else
	printk("%s,%d Error: CUST_EINT_MHL_NUM is not defined\n", __func__, __LINE__);
#endif
#if 0
	gMhlDevice.irqHandler = irqHandler;
	retStatus = request_threaded_irq(gMhlDevice.pI2cClient->irq, NULL,
					 HalThreadedIrqHandler,
					 IRQF_TRIGGER_LOW | IRQF_ONESHOT,
					 gMhlI2cIdTable[0].name, &gMhlDevice);
	if (retStatus != 0) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
				"HalInstallIrqHandler: request_threaded_irq failed, status: %d\n",
				retStatus);
		gMhlDevice.irqHandler = NULL;
		return HAL_RET_FAILURE;
	}
#endif
	return HAL_RET_SUCCESS;
}

halReturn_t HalRemoveIrqHandler(void)
{
	halReturn_t halRet;
	halRet = I2cAccessCheck();
	if (halRet != HAL_RET_SUCCESS) {
		return halRet;
	}
	if (gMhlDevice.irqHandler == NULL) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
				"HalRemoveIrqHandler: no irqHandler installed!\n");
		return HAL_RET_FAILURE;
	}
	free_irq(gMhlDevice.pI2cClient->irq, &gMhlDevice);
	gMhlDevice.irqHandler = NULL;
	return HAL_RET_SUCCESS;
}

void HalEnableIrq(uint8_t bEnable)
{
	return;
	if (bEnable) {
		enable_irq(gMhlDevice.pI2cClient->irq);
	} else {
		disable_irq(gMhlDevice.pI2cClient->irq);
	}
}

#if 0
static irqreturn_t HalSilMonRequestIrqHandler(int irq, void *data)
{
	pMhlDeviceContext pMhlDevContext = (pMhlDeviceContext) data;
	int gpio_value;
	unsigned long flags;
	spin_lock_irqsave(&pMhlDevContext->SilMonRequestIRQ_Lock, flags);
	if (HalGpioGetPin(GPIO_REQ_IN, &gpio_value) < 0) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
				"HalSilMonRequestIrqHandler GPIO(%d) get error\n", gpio_value);
		spin_unlock_irqrestore(&pMhlDevContext->SilMonRequestIRQ_Lock, flags);
		return IRQ_HANDLED;
	}
	if ((gMhlDevice.SilMonControlReleased && gpio_value)
	    || (!gMhlDevice.SilMonControlReleased && !gpio_value)) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
				"HalSilMonRequestIrqHandler, wrong IRQ coming, please check you board\n");
		spin_unlock_irqrestore(&pMhlDevContext->SilMonRequestIRQ_Lock, flags);
		return IRQ_HANDLED;
	}
	if (gpio_value) {
		/* HalGpioSetPin(GPIO_GNT,1); */
		HalEnableI2C(true);
		enable_irq(pMhlDevContext->pI2cClient->irq);
#ifdef RGB_BOARD
		enable_irq(pMhlDevContext->SilExtDeviceIRQ);
#endif
		gMhlDevice.SilMonControlReleased = true;
	} else {
		disable_irq(pMhlDevContext->pI2cClient->irq);
#ifdef RGB_BOARD
		disable_irq(pMhlDevContext->SilExtDeviceIRQ);
#endif
		HalEnableI2C(false);
		/* HalGpioSetPin(GPIO_GNT,0); */
		gMhlDevice.SilMonControlReleased = false;
	}
	spin_unlock_irqrestore(&pMhlDevContext->SilMonRequestIRQ_Lock, flags);
	return IRQ_HANDLED;
}

halReturn_t HalInstallSilMonRequestIrqHandler(void)
{
	int retStatus;
	halReturn_t halRet;
	halRet = I2cAccessCheck();
	if (halRet != HAL_RET_SUCCESS) {
		return halRet;
	}
	halRet = HalGetGpioIrqNumber(GPIO_REQ_IN, &gMhlDevice.SilMonRequestIRQ);
	if (halRet != HAL_RET_SUCCESS) {
		return halRet;
	}
	spin_lock_init(&gMhlDevice.SilMonRequestIRQ_Lock);
	gMhlDevice.SilMonControlReleased = true;
	retStatus = request_threaded_irq(gMhlDevice.SilMonRequestIRQ, NULL,
					 HalSilMonRequestIrqHandler,
					 IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					 gMhlI2cIdTable[0].name, &gMhlDevice);
	if (retStatus != 0) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
				"HalInstallIrqHandler: request_threaded_irq failed, status: %d\n",
				retStatus);
		return HAL_RET_FAILURE;
	}
	return HAL_RET_SUCCESS;
}

halReturn_t HalRemoveSilMonRequestIrqHandler(void)
{
	halReturn_t halRet;
	halRet = I2cAccessCheck();
	if (halRet != HAL_RET_SUCCESS) {
		return halRet;
	}
	free_irq(gMhlDevice.SilMonRequestIRQ, &gMhlDevice);
	return HAL_RET_SUCCESS;
}

#ifdef RGB_BOARD
static irqreturn_t HalSilExtDeviceIrqHandler(int irq, void *data)
{
	pMhlDeviceContext pMhlDevContext = (pMhlDeviceContext) data;
	if (HalAcquireIsrLock() == HAL_RET_SUCCESS) {
		if (pMhlDevContext->CheckDevice && !pMhlDevContext->CheckDevice(1)) {
			HalReleaseIsrLock();
			return IRQ_HANDLED;
		}
		if (pMhlDevContext->ExtDeviceirqHandler) {
			(pMhlDevContext->ExtDeviceirqHandler) ();
		}
		HalReleaseIsrLock();
	} else {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
				"------------- ExtDevice irq missing! -------------\n");
		return IRQ_HANDLED;
	}
	return IRQ_HANDLED;
}

halReturn_t HalInstallSilExtDeviceIrqHandler(fwIrqHandler_t irqHandler)
{
	int retStatus;
	halReturn_t halRet;
	halRet = I2cAccessCheck();
	if (halRet != HAL_RET_SUCCESS) {
		return halRet;
	}
	halRet = HalGetGpioIrqNumber(GPIO_V_INT, &gMhlDevice.SilExtDeviceIRQ);
	if (halRet != HAL_RET_SUCCESS) {
		return halRet;
	}
	gMhlDevice.ExtDeviceirqHandler = irqHandler;
	retStatus = request_threaded_irq(gMhlDevice.SilExtDeviceIRQ, NULL,
					 HalSilExtDeviceIrqHandler,
					 IRQF_TRIGGER_LOW | IRQF_ONESHOT,
					 gMhlI2cIdTable[0].name, &gMhlDevice);
	if (retStatus != 0) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
				"HalInstallIrqHandler: request_threaded_irq failed, status: %d\n",
				retStatus);
		gMhlDevice.ExtDeviceirqHandler = NULL;
		return HAL_RET_FAILURE;
	}
	return HAL_RET_SUCCESS;
}

halReturn_t HalRemoveSilExtDeviceIrqHandler(void)
{
	halReturn_t halRet;
	halRet = I2cAccessCheck();
	if (halRet != HAL_RET_SUCCESS) {
		return halRet;
	}
	if (gMhlDevice.ExtDeviceirqHandler == NULL) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
				"HalRemoveSilExtDeviceIrqHandler: no irqHandler installed!\n");
		return HAL_RET_FAILURE;
	}
	free_irq(gMhlDevice.SilExtDeviceIRQ, &gMhlDevice);
	return HAL_RET_SUCCESS;
}
#endif
#endif
halReturn_t HalInstallCheckDeviceCB(fnCheckDevice fn)
{
	gMhlDevice.CheckDevice = fn;
	return HAL_RET_SUCCESS;
}
