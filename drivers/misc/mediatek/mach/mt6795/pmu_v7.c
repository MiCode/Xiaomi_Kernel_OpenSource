#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/fs.h>
#include "mach/pmu_v7.h"

static void smp_pmu_stop(void);
static void smp_pmu_start(void);
static void smp_pmu_reset(void);
static void smp_pmu_enable_event(void);
static void smp_pmu_read_counter(void);
static u32 __init armv7_read_num_pmnc_events(void);

static int event_mask = 0x8000003f;

struct arm_pmu armv7pmu = {
        .enable                 = smp_pmu_enable_event,
        .read_counter           = smp_pmu_read_counter,
        .start                  = smp_pmu_start,
        .stop                   = smp_pmu_stop,
        .reset                  = smp_pmu_reset,
        .num_events             = (u32)armv7_read_num_pmnc_events,
        .id                     = ARM_PMU_ID_CA7,
        .name                   = "ARMv7 Cortex-A7",
        .perf_cfg               = {	
                                    /*PORTING-NOTE, per cpu has 4 event counter*/
                                        .event_cfg  = {	
                                                        ARMV7_IFETCH_MISS, 
                                                        ARMV7_L1_ICACHE_ACCESS, 
                                                        ARMV7_DCACHE_REFILL, 
                                                        ARMV7_DCACHE_ACCESS,
                                                      },
				  },
        .perf_data              = {	
                                    /*PORTING-NOTE, per cpu has one cnt_val*/
                                    .cnt_val 	= {
                                                    {0,0,0,0,0}, 
                                                    {0,0,0,0,0},
                                                    {0,0,0,0,0},
                                                    {0,0,0,0,0}
                                                  },
                                    /*PORTING-NOTE, per cpu has one overflow variable*/
				    .overflow	= {
				                    0, 
				                    0, 
				                    0, 
				                    0
				                  },
				  },
};

/*
 * armv7_pmnc_read: return the Performance Monitors Control Register
 * @:
 */

static inline unsigned long armv7_pmnc_read(void)
{
        u32 val;
        asm volatile("mrc p15, 0, %0, c9, c12, 0" : "=r"(val));
        return val;
}

/*
 * armv7_pmnc_read: write value to the Performance Monitors Control Register
 * @:
 */
static inline void armv7_pmnc_write(unsigned long val)
{
        val &= ARMV7_PMNC_MASK;
        isb();
        asm volatile("mcr p15, 0, %0, c9, c12, 0" : : "r"(val));
}
/*
 * armv7_read_num_pmnc_events: return the Performance Monitors Control Register counter
 * @:
 */
static u32 __init armv7_read_num_pmnc_events(void)
{
        u32 nb_cnt;

        /* Read the nb of CNTx counters supported from PMNC */
        nb_cnt = (armv7_pmnc_read() >> ARMV7_PMNC_N_SHIFT) & ARMV7_PMNC_N_MASK;

        return nb_cnt;
}


/*
 * armv7_pmnc_has_overflowed: return whether the performance counter is overflowed
 * @pmnc: performance counter value
 */
static inline int armv7_pmnc_has_overflowed(unsigned long pmnc)
{
        return pmnc & ARMV7_OVERFLOWED_MASK;
}

/*
 * armv7_pmnc_counter_has_overflowed: return whether the performance counter is overflowed
 * @pmnc: performance counter value
 * @counter: performance counter number
 */
static inline int armv7_pmnc_counter_has_overflowed(unsigned long pmnc,
                                        int counter)
{
        int ret = 0;

        if (counter == ARMV7_CYCLE_COUNTER)
                ret = pmnc & ARMV7_FLAG_C;
        else if ((counter >= ARMV7_COUNTER0) && (counter <= ARMV7_COUNTER_LAST))
                ret = pmnc & ARMV7_FLAG_P(counter);
        else
                pr_err("CPU%u checking wrong counter %d overflow status\n",
                        raw_smp_processor_id(), counter);

        return ret;
}

/*
 * armv7_pmnc_counter_select_counter: select monitor counter and return the selected monitor counter register index
 * to make sure the selected idx is not larger the maximum idx
 * @idx: the performance counter index
 */
static inline int armv7_pmnc_select_counter(unsigned int idx)
{
        u32 val;

        if ((idx < ARMV7_COUNTER0) || (idx > ARMV7_COUNTER_LAST)) {
                pr_err("CPU%u selecting wrong PMNC counter"
                        " %d\n", raw_smp_processor_id(), idx);
                return -1;
        }
        // why
        val = (idx - ARMV7_EVENT_CNT_TO_CNTx) & ARMV7_SELECT_MASK;
        asm volatile("mcr p15, 0, %0, c9, c12, 5" : : "r" (val));
        isb();

        return idx;
}

