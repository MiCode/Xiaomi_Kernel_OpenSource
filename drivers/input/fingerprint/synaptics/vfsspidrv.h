/*! @file vfsSpiDrv.h
 *******************************************************************************
 **  SPI Driver Interface Functions
 **
 *******************************************************************************
 **
 **                           NDA AND NEED-TO-KNOW REQUIRED
 **
 *****************************************************************************
 **
 **  Copyright (C) 2011-2017 Synaptics Incorporated. All rights reserved.
 **
 **
 ** This file contains information that is proprietary to Synaptics
 ** Incorporated ("Synaptics"). The holder of this file shall treat all
 ** information contained herein as confidential, shall use the
 ** information only for its intended purpose, and shall not duplicate,
 ** disclose, or disseminate any of this information in any manner unless
 ** Synaptics has otherwise provided express, written permission.
 ** Use of the materials may require a license of intellectual property
 ** from a third party or from Synaptics. Receipt or possession of this
 ** file conveys no express or implied licenses to any intellectual
 ** property rights belonging to Synaptics.
 **
 **
 ** INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS," AND
 ** SYNAPTICS EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES,
 ** INCLUDING ANY IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 ** PARTICULAR PURPOSE, AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY
 ** INTELLECTUAL PROPERTY RIGHTS. IN NO EVENT SHALL SYNAPTICS BE LIABLE
 ** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, PUNITIVE, OR
 ** CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION WITH THE USE OF
 ** THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED AND BASED
 ** ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 ** NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF SYNAPTICS WAS ADVISED
 ** OF THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF COMPETENT
 ** JURISDICTION DOES NOT PERMIT THE DISCLAIMER OF DIRECT DAMAGES OR ANY
 ** OTHER DAMAGES, SYNAPTICS' TOTAL CUMULATIVE LIABILITY TO ANY PARTY
 ** SHALL NOT EXCEED ONE HUNDRED U.S. DOLLARS.
*/


#ifndef VFSSPIDRV_H_
#define VFSSPIDRV_H_

#define PR_ERR(fmt, args...)   pr_err("synafpspi:"fmt, ## args)
#define PR_DEBUG(fmt, args...) pr_debug("synafpspi:"fmt, ## args)
#define PR_INFO(fmt, args...)  pr_info("synafpspi:"fmt, ## args)

#define MAX_BAUD_RATE       19200000 /* llei_2015-11-20, set to 15000000, was 12000000 */
#define BAUD_RATE_COEF      1000

#define SYNA_PART_NAME "syna_fingerprint"
#define SYNA_DEV_NAME  "vfsspi"

#define DRDY_ACTIVE_STATUS		1
#define BITS_PER_WORD			8

/* Max baud rate supported by Validity sensor. */
#define SPI_BAUD_RATE			12000000

/* Maximum transfer size */
#define DEFAULT_BUFFER_SIZE		(72 * 1024)

/* Indicates DRDY IRQ enabled or disabled */
#define DRDY_IRQ_ENABLE			1
#define DRDY_IRQ_DISABLE		0

/* Indicates HBM IRQ enabled or disabled */
#define HBM_REQ_IRQ_ENABLE		1
#define HBM_REQ_IRQ_DISABLE		0

/* Magic number of IOCTL command */
#define VFSSPI_IOCTL_MAGIC    'k'

/*
 * Definitions of structures which are used by IOCTL commands
 */

/**
 * vfsspi_iocTransfer - structure to pass to VFSSPI_IOCTL_RW_SPI_MESSAGE command
 * @rxBuffer:pointer to retrieved data
 * @txBuffer:pointer to transmitted data
 * @len:transmitted/retrieved data size
 */
typedef struct vfsspi_iocTransfer {
	unsigned char *rxBuffer;
	unsigned char *txBuffer;
	unsigned int len;
} vfsspi_iocTransfer_t;

/* Pass to VFSSPI_IOCTL_REGISTER_DRDY_SIGNAL command */
/**
 * vfsspi_iocRegSignal - structure to pass to VFSSPI_IOCTL_REGISTER_DRDY_SIGNAL
 *          command
 * @userPID:Process ID to which SPI driver sends signal indicating that DRDY
 *          is asserted
 * @signalID:signalID
*/
typedef struct vfsspi_iocRegSignal {
	int userPID;
	int signalID;
} vfsspi_iocRegSignal_t;

/* VFSSPI_IOCTL_SELECT_DRDY_NTF_TYPE command:
 * definitions of DRDY notification type */
/* Notify DRDY through the signaling mechanism */
#define VFSSPI_DRDY_NOTIFY_TYPE_SIGNAL      0x00000001
/* Notify DRDY through the eventfd mechanism */
#define VFSSPI_DRDY_NOTIFY_TYPE_EVENTFD     0x00000002

typedef struct vfsspi_iocSelectDrdyNtfType {
	unsigned int supportedTypes;
	unsigned int selectedType;
} vfsspi_iocSelectDrdyNtfType_t;

/* Pass to VFSSPI_IOCTL_SET_HBM_REQ_INT command */
/* Trigger HBM request on falling edge */
#define VFSSPI_HBMREQ_TYPE_TRIGGER_LOW      0x00000000
/* Trigger HBM request on raising edge */
#define VFSSPI_HBMREQ_TYPE_TRIGGER_HIGH     0x00000001

