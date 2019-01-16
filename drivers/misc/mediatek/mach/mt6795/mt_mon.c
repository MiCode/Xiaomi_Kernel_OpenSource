#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/trace_seq.h>
#include <linux/ftrace_event.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>


#include "asm/hardware/cache-l2x0.h"
#include "mach/mt_reg_base.h"
#include "mach/sync_write.h"
#include "mach/mt_emi_bm.h"
//#include "mach/mt6573_pll.h"
#include "mach/mt_mon.h"
// FIX-ME mark for porting
#include "mach/mt_dcm.h"
#include <../../kernel/kernel/trace/trace.h>
#include <asm/io.h> 

#define MON_LOG_BUFF_LEN    (64 * 1024)
#define DEF_BM_RW_TYPE      (BM_BOTH_READ_WRITE)

static DECLARE_BITMAP(buf_bitmap, MON_LOG_BUFF_LEN);


static unsigned int bm_master_evt = BM_MASTER_AP_MCU1|BM_MASTER_AP_MCU2;
static unsigned int bm_rw_type_evt = DEF_BM_RW_TYPE;

static MonitorMode register_mode = MODE_FREE;
static unsigned long mon_period_evt;
static unsigned int mon_manual_evt;

struct mt_mon_log *mt_mon_log_buff; //this buffer is allocated for MODE_MANUAL_USER & MODE_MANUAL_KERNEL only
unsigned int mt_mon_log_buff_index;
unsigned int mt_kernel_ring_buff_index;

struct arm_pmu *p_pmu;
struct mtk_monitor mtk_mon;

//static DEFINE_SPINLOCK(mtk_monitor_lock);

/*
 * mt65xx_mon_init: Initialize the monitor.
 * Return 0.
 */
static int mt65xx_mon_init(void)
{
    BM_Init();

#if 0
    // disable system DCM
    if (0 == BM_GetEmiDcm()) //0 means EMI dcm is enabled
    {
        printk("[MON] Disable system DCM\n");
        dcm_disable(ALL_DCM);
        BM_SetEmiDcm(0xff); //disable EMI dcm
    }
#endif
    return 0;
}

/*
 * mt65xx_mon_deinit: De-initialize the monitor.
 * Return 0.
 */
static int mt65xx_mon_deinit(void)
{

    BM_DeInit();

/*
    if (mt_mon_log_buff) {
        vfree(mt_mon_log_buff);

        mt_mon_log_buff = 0;
    }
*/
#if 0
    // enable system DCM
    if (1 == BM_GetEmiDcm()) //1 means EMI dcm is disabled
    {
        printk("[MON] Enable system DCM\n");
        dcm_enable(ALL_DCM);
        BM_SetEmiDcm(0x0); //enable EMI dcm
    }
#endif
    return 0;

}

/*
 * mt65xx_mon_enable: Enable hardware monitors.
 * Return 0.
 */
static int mt65xx_mon_enable(void)
{
	
	p_pmu->reset();

    // enable & start ARM performance monitors
    p_pmu->enable();
	p_pmu->start();

    // stopping EMI monitors will reset all counters
    BM_Enable(0);

    // start EMI monitor counting
    BM_Enable(1);
	
    return 0;
}

/*
 * mt65xx_mon_disable: Disable hardware monitors.
 * Return 0.
 */
static int mt65xx_mon_disable(void)
{
	
    // disable ARM performance monitors
	p_pmu->stop();
	
    BM_Pause();
    
    return 0;
}

static inline void set_cpumask(unsigned int cpu, unsigned int index, volatile unsigned long *p)
{
	unsigned long cpumask = 1UL << cpu;
	unsigned int offset = (index & 3) << 3; // (index % 4) * 8
	
	p += (index >> 2);
	
	*p |= (cpumask << offset);
}

static inline void clear_bitmap(unsigned int bit, volatile unsigned long *p)
{
/*  
	unsigned long mask = 1UL << (bit & 31);
	
	p += bit >> 5;
	
	*p &= ~mask;
*/
}

static inline int get_cpumask(unsigned int index, volatile unsigned long *p)
{
	unsigned int res;
	unsigned long offset = (index & 3) << 3; // (index % 4) * 8
	
	p += (index >> 2);
	res = *p;
	res = (res >> offset) & 0x7;
	
	return res;
}
/*
 * mt65xx_mon_log: Get the current log from hardware monitors.
 * Return a index to the curret log entry in the log buffer.
 */