/*
 * armv7pmu_read_counter: return Performance Monitors Cycle Count Register
 * @idx: the performance counter index
 */
static inline u32 armv7pmu_read_counter(int idx)
{
        unsigned long value = 0;

        if (idx == ARMV7_CYCLE_COUNTER)
                asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (value));
        else if ((idx >= ARMV7_COUNTER0) && (idx <= ARMV7_COUNTER_LAST)) {
                if (armv7_pmnc_select_counter(idx) == idx)
                        asm volatile("mrc p15, 0, %0, c9, c13, 2"
                                     : "=r" (value));
        } else
                pr_err("CPU%u reading wrong counter %d\n",
                        raw_smp_processor_id(), idx);

        return value;
}

/*
 * armv7pmu_write_counter: write value to specific Performance Monitors Cycle Count Register
 * @idx: the performance counter index
 */
static inline void armv7pmu_write_counter(int idx, u32 value)
{
        if (idx == ARMV7_CYCLE_COUNTER)
                asm volatile("mcr p15, 0, %0, c9, c13, 0" : : "r" (value));
        else if ((idx >= ARMV7_COUNTER0) && (idx <= ARMV7_COUNTER_LAST)) {
                if (armv7_pmnc_select_counter(idx) == idx)
                        asm volatile("mcr p15, 0, %0, c9, c13, 2"
                                     : : "r" (value));
        } else
                pr_err("CPU%u writing wrong counter %d\n",
                        raw_smp_processor_id(), idx);
}

static inline void armv7_pmnc_write_evtsel(unsigned int idx, u32 val)
{
        if (armv7_pmnc_select_counter(idx) == idx) {
                val &= ARMV7_EVTSEL_MASK;
                asm volatile("mcr p15, 0, %0, c9, c13, 1" : : "r" (val));
        }
}

/*
 * armv7_pmnc_enable_counter: enable the selected Performance Monitors Count register
 * @idx: the performance counter index
 */
static inline u32 armv7_pmnc_enable_counter(unsigned int idx)
{
        u32 val;

        if ((idx != ARMV7_CYCLE_COUNTER) &&
            ((idx < ARMV7_COUNTER0) || (idx > ARMV7_COUNTER_LAST))) {
                pr_err("CPU%u enabling wrong PMNC counter"
                        " %d\n", raw_smp_processor_id(), idx);
                return -1;
        }

        if (idx == ARMV7_CYCLE_COUNTER)
                val = ARMV7_CNTENS_C;
        else
                val = ARMV7_CNTENS_P(idx);

        asm volatile("mcr p15, 0, %0, c9, c12, 1" : : "r" (val));

        return idx;
}

/*
 * armv7_pmnc_disable_counter: disable the selected Performance Monitors Count register
 * @idx: the performance counter index
 */
static inline u32 armv7_pmnc_disable_counter(unsigned int idx)
{
        u32 val;


        if ((idx != ARMV7_CYCLE_COUNTER) &&
            ((idx < ARMV7_COUNTER0) || (idx > ARMV7_COUNTER_LAST))) {
                pr_err("CPU%u disabling wrong PMNC counter"
                        " %d\n", raw_smp_processor_id(), idx);
                return -1;
        }

        if (idx == ARMV7_CYCLE_COUNTER)
                val = ARMV7_CNTENC_C;
        else
                val = ARMV7_CNTENC_P(idx);

        asm volatile("mcr p15, 0, %0, c9, c12, 2" : : "r" (val));

        return idx;
}

#if 0
static inline u32 armv7_pmnc_enable_interrupt(unsigned int idx)
{
        u32 val;

        if ((idx != ARMV7_CYCLE_COUNTER) &&
            ((idx < ARMV7_COUNTER0) || (idx > ARMV7_COUNTER_LAST))) {
                pr_err("CPU%u enabling wrong PMNC counter"
                        " interrupt enable %d\n", raw_smp_processor_id(), idx);
                return -1;
        }

        if (idx == ARMV7_CYCLE_COUNTER)
                val = ARMV7_INTENS_C;
        else
                val = ARMV7_INTENS_P(idx);

        asm volatile("mcr p15, 0, %0, c9, c14, 1" : : "r" (val));

        return idx;
}