/**
 * vfsspi_ioctlSetHbmReqInt_t - structure to pass to VFSSPI_IOCTL_SET_HBM_REQ_INT
 *          command
 * @enable:Indicates to enable/disable HBM request interrupt listening.
 * @type:Indicates interrupt type. One of VFSSPI_HBMREQ_TYPE_* definition
*/
typedef struct vfsspi_ioctlSetHbmReqInt {
	unsigned int enable;
	unsigned int type;
} vfsspi_ioctlSetHbmReqInt_t;

/**
 * IOCTL commands definitions
 */

/* Transmit data to the device and retrieve data from it simultaneously */
#define VFSSPI_IOCTL_RW_SPI_MESSAGE          _IOWR(VFSSPI_IOCTL_MAGIC,  \
	                         1, unsigned int)

/* Hard reset the device */
#define VFSSPI_IOCTL_DEVICE_RESET            _IO(VFSSPI_IOCTL_MAGIC,   2)

/* Set the baud rate of SPI master clock */
#define VFSSPI_IOCTL_SET_CLK                 _IOW(VFSSPI_IOCTL_MAGIC,   \
                             3, unsigned int)

/* Register DRDY signal. It is used by SPI driver for indicating host that
 * DRDY signal is asserted. */
#define VFSSPI_IOCTL_REGISTER_DRDY_SIGNAL    _IOW(VFSSPI_IOCTL_MAGIC,   \
                             5, unsigned int)

/* Enable/disable DRDY interrupt handling in the SPI driver */
#define VFSSPI_IOCTL_SET_DRDY_INT            _IOW(VFSSPI_IOCTL_MAGIC,   \
                             8, unsigned int)

/* Put the sensor device into deep sleep mode (min power consumption) */
#define VFSSPI_IOCTL_DEVICE_SUSPEND          _IO(VFSSPI_IOCTL_MAGIC,   9)

/* Turn on the power to the sensor */
#define VFSSPI_IOCTL_POWER_ON                _IO(VFSSPI_IOCTL_MAGIC,   13)

/* Turn off the power to the sensor */
#define VFSSPI_IOCTL_POWER_OFF               _IO(VFSSPI_IOCTL_MAGIC,   14)

/* Disable SPI core clock */
#define VFSSPI_IOCTL_DISABLE_SPI_CLOCK       _IO(VFSSPI_IOCTL_MAGIC,   15)

/* Initialize and enable the SPI core (configure with last set (or
 * defaults, if not set, gpios, clks, etc.) */
#define VFSSPI_IOCTL_SET_SPI_CONFIGURATION   _IO(VFSSPI_IOCTL_MAGIC,   16)

/* Uninitialize and disable the SPI core */
#define VFSSPI_IOCTL_RESET_SPI_CONFIGURATION _IO(VFSSPI_IOCTL_MAGIC,   17)

/* Retrieve sensor mount orientation:
 * 0 - right side up (swipe primary first);
 * 1 - upside down (swipe secondary first) */
#define VFSSPI_IOCTL_GET_SENSOR_ORIENTATION  _IOR(VFSSPI_IOCTL_MAGIC,   \
                             18, unsigned int)

/* Speed up the frequency of the CPU:
 * 0 - fall back the CPU to normal frequency
 * 1 - speed up the CPU */
#define VFSSPI_IOCTL_CPU_SPEEDUP             _IOW(VFSSPI_IOCTL_MAGIC,   \
                             19, unsigned int)

/* Select DRDY notification type.
 * Host sends all supported types (bit-mask of VFSSPI_DRDY_NOTIFY_TYPE_*
 * definitions) and kernel driver returns type which is selected. If driver
 * doesn't support this IOCTL, the VFSSPI_DRDY_NOTIFY_TYPE_SIGNAL type
 * is set by default. */
#define VFSSPI_IOCTL_SELECT_DRDY_NTF_TYPE   _IOWR(VFSSPI_IOCTL_MAGIC,   \
                             21, unsigned int)

#define VFSSPI_IOCTL_REGISTER_SCREEN_DETECTION_SIGNAL   _IOW(VFSSPI_IOCTL_MAGIC,   \
                             22, unsigned int)
/* Set buffer size.
 * Allow setting buffer size other than DEFAULT_BUFFER_SIZE which is 24k.
 * Input is # of bytes.
 */
#define VFSSPI_IOCTL_SET_BUFFER_SIZE        _IOW(VFSSPI_IOCTL_MAGIC,    \
                             23, unsigned int)

/* Drives HBM GPIO. It indicates whether HBM mode is turned on/off. */
#define VFSSPI_IOCTL_SET_HBM_READY           _IOW(VFSSPI_IOCTL_MAGIC,    \
                             24, unsigned int)

/* Register HBM REQ signal. It is used by SPI driver for indicating host that
* HBM mode should be enabled. */
#define VFSSPI_IOCTL_REGISTER_HBM_REQ_SIGNAL    _IOW(VFSSPI_IOCTL_MAGIC,   \
                             25, unsigned int)

/* Enable/disable HBM REQ interrupt handling in the SPI driver */
#define VFSSPI_IOCTL_SET_HBM_REQ_INT            _IOW(VFSSPI_IOCTL_MAGIC,   \
                             26, unsigned int)
#endif /* VFSSPIDRV_H__ */