static unsigned int mt65xx_mon_log(void* log_buff)
{
    struct mt_mon_log* mon_buff;
    struct pmu_data *pmu_data =  & p_pmu->perf_data;
    unsigned int cpu = raw_smp_processor_id();
    unsigned int cur = 0;
	        
    p_pmu->read_counter();
	
	
	/* In MODE_SCHED_SWITCH, we need to record the current CPU number to get the Context Switch CPU number.
	 * 
	 * */
    if( register_mode == MODE_MANUAL_USER || register_mode == MODE_MANUAL_KERNEL){
        set_cpumask(cpu,cur,buf_bitmap);
        cur = mt_mon_log_buff_index++;
        mon_buff = &mt_mon_log_buff[cur];
        mt_mon_log_buff_index %= MON_LOG_BUFF_LEN;
    }else {
        cur = mt_kernel_ring_buff_index++;
        mon_buff = (struct mt_mon_log*)log_buff;
    }

        if(mon_buff) 
        {
            for_each_present_cpu(cpu){
                mon_buff->cpu_cnt0[cpu] = pmu_data->cnt_val[cpu][0];
                mon_buff->cpu_cnt1[cpu] = pmu_data->cnt_val[cpu][1];
                mon_buff->cpu_cnt2[cpu] = pmu_data->cnt_val[cpu][2];
                mon_buff->cpu_cnt3[cpu] = pmu_data->cnt_val[cpu][3];
                mon_buff->cpu_cyc[cpu] =  pmu_data->cnt_val[cpu][ARMV7_CYCLE_COUNTER];
            }
			
#if 1
            mon_buff->BM_BCNT = BM_GetBusCycCount();
            mon_buff->BM_TACT = BM_GetTransAllCount();
            mon_buff->BM_TSCT = BM_GetTransCount(1);
            mon_buff->BM_WACT = BM_GetWordAllCount();
            mon_buff->BM_WSCT = BM_GetWordCount(1);
            mon_buff->BM_BACT = BM_GetBandwidthWordCount();
            mon_buff->BM_BSCT = BM_GetOverheadWordCount();
            mon_buff->BM_TSCT2 = BM_GetTransCount(2);
            mon_buff->BM_WSCT2 = BM_GetWordCount(2);
            mon_buff->BM_TSCT3 = BM_GetTransCount(3);
            mon_buff->BM_WSCT3 = BM_GetWordCount(3);
            mon_buff->BM_WSCT4 = BM_GetWordCount(4);
            mon_buff->BM_TTYPE1 = BM_GetLatencyCycle(1);
            mon_buff->BM_TTYPE2 = BM_GetLatencyCycle(2);
            mon_buff->BM_TTYPE3 = BM_GetLatencyCycle(3);
            mon_buff->BM_TTYPE4 = BM_GetLatencyCycle(4);
            mon_buff->BM_TTYPE5 = BM_GetLatencyCycle(5);
            mon_buff->BM_TTYPE6 = BM_GetLatencyCycle(6);
            mon_buff->BM_TTYPE7 = BM_GetLatencyCycle(7);
            
            mon_buff->BM_TTYPE9 = BM_GetLatencyCycle(9);
            mon_buff->BM_TTYPE10 = BM_GetLatencyCycle(10);
            mon_buff->BM_TTYPE11 = BM_GetLatencyCycle(11);
            mon_buff->BM_TTYPE12 = BM_GetLatencyCycle(12);
            mon_buff->BM_TTYPE13 = BM_GetLatencyCycle(13);
            mon_buff->BM_TTYPE14 = BM_GetLatencyCycle(14);
            mon_buff->BM_TTYPE15 = BM_GetLatencyCycle(15);

            //mon_buff->BM_TPCT1 = BM_GetTransTypeCount(1); //not used now
            

            mon_buff->DRAMC_PageHit = DRAMC_GetPageHitCount(DRAMC_ALL);
            mon_buff->DRAMC_PageMiss = DRAMC_GetPageMissCount(DRAMC_ALL);
            mon_buff->DRAMC_Interbank = DRAMC_GetInterbankCount(DRAMC_ALL);
            mon_buff->DRAMC_Idle = DRAMC_GetIdleCount();        
#endif
        }

    memset(pmu_data->cnt_val[0], 0, sizeof(struct pmu_data));
    return cur;
}

