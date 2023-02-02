/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __SSPM_IPI_DEFINE_H__
#define __SSPM_IPI_DEFINE_H__


/* definition of slot size for received PINs */
#define PINR_SIZE_PLATFORM       3  /* the following will use mbox 2 */
#define PINR_SIZE_CPU_DVFS       4
#define PINR_SIZE_QOS            4
#define PINR_SIZE_TST1           4
/* definition of slot offset for PINs */
#define PINR_OFFSET_PLATFORM     0  /* the following will use mbox 2 */
#define PINR_OFFSET_CPU_DVFS     (PINR_OFFSET_PLATFORM + PINR_SIZE_PLATFORM)
#define PINR_OFFSET_QOS          (PINR_OFFSET_CPU_DVFS + PINR_SIZE_CPU_DVFS)
#define PINR_OFFSET_TST1         (PINR_OFFSET_QOS + PINR_SIZE_QOS)
#define PINR_MBOX2_USED          (PINR_OFFSET_TST1 + PINR_SIZE_TST1)
#if (PINR_MBOX2_USED > IPI_MBOX2_SLOTS)
#error "MBOX2 cannot hold all pin definitions"
#endif

/* mutex_send, sema_ack, mbox, slot, size,
 * shared, retdata, lock, share_grp, polling, unused
 */
struct _pin_send send_pintable[] = {
	{{{0} }, {0}, 0, 0, 0,
	 0, 0, 0, 0, 0, 0},
	{{{0} }, {0}, 0, 0, 0,
	 0, 0, 0, 0, 0, 0},
	{{{0} }, {0}, 0, 0, 0,
	 0, 0, 0, 0, 0, 0},
	{{{0} }, {0}, 0, 0,  0,
	 0, 0, 0, 0, 0, 0},
	{{{0} }, {0}, 0, 0, 0,
	 0, 0, 0, 0, 0, 0},
	/*====================================================================*/
	{{{0} }, {0}, 0, 0, 0,
	 1, 0, 0, 0, 0, 0},
	{{{0} }, {0}, 0, 0, 0,
	 1, 0, 0, 0, 0, 0},
	{{{0} }, {0}, 0, 0, 0,
	 0, 0, 0, 0, 0, 0},
	{{{0} }, {0}, 0, 0,  0,
	 0, 0, 0, 0, 0, 0},
	{{{0} }, {0}, 0, 0, 0,
	 0, 0, 0, 0, 0, 0},
	{{{0} }, {0}, 0, 0,  0,
	 0, 0, 0, 0, 0, 0},
	{{{0} }, {0}, 0, 0,  0,
	 0, 0, 0, 0, 0, 0},
 /*====================================================================*/
};
#define TOTAL_SEND_PIN	\
		(sizeof(send_pintable)/sizeof(struct _pin_send))


/* act, mbox, slot, size, shared, retdata, lock, share_grp, unused */
struct _pin_recv recv_pintable[] = {
	{NULL, 2, PINR_OFFSET_PLATFORM, PINR_SIZE_PLATFORM, 0, 1, 0, 0, 0},
	{NULL, 2, PINR_OFFSET_CPU_DVFS, PINR_SIZE_CPU_DVFS, 0, 1, 0, 0, 0},
	{NULL, 2, PINR_OFFSET_QOS, PINR_SIZE_QOS, 0, 1, 0, 0, 0},
	{NULL, 2, PINR_OFFSET_TST1, PINR_SIZE_TST1, 0, 1, 0, 0, 0},
};
#define TOTAL_RECV_PIN	\
		(sizeof(recv_pintable)/sizeof(struct _pin_recv))

/* info for all mbox: start, end, used_slot, mode, unused */
struct _mbox_info mbox_table[IPI_MBOX_TOTAL] = {
	{0, 4, 0, 2, 0},  /* mbox 0 for send */
	{5, 10, 0, 2, 0},  /* mbox 1 for send */
	{0, 3, 0, 1, 0},  /* mbox 2 for recv */
	{0, 0, 0, 0, 0}, /* mbox 3 */
};

const char *pin_name[IPI_ID_TOTAL] = { };

#endif /* __SSPM_IPI_DEFINE_H__ */
