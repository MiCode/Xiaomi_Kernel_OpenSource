#ifndef _UAPI_AVTIMER_H
#define _UAPI_AVTIMER_H

#include <linux/ioctl.h>

#define MAJOR_NUM 100

#define IOCTL_GET_AVTIMER_TICK _IOR(MAJOR_NUM, 0, uint64_t)

#endif
