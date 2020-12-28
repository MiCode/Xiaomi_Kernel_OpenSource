#ifndef _DEV_DEP_H
#define _DEV_DEP_H

#include <linux/types.h>

struct dolby_param_data {
	__s32 version;
	__s32 device_id;
	__s32 be_id;
	__s32 param_id;
	__s32 length;
	__s32 __user *data;
};

struct dolby_param_license {
	__s32 dmid;
	__s32 license_key;
};

#define SNDRV_DEVDEP_DAP_IOCTL_SET_PARAM\
		_IOWR('U', 0x10, struct dolby_param_data)
#define SNDRV_DEVDEP_DAP_IOCTL_GET_PARAM\
		_IOR('U', 0x11, struct dolby_param_data)
#define SNDRV_DEVDEP_DAP_IOCTL_DAP_COMMAND\
		_IOWR('U', 0x13, struct dolby_param_data)
#define SNDRV_DEVDEP_DAP_IOCTL_DAP_LICENSE\
		_IOWR('U', 0x14, struct dolby_param_license)
#define SNDRV_DEVDEP_DAP_IOCTL_GET_VISUALIZER\
		_IOR('U', 0x15, struct dolby_param_data)

#define DTS_EAGLE_MODULE			0x00005000
#define DTS_EAGLE_MODULE_ENABLE			0x00005001
#define EAGLE_DRIVER_ID				0xF2
#define DTS_EAGLE_IOCTL_GET_CACHE_SIZE		_IOR(EAGLE_DRIVER_ID, 0, int)
#define DTS_EAGLE_IOCTL_SET_CACHE_SIZE		_IOW(EAGLE_DRIVER_ID, 1, int)
#define DTS_EAGLE_IOCTL_GET_PARAM		_IOR(EAGLE_DRIVER_ID, 2, void*)
#define DTS_EAGLE_IOCTL_SET_PARAM		_IOW(EAGLE_DRIVER_ID, 3, void*)
#define DTS_EAGLE_IOCTL_SET_CACHE_BLOCK		_IOW(EAGLE_DRIVER_ID, 4, void*)
#define DTS_EAGLE_IOCTL_SET_ACTIVE_DEVICE	_IOW(EAGLE_DRIVER_ID, 5, void*)
#define DTS_EAGLE_IOCTL_GET_LICENSE		_IOR(EAGLE_DRIVER_ID, 6, void*)
#define DTS_EAGLE_IOCTL_SET_LICENSE		_IOW(EAGLE_DRIVER_ID, 7, void*)
#define DTS_EAGLE_IOCTL_SEND_LICENSE		_IOW(EAGLE_DRIVER_ID, 8, int)
#define DTS_EAGLE_IOCTL_SET_VOLUME_COMMANDS	_IOW(EAGLE_DRIVER_ID, 9, void*)
#define DTS_EAGLE_FLAG_IOCTL_PRE		(1<<30)
#define DTS_EAGLE_FLAG_IOCTL_JUSTSETCACHE	(1<<31)
#define DTS_EAGLE_FLAG_IOCTL_GETFROMCORE       DTS_EAGLE_FLAG_IOCTL_JUSTSETCACHE
#define DTS_EAGLE_FLAG_IOCTL_MASK		(~(DTS_EAGLE_FLAG_IOCTL_PRE | \
					     DTS_EAGLE_FLAG_IOCTL_JUSTSETCACHE))
#define DTS_EAGLE_FLAG_ALSA_GET			(1<<31)

struct dts_eagle_param_desc {
	__u32 id;
	__u32 size;
	__s32 offset;
	__u32 device;
} __packed;

#define HWDEP_FE_BASE                   3000 /*unique base for FE hw dep nodes*/
struct snd_pcm_mmap_fd {
	__s32 dir;
	__s32 fd;
	__s32 size;
	__s32 actual_size;
};

struct snd_pcm_prsnt_position {
	__u64 timestamp;
	__u64 frames;
	__s32 clock_id;
};

#define SNDRV_PCM_IOCTL_MMAP_DATA_FD    _IOWR('U', 0xd2, struct snd_pcm_mmap_fd)
#define SNDRV_PCM_IOCTL_DSP_POSITION\
			_IOWR('U', 0xd3, struct snd_pcm_prsnt_position)

#endif
