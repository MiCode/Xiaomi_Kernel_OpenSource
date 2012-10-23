#ifndef _CI_BRIDGE_SPI_H_
#define _CI_BRIDGE_SPI_H_

#include <linux/ioctl.h>

#define CI_BRIDGE_IOCTL_MAGIC 'c'
#define CI_BRIDGE_IOCTL_RESET         _IOW(CI_BRIDGE_IOCTL_MAGIC, 0, unsigned)
#define CI_BRIDGE_IOCTL_GET_INT_STATE _IOR(CI_BRIDGE_IOCTL_MAGIC, 1, unsigned)

#ifdef __KERNEL__
struct ci_bridge_platform_data {
	unsigned int reset_pin;
	unsigned int interrupt_pin;
};
#endif /* __KERNEL__ */

#endif
