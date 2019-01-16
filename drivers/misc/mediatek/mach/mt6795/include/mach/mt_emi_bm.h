#ifndef __MT_EMI_BM_H__
#define __MT_EMI_BW_H__

#define EMI_CONM    (EMI_BASE_ADDR + 0x060)
#define EMI_ARBA    (EMI_BASE_ADDR + 0x100)
#define EMI_ARBB    (EMI_BASE_ADDR + 0x108)
#define EMI_ARBC    (EMI_BASE_ADDR + 0x110)
#define EMI_ARBD    (EMI_BASE_ADDR + 0x118)
#define EMI_ARBE    (EMI_BASE_ADDR + 0x120)
#define EMI_ARBF    (EMI_BASE_ADDR + 0x128)
#define EMI_ARBG    (EMI_BASE_ADDR + 0x130)
#define EMI_ARBH    (EMI_BASE_ADDR + 0x138)
#define EMI_BMEN    (EMI_BASE_ADDR + 0x400)
#define EMI_BCNT    (EMI_BASE_ADDR + 0x408)
#define EMI_TACT    (EMI_BASE_ADDR + 0x410)
#define EMI_TSCT    (EMI_BASE_ADDR + 0x418)
#define EMI_WACT    (EMI_BASE_ADDR + 0x420)
#define EMI_WSCT    (EMI_BASE_ADDR + 0x428)
#define EMI_BACT    (EMI_BASE_ADDR + 0x430)
#define EMI_BSCT    (EMI_BASE_ADDR + 0x438)
#define EMI_MSEL    (EMI_BASE_ADDR + 0x440)
#define EMI_TSCT2   (EMI_BASE_ADDR + 0x448)
#define EMI_TSCT3   (EMI_BASE_ADDR + 0x450)
#define EMI_WSCT2   (EMI_BASE_ADDR + 0x458)
#define EMI_WSCT3   (EMI_BASE_ADDR + 0x460)
#define EMI_WSCT4   (EMI_BASE_ADDR + 0x464)
#define EMI_MSEL2   (EMI_BASE_ADDR + 0x468)
#define EMI_MSEL3   (EMI_BASE_ADDR + 0x470)
#define EMI_MSEL4   (EMI_BASE_ADDR + 0x478)
#define EMI_MSEL5   (EMI_BASE_ADDR + 0x480)
#define EMI_MSEL6   (EMI_BASE_ADDR + 0x488)
#define EMI_MSEL7   (EMI_BASE_ADDR + 0x490)
#define EMI_MSEL8   (EMI_BASE_ADDR + 0x498)
#define EMI_MSEL9   (EMI_BASE_ADDR + 0x4A0)
#define EMI_MSEL10  (EMI_BASE_ADDR + 0x4A8)
#define EMI_BMID0   (EMI_BASE_ADDR + 0x4B0)
#define EMI_BMEN1   (EMI_BASE_ADDR + 0x4E0)
#define EMI_BMEN2   (EMI_BASE_ADDR + 0x4E8)
#define EMI_TTYPE1  (EMI_BASE_ADDR + 0x500)
#define EMI_TTYPE2  (EMI_BASE_ADDR + 0x508)
#define EMI_TTYPE3  (EMI_BASE_ADDR + 0x510)
#define EMI_TTYPE4  (EMI_BASE_ADDR + 0x518)
#define EMI_TTYPE5  (EMI_BASE_ADDR + 0x520)
#define EMI_TTYPE6  (EMI_BASE_ADDR + 0x528)
#define EMI_TTYPE7  (EMI_BASE_ADDR + 0x530)
#define EMI_TTYPE9  (EMI_BASE_ADDR + 0x540)
#define EMI_TTYPE10  (EMI_BASE_ADDR + 0x548)
#define EMI_TTYPE11  (EMI_BASE_ADDR + 0x550)
#define EMI_TTYPE12  (EMI_BASE_ADDR + 0x558)
#define EMI_TTYPE13  (EMI_BASE_ADDR + 0x560)
#define EMI_TTYPE14  (EMI_BASE_ADDR + 0x568)
#define EMI_TTYPE15  (EMI_BASE_ADDR + 0x570)
#define EMI_TTYPE16  (EMI_BASE_ADDR + 0x578)

