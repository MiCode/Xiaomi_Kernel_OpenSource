/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _ION_IPC_TRUSTY_H_
#define _ION_IPC_TRUSTY_H_

struct ion_message {
	u32 cmd;
	u64 payload[2];
};

enum ion_command {
	TA_LOCK_DRM_MEM = 8,
	TA_UNLOCK_DRM_MEM  = 9
};

int ion_tipc_init(void);
ssize_t ion_tipc_write(void *data_ptr, size_t len);
ssize_t ion_tipc_read(void *data_ptr, size_t max_len);
void ion_tipc_exit(void);

#endif
