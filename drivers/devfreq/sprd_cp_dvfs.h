/* SPDX-License-Identifier: GPL-2.0 */
#ifndef SPRD_CPDVFS_H
#define SPRD_CPDVFS_H

/* SBUF Config */
#define CP_DVFS_SBUF_NUM  1
#define CP_DVFS_SBUFID  0
#define CP_DVFS_TXBUFSIZE  0x200
#define CP_DVFS_RXBUFSIZE  0x200
#define SBUF_RD_TIMEOUT_MS  500
#define SBUF_TRY_WAIT_TIEMS  10
#define SBUF_TRY_WAIT_MS  100

#define CMD_PARA_MAX_LEN  64
#define RECORDS_MAX_NUM  20

enum dvfs_index {
	DVFS_INDEX_MODE_LEAVE = -1,
	DVFS_INDEX_0 = 0,
	DVFS_INDEX_1,
	DVFS_INDEX_2,
	DVFS_INDEX_3,
	DVFS_INDEX_4,
	DVFS_INDEX_5,
	DVFS_INDEX_6,
	DVFS_INDEX_7,
	DVFS_INDEX_MAX
};

enum device_id_list {
	DVFS_CORE,
	DVFS_AXI,
	DVFS_APB,
	DVFS_ATBM,
	DVFS_ACCEMC,/* only phycp */
	DVFS_DEVICE_MAX
};

enum core_id_list {
	DVFS_CORE_PUBCP = 0,
	DVFS_CORE_LDSP,
	DVFS_CORE_PSCP,
	DVFS_CORE_PHYCP,
	DVFS_CORE_MAX
};

enum dvfs_cmd_type {
/* get cmd need return to AP */
	DVFS_GET_CMD_BEGIN = 0,
	DVFS_GET_TABLE,
	DVFS_GET_RECORD,
	DVFS_GET_AUTO,
	DVFS_GET_INDEX,
	DVFS_GET_IDLE_INDEX,
	DVFS_GET_REG,
	DVFS_GET_CMD_END = 0x80,

/* set cmd no need return to AP */
	DVFS_SET_CMD_BEGIN = 0x80,
	DVFS_SET_AUTO,
	DVFS_SET_INDEX,
	DVFS_SET_IDLE_INDEX,
	DVFS_SET_REG,
	DVFS_CMD_MAX
};

struct cp_dvfs_record {
	u32 time_32k;
	u8 index;
};

struct reg_t {
	u32 reg_addr;
	u32 reg_val;
};

struct device_info {
	const char *name;
	u32 dev_id;
	u8 index;
	u8 idle_index;
	struct cp_dvfs_record records[RECORDS_MAX_NUM];
};
/**
 * =  AP-CP cmd format format =
 * | id |cmd type  |cmd para... |
 * | 1 Byte  | 1 Byte   |n bytes |
 */
struct cmd_pkt {
/* reuse：pubcp&wtlcp use core_id; pscp&phycp use device id */
	u8 id;
	u8 cmd;
	u8 para[0];
};

struct userspace_data {
	u8 auto_enable;
	struct reg_t set_reg;
	struct reg_t inq_reg;
	u8 curr_dev_id;
	struct device_info *devices;
};

struct cpdvfs_data {
	struct device *dev;
	const char *name;
	u32 core_id;
	u32 dst;
	u32 devices_num;
	u32 record_num;
	u8 rd_buf[CP_DVFS_RXBUFSIZE];
	struct userspace_data *user_data;
	struct cmd_pkt *sent_cmd;
	u32 cmd_len;
};
#endif
