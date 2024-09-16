/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __FM_RDS_H__
#define __FM_RDS_H__
#include "fm_typedef.h"

/* FM_RDS_DATA_CRC_FFOST */
#define FM_RDS_GDBK_IND_A	 (0x08)
#define FM_RDS_GDBK_IND_B	 (0x04)
#define FM_RDS_GDBK_IND_C	 (0x02)
#define FM_RDS_GDBK_IND_D	 (0x01)
#define FM_RDS_DCO_FIFO_OFST (0x01E0)
#define	FM_RDS_READ_DELAY	 (0x80)

#define RDS_RX_BLOCK_PER_GROUP (4)
#define RDS_RX_GROUP_SIZE (2*RDS_RX_BLOCK_PER_GROUP)
#define MAX_RDS_RX_GROUP_CNT (12)
#define RDS_RT_MULTI_REV_TH 80	/* 100 */
/* #define RDS_CBC_DEPENDENCY */

struct rds_packet_t {
	unsigned short blkA;
	unsigned short blkB;
	unsigned short blkC;
	unsigned short blkD;
	unsigned short cbc;		/* correct bit cnt */
	unsigned short crc;		/* crc checksum */
};

struct rds_rx_t {
	unsigned short sin;
	unsigned short cos;
	struct rds_packet_t data[MAX_RDS_RX_GROUP_CNT];
};

enum rds_ps_state_machine_t {
	RDS_PS_START = 0,
	RDS_PS_DECISION,
	RDS_PS_GETLEN,
	RDS_PS_DISPLAY,
	RDS_PS_FINISH,
	RDS_PS_MAX
};

enum rds_rt_state_machine_t {
	RDS_RT_START = 0,
	RDS_RT_DECISION,
	RDS_RT_GETLEN,
	RDS_RT_DISPLAY,
	RDS_RT_FINISH,
	RDS_RT_MAX
};

enum {
	RDS_GRP_VER_A = 0,	/* group version A */
	RDS_GRP_VER_B
};

enum rds_blk_t {
	RDS_BLK_A = 0,
	RDS_BLK_B,
	RDS_BLK_C,
	RDS_BLK_D,
	RDS_BLK_MAX
};

/* For RDS feature, these strcutures also be defined in "fm.h" */
struct rds_flag_t {
	unsigned char TP;
	unsigned char TA;
	unsigned char Music;
	unsigned char Stereo;
	unsigned char Artificial_Head;
	unsigned char Compressed;
	unsigned char Dynamic_PTY;
	unsigned char Text_AB;
	unsigned int flag_status;
};

struct rds_ct_t {
	unsigned short Month;
	unsigned short Day;
	unsigned short Year;
	unsigned short Hour;
	unsigned short Minute;
	unsigned char Local_Time_offset_signbit;
	unsigned char Local_Time_offset_half_hour;
};

struct rds_af_t {
	signed short AF_Num;
	signed short AF[2][25];	/* 100KHz */
	unsigned char Addr_Cnt;
	unsigned char isMethod_A;
	unsigned char isAFNum_Get;
};

struct rds_ps_t {
	unsigned char PS[4][8];
	unsigned char Addr_Cnt;
};

struct rds_rt_t {
	unsigned char TextData[4][64];
	unsigned char GetLength;
	unsigned char isRTDisplay;
	unsigned char TextLength;
	unsigned char isTypeA;
	unsigned char BufCnt;
	unsigned short Addr_Cnt;
};

struct rds_raw_t {
	signed int dirty;		/* indicate if the data changed or not */
	signed int len;		/* the data len form chip */
	unsigned char data[148];
};

struct rds_group_cnt_t {
	unsigned int total;
	unsigned int groupA[16];	/* RDS groupA counter */
	unsigned int groupB[16];	/* RDS groupB counter */
};

enum rds_group_cnt_op_t {
	RDS_GROUP_CNT_READ = 0,
	RDS_GROUP_CNT_WRITE,
	RDS_GROUP_CNT_RESET,
	RDS_GROUP_CNT_MAX
};

struct rds_group_cnt_req_t {
	signed int err;
	enum rds_group_cnt_op_t op;
	struct rds_group_cnt_t gc;
};

