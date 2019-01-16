#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include "mhl_linuxdrv.h"
#include "osal/include/osal.h"
#include "si_mhl_tx_api.h"
#include "si_cra.h"
#include "si_drvisrconfig.h"
#include "si_drv_mhl_tx.h"


/* interrupt mode or polling mode for 8338 driver (if you want to use polling mode, pls comment below sentense) */
#define SiI8338DRIVER_INTERRUPT_MODE   1

static int32_t devMajor;
/* static struct cdev siiMhlCdev; */
static struct class *siiMhlClass;
MHL_DRIVER_CONTEXT_T gDriverContext;
#if defined(DEBUG)
unsigned char DebugChannelMasks[SII_OSAL_DEBUG_NUM_CHANNELS / 8 + 1] = { 0xFF, 0xFF, 0xFF, 0xFF };

module_param_array(DebugChannelMasks, byte, NULL, S_IRUGO | S_IWUSR);
ushort DebugFormat = SII_OS_DEBUG_FORMAT_FILEINFO;
module_param(DebugFormat, ushort, S_IRUGO | S_IWUSR);
#endif
unsigned char DebugSW = (pinDbgMsgs_HIGH | pinDbgSw3_HIGH | pinDbgSw5_HIGH);
module_param(DebugSW, byte, S_IRUGO | S_IWUSR);
uint16_t deviceID;
/* static char DRV_VERSION[] = "RGB-0.80.4"; */
/* static char BUILT_TIME[64]; */

#ifndef SiI8338DRIVER_INTERRUPT_MODE

/*****************************************************************************/
/**
 *  @brief Thread function that periodically polls for MHLTx events.
 *
 *  @param[in]	data	Pointer to driver context structure
 *
 *  @return		Always returns zero when the thread exits.
 *
 *****************************************************************************/
static int EventThread(void *data)
{
	uint8_t event;
	uint8_t eventParam;


	printk("%s EventThread starting up\n", MHL_DRIVER_NAME);

	while (true) {
		if (kthread_should_stop()) {
			printk("%s EventThread exiting\n", MHL_DRIVER_NAME);
			break;
		}

		HalTimerWait(30);
		SiiMhlTxDeviceIsr();
	}
	return 0;
}


/***** public functions ******************************************************/


/*****************************************************************************/
/**
 * @brief Start drivers event monitoring thread.
 *
 *****************************************************************************/
struct task_struct *pTaskStruct;
void StartEventThread(void)
{
	pTaskStruct = kthread_run(EventThread, &gDriverContext, MHL_DRIVER_NAME);
}

/*****************************************************************************/
/**
 * @brief Stop driver's event monitoring thread.
 *
 *****************************************************************************/
void StopEventThread(void)
{
	kthread_stop(pTaskStruct);

}
#endif

#if 0
static void InitDebugSW(void)
{
#define INIT_DEBUGSW_PIN(a) do { a = a##_HIGH; a = (a)&DebugSW; } while (0)
	INIT_DEBUGSW_PIN(pinDbgMsgs);
	INIT_DEBUGSW_PIN(pinOverrideTiming);
	INIT_DEBUGSW_PIN(pinDbgSw3);
	INIT_DEBUGSW_PIN(pinDbgSw4);
	INIT_DEBUGSW_PIN(pinDbgSw5);
	INIT_DEBUGSW_PIN(pinDbgSw6);
	INIT_DEBUGSW_PIN(pinSw);
	INIT_DEBUGSW_PIN(pinPwSw1aEn);
#if defined(DEBUG)
	if (!pinDbgMsgs) {
		memset(DebugChannelMasks, 0, (SII_OSAL_DEBUG_NUM_CHANNELS / 8 + 1));
	}
#endif
}
#endif

#ifdef USE_PROC
static int mhldrv_proc_show(struct seq_file *s, void *unused)
{
	seq_printf(s, "%s\n", "===========<<sii8338 status>>===========");
	seq_printf(s, "ver:%s\n", DRV_VERSION);
	seq_printf(s, "built at %s\n", BUILT_TIME);
	mhl_seq_show(s);
	drv_mhl_seq_show(s);
	seq_printf(s, "%s\n", "===========<<sii8338 status>>===========");
	return 0;
}

