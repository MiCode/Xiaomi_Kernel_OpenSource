#include <linux/workqueue.h>
#include <linux/aee.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/aee.h>
#include <linux/interrupt.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_reg_base.h>
#include <mach/sync_write.h>
#include <mach/md32_ipi.h>
#include <mach/md32_helper.h>
//#include <mach/md32_excep.h>
#include "md32_irq.h"

//FIXME: This setting should be included from mt_irqs.h
//#define MT_IPC_MD2HOST_INTR    198
static md32_wdt_func MD32_WDT_FUN;
static md32_assert_func  MD32_ASSERT_FUN;

static struct work_struct work_md32_wdt;
static struct work_struct work_md32_assert;
static struct workqueue_struct *wq_md32_reboot;
static struct workqueue_struct *wq_md32_wdt;
static struct workqueue_struct *wq_md32_assert;

struct md32_aed_cfg {
    int *log;
    int log_size;
    int *phy;
    int phy_size;
    char *detail;
};

struct md32_reboot_work {
    struct work_struct work;
    struct md32_aed_cfg aed;
};

static struct md32_reboot_work work_md32_reboot;

#define MD32_AP_DEBUG  1
#if(MD32_AP_DEBUG == 1)
#define dbg_msg pr_debug
#else
#define dbg_msg(...)
#endif

void md32_wdt_reset_func(struct work_struct *ws)
{
    int index;
#define TEST_PHY_SIZE 0x10000
    pr_crit("reset_md32_func\n");

    //1. AEE function
    aee_kernel_exception("MD32","MD32 WDT Time out ");
    //2. Reset md32
    pr_debug("WDT Reset md32 ok!\n");
    //3. Call driver's callback
    for(index=0;index<MD32_MAX_USER;index++)
    {
        if(MD32_WDT_FUN.in_use[index] && MD32_WDT_FUN.reset_func[index]!=NULL )
        {
            MD32_WDT_FUN.reset_func[index](MD32_WDT_FUN.private_data[index]);
        }
    }
}

void md32_assert_reset_func(struct work_struct *ws)
{
    int index;
    pr_crit("reset_md32_func\n");
    //1. AEE function
    aee_kernel_exception("MD32","MD32 ASSERT ");
    //2. Reset md32
    pr_crit("ASSERT Reset md32 ok!\n");
    //3. Call driver's callback
    for(index=0;index<MD32_MAX_USER;index++)
    {
        if(MD32_ASSERT_FUN.in_use[index] && MD32_ASSERT_FUN.reset_func[index]!=NULL )
        {
            MD32_ASSERT_FUN.reset_func[index](MD32_ASSERT_FUN.private_data[index]);
        }
    }
}

int md32_dump_regs(char *buf)
{
    volatile unsigned long *reg;
    char *ptr = buf;

    if(!buf)
        return 0;
    //dbg_msg("md32_dump_regs\n");
    ptr += sprintf(ptr, "md32 pc=0x%08x, r14=0x%08x, r15=0x%08x\n", MD32_DEBUG_PC_REG, MD32_DEBUG_R14_REG, MD32_DEBUG_R15_REG);
    ptr += sprintf(ptr, "md32 to host inerrupt = 0x%08x\n", MD32_TO_HOST_REG);

    ptr += sprintf(ptr, "wdt en=%d, count=0x%08x\n",
           (MD32_WDT_REG & 0x10000000) ? 1 : 0,
           (MD32_WDT_REG & 0xFFFFF));

    /*dump all md32 regs*/
    for(reg = MD32_BASE;
        (unsigned long)reg < (unsigned long)(MD32_BASE + 0x90); reg++)
    {
        if(!((unsigned long)reg & 0xF))
        {
            ptr += sprintf(ptr, "\n");
            ptr += sprintf(ptr, "[0x%016lx]   ", (unsigned long)reg); //address
        }

        ptr += sprintf(ptr, "0x%016lx  ", *reg); //values

    }
    ptr += sprintf(ptr, "\n");

    return ptr - buf;
}

