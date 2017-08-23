#ifndef HABMM_H
#define HABMM_H

#include <linux/types.h>

struct hab_send {
	__u64 data;
	__s32 vcid;
	__u32 sizebytes;
	__u32 flags;
};

struct hab_recv {
	__u64 data;
	__s32 vcid;
	__u32 sizebytes;
	__u32 flags;
};

struct hab_open {
	__s32 vcid;
	__u32 mmid;
	__u32 timeout;
	__u32 flags;
};

struct hab_close {
	__s32 vcid;
	__u32 flags;
};

struct hab_export {
	__u64 buffer;
	__s32 vcid;
	__u32 sizebytes;
	__u32 exportid;
	__u32 flags;
};

struct hab_import {
	__u64 index;
	__u64 kva;
	__s32 vcid;
	__u32 sizebytes;
	__u32 exportid;
	__u32 flags;
};

struct hab_unexport {
	__s32 vcid;
	__u32 exportid;
	__u32 flags;
};

struct hab_unimport {
	__s32 vcid;
	__u32 exportid;
	__u64 kva;
	__u32 flags;
};

#define HAB_IOC_TYPE 0x0A
#define HAB_MAX_MSG_SIZEBYTES 0x1000
#define HAB_MAX_EXPORT_SIZE 0x8000000

#define HAB_MMID_CREATE(major, minor) ((major&0xFFFF) | ((minor&0xFF)<<16))

#define MM_AUD_START	100
#define MM_AUD_1	101
#define MM_AUD_2	102
#define MM_AUD_3	103
#define MM_AUD_4	104
#define MM_AUD_END	105

#define MM_CAM_START	200
#define MM_CAM		201
#define MM_CAM_END	202

#define MM_DISP_START	300
#define MM_DISP_1	301
#define MM_DISP_2	302
#define MM_DISP_3	303
#define MM_DISP_4	304
#define MM_DISP_5	305
#define MM_DISP_END	306

#define MM_GFX_START	400
#define MM_GFX		401
#define MM_GFX_END	402

#define MM_VID_START	500
#define MM_VID		501
#define MM_VID_END	502

#define MM_MISC_START	600
#define MM_MISC		601
#define MM_MISC_END	602

#define MM_QCPE_START	700
#define MM_QCPE_VM1	701
#define MM_QCPE_VM2	702
#define MM_QCPE_VM3	703
#define MM_QCPE_VM4	704
#define MM_QCPE_END	705
#define MM_ID_MAX	706

#define HABMM_SOCKET_OPEN_FLAGS_SINGLE_BE_SINGLE_FE        0x00000000
#define HABMM_SOCKET_OPEN_FLAGS_SINGLE_BE_SINGLE_DOMU      0x00000001
#define HABMM_SOCKET_OPEN_FLAGS_SINGLE_BE_MULTI_DOMUS      0x00000002

#define HABMM_SOCKET_SEND_FLAGS_NON_BLOCKING 0x00000001

#define HABMM_SOCKET_RECV_FLAGS_NON_BLOCKING 0x00000001

#define HABMM_EXP_MEM_TYPE_DMA 0x00000001

#define HABMM_IMPORT_FLAGS_CACHED 0x00000001

#define IOCTL_HAB_SEND \
	_IOW(HAB_IOC_TYPE, 0x2, struct hab_send)

#define IOCTL_HAB_RECV \
	_IOWR(HAB_IOC_TYPE, 0x3, struct hab_recv)

#define IOCTL_HAB_VC_OPEN \
	_IOWR(HAB_IOC_TYPE, 0x4, struct hab_open)

#define IOCTL_HAB_VC_CLOSE \
	_IOW(HAB_IOC_TYPE, 0x5, struct hab_close)

#define IOCTL_HAB_VC_EXPORT \
	_IOWR(HAB_IOC_TYPE, 0x6, struct hab_export)

#define IOCTL_HAB_VC_IMPORT \
	_IOWR(HAB_IOC_TYPE, 0x7, struct hab_import)

#define IOCTL_HAB_VC_UNEXPORT \
	_IOW(HAB_IOC_TYPE, 0x8, struct hab_unexport)

#define IOCTL_HAB_VC_UNIMPORT \
	_IOW(HAB_IOC_TYPE, 0x9, struct hab_unimport)

#endif /* HABMM_H */
