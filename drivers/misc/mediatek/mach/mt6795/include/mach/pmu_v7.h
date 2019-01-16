#ifndef _PMU_v7_H
#define _PMU_V7_H
#include<linux/threads.h>

/*
 * The cycle counter is ARMV7_CYCLE_COUNTER.
 * The first event counter is ARMV7_COUNTER0.
 * The last event counter is (ARMV7_COUNTER0 + armpmu->num_events - 1).
 */
#define NUMBER_OF_EVENT                 4
#define ARMV7_COUNTER0			0
#define ARMV7_COUNTER_LAST              (NUMBER_OF_EVENT - 1)
#define ARMV7_CYCLE_COUNTER		(ARMV7_COUNTER_LAST + 1)
#define NUMBER_OF_CPU  NR_CPUS 

enum mtk_arm_perf_pmu_ids {
    ARM_PMU_ID_CA8 = 0,
    ARM_PMU_ID_CA9,
    ARM_PMU_ID_CA5,
    ARM_PMU_ID_CA15,
    ARM_PMU_ID_CA7,
    ARM_NUM_PMU_ID,
};

#define EVENT_MASK  0x00000001

struct pmu_data{
    u32 cnt_val[NUMBER_OF_CPU][NUMBER_OF_EVENT + 1];  //Number of event counters + cycle event counter
    u32 overflow[NUMBER_OF_CPU];
};

struct pmu_cfg{
    u32 event_cfg[NUMBER_OF_EVENT];
};
  
struct arm_pmu {
    enum mtk_arm_perf_pmu_ids id;
    const char      *name;
    //irqreturn_t     (*handle_irq)(int irq_num, void *dev);
    void            (*enable)(void);
    //void            (*disable)(void);
    void             (*read_counter)(void);
    void            (*start)(void);
    void            (*stop)(void);
    void            (*reset)(void);
    int             num_events;
    struct pmu_data perf_data;
    struct pmu_cfg  perf_cfg;
    int 			multicore;
};

enum mtk_armv7_perf_types {
    ARMV7_PMNC_SW_INCR              = 0x00,
    ARMV7_IFETCH_MISS               = 0x01,
    ARMV7_ITLB_MISS                 = 0x02,
    ARMV7_DCACHE_REFILL             = 0x03, /* L1 */
    ARMV7_DCACHE_ACCESS             = 0x04, /* L1 */
    ARMV7_DTLB_REFILL               = 0x05,
    ARMV7_DREAD                     = 0x06,
    ARMV7_DWRITE                    = 0x07,
    ARMV7_INSTR_EXECUTED            = 0x08,
    ARMV7_EXC_TAKEN                 = 0x09,
    ARMV7_EXC_EXECUTED              = 0x0A,
    ARMV7_CID_WRITE                 = 0x0B,
    /* ARMV7_PC_WRITE is equivalent to HW_BRANCH_INSTRUCTIONS.
     * It counts:
     *  - all branch instructions,
     *  - instructions that explicitly write the PC,
     *  - exception generating instructions.
     */
    ARMV7_PC_WRITE                  = 0x0C,
    ARMV7_PC_IMM_BRANCH             = 0x0D,
    ARMV7_PC_PROC_RETURN            = 0x0E,
    ARMV7_UNALIGNED_ACCESS          = 0x0F,

    /* These events are defined by the PMUv2 supplement (ARM DDI 0457A). */
    ARMV7_PC_BRANCH_MIS_PRED        = 0x10,
    ARMV7_CLOCK_CYCLES              = 0x11,
    ARMV7_PC_BRANCH_PRED            = 0x12,
    ARMV7_MEM_ACCESS                = 0x13,
    ARMV7_L1_ICACHE_ACCESS          = 0x14,
    ARMV7_L1_DCACHE_WB              = 0x15,
    ARMV7_L2_DCACHE_ACCESS          = 0x16,
    ARMV7_L2_DCACHE_REFILL          = 0x17,
    ARMV7_L2_DCACHE_WB              = 0x18,
    ARMV7_BUS_ACCESS                = 0x19,
    ARMV7_MEMORY_ERROR              = 0x1A,
    ARMV7_INSTR_SPEC                = 0x1B,
    ARMV7_TTBR_WRITE                = 0x1C,
    ARMV7_BUS_CYCLES                = 0x1D,

    ARMV7_CPU_CYCLES                = 0xFF
};            
/* ARMv7 Cortex-A9 specific event types */                     
enum armv7_a9_perf_types {                                     
    ARMV7_JAVA_HW_BYTECODE_EXEC     = 0x40,        
    ARMV7_JAVA_SW_BYTECODE_EXEC     = 0x41,        
    ARMV7_JAZELLE_BRANCH_EXEC       = 0x42,        

    ARMV7_COHERENT_LINE_MISS        = 0x50,        
    ARMV7_COHERENT_LINE_HIT         = 0x51,        

    ARMV7_ICACHE_DEP_STALL_CYCLES   = 0x60,        
    ARMV7_DCACHE_DEP_STALL_CYCLES   = 0x61,        
    ARMV7_TLB_MISS_DEP_STALL_CYCLES = 0x62,        
    ARMV7_STREX_EXECUTED_PASSED     = 0x63,        
    ARMV7_STREX_EXECUTED_FAILED     = 0x64,        
    ARMV7_DATA_EVICTION             = 0x65,        
    ARMV7_ISSUE_STAGE_NO_INST       = 0x66,        
    ARMV7_ISSUE_STAGE_EMPTY         = 0x67,        
    ARMV7_INST_OUT_OF_RENAME_STAGE  = 0x68,        

