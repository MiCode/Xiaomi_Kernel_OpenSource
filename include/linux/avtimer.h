#ifndef AVTIMER_H
#define AVTIMER_H

#include <linux/ioctl.h>

#define MAJOR_NUM 100

#define IOCTL_GET_AVTIMER_TICK _IOR(MAJOR_NUM, 0, char *)
/*
 * This IOCTL is used read the avtimer tick value.
 * Avtimer is a 64 bit timer tick, hence the expected
 * argument is of type uint64_t
 */
struct dev_avtimer_data {
	uint32_t avtimer_msw_phy_addr;
	uint32_t avtimer_lsw_phy_addr;
};
int avcs_core_open(void);
int avcs_core_disable_power_collapse(int disable);/* true or flase */

#endif
