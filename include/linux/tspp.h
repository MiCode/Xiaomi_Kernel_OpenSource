#ifndef _TSPP_H_
#define _TSPP_H_

#include <linux/ioctl.h>

#define TSPP_NUM_SYSTEM_KEYS 8

enum tspp_key_parity {
	TSPP_KEY_PARITY_EVEN,
	TSPP_KEY_PARITY_ODD
};

enum tspp_source {
	TSPP_SOURCE_TSIF0,
	TSPP_SOURCE_TSIF1,
	TSPP_SOURCE_MEM,
	TSPP_SOURCE_NONE = -1
};

enum tspp_mode {
	TSPP_MODE_DISABLED,
	TSPP_MODE_PES,
	TSPP_MODE_RAW,
	TSPP_MODE_RAW_NO_SUFFIX
};

enum tspp_tsif_mode {
	TSPP_TSIF_MODE_LOOPBACK, /* loopback mode */
	TSPP_TSIF_MODE_1,        /* without sync */
	TSPP_TSIF_MODE_2         /* with sync signal */
};

struct tspp_filter {
	int pid;
	int mask;
	enum tspp_mode mode;
	int priority;	/* 0 - 15 */
	int decrypt;
	enum tspp_source source;
};

struct tspp_select_source {
	enum tspp_source source;
	enum tspp_tsif_mode mode;
	int clk_inverse;
	int data_inverse;
	int sync_inverse;
	int enable_inverse;
};

struct tspp_pid {
	int pid;
};

struct tspp_key {
	enum tspp_key_parity parity;
	int lsb;
	int msb;
};

struct tspp_iv {
	int data[2];
};

struct tspp_system_keys {
	int data[TSPP_NUM_SYSTEM_KEYS];
};

struct tspp_buffer {
	int size;
};

/* defines for IOCTL functions */
/* read Documentation/ioctl-number.txt */
/* some random number to avoid coinciding with other ioctl numbers */
#define TSPP_IOCTL_BASE					0xAA
#define TSPP_IOCTL_SELECT_SOURCE		\
	_IOW(TSPP_IOCTL_BASE, 0, struct tspp_select_source)
#define TSPP_IOCTL_ADD_FILTER			\
	_IOW(TSPP_IOCTL_BASE, 1, struct tspp_filter)
#define TSPP_IOCTL_REMOVE_FILTER		\
	_IOW(TSPP_IOCTL_BASE, 2, struct tspp_pid)
#define TSPP_IOCTL_SET_KEY				\
	_IOW(TSPP_IOCTL_BASE, 3, struct tspp_key)
#define TSPP_IOCTL_SET_IV				\
	_IOW(TSPP_IOCTL_BASE, 4, struct tspp_iv)
#define TSPP_IOCTL_SET_SYSTEM_KEYS	\
	_IOW(TSPP_IOCTL_BASE, 5, struct tspp_system_keys)
#define TSPP_IOCTL_BUFFER_SIZE		\
	_IOW(TSPP_IOCTL_BASE, 6, struct tspp_buffer)

#endif /* _TSPP_H_ */