extern unsigned int mt_get_emi_freq(void);

enum print_line_t mt65xx_mon_print_entry(struct mt65xx_mon_entry *entry, struct trace_iterator *iter){
    struct trace_seq *s = &iter->seq;
    int cpu = entry->cpu;
    struct mt_mon_log *log_entry;
    unsigned int log = 0;    
    MonitorMode mon_mode_evt = get_mt65xx_mon_mode();
    

    if(entry == NULL)
        return TRACE_TYPE_HANDLED;
    else{
        log_entry = &entry->field;
        log = entry->log;
    }

    if (log == 0) {
        trace_seq_printf(s, "MON_LOG_BUFF_LEN = %d, ", MON_LOG_BUFF_LEN);    
        trace_seq_printf(s, "EMI_CLOCK = %d, ", mt_get_emi_freq());
    }
    
        if(mon_mode_evt != MODE_SCHED_SWITCH){
          for_each_present_cpu(cpu)
          {
            trace_seq_printf(
              				s,
              				" cpu%d_cyc = %d, cpu%d_cnt0 = %d, cpu%d_cnt1 = %d, cpu%d_cnt2 = %d, cpu%d_cnt3 = %d, ",
              				cpu,
              				log_entry->cpu_cyc[cpu],
              				cpu,
              				log_entry->cpu_cnt0[cpu],
              				cpu,
              				log_entry->cpu_cnt1[cpu],
              				cpu,
              				log_entry->cpu_cnt2[cpu],
              				cpu,
              				log_entry->cpu_cnt3[cpu]);
          }
        }
        else /*SCHED_SWITCH - only print self cpu*/
        {
          trace_seq_printf(
              				s,
              				" cpu%d_cyc = %d, cpu%d_cnt0 = %d, cpu%d_cnt1 = %d, cpu%d_cnt2 = %d, cpu%d_cnt3 = %d, ",
              				cpu,
              				log_entry->cpu_cyc[cpu],
              				cpu,
              				log_entry->cpu_cnt0[cpu],
              				cpu,
              				log_entry->cpu_cnt1[cpu],
              				cpu,
              				log_entry->cpu_cnt2[cpu],
              				cpu,
              				log_entry->cpu_cnt3[cpu]);
        }

        trace_seq_printf(
            s,
            "BM_BCNT = %d, BM_TACT = %d, BM_TSCT = %d, ",
            log_entry->BM_BCNT,
            log_entry->BM_TACT,
            log_entry->BM_TSCT);

        trace_seq_printf(
            s,
            "BM_WACT = %d, BM_WSCT0 = %d, BM_BACT = %d, ",
            log_entry->BM_WACT,
            log_entry->BM_WSCT,
            log_entry->BM_BACT);

        trace_seq_printf(
            s,
            "BM_BSCT = %d, ",
            log_entry->BM_BSCT);

        trace_seq_printf(
            s,
            "BM_TSCT2 = %d, BM_WSCT2 = %d, ",
            log_entry->BM_TSCT2,
            log_entry->BM_WSCT2);

        trace_seq_printf(
            s,
            "BM_TSCT3 = %d, BM_WSCT3 = %d, ",
            log_entry->BM_TSCT3,
            log_entry->BM_WSCT3);

        trace_seq_printf(
            s,
            "BM_WSCT4 = %d, BM_TPCT1 = %d, ",
            log_entry->BM_WSCT4,
            log_entry->BM_TPCT1);
            
        trace_seq_printf(
            s,
            "BM_TTYPE01 = %d, BM_TTYPE09 = %d, ",
            log_entry->BM_TTYPE1, log_entry->BM_TTYPE9);

        trace_seq_printf(
            s,
            "BM_TTYPE02 = %d, BM_TTYPE10 = %d, ",
            log_entry->BM_TTYPE2, log_entry->BM_TTYPE10);
        
        trace_seq_printf(
            s,
            "BM_TTYPE03 = %d, BM_TTYPE11 = %d, ",
            log_entry->BM_TTYPE3, log_entry->BM_TTYPE11);    
        
        trace_seq_printf(
            s,
            "BM_TTYPE04 = %d, BM_TTYPE12 = %d, ",
            log_entry->BM_TTYPE4, log_entry->BM_TTYPE12);    
        
