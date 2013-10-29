#ifndef _CI_BRIDGE_SPI_H_
#define _CI_BRIDGE_SPI_H_

#include <uapi/linux/ci-bridge-spi.h>

struct ci_bridge_platform_data {
	int reset_pin;
	int interrupt_pin;
};

#endif
