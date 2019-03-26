#ifndef _CALIB_HWDEP_H
#define _CALIB_HWDEP_H

#define WCD9XXX_CODEC_HWDEP_NODE    1000
#define AQT1000_CODEC_HWDEP_NODE    1001
#define Q6AFE_HWDEP_NODE    1002
enum wcd_cal_type {
	WCD9XXX_MIN_CAL,
	WCD9XXX_ANC_CAL = WCD9XXX_MIN_CAL,
	WCD9XXX_MAD_CAL,
	WCD9XXX_MBHC_CAL,
	WCD9XXX_VBAT_CAL,
	WCD9XXX_MAX_CAL,
};

struct wcdcal_ioctl_buffer {
	__u32 size;
	__u8 __user *buffer;
	enum wcd_cal_type cal_type;
};

#define SNDRV_CTL_IOCTL_HWDEP_CAL_TYPE \
	_IOW('U', 0x1, struct wcdcal_ioctl_buffer)

enum q6afe_cal_type {
	Q6AFE_MIN_CAL,
	Q6AFE_VAD_CORE_CAL = Q6AFE_MIN_CAL,
	Q6AFE_MAX_CAL,
};

struct q6afecal_ioctl_buffer {
	__u32 size;
	__u8 __user *buffer;
	enum q6afe_cal_type cal_type;
};

#define SNDRV_IOCTL_HWDEP_VAD_CAL_TYPE \
	_IOW('U', 0x1, struct q6afecal_ioctl_buffer)

#endif /*_CALIB_HWDEP_H*/