void md32_prepare_aed(char *aed_str, struct md32_aed_cfg *aed)
{
	char *detail;
    u8 *log, *phy, *ptr;
    u32 log_size, phy_size;

    detail = (char *)kmalloc(MD32_AED_STR_LEN, GFP_KERNEL);

    if(!detail)
    {
        pr_err("ap allocate buffer fail, size=0x%x\n", MD32_AED_STR_LEN);
    }
    else
    {
        ptr = detail;
        detail[MD32_AED_STR_LEN - 1] = '\0';
        ptr += snprintf(detail, MD32_AED_STR_LEN, "%s", aed_str);
        ptr += sprintf(ptr, "md32 pc=0x%08x, r14=0x%08x, r15=0x%08x\n", MD32_DEBUG_PC_REG, MD32_DEBUG_R14_REG, MD32_DEBUG_R15_REG);
    }

    phy_size = MD32_AED_PHY_SIZE;

    phy = (u8 *)kmalloc(phy_size, GFP_KERNEL);

    if(!phy)
    {
        pr_err("ap allocate buffer fail\n");
        phy_size = 0;
    }
    else
    {
        ptr = phy;
        memcpy((void *)ptr, (void *)MD32_BASE, MD32_CFGREG_SIZE);

        ptr += MD32_CFGREG_SIZE;

        memcpy((void *)ptr, (void *)MD32_PTCM, MD32_PTCM_SIZE);

        ptr += MD32_PTCM_SIZE;

        memcpy((void *)ptr, (void *)MD32_DTCM, MD32_DTCM_SIZE);

        ptr += MD32_DTCM_SIZE;
    }

    log_size = 0x10000; /* 64K for try*/
    log = (u8 *)kmalloc(log_size, GFP_KERNEL);
    if(!log)
    {
        pr_err("ap allocate buffer fail\n");
        log_size = 0;
    }
    else
    {
        int size;
        memset(log, 0, log_size);

        ptr = log;

        ptr += md32_dump_regs(ptr);

        /* print log in kernel */
        pr_debug("%s", log);


        ptr += sprintf(ptr, "dump memory info\n");
        ptr += sprintf(ptr, "md32 cfgreg: 0x%08x\n", 0);
        ptr += sprintf(ptr, "md32 ptcm  : 0x%08x\n", MD32_CFGREG_SIZE);
        ptr += sprintf(ptr, "md32 dtcm  : 0x%08x\n", MD32_CFGREG_SIZE + MD32_PTCM_SIZE);
        ptr += sprintf(ptr, "<<md32 log buf>>\n");
        size = log_size - (ptr - log);
        ptr += md32_get_log_buf(ptr, size);
        *ptr = '\0';
        log_size = ptr - log;

    }

    aed->log = (int *)log;
    aed->log_size = log_size;
    aed->phy = (int *)phy;
    aed->phy_size = phy_size;
    aed->detail = detail;

}

void md32_dmem_abort_handler(void)
{
    dbg_msg("[MD32] DMEM Abort\n");
}
void md32_pmem_abort_handler(void)
{
    dbg_msg("[MD32] PMEM Abort\n");
}

void md32_wdt_handler(void)
{

    int index;
    dbg_msg("In wdt_isr\n");

    for(index=0;index<MD32_MAX_USER;index++)
    {
        dbg_msg("in use - index %d,%d\n",index,MD32_WDT_FUN.in_use[index]);
        if((MD32_WDT_FUN.in_use[index]==1) && (MD32_WDT_FUN.wdt_func[index]!=NULL) )
        {
            dbg_msg("do call back - index %d\n",index);
            MD32_WDT_FUN.wdt_func[index](MD32_WDT_FUN.private_data[index]);
        }
    }
    //queue_work(wq_md32_wdt,&work_md32_wdt);
}


int alloc_md32_assert_func(void)
{
    int index;
    for(index=0;index<MD32_MAX_USER;index++)
    {
        if(!MD32_ASSERT_FUN.in_use[index])
        {
            MD32_ASSERT_FUN.in_use[index]=1;
            return index;
        }
    }
    return -1;
}

int alloc_md32_wdt_func(void)
{
    int index;
    for(index=0;index<MD32_MAX_USER;index++)
    {
        if(!MD32_WDT_FUN.in_use[index])
        {
            MD32_WDT_FUN.in_use[index]=1;
            return index;
        }
    }
    return -1;
}

int md32_wdt_register_handler_services( void MD32_WDT_FUNC_PTR(void*),void WDT_RESET(void*), void* private_data, char *module_name)
{
    int index_cur;
    index_cur=alloc_md32_wdt_func();
    dbg_msg("wdt register index %d\n",index_cur);
    if(index_cur<0)
    {
        dbg_msg("MD32_WDT_FUNC is full");
        return -1;
    }
    MD32_WDT_FUN.wdt_func[index_cur]=MD32_WDT_FUNC_PTR;
    MD32_WDT_FUN.reset_func[index_cur]=WDT_RESET;
    MD32_WDT_FUN.private_data[index_cur]=private_data;
    strcpy(MD32_WDT_FUN.MODULE_NAME[index_cur],module_name);
    return 1;
}

int md32_assert_register_handler_services( void MD32_ASSERT_FUNC_PTR(void*),void ASSERT_RESET(void *), void* private_data, char *module_name)
{
    int index_cur;
    index_cur=alloc_md32_assert_func();
    dbg_msg("assert register index %d\n",index_cur);
    if(index_cur<0)
    {
        dbg_msg("MD32_ASSERT_FUNC is full");
        return -1;
    }
    MD32_ASSERT_FUN.assert_func[index_cur]=MD32_ASSERT_FUNC_PTR;
    MD32_ASSERT_FUN.reset_func[index_cur]=ASSERT_RESET;
    MD32_ASSERT_FUN.private_data[index_cur]=private_data;
    strcpy(MD32_ASSERT_FUN.MODULE_NAME[index_cur],module_name);
    return 1;
}


