#ifndef _MEMSW_STATE_H
#define _MEMSW_STATE_H

#include <linux/types.h>
#include <linux/ioctl.h>

struct memsw_state_data {
	uint8_t low_mem_triggered;
	uint8_t low_swap_triggered;
	unsigned long mem_threshold;
	unsigned long swap_threshold;
	unsigned long current_freeram;
	unsigned long current_freeswap;
};

struct memsw_dev {
	struct memsw_state_data memsw_data;
	struct miscdevice 	misc_dev;
	wait_queue_head_t 	wq;
	struct list_head 	reader_list;
	struct mutex 	mutex; /* protects reader_list */
};

struct memsw_reader {
	struct memsw_dev *dev; /* associated device*/
	struct list_head list; /* node in memsw_dev.reader_list*/
	uint8_t auth_rights;
	uint8_t version;
};

struct memsw_dev *memsw_dev_get_wo_check(void);
struct memsw_dev *memsw_dev_get_w_check(int minor);
void wakeup_kmemsw_chkd(void);

#define __MEMSWIO	0xAE

#define GET_MEM_THRESHOLD		_IO(__MEMSWIO, 0x11)
#define SET_MEM_THRESHOLD		_IO(__MEMSWIO, 0x12)
#define GET_SWAP_THRESHOLD		_IO(__MEMSWIO, 0x13)
#define SET_SWAP_THRESHOLD		_IO(__MEMSWIO, 0x14)
#define GET_MEMSW_DEV_VERSION	_IO(__MEMSWIO, 0x15)
#define SET_MEMSW_DEV_VERSION	_IO(__MEMSWIO, 0x16)

#endif
