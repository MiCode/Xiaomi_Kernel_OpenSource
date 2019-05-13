#ifndef _DSI_ACCESS_H_
#define _DSI_ACCESS_H_

#include "mdss_dsi.h"

#define DSI_ACCESS

#define DSI_ACCESS_DIR "dsi_access"

#define BUFFER_LENGTH 1024

enum read_write {
	CMD_READ = 0,
	CMD_WRITE = 1,
};

struct dsi_access {
	unsigned int cmd_length;
	unsigned int read_length;
	unsigned int desc_length;
	unsigned char cmd_buffer[BUFFER_LENGTH];
	unsigned char read_buffer[BUFFER_LENGTH];
	unsigned char *desc_buffer;
	struct kobject *sysfs_dir;
	struct dsi_panel_cmds cmds;
	struct attribute_group attr_group;
};

#endif