        trace_seq_printf(
            s,
            "BM_TTYPE05 = %d, BM_TTYPE13 = %d, ",
            log_entry->BM_TTYPE5, log_entry->BM_TTYPE13);    

        trace_seq_printf(
            s,
            "DRAMC_PageHit = %d, DRAMC_PageMiss = %d, DRAMC_Interbank = %d, DRAMC_Idle = %d\n",
            log_entry->DRAMC_PageHit,
            log_entry->DRAMC_PageMiss,
            log_entry->DRAMC_Interbank,
            log_entry->DRAMC_Idle);                   

    return 0;
}

static void mt65xx_mon_set_pmu(struct pmu_cfg *p_cfg)
{
	memcpy(&p_pmu->perf_cfg, p_cfg, sizeof(struct pmu_cfg));
}

static void mt65xx_mon_get_pmu(struct pmu_cfg *p_cfg)
{
	memcpy(p_cfg, &p_pmu->perf_cfg, sizeof(struct pmu_cfg));
}

static void mt65xx_mon_set_l2c(struct l2c_cfg *l_cfg)
{
}

static void mt65xx_mon_get_l2c(struct l2c_cfg *l_cfg)
{
}

static void mt65xx_mon_set_bm_rw(int type)
{
	
	if(type > BM_WRITE_ONLY) {
		printk("invalid event\n");
	} else {
		BM_SetReadWriteType(type);
	}
   
}

static ssize_t bm_master_evt_show(struct device_driver *driver, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "EMI bus monitor master = %d\n", bm_master_evt);
}

static ssize_t bm_master_evt_store(struct device_driver *driver, const char *buf, size_t count)
{
    if (!strncmp(buf, "MM1", strlen("MM1"))) {
        bm_master_evt = BM_MASTER_MM1;
    }else if (!strncmp(buf, "MM2", strlen("MM2"))) {
        bm_master_evt = BM_MASTER_MM2;
    }else if (!strncmp(buf, "MM", strlen("MM"))) {
	bm_master_evt = BM_MASTER_MM1 | BM_MASTER_MM2;
    }else if (!strncmp(buf, "APMCU", strlen("APMCU"))) {
        bm_master_evt = BM_MASTER_AP_MCU1 | BM_MASTER_AP_MCU2;
    }else if (!strncmp(buf, "MDMCU", strlen("MDMCU"))) {
        bm_master_evt = BM_MASTER_MD_MCU;
    }else if (!strncmp(buf, "2G_3G_MDDMA", strlen("2G_3G_MDDMA"))) {
        bm_master_evt = BM_MASTER_2G_3G_MDDMA;
    }else if (!strncmp(buf, "MD_ALL", strlen("MD_ALL"))) {
        bm_master_evt = BM_MASTER_MD_MCU | BM_MASTER_2G_3G_MDDMA;   
    }else if (!strncmp(buf, "GPU1", strlen("GPU1"))) {
        bm_master_evt = BM_MASTER_GPU1;
   }else if (!strncmp(buf, "GPU2", strlen("GPU2"))) {
	bm_master_evt = BM_MASTER_GPU2;
    }else if (!strncmp(buf, "ALL", strlen("ALL"))) {
        bm_master_evt = BM_MASTER_ALL;    
    }else {
        printk("invalid event\n");

        return count;
    }

    BM_SetMaster(1, bm_master_evt);

    return count;
}

static ssize_t bm_rw_type_evt_show(struct device_driver *driver, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "EMI bus read write type = %d\n", bm_rw_type_evt);
}

static ssize_t bm_rw_type_evt_store(struct device_driver *driver, const char *buf, size_t count)
{
    if (!strncmp(buf, "RW", strlen("RW"))) {
        bm_rw_type_evt = BM_BOTH_READ_WRITE;
    } else if (!strncmp(buf, "RO", strlen("RO"))) {
        bm_rw_type_evt = BM_READ_ONLY;
    } else if (!strncmp(buf, "WO", strlen("WO"))) {
        bm_rw_type_evt = BM_WRITE_ONLY;
    } else {
        printk("invalid event\n");
        return count;
    }

    BM_SetReadWriteType(bm_rw_type_evt);

    return count;
}

