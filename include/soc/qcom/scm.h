/* Copyright (c) 2010-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __MACH_SCM_H
#define __MACH_SCM_H

#define SCM_SVC_BOOT			0x1
#define SCM_SVC_PIL			0x2
#define SCM_SVC_UTIL			0x3
#define SCM_SVC_TZ			0x4
#define SCM_SVC_IO			0x5
#define SCM_SVC_INFO			0x6
#define SCM_SVC_SSD			0x7
#define SCM_SVC_FUSE			0x8
#define SCM_SVC_PWR			0x9
#define SCM_SVC_MP			0xC
#define SCM_SVC_DCVS			0xD
#define SCM_SVC_ES			0x10
#define SCM_SVC_HDCP			0x11
#define SCM_SVC_TZSCHEDULER		0xFC

#define SCM_FUSE_READ			0x7
#define SCM_CMD_HDCP			0x01

/* SCM Features */
#define SCM_SVC_SEC_CAMERA		0xD

#define DEFINE_SCM_BUFFER(__n) \
static char __n[PAGE_SIZE] __aligned(PAGE_SIZE);

#define SCM_BUFFER_SIZE(__buf)	sizeof(__buf)

#define SCM_BUFFER_PHYS(__buf)	virt_to_phys(__buf)

#ifdef CONFIG_MSM_SCM
extern int scm_call(u32 svc_id, u32 cmd_id, const void *cmd_buf, size_t cmd_len,
		void *resp_buf, size_t resp_len);

extern int scm_call_noalloc(u32 svc_id, u32 cmd_id, const void *cmd_buf,
		size_t cmd_len, void *resp_buf, size_t resp_len,
		void *scm_buf, size_t scm_buf_size);


extern s32 scm_call_atomic1(u32 svc, u32 cmd, u32 arg1);
extern s32 scm_call_atomic1_1(u32 svc, u32 cmd, u32 arg1, u32 *ret1);
extern s32 scm_call_atomic2(u32 svc, u32 cmd, u32 arg1, u32 arg2);
extern s32 scm_call_atomic3(u32 svc, u32 cmd, u32 arg1, u32 arg2, u32 arg3);
extern s32 scm_call_atomic4_3(u32 svc, u32 cmd, u32 arg1, u32 arg2, u32 arg3,
		u32 arg4, u32 *ret1, u32 *ret2);

#define SCM_VERSION(major, minor) (((major) << 16) | ((minor) & 0xFF))

extern u32 scm_get_version(void);
extern int scm_is_call_available(u32 svc_id, u32 cmd_id);
extern int scm_get_feat_version(u32 feat);

#define SCM_HDCP_MAX_REG 5

struct scm_hdcp_req {
	u32 addr;
	u32 val;
};

#else

static inline int scm_call(u32 svc_id, u32 cmd_id, const void *cmd_buf,
		size_t cmd_len, void *resp_buf, size_t resp_len)
{
	return 0;
}

static inline int scm_call_noalloc(u32 svc_id, u32 cmd_id,
		const void *cmd_buf, size_t cmd_len, void *resp_buf,
		size_t resp_len, void *scm_buf, size_t scm_buf_size)
{
	return 0;
}

static inline s32 scm_call_atomic1(u32 svc, u32 cmd, u32 arg1)
{
	return 0;
}

static inline s32 scm_call_atomic1_1(u32 svc, u32 cmd, u32 arg1, u32 *ret1)
{
	return 0;
}

static inline s32 scm_call_atomic2(u32 svc, u32 cmd, u32 arg1, u32 arg2)
{
	return 0;
}

static inline s32 scm_call_atomic3(u32 svc, u32 cmd, u32 arg1, u32 arg2,
		u32 arg3)
{
	return 0;
}

static inline s32 scm_call_atomic4_3(u32 svc, u32 cmd, u32 arg1, u32 arg2,
		u32 arg3, u32 arg4, u32 *ret1, u32 *ret2)
{
	return 0;
}

static inline u32 scm_get_version(void)
{
	return 0;
}

static inline int scm_is_call_available(u32 svc_id, u32 cmd_id)
{
	return 0;
}

static inline int scm_get_feat_version(u32 feat)
{
	return 0;
}

#endif
#endif