    ARMV7_PREDICTABLE_FUNCT_RETURNS = 0x6E,        

    ARMV7_MAIN_UNIT_EXECUTED_INST   = 0x70,        
    ARMV7_SECOND_UNIT_EXECUTED_INST = 0x71,        
    ARMV7_LD_ST_UNIT_EXECUTED_INST  = 0x72,        
    ARMV7_FP_EXECUTED_INST          = 0x73,        
    ARMV7_NEON_EXECUTED_INST        = 0x74,        

    ARMV7_PLD_FULL_DEP_STALL_CYCLES = 0x80,        
    ARMV7_DATA_WR_DEP_STALL_CYCLES  = 0x81,        
    ARMV7_ITLB_MISS_DEP_STALL_CYCLES        = 0x82,
    ARMV7_DTLB_MISS_DEP_STALL_CYCLES        = 0x83,
    ARMV7_MICRO_ITLB_MISS_DEP_STALL_CYCLES  = 0x84,
    ARMV7_MICRO_DTLB_MISS_DEP_STALL_CYCLES  = 0x85,
    ARMV7_DMB_DEP_STALL_CYCLES      = 0x86,        

    ARMV7_INTGR_CLK_ENABLED_CYCLES  = 0x8A,        
    ARMV7_DATA_ENGINE_CLK_EN_CYCLES = 0x8B,        

    ARMV7_ISB_INST                  = 0x90,        
    ARMV7_DSB_INST                  = 0x91,        
    ARMV7_DMB_INST                  = 0x92,        
    ARMV7_EXT_INTERRUPTS            = 0x93,        

    ARMV7_PLE_CACHE_LINE_RQST_COMPLETED     = 0xA0,
    ARMV7_PLE_CACHE_LINE_RQST_SKIPPED       = 0xA1,
    ARMV7_PLE_FIFO_FLUSH            = 0xA2,        
    ARMV7_PLE_RQST_COMPLETED        = 0xA3,        
    ARMV7_PLE_FIFO_OVERFLOW         = 0xA4,        
    ARMV7_PLE_RQST_PROG             = 0xA5         
};  

int register_pmu(struct arm_pmu **p_pmu);
void unregister_pmu(struct arm_pmu **p_pmu);




/*
 * Per-CPU PMNC: config reg
 */
#define ARMV7_PMNC_E            (1 << 0) /* Enable all counters */
#define ARMV7_PMNC_P            (1 << 1) /* Reset all counters */
#define ARMV7_PMNC_C            (1 << 2) /* Cycle counter reset */
#define ARMV7_PMNC_D            (1 << 3) /* CCNT counts every 64th cpu cycle */
#define ARMV7_PMNC_X            (1 << 4) /* Export to ETM */
#define ARMV7_PMNC_DP           (1 << 5) /* Disable CCNT if non-invasive debug*/
#define ARMV7_PMNC_N_SHIFT      11       /* Number of counters supported */
#define ARMV7_PMNC_N_MASK       0x1f
#define ARMV7_PMNC_MASK         0x3f     /* Mask for writable bits */

/*
 * Available counters
 */
#define ARMV7_CNT0              0       /* First event counter */
#define ARMV7_CCNT              31      /* Cycle counter */

/* Event to low level counters mapping */
#define ARMV7_EVENT_CNT_TO_CNTx (ARMV7_COUNTER0 - ARMV7_CNT0)

/*
 * CNTENS: counters enable reg
 */
#define ARMV7_CNTENS_P(idx)     (1 << (idx - ARMV7_EVENT_CNT_TO_CNTx))
#define ARMV7_CNTENS_C          (1 << ARMV7_CCNT)

/*
 * CNTENC: counters disable reg
 */
#define ARMV7_CNTENC_P(idx)     (1 << (idx - ARMV7_EVENT_CNT_TO_CNTx))
#define ARMV7_CNTENC_C          (1 << ARMV7_CCNT)

/*
 * INTENS: counters overflow interrupt enable reg
 */
#define ARMV7_INTENS_P(idx)     (1 << (idx - ARMV7_EVENT_CNT_TO_CNTx))
#define ARMV7_INTENS_C          (1 << ARMV7_CCNT)

/*
 * INTENC: counters overflow interrupt disable reg
 */
#define ARMV7_INTENC_P(idx)     (1 << (idx - ARMV7_EVENT_CNT_TO_CNTx))
#define ARMV7_INTENC_C          (1 << ARMV7_CCNT)

/*
 * EVTSEL: Event selection reg
 */
#define ARMV7_EVTSEL_MASK       0xff            /* Mask for writable bits */

/*
 * SELECT: Counter selection reg
 */
#define ARMV7_SELECT_MASK       0x1f            /* Mask for writable bits */

/*
 * FLAG: counters overflow flag status reg
 */
#define ARMV7_FLAG_P(idx)       (1 << (idx - ARMV7_EVENT_CNT_TO_CNTx))
#define ARMV7_FLAG_C            (1 << ARMV7_CCNT)
#define ARMV7_FLAG_MASK         0xffffffff      /* Mask for writable bits */
#define ARMV7_OVERFLOWED_MASK   ARMV7_FLAG_MASK

#endif
