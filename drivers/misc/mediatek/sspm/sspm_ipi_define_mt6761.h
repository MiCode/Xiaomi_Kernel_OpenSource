/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __SSPM_IPI_DEFINE_H__
#define __SSPM_IPI_DEFINE_H__

/* definition of slot size for send PINs */
#define PINS_SIZE_PLATFORM       3  /* the following will use mbox 0 */
#define PINS_SIZE_CPU_DVFS       4
#define PINS_SIZE_QOS            4
#define PINS_SIZE_TST1           4
#define PINS_SIZE_FHCTL          9
/* ============================================================ */
#define PINS_SIZE_MCDI           2  /* the following will use mbox 1 */
#define PINS_SIZE_SPM_SUSPEND    8
#define PINS_SIZE_PMIC           5
#define PINS_SIZE_PPM            7
#define PINS_SIZE_THERMAL        4
#define PINS_SIZE_UPOWER         4
#define PINS_SIZE_CM             2
/* ============================================================ */

/* definition of slot offset for PINs */
#define PINS_OFFSET_PLATFORM     0  /* the following will use mbox 0 */
#define PINS_OFFSET_CPU_DVFS     (PINS_OFFSET_PLATFORM + PINS_SIZE_PLATFORM)
#define PINS_OFFSET_QOS          (PINS_OFFSET_CPU_DVFS + PINS_SIZE_CPU_DVFS)
#define PINS_OFFSET_TST1         (PINS_OFFSET_QOS + PINS_SIZE_QOS)
#define PINS_OFFSET_FHCTL        (PINS_OFFSET_TST1 + PINS_SIZE_TST1)
#define PINS_MBOX0_USED          (PINS_OFFSET_FHCTL + PINS_SIZE_FHCTL)
#if (PINS_MBOX0_USED > IPI_MBOX0_SLOTS)
#error "MBOX0 cannot hold all pin definitions"
#endif
/* ============================================================ */
#define PINS_OFFSET_MCDI         0  /* the following will use mbox 1 */
#define PINS_OFFSET_SPM_SUSPEND  (PINS_OFFSET_MCDI + PINS_SIZE_MCDI)
#define PINS_OFFSET_PMIC         (PINS_OFFSET_SPM_SUSPEND + \
				PINS_SIZE_SPM_SUSPEND)
#define PINS_OFFSET_PPM          (PINS_OFFSET_PMIC + PINS_SIZE_PMIC)
#define PINS_OFFSET_THERMAL      (PINS_OFFSET_PPM + PINS_SIZE_PPM)
#define PINS_OFFSET_UPOWER       (PINS_OFFSET_THERMAL + PINS_SIZE_THERMAL)
#define PINS_OFFSET_CM           (PINS_OFFSET_UPOWER + PINS_SIZE_UPOWER)
#define PINS_MBOX1_USED          (PINS_OFFSET_CM + PINS_SIZE_CM)
#if (PINS_MBOX1_USED > IPI_MBOX1_SLOTS)
#error "MBOX1 cannot hold all pin definitions"
#endif
/* ============================================================ */

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
struct _pin_send mt6761_send_pintable[] = {
	{{{0} }, {0}, 0, PINS_OFFSET_PLATFORM, PINS_SIZE_PLATFORM,
	 0, 1, 0, 0, 0, 0},
	{{{0} }, {0}, 0, PINS_OFFSET_CPU_DVFS, PINS_SIZE_CPU_DVFS,
	 0, 1, 1, 1, 0, 0},
	{{{0} }, {0}, 0, PINS_OFFSET_QOS, PINS_SIZE_QOS,
	 0, 1, 1, 1, 0, 0},
	{{{0} }, {0}, 0, PINS_OFFSET_TST1,  PINS_SIZE_TST1,
	 0, 1, 0, 0, 0, 0},
	{{{0} }, {0}, 0, PINS_OFFSET_FHCTL, PINS_SIZE_FHCTL,
	 0, 1, 1, 1, 0, 0},
	/*====================================================================*/
	{{{0} }, {0}, 1, PINS_OFFSET_MCDI, PINS_SIZE_MCDI,
	 1, 1, 1, 1, 0, 0},
	{{{0} }, {0}, 1, PINS_OFFSET_SPM_SUSPEND, PINS_SIZE_SPM_SUSPEND,
	 1, 1, 1, 1, 0, 0},
	{{{0} }, {0}, 1, PINS_OFFSET_PMIC, PINS_SIZE_PMIC,
	 0, 1, 1, 1, 0, 0},
	{{{0} }, {0}, 1, PINS_OFFSET_PPM,  PINS_SIZE_PPM,
	 0, 1, 1, 1, 0, 0},
	{{{0} }, {0}, 1, PINS_OFFSET_THERMAL, PINS_SIZE_THERMAL,
	 0, 1, 0, 0, 0, 0},
	{{{0} }, {0}, 1, PINS_OFFSET_UPOWER,  PINS_SIZE_UPOWER,
	 0, 1, 1, 1, 0, 0},
	{{{0} }, {0}, 1, PINS_OFFSET_CM,  PINS_SIZE_CM,
	 0, 1, 1, 1, 0, 0},
 /*====================================================================*/
};
#define MT6761_TOTAL_SEND_PIN	\
		(sizeof(mt6761_send_pintable)/sizeof(struct _pin_send))


/* act, mbox, slot, size, shared, retdata, lock, share_grp, unused */
struct _pin_recv mt6761_recv_pintable[] = {
	{NULL, 2, PINR_OFFSET_PLATFORM, PINR_SIZE_PLATFORM, 0, 1, 0, 0, 0},
	{NULL, 2, PINR_OFFSET_CPU_DVFS, PINR_SIZE_CPU_DVFS, 0, 1, 0, 0, 0},
	{NULL, 2, PINR_OFFSET_QOS, PINR_SIZE_QOS, 0, 1, 0, 0, 0},
	{NULL, 2, PINR_OFFSET_TST1, PINR_SIZE_TST1, 0, 1, 0, 0, 0},
};
#define MT6761_TOTAL_RECV_PIN	\
		(sizeof(mt6761_recv_pintable)/sizeof(struct _pin_recv))

/* info for all mbox: start, end, used_slot, mode, unused */
struct _mbox_info mt6761_mbox_table[IPI_MBOX_TOTAL] = {
	{0, 4, PINS_MBOX0_USED, 2, 0},  /* mbox 0 for send */
	{5, 11, PINS_MBOX1_USED, 2, 0},  /* mbox 1 for send */
	{0, 3, PINR_MBOX2_USED, 1, 0},  /* mbox 2 for recv */
	{0, 0, 0, 0, 0}, /* mbox 3 */
};

char *mt6761_pin_name[IPI_ID_TOTAL] = {
	"PLATFORM",
	"CPU_DVFS",
	"QOS",
	"TST1",
	"FHCTL",
	"MCDI",
	"SPM_SUSPEND",
	"PMIC",
	"PPM",
	"Thermal",
	"UPower",
	"CM_MGR",
};

#endif /* __SSPM_IPI_DEFINE_H__ */