#define DRAMC_R2R_PAGE_HIT      (DRAMCNAO_BASE_ADDR + 0x280)
#define DRAMC_R2R_PAGE_MISS     (DRAMCNAO_BASE_ADDR + 0x284)
#define DRAMC_R2R_INTERBANK     (DRAMCNAO_BASE_ADDR + 0x288)
#define DRAMC_R2W_PAGE_HIT      (DRAMCNAO_BASE_ADDR + 0x28C)
#define DRAMC_R2W_PAGE_MISS     (DRAMCNAO_BASE_ADDR + 0x290)
#define DRAMC_R2W_INTERBANK     (DRAMCNAO_BASE_ADDR + 0x294)
#define DRAMC_W2R_PAGE_HIT      (DRAMCNAO_BASE_ADDR + 0x298)
#define DRAMC_W2R_PAGE_MISS     (DRAMCNAO_BASE_ADDR + 0x29C)
#define DRAMC_W2R_INTERBANK     (DRAMCNAO_BASE_ADDR + 0x2A0)
#define DRAMC_W2W_PAGE_HIT      (DRAMCNAO_BASE_ADDR + 0x2A4)
#define DRAMC_W2W_PAGE_MISS     (DRAMCNAO_BASE_ADDR + 0x2A8)
#define DRAMC_W2W_INTERBANK     (DRAMCNAO_BASE_ADDR + 0x2AC)
#define DRAMC_IDLE_COUNT        (DRAMCNAO_BASE_ADDR + 0x2B0)

typedef enum
{
    DRAMC_R2R,
    DRAMC_R2W,
    DRAMC_W2R,
    DRAMC_W2W,
    DRAMC_ALL
} DRAMC_Cnt_Type;

typedef enum
{
    BM_BOTH_READ_WRITE,
    BM_READ_ONLY,
    BM_WRITE_ONLY
} BM_RW_Type;

enum 
{
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

#define BM_MASTER_AP_MCU1        (0x01)
#define BM_MASTER_AP_MCU2        (0x02)
#define BM_MASTER_MM1              (0x04)
#define BM_MASTER_MD_MCU        (0x08)
#define BM_MASTER_2G_3G_MDDMA   (0x10)
#define BM_MASTER_MM2              (0x20)
#define BM_MASTER_GPU1             (0x40)
#define BM_MASTER_GPU2             (0x80)
#define BM_MASTER_ALL               (0xFF)

#define BUS_MON_EN      (0x00000001)
#define BUS_MON_PAUSE   (0x00000002)
#define BC_OVERRUN      (0x00000100)

#define BM_COUNTER_MAX  (21)

#define BM_REQ_OK           (0)
#define BM_ERR_WRONG_REQ    (-1)
#define BM_ERR_OVERRUN      (-2)

extern void BM_Init(void);
extern void BM_DeInit(void);
extern void BM_Enable(const unsigned int enable);
//extern void BM_Disable(void);
extern void BM_Pause(void);
extern void BM_Continue(void);
extern unsigned int BM_IsOverrun(void);
extern void BM_SetReadWriteType(const unsigned int ReadWriteType);
extern int BM_GetBusCycCount(void);
extern unsigned int BM_GetTransAllCount(void);
extern int BM_GetTransCount(const unsigned int counter_num);
extern int BM_GetWordAllCount(void);
extern int BM_GetWordCount(const unsigned int counter_num);
extern unsigned int BM_GetBandwidthWordCount(void);
extern unsigned int BM_GetOverheadWordCount(void);
extern int BM_GetTransTypeCount(const unsigned int counter_num);
extern int BM_SetMonitorCounter(const unsigned int counter_num, const unsigned int master, const unsigned int trans_type);
extern int BM_SetMaster(const unsigned int counter_num, const unsigned int master);
extern int BM_SetIDSelect(const unsigned int counter_num, const unsigned int id, const unsigned int enable);
extern int BM_SetUltraHighFilter(const unsigned int counter_num, const unsigned int enable);
extern int BM_SetLatencyCounter(void);
extern int BM_GetLatencyCycle(const unsigned int counter_num);
extern int BM_GetEmiDcm(void);
extern int BM_SetEmiDcm(const unsigned int setting);

extern unsigned int DRAMC_GetPageHitCount(DRAMC_Cnt_Type CountType);
extern unsigned int DRAMC_GetPageMissCount(DRAMC_Cnt_Type CountType);
extern unsigned int DRAMC_GetInterbankCount(DRAMC_Cnt_Type CountType);
extern unsigned int DRAMC_GetIdleCount(void);

#endif  /* !__MT_EMI_BW_H__ */