static ssize_t mon_mode_evt_show(struct device_driver *driver, char *buf)
{
    MonitorMode mon_mode_evt;
    mon_mode_evt = get_mt65xx_mon_mode();
	if(mon_mode_evt == MODE_MANUAL_USER)
        return snprintf(buf, PAGE_SIZE, "Monitor mode = MANUAL_USER\n");
    else if(mon_mode_evt == MODE_SCHED_SWITCH)
        return snprintf(buf, PAGE_SIZE, "Monitor mode = SCHED_SWITCH\n");
    else if(mon_mode_evt == MODE_PERIODIC)
        return snprintf(buf, PAGE_SIZE, "Monitor mode = PERIODIC\n");
    else if(mon_mode_evt == MODE_MANUAL_TRACER)
        return snprintf(buf, PAGE_SIZE, "Monitor mode = MANUAL_TRACER\n");  
    else if(mon_mode_evt == MODE_MANUAL_KERNEL)
        return snprintf(buf, PAGE_SIZE, "Monitor mode = MANUAL_KERNEL\n");
    else if(mon_mode_evt == MODE_FREE)          
    	return snprintf(buf, PAGE_SIZE, "Monitor mode = FREE\n");
    else
    	return snprintf(buf, PAGE_SIZE, "Monitor mode = Unknown\n");
}

static ssize_t mon_mode_evt_store(struct device_driver *driver, const char *buf, size_t count)
{
	MonitorMode mon_mode_evt;
	if (!strncmp(buf, "SCHED_SWITCH", strlen("SCHED_SWITCH"))) {
        mon_mode_evt = MODE_SCHED_SWITCH;
    } else if (!strncmp(buf, "PERIODIC", strlen("PERIODIC"))) {
        mon_mode_evt = MODE_PERIODIC;
    } else if (!strncmp(buf, "MANUAL_TRACER", strlen("MANUAL_TRACER"))) {
        mon_mode_evt = MODE_MANUAL_TRACER;
    } else {
        printk("invalid event\n");
        return count;
    }
    
    set_mt65xx_mon_mode(mon_mode_evt);
    
    return count;
}

static ssize_t mon_period_evt_show(struct device_driver *driver, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "Monitor period = %ld (for periodic mode)\n", get_mt65xx_mon_period());    
}

static ssize_t mon_period_evt_store(struct device_driver *driver, const char *buf, size_t count)
{
    
    sscanf(buf, "%ld", &mon_period_evt);    
    set_mt65xx_mon_period(mon_period_evt);
    
    return count;
}

static ssize_t mon_manual_evt_show(struct device_driver *driver, char *buf)
{
    mon_manual_evt = get_mt65xx_mon_manual_start();
    if(mon_manual_evt == 1)
        return snprintf(buf, PAGE_SIZE, "Manual Monitor is Started (for manual mode)\n");
    else if(mon_manual_evt == 0)
        return snprintf(buf, PAGE_SIZE, "Manual Monitor is Stopped (for manual mode)\n");
    else
        return 0;
}

static ssize_t mon_manual_evt_store(struct device_driver *driver, const char *buf, size_t count)
{
    
    if (!strncmp(buf, "START", strlen("START"))) {
        mon_manual_evt = 1;
    } else if (!strncmp(buf, "STOP", strlen("STOP"))) {
        mon_manual_evt = 0;
    } else {
        printk("invalid event\n");
        return count;
    }
    
    set_mt65xx_mon_manual_start(mon_manual_evt);
    
    return count;
}

static ssize_t cpu_pmu_evt_show(struct device_driver *driver, char *buf)   
{   

	u32 j;
	int size = 0;
	
	if(p_pmu == NULL) {
		printk("PMU user interface isn't ready now!\n"); 
		return 0;
	}	
	
	for(j = 0; j < NUMBER_OF_EVENT; j++) {
		size += sprintf(buf + size, "Evt%d = 0x%x\t", j, p_pmu->perf_cfg.event_cfg[j]);	
	}
	size += sprintf(buf + size, "\n");
	
    return (size); 
}
 
