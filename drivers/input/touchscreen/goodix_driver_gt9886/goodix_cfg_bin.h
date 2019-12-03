#ifndef _GOODIX_CFG_BIN_H_
#define _GOODIX_CFG_BIN_H_

#include "goodix_ts_core.h"

#define TS_DEFAULT_CFG_BIN "goodix_gt9886_cfg_f10.bin"
#define TS_BIN_VERSION_START_INDEX	5
#define TS_BIN_VERSION_LEN	4
#define TS_CFG_BIN_HEAD_RESERVED_LEN	6
#define TS_CFG_OFFSET_LEN	2
#define TS_IC_TYPE_NAME_MAX_LEN	15
#define TS_CFG_BIN_HEAD_LEN (sizeof(struct goodix_cfg_bin_head) + TS_CFG_BIN_HEAD_RESERVED_LEN)
#define TS_PKG_CONST_INFO_LEN  (sizeof(struct goodix_cfg_pkg_const_info))
#define TS_PKG_REG_INFO_LEN (sizeof(struct goodix_cfg_pkg_reg_info))
#define TS_PKG_HEAD_LEN (TS_PKG_CONST_INFO_LEN + TS_PKG_REG_INFO_LEN)

/*cfg block definitin*/
#define TS_CFG_BLOCK_PID_LEN	8
#define TS_CFG_BLOCK_VID_LEN	8
#define TS_CFG_BLOCK_FW_MASK_LEN	9
#define TS_CFG_BLOCK_FW_PATCH_LEN	4
#define TS_CFG_BLOCK_RESERVED_LEN	9

#define TS_NORMAL_CFG 0x01
#define TS_HIGH_SENSE_CFG 0x03
#define TS_RQST_FW_RETRY_TIMES 2
#define TS_LOCKDOWN_REG 0xBDB4

#pragma pack(1)
struct goodix_cfg_pkg_reg {
	u16 addr;
	u8 reserved1;
	u8 reserved2;
};

struct goodix_cfg_pkg_const_info {
	u32 pkg_len;
	u8 ic_type[TS_IC_TYPE_NAME_MAX_LEN];
	u8 cfg_type;
	u8 sensor_id;
	u8 hw_pid[TS_CFG_BLOCK_PID_LEN];
	u8 hw_vid[TS_CFG_BLOCK_VID_LEN];
	u8 fw_mask[TS_CFG_BLOCK_FW_MASK_LEN];
	u8 fw_patch[TS_CFG_BLOCK_FW_PATCH_LEN];
	u16 x_res_offset;
	u16 y_res_offset;
	u16 trigger_offset;
};

struct goodix_cfg_pkg_reg_info {
	struct goodix_cfg_pkg_reg cfg_send_flag;
	struct goodix_cfg_pkg_reg version_base;
	struct goodix_cfg_pkg_reg pid;
	struct goodix_cfg_pkg_reg vid;
	struct goodix_cfg_pkg_reg sensor_id;
	struct goodix_cfg_pkg_reg fw_mask;
	struct goodix_cfg_pkg_reg fw_status;
	struct goodix_cfg_pkg_reg cfg_addr;
	struct goodix_cfg_pkg_reg esd;
	struct goodix_cfg_pkg_reg command;
	struct goodix_cfg_pkg_reg coor;
	struct goodix_cfg_pkg_reg gesture;
	struct goodix_cfg_pkg_reg fw_request;
	struct goodix_cfg_pkg_reg proximity;
	u8 reserved[TS_CFG_BLOCK_RESERVED_LEN];
};

struct goodix_cfg_bin_head {
	u32 bin_len;
	u8 checksum;
	u8 bin_version[TS_BIN_VERSION_LEN];
	u8 pkg_num;
};

#pragma pack()

struct goodix_cfg_package {
	struct goodix_cfg_pkg_const_info cnst_info;
	struct goodix_cfg_pkg_reg_info reg_info;
	const u8 *cfg;
	u32 pkg_len;
};



struct goodix_cfg_bin {
	unsigned char *bin_data;
	unsigned int bin_data_len;
	struct goodix_cfg_bin_head head;
	struct goodix_cfg_package *cfg_pkgs;
};


int goodix_cfg_bin_proc(void *data);

int goodix_parse_cfg_bin(struct goodix_cfg_bin *cfg_bin);

int goodix_get_reg_and_cfg(struct goodix_ts_device *ts_dev, struct goodix_cfg_bin *cfg_bin);

int goodix_read_cfg_bin(struct device *dev, struct goodix_cfg_bin *cfg_bin);

int goodix_read_cfg_bin_from_dts(struct device_node *node, struct goodix_cfg_bin *cfg_bin);

void goodix_cfg_pkg_leToCpu(struct goodix_cfg_package *pkg);

int goodix_start_cfg_bin(struct goodix_ts_core *ts_core);

#endif
