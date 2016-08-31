#ifndef __LINUX_LM3528_BACKLIGHT_H
#define __LINUX_LM3528_BACKLIGHT_H

#include <linux/backlight.h>

struct lm3528_platform_data {
	unsigned int dft_brightness;
	bool (*is_powered)(void);
	int (*notify)(struct device *dev, int brightness);
};

#endif

