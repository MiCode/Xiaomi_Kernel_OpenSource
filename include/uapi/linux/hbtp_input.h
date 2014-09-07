#ifndef _UAPI_HBTP_INPUT_H
#define _UAPI_HBTP_INPUT_H

#include <linux/input.h>

#define HBTP_MAX_FINGER		20

struct hbtp_input_touch {
	bool active;
	__s32 tool;
	__s32 x;
	__s32 y;
	__s32 pressure;
	__s32 major;
	__s32 minor;
	__s32 orientation;
};

struct hbtp_input_mt {
	__s32 num_touches;
	struct hbtp_input_touch touches[HBTP_MAX_FINGER];
	struct timeval time_val;
};

struct hbtp_input_absinfo {
	bool  active;
	__u16 code;
	__s32 minimum;
	__s32 maximum;
};

enum hbtp_afe_power_cmd {
	HBTP_AFE_POWER_ON,
	HBTP_AFE_POWER_OFF,
};

struct hbtp_input_key {
	__u32 code;
	__s32 value;
};

/* ioctl */
#define HBTP_INPUT_IOCTL_BASE	'T'
#define HBTP_SET_ABSPARAM	_IOW(HBTP_INPUT_IOCTL_BASE, 201, \
					struct hbtp_input_absinfo *)
#define HBTP_SET_TOUCHDATA	_IOW(HBTP_INPUT_IOCTL_BASE, 202, \
					struct hbtp_input_mt)
#define HBTP_SET_POWERSTATE	_IOW(HBTP_INPUT_IOCTL_BASE, 203, \
					enum hbtp_afe_power_cmd)
#define HBTP_SET_KEYDATA	_IOW(HBTP_INPUT_IOCTL_BASE, 204, \
					struct hbtp_input_key)

#endif	/* _UAPI_HBTP_INPUT_H */

