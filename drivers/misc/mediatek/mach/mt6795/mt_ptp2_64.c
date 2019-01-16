#define __MT_PTP2_C__



#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <mach/mt_spm.h>
#include <mach/mt_boot.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif
#include "mach/mt_ptp2_64.h"


#ifdef USING_XLOG
#include <linux/xlog.h>

#define TAG     "PTP2"

#define ptp2_err(fmt, args...)       \
    xlog_printk(ANDROID_LOG_ERROR, TAG, fmt, ##args)
#define ptp2_warn(fmt, args...)      \
    xlog_printk(ANDROID_LOG_WARN, TAG, fmt, ##args)
#define ptp2_info(fmt, args...)      \
    xlog_printk(ANDROID_LOG_INFO, TAG, fmt, ##args)
#define ptp2_dbg(fmt, args...)       \
    xlog_printk(ANDROID_LOG_DEBUG, TAG, fmt, ##args)
#define ptp2_ver(fmt, args...)       \
    xlog_printk(ANDROID_LOG_VERBOSE, TAG, fmt, ##args)

#else   /* USING_XLOG */

#define TAG     "[PTP2] "

#define ptp2_err(fmt, args...)       \
    printk(KERN_ERR TAG KERN_CONT fmt, ##args)
#define ptp2_warn(fmt, args...)      \
    printk(KERN_WARNING TAG KERN_CONT fmt, ##args)
#define ptp2_info(fmt, args...)      \
    printk(KERN_NOTICE TAG KERN_CONT fmt, ##args)
#define ptp2_dbg(fmt, args...)       \
    printk(KERN_INFO TAG KERN_CONT fmt, ##args)
#define ptp2_ver(fmt, args...)       \
    printk(KERN_DEBUG TAG KERN_CONT fmt, ##args)

#endif  /* USING_XLOG */



#ifdef CONFIG_OF
void __iomem *ptp2_base; //0x10200000
#endif



// enable debug message
#define DEBUG   0

static struct PTP2_data ptp2_data;
static struct PTP2_trig ptp2_trig;

static int ptp2_lo_enable = 0;
static unsigned int ptp2_ctrl_lo[2];
static CHIP_SW_VER ver = CHIP_SW_VER_01;



void dump_ptp2_ctrl_regs(void)
{
#if DEBUG
    int i;

    for (i = 0; i < PTP2_REG_NUM; i++)
        ptp2_info("reg 0x%x = 0x%x", PTP2_CTRL_REG_0 + (i << 2), ptp2_read(PTP2_CTRL_REG_0 + (i << 2)));
#endif
}



void dump_spm_regs(void)
{
#if DEBUG
    ptp2_info("reg 0x%x = 0x%x", SPM_SLEEP_PTPOD2_CON, spm_read(SPM_SLEEP_PTPOD2_CON));
#endif
}



void ptp2_reset_data(struct PTP2_data *data)
{
    memset((void *)data, 0, sizeof(struct PTP2_data));
}



void ptp2_reset_trig(struct PTP2_trig *trig)
{
    memset((void *)trig, 0, sizeof(struct PTP2_trig));
}



int ptp2_set_rampstart(struct PTP2_data *data, unsigned int rampstart)
{
    if (rampstart & ~(0x3)) {
        ptp2_err("bad argument!! argument should be \"0\" ~ \"3\"\n");

		return -1;
	}

    data->RAMPSTART = rampstart;

    return 0;
}



int ptp2_set_rampstep(struct PTP2_data *data, unsigned int rampstep)
{
    if (rampstep & ~(0xF)) {
	    ptp2_err("bad argument!! argument should be \"0\" ~ \"15\"\n");

        return -1;
    }

    data->RAMPSTEP = rampstep;

    return 0;
}



int ptp2_set_delay(struct PTP2_data *data, unsigned int delay)
{
    if (delay & ~(0xF)) {
        ptp2_err("bad argument!! argument should be \"0\" ~ \"15\"\n");

        return -1;
    }

    data->DELAY = delay;

    return 0;
}



int ptp2_set_autoStopBypass_enable(struct PTP2_data *data, unsigned int autostop_enable)
{
    if (autostop_enable & ~(0x1)) {
        ptp2_err("bad argument!! argument should be \"0\" ~ \"1\"\n");

        return -1;
    }

    data->AUTO_STOP_BYPASS_ENABLE = autostop_enable;

    return 0;
}



int ptp2_set_triggerPulDelay(struct PTP2_data *data, unsigned int triggerPulDelay)
{
    if (triggerPulDelay & ~(0x1)) {
        ptp2_err("bad argument!! argument should be \"0\" ~ \"1\"\n");

        return -1;
    }
    data->TRIGGER_PUL_DELAY = triggerPulDelay;

   return 0;
}



int ptp2_set_ctrl_enable(struct PTP2_data *data, unsigned int ctrlEnable)
{
    if (ctrlEnable & ~(0x1)) {
        ptp2_err("bad argument!! argument should be \"0\" ~ \"1\"\n");

        return -1;
    }
    data->CTRL_ENABLE = ctrlEnable;

    return 0;
}



int ptp2_set_det_enable(struct PTP2_data *data, unsigned int detEnable)
{
    if (detEnable & ~(0x1)) {
        ptp2_err("bad argument!! argument should be \"0\" ~ \"1\"\n");

        return -1;
    }
    data->DET_ENABLE = detEnable;

    return 0;
}



int ptp2_set_mp0_nCORERESET(struct PTP2_trig *trig, unsigned int mp0_nCoreReset)
{
    if (mp0_nCoreReset & ~(0xF)) {
        ptp2_err("bad argument!! argument should be \"0\" ~ \"15\"\n");

        return -1;
    }
    trig->mp0_nCORE_RESET = mp0_nCoreReset;

    return 0;
}



int ptp2_set_mp0_STANDBYWFE(struct PTP2_trig *trig, unsigned int mp0_StandbyWFE)
{
    if (mp0_StandbyWFE & ~(0xF)) {
        ptp2_err("bad argument!! argument should be \"0\" ~ \"15\"\n");

        return -1;
    }
    trig->mp0_STANDBY_WFE = mp0_StandbyWFE;

    return 0;
}



int ptp2_set_mp0_STANDBYWFI(struct PTP2_trig *trig, unsigned int mp0_StandbyWFI)
{
    if (mp0_StandbyWFI & ~(0xF)) {
        ptp2_err("bad argument!! argument should be \"0\" ~ \"15\"\n");

        return -1;
    }
    trig->mp0_STANDBY_WFI = mp0_StandbyWFI;

    return 0;
}



int ptp2_set_mp0_STANDBYWFIL2(struct PTP2_trig *trig, unsigned int mp0_StandbyWFIL2)
{
    if (mp0_StandbyWFIL2 & ~(0x1)) {
        ptp2_err("bad argument!! argument should be \"0\" ~ \"1\"\n");

        return -1;
    }
    trig->mp0_STANDBY_WFIL2 = mp0_StandbyWFIL2;

    return 0;
}



int ptp2_set_mp1_nCORERESET(struct PTP2_trig *trig, unsigned int mp1_nCoreReset)
{
    if (mp1_nCoreReset & ~(0xF)) {
        ptp2_err("bad argument!! argument should be \"0\" ~ \"15\"\n");

        return -1;
    }
    trig->mp1_nCORE_RESET = mp1_nCoreReset;

    return 0;
}



int ptp2_set_mp1_STANDBYWFE(struct PTP2_trig *trig, unsigned int mp1_StandbyWFE)
{
    if (mp1_StandbyWFE & ~(0xF)) {
        ptp2_err("bad argument!! argument should be \"0\" ~ \"15\"\n");

        return -1;
    }
    trig->mp1_STANDBY_WFE = mp1_StandbyWFE;

    return 0;
}



int ptp2_set_mp1_STANDBYWFI(struct PTP2_trig *trig, unsigned int mp1_StandbyWFI)
{
    if (mp1_StandbyWFI & ~(0xF)) {
        ptp2_err("bad argument!! argument should be \"0\" ~ \"15\"\n");

        return -1;
    }
    trig->mp1_STANDBY_WFI = mp1_StandbyWFI;

    return 0;
}



int ptp2_set_mp1_STANDBYWFIL2(struct PTP2_trig *trig, unsigned int mp1_StandbyWFIL2)
{
    if (mp1_StandbyWFIL2 & ~(0x1)) {
        ptp2_err("bad argument!! argument should be \"0\" ~ \"1\"\n");

        return -1;
    }
    trig->mp1_STANDBY_WFIL2 = mp1_StandbyWFIL2;

    return 0;
}



void ptp2_apply(struct PTP2_data *data, struct PTP2_trig *trig)
{
    volatile unsigned int val_0 = BITS(PTP2_DET_RAMPSTART, data->RAMPSTART) |
                                  BITS(PTP2_DET_RAMPSTEP, data->RAMPSTEP) |
                                  BITS(PTP2_DET_DELAY, data->DELAY) |
                                  BITS(PTP2_DET_AUTO_STOP_BYPASS_ENABLE, data->AUTO_STOP_BYPASS_ENABLE) |
                                  BITS(PTP2_DET_TRIGGER_PUL_DELAY, data->TRIGGER_PUL_DELAY) |
                                  BITS(PTP2_CTRL_ENABLE, data->CTRL_ENABLE) |
                                  BITS(PTP2_DET_ENABLE, data->DET_ENABLE);

    volatile unsigned int val_1 = BITS(PTP2_MP0_nCORERESET,   trig->mp0_nCORE_RESET) |
                                  BITS(PTP2_MP0_STANDBYWFE,   trig->mp0_STANDBY_WFE) |
                                  BITS(PTP2_MP0_STANDBYWFI,   trig->mp0_STANDBY_WFI) |
                                  BITS(PTP2_MP0_STANDBYWFIL2, trig->mp0_STANDBY_WFIL2) |
                                  BITS(PTP2_MP1_nCORERESET,   trig->mp1_nCORE_RESET) |
                                  BITS(PTP2_MP1_STANDBYWFE,   trig->mp1_STANDBY_WFE) |
                                  BITS(PTP2_MP1_STANDBYWFI,   trig->mp1_STANDBY_WFI) |
                                  BITS(PTP2_MP1_STANDBYWFIL2, trig->mp1_STANDBY_WFIL2);

	ptp2_ctrl_lo[0] = val_0;
	ptp2_ctrl_lo[1] = val_1;
    ptp2_write(PTP2_CTRL_REG_0, val_0);
    ptp2_write(PTP2_CTRL_REG_1, val_1);

    //Apply software reset that apply the PTP2 LO value to system
    ptp2_write_field(PTP2_CTRL_REG_0, PTP2_DET_SWRST, 1);
    udelay(1000);
    ptp2_write_field(PTP2_CTRL_REG_0, PTP2_DET_SWRST, 0);
}



//config_LO_CTRL(PTP2_RAMPSTART_3, 9, 13, 0, 0, 1, 1, 0xF, 0xF, 0xF, 1, 0xF, 0xF, 0xF, 1) => For all on and worst case
//config_LO_CTRL(PTP2_RAMPSTART_3, 1, 1, 0, 0, 1, 1, 0xF, 0xF, 0xF, 1, 0xF, 0xF, 0xF, 1) => For all on and best case
//config_LO_CTRL(PTP2_RAMPSTART_3, 1, 1, 0, 0, 1, 1, 0x8, 0x8, 0x8, 1, 0xF, 0xF, 0xF, 1) => For >= 4 core on and best case
//config_LO_CTRL(PTP2_RAMPSTART_3, 0, 0, 0, 0, 0, 0, 0x0, 0x0, 0x0, 0, 0x0, 0x0, 0x0, 0) => For all off 
void config_LO_CTRL( unsigned int rampStart,
                     unsigned int rampStep,
                     unsigned int delay,
                     unsigned int autoStopEnable,
                     unsigned int triggerPulDelay,
                     unsigned int ctrlEnable,
                     unsigned int detEnable,
                     unsigned int mp0_nCoreReset,
                     unsigned int mp0_StandbyWFE,
                     unsigned int mp0_StandbyWFI,
                     unsigned int mp0_StandbyWFIL2,
                     unsigned int mp1_nCoreReset,
                     unsigned int mp1_StandbyWFE,
                     unsigned int mp1_StandbyWFI,
                     unsigned int mp1_StandbyWFIL2)
{
    ptp2_reset_data(&ptp2_data);
	smp_mb();
    ptp2_set_rampstart(&ptp2_data, rampStart);
    ptp2_set_rampstep(&ptp2_data, rampStep);
    ptp2_set_delay(&ptp2_data, delay);
    ptp2_set_autoStopBypass_enable(&ptp2_data, autoStopEnable);
    ptp2_set_triggerPulDelay(&ptp2_data, triggerPulDelay);
    ptp2_set_ctrl_enable(&ptp2_data, ctrlEnable);
    ptp2_set_det_enable(&ptp2_data, detEnable);

    ptp2_reset_trig(&ptp2_trig);
	smp_mb();
    ptp2_set_mp0_nCORERESET(&ptp2_trig, mp0_nCoreReset);
    ptp2_set_mp0_STANDBYWFE(&ptp2_trig, mp0_StandbyWFE);
    ptp2_set_mp0_STANDBYWFI(&ptp2_trig, mp0_StandbyWFI);
    ptp2_set_mp0_STANDBYWFIL2(&ptp2_trig, mp0_StandbyWFIL2);
    ptp2_set_mp1_nCORERESET(&ptp2_trig, mp1_nCoreReset);
    ptp2_set_mp1_STANDBYWFE(&ptp2_trig, mp1_StandbyWFE);
    ptp2_set_mp1_STANDBYWFI(&ptp2_trig, mp1_StandbyWFI);
    ptp2_set_mp1_STANDBYWFIL2(&ptp2_trig, mp1_StandbyWFIL2);
    smp_mb();
    ptp2_apply(&ptp2_data, &ptp2_trig);
}



void enable_LO(void)
{
	int i;
	
	if( (ptp2_ctrl_lo[0] & 0x03) == 0 )
		config_LO_CTRL(PTP2_RAMPSTART_3, 1, 1, 0, 0, 1, 1, 0x8, 0x8, 0x8, 0, 0xF, 0xF, 0xF, 0); //For >= 4 core on and best case; Fix me
	else
	{
        config_LO_CTRL(
                    (ptp2_ctrl_lo[0]>>12) & 0x03,
					(ptp2_ctrl_lo[0]>>8) & 0x0F,
					(ptp2_ctrl_lo[0]>>4) & 0x0F,
					(ptp2_ctrl_lo[0]>>3) & 0x01,
					(ptp2_ctrl_lo[0]>>2) & 0x01,
					(ptp2_ctrl_lo[0]>>1) & 0x01,
					ptp2_ctrl_lo[0] & 0x01,
					(ptp2_ctrl_lo[1]>>28) & 0x0f,
					(ptp2_ctrl_lo[1]>>24) & 0x0f,
					(ptp2_ctrl_lo[1]>>20) & 0x0f,
					(ptp2_ctrl_lo[1]>>19) & 0x01,
					(ptp2_ctrl_lo[1]>>15) & 0x0f,
					(ptp2_ctrl_lo[1]>>11) & 0x0f,
					(ptp2_ctrl_lo[1]>>7) & 0x0f,
					(ptp2_ctrl_lo[1]>>6) & 0x01
				  );

	}

	for(i=0; i<PTP2_REG_NUM; i++){
		ptp2_warn("\n\n\n\n\n[I-Chang]*********************************[%d]=[%x] \n\n\n\n\n", i, ptp2_read(PTP2_CTRL_REG_0 + (i << 2)));
	}
}



void disable_LO(void)
{
	config_LO_CTRL(0, 0, 0, 0, 0, 0, 0, 0x0, 0x0, 0x0, 0, 0x0, 0x0, 0x0, 0);// => For all off
}



void turn_on_LO(void)
{
    if (0 == ptp2_lo_enable)
        return;

	enable_LO();
}



void turn_off_LO(void)
{
    if (0 == ptp2_lo_enable)
        return;

    disable_LO();
}



//Device infrastructure
static int ptp2_remove(struct platform_device *pdev)
{
	return 0;
}



static int ptp2_probe(struct platform_device *pdev)
{
	return 0;
}



static int ptp2_suspend(struct platform_device *pdev, pm_message_t state)
{
	/*
	kthread_stop(ptp2_thread);
	*/

	return 0;
}



static int ptp2_resume(struct platform_device *pdev)
{
	/*
	ptp2_thread = kthread_run(ptp2_thread_handler, 0, "ptp2 xxx");
	if (IS_ERR(ptp2_thread))
	{
	    printk("[%s]: failed to create ptp2 xxx thread\n", __func__);
	}
	*/

	return 0;
}



#ifdef CONFIG_OF
static const struct of_device_id mt_ptp2_of_match[] = {
	{ .compatible = "mediatek,MCUCFG", },
	{},
};
#endif
static struct platform_driver ptp2_driver = {
	.remove     = ptp2_remove,
	.shutdown   = NULL,
	.probe      = ptp2_probe,
	.suspend    = ptp2_suspend,
	.resume     = ptp2_resume,
	.driver     = {
		.name   = "mt-ptp2",
		#ifdef CONFIG_OF
		.of_match_table = mt_ptp2_of_match,
		#endif
	},
};



#ifdef CONFIG_PROC_FS
static char *_copy_from_user_for_proc(const char __user *buffer, size_t count)
{
    char *buf = (char *)__get_free_page(GFP_USER);

    if (!buf)
        return NULL;

    if (count >= PAGE_SIZE)
        goto out;

    if (copy_from_user(buf, buffer, count))
        goto out;

    buf[count] = '\0';

    return buf;

out:
    free_page((unsigned long)buf);

    return NULL;
}



/* ptp2_lo_enable */
static int ptp2_lo_enable_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "ptp2_lo_enable = %d\n", ptp2_lo_enable);

    return 0;
}



static ssize_t ptp2_lo_enable_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
    int val = 0;

    char *buf = _copy_from_user_for_proc(buffer, count);

    if (!buf)
        return -EINVAL;

    sscanf(buf, "%d", &val);
    if (val == 1) {
		enable_LO();
        ptp2_lo_enable = 1;
    } else {
        ptp2_lo_enable = 0;
        disable_LO();
    }

    free_page((unsigned long)buf);

    return count;
}



/* ptp2_ctrl_lo_0 */
static int ptp2_ctrl_lo_0_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "ptp2_ctrl_lo_0 = %x\n", ptp2_ctrl_lo[0]);
    seq_printf(m, "[print by register ]ptp2_ctrl_lo_0 = %x\n", ptp2_read(PTP2_CTRL_REG_0));
    return 0;
}



static ssize_t ptp2_ctrl_lo_0_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
    char *buf = _copy_from_user_for_proc(buffer, count);

    if (!buf)
        return -EINVAL;

    sscanf(buf, "%x", ptp2_ctrl_lo);
	config_LO_CTRL(
					(ptp2_ctrl_lo[0]>>12) & 0x03,
					(ptp2_ctrl_lo[0]>>8) & 0x0F,
					(ptp2_ctrl_lo[0]>>4) & 0x0F,
					(ptp2_ctrl_lo[0]>>3) & 0x01,
					(ptp2_ctrl_lo[0]>>2) & 0x01,
					(ptp2_ctrl_lo[0]>>1) & 0x01,
					ptp2_ctrl_lo[0] & 0x01,
					(ptp2_ctrl_lo[1]>>28) & 0x0f,
					(ptp2_ctrl_lo[1]>>24) & 0x0f,
					(ptp2_ctrl_lo[1]>>20) & 0x0f,
					(ptp2_ctrl_lo[1]>>19) & 0x01,
					(ptp2_ctrl_lo[1]>>15) & 0x0f,
					(ptp2_ctrl_lo[1]>>11) & 0x0f,
					(ptp2_ctrl_lo[1]>>7) & 0x0f,
					(ptp2_ctrl_lo[1]>>6) & 0x01
				  );

    free_page((unsigned long)buf);

    return count;
}



/* ptp2_ctrl_lo_1*/
static int ptp2_ctrl_lo_1_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "ptp2_ctrl_lo_1 = %x\n", ptp2_ctrl_lo[1]);
    seq_printf(m, "[print by register ]ptp2_ctrl_lo_1 = %x\n", ptp2_read(PTP2_CTRL_REG_1) );
    return 0;
}



static ssize_t ptp2_ctrl_lo_1_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
    char *buf = _copy_from_user_for_proc(buffer, count);

    if (!buf)
        return -EINVAL;

    sscanf(buf, "%x", ptp2_ctrl_lo + 1);
	config_LO_CTRL(
					(ptp2_ctrl_lo[0]>>12) & 0x03,
					(ptp2_ctrl_lo[0]>>8) & 0x0F,
					(ptp2_ctrl_lo[0]>>4) & 0x0F,
					(ptp2_ctrl_lo[0]>>3) & 0x01,
					(ptp2_ctrl_lo[0]>>2) & 0x01,
					(ptp2_ctrl_lo[0]>>1) & 0x01,
					ptp2_ctrl_lo[0] & 0x01,
					(ptp2_ctrl_lo[1]>>28) & 0x0f,
					(ptp2_ctrl_lo[1]>>24) & 0x0f,
					(ptp2_ctrl_lo[1]>>20) & 0x0f,
					(ptp2_ctrl_lo[1]>>19) & 0x01,
					(ptp2_ctrl_lo[1]>>15) & 0x0f,
					(ptp2_ctrl_lo[1]>>11) & 0x0f,
					(ptp2_ctrl_lo[1]>>7) & 0x0f,
					(ptp2_ctrl_lo[1]>>6) & 0x01
				  );

    free_page((unsigned long)buf);

    return count;
}



/* ptp2_dump */
static int ptp2_dump_proc_show(struct seq_file *m, void *v)
{
    int i;

    for (i = 0; i < PTP2_REG_NUM; i++)
        seq_printf(m, "%x\n", ptp2_read(PTP2_CTRL_REG_0 + (i << 2)));
#if 0
    seq_printf(m, "%x\n", spm_read(SPM_SLEEP_PTPOD2_CON));
#endif

    return 0;
}



#define PROC_FOPS_RW(name)                          \
    static int name ## _proc_open(struct inode *inode, struct file *file)   \
    {                                   \
        return single_open(file, name ## _proc_show, PDE_DATA(inode));  \
    }                                   \
    static const struct file_operations name ## _proc_fops = {      \
        .owner          = THIS_MODULE,                  \
        .open           = name ## _proc_open,               \
        .read           = seq_read,                 \
        .llseek         = seq_lseek,                    \
        .release        = single_release,               \
        .write          = name ## _proc_write,              \
    }

#define PROC_FOPS_RO(name)                          \
    static int name ## _proc_open(struct inode *inode, struct file *file)   \
    {                                   \
        return single_open(file, name ## _proc_show, PDE_DATA(inode));  \
    }                                   \
    static const struct file_operations name ## _proc_fops = {      \
        .owner          = THIS_MODULE,                  \
        .open           = name ## _proc_open,               \
        .read           = seq_read,                 \
        .llseek         = seq_lseek,                    \
        .release        = single_release,               \
    }

#define PROC_ENTRY(name)    {__stringify(name), &name ## _proc_fops}

PROC_FOPS_RW(ptp2_lo_enable);
PROC_FOPS_RW(ptp2_ctrl_lo_0);
PROC_FOPS_RW(ptp2_ctrl_lo_1);
PROC_FOPS_RO(ptp2_dump);



static int _create_procfs(void)
{
    struct proc_dir_entry *dir = NULL;
    int i;

    struct pentry {
        const char *name;
        const struct file_operations *fops;
    };

    const struct pentry entries[] = {
        PROC_ENTRY(ptp2_lo_enable),
		PROC_ENTRY(ptp2_ctrl_lo_0),
		PROC_ENTRY(ptp2_ctrl_lo_1),
        PROC_ENTRY(ptp2_dump),
    };

    dir = proc_mkdir("ptp2", NULL);

    if (!dir) {
        ptp2_err("fail to create /proc/ptp2 @ %s()\n", __func__);
        return -ENOMEM;
    }

    for (i = 0; i < ARRAY_SIZE(entries); i++) {
        if (!proc_create(entries[i].name, S_IRUGO | S_IWUSR | S_IWGRP, dir, entries[i].fops))
            ptp2_err("%s(), create /proc/ptp2/%s failed\n", __func__, entries[i].name);
    }

    return 0;
}

#endif /* CONFIG_PROC_FS */



/*
 * Module driver
 */
static int __init ptp2_init(void)
{
    int err = 0;

    struct device_node *node = NULL;
	node = of_find_compatible_node(NULL, NULL, "mediatek,MCUCFG");
    if(node){
		/* Setup IO addresses */
		ptp2_base = of_iomap(node, 0);
		//printk("[PTP2] ptp2_base=0x%x\n",ptp2_base);
    }
    
	err = platform_driver_register(&ptp2_driver);
	if (err) {
        ptp2_err("%s(), PTP2 driver callback register failed..\n", __func__);
		return err;
	}

    ver = mt_get_chip_sw_ver();
    ptp2_lo_enable = 1;
	turn_on_LO();

#ifdef CONFIG_PROC_FS
    /* init proc */
    if (_create_procfs()) {
        err = -ENOMEM;
        goto out;
    }
#endif /* CONFIG_PROC_FS */

out:
    return err;
}



static void __exit ptp2_exit(void)
{
    ptp2_info("PTP2 de-initialization\n");
}



module_init(ptp2_init);
module_exit(ptp2_exit);

MODULE_DESCRIPTION("MediaTek PTP2 Driver v0.1");
MODULE_LICENSE("GPL");

#undef __MT_PTP2_C__
