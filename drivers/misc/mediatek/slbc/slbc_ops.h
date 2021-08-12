/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _SLBC_OPS_H_
#define _SLBC_OPS_H_

#include <linux/list.h>
#include <linux/bitops.h>

/* error code */
#define EWAIT_RELEASE		1 /* wait for release */
#define ENOT_AVAILABLE		2 /* not available for now */
#define EREQ_DONE		3 /* already requested */
#define EREQ_MASKED		4 /* req madk bit set */
#define EDISABLED		5 /* req madk bit set */

/* call back return value */
#define CB_DONE			0 /* no need to use*/
#define CB_OK			1 /* ready to use/release */

/* slot status */
#define SLOT_AVAILABLE		0 /* slot available*/
#define SLOT_NOT_FOUND		1 /* slot not found */
#define SLOT_USED		2 /* slot used */

/* sid status */
#define SID_NOT_FOUND		0xffff

/* need to modify slbc_uid_str  */
enum slbc_uid {
	UID_ZERO = 0,
	UID_MM_VENC,
	UID_MM_DISP,
	UID_MM_MDP,
	UID_MM_VDEC,
	UID_AI_MDLA,
	UID_AI_ISP,
	UID_GPU,
	UID_HIFI3,
	UID_CPU,
	UID_AOV,
	UID_SH_P2,
	UID_SH_APU,
	UID_MML,
	UID_DSC_IDLE,
	UID_AINR,
	UID_TEST_BUFFER,
	UID_TEST_CACHE,
	UID_TEST_ACP,
	UID_DISP,
	UID_MAX,
};

#define UID_MM_BITS_1 (BIT(UID_MM_DISP) | BIT(UID_MM_MDP))
#define BIT_IN_MM_BITS_1(x) ((x) & UID_MM_BITS_1)

#define UID_MM_BITS_2 (BIT(UID_SH_P2) | BIT(UID_SH_APU))
#define BIT_IN_MM_BITS_2(x) ((x) & UID_MM_BITS_2)

#define UID_MM_BITS_3 (BIT(UID_MML) | BIT(UID_DISP))
#define BIT_IN_MM_BITS_3(x) ((x) & UID_MM_BITS_3)

enum slbc_type {
	TP_BUFFER = 0,
	TP_CACHE,
	TP_ACP,
};

enum slbc_force {
	FR_DIS = 0,
	FR_CPU,
	FR_GPU,
	FR_APU,
	FR_MAX,
};

#define ACP_ONLY_BIT	2
enum slbc_flag {
	FG_SECURE = BIT(0),
	FG_POWER = BIT(1),
	FG_ACP_ONLY = BIT(ACP_ONLY_BIT),
	FG_ACP_1_4 = BIT(ACP_ONLY_BIT + 1),
	FG_ACP_2_4 = BIT(ACP_ONLY_BIT + 2),
	FG_ACP_3_4 = BIT(ACP_ONLY_BIT + 3),
	FG_ACP_4_4 = BIT(ACP_ONLY_BIT + 4),
};

#define FG_ACP_BITS (FG_ACP_1_4 | FG_ACP_2_4 | FG_ACP_3_4 | FG_ACP_4_4)

#define SLBC_TRY_FLAG_BIT(d, bit) (((d)->flag & (bit)) == (bit))

struct slbc_data {
	unsigned int uid;
	unsigned int type;
	ssize_t size;
	unsigned int flag;
	int ret;
	/* below used by slbc driver */
	void __iomem *paddr;
	void __iomem *vaddr;
	unsigned int sid;
	unsigned int slot_used;
	void *config;
	int ref;
	int pwr_ref;
	struct slbc_data *private;
};

#define ui_to_slbc_data(d, ui) \
	do { \
		(d)->uid = ((ui) >> 24 & 0xff); \
		(d)->type = ((ui) >> 16 & 0xff); \
		(d)->flag = ((ui) >> 8 & 0xff); \
	} while (0)

#define slbc_data_to_ui(d) \
	((((d)->uid) & 0xff) << 24 | \
	(((d)->type) & 0xff) << 16 | \
	(((d)->flag) & 0xff) << 8)

struct slbc_ops {
	struct list_head node;
	struct slbc_data *data;
	int (*activate)(struct slbc_data *data);
	void (*deactivate)(struct slbc_data *data);
};

extern int slbc_enable;
extern char *slbc_uid_str[UID_MAX];
extern int popcount(unsigned int x);

#if IS_ENABLED(CONFIG_MTK_SLBC)
extern int slbc_request(struct slbc_data *d);
extern int slbc_release(struct slbc_data *d);
extern int slbc_power_on(struct slbc_data *d);
extern int slbc_power_off(struct slbc_data *d);
extern int slbc_secure_on(struct slbc_data *d);
extern int slbc_secure_off(struct slbc_data *d);
extern void slbc_update_mm_bw(unsigned int bw);
extern void slbc_update_mic_num(unsigned int num);
extern void slbc_update_inner(unsigned int inner);
extern void slbc_update_outer(unsigned int outer);
#else
__attribute__ ((weak)) int slbc_request(struct slbc_data *d)
{
	return -EDISABLED;
};
__attribute__ ((weak)) int slbc_release(struct slbc_data *d)
{
	return -EDISABLED;
};
__attribute__ ((weak)) int slbc_power_on(struct slbc_data *d)
{
	return -EDISABLED;
};
__attribute__ ((weak)) int slbc_power_off(struct slbc_data *d)
{
	return -EDISABLED;
};
__attribute__ ((weak)) int slbc_secure_on(struct slbc_data *d)
{
	return -EDISABLED;
};
__attribute__ ((weak)) int slbc_secure_off(struct slbc_data *d)
{
	return -EDISABLED;
};
__attribute__ ((weak)) void slbc_update_mm_bw(unsigned int bw) {}
__attribute__ ((weak)) void slbc_update_mic_num(unsigned int num) {}
__attribute__ ((weak)) void slbc_update_inner(unsigned int inner) {}
__attribute__ ((weak)) void slbc_update_outer(unsigned int outer) {}
#endif /* CONFIG_MTK_SLBC */

#endif /* _SLBC_OPS_H_ */