static int mhldrv_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mhldrv_proc_show, NULL);
}

static const struct file_operations mhldrv_proc_operations = {
	.open = mhldrv_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void mhldrv_create_proc(void)
{
	struct proc_dir_entry *entry;
	entry = create_proc_entry("sii8338", 0, NULL);
	if (entry)
		entry->proc_fops = &mhldrv_proc_operations;
}

static void mhldrv_remove_proc(void)
{
	remove_proc_entry("sii8338", NULL);
}
#endif

extern void SiiMhlTxHwReset(uint16_t hwResetPeriod, uint16_t hwResetDelay);
int32_t StartMhlTxDevice(void)
{
	halReturn_t halStatus;
	SiiOsStatus_t osalStatus;
	printk("Starting %s\n", MHL_PART_NAME);

	SiiMhlTxHwReset(10, 200);
	if (!SiiCraInitialize()) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "Initialization of CRA layer failed!\n");
		return -EIO;
	}
	osalStatus = SiiOsInit(0);
	if (osalStatus != SII_OS_STATUS_SUCCESS) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
				"Initialization of OSAL failed, error code: %d\n", osalStatus);
		return -EIO;
	}
	halStatus = HalInit();
	if (halStatus != HAL_RET_SUCCESS) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
				"Initialization of HAL failed, error code: %d\n", halStatus);
		SiiOsTerm();
		return -EIO;
	}
/* #if MTK_project */
	/* I2c_Init(); */
/* #endif */

	halStatus = HalOpenI2cDevice(MHL_PART_NAME, MHL_DRIVER_NAME);
	if (halStatus != HAL_RET_SUCCESS) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
				"Opening of I2c device %s failed, error code: %d\n", MHL_PART_NAME,
				halStatus);
		HalTerm();
		SiiOsTerm();
		return -EIO;
	}


	msleep(200);

#ifdef SiI8338DRIVER_INTERRUPT_MODE
	halStatus = HalInstallIrqHandler(SiiMhlTxDeviceIsr);
	if (halStatus != HAL_RET_SUCCESS) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
				"Initialization of HAL interrupt support failed, error code: %d\n",
				halStatus);
		HalCloseI2cDevice();
		HalTerm();
		SiiOsTerm();
		return -EIO;
	}
#else
	StartEventThread();	/* begin monitoring for events if using polling mode */
#endif

	HalInstallCheckDeviceCB(SiiCheckDevice);
	HalAcquireIsrLock();
	siHdmiTx_VideoSel(HDMI_720P60);
	siHdmiTx_AudioSel(I2S_44);
	SiiMhlTxInitialize(EVENT_POLL_INTERVAL_MS);
	HalReleaseIsrLock();
	return 0;
}

int32_t ResumeMhlTxDevice(void)
{
	/* halReturn_t           halStatus; */
	/* SiiOsStatus_t osalStatus; */
	printk("Starting %s\n", MHL_PART_NAME);

	SiiMhlTxHwReset(10, 200);
	if (!SiiCraInitialize()) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "Initialization of CRA layer failed!\n");
		return -EIO;
	}
	/* msleep(200); */

	HalInstallCheckDeviceCB(SiiCheckDevice);
	HalAcquireIsrLock();
	siHdmiTx_VideoSel(HDMI_720P60);
	siHdmiTx_AudioSel(I2S_44);
	SiiMhlTxInitialize(EVENT_POLL_INTERVAL_MS);
	HalReleaseIsrLock();
	return 0;
}


int32_t StopMhlTxDevice(void)
{
	halReturn_t halStatus;
	printk("Stopping %s\n", MHL_PART_NAME);
	HalRemoveIrqHandler();
	/* HalRemoveSilMonRequestIrqHandler(); */
#ifdef RGB_BOARD
	/* HalRemoveSilExtDeviceIrqHandler(); */
#endif
	halStatus = HalCloseI2cDevice();
	if (halStatus != HAL_RET_SUCCESS) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
				"Closing of I2c device failed, error code: %d\n", halStatus);
		return -EIO;
	}
	halStatus = HalTerm();
	if (halStatus != HAL_RET_SUCCESS) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
				"Termination of HAL failed, error code: %d\n", halStatus);
		return -EIO;
	}
	SiiOsTerm();
	return 0;
}

