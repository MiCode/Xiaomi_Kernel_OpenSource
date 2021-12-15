/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MT_MEM_BW_H__
#define __MT_MEM_BW_H__

#define ENABLE_MBW		1
#define DISABLE_FLIPPER_FUNC	0
#ifdef CONFIG_MTK_ENG_BUILD
/* #define ENABLE_RUNTIME_BM */
#endif

#define EMI_BMEN	(CEN_EMI_BASE + 0x400)
#define EMI_BCNT	(CEN_EMI_BASE + 0x408)
#define EMI_TACT	(CEN_EMI_BASE + 0x410)
#define EMI_TSCT	(CEN_EMI_BASE + 0x418)
#define EMI_WACT	(CEN_EMI_BASE + 0x420)
#define EMI_WSCT	(CEN_EMI_BASE + 0x428)
#define EMI_BACT	(CEN_EMI_BASE + 0x430)
#define EMI_BSCT	(CEN_EMI_BASE + 0x438)
#define EMI_MSEL	(CEN_EMI_BASE + 0x440)
#define EMI_TSCT2	(CEN_EMI_BASE + 0x448)
#define EMI_TSCT3	(CEN_EMI_BASE + 0x450)
#define EMI_WSCT2	(CEN_EMI_BASE + 0x458)
#define EMI_WSCT3	(CEN_EMI_BASE + 0x460)
#define EMI_WSCT4	(CEN_EMI_BASE + 0x464)
#define EMI_MSEL2	(CEN_EMI_BASE + 0x468)
#define EMI_MSEL3	(CEN_EMI_BASE + 0x470)
#define EMI_MSEL4	(CEN_EMI_BASE + 0x478)
#define EMI_MSEL5	(CEN_EMI_BASE + 0x480)
#define EMI_MSEL6	(CEN_EMI_BASE + 0x488)
#define EMI_MSEL7	(CEN_EMI_BASE + 0x490)
#define EMI_MSEL8	(CEN_EMI_BASE + 0x498)
#define EMI_MSEL9	(CEN_EMI_BASE + 0x4A0)
#define EMI_MSEL10	(CEN_EMI_BASE + 0x4A8)
#define EMI_BMID0	(CEN_EMI_BASE + 0x4B0)
#define EMI_BMID1	(CEN_EMI_BASE + 0x4B4)
#define EMI_BMID2	(CEN_EMI_BASE + 0x4B8)
#define EMI_BMID3	(CEN_EMI_BASE + 0x4BC)
#define EMI_BMID4	(CEN_EMI_BASE + 0x4C0)
#define EMI_BMID5	(CEN_EMI_BASE + 0x4C4)
#define EMI_BMID6	(CEN_EMI_BASE + 0x4C8)
#define EMI_BMID7	(CEN_EMI_BASE + 0x4CC)
#define EMI_BMID8	(CEN_EMI_BASE + 0x4D0)
#define EMI_BMID9	(CEN_EMI_BASE + 0x4D4)
#define EMI_BMID10	(CEN_EMI_BASE + 0x4D8)
#define EMI_BMEN1	(CEN_EMI_BASE + 0x4E0)
#define EMI_BMEN2	(CEN_EMI_BASE + 0x4E8)
#define EMI_BMRW0	(CEN_EMI_BASE + 0x4F8)
#define EMI_TTYPE1	(CEN_EMI_BASE + 0x500)
#define EMI_TTYPE2	(CEN_EMI_BASE + 0x508)
#define EMI_TTYPE3	(CEN_EMI_BASE + 0x510)
#define EMI_TTYPE4	(CEN_EMI_BASE + 0x518)
#define EMI_TTYPE5	(CEN_EMI_BASE + 0x520)
#define EMI_TTYPE6	(CEN_EMI_BASE + 0x528)
#define EMI_TTYPE7	(CEN_EMI_BASE + 0x530)
#define EMI_TTYPE8	(CEN_EMI_BASE + 0x538)
#define EMI_TTYPE9	(CEN_EMI_BASE + 0x540)
#define EMI_TTYPE10	(CEN_EMI_BASE + 0x548)
#define EMI_TTYPE11	(CEN_EMI_BASE + 0x550)
#define EMI_TTYPE12	(CEN_EMI_BASE + 0x558)
#define EMI_TTYPE13	(CEN_EMI_BASE + 0x560)
#define EMI_TTYPE14	(CEN_EMI_BASE + 0x568)
#define EMI_TTYPE15	(CEN_EMI_BASE + 0x570)
#define EMI_TTYPE16	(CEN_EMI_BASE + 0x578)
#define EMI_TTYPE17	(CEN_EMI_BASE + 0x580)
#define EMI_TTYPE18	(CEN_EMI_BASE + 0x588)
#define EMI_TTYPE19	(CEN_EMI_BASE + 0x590)
#define EMI_TTYPE20	(CEN_EMI_BASE + 0x598)
#define EMI_TTYPE21	(CEN_EMI_BASE + 0x5A0)
#define EMI_TTYPE(i)	(EMI_TTYPE1 + (i*8))

