#ifndef __MT_MON_H__
#define __MT_MON_H__

#include "mach/pmu_v7.h"
#include <linux/threads.h>

struct mt_mon_log
{

    __u32 cpu_cyc[NR_CPUS];
    __u32 cpu_cnt0[NR_CPUS];
    __u32 cpu_cnt1[NR_CPUS];
    __u32 cpu_cnt2[NR_CPUS];
    __u32 cpu_cnt3[NR_CPUS];

    __u32 BM_BCNT;
    __u32 BM_TACT;
    __u32 BM_TSCT;
    __u32 BM_WACT;
    __u32 BM_WSCT;
    __u32 BM_BACT;
    __u32 BM_BSCT;
    __u32 BM_TSCT2;
    __u32 BM_WSCT2;
    __u32 BM_TSCT3;
    __u32 BM_WSCT3;
    __u32 BM_WSCT4;
    __u32 BM_TTYPE1;
    __u32 BM_TTYPE2;
    __u32 BM_TTYPE3;
    __u32 BM_TTYPE4;
    __u32 BM_TTYPE5;
#if 1    
    __u32 BM_TTYPE6;
    __u32 BM_TTYPE7;
#endif    
    __u32 BM_TTYPE9;
    __u32 BM_TTYPE10;
    __u32 BM_TTYPE11;
    __u32 BM_TTYPE12;
    __u32 BM_TTYPE13;
#if 1    
    __u32 BM_TTYPE14;
    __u32 BM_TTYPE15;
#endif

    /* 6582 doesn't have MCI */
#if 0
    __u32 MCI_CNT0;
    __u32 MCI_CNT1;
#endif
    __u32 BM_TPCT1;
    
    __u32 DRAMC_PageHit;
    __u32 DRAMC_PageMiss;
    __u32 DRAMC_Interbank;
    __u32 DRAMC_Idle;   
};
typedef enum 
{
    MODE_MANUAL_USER,
    MODE_MANUAL_KERNEL,
    MODE_SCHED_SWITCH,
    MODE_PERIODIC,
    MODE_MANUAL_TRACER,
    MODE_FREE    
} MonitorMode;

extern void set_mt65xx_mon_period(long time_ns);
extern long get_mt65xx_mon_period(void);
extern void set_mt65xx_mon_manual_start(unsigned int bStart);
extern unsigned int get_mt65xx_mon_manual_start(void);
extern void set_mt65xx_mon_mode(MonitorMode mode);
extern MonitorMode get_mt65xx_mon_mode(void);

struct l2c_cfg{
    u32 l2c_evt[2];
};


struct mtk_monitor {
    int				(*init)(void);
    int				(*deinit)(void);
    int            	(*enable)(void);
    int            	(*disable)(void);
        unsigned int	(*mon_log)(void *);
    void 			(*set_pmu)(struct pmu_cfg *p_cfg);
    void 			(*get_pmu)(struct pmu_cfg *p_cfg);
    void 			(*set_l2c)(struct l2c_cfg *l_cfg);
    void 			(*get_l2c)(struct l2c_cfg *l_cfg);
    void			(*set_bm_rw)(int type);
    struct mt_mon_log *log_buff;
};

int register_monitor(struct mtk_monitor **mtk_mon, MonitorMode mode);
void unregister_monitor(struct mtk_monitor **mtk_mon);

#endif  /* !__MT65XX_MON_H__ */
