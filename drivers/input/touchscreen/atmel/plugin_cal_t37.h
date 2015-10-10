
#ifndef __LINUX_ATMEL_MXT_PLUG_CAL
#define __LINUX_ATMEL_MXT_PLUG_CAL

#include <linux/types.h>

struct stat_grad_config {
	s16 thld;
	int percent;
};


extern struct plugin_cal mxt_plugin_cal;

#endif
