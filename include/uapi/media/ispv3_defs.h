/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 */

#ifndef __UAPI_ISP_DEFS_H__
#define __UAPI_ISP_DEFS_H__

#include <linux/videodev2.h>
#include <linux/types.h>
#include <linux/ioctl.h>

#define ISPV3_VNODE_NAME "ispv3-devnode"

#define MFNR_REF_FRAME_NUM      7

#define ISP_DEVICE_TYPE_BASE		(MEDIA_ENT_F_OLD_BASE + 20)
#define ISP_VNODE_DEVICE_TYPE		(ISP_DEVICE_TYPE_BASE + 1)

/* MMAP offset definition */
#define ISP_MITOP_REG_MAPOFFSET			0
#define ISP_MIPORT_REG_MAPOFFSET		PAGE_SIZE
#define ISP_MITOP_OCRAM_MAPOFFSET		(2*PAGE_SIZE)
#define ISP_MITOP_DDR_MAPOFFSET			(3*PAGE_SIZE)

/***MMAP Mapping Addr***/
#define MIPORT_REG_SIZE			0x200000
#define MIPORT_REG_OFFSET		0xe00000
#define MIPORT_REG_MASK			0xFFE00000
#define MIPORT_REG_MIN_ADDR		0xFFE00000
#define MIPORT_REG_MAX_ADDR		(MIPORT_REG_MIN_ADDR-1+MIPORT_REG_SIZE)

#define MITOP_REG_SIZE			0x200000
#define MITOP_REG_OFFSET		0x0
#define MITOP_REG_MASK			0xFF000000
#define MITOP_REG_MIN_ADDR		0xFF000000
#define MITOP_REG_MAX_ADDR		(MITOP_REG_MIN_ADDR-1+MITOP_REG_SIZE)

#define MITOP_OCRAM_SIZE		0x750000  /*include 300K OCRAM for test**/
#define MITOP_OCRAM_OFFSET		0x0
#define MITOP_OCRAM_MASK		0x7F000000
#define MITOP_OCRAM_MIN_ADDR		0x7F000000
#define MITOP_OCRAM_MAX_ADDR		(MITOP_OCRAM_MIN_ADDR-1+MITOP_OCRAM_SIZE)

#define MITOP_DDR_SIZE			0x10000000
#define MITOP_DDR_OFFSET		0x0
#define MITOP_DDR_MASK			0x00000000
#define MITOP_DDR_MIN_ADDR		0x00000000
#define MITOP_DDR_MAX_ADDR		(MITOP_DDR_MIN_ADDR-1+MITOP_DDR_SIZE)

/**
 * struct ispv3_control - Structure used by ioctl control for ispv3
 *
 * @op_code:          This is the op code for isp control
 * @size:                  Control command size
 * @handle:              Control command payload
 */
struct ispv3_control {
	uint32_t        op_code;
	uint32_t        size;
	uint64_t        handle;
};

/* ISP IOCTL */
#define VIDIOC_ISPV3_CONTROL \
	_IOWR('V', BASE_VIDIOC_PRIVATE+15, struct ispv3_control)


struct ispv3_mfnr_capture_info {
	int32_t full_keyframe_fd;
	int32_t full_refframe_fd[MFNR_REF_FRAME_NUM];
	int32_t ds_keyframe_fd;
	int32_t ds_refframe_fd[MFNR_REF_FRAME_NUM];
	int32_t result_fd;
	bool is_transfer_ds;
};

//=====================================================

enum isp_interface_type {
	ISP_INTERFACE_INVALID = 0,
	ISP_INTERFACE_IIC,
	ISP_INTERFACE_SPI,
	ISP_INTERFACE_PCIE,
	ISP_INTERFACE_MAX,
};

struct ispv3_reg_array {
	uint32_t reg_addr;
	uint32_t reg_data;
};

enum isp_access_mode {
	ISP_SINGLECPY,
	ISP_BURSTCPY,
	ISP_BURSTSET,
};

struct ispv3_reg_burst_setting {
	uint32_t reg_addr;
	uint64_t data_buf;
};

struct ispv3_config_setting {
	enum isp_interface_type bus_type;
	enum isp_access_mode access_mode;
	uint32_t data_num;
	union {
		//single mode read&write
		struct ispv3_reg_array *reg_array_buf;
		//burst mode read&write
		struct ispv3_reg_burst_setting reg_burst_setting;
		//burst mode memset
		struct ispv3_reg_array reg_array;
	};
};
struct ispv3_info {
	int		ispv3_fd;
	int		fd_mem;
	uint32_t	*ispv3_miport_reg_pointer;
	uint32_t	*ispv3_mitop_reg_pointer;
	uint32_t	*ispv3_mitop_ocram_pointer;
	uint32_t	*ispv3_ddr_pointer;
};
#endif