ssize_t ShowConnectionState(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (gDriverContext.flags & MHL_STATE_FLAG_CONNECTED) {
		return scnprintf(buf, PAGE_SIZE, "connected %s_ready",
				 gDriverContext.
				 flags & MHL_STATE_FLAG_RCP_READY ? "rcp" : "not_rcp");
	} else {
		return scnprintf(buf, PAGE_SIZE, "not connected");
	}
}

ssize_t ShowRcp(struct device *dev, struct device_attribute *attr, char *buf)
{
	int status = 0;
	if (HalAcquireIsrLock() != HAL_RET_SUCCESS) {
		return -ERESTARTSYS;
	}
	if (gDriverContext.flags & (MHL_STATE_FLAG_RCP_SENT | MHL_STATE_FLAG_RCP_RECEIVED)) {
		status = scnprintf(buf, PAGE_SIZE, "0x%02x %s",
				   gDriverContext.keyCode,
				   gDriverContext.
				   flags & MHL_STATE_FLAG_RCP_SENT ? "sent" : "received");
	}
	HalReleaseIsrLock();
	return status;
}

ssize_t SendRcp(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long keyCode;
	int status = -EINVAL;
	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "SendRcp received string: " "%s" "\n", buf);
	if (HalAcquireIsrLock() != HAL_RET_SUCCESS) {
		return -ERESTARTSYS;
	}
	while (gDriverContext.flags & MHL_STATE_FLAG_RCP_READY) {
		if (strict_strtoul(buf, 0, &keyCode)) {
			SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "Unable to convert keycode string\n");
			break;
		}
		if (keyCode >= 0xFE) {
			SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
					"keycode (0x%x) is too large to be valid\n", (int)keyCode);
			break;
		}
		gDriverContext.flags &= ~(MHL_STATE_FLAG_RCP_RECEIVED |
					  MHL_STATE_FLAG_RCP_ACK | MHL_STATE_FLAG_RCP_NAK);
		gDriverContext.flags |= MHL_STATE_FLAG_RCP_SENT;
		gDriverContext.keyCode = (uint8_t) keyCode;
		SiiMhlTxRcpSend((uint8_t) keyCode);
		status = count;
		break;
	}
	HalReleaseIsrLock();
	return status;
}

ssize_t ShowRcpAck(struct device *dev, struct device_attribute *attr, char *buf)
{
	int status = -EINVAL;
	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "ShowRcpAck called\n");
	if (HalAcquireIsrLock() != HAL_RET_SUCCESS) {
		return -ERESTARTSYS;
	}
	if (gDriverContext.flags & (MHL_STATE_FLAG_RCP_ACK | MHL_STATE_FLAG_RCP_NAK)) {
		status = scnprintf(buf, PAGE_SIZE, "keycode=0x%02x errorcode=0x%02x",
				   gDriverContext.keyCode, gDriverContext.errCode);
	}
	HalReleaseIsrLock();
	return status;
}

ssize_t SendRcpAck(struct device *dev, struct device_attribute *attr,
		   const char *buf, size_t count)
{
	unsigned long keyCode = 0x100;
	unsigned long errCode = 0x100;
	char *pStr;
	int status = -EINVAL;
	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "SendRcpAck received buf len = %d\n", count);
	while (count) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "SendRcpAck received string: " "%s" "\n",
				buf);
		pStr = strstr(buf, "keycode=");
		if (pStr != NULL) {
			if (strict_strtoul(pStr + strlen("keycode="), 0, &keyCode)) {
				SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
						"Unable to convert keycode string\n");
				break;
			}
		}
		pStr = strstr(buf, "errorcode=");
		if (pStr != NULL) {
			if (strict_strtoul(pStr + strlen("errorcode="), 0, &errCode)) {
				SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
						"Unable to convert keycode string\n");
				break;
			}
		}
		while (--count && *buf++);
	}
	if ((keyCode > 0xFF) || (errCode > 0xFF)) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "Invalid key code or error code "
				"specified, key code: 0x%02x  error code: 0x%02x\n",
				(int)keyCode, (int)errCode);
		return status;
	}
	if (HalAcquireIsrLock() != HAL_RET_SUCCESS) {
		return -ERESTARTSYS;
	}
	while (gDriverContext.flags & MHL_STATE_FLAG_RCP_READY) {
		if ((keyCode != gDriverContext.keyCode)
		    || !(gDriverContext.flags & MHL_STATE_FLAG_RCP_RECEIVED)) {
			SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
					"Attempting to ACK a key code that was not received!\n");
			break;
		}
		if (errCode == 0) {
			SiiMhlTxRcpkSend((uint8_t) keyCode);
		} else {
			SiiMhlTxRcpeSend((uint8_t) errCode);
		}
		status = count;
		break;
	}
	HalReleaseIsrLock();
	return status;
}