static inline u32 armv7_pmnc_disable_interrupt(unsigned int idx)
{
        u32 val;

        if ((idx != ARMV7_CYCLE_COUNTER) &&
            ((idx < ARMV7_COUNTER0) || (idx > ARMV7_COUNTER_LAST))) {
                pr_err("CPU%u disabling wrong PMNC counter"
                        " interrupt enable %d\n", raw_smp_processor_id(), idx);
                return -1;
        }

        if (idx == ARMV7_CYCLE_COUNTER)
                val = ARMV7_INTENC_C;
        else
                val = ARMV7_INTENC_P(idx);

        asm volatile("mcr p15, 0, %0, c9, c14, 2" : : "r" (val));

        return idx;
}
#endif

/*
 * armv7_pmnc_get_overflow_status: get the overflow status.
 * :
 */
static u32 armv7_pmnc_get_overflow_status(void)
{
        u32 val;
        /* Read */
        asm volatile("mrc p15, 0, %0, c9, c12, 3" : "=r" (val));

        /* Write to clear flags */
        val &= ARMV7_FLAG_MASK;
        asm volatile("mcr p15, 0, %0, c9, c12, 3" : : "r" (val));
		
		return val;        
}

#ifdef DEBUG
static void armv7_pmnc_dump_regs(void)
{
        u32 val;
        unsigned int cnt;

        printk(KERN_INFO "PMNC registers dump:\n");

        asm volatile("mrc p15, 0, %0, c9, c12, 0" : "=r" (val));
        printk(KERN_INFO "PMNC  =0x%08x\n", val);

        asm volatile("mrc p15, 0, %0, c9, c12, 1" : "=r" (val));
        printk(KERN_INFO "CNTENS=0x%08x\n", val);

        asm volatile("mrc p15, 0, %0, c9, c14, 1" : "=r" (val));
        printk(KERN_INFO "INTENS=0x%08x\n", val);

        asm volatile("mrc p15, 0, %0, c9, c12, 3" : "=r" (val));
        printk(KERN_INFO "FLAGS =0x%08x\n", val);

        asm volatile("mrc p15, 0, %0, c9, c12, 5" : "=r" (val));
        printk(KERN_INFO "SELECT=0x%08x\n", val);

        asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (val));
        printk(KERN_INFO "CCNT  =0x%08x\n", val);

        for (cnt = ARMV7_COUNTER0; cnt < ARMV7_COUNTER_LAST; cnt++) {
                armv7_pmnc_select_counter(cnt);
                asm volatile("mrc p15, 0, %0, c9, c13, 2" : "=r" (val));
                printk(KERN_INFO "CNT[%d] count =0x%08x\n",
                        cnt-ARMV7_EVENT_CNT_TO_CNTx, val);
                asm volatile("mrc p15, 0, %0, c9, c13, 1" : "=r" (val));
                printk(KERN_INFO "CNT[%d] evtsel=0x%08x\n",
                        cnt-ARMV7_EVENT_CNT_TO_CNTx, val);
        }
}
#endif
/*
 * armv7pmu_enable_event: enable the selected performance counter register
 * @event: selected event
 * @idx: selected performance counter register
 */

static void armv7pmu_enable_event(u32 event , int idx)
{
       
       /*
        * Enable counter and interrupt, and set the counter to count
        * the event that we're interested in.
        */
 

       /*
        * Disable counter
         */
        armv7_pmnc_disable_counter(idx);

        /*
         * Set event (if destined for PMNx counters)
         * We don't need to set the event if it's a cycle count
         */
        if (idx != ARMV7_CYCLE_COUNTER)
                armv7_pmnc_write_evtsel(idx, event);

        /*
         * Enable interrupt for this counter
         */
        //armv7_pmnc_enable_interrupt(idx);

        /*
         * Enable counter
         */
        armv7_pmnc_enable_counter(idx);


}
            
/*
 * armv7pmu_disable_event: disable the selected performance counter register
 * @event: selected event
 * @idx: selected performance counter register
 */
static void armv7pmu_disable_event(u32 event, int idx)
{

        /*
         * Disable counter and interrupt
         */


        /*
         * Disable counter
         */
        armv7_pmnc_disable_counter(idx);

        /*
         * Disable interrupt for this counter
         */
        //armv7_pmnc_disable_interrupt(idx);


}

/*
 * armv7pmu_start:All counters, including PMCCNTR, are enabled.
 * @info:NULL
 */
static void armv7pmu_start(void *info)
{
        /* Enable all counters */
        armv7_pmnc_write(armv7_pmnc_read() | ARMV7_PMNC_E);
}

