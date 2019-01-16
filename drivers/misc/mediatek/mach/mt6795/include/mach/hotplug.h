#ifndef _HOTPLUG
#define _HOTPLUG


#include <linux/xlog.h>
#include <linux/kernel.h>   //printk
#include <asm/atomic.h>
#include <mach/mt_reg_base.h>
#include <mach/sync_write.h>


/* log */
#define HOTPLUG_LOG_NONE                                0
#define HOTPLUG_LOG_WITH_XLOG                           1
#define HOTPLUG_LOG_WITH_PRINTK                         2

#define HOTPLUG_LOG_PRINT                               HOTPLUG_LOG_WITH_PRINTK

#if (HOTPLUG_LOG_PRINT == HOTPLUG_LOG_NONE)
#define HOTPLUG_INFO(fmt, args...)                    
#elif (HOTPLUG_LOG_PRINT == HOTPLUG_LOG_WITH_XLOG)
#define HOTPLUG_INFO(fmt, args...)                      xlog_printk(ANDROID_LOG_INFO, "Power/hotplug", fmt, ##args)
#elif (HOTPLUG_LOG_PRINT == HOTPLUG_LOG_WITH_PRINTK)
#define HOTPLUG_INFO(fmt, args...)                      pr_warn("[Power/hotplug] "fmt, ##args)
//#define HOTPLUG_INFO(fmt, args...)                      pr_emerg("[Power/hotplug] "fmt, ##args)

#define STEP_BY_STEP_DEBUG                              pr_emerg("@@@### file:%s, func:%s, line:%d ###@@@\n", __FILE__, __func__, __LINE__)
#endif


/* profilling */
//#define CONFIG_HOTPLUG_PROFILING                        
#define CONFIG_HOTPLUG_PROFILING_COUNT                  100


/* register address - bootrom power*/
#define BOOTROM_BOOT_ADDR                               (INFRACFG_AO_BASE + 0x800)
#define BOOTROM_SEC_CTRL                                (INFRACFG_AO_BASE + 0x804)
#define SW_ROM_PD                                       (1U << 31)


/* register address - CCI400 */
#define CCI400_STATUS                                   (CCI400_BASE + 0x000C)
#define CHANGE_PENDING                                  (1U << 0)
//cluster 0
#define CCI400_SI4_BASE                                 (CCI400_BASE + 0x5000)
#define CCI400_SI4_SNOOP_CONTROL                        (CCI400_SI4_BASE)
//cluster 1
#define CCI400_SI3_BASE                                 (CCI400_BASE + 0x4000)
#define CCI400_SI3_SNOOP_CONTROL                        (CCI400_SI3_BASE)
#define DVM_MSG_REQ                                     (1U << 1)
#define SNOOP_REQ                                       (1U << 0)


/* register address - cross trigger */
#define MCUSYS_CONFIG                                   (MCUCFG_BASE + 0x001C)
#define CA7_EVENTI_SEL                                  (1 << 14)
#define CA15_EVENTI_SEL                                 (1 << 15)


/* register address - acinactm */
//cluster 0
#define BUS_CONFIG                                      (CA7MCUCFG_BASE + 0x1C)
#define CA7_ACINACTM                                    (1U << 4)
//cluster 1
#define MISCDBG                                         (CA15L_CONFIG_BASE + 0x0C)
#define CA15L_AINACTS                                   (1U << 4)
#define CA15L_ACINACTM                                  (1U << 0)


/* register address - disable rguX reset wait for cpuX L1 pdn ack */
//cluster 1
#define CONFIG_RES                                      (CA15L_CONFIG_BASE + 0x68)


/* register address - debug monitor */
//cluster 0
#define DBG_MON_CTL                                     (CA7MCUCFG_BASE + 0x40)
#define DBG_MON_DATA                                    (CA7MCUCFG_BASE + 0x44)
//cluster 1
#define CA15L_MON_SEL                                   (CA15L_CONFIG_BASE + 0x1C)
#define CA15L_MON                                       (CA15L_CONFIG_BASE + 0x5C)


/* register address - e2 eco delay IP shift fail */
#define CA15L_CLKENM_DIV                                (CA15L_CONFIG_BASE + 0x48)
#define FORCE_SHIFT                                     (1U << 12)


/* register address - L2 reset disable */
#define CA7_CACHE_CONFIG                                (CA7MCUCFG_BASE + 0x0)
#define CA7_RG_L2RSTDISABLE                             (1U << 4)
#define CA15L_RST_CTL                                   (CA15L_CONFIG_BASE + 0x44)
#define CA15L_L2RSTDISABLE                              (1U << 14)


/* register read/write */
#define REG_READ(addr)                                  (*(volatile u32 *)(addr))
#define REG_WRITE(addr, value)                          mt_reg_sync_writel(value, addr)


/* power on/off cpu*/
//XXX: for early porting, don't define CONFIG_HOTPLUG_WITH_POWER_CTRL for cpu hotplug with WFI
#define CONFIG_HOTPLUG_WITH_POWER_CTRL


/* global variable */
extern atomic_t hotplug_cpu_count;


/* mt cpu hotplug callback for smp_operations */
extern int mt_cpu_kill(unsigned int cpu);
extern void mt_cpu_die(unsigned int cpu);
extern int mt_cpu_disable(unsigned int cpu);

#endif //enf of #ifndef _HOTPLUG