ssize_t SelectDevCap(struct device *dev, struct device_attribute *attr,
		     const char *buf, size_t count)
{
	unsigned long devCapOffset;
	int status = -EINVAL;
	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "SelectDevCap received string: " "%s" "\n", buf);
	do {
		if (strict_strtoul(buf, 0, &devCapOffset)) {
			SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
					"Unable to convert register offset string\n");
			break;
		}
		if (devCapOffset >= 0x0F) {
			SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
					"dev cap offset (0x%x) is too large to be valid\n",
					devCapOffset);
			break;
		}
		gDriverContext.devCapOffset = (uint8_t) devCapOffset;
		status = count;
	} while (false);
	return status;
}

ssize_t ReadDevCap(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t regValue;
	int status = -EINVAL;
	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "ReadDevCap called\n");
	if (HalAcquireIsrLock() != HAL_RET_SUCCESS) {
		return -ERESTARTSYS;
	}
	do {
		if (gDriverContext.flags & MHL_STATE_FLAG_CONNECTED) {
			status = SiiTxGetPeerDevCapEntry(gDriverContext.devCapOffset, &regValue);
			if (status != 0) {
				status = -EAGAIN;
				break;
			}
			status = scnprintf(buf, PAGE_SIZE, "offset:0x%02x=0x%02x",
					   gDriverContext.devCapOffset, regValue);
		}
	} while (false);
	HalReleaseIsrLock();
	return status;
}

#if 0
#define MAX_EVENT_STRING_LEN 40
void AppNotifyMhlDownStreamHPDStatusChange(bool_t connected)
{
	char event_string[MAX_EVENT_STRING_LEN];
	char *envp[] = { event_string, NULL };
	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
			"AppNotifyMhlDownStreamHPDStatusChange called, "
			"HPD status is: %s\n", connected ? "CONNECTED" : "NOT CONNECTED");
	snprintf(event_string, MAX_EVENT_STRING_LEN, "MHLEVENT=%s", connected ? "HPD" : "NO_HPD");
	printk("MHLEVENT=%s\n", connected ? "HPD" : "NO_HPD");
	return;
#if 0
	kobject_uevent_env(&gDriverContext.pDevice->kobj, KOBJ_CHANGE, envp);
#endif
}