#define EMI_BWCT0	(CEN_EMI_BASE + 0x5B0)
#define EMI_BWST0	(CEN_EMI_BASE + 0x5C4)
#define EMI_BWCT0_2ND	(CEN_EMI_BASE + 0x6A0)
#define EMI_BWST_2ND	(CEN_EMI_BASE + 0x6A8)
#define EMI_BWCT0_3RD	(CEN_EMI_BASE + 0x770)
#define EMI_BWST_3RD	(CEN_EMI_BASE + 0x778)
#define EMI_BWCT0_4TH	(CEN_EMI_BASE + 0x780)
#define EMI_BWST_4TH	(CEN_EMI_BASE + 0x788)

enum {
	BM_BOTH_READ_WRITE,
	BM_READ_ONLY,
	BM_WRITE_ONLY
};

enum {
	BM_TRANS_TYPE_1BEAT = 0x0,
	BM_TRANS_TYPE_2BEAT,
	BM_TRANS_TYPE_3BEAT,
	BM_TRANS_TYPE_4BEAT,
	BM_TRANS_TYPE_5BEAT,
	BM_TRANS_TYPE_6BEAT,
	BM_TRANS_TYPE_7BEAT,
	BM_TRANS_TYPE_8BEAT,
	BM_TRANS_TYPE_9BEAT,
	BM_TRANS_TYPE_10BEAT,
	BM_TRANS_TYPE_11BEAT,
	BM_TRANS_TYPE_12BEAT,
	BM_TRANS_TYPE_13BEAT,
	BM_TRANS_TYPE_14BEAT,
	BM_TRANS_TYPE_15BEAT,
	BM_TRANS_TYPE_16BEAT,
	BM_TRANS_TYPE_1Byte = 0 << 4,
	BM_TRANS_TYPE_2Byte = 1 << 4,
	BM_TRANS_TYPE_4Byte = 2 << 4,
	BM_TRANS_TYPE_8Byte = 3 << 4,
	BM_TRANS_TYPE_16Byte = 4 << 4,
	BM_TRANS_TYPE_BURST_WRAP = 0 << 7,
	BM_TRANS_TYPE_BURST_INCR = 1 << 7
};

#define BM_MASTER_APMCU0	(0x01)
#define BM_MASTER_APMCU1	(0x02)
#define BM_MASTER_MM0		(0x04)
#define BM_MASTER_MDMCU		(0x08)
#define BM_MASTER_MD		(0x10)
#define BM_MASTER_MM1		(0x20)
#define BM_MASTER_GPU0		(0x40)
#define BM_MASTER_GPU1		(0x80)
#define BM_MASTER_ALL		(0xFF)

#define BUS_MON_EN	(0x00000001)
#define BUS_MON_PAUSE	(0x00000002)
#define BC_OVERRUN	(0x00000100)

#define BM_COUNTER_MAX	(21)

#define BM_REQ_OK		(0)
#define BM_ERR_WRONG_REQ	(-1)
#define BM_ERR_OVERRUN		(-2)

extern void BM_Init(void);
extern void BM_DeInit(void);
extern void BM_Enable(const unsigned int enable);
extern void BM_Pause(void);
extern void BM_Continue(void);
extern unsigned int BM_IsOverrun(void);
extern void BM_SetReadWriteType(const unsigned int ReadWriteType);
extern int BM_GetBusCycCount(void);
extern unsigned int BM_GetTransAllCount(void);
extern int BM_GetTransCount(const unsigned int counter_num);
extern long long BM_GetWordAllCount(void);
extern int BM_GetWordCount(const unsigned int counter_num);
extern unsigned int BM_GetBandwidthWordCount(void);
extern unsigned int BM_GetOverheadWordCount(void);
extern int BM_GetTransTypeCount(const unsigned int counter_num);
extern int BM_SetMonitorCounter(const unsigned int counter_num,
const unsigned int master, const unsigned int trans_type);
extern int BM_SetMaster(const unsigned int counter_num,
				const unsigned int master);
extern int BM_SetIDSelect(const unsigned int counter_num, const unsigned int id,
			  const unsigned int enable);
extern int BM_SetUltraHighFilter(const unsigned int counter_num,
				const unsigned int enable);
extern int BM_SetLatencyCounter(void);
extern int BM_GetLatencyCycle(const unsigned int counter_num);
extern int BM_GetEmiDcm(void);
extern int BM_SetEmiDcm(const unsigned int setting);

extern int BM_SetBW(const unsigned int BW_config);
extern int BM_SetBW2(const unsigned int BW_config);
extern int BM_SetBW3(const unsigned int BW_config);
extern int BM_SetBW4(const unsigned int BW_config);
extern unsigned int BM_GetBWST(void);
extern unsigned int BM_GetBWST2(void);
extern unsigned int BM_GetBWST3(void);
extern unsigned int BM_GetBWST4(void);
extern unsigned int BM_GetBW(void);
extern unsigned int BM_GetBW2(void);
extern unsigned int BM_GetBW3(void);
extern unsigned int BM_GetBW4(void);

extern int mbw_init(void);
extern void enable_dump_latency(void);
extern void disable_dump_latency(void);
extern void dump_emi_latency(void);
extern void dump_emi_outstanding(void);
extern void dump_emi_outstanding_for_md(void);
extern void dump_last_bm(char *buf, unsigned int leng);

typedef unsigned long long (*getmembw_func)(void);
extern void mt_getmembw_registerCB(getmembw_func pCB);

unsigned long long get_mem_bw(void);

#endif  /* !__MT_MEM_BW_H__ */
