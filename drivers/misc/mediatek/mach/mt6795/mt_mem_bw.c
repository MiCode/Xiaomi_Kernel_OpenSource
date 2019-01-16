#include <linux/device.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include "mach/mt_emi_bm.h"
#include "mach/mt_dcm.h"
#include "mach/mt_mem_bw.h"
#include <asm/div64.h>  

unsigned long long last_time_ns;
unsigned long long LastWordAllCount=0;
unsigned long long get_mem_bw(void)
{
    unsigned long long throughput;
    unsigned long long WordAllCount;
    unsigned long long current_time_ns, time_period_ns;
    int count, value;
    int emi_dcm_disable = BM_GetEmiDcm();

#if DISABLE_FLIPPER_FUNC    
return 0;
#endif

    //printk("[get_mem_bw]emi_dcm_disable = %d\n", emi_dcm_disable);
    current_time_ns = sched_clock();
    time_period_ns = current_time_ns - last_time_ns;
    //printk("[get_mem_bw]last_time=%llu, current_time=%llu, period=%llu\n", last_time_ns, current_time_ns, time_period_ns);
    
    //disable_infra_dcm();
    BM_SetEmiDcm(0xff); //disable EMI dcm
        
    BM_Pause();
    WordAllCount = BM_GetWordAllCount();
    if(WordAllCount == 0) 
        LastWordAllCount = 0;

    WordAllCount -= LastWordAllCount;
    throughput = (WordAllCount * 8 * 1000);
    do_div(throughput,time_period_ns);
    //printk("[get_mem_bw]Total MEMORY THROUGHPUT =%llu(MB/s), WordAllCount_delta = 0x%llx, LastWordAllCount = 0x%llx\n",throughput, WordAllCount, LastWordAllCount);

    // stopping EMI monitors will reset all counters
    BM_Enable(0);
    
    value = BM_GetWordAllCount();
    count = 100;
    if((value != 0) && (value > 0xB0000000))
    {  
      do
      {
        if((value = BM_GetWordAllCount()) != 0)
        {
          count--;
          BM_Enable(1);
          BM_Enable(0);
        }
        else
        {
          break;
        }
      }while(count > 0);      
    }
    LastWordAllCount = value;
    
    //printk("[get_mem_bw]loop count = %d, last_word_all_count = 0x%x\n", count, LastWordAllCount);
   
    // start EMI monitor counting
    BM_Enable(1);
    last_time_ns = sched_clock();
    
    //restore_infra_dcm();      
    BM_SetEmiDcm(emi_dcm_disable); 
    
    //printk("[get_mem_bw]throughput = %llx\n", throughput); 
    
    return throughput;
}

static int __init mon_kernel_init(void)
{
    int emi_dcm_disable;
    
    BM_Init();

    disable_infra_dcm();                 
    emi_dcm_disable = BM_GetEmiDcm();
    //printk("[MT_MEM_BW]emi_dcm_disable = %d\n", emi_dcm_disable);
    BM_SetEmiDcm(0xff); //disable EMI dcm
        	
    BM_SetReadWriteType(BM_BOTH_READ_WRITE);
    BM_SetMonitorCounter(1, BM_MASTER_MM1 | BM_MASTER_MM1, BM_TRANS_TYPE_4BEAT | BM_TRANS_TYPE_8Byte | BM_TRANS_TYPE_BURST_WRAP);
    BM_SetMonitorCounter(2, BM_MASTER_AP_MCU1 | BM_MASTER_AP_MCU2, BM_TRANS_TYPE_4BEAT | BM_TRANS_TYPE_8Byte | BM_TRANS_TYPE_BURST_WRAP);
    BM_SetMonitorCounter(3, BM_MASTER_MD_MCU | BM_MASTER_2G_3G_MDDMA, BM_TRANS_TYPE_4BEAT | BM_TRANS_TYPE_8Byte | BM_TRANS_TYPE_BURST_WRAP);
    BM_SetMonitorCounter(4, BM_MASTER_GPU1 | BM_MASTER_GPU1, BM_TRANS_TYPE_4BEAT | BM_TRANS_TYPE_8Byte | BM_TRANS_TYPE_BURST_WRAP);

    BM_SetLatencyCounter();

     // stopping EMI monitors will reset all counters
    BM_Enable(0);
     // start EMI monitor counting
    BM_Enable(1);
    last_time_ns = sched_clock();

    restore_infra_dcm();    
    BM_SetEmiDcm(emi_dcm_disable); //enable EMI dcm
      
    return 0;
}

static void __exit mon_kernel_exit(void)
{
    return;
}

module_init(mon_kernel_init);
module_exit(mon_kernel_exit);