MhlTxNotifyEventsStatus_e AppNotifyMhlEvent(uint8_t eventCode, uint8_t eventParam)
{
	char event_string[MAX_EVENT_STRING_LEN];
	char *envp[] = { event_string, NULL };
	MhlTxNotifyEventsStatus_e retVal = MHL_TX_EVENT_STATUS_PASSTHROUGH;
	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
			"AppNotifyEvent called, eventCode: 0x%02x eventParam: 0x%02x\n",
			eventCode, eventParam);

	switch (eventCode) {
	case MHL_TX_EVENT_CONNECTION:
		gDriverContext.flags |= MHL_STATE_FLAG_CONNECTED;
		strncpy(event_string, "MHLEVENT=connected", MAX_EVENT_STRING_LEN);
		/* kobject_uevent_env(&gDriverContext.pDevice->kobj, KOBJ_CHANGE, envp); */
		break;
	case MHL_TX_EVENT_RCP_READY:
		gDriverContext.flags |= MHL_STATE_FLAG_RCP_READY;
		strncpy(event_string, "MHLEVENT=rcp_ready", MAX_EVENT_STRING_LEN);
		/* kobject_uevent_env(&gDriverContext.pDevice->kobj, KOBJ_CHANGE, envp); */
		break;
	case MHL_TX_EVENT_DISCONNECTION:
		gDriverContext.flags = 0;
		gDriverContext.keyCode = 0;
		gDriverContext.errCode = 0;
		strncpy(event_string, "MHLEVENT=disconnected", MAX_EVENT_STRING_LEN);
		/* kobject_uevent_env(&gDriverContext.pDevice->kobj, KOBJ_CHANGE, envp); */
		break;
	case MHL_TX_EVENT_RCP_RECEIVED:
		gDriverContext.flags &= ~MHL_STATE_FLAG_RCP_SENT;
		gDriverContext.flags |= MHL_STATE_FLAG_RCP_RECEIVED;
		gDriverContext.keyCode = eventParam;

		snprintf(event_string, MAX_EVENT_STRING_LEN,
			 "MHLEVENT=received_RCP key code=0x%02x", eventParam);

		/* for RCP report function by garyyuan */
		if (eventParam > 0x7F)
			break;
		input_report_mhl_rcp_key(gDriverContext.keyCode);

		/* kobject_uevent_env(&gDriverContext.pDevice->kobj, KOBJ_CHANGE, envp); */
		break;
	case MHL_TX_EVENT_RCPK_RECEIVED:
		if ((gDriverContext.flags & MHL_STATE_FLAG_RCP_SENT)
		    && (gDriverContext.keyCode == eventParam)) {
			gDriverContext.flags |= MHL_STATE_FLAG_RCP_ACK;
			SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
					"Generating RCPK received event, keycode: 0x%02x\n",
					eventParam);
			snprintf(event_string, MAX_EVENT_STRING_LEN,
				 "MHLEVENT=received_RCPK key code=0x%02x", eventParam);
			/* kobject_uevent_env(&gDriverContext.pDevice->kobj, KOBJ_CHANGE, envp); */
		} else {
			SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
					"Ignoring unexpected RCPK received event, keycode: 0x%02x\n",
					eventParam);
		}
		break;
	case MHL_TX_EVENT_RCPE_RECEIVED:
		if (gDriverContext.flags & MHL_STATE_FLAG_RCP_SENT) {
			gDriverContext.errCode = eventParam;
			gDriverContext.flags |= MHL_STATE_FLAG_RCP_NAK;
			SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
					"Generating RCPE received event, error code: 0x%02x\n",
					eventParam);
			snprintf(event_string, MAX_EVENT_STRING_LEN,
				 "MHLEVENT=received_RCPE error code=0x%02x", eventParam);
			/* kobject_uevent_env(&gDriverContext.pDevice->kobj, KOBJ_CHANGE, envp); */
		} else {
			SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
					"Ignoring unexpected RCPE received event, error code: 0x%02x\n",
					eventParam);
		}
		break;
	case MHL_TX_EVENT_DCAP_CHG:
		snprintf(event_string, MAX_EVENT_STRING_LEN, "MHLEVENT=DEVCAP change");
		/* kobject_uevent_env(&gDriverContext.pDevice->kobj, KOBJ_CHANGE, envp); */
		break;
	case MHL_TX_EVENT_DSCR_CHG:
		snprintf(event_string, MAX_EVENT_STRING_LEN, "MHLEVENT=SCRATCHPAD change");
		/* kobject_uevent_env(&gDriverContext.pDevice->kobj, KOBJ_CHANGE, envp); */
		break;
	case MHL_TX_EVENT_POW_BIT_CHG:
		if (eventParam) {
			snprintf(event_string, MAX_EVENT_STRING_LEN, "MHLEVENT=MHL VBUS power OFF");
		} else {
			snprintf(event_string, MAX_EVENT_STRING_LEN, "MHLEVENT=MHL VBUS power ON");
		}
		/* kobject_uevent_env(&gDriverContext.pDevice->kobj, KOBJ_CHANGE, envp); */
		break;
	case MHL_TX_EVENT_RGND_MHL:
		snprintf(event_string, MAX_EVENT_STRING_LEN, "MHLEVENT=MHL device detected");
		/* kobject_uevent_env(&gDriverContext.pDevice->kobj, KOBJ_CHANGE, envp); */
		break;
	default:
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
				"AppNotifyEvent called with unrecognized event code!\n");
	}

	return retVal;
}
#endif

