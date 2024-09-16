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

#include <linux/string.h>

#include "fm_typedef.h"
#include "fm_rds.h"
#include "fm_dbg.h"
#include "fm_err.h"
#include "fm_stdlib.h"

/* static enum rds_ps_state_machine_t ps_state_machine = RDS_PS_START; */
/* static enum rds_rt_state_machine_t rt_state_machine = RDS_RT_START; */
struct fm_state_machine {
	signed int state;
	signed int (*state_get)(struct fm_state_machine *thiz);
	signed int (*state_set)(struct fm_state_machine *thiz, signed int new_state);
};

static signed int fm_state_get(struct fm_state_machine *thiz)
{
	return thiz->state;
}

static signed int fm_state_set(struct fm_state_machine *thiz, signed int new_state)
{
	return thiz->state = new_state;
}

#define STATE_SET(a, s)        \
{                           \
	if ((a)->state_set) {          \
		(a)->state_set((a), (s));    \
	}                       \
}

#define STATE_GET(a)         \
({                             \
	signed int __ret = 0;              \
	if ((a)->state_get) {          \
		__ret = (a)->state_get((a));    \
	}                       \
	__ret;                  \
})

static unsigned short (*rds_get_freq)(void);

/* RDS spec related handle flow */
/*
 * rds_cnt_get
 * To get rds group count form raw data
 * If success return 0, else return error code
*/
static signed int rds_cnt_get(struct rds_rx_t *rds_raw, signed int raw_size, signed int *cnt)
{
	signed int gap = sizeof(rds_raw->cos) + sizeof(rds_raw->sin);

	if (rds_raw == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (cnt == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	*cnt = (raw_size - gap) / sizeof(struct rds_packet_t);
	WCN_DBG(FM_INF | RDSC, "group cnt=%d\n", *cnt);

	return 0;
}

/*
 * rds_grp_get
 * To get rds group[n] data form raw data with index
 * If success return 0, else return error code
*/
static signed int rds_grp_get(unsigned short *dst, struct rds_rx_t *raw, signed int idx)
{
	if (dst == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (raw == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (idx > (MAX_RDS_RX_GROUP_CNT - 1) || idx < 0)
		return -FM_EPARA;

	dst[0] = raw->data[idx].blkA;
	dst[1] = raw->data[idx].blkB;
	dst[2] = raw->data[idx].blkC;
	dst[3] = raw->data[idx].blkD;
	dst[4] = raw->data[idx].crc;
	dst[5] = raw->data[idx].cbc;

	WCN_DBG(FM_NTC | RDSC, "BLOCK:%04x %04x %04x %04x, CRC:%04x CBC:%04x\n", dst[0], dst[1],
		dst[2], dst[3], dst[4], dst[5]);

	return 0;
}

/*
 * rds_checksum_check
 * To check CRC rerult, if OK, *valid=true, else *valid=false
 * If success return 0, else return error code
*/
static signed int rds_checksum_check(unsigned short crc, signed int mask, bool *valid)
{
	if (valid == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if ((crc & mask) == mask)
		*valid = true;
	else
		*valid = false;

	return 0;
}

/*
 * rds_event_set
 * To set rds event, and user space can use this flag to juge which event happened
 * If success return 0, else return error code
*/
static signed int rds_event_set(unsigned short *events, signed int event_mask)
{
	if (events == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	WCN_DBG(FM_INF | RDSC, "rds set event[0x%04x->0x%04x]\n", event_mask, *events);
	*events |= event_mask;

	return 0;
}

/*
 * rds_flag_set
 * To set rds event flag, and user space can use this flag to juge which event happened
 * If success return 0, else return error code
*/
static signed int rds_flag_set(unsigned int *flags, signed int flag_mask)
{
	if (flags == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	WCN_DBG(FM_INF | RDSC, "rds set flag[0x%04x->0x%04x]\n", flag_mask, *flags);
	*flags |= flag_mask;

	return 0;
}

/*
 * rds_grp_type_get
 * To get rds group type form blockB
 * If success return 0, else return error code
*/
static signed int rds_grp_type_get(unsigned short crc, unsigned short blk, unsigned char *type, unsigned char *subtype)
{
	bool valid = false;

	if (type == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (subtype == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	/* to get the group type from block B */
	rds_checksum_check(crc, FM_RDS_GDBK_IND_B, &valid);

	if (valid == true) {
		*type = (blk & 0xF000) >> 12;	/* Group type(4bits) */
		*subtype = (blk & 0x0800) >> 11;	/* version code(1bit), 0=vesionA, 1=versionB */
	} else {
		WCN_DBG(FM_WAR | RDSC, "Block1 CRC err\n");
		return -FM_ECRC;
	}

	WCN_DBG(FM_DBG | RDSC, "Type=%d, subtype:%s\n", (signed int) *type, *subtype ? "version B" : "version A");
	return 0;
}

/*
 * rds_grp_counter_add
 * @type -- group type, rang: 0~15
 * @subtype -- sub group type, rang:0~1
 *
 * add group counter, g0a~g15b
 * we use type value as the index
 * If success return 0, else return error code
*/
static signed int rds_grp_counter_add(unsigned char type, unsigned char subtype, struct rds_group_cnt_t *gc)
{
	if (gc == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (type > 15)
		return -FM_EPARA;

	switch (subtype) {
	case RDS_GRP_VER_A:
		gc->groupA[type]++;
		break;
	case RDS_GRP_VER_B:
		gc->groupB[type]++;
		break;
	default:
		return -FM_EPARA;
	}

	gc->total++;
	WCN_DBG(FM_INF | RDSC, "group counter:%d\n", (signed int) gc->total);
	return 0;
}

/*
 * rds_grp_counter_get
 *
 * read group counter , g0a~g15b
 * If success return 0, else return error code
*/
extern signed int rds_grp_counter_get(struct rds_group_cnt_t *dst, struct rds_group_cnt_t *src)
{
	if (dst == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (src == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	fm_memcpy(dst, src, sizeof(struct rds_group_cnt_t));
	WCN_DBG(FM_DBG | RDSC, "rds gc get[total=%d]\n", (signed int) dst->total);
	return 0;
}

/*
 * rds_grp_counter_reset
 *
 * clear group counter to 0, g0a~g15b
 * If success return 0, else return error code
*/
extern signed int rds_grp_counter_reset(struct rds_group_cnt_t *gc)
{
	if (gc == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	fm_memset(gc, 0, sizeof(struct rds_group_cnt_t));
	return 0;
}

extern signed int rds_log_in(struct rds_log_t *thiz, struct rds_rx_t *new_log, signed int new_len)
{
	if (new_log == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	new_len = (new_len < sizeof(struct rds_rx_t)) ? new_len : sizeof(struct rds_rx_t);
	fm_memcpy(&(thiz->rds_log[thiz->in]), new_log, new_len);
	thiz->log_len[thiz->in] = new_len;
	thiz->in = (thiz->in + 1) % thiz->size;
	thiz->len++;
	thiz->len = (thiz->len >= thiz->size) ? thiz->size : thiz->len;
	WCN_DBG(FM_DBG | RDSC, "add a new log[len=%d]\n", thiz->len);

	return 0;
}

extern signed int rds_log_out(struct rds_log_t *thiz, struct rds_rx_t *dst, signed int *dst_len)
{
	if (dst == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (dst_len == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (thiz->len > 0) {
		*dst_len = thiz->log_len[thiz->out];
		*dst_len = (*dst_len < sizeof(struct rds_rx_t)) ? *dst_len : sizeof(struct rds_rx_t);
		fm_memcpy(dst, &(thiz->rds_log[thiz->out]), *dst_len);
		thiz->out = (thiz->out + 1) % thiz->size;
		thiz->len--;
		WCN_DBG(FM_DBG | RDSC, "del a new log[len=%d]\n", thiz->len);
	} else {
		*dst_len = 0;
		WCN_DBG(FM_WAR | RDSC, "rds log buf is empty\n");
	}

	return 0;
}

/*
 * rds_grp_pi_get
 * To get rds group pi code form blockA
 * If success return 0, else return error code
*/
static signed int rds_grp_pi_get(unsigned short crc, unsigned short blk, unsigned short *pi, bool *dirty)
{
	signed int ret = 0;
	bool valid = false;

	if (pi == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (dirty == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	/* to get the group pi code from block A */
	ret = rds_checksum_check(crc, FM_RDS_GDBK_IND_A, &valid);

	if (valid == true) {
		if (*pi != blk) {
			/* PI=program Identication */
			*pi = blk;
			*dirty = true;	/* yes, we got new PI code */
		} else {
			*dirty = false;	/* PI is the same as last one */
		}
	} else {
		WCN_DBG(FM_WAR | RDSC, "Block0 CRC err\n");
		return -FM_ECRC;
	}

	WCN_DBG(FM_INF | RDSC, "PI=0x%04x, %s\n", *pi, *dirty ? "new" : "old");
	return ret;
}

/*
 * rds_grp_pty_get
 * To get rds group pty code form blockB
 * If success return 0, else return error code
*/
static signed int rds_grp_pty_get(unsigned short crc, unsigned short blk, unsigned char *pty, bool *dirty)
{
	signed int ret = 0;
/* bool valid = false; */

	if (pty == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (dirty == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	/* to get PTY code from block B */
/* ret = rds_checksum_check(crc, FM_RDS_GDBK_IND_B, &valid); */

/* if (valid == false) { */
/* WCN_DBG(FM_WAR | RDSC, "Block1 CRC err\n"); */
/* return -FM_ECRC; */
/* } */

	if (*pty != ((blk & 0x03E0) >> 5)) {
		/* PTY=Program Type Code */
		*pty = (blk & 0x03E0) >> 5;
		*dirty = true;	/* yes, we got new PTY code */
	} else {
		*dirty = false;	/* PTY is the same as last one */
	}

	WCN_DBG(FM_INF | RDSC, "PTY=%d, %s\n", (signed int) *pty, *dirty ? "new" : "old");
	return ret;
}

/*
 * rds_grp_tp_get
 * To get rds group tp code form blockB
 * If success return 0, else return error code
*/
static signed int rds_grp_tp_get(unsigned short crc, unsigned short blk, unsigned char *tp, bool *dirty)
{
	signed int ret = 0;
/* bool valid = false; */

	if (tp == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (dirty == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	/* to get TP code from block B */
/* ret = rds_checksum_check(crc, FM_RDS_GDBK_IND_B, &valid); */

/* if (valid == false) { */
/* WCN_DBG(FM_WAR | RDSC, "Block1 CRC err\n"); */
/* return -FM_ECRC; */
/* } */

	if (*tp != ((blk & 0x0400) >> 10)) {
		/* Tranfic Program Identification */
		*tp = (blk & 0x0400) >> 10;
		*dirty = true;	/* yes, we got new TP code */
	} else {
		*dirty = false;	/* TP is the same as last one */
	}

	/* WCN_DBG(FM_INF | RDSC, "TP=%d, %s\n", (signed int) *tp, *dirty ? "new" : "old"); */
	return ret;
}

/*
 * rds_g0_ta_get
 * To get rds group ta code form blockB
 * If success return 0, else return error code
*/
static signed int rds_g0_ta_get(unsigned short blk, unsigned char *ta, bool *dirty)
{
	signed int ret = 0;

	if (ta == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (dirty == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	/* TA=Traffic Announcement code */
	if (*ta != ((blk & 0x0010) >> 4)) {
		*ta = (blk & 0x0010) >> 4;
		*dirty = true;	/* yes, we got new TA code */
	} else {
		*dirty = false;	/* TA is the same as last one */
	}

	WCN_DBG(FM_INF | RDSC, "TA=%d, %s\n", (signed int) *ta, *dirty ? "new" : "old");
	return ret;
}

/*
 * rds_g0_music_get
 * To get music-speech switch code form blockB
 * If success return 0, else return error code
*/
static signed int rds_g0_music_get(unsigned short blk, unsigned char *music, bool *dirty)
{
	signed int ret = 0;

	if (music == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (dirty == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	/* M/S=music speech switch code */
	if (*music != ((blk & 0x0008) >> 3)) {
		*music = (blk & 0x0008) >> 3;
		*dirty = true;	/* yes, we got new music code */
	} else {
		*dirty = false;	/* music  is the same as last one */
	}

	WCN_DBG(FM_INF | RDSC, "Music=%d, %s\n", (signed int) *music, *dirty ? "new" : "old");
	return ret;
}

/*
 * rds_g0_ps_addr_get
 * To get ps addr form blockB, blkB b0~b1
 * If success return 0, else return error code
*/
static signed int rds_g0_ps_addr_get(unsigned short blkB, unsigned char *addr)
{
	if (addr == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	*addr = (unsigned char) blkB & 0x03;

	WCN_DBG(FM_INF | RDSC, "addr=0x%02x\n", *addr);
	return 0;
}

/*
 * rds_g0_di_flag_get
 * To get DI segment flag form blockB, blkB b2
 * If success return 0, else return error code
*/
static signed int rds_g0_di_flag_get(unsigned short blkB, unsigned char *flag)
{
	if (flag == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	*flag = (unsigned char) ((blkB & 0x0004) >> 2);

	WCN_DBG(FM_INF | RDSC, "flag=0x%02x\n", *flag);
	return 0;
}

static signed int rds_g0_ps_get(unsigned short crc, unsigned short blkD, unsigned char addr, unsigned char *buf)
{
/* bool valid = false; */
	signed int idx = 0;

	if (buf == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	/* ps segment addr rang 0~3 */
	if (addr > 0x03) {
		WCN_DBG(FM_ERR | RDSC, "addr invalid(0x%02x)\n", addr);
		return -FM_EPARA;
	}

	idx = 2 * addr;
	buf[idx] = blkD >> 8;
	buf[idx + 1] = blkD & 0xFF;
#if 0
	rds_checksum_check(crc, FM_RDS_GDBK_IND_D, &valid);

	if (valid == true) {
		buf[idx] = blkD >> 8;
		buf[idx + 1] = blkD & 0xFF;
	} else {
		WCN_DBG(FM_ERR | RDSC, "ps crc check err\n");
		return -FM_ECRC;
	}
#endif

	WCN_DBG(FM_INF | RDSC, "PS:addr[%02x]:0x%02x 0x%02x\n", addr, buf[idx], buf[idx + 1]);
	return 0;
}

/*
 * rds_g0_ps_cmp
 * this function is the most importent flow for PS parsing
 * 1.Compare fresh buf with once buf per byte, if eque copy this byte to twice buf, else copy it to once buf
 * 2.Check whether we got a full segment
 * If success return 0, else return error code
*/
static signed int rds_g0_ps_cmp(unsigned char addr, unsigned short cbc, unsigned char *fresh,
			    unsigned char *once, unsigned char *twice, /*bool *valid, */ unsigned char *bm)
{
	signed int ret = 0, indx;
	/* signed int i = 0; */
	/* signed int j = 0; */
	/* signed int cnt = 0; */
	unsigned char AF_H, AF_L, PS_Num;
	/* unsigned char corrBitCnt_BlkB, corrBitCnt_BlkD; */
	static signed char Pre_PS_Num = -1;

	if (fresh == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (once == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (twice == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (addr > 3) {		/* ps limited in 8 chars */
		WCN_DBG(FM_NTC | RDSC, "PS Address error, addr=%x\n", addr);
		return -1;
	}

	/* j = 2; // PS segment width */
	PS_Num = addr;
	/* corrBitCnt_BlkB = rds_cbc_get(cbc, RDS_BLK_B); */
	/* corrBitCnt_BlkD = rds_cbc_get(cbc, RDS_BLK_D); */

	AF_H = once[2 * PS_Num];
	AF_L = once[2 * PS_Num + 1];
	if ((fresh[2 * PS_Num] == AF_H) && (fresh[2 * PS_Num + 1] == AF_L)) {
		twice[2 * PS_Num] = once[2 * PS_Num];
		twice[2 * PS_Num + 1] = once[2 * PS_Num + 1];
		*bm |= 1 << PS_Num;
	} else {
		if (PS_Num - Pre_PS_Num > 1) {
			for (indx = Pre_PS_Num + 1; indx < PS_Num; indx++) {
				*bm &= ~(1 << indx);
				once[2 * indx] = 0x00;
				once[2 * indx + 1] = 0x00;
				twice[2 * indx] = 0x00;
				twice[2 * indx + 1] = 0x00;
			}
		} else if (PS_Num - Pre_PS_Num < 1) {
			for (indx = 0; indx < PS_Num; indx++) {
				*bm &= ~(1 << indx);
				once[2 * indx] = 0x00;
				once[2 * indx + 1] = 0x00;
				twice[2 * indx] = 0x00;
				twice[2 * indx + 1] = 0x00;
			}
		}

		if ((once[2 * PS_Num] != 0) || (once[2 * PS_Num + 1] != 0)) {
			for (indx = PS_Num; indx < 4; indx++)
				*bm &= ~(1 << indx);
		}
		/* if((corrBitCnt_BlkB == 0) && (corrBitCnt_BlkD == 0)) */
		/* ALPS00523685:6627 CBC sometime is unreliable */
#ifdef RDS_CBC_DEPENDENCY
		if (cbc == 0) {
			*bm |= 1 << PS_Num;
			once[2 * PS_Num] = fresh[2 * PS_Num];
			once[2 * PS_Num + 1] = fresh[2 * PS_Num + 1];
			twice[2 * PS_Num] = fresh[2 * PS_Num];
			twice[2 * PS_Num + 1] = fresh[2 * PS_Num + 1];
		} else
#endif
		{
			once[2 * PS_Num] = fresh[2 * PS_Num];
			once[2 * PS_Num + 1] = fresh[2 * PS_Num + 1];
		}
	}

	Pre_PS_Num = PS_Num;
#if	0
	if (rds_cbc_get(cbc, RDS_BLK_D) == 0) {
		once[j * addr] = fresh[j * addr];
		once[j * addr + 1] = fresh[j * addr + 1];
	}
	if ((once[j * addr] == fresh[j * addr]) && (once[j * addr + 1] == fresh[j * addr + 1])) {
		twice[j * addr] = once[j * addr];
		twice[j * addr + 1] = once[j * addr + 1];
		*valid = true;
	} else {
		once[j * addr] = fresh[j * addr];
		once[j * addr + 1] = fresh[j * addr + 1];
		*valid = false;
	}
#endif
#if	0
	for (i = 0; i < j; i++) {
		if (fresh[j * addr + i] == once[j * addr + i]) {
			twice[j * addr + i] = once[j * addr + i];	/* get the same byte 2 times */
			cnt++;
		} else {
			once[j * addr + i] = fresh[j * addr + i];	/* use new val */
		}
	}

	/* check if we got a valid segment */
	if (cnt == j)
		*valid = true;
	else
		*valid = false;
#endif
	/* WCN_DBG(FM_NTC | RDSC, "PS seg=%s\n", *valid == true ? "true" : "false"); */
	WCN_DBG(FM_INF | RDSC, "bitmap=%x\n", *bm);
	WCN_DBG(FM_INF | RDSC, "PS[%02x][1][2]=%x %x|%x %x|%x %x|%x %x|%x %x|%x %x|%x %x|%x %x\n",
		addr, once[0], twice[0], once[1], twice[1], once[2], twice[2], once[3], twice[3],
		once[4], twice[4], once[5], twice[5], once[6], twice[6], once[7], twice[7]);
	return ret;
}

struct rds_bitmap {
	unsigned short bm;
	signed int cnt;
	signed int max_addr;
	unsigned short (*bm_get)(struct rds_bitmap *thiz);
	signed int (*bm_cnt_get)(struct rds_bitmap *thiz);
	signed int (*bm_get_pos)(struct rds_bitmap *thiz);
	signed int (*bm_clr)(struct rds_bitmap *thiz);
	signed int (*bm_cmp)(struct rds_bitmap *thiz, struct rds_bitmap *that);
	signed int (*bm_set)(struct rds_bitmap *thiz, unsigned char addr);
};

static unsigned short rds_bm_get(struct rds_bitmap *thiz)
{
	return thiz->bm;
}

static signed int rds_bm_cnt_get(struct rds_bitmap *thiz)
{
	return thiz->cnt;
}

#define FM_RDS_USE_SOLUTION_B

static signed int rds_bm_get_pos(struct rds_bitmap *thiz)
{
	signed int i = thiz->max_addr;
	signed int j;

	j = 0;

	while ((i > -1) && !(thiz->bm & (1 << i)))
		i--;

#ifdef FM_RDS_USE_SOLUTION_B
	for (j = i; j >= 0; j--) {
		if (!(thiz->bm & (1 << j))) {
			WCN_DBG(FM_NTC | RDSC, "uncomplete msg 0x%04x, delete it\n", thiz->bm);
			return -1;
		}
	}
#endif

	return i;
}

static signed int rds_bm_clr(struct rds_bitmap *thiz)
{
	thiz->bm = 0x0000;
	thiz->cnt = 0;
	return 0;
}

static signed int rds_bm_cmp(struct rds_bitmap *bitmap1, struct rds_bitmap *bitmap2)
{
	return (signed int) (bitmap1->bm - bitmap2->bm);
}

static signed int rds_bm_set(struct rds_bitmap *thiz, unsigned char addr)
{
	struct rds_bitmap bm_old;

	/* text segment addr rang */
	if (addr > thiz->max_addr) {
		WCN_DBG(FM_ERR | RDSC, "addr invalid(0x%02x)\n", addr);
		return -FM_EPARA;
	}

	bm_old.bm = thiz->bm;
	thiz->bm |= (1 << addr);	/* set bitmap */

	if (!rds_bm_cmp(&bm_old, thiz))
		thiz->cnt++;	/* multi get a segment */
	else if (thiz->cnt > 0)
		thiz->cnt--;

	return 0;
}

/*
 * rds_g2_rt_addr_get
 * To get rt addr form blockB
 * If success return 0, else return error code
*/
static signed int rds_g2_rt_addr_get(unsigned short blkB, unsigned char *addr)
{
	signed int ret = 0;

	if (addr == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	*addr = (unsigned char) blkB & 0x0F;

	WCN_DBG(FM_INF | RDSC, "addr=0x%02x\n", *addr);
	return ret;
}

static signed int rds_g2_txtAB_get(unsigned short blk, unsigned char *txtAB, bool *dirty)
{
	signed int ret = 0;
	static bool once_dirty; /* false */

	if (txtAB == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (dirty == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	*dirty = false;
	if (*txtAB != ((blk & 0x0010) >> 4)) {
		if (once_dirty) {
			*txtAB = (blk & 0x0010) >> 4;
			*dirty = true;	/* yes, we got new txtAB code */
			once_dirty = false;
			WCN_DBG(FM_NTC | RDSC, "changed! txtAB=%d\n", *txtAB);
			return ret;
		}
		once_dirty = true;
	} else {
		once_dirty = false;	/* txtAB is the same as last one */
	}

	WCN_DBG(FM_INF | RDSC, "txtAB=%d, %s\n", *txtAB, *dirty ? "new" : "old");
	return ret;
}

static signed int rds_g2_rt_get(unsigned short crc, unsigned char subtype, unsigned short blkC, unsigned short blkD,
									unsigned char addr, unsigned char *buf)
{
	signed int ret = 0;
	bool valid = false;
	signed int idx = 0;

	if (buf == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	/* text segment addr rang 0~15 */
	if (addr > 0x0F) {
		WCN_DBG(FM_ERR | RDSC, "addr invalid(0x%02x)\n", addr);
		ret = -FM_EPARA;
		return ret;
	}

	switch (subtype) {
	case RDS_GRP_VER_A:
		idx = 4 * addr;
		ret = rds_checksum_check(crc, FM_RDS_GDBK_IND_C | FM_RDS_GDBK_IND_D, &valid);

		if (valid == true) {
			buf[idx] = blkC >> 8;
			buf[idx + 1] = blkC & 0xFF;
			buf[idx + 2] = blkD >> 8;
			buf[idx + 3] = blkD & 0xFF;
		} else {
			WCN_DBG(FM_ERR | RDSC, "rt crc check err\n");
			ret = -FM_ECRC;
		}

		break;
	case RDS_GRP_VER_B:
		idx = 2 * addr;
		ret = rds_checksum_check(crc, FM_RDS_GDBK_IND_D, &valid);

		if (valid == true) {
			buf[idx] = blkD >> 8;
			buf[idx + 1] = blkD & 0xFF;
		} else {
			WCN_DBG(FM_ERR | RDSC, "rt crc check err\n");
			ret = -FM_ECRC;
		}

		break;
	default:
		break;
	}

	WCN_DBG(FM_NTC | RDSC, "fresh addr[%02x]:0x%02x%02x 0x%02x%02x\n", addr, buf[idx],
		buf[idx + 1], buf[idx + 2], buf[idx + 3]);
	return ret;
}

static signed int rds_g2_rt_get_len(unsigned char subtype, signed int pos, signed int *len)
{
	signed int ret = 0;

	if (len == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (subtype == RDS_GRP_VER_A)
		*len = 4 * (pos + 1);
	else
		*len = 2 * (pos + 1);

	return ret;
}

/*
 * rds_g2_rt_cmp
 * this function is the most importent flow for RT parsing
 * 1.Compare fresh buf with once buf per byte, if eque copy this byte to twice buf, else copy it to once buf
 * 2.Check whether we got a full segment, for typeA if copyed 4bytes to twice buf, for typeB 2bytes copyed to twice buf
 * 3.Check whether we got the end of RT, if we got 0x0D
 * 4.If we got the end, then caculate the RT length
 * If success return 0, else return error code
*/
static signed int rds_g2_rt_cmp(unsigned char addr, unsigned short cbc, unsigned char subtype, unsigned char *fresh,
			    unsigned char *once, unsigned char *twice, bool *valid)
{
	signed int ret = 0;
	signed int i = 0;
	signed int j = 0;
	signed int cnt = 0;

	if (fresh == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (once == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (twice == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (valid == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	j = (subtype == RDS_GRP_VER_A) ? 4 : 2;	/* RT segment width */

	if (subtype == RDS_GRP_VER_A) {
		/* if (rds_cbc_get(cbc, RDS_BLK_C) == 0) */
#ifdef RDS_CBC_DEPENDENCY
		if (cbc == 0) {
#endif
			once[j * addr + 0] = fresh[j * addr + 0];
			once[j * addr + 1] = fresh[j * addr + 1];
			once[j * addr + 2] = fresh[j * addr + 2];
			once[j * addr + 3] = fresh[j * addr + 3];
#ifdef RDS_CBC_DEPENDENCY
		}
#endif
	} else if (subtype == RDS_GRP_VER_B) {
#ifdef RDS_CBC_DEPENDENCY
		if (cbc == 0) {
#endif
			once[j * addr + 0] = fresh[j * addr + 0];
			once[j * addr + 1] = fresh[j * addr + 1];
#ifdef RDS_CBC_DEPENDENCY
		}
#endif
	}
#ifdef RDS_CBC_DEPENDENCY
	for (i = 0; i < j; i++) {
		if (fresh[j * addr + i] == once[j * addr + i]) {
			twice[j * addr + i] = once[j * addr + i];	/* get the same byte 2 times */
			cnt++;
			/* WCN_DBG(FM_NTC | RDSC, "twice=%d\n", j * addr + i); */
		} else {
			once[j * addr + i] = fresh[j * addr + i];	/* use new val */
			/* WCN_DBG(FM_NTC | RDSC, "once=%d\n", j * addr + i); */
		}
	}
#else
	for (i = 0; i < j; i++) {
		if (twice[j * addr + i] == once[j * addr + i]) {
			cnt++;
			/* WCN_DBG(FM_NTC | RDSC, "twice=%d\n", j * addr + i); */
		} else {
			twice[j * addr + i] = once[j * addr + i];
			/* WCN_DBG(FM_NTC | RDSC, "once=%d\n", j * addr + i); */
		}
	}
#endif

	/* check if we got a valid segment 4bytes for typeA, 2bytes for typeB */
	if (cnt == j)
		*valid = true;
	else
		*valid = false;

	WCN_DBG(FM_INF | RDSC, "RT seg=%s\n", *valid == true ? "true" : "false");
/* WCN_DBG(FM_INF | RDSC, "RT end=%s\n", *end == true ? "true" : "false"); */
/* WCN_DBG(FM_INF | RDSC, "RT len=%d\n", *len); */
	return ret;
}

/*
 * rds_g2_rt_check_end
 * check 0x0D end flag
 * If we got the end, then caculate the RT length
 * If success return 0, else return error code
*/
static signed int rds_g2_rt_check_end(unsigned char addr, unsigned char subtype, unsigned char *twice, bool *end)
{
	signed int i = 0;
	signed int j = 0;

	if (twice == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (end == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	j = (subtype == RDS_GRP_VER_A) ? 4 : 2;	/* RT segment width */
	*end = false;

	for (i = 0; i < j; i++) {
		/* if we got 0x0D twice, it means a RT end */
		if (twice[j * addr + i] == 0x0D) {
			*end = true;
			WCN_DBG(FM_NTC | RDSC, "get 0x0D\n");
			break;
		}
	}

	return 0;
}

static signed int rds_retrieve_g0_af(unsigned short *block_data, unsigned char SubType, struct rds_t *pstRDSData)
{
	static signed short preAF_Num;
	unsigned char indx, indx2, AF_H, AF_L, num;
	signed short temp_H, temp_L;
	signed int ret = 0;
	bool valid = false;
	bool dirty = false;
	unsigned short *event = &pstRDSData->event_status;
	unsigned int *flag = &pstRDSData->RDSFlag.flag_status;

/* ret = rds_checksum_check(block_data[4], FM_RDS_GDBK_IND_D, &valid); */

/* if (valid == false) { */
/* WCN_DBG(FM_WAR | RDSC, "Group0 BlockD crc err\n"); */
/* return -FM_ECRC; */
/* } */

	ret = rds_g0_ta_get(block_data[1], &pstRDSData->RDSFlag.TA, &dirty);

	if (ret) {
		WCN_DBG(FM_WAR | RDSC, "get ta failed[ret=%d]\n", ret);
	} else if (dirty == true) {
		ret = rds_event_set(event, RDS_EVENT_FLAGS);	/* yes, we got new TA code */
		ret = rds_flag_set(flag, RDS_FLAG_IS_TA);
	}

	ret = rds_g0_music_get(block_data[1], &pstRDSData->RDSFlag.Music, &dirty);

	if (ret) {
		WCN_DBG(FM_WAR | RDSC, "get music failed[ret=%d]\n", ret);
	} else if (dirty == true) {
		ret = rds_event_set(event, RDS_EVENT_FLAGS);	/* yes, we got new MUSIC code */
		ret = rds_flag_set(flag, RDS_FLAG_IS_MUSIC);
	}

	if ((pstRDSData->Switch_TP) && (pstRDSData->RDSFlag.TP) && !(pstRDSData->RDSFlag.TA))
		ret = rds_event_set(event, RDS_EVENT_TAON_OFF);

	if (SubType)	/* Type B no AF information */
		goto out;

	/* Type A */

	ret = rds_checksum_check(block_data[4], FM_RDS_GDBK_IND_C, &valid);

	if (valid == false) {
		WCN_DBG(FM_WAR | RDSC, "Group0 BlockC crc err\n");
		return -FM_ECRC;
	}

	AF_H = (block_data[2] & 0xFF00) >> 8;
	AF_L = block_data[2] & 0x00FF;

	if ((AF_H > 224) && (AF_H < 250)) {
		/* Followed AF Number, see RDS spec Table 11, valid(224-249) */
		WCN_DBG(FM_INF | RDSC, "RetrieveGroup0 AF_H:%d, AF_L:%d\n", AF_H, AF_L);
		preAF_Num = AF_H - 224;	/* AF Number */

		if (preAF_Num != pstRDSData->AF_Data.AF_Num) {
			pstRDSData->AF_Data.AF_Num = preAF_Num;
			pstRDSData->AF_Data.isAFNum_Get = 0;
		} else {
			/* Get the same AFNum two times */
			pstRDSData->AF_Data.isAFNum_Get = 1;
		}

		if ((AF_L < 205) && (AF_L > 0)) {
			/* See RDS Spec table 10, valid VHF */
			pstRDSData->AF_Data.AF[0][0] = AF_L + 875;	/* convert to 100KHz */
			pstRDSData->AF_Data.AF[0][0] *= 10;
			WCN_DBG(FM_NTC | RDSC, "RetrieveGroup0 AF[0][0]:%d\n",
				pstRDSData->AF_Data.AF[0][0]);

			if ((pstRDSData->AF_Data.AF[0][0]) != (pstRDSData->AF_Data.AF[1][0])) {
				pstRDSData->AF_Data.AF[1][0] = pstRDSData->AF_Data.AF[0][0];
			} else {
				if (pstRDSData->AF_Data.AF[1][0] != rds_get_freq())
					pstRDSData->AF_Data.isMethod_A = 1;
				else
					pstRDSData->AF_Data.isMethod_A = 0;
			}

			WCN_DBG(FM_NTC | RDSC,
				"RetrieveGroup0 isAFNum_Get:%d, isMethod_A:%d\n",
				pstRDSData->AF_Data.isAFNum_Get, pstRDSData->AF_Data.isMethod_A);

			/* only one AF handle */
			if ((pstRDSData->AF_Data.isAFNum_Get)
			    && (pstRDSData->AF_Data.AF_Num == 1)) {
				pstRDSData->AF_Data.Addr_Cnt = 0xFF;
				pstRDSData->event_status |= RDS_EVENT_AF_LIST;
				WCN_DBG(FM_NTC | RDSC, "RetrieveGroup0 RDS_EVENT_AF_LIST update\n");
			}
		}
	} else if ((pstRDSData->AF_Data.isAFNum_Get)
		   && (pstRDSData->AF_Data.Addr_Cnt != 0xFF)) {
		/* AF Num correct */
		num = pstRDSData->AF_Data.AF_Num;
		num = (num > 25) ? 25 : num;
		num = num >> 1;
		WCN_DBG(FM_INF | RDSC, "RetrieveGroup0 +num:%d\n", num);

		/* Put AF freq into buffer and check if AF freq is repeat again */
		for (indx = 1; indx < (num + 1); indx++) {
			if ((AF_H == (pstRDSData->AF_Data.AF[0][2 * indx - 1] / 10 - 875))
			    && (AF_L == (pstRDSData->AF_Data.AF[0][2 * indx] / 10 - 875))) {
				WCN_DBG(FM_NTC | RDSC,
					"RetrieveGroup0 +num:%d AF same as indx:%d\n", num, indx);
				break;
			} else if (!(pstRDSData->AF_Data.AF[0][2 * indx - 1])) {
				/* null buffer */
				/* convert to 100KHz */
				pstRDSData->AF_Data.AF[0][2 * indx - 1] = AF_H + 875;
				pstRDSData->AF_Data.AF[0][2 * indx] = AF_L + 875;

				pstRDSData->AF_Data.AF[0][2 * indx - 1] *= 10;
				pstRDSData->AF_Data.AF[0][2 * indx] *= 10;

				WCN_DBG(FM_NTC | RDSC,
					"RetrieveGroup0 +num:%d AF[0][%d]:%d, AF[0][%d]:%d\n",
					num, 2 * indx - 1,
					pstRDSData->AF_Data.AF[0][2 * indx - 1],
					2 * indx, pstRDSData->AF_Data.AF[0][2 * indx]);
				break;
			}
		}

		num = pstRDSData->AF_Data.AF_Num;
		num = (num > 25) ? 25 : num;
		WCN_DBG(FM_NTC | RDSC, "RetrieveGroup0 ++num:%d\n", num);

		if (num <= 0)
			goto out;

		if ((pstRDSData->AF_Data.AF[0][num - 1]) == 0)
			goto out;

		num = num >> 1;
		WCN_DBG(FM_NTC | RDSC, "RetrieveGroup0 +++num:%d\n", num);

		/* arrange frequency from low to high:start */
		for (indx = 1; indx < num; indx++) {
			for (indx2 = indx + 1; indx2 < (num + 1); indx2++) {
				temp_H = pstRDSData->AF_Data.AF[0][2 * indx - 1];
				temp_L = pstRDSData->AF_Data.AF[0][2 * indx];

				if (temp_H > (pstRDSData->AF_Data.AF[0][2 * indx2 - 1])) {
					pstRDSData->AF_Data.AF[0][2 * indx - 1] =
					    pstRDSData->AF_Data.AF[0][2 * indx2 - 1];
					pstRDSData->AF_Data.AF[0][2 * indx] =
					    pstRDSData->AF_Data.AF[0][2 * indx2];
					pstRDSData->AF_Data.AF[0][2 * indx2 - 1] = temp_H;
					pstRDSData->AF_Data.AF[0][2 * indx2] = temp_L;
				} else if (temp_H == (pstRDSData->AF_Data.AF[0][2 * indx2 - 1])) {
					if (temp_L > (pstRDSData->AF_Data.AF[0][2 * indx2])) {
						pstRDSData->AF_Data.AF[0][2 * indx - 1] =
						    pstRDSData->AF_Data.AF[0][2 * indx2 - 1];
						pstRDSData->AF_Data.AF[0][2 * indx] =
						    pstRDSData->AF_Data.AF[0][2 * indx2];
						pstRDSData->AF_Data.AF[0][2 * indx2 - 1] = temp_H;
						pstRDSData->AF_Data.AF[0][2 * indx2] = temp_L;
					}
				}
			}
		}

		/* arrange frequency from low to high:end */
		/* compare AF buff0 and buff1 data:start */
		num = pstRDSData->AF_Data.AF_Num;
		num = (num > 25) ? 25 : num;
		indx2 = 0;

		for (indx = 0; indx < num; indx++) {
			if ((pstRDSData->AF_Data.AF[1][indx]) == (pstRDSData->AF_Data.AF[0][indx])) {
				if (pstRDSData->AF_Data.AF[1][indx] != 0)
					indx2++;
			} else
				pstRDSData->AF_Data.AF[1][indx] = pstRDSData->AF_Data.AF[0][indx];
		}

		WCN_DBG(FM_NTC | RDSC, "RetrieveGroup0 indx2:%d, num:%d\n", indx2, num);

		/* compare AF buff0 and buff1 data:end */
		if (indx2 == num) {
			pstRDSData->AF_Data.Addr_Cnt = 0xFF;
			pstRDSData->event_status |= RDS_EVENT_AF_LIST;
			WCN_DBG(FM_NTC | RDSC,
				"RetrieveGroup0 AF_Num:%d\n",
				pstRDSData->AF_Data.AF_Num);

			for (indx = 0; indx < num; indx++) {
				if ((pstRDSData->AF_Data.AF[1][indx]) == 0) {
					pstRDSData->AF_Data.Addr_Cnt = 0x0F;
					pstRDSData->event_status &= (~RDS_EVENT_AF_LIST);
				}
			}
		} else
			pstRDSData->AF_Data.Addr_Cnt = 0x0F;
	}

out:	return ret;
}

static signed int rds_retrieve_g0_di(unsigned short *block_data, unsigned char SubType, struct rds_t *pstRDSData)
{
	unsigned char DI_Code, DI_Flag;
	signed int ret = 0;
/* bool valid = false; */

	unsigned short *event = &pstRDSData->event_status;
	unsigned int *flag = &pstRDSData->RDSFlag.flag_status;

	/* parsing Program service name segment (in BlockD) */
/* ret = rds_checksum_check(block_data[4], FM_RDS_GDBK_IND_D, &valid); */

/* if (valid == false) { */
/* WCN_DBG(FM_WAR | RDSC, "Group0 BlockD crc err\n"); */
/* return -FM_ECRC; */
/* } */

	rds_g0_ps_addr_get(block_data[1], &DI_Code);
	rds_g0_di_flag_get(block_data[1], &DI_Flag);

	switch (DI_Code) {
	case 3:

		if (pstRDSData->RDSFlag.Stereo != DI_Flag) {
			pstRDSData->RDSFlag.Stereo = DI_Flag;
			ret = rds_event_set(event, RDS_EVENT_FLAGS);
			ret = rds_flag_set(flag, RDS_FLAG_IS_STEREO);
		}

		break;
	case 2:

		if (pstRDSData->RDSFlag.Artificial_Head != DI_Flag) {
			pstRDSData->RDSFlag.Artificial_Head = DI_Flag;
			ret = rds_event_set(event, RDS_EVENT_FLAGS);
			ret = rds_flag_set(flag, RDS_FLAG_IS_ARTIFICIAL_HEAD);
		}

		break;
	case 1:

		if (pstRDSData->RDSFlag.Compressed != DI_Flag) {
			pstRDSData->RDSFlag.Compressed = DI_Flag;
			ret = rds_event_set(event, RDS_EVENT_FLAGS);
			ret = rds_flag_set(flag, RDS_FLAG_IS_COMPRESSED);
		}

		break;
	case 0:

		if (pstRDSData->RDSFlag.Dynamic_PTY != DI_Flag) {
			pstRDSData->RDSFlag.Dynamic_PTY = DI_Flag;
			ret = rds_event_set(event, RDS_EVENT_FLAGS);
			ret = rds_flag_set(flag, RDS_FLAG_IS_DYNAMIC_PTY);
		}

		break;
	default:
		break;
	}

	return ret;
}

static signed int rds_retrieve_g0_ps(unsigned short *block_data, unsigned char SubType, struct rds_t *pstRDSData)
{
	unsigned char ps_addr;
	signed int ret = 0, i, num;
	bool valid = false;
/* signed int pos = 0; */
	static struct fm_state_machine ps_sm = {
		.state = RDS_PS_START,
		.state_get = fm_state_get,
		.state_set = fm_state_set,
	};
#if 0
	static struct rds_bitmap ps_bm = {
		.bm = 0,
		.cnt = 0,
		.max_addr = 0x03,
		.bm_get = rds_bm_get,
		.bm_cnt_get = rds_bm_cnt_get,
		.bm_set = rds_bm_set,
		.bm_get_pos = rds_bm_get_pos,
		.bm_clr = rds_bm_clr,
		.bm_cmp = rds_bm_cmp,
	};
#endif
	unsigned short *event = &pstRDSData->event_status;

	/* parsing Program service name segment (in BlockD) */
	ret = rds_checksum_check(block_data[4], FM_RDS_GDBK_IND_D, &valid);

	if (valid == false) {
		WCN_DBG(FM_WAR | RDSC, "Group0 BlockD crc err\n");
		return -FM_ECRC;
	}

	rds_g0_ps_addr_get(block_data[1], &ps_addr);

	/* PS parsing state machine run */
	while (1) {
		switch (STATE_GET(&ps_sm)) {
		case RDS_PS_START:
			if (rds_g0_ps_get(block_data[4], block_data[3], ps_addr, pstRDSData->PS_Data.PS[0])) {
				STATE_SET(&ps_sm, RDS_PS_FINISH);	/* if CRC error, we should not do parsing */
				break;
			}

			rds_g0_ps_cmp(ps_addr, block_data[5], pstRDSData->PS_Data.PS[0],
				      pstRDSData->PS_Data.PS[1], pstRDSData->PS_Data.PS[2],
				      /*&valid, */ &pstRDSData->PS_Data.Addr_Cnt);

			/* if (valid == true) { */
			/* ps_bm.bm_set(&ps_bm, ps_addr); */
			/* } */

			STATE_SET(&ps_sm, RDS_PS_DECISION);
			break;
		case RDS_PS_DECISION:

			if (pstRDSData->PS_Data.Addr_Cnt == 0x000F) {	/* get max  8 chars */
				STATE_SET(&ps_sm, RDS_PS_GETLEN);
			} else {
				STATE_SET(&ps_sm, RDS_PS_FINISH);
			}

			break;
		case RDS_PS_GETLEN:
		{
			num = 0;
			WCN_DBG(FM_INF | RDSC, "PS[3]=%x %x %x %x %x %x %x %x\n",
				pstRDSData->PS_Data.PS[3][0],
				pstRDSData->PS_Data.PS[3][1],
				pstRDSData->PS_Data.PS[3][2],
				pstRDSData->PS_Data.PS[3][3],
				pstRDSData->PS_Data.PS[3][4],
				pstRDSData->PS_Data.PS[3][5],
				pstRDSData->PS_Data.PS[3][6], pstRDSData->PS_Data.PS[3][7]);
			for (i = 0; i < 8; i++) {	/* compare with last PS. */
				if (pstRDSData->PS_Data.PS[3][i] == pstRDSData->PS_Data.PS[2][i])
					num++;
			}
			if (num != 8) {
				num = 0;
				for (i = 0; i < 8; i++) {
					/* even ps=0x20 and bitmap=0xF, send event to host to cover last ps. */
					if (pstRDSData->PS_Data.PS[2][i] == 0x0)
						num++;
				}
				if (num != 8) {
					fm_memcpy(pstRDSData->PS_Data.PS[3], pstRDSData->PS_Data.PS[2], 8);
					rds_event_set(event, RDS_EVENT_PROGRAMNAME);
					WCN_DBG(FM_NTC | RDSC, "Yes, get an PS!\n");
				} else {
					/* clear bitmap */
					pstRDSData->PS_Data.Addr_Cnt = 0;
				}
			} else {
				/* if px3==ps2,clear bitmap */
				pstRDSData->PS_Data.Addr_Cnt = 0;
				/* clear buf */
				fm_memset(pstRDSData->PS_Data.PS[0], 0x00, 8);
				fm_memset(pstRDSData->PS_Data.PS[1], 0x00, 8);
				fm_memset(pstRDSData->PS_Data.PS[2], 0x00, 8);
			}
		}
#if 0
			ps_bm.bm_clr(&ps_bm);
			/* clear buf */
			fm_memset(pstRDSData->PS_Data.PS[0], 0x20, 8);
			fm_memset(pstRDSData->PS_Data.PS[1], 0x20, 8);
			fm_memset(pstRDSData->PS_Data.PS[2], 0x20, 8);
#endif
			STATE_SET(&ps_sm, RDS_PS_FINISH);
			break;
		case RDS_PS_FINISH:
			STATE_SET(&ps_sm, RDS_PS_START);
			goto out;
		default:
			break;
		}
	}

out:
	return ret;
}

static signed int rds_retrieve_g0(unsigned short *block_data, unsigned char SubType, struct rds_t *pstRDSData)
{
	signed int ret = 0;

	ret = rds_retrieve_g0_af(block_data, SubType, pstRDSData);

	if (ret)
		return ret;

	ret = rds_retrieve_g0_di(block_data, SubType, pstRDSData);

	if (ret)
		return ret;

	ret = rds_retrieve_g0_ps(block_data, SubType, pstRDSData);

	if (ret)
		return ret;

	return ret;
}

static signed int rds_ecc_get(unsigned short blk, unsigned char *ecc, bool *dirty)
{
	signed int ret = 0;

	if (ecc == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (dirty == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (*ecc != (blk & 0xFF)) {
		*ecc = (unsigned char)blk & 0xFF;
		*dirty = true;	/* yes, we got new ecc code */
	} else {
		*dirty = false;	/* ecc is the same as last one */
	}

	WCN_DBG(FM_NTC | RDSC, "ecc=%02x, %s\n", *ecc, *dirty ? "new" : "old");
	return ret;
}

static signed int rds_retrieve_g1(unsigned short *block_data, unsigned char SubType, struct rds_t *pstRDSData)
{
	unsigned char variant_code = (block_data[2] & 0x7000) >> 12;
	signed int ret = 0;
	bool dirty = false;

	if (variant_code == 0) {
		ret = rds_ecc_get(block_data[2], &pstRDSData->Extend_Country_Code, &dirty);
			if (!ret) {
				if (dirty == true)
					rds_event_set(&pstRDSData->event_status, RDS_EVENT_ECC_CODE);
			} else
				WCN_DBG(FM_ERR | RDSC, "get ecc fail(%d)\n", ret);
			WCN_DBG(FM_DBG | RDSC, "Extend_Country_Code:%d\n", pstRDSData->Extend_Country_Code);
	} else if (variant_code == 3) {
		pstRDSData->Language_Code = block_data[2] & 0xFFF;
		WCN_DBG(FM_DBG | RDSC, "Language_Code:%d\n", pstRDSData->Language_Code);
	}

	pstRDSData->Radio_Page_Code = block_data[1] & 0x001F;
	pstRDSData->Program_Item_Number_Code = block_data[3];

	return ret;
}

static signed int rds_retrieve_g2(unsigned short *source, unsigned char subtype, struct rds_t *target)
{
	signed int ret = 0;
	unsigned short crc, cbc;
	unsigned short blkA, blkB, blkC, blkD;
	unsigned char *fresh, *once, *twice, *display;
	unsigned short *event;
	unsigned int *flag;
	unsigned short i = 0;
	static struct fm_state_machine rt_sm = {
		.state = RDS_RT_START,
		.state_get = fm_state_get,
		.state_set = fm_state_set,
	};
	static struct rds_bitmap rt_bm = {
		.bm = 0,
		.cnt = 0,
		.max_addr = 0xF,
		.bm_get = rds_bm_get,
		.bm_cnt_get = rds_bm_cnt_get,
		.bm_set = rds_bm_set,
		.bm_get_pos = rds_bm_get_pos,
		.bm_clr = rds_bm_clr,
		.bm_cmp = rds_bm_cmp,
	};
	unsigned char rt_addr = 0;
	bool txtAB_change = false;	/* text AB flag 0 --> 1 or 1-->0 meas new RT incoming */
	bool txt_end = false;	/* 0x0D means text end */
	bool seg_ok = 0;
	signed int pos = 0;
	signed int rt_len = 0, indx = 0, invalid_cnt = 0;
	signed int bufsize = 0;

	if (source == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (target == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	/* source */
	blkA = source[0];
	blkB = source[1];
	blkC = source[2];
	blkD = source[3];
	crc = source[4];
	cbc = source[5];
	/* target */
	fresh = target->RT_Data.TextData[0];
	once = target->RT_Data.TextData[1];
	twice = target->RT_Data.TextData[2];
	display = target->RT_Data.TextData[3];
	event = &target->event_status;
	flag = &target->RDSFlag.flag_status;
	bufsize = sizeof(target->RT_Data.TextData[0]);
	rt_bm.bm = target->RT_Data.Addr_Cnt;

	/* get basic info: addr, txtAB */
	if (rds_g2_rt_addr_get(blkB, &rt_addr))
		return ret;

	if (rds_g2_txtAB_get(blkB, &target->RDSFlag.Text_AB, &txtAB_change))
		return ret;
	if (txtAB_change == true) {
		/* clear buf */
		fm_memset(fresh, 0x20, bufsize);
		fm_memset(once, 0x20, bufsize);
		fm_memset(twice, 0x20, bufsize);
		rt_bm.bm_clr(&rt_bm);
	}
	/* RT parsing state machine run */
	while (1) {
		switch (STATE_GET(&rt_sm)) {
		case RDS_RT_START:
		{
#if 0
			if (txtAB_change == true)
				STATE_SET(&rt_sm, RDS_RT_DECISION);
			else
#endif
			{
				if (rds_g2_rt_get(crc, subtype, blkC, blkD, rt_addr, fresh) == 0) {
					rds_g2_rt_cmp(rt_addr, cbc, subtype, fresh, once, twice, &seg_ok);

					if (seg_ok == true)
						rt_bm.bm_set(&rt_bm, rt_addr);
					else
						rt_bm.bm &= ~(1 << rt_addr);
				}
				WCN_DBG(FM_NTC | RDSC, "bitmap=0x%04x, bmcnt=%d\n", rt_bm.bm, rt_bm.cnt);
				rds_g2_rt_check_end(rt_addr, subtype, twice, &txt_end);

				STATE_SET(&rt_sm, RDS_RT_DECISION);
			}
			break;
		}
		case RDS_RT_DECISION:
		{
			if ((txt_end == true) || (rt_bm.bm_get(&rt_bm) == 0xFFFF) /* get max  64 chars */
			    || (rt_bm.bm_cnt_get(&rt_bm) > RDS_RT_MULTI_REV_TH)) {
				/* repeate many times, but no end char get */
				pos = rt_bm.bm_get_pos(&rt_bm);
				rds_g2_rt_get_len(subtype, pos, &rt_len);

				if (pos == -1) {
					STATE_SET(&rt_sm, RDS_RT_FINISH);
				} else {
					if (rt_addr == pos) {
						STATE_SET(&rt_sm, RDS_RT_GETLEN);
					} else if (pos > rt_addr) {
						rt_bm.bm &= ~(1 << (rt_addr + 1));
						STATE_SET(&rt_sm, RDS_RT_FINISH);
					} else
						STATE_SET(&rt_sm, RDS_RT_FINISH);
				}

				if (txt_end == true) {
					for (i = rt_addr + 1; i < rt_bm.max_addr; i++)
						rt_bm.bm &= ~(1 << i);
				}
			} else {
				STATE_SET(&rt_sm, RDS_RT_FINISH);
			}

			break;
		}
		case RDS_RT_GETLEN:
			if (rt_len > 0) {
				for (indx = 0; indx < rt_len; indx++) {
					if (twice[indx] == 0x20)
						invalid_cnt++;
				}
				if (invalid_cnt != rt_len) {
					if (memcmp(display, twice, bufsize) != 0) {
						fm_memcpy(display, twice, bufsize);
						target->RT_Data.TextLength = rt_len;
						rds_event_set(event, RDS_EVENT_LAST_RADIOTEXT);
						/* yes we got a new RT */
						WCN_DBG(FM_NTC | RDSC, "Yes, get an RT! [len=%d]\n", rt_len);
					}
					rt_bm.bm_clr(&rt_bm);
					/* clear buf */
					fm_memset(fresh, 0x20, bufsize);
					fm_memset(once, 0x20, bufsize);
					fm_memset(twice, 0x20, bufsize);
				} else
					WCN_DBG(FM_NTC | RDSC, "Get 0x20 RT %d\n", invalid_cnt);
			}
#if 0
			if (txtAB_change == true) {
				txtAB_change = false;
				/* we need get new RT after show the old RT to the display */
				STATE_SET(&rt_sm, RDS_RT_START);
			} else
#endif
			{
				STATE_SET(&rt_sm, RDS_RT_FINISH);
			}
			break;
		case RDS_RT_FINISH:
			STATE_SET(&rt_sm, RDS_RT_START);
			goto out;
		default:
			break;
		}
	}

out:
	target->RT_Data.Addr_Cnt = rt_bm.bm;
	return ret;
}

static signed int rds_retrieve_g4(unsigned short *block_data, unsigned char SubType, struct rds_t *pstRDSData)
{
	unsigned short year, month, k = 0, D2, minute;
	unsigned int MJD, D1;
	signed int ret = 0;

	WCN_DBG(FM_DBG | RDSC, "RetrieveGroup4 %d\n", SubType);

	if (!SubType) {
		/* Type A */
		if ((block_data[4] & FM_RDS_GDBK_IND_C) && (block_data[4] & FM_RDS_GDBK_IND_D)) {
			MJD = (unsigned int) (((block_data[1] & 0x0003) << 15) + ((block_data[2] & 0xFFFE) >> 1));
			year = (MJD * 100 - 1507820) / 36525;

			if (year > 1000) {
				WCN_DBG(FM_DBG | RDSC, "Abnormal year: %d.\n", year);
				return ret;
			}

			month = (MJD * 10000 - 149561000 - 3652500 * year) / 306001;

			if ((month == 14) || (month == 15))
				k = 1;

			D1 = (unsigned int) ((36525 * year) / 100);
			D2 = (unsigned short) ((306001 * month) / 10000);
			pstRDSData->CT.Year = 1900 + year + k;
			pstRDSData->CT.Month = month - 1 - k * 12;
			pstRDSData->CT.Day = (unsigned short) (MJD - 14956 - D1 - D2);
			pstRDSData->CT.Hour = ((block_data[2] & 0x0001) << 4) + ((block_data[3] & 0xF000) >> 12);
			minute = (block_data[3] & 0x0FC0) >> 6;

			if (block_data[3] & 0x0020)
				pstRDSData->CT.Local_Time_offset_signbit = 1;	/* 0=+, 1=- */

			pstRDSData->CT.Local_Time_offset_half_hour = block_data[3] & 0x001F;

			if (pstRDSData->CT.Minute != minute) {
				pstRDSData->CT.Minute = (block_data[3] & 0x0FC0) >> 6;
				pstRDSData->event_status |= RDS_EVENT_UTCDATETIME;
			}
		}
	}

	return ret;
}

static signed int rds_retrieve_g14(unsigned short *block_data, unsigned char SubType, struct rds_t *pstRDSData)
{
	static signed short preAFON_Num;
	unsigned char TP_ON, TA_ON, PI_ON, AF_H, AF_L, indx, indx2, num;
	unsigned short PS_Num = 0;
	signed int ret = 0;

	WCN_DBG(FM_DBG | RDSC, "RetrieveGroup14 %d\n", SubType);
	/* SubType = (*(block_data+1)&0x0800)>>11; */
	PI_ON = block_data[3];
	TP_ON = block_data[1] & 0x0010;

	if ((!SubType) && (block_data[4] & FM_RDS_GDBK_IND_C)) {
		/* Type A */
		PS_Num = block_data[1] & 0x000F; /* variant code */

		if (PS_Num >= 0 && PS_Num < 4) { /* variant code = 0~3 represent PS */
			for (indx = 0; indx < 2; indx++) {
				pstRDSData->PS_ON[2 * PS_Num] = block_data[2] >> 8;
				pstRDSData->PS_ON[2 * PS_Num + 1] = block_data[2] & 0xFF;
			}

			goto out;
		} else if (PS_Num < 0 || PS_Num > 4)	/* variant code > 4 */
			goto out;

		/* variant code = 4 represent AF(ON) */

		AF_H = (block_data[2] & 0xFF00) >> 8;
		AF_L = block_data[2] & 0x00FF;

		if ((AF_H > 224) && (AF_H < 250)) {
			/* Followed AF Number */
			pstRDSData->AFON_Data.isAFNum_Get = 0;
			preAFON_Num = AF_H - 224;

			if (pstRDSData->AFON_Data.AF_Num != preAFON_Num)
				pstRDSData->AFON_Data.AF_Num = preAFON_Num;
			else
				pstRDSData->AFON_Data.isAFNum_Get = 1;

			if (AF_L < 205) {
				pstRDSData->AFON_Data.AF[0][0] = AF_L + 875;

				if ((pstRDSData->AFON_Data.AF[0][0]) != (pstRDSData->AFON_Data.AF[1][0]))
					pstRDSData->AFON_Data.AF[1][0] = pstRDSData->AFON_Data.AF[0][0];
				else
					pstRDSData->AFON_Data.isMethod_A = 1;
			}

			goto out;
		}

		if (!(pstRDSData->AFON_Data.isAFNum_Get) || ((pstRDSData->AFON_Data.Addr_Cnt) == 0xFF))
			goto out;

		/* AF Num correct */
		num = pstRDSData->AFON_Data.AF_Num;
		num = (num > 25) ? 25 : num;
		num = num >> 1;

		/* Put AF freq into buffer and check if AF freq is repeat again */
		for (indx = 1; indx < (num + 1); indx++) {
			if ((AF_H == (pstRDSData->AFON_Data.AF[0][2 * indx - 1]))
			    && (AF_L == (pstRDSData->AFON_Data.AF[0][2 * indx]))) {
				WCN_DBG(FM_NTC | RDSC, "RetrieveGroup14 AFON same as indx:%d\n", indx);
				break;
			} else if (!(pstRDSData->AFON_Data.AF[0][2 * indx - 1])) {
				/* null buffer */
				pstRDSData->AFON_Data.AF[0][2 * indx - 1] = AF_H + 875;
				pstRDSData->AFON_Data.AF[0][2 * indx] = AF_L + 875;
				break;
			}
		}

		num = pstRDSData->AFON_Data.AF_Num;
		num = (num > 25) ? 25 : num;
		if (num <= 0)
			goto out;

		if ((pstRDSData->AFON_Data.AF[0][num - 1]) == 0)
			goto out;

		num = num >> 1;
		/* arrange frequency from low to high:start */
		for (indx = 1; indx < num; indx++) {
			for (indx2 = indx + 1; indx2 < (num + 1); indx2++) {
				AF_H = pstRDSData->AFON_Data.AF[0][2 * indx - 1];
				AF_L = pstRDSData->AFON_Data.AF[0][2 * indx];

				if (AF_H > (pstRDSData->AFON_Data.AF[0][2 * indx2 - 1])) {
					pstRDSData->AFON_Data.AF[0][2 * indx - 1] =
					    pstRDSData->AFON_Data.AF[0][2 * indx2 - 1];
					pstRDSData->AFON_Data.AF[0][2 * indx] =
					    pstRDSData->AFON_Data.AF[0][2 * indx2];
					pstRDSData->AFON_Data.AF[0][2 * indx2 - 1] =
					    AF_H;
					pstRDSData->AFON_Data.AF[0][2 * indx2] = AF_L;
				} else if (AF_H == (pstRDSData->AFON_Data.AF[0][2 * indx2 - 1])) {
					if (AF_L > (pstRDSData->AFON_Data.AF[0][2 * indx2])) {
						pstRDSData->AFON_Data.AF[0][2 * indx - 1] =
						    pstRDSData->AFON_Data.AF[0][2 * indx2 - 1];
						pstRDSData->AFON_Data.AF[0][2 * indx] =
						    pstRDSData->AFON_Data.AF[0][2 * indx2];
						pstRDSData->AFON_Data.AF[0][2 * indx2 - 1] = AF_H;
						pstRDSData->AFON_Data.AF[0][2 * indx2] = AF_L;
					}
				}
			}
		}

		/* arrange frequency from low to high:end */
		/* compare AF buff0 and buff1 data:start */
		num = pstRDSData->AFON_Data.AF_Num;
		num = (num > 25) ? 25 : num;
		indx2 = 0;

		for (indx = 0; indx < num; indx++) {
			if ((pstRDSData->AFON_Data.AF[1][indx]) == (pstRDSData->AFON_Data.AF[0][indx])) {
				if (pstRDSData->AFON_Data.AF[1][indx] != 0)
					indx2++;
			} else
				pstRDSData->AFON_Data.AF[1][indx] = pstRDSData->AFON_Data.AF[0][indx];
		}

		/* compare AF buff0 and buff1 data:end */
		if (indx2 == num) {
			pstRDSData->AFON_Data.Addr_Cnt = 0xFF;
			pstRDSData->event_status |= RDS_EVENT_AFON_LIST;

			for (indx = 0; indx < num; indx++) {
				if ((pstRDSData->AFON_Data.AF[1][indx]) == 0) {
					pstRDSData->AFON_Data.Addr_Cnt = 0x0F;
					pstRDSData->event_status &= (~RDS_EVENT_AFON_LIST);
				}
			}
		} else
			pstRDSData->AFON_Data.Addr_Cnt = 0x0F;
	} else {
		/* Type B */
		TA_ON = block_data[1] & 0x0008;
		WCN_DBG(FM_DBG | RDSC,
			"TA g14 typeB pstRDSData->RDSFlag.TP=%d pstRDSData->RDSFlag.TA=%d TP_ON=%d TA_ON=%d\n",
			pstRDSData->RDSFlag.TP, pstRDSData->RDSFlag.TA, TP_ON, TA_ON);

		if ((!pstRDSData->RDSFlag.TP) && (pstRDSData->RDSFlag.TA) && TP_ON && TA_ON) {
			signed int TA_num = 0;

			for (num = 0; num < 25; num++) {
				if (pstRDSData->AFON_Data.AF[1][num] != 0)
					TA_num++;
				else
					break;
			}

			WCN_DBG(FM_NTC | RDSC, "TA set RDS_EVENT_TAON");

			if (TA_num == pstRDSData->AFON_Data.AF_Num)
				pstRDSData->event_status |= RDS_EVENT_TAON;
		}
	}

out:	return ret;
}

/*
 *  rds_parser
 *  Block0:	PI code(16bits)
 *  Block1:	Group type(4bits), B0=version code(1bit), TP=traffic program code(1bit),
 *  PTY=program type code(5bits), other(5bits)
 *  Block2:	16bits
 *  Block3:	16bits
 *  @rds_dst - target buffer that record RDS parsing result
 *  @rds_raw - rds raw data
 *  @rds_size - size of rds raw data
 *  @getfreq - function pointer, AF need get current freq
 */
signed int rds_parser(struct rds_t *rds_dst, struct rds_rx_t *rds_raw,
						signed int rds_size, unsigned short(*getfreq) (void))
{
	signed int ret = 0;
	/* block_data[0] = blockA,   block_data[1] = blockB, block_data[2] = blockC,   block_data[3] = blockD, */
	/* block_data[4] = CRC,      block_data[5] = CBC */
	unsigned short block_data[6];
	unsigned char GroupType, SubType = 0;
	signed int rds_cnt = 0;
	signed int i = 0;
	bool dirty = false;
	/* target buf to fill the result in */
	unsigned short *event = &rds_dst->event_status;
	unsigned int *flag = &rds_dst->RDSFlag.flag_status;

	if (getfreq == NULL) {
		WCN_DBG(FM_ERR | RDSC, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	rds_get_freq = getfreq;

	ret = rds_cnt_get(rds_raw, rds_size, &rds_cnt);

	if (ret) {
		WCN_DBG(FM_WAR | RDSC, "get cnt err[ret=%d]\n", ret);
		return ret;
	}

	while (rds_cnt > 0) {
		ret = rds_grp_get(&block_data[0], rds_raw, i);

		if (ret) {
			WCN_DBG(FM_WAR | RDSC, "get group err[ret=%d]\n", ret);
			goto do_next;
		}

		ret = rds_grp_type_get(block_data[4], block_data[1], &GroupType, &SubType);

		if (ret) {
			WCN_DBG(FM_WAR | RDSC, "get group type err[ret=%d]\n", ret);
			goto do_next;
		}

		ret = rds_grp_counter_add(GroupType, SubType, &rds_dst->gc);

		ret = rds_grp_pi_get(block_data[4], block_data[0], &rds_dst->PI, &dirty);

		if (ret) {
			WCN_DBG(FM_WAR | RDSC, "get group pi err[ret=%d]\n", ret);
			goto do_next;
		} else if (dirty == false) {
			WCN_DBG(FM_INF | RDSC, "dirty = %d, update PI event\n", dirty);
			ret = rds_event_set(event, RDS_EVENT_PI_CODE);	/* yes, we got same PI, can be trust */
		}

		ret = rds_grp_pty_get(block_data[4], block_data[1], &rds_dst->PTY, &dirty);

		if (ret) {
			WCN_DBG(FM_WAR | RDSC, "get group pty err[ret=%d]\n", ret);
			goto do_next;
		} else if (dirty == true) {
			ret = rds_event_set(event, RDS_EVENT_PTY_CODE);	/* yes, we got new PTY code */
		}

		ret = rds_grp_tp_get(block_data[4], block_data[1], &rds_dst->RDSFlag.TP, &dirty);

		if (ret) {
			WCN_DBG(FM_WAR | RDSC, "get group tp err[ret=%d]\n", ret);
			goto do_next;
		} else if (dirty == true) {
			ret = rds_event_set(event, RDS_EVENT_FLAGS);	/* yes, we got new TP code */
			ret = rds_flag_set(flag, RDS_FLAG_IS_TP);
		}

		switch (GroupType) {
		case 0:
			ret = rds_retrieve_g0(&block_data[0], SubType, rds_dst);
			if (ret)
				goto do_next;

			break;
		case 1:
			ret = rds_retrieve_g1(&block_data[0], SubType, rds_dst);
			if (ret)
				goto do_next;

			break;
		case 2:
			ret = rds_retrieve_g2(&block_data[0], SubType, rds_dst);
			if (ret)
				goto do_next;

			break;
		case 4:
			ret = rds_retrieve_g4(&block_data[0], SubType, rds_dst);
			if (ret)
				goto do_next;

			break;
		case 14:
			ret = rds_retrieve_g14(&block_data[0], SubType, rds_dst);
			if (ret)
				goto do_next;

			break;
		default:
			break;
		}

do_next:

		if (ret && (ret != -FM_ECRC)) {
			WCN_DBG(FM_ERR | RDSC, "parsing err[ret=%d]\n", ret);
			return ret;
		}

		rds_cnt--;
		i++;
	}

	return ret;
}