static ssize_t cpu_pmu_evt_store(struct device_driver *driver, const char *buf, size_t count) 
{   
    	
    char *p = (char *)buf;
    char *token[8];
    char *ptr;
    int i = 0;

    if((strlen(buf)+1) > 128)
    {
        printk("[PMU] command overflow!");
        return -1;
    }

    do{
        ptr = strsep (&p, " ");
        token[i] = ptr;
        i++;
    }while(ptr != NULL);

    //because we use i to count num of evt setting, i is NUMBER_OF_EVENT+1
    if(i != (NUMBER_OF_EVENT+1)){
        printk("[PMU]The number of parameter is wrong!!! Please echo ");
        for(i = 0; i < NUMBER_OF_EVENT; i++)
            printk("Evt%d ",i);
        printk("> cpu_pmu_cfg\n");
        return -1;
    }


    for(i = 0; i < NUMBER_OF_EVENT; i++)
        p_pmu->perf_cfg.event_cfg[i] =  simple_strtoul(token[i], &token[i], 16);


    for(i=0; i<NUMBER_OF_EVENT; i++)
        printk("%x  ",p_pmu->perf_cfg.event_cfg[i]);
    printk("\n");
    p_pmu->enable();

    return count;

}

static ssize_t emi_dcm_ctrl_show(struct device_driver *driver, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "EMI DCM is %s\n", BM_GetEmiDcm() ? "OFF" : "ON");
}

static ssize_t emi_dcm_ctrl_store(struct device_driver *driver, const char *buf, size_t count)
{ 
    if (!strncmp(buf, "OFF", strlen("OFF"))) {
        BM_SetEmiDcm(0xff);
    } else if (!strncmp(buf, "ON", strlen("ON"))) {
        BM_SetEmiDcm(0x0);
    } else {
        printk("invalid event\n");
    }
    
    return count;
}

#if 0
static ssize_t mci_evt_show(struct device_driver *driver, char *buf)
{
  MCI_Event_Read();
  return;
}

ssize_t mci_evt_store(struct device_driver *driver, const char *buf, size_t count) 
{
  unsigned int evt0, evt1;
  if (sscanf(buf, "%x %x", &evt0, &evt1) != 2)
		return -EINVAL;
  MCI_Event_Set(evt0, evt1); 

  return count;
}
#endif

DRIVER_ATTR(bm_master_evt, 0644, bm_master_evt_show, bm_master_evt_store);
DRIVER_ATTR(bm_rw_type_evt, 0644, bm_rw_type_evt_show, bm_rw_type_evt_store);

DRIVER_ATTR(mon_mode_evt, 0644, mon_mode_evt_show, mon_mode_evt_store);
DRIVER_ATTR(mon_period_evt, 0644, mon_period_evt_show, mon_period_evt_store);
DRIVER_ATTR(mon_manual_evt, 0644, mon_manual_evt_show, mon_manual_evt_store);

DRIVER_ATTR(cpu_pmu_cfg,	0644, cpu_pmu_evt_show,		cpu_pmu_evt_store);
DRIVER_ATTR(emi_dcm_ctrl,	0644, emi_dcm_ctrl_show,	emi_dcm_ctrl_store);

//DRIVER_ATTR(mci_evt, 0644, mci_evt_show, mci_evt_store);


static struct device_driver mt_mon_drv = 
{
    .name = "mt_monitor",
    .bus = &platform_bus_type,
    .owner = THIS_MODULE,
};


struct mtk_monitor mtk_mon = {
        .init			= mt65xx_mon_init,
        .deinit			= mt65xx_mon_deinit,
        .enable			= mt65xx_mon_enable,
        .disable		= mt65xx_mon_disable,
        .mon_log		= mt65xx_mon_log,
        .set_pmu		= mt65xx_mon_set_pmu,
        .get_pmu		= mt65xx_mon_get_pmu,
        .set_l2c		= mt65xx_mon_set_l2c,
        .get_l2c		= mt65xx_mon_get_l2c,
		.set_bm_rw		= mt65xx_mon_set_bm_rw,
};

/*
 * mt_mon_mod_init: module init function
 */
