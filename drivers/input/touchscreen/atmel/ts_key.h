
#ifndef __ATMEL_TS_KEY_H
#define __ATMEL_TS_KEY_H

#include <linux/types.h>



#define CONFIG_MXT_WAKEUP_KEY_T24_INDEX				8
#define CONFIG_MXT_WAKEUP_KEY_T61_INDEX				3
#define CONFIG_MXT_WAKEUP_KEY_T81_INDEX				3
#define CONFIG_MXT_WAKEUP_KEY_T92_SYMBOL_INDEX		4
#define CONFIG_MXT_WAKEUP_KEY_T92_STROKE_INDEX		4
#define CONFIG_MXT_WAKEUP_KEY_T93_INDEX				8

struct mxt_virtual_key_space{
	int x_edge;
	int x_space;
	int y_top;
	int y_bottom;
};

struct kobject * create_virtual_key_object(struct device *dev);

#endif