/*
 * armv7pmu_stop:All counters, including PMCCNTR, are disabled.
 * @info:NULL
 * @info: 
 */
static void armv7pmu_stop(void *info)
{
        /* Disable all counters */
        armv7_pmnc_write(armv7_pmnc_read() & ~ARMV7_PMNC_E);
}

/*
 * armv7pmu_reset:All counters, including PMCCNTR, are reseted.
 * @info:NULL
 */
static void armv7pmu_reset(void *info)
{
        
        /* The counter and interrupt enable registers are unknown at reset. */
        //for (idx = 0; idx < NUMBER_OF_EVENT; ++idx)
        //        armv7pmu_disable_event(NULL, idx);
		
		//armv7_pmnc_disable_counter(ARMV7_CYCLE_COUNTER);
		
        /* Initialize & Reset PMNC: C and P bits */
        armv7_pmnc_write(ARMV7_PMNC_P | ARMV7_PMNC_C);
}

/*
 * armv7pmu_enable: eanble all event, All counters, including PMCCNTR, are enabled
 * @info:NULL 
 */
static void armv7pmu_enable(void *info)
{
	int idx;
	struct  pmu_cfg *p_cfg= (struct  pmu_cfg *) &armv7pmu.perf_cfg;
	
	armv7pmu_reset(NULL);
	
	if(event_mask >> 31)
		armv7_pmnc_enable_counter(ARMV7_CYCLE_COUNTER);
	else
		armv7_pmnc_disable_counter(ARMV7_CYCLE_COUNTER);
	
	for (idx = 0; idx < NUMBER_OF_EVENT; idx++) {
		if( (event_mask >> idx) & EVENT_MASK )
			armv7pmu_enable_event(p_cfg->event_cfg[idx], idx);
		else
			armv7pmu_disable_event(0, idx);
	}  	
}

/*
 * armv7pmu_read_all_counter: read all counter, including PMCCNTR 
 * @info:NULL 
 */
static void armv7pmu_read_all_counter(void *info)
{
	int idx, cpu = raw_smp_processor_id();
	struct  pmu_data *p_data = (struct  pmu_data *) &armv7pmu.perf_data;
	
	for (idx = 0; idx < NUMBER_OF_EVENT + 1; idx++){ 
            p_data->cnt_val[cpu][idx] = armv7pmu_read_counter(idx);
   	}
   	
   	p_data->overflow[cpu] = armv7_pmnc_get_overflow_status();
}

static void smp_pmu_stop(void)
{
#ifdef CONFIG_SMP
	int i; 
	if(armv7pmu.multicore)
	{
		for(i = 0; i < NUMBER_OF_CPU; i++)
			mtk_smp_call_function_single(i, armv7pmu_stop, NULL, 1);
	} else
#endif
		armv7pmu_stop(NULL);
}

static void smp_pmu_start(void)
{
#ifdef CONFIG_SMP 
	int i;
	if(armv7pmu.multicore)
	{
		for(i = 0; i < NUMBER_OF_CPU; i++)
			mtk_smp_call_function_single(i, armv7pmu_start, NULL, 1);
	} else
#endif
		armv7pmu_start(NULL);
}

static void smp_pmu_reset(void)
{
#ifdef CONFIG_SMP 
        int i;
	if(armv7pmu.multicore)
	{
		for(i = 0; i < NUMBER_OF_CPU; i++)
			mtk_smp_call_function_single(i, armv7pmu_reset, NULL, 1);
	} else
#endif
		armv7pmu_reset(NULL);
}

static void smp_pmu_enable_event(void)
{
#ifdef CONFIG_SMP 	
        int i;
	if(armv7pmu.multicore)
	{
		for(i = 0; i < NUMBER_OF_CPU; i++)
			mtk_smp_call_function_single(i, armv7pmu_enable, NULL, 1);
	} else
#endif
		armv7pmu_enable(NULL);
}

/*static void smp_pmu_disable_event(void)
{
	
}*/

static void smp_pmu_read_counter(void)
{
#ifdef CONFIG_SMP 	
        int i;
	if(armv7pmu.multicore)
	{
		for(i = 0; i < NUMBER_OF_CPU; i++)
			mtk_smp_call_function_single(i, armv7pmu_read_all_counter, NULL, 1);
	} else
#endif
		armv7pmu_read_all_counter(NULL);
}

int register_pmu(struct arm_pmu **p_pmu)
{

	*p_pmu = &armv7pmu;	
	return 0;
}   


void unregister_pmu(struct arm_pmu **p_pmu)
{
	*p_pmu = NULL;
}

  
         
