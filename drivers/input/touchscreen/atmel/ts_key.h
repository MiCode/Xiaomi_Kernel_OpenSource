
#ifndef __ATMEL_TS_KEY_H
#define __ATMEL_TS_KEY_H

#include <linux/types.h>

#define MAX_KEYS_SUPPORTED_IN_DRIVER 6

struct mxt_virtual_key_space {
	int x_edge;
	int x_space;
	int y_top;
	int y_bottom;
};

struct kobject *create_virtual_key_object(struct device *dev);

#endif
