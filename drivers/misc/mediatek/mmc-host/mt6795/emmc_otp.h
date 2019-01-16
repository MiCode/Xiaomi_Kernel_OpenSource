#ifndef EMMC_OTP_H
#define EMMC_OTP_H

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#define OTP_MAGIC_NUM           0x4E3AF28B

struct emmc_otp_config {
	u32(*read) (u32 blkno, void *BufferPtr);
	u32(*write) (u32 blkno, void *BufferPtr);
	u32(*query_length) (u32 *Length);
};

struct otp_ctl {
	unsigned int QLength;
	unsigned int Offset;
	unsigned int Length;
	char *BufferPtr;
	unsigned int status;
};

#ifdef CONFIG_COMPAT
struct compat_otp_ctl {
	compat_uint_t QLength;
	compat_uint_t Offset;
	compat_uint_t Length;
	compat_uptr_t BufferPtr;
	unsigned int status;
};
#endif


#define EMMC_OTP_START_ADDRESS   (0xc0000000)	/* just for debug */

#define EMMC_HOST_NUM            0
#define EMMC_OTP_MAGIC           'k'

/* EMMC OTP IO control number */
#define EMMC_OTP_GET_LENGTH    _IOW(EMMC_OTP_MAGIC, 1, int)
#define EMMC_OTP_READ          _IOW(EMMC_OTP_MAGIC, 2, int)
#define EMMC_OTP_WRITE         _IOW(EMMC_OTP_MAGIC, 3, int)

#ifdef CONFIG_COMPAT
#define COMPAT_EMMC_OTP_GET_LENGTH       _IOW(EMMC_OTP_MAGIC, 1, compat_int_t)
#define COMPAT_EMMC_OTP_READ             _IOW(EMMC_OTP_MAGIC, 2, compat_int_t)
#define COMPAT_EMMC_OTP_WRITE            _IOW(EMMC_OTP_MAGIC, 3, compat_int_t)
#endif

#define FS_EMMC_OTP_READ         0
#define FS_EMMC_OTP_WRITE        1

/* EMMC OTP Error codes */
#define OTP_SUCCESS                   0
#define OTP_ERROR_OVERSCOPE          -1
#define OTP_ERROR_TIMEOUT            -2
#define OTP_ERROR_BUSY               -3
#define OTP_ERROR_NOMEM              -4
#define OTP_ERROR_RESET              -5

#endif				/* end of EMMC_OTP_H */
