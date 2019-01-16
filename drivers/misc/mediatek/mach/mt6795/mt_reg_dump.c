#include "mach/mt_reg_base.h"
#include "mach/mt_reg_dump.h"
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <mach/sync_write.h>
#include <mach/dbg_dump.h>
#include <linux/kallsyms.h>
#include <linux/init.h>

struct reg_dump_driver_data reg_dump_driver_data =
{
    .mcu_regs = (MCUCFG_BASE),
};

static struct platform_device reg_dump_device = 
{    
    .name = "dbg_reg_dump",
    .dev = 
    {
        .platform_data = &(reg_dump_driver_data),
    },
};

/*
 * mt_reg_dump_init: initialize driver.
 * Always return 0.
 */

static int __init mt_reg_dump_init(void)
{
    int err;
    
    err = platform_device_register(&(reg_dump_device));
    if (err) {
        pr_err("Fail to register reg_dump_device");
        return err;
    }  
    
  return 0;
}

#define	LASTPC					0X20
#define	LASTSP					0X24
#define	LASTFP  0X28
#define	MUX_CONTOL_CA7_REG		(MCUCFG_BASE + 0x140)
#define	MUX_READ_CA7_REG		(MCUCFG_BASE + 0x144)
#define	MUX_CONTOL_CA17_REG		(MCUCFG_BASE + 0x21C)
#define	MUX_READ_CA17_REG		(MCUCFG_BASE + 0x25C)

static const LASTPC_MAGIC_NUM[] = {0X3, 0XB, 0X33, 0X43};

int reg_dump_platform(char *buf)
{
  /* Get core numbers */
    int ret = -1, cnt = num_possible_cpus();
    char *ptr = buf;
    unsigned int pc_value;
    unsigned int pc_i1_value;
    unsigned int fp_value;
    unsigned int sp_value;
    unsigned long size = 0;
    unsigned long offset = 0;
    char str[KSYM_SYMBOL_LEN];
    int i;
    int cluster, cpu_in_cluster;
    
    if(cnt < 0)
        return ret;

    /* Get PC, FP, SP and save to buf */
    for (i = 0; i < cnt; i++) {
        cluster = i / 4;
        cpu_in_cluster = i % 4;
        if(cluster == 0) {
            writel(LASTPC + i, MUX_CONTOL_CA7_REG);
            pc_value = readl(MUX_READ_CA7_REG);
            writel(LASTSP + i, MUX_CONTOL_CA7_REG);
            sp_value = readl(MUX_READ_CA7_REG);
            writel(LASTFP + i, MUX_CONTOL_CA7_REG);
            fp_value = readl(MUX_READ_CA7_REG);
            kallsyms_lookup((unsigned long)pc_value, &size, &offset, NULL, str);
            ptr += sprintf(ptr, "CORE_%d PC = 0x%x(%s + 0x%lx), FP = 0x%x, SP = 0x%x\n", i, pc_value, str, offset, fp_value, sp_value);
        }
        else{
            writel(LASTPC_MAGIC_NUM[cpu_in_cluster], MUX_CONTOL_CA17_REG);
            pc_value = readl(MUX_READ_CA17_REG);
            writel(LASTPC_MAGIC_NUM[cpu_in_cluster] + 1, MUX_CONTOL_CA17_REG);
            pc_i1_value = readl(MUX_READ_CA17_REG);
            ptr += sprintf(ptr, "CORE_%d PC_i0 = 0x%x, PC_i1 = 0x%x\n", i, pc_value, pc_i1_value);
        }
    }

    //printk("CORE_%d PC = 0x%x(%s), FP = 0x%x, SP = 0x%x\n", i, pc_value, str, fp_value, sp_value);
    return 0;

}
arch_initcall(mt_reg_dump_init);