struct rds_t {
	struct rds_ct_t CT;
	struct rds_flag_t RDSFlag;
	unsigned short PI;
	unsigned char Switch_TP;
	unsigned char PTY;
	struct rds_af_t AF_Data;
	struct rds_af_t AFON_Data;
	unsigned char Radio_Page_Code;
	unsigned short Program_Item_Number_Code;
	unsigned char Extend_Country_Code;
	unsigned short Language_Code;
	struct rds_ps_t PS_Data;
	unsigned char PS_ON[8];
	struct rds_rt_t RT_Data;
	/* will use RDSFlag_Struct RDSFlag->flag_status to check which event, is that ok? */
	unsigned short event_status;
	struct rds_group_cnt_t gc;
};

/* Need care the following definition. */
/* valid Rds Flag for notify */
enum rds_flag_status_t {
	RDS_FLAG_IS_TP = 0x0001,	/* Program is a traffic program */
	RDS_FLAG_IS_TA = 0x0002,	/* Program currently broadcasts a traffic ann. */
	RDS_FLAG_IS_MUSIC = 0x0004,	/* Program currently broadcasts music */
	RDS_FLAG_IS_STEREO = 0x0008,	/* Program is transmitted in stereo */
	RDS_FLAG_IS_ARTIFICIAL_HEAD = 0x0010,	/* Program is an artificial head recording */
	RDS_FLAG_IS_COMPRESSED = 0x0020,	/* Program content is compressed */
	RDS_FLAG_IS_DYNAMIC_PTY = 0x0040,	/* Program type can change */
	RDS_FLAG_TEXT_AB = 0x0080	/* If this flag changes state, a new radio text string begins */
};

enum rds_event_status_t {
	RDS_EVENT_FLAGS = 0x0001,	/* One of the RDS flags has changed state */
	RDS_EVENT_PI_CODE = 0x0002,	/* The program identification code has changed */
	RDS_EVENT_PTY_CODE = 0x0004,	/* The program type code has changed */
	RDS_EVENT_PROGRAMNAME = 0x0008,	/* The program name has changed */
	RDS_EVENT_UTCDATETIME = 0x0010,	/* A new UTC date/time is available */
	RDS_EVENT_LOCDATETIME = 0x0020,	/* A new local date/time is available */
	RDS_EVENT_LAST_RADIOTEXT = 0x0040,	/* A radio text string was completed */
	RDS_EVENT_AF = 0x0080,	/* Current Channel RF signal strength too weak, need do AF switch */
	RDS_EVENT_AF_LIST = 0x0100,	/* An alternative frequency list is ready */
	RDS_EVENT_AFON_LIST = 0x0200,	/* An alternative frequency list is ready */
	RDS_EVENT_TAON = 0x0400,	/* Other Network traffic announcement start */
	RDS_EVENT_TAON_OFF = 0x0800,	/* Other Network traffic announcement finished. */
	RDS_EVENT_ECC_CODE = 0x1000,	/* ECC code */
	RDS_EVENT_RDS = 0x2000,	/* RDS Interrupt had arrived durint timer period */
	RDS_EVENT_NO_RDS = 0x4000,	/* RDS Interrupt not arrived durint timer period */
	RDS_EVENT_RDS_TIMER = 0x8000	/* Timer for RDS Bler Check. ---- BLER  block error rate */
};

#define RDS_LOG_SIZE 1
struct rds_log_t {
	struct rds_rx_t rds_log[RDS_LOG_SIZE];
	signed int log_len[RDS_LOG_SIZE];
	unsigned int size;
	unsigned int in;
	unsigned int out;
	unsigned int len;
	signed int (*log_in)(struct rds_log_t *thiz, struct rds_rx_t *new_log, signed int new_len);
	signed int (*log_out)(struct rds_log_t *thiz, struct rds_rx_t *dst, signed int *dst_len);
};

extern signed int rds_parser(struct rds_t *rds_dst, struct rds_rx_t *rds_raw,
				signed int rds_size, unsigned short(*getfreq) (void));
extern signed int rds_grp_counter_get(struct rds_group_cnt_t *dst, struct rds_group_cnt_t *src);
extern signed int rds_grp_counter_reset(struct rds_group_cnt_t *gc);
extern signed int rds_log_in(struct rds_log_t *thiz, struct rds_rx_t *new_log, signed int new_len);
extern signed int rds_log_out(struct rds_log_t *thiz, struct rds_rx_t *dst, signed int *dst_len);

#define DEFINE_RDSLOG(name) \
	struct rds_log_t name = { \
		.size = RDS_LOG_SIZE, \
		.in = 0, \
		.out = 0, \
		.len = 0, \
		.log_in = rds_log_in, \
		.log_out = rds_log_out, \
	}

#endif /* __FM_RDS_H__ */