static const struct file_operations siiMhlFops = {
	.owner = THIS_MODULE,
	.open = SiiMhlOpen,
	.release = SiiMhlRelease,
	.unlocked_ioctl = SiiMhlIoctl
};

struct device_attribute driver_attribs[] = {
	__ATTR(connection_state, 0444, ShowConnectionState, NULL),
	__ATTR(rcp_keycode, 0666, ShowRcp, SendRcp),
	__ATTR(rcp_ack, 0666, ShowRcpAck, SendRcpAck),
	__ATTR(devcap, 0666, ReadDevCap, SelectDevCap),
	__ATTR_NULL
};

#if 0
static int __init SiiMhlInit(void)
{
	int32_t ret;
	dev_t devno;

	printk("\n============================================\n");
	printk("%s driver starting!\n", MHL_DRIVER_NAME);
	sprintf(BUILT_TIME, "Build: %s", __TIME__ "-" __DATE__);
	printk("Version: %s \n%s\n", DRV_VERSION, BUILT_TIME);
	printk("register_chrdev %s\n", MHL_DRIVER_NAME);
	printk("============================================\n");
	/* InitDebugSW(); */
	if (devMajor) {
		devno = MKDEV(devMajor, 0);
		ret = register_chrdev_region(devno, MHL_DRIVER_MINOR_MAX, MHL_DRIVER_NAME);
	} else {
		ret = alloc_chrdev_region(&devno, 0, MHL_DRIVER_MINOR_MAX, MHL_DRIVER_NAME);
		devMajor = MAJOR(devno);
	}
	if (ret) {
		printk("register_chrdev %d, %s failed, error code: %d\n",
		       devMajor, MHL_DRIVER_NAME, ret);
		return ret;
	}
	cdev_init(&siiMhlCdev, &siiMhlFops);
	siiMhlCdev.owner = THIS_MODULE;
	ret = cdev_add(&siiMhlCdev, devno, MHL_DRIVER_MINOR_MAX);
	if (ret) {
		printk("cdev_add %s failed %d\n", MHL_DRIVER_NAME, ret);
		goto free_chrdev;
	}
	siiMhlClass = class_create(THIS_MODULE, "mhl");
	if (IS_ERR(siiMhlClass)) {
		printk("class_create failed %d\n", ret);
		ret = PTR_ERR(siiMhlClass);
		goto free_cdev;
	}
	siiMhlClass->dev_attrs = driver_attribs;
	gDriverContext.pDevice = device_create(siiMhlClass, NULL,
					       MKDEV(devMajor, 0), NULL, "%s", MHL_DEVICE_NAME);
	if (IS_ERR(gDriverContext.pDevice)) {
		printk("class_device_create failed %s %d\n", MHL_DEVICE_NAME, ret);
		ret = PTR_ERR(gDriverContext.pDevice);
		goto free_class;
	}
	ret = StartMhlTxDevice();
	if (ret == 0) {
#ifdef USE_PROC
		mhldrv_create_proc();
#endif
		printk(KERN_NOTICE " mhldrv initialized successfully\n");
		return 0;
	} else {
		device_destroy(siiMhlClass, MKDEV(devMajor, 0));
	}
 free_class:
	class_destroy(siiMhlClass);
 free_cdev:
	cdev_del(&siiMhlCdev);
 free_chrdev:
	unregister_chrdev_region(MKDEV(devMajor, 0), MHL_DRIVER_MINOR_MAX);
	return ret;
}
#endif
static void __exit SiiMhlExit(void)
{
	printk("%s driver exiting!\n", MHL_DRIVER_NAME);
#ifdef USE_PROC
	mhldrv_remove_proc();
#endif
#ifdef MDT_SUPPORT
	mdt_deregister();	/* MDT initialization; support dynamic input_dev loading. */
#endif

	StopMhlTxDevice();
	device_destroy(siiMhlClass, MKDEV(devMajor, 0));
	class_destroy(siiMhlClass);
	unregister_chrdev_region(MKDEV(devMajor, 0), MHL_DRIVER_MINOR_MAX);
}

/* late_initcall(SiiMhlInit); */
/* module_exit(SiiMhlExit); */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Silicon Image <http://www.siliconimage.com>");
MODULE_DESCRIPTION(MHL_DRIVER_DESC);
