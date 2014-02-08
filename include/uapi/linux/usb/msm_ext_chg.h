#ifndef __UAPI_USB_MSM_EXT_CHG_H
#define __UAPI_USB_MSM_EXT_CHG_H

#include <linux/ioctl.h>

#define USB_CHG_BLOCK_ULPI	1
#define USB_CHG_BLOCK_QSCRATCH	2

#define USB_REQUEST_5V		1
#define USB_REQUEST_9V		2
/**
 * struct msm_usb_chg_info - MSM USB charger block details.
 * @chg_block_type: The type of charger block. QSCRATCH/ULPI.
 * @page_offset: USB charger register base may not be aligned to
 *              PAGE_SIZE.  The kernel driver aligns the base
 *              address and use it for memory mapping.  This
 *              page_offset is used by user space to calaculate
 *              the corret charger register base address.
 * @length: The length of the charger register address space.
 */
struct msm_usb_chg_info {
	uint32_t chg_block_type;
	off_t page_offset;
	size_t length;
};

/* Get the MSM USB charger block information */
#define MSM_USB_EXT_CHG_INFO _IOW('M', 0, struct msm_usb_chg_info)

/* Vote against USB hardware low power mode */
#define MSM_USB_EXT_CHG_BLOCK_LPM _IOW('M', 1, int)

/* To tell kernel about voltage being voted */
#define MSM_USB_EXT_CHG_VOLTAGE_INFO _IOW('M', 2, int)

/* To tell kernel about voltage request result */
#define MSM_USB_EXT_CHG_RESULT _IOW('M', 3, int)
#endif /* __UAPI_USB_MSM_EXT_CHG_H */