static int __init mt_mon_mod_init(void)
{
    int ret;
	
    /* register driver and create sysfs files */
    ret = driver_register(&mt_mon_drv);

    if (ret) {
        printk("fail to register mt_mon_drv\n");
        return ret;
    }
    ret = driver_create_file(&mt_mon_drv, &driver_attr_bm_master_evt);
    ret |= driver_create_file(&mt_mon_drv, &driver_attr_bm_rw_type_evt);
    ret |= driver_create_file(&mt_mon_drv, &driver_attr_mon_mode_evt);
    ret |= driver_create_file(&mt_mon_drv, &driver_attr_mon_period_evt);    
    ret |= driver_create_file(&mt_mon_drv, &driver_attr_mon_manual_evt);
    ret |= driver_create_file(&mt_mon_drv, &driver_attr_cpu_pmu_cfg);
    ret |= driver_create_file(&mt_mon_drv, &driver_attr_emi_dcm_ctrl);
//    ret |= driver_create_file(&mt_mon_drv, &driver_attr_mci_evt);

    if (ret) {
        printk("fail to create mt_mon sysfs files\n");

        return ret;
    }

    /* SPNIDEN[12] must be 1 for using ARM11 performance monitor unit */
//    *(volatile unsigned int *)0xF702A000 |= 0x1000;


	ret = register_pmu(&p_pmu);
	if(ret != 0 || p_pmu == NULL) {
		printk("Register PMU Fail\n");
		return ret;
	}
	

    /* init EMI bus monitor */
    BM_SetReadWriteType(DEF_BM_RW_TYPE);
    BM_SetMonitorCounter(1, BM_MASTER_MM1|BM_MASTER_MM2, BM_TRANS_TYPE_4BEAT | BM_TRANS_TYPE_8Byte | BM_TRANS_TYPE_BURST_WRAP);
    BM_SetMonitorCounter(2, BM_MASTER_AP_MCU1|BM_MASTER_AP_MCU2, BM_TRANS_TYPE_4BEAT | BM_TRANS_TYPE_8Byte | BM_TRANS_TYPE_BURST_WRAP);
    BM_SetMonitorCounter(3, BM_MASTER_MD_MCU | BM_MASTER_2G_3G_MDDMA, BM_TRANS_TYPE_4BEAT | BM_TRANS_TYPE_8Byte | BM_TRANS_TYPE_BURST_WRAP);
    BM_SetMonitorCounter(4, BM_MASTER_GPU1|BM_MASTER_GPU2, BM_TRANS_TYPE_4BEAT | BM_TRANS_TYPE_8Byte | BM_TRANS_TYPE_BURST_WRAP);
    BM_SetLatencyCounter();

    /*select MCI monitor event*/
    //MCI_Event_Set(0x6, 0x7); /*0x6: Si0 AR input queue full; 0x7: Si0 AW input queue full*/

    return 0;
}

arch_initcall (mt_mon_mod_init);

static DEFINE_SPINLOCK(reg_mon_lock);
int register_monitor(struct mtk_monitor **p_mon, MonitorMode mode)
{
	int ret = 0;
	
	spin_lock(&reg_mon_lock);
	
  mt_kernel_ring_buff_index = 0;
  mt_mon_log_buff_index = 0;
	
	if(register_mode != MODE_FREE) {
        goto _err;
    } else {
		register_mode = mode;
		*p_mon = &mtk_mon;
	}	
	
    if(mode == MODE_MANUAL_USER || mode == MODE_MANUAL_KERNEL) {
        if (!mt_mon_log_buff) {
            mt_mon_log_buff = vmalloc(sizeof(struct mt_mon_log) * MON_LOG_BUFF_LEN);        
            if (!mt_mon_log_buff) {
                printk(KERN_WARNING "fail to allocate the buffer for the monitor log\n");
                register_mode = MODE_FREE;
                goto _err;
            }
            mtk_mon.log_buff = mt_mon_log_buff;
        }    
    }    

	if(mode == MODE_SCHED_SWITCH)
		p_pmu->multicore = 0;
	else 
		p_pmu->multicore = 1;
		
	spin_unlock(&reg_mon_lock);
	
	return ret;

_err:
    p_mon = NULL;
    spin_unlock(&reg_mon_lock);
    return -1;    
}   
EXPORT_SYMBOL(register_monitor);

void unregister_monitor(struct mtk_monitor **p_mon)
{
	
	spin_lock(&reg_mon_lock);
	
    if(mt_mon_log_buff) {
        vfree(mt_mon_log_buff);
        mt_mon_log_buff = NULL;
    }
    
	register_mode = MODE_FREE;
	*p_mon = NULL;

    mt_kernel_ring_buff_index = 0;
    mt_mon_log_buff_index = 0;
	spin_unlock(&reg_mon_lock);
}
EXPORT_SYMBOL(unregister_monitor);
