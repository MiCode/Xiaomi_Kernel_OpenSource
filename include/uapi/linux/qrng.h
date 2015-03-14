#ifndef _UAPI_QRNG_H_
#define _UAPI_QRNG_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#define QRNG_IOC_MAGIC    0x100

#define QRNG_IOCTL_RESET_BUS_BANDWIDTH\
	_IO(QRNG_IOC_MAGIC, 1)

#define QRNG_IOCTL_UPDATE_FIPS_STATUS\
	_IO(QRNG_IOC_MAGIC, 2)

#endif /* _UAPI_QRNG_H_ */
