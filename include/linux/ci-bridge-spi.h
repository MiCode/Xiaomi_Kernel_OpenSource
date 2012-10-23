#ifndef _CI_BRIDGE_SPI_H_
#define _CI_BRIDGE_SPI_H_

#include <uapi/linux/ci-bridge-spi.h>

struct ci_bridge_platform_data {
	unsigned int reset_pin;
	unsigned int interrupt_pin;
};

#endif