int free_md32_wdt_func(char *module_name)
{
    int index;
    dbg_msg("Flush works in WDT work queue\n");
    flush_workqueue(wq_md32_wdt);
    dbg_msg("Free md32_wdt structure\n");
    for(index=0;index<MD32_MAX_USER;index++)
    {
        if(strcmp(module_name,MD32_WDT_FUN.MODULE_NAME[index])==0)
        {
            MD32_WDT_FUN.in_use[index]=0;
            MD32_WDT_FUN.wdt_func[index]=NULL;
            MD32_WDT_FUN.reset_func[index]=NULL;
            MD32_WDT_FUN.private_data[index]=NULL;
            return 0;
        }
    }
    dbg_msg("Can't free %s\n",module_name);
    return -1;
}

int free_md32_assert_func(char *module_name)
{
    int index;
    dbg_msg("Flush works in ASSERT work queue\n");
    flush_workqueue(wq_md32_assert);
    dbg_msg("Free md32_assert structure\n");
    for(index=0;index<MD32_MAX_USER;index++)
    {
        if(strcmp(module_name,MD32_ASSERT_FUN.MODULE_NAME[index])==0)
        {
            MD32_ASSERT_FUN.in_use[index]=0;
            MD32_ASSERT_FUN.assert_func[index]=NULL;
            MD32_ASSERT_FUN.reset_func[index]=NULL;
            MD32_ASSERT_FUN.private_data[index]=NULL;
            return 0;
        }
    }
    dbg_msg("Can't free %s\n",module_name);
    return -1;
}


irqreturn_t md32_irq_handler(int irq, void *dev_id)
{
    struct reg_md32_to_host_ipc *md32_irq;
    int reboot = 0;

    md32_irq = (struct reg_md32_to_host_ipc *)MD32_TO_HOST_ADDR;

    if(md32_irq->wdt_int)
    {
        md32_prepare_aed("md32 wdt", &work_md32_reboot.aed);
        reboot = 1;
        md32_wdt_handler();
        md32_irq->wdt_int = 0;
    }

    if(md32_irq->pmem_disp_int)
    {
        md32_prepare_aed("md32 pmem abort", &work_md32_reboot.aed);
        md32_pmem_abort_handler();
        md32_irq->pmem_disp_int = 0;
        //reboot = 1;
    }

    if(md32_irq->dmem_disp_int)
    {
        md32_dmem_abort_handler();
        md32_irq->dmem_disp_int = 0;
        //reboot = 1;
    }

    if(md32_irq->md32_ipc_int)
    {
        md32_ipi_handler();
        md32_irq->ipc_md2host = 0;
        md32_irq->md32_ipc_int = 0;
    }

    MD32_TO_HOST_REG = 0x0;

    if(reboot)
    {
        queue_work(wq_md32_reboot, (struct work_struct *)&work_md32_reboot);
    }

    return IRQ_HANDLED;
}

void md32_reboot_from_irq(struct work_struct * ws)
{
    struct md32_reboot_work *rb_ws = (struct md32_reboot_work *)ws;
    struct md32_aed_cfg *aed = &rb_ws->aed;


    pr_debug("%s", aed->detail);

    aed_md32_exception_api(aed->log, aed->log_size, aed->phy, aed->phy_size, aed->detail, DB_OPT_DEFAULT);

    if(aed->detail)
        kfree(aed->detail);
    if(aed->phy)
        kfree(aed->phy);
    if(aed->log)
        kfree(aed->log);
#if 0
    if(reboot_load_md32() < 0)
    {
        pr_err("Reboot MD32 Fail\n");
    }
#endif
}

void md32_irq_init(void)
{

    MD32_TO_HOST_REG = 0; //clear md32 irq

    //1. Reset Structures
    memset(&MD32_WDT_FUN,0,sizeof(MD32_WDT_FUN));
    memset(&MD32_ASSERT_FUN,0,sizeof(MD32_ASSERT_FUN));

    wq_md32_reboot = create_workqueue("MD32_REBOOT_WQ");
    wq_md32_wdt    = create_workqueue("MD32_WDT_WQ");
    wq_md32_assert = create_workqueue("MD32_ASSERT_WQ");

    INIT_WORK((struct work_struct *)&work_md32_reboot, md32_reboot_from_irq);
    INIT_WORK(&work_md32_wdt, md32_wdt_reset_func);
    INIT_WORK(&work_md32_assert, md32_assert_reset_func);


    //if(request_irq(MT_IPC_MD2HOST_INTR, md32_irq_handler, IRQF_TRIGGER_HIGH, "MD32 IPC_MD2HOST", NULL))
    //    pr_err("Create md32 irq fail\n");

}
