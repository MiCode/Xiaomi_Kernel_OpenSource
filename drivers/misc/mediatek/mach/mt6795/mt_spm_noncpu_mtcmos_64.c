#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/delay.h>    //udelay
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#include <mach/mt_typedefs.h>
#include <mach/mt_spm.h>
#include <mach/mt_spm_mtcmos.h>
#include <mach/mt_spm_mtcmos_internal.h>
#include <mach/hotplug.h>
#include <mach/mt_clkmgr.h>
#include <mach/mt_dcm.h>
#include <mach/mt_boot.h> //mt_get_chip_sw_ver


/**************************************
 * for non-CPU MTCMOS
 **************************************/
static DEFINE_SPINLOCK(spm_noncpu_lock);

#if 0
void spm_mtcmos_noncpu_lock(unsigned long *flags)
{
    spin_lock_irqsave(&spm_noncpu_lock, *flags);
}

void spm_mtcmos_noncpu_unlock(unsigned long *flags)
{
    spin_unlock_irqrestore(&spm_noncpu_lock, *flags);
}
#else
#define spm_mtcmos_noncpu_lock(flags)   \
do {    \
    spin_lock_irqsave(&spm_noncpu_lock, flags);  \
} while (0)

#define spm_mtcmos_noncpu_unlock(flags) \
do {    \
    spin_unlock_irqrestore(&spm_noncpu_lock, flags);    \
} while (0)

#endif

#define AUD_PWR_STA_MASK    (0x1 << 24)
#define MFG_ASYNC_PWR_STA_MASK (0x1 << 23)
#define MFG_2D_PWR_STA_MASK (0x1 << 22)
#define VEN_PWR_STA_MASK    (0x1 << 21)
#define MJC_PWR_STA_MASK    (0x1 << 20) 
#define VDE_PWR_STA_MASK    (0x1 << 7)
//#define IFR_PWR_STA_MASK    (0x1 << 6)
#define ISP_PWR_STA_MASK    (0x1 << 5)
#define MFG_PWR_STA_MASK    (0x1 << 4)
#define DIS_PWR_STA_MASK    (0x1 << 3)
//#define DPY_PWR_STA_MASK    (0x1 << 2)
#define MD1_PWR_STA_MASK    (0x1 << 0)

#if 0
#define PWR_RST_B           (0x1 << 0)
#define PWR_ISO             (0x1 << 1)
#define PWR_ON              (0x1 << 2)
#define PWR_ON_2ND          (0x1 << 3)
#define PWR_CLK_DIS         (0x1 << 4)
#endif

#define SRAM_PDN            (0xf << 8)
#define MFG_SRAM_PDN        (0x3f << 8) 
#define MD_SRAM_PDN         (0x1 << 8)
#define E3TCM_SRAM_PDN      (0x1f << 8)

/*
#define VDE_SRAM_ACK        (0x1 << 12)
#define IFR_SRAM_ACK        (0xf << 12)
#define ISP_SRAM_ACK        (0x3 << 12)
#define DIS_SRAM_ACK        (0xf << 12)
#define MFG_SRAM_ACK        (0x1 << 12)
*/
#define VDE_SRAM_ACK        (0x1 << 12)
#define VEN_SRAM_ACK        (0xf << 12)
#define ISP_SRAM_ACK        (0x3 << 12)
#define DIS_SRAM_ACK        (0x1 << 12)
#define MFG_SRAM_ACK        (0x3f << 16)
#define MFG_2D_SRAM_ACK     (0x3 << 12)
#define MJC_SRAM_ACK        (0x1 << 12)
#define AUD_SRAM_ACK        (0xf << 12)
#define E3TCM_SRAM_ACK      (0x1f << 16)

#define MD1_PROT_MASK     0x04B8//bit 3,4,5,7,10
//#define DISP_PROT_MASK    0x0046//bit 1,2,6
#define DISP_PROT_MASK    0x0006//bit 1,2
#define MFG_2D_PROT_MASK  0x00E04000//bit 14,21,22,23


int spm_mtcmos_ctrl_vdec(int state)
{
    int err = 0;

    volatile unsigned int val;
    unsigned long flags;
    int count = 0;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {
        spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) | SRAM_PDN);

        while ((spm_read(SPM_VDE_PWR_CON) & VDE_SRAM_ACK) != VDE_SRAM_ACK) {
            count++;
            if (count > 1000 && count<1010) {
                printk("there is no fmm_clk, CLK_CFG_0 = 0x%x\n", spm_read(CLK_CFG_0));
            }
            if (count > 2000) {
                clk_stat_check(SYS_DIS);
                BUG();
            }    
        }

        spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_VDE_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_VDE_PWR_CON, val);

        spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & VDE_PWR_STA_MASK) 
                || (spm_read(SPM_PWR_STATUS_2ND) & VDE_PWR_STA_MASK)) {
        }
    } else {    /* STA_POWER_ON */
        spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) | PWR_ON);
        spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & VDE_PWR_STA_MASK)
                || !(spm_read(SPM_PWR_STATUS_2ND) & VDE_PWR_STA_MASK)) {
        }

        spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) | PWR_RST_B);

        spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) & ~SRAM_PDN);

        while ((spm_read(SPM_VDE_PWR_CON) & VDE_SRAM_ACK)) {
            count++;
            if (count > 1000 && count<1010) {
                printk("there is no fmm_clk, CLK_CFG_0 = 0x%x\n", spm_read(CLK_CFG_0));
            }
            if (count > 2000) {
            	clk_stat_check(SYS_DIS);
                BUG();	
            }    
        }
    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}

int spm_mtcmos_ctrl_venc(int state)
{
    int err = 0;

    volatile unsigned int val;
    unsigned long flags;
    int count = 0;    

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {
        spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) | SRAM_PDN);

        while ((spm_read(SPM_VEN_PWR_CON) & VEN_SRAM_ACK) != VEN_SRAM_ACK) {
            count++;
            if (count > 1000 && count<1010) {
                printk("there is no fmm_clk, CLK_CFG_0 = 0x%x\n", spm_read(CLK_CFG_0));
            }
            if (count > 2000) {
                clk_stat_check(SYS_DIS);
                BUG();
            }    
        }

        spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_VEN_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_VEN_PWR_CON, val);

        spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & VEN_PWR_STA_MASK) 
                || (spm_read(SPM_PWR_STATUS_2ND) & VEN_PWR_STA_MASK)) {
        }
    } else {    /* STA_POWER_ON */
        spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) | PWR_ON);
        spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & VEN_PWR_STA_MASK)
                || !(spm_read(SPM_PWR_STATUS_2ND) & VEN_PWR_STA_MASK)) {
        }

        spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) | PWR_RST_B);

        spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) & ~SRAM_PDN);

        while ((spm_read(SPM_VEN_PWR_CON) & VEN_SRAM_ACK)) {
            count++;
            if (count > 1000 && count<1010) {
                printk("there is no fmm_clk, CLK_CFG_0 = 0x%x\n", spm_read(CLK_CFG_0));
            }
            if (count > 2000) {
                clk_stat_check(SYS_DIS);
                BUG();
            }
        }
    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}

int spm_mtcmos_ctrl_isp(int state)
{
    int err = 0;

    volatile unsigned int val;
    unsigned long flags;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {
        spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) | SRAM_PDN);

        while ((spm_read(SPM_ISP_PWR_CON) & ISP_SRAM_ACK) != ISP_SRAM_ACK) {
        }

        spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_ISP_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_ISP_PWR_CON, val);

        spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & ISP_PWR_STA_MASK)
                || (spm_read(SPM_PWR_STATUS_2ND) & ISP_PWR_STA_MASK)) {
        }
    } else {    /* STA_POWER_ON */
        spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) | PWR_ON);
        spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & ISP_PWR_STA_MASK)
                || !(spm_read(SPM_PWR_STATUS_2ND) & ISP_PWR_STA_MASK)) {
        }

        spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) | PWR_RST_B);

        spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) & ~SRAM_PDN);

        while ((spm_read(SPM_ISP_PWR_CON) & ISP_SRAM_ACK)) {
        }
    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}
#if 0
int spm_mtcmos_ctrl_disp(int state)
{
    int err = 0;
    volatile unsigned int val;
    unsigned long flags;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {
        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) | DISP_PROT_MASK);
        while ((spm_read(TOPAXI_PROT_STA1) & DISP_PROT_MASK) != DISP_PROT_MASK) {
        }
        
        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | SRAM_PDN);
#if 0
        while ((spm_read(SPM_DIS_PWR_CON) & DIS_SRAM_ACK) != DIS_SRAM_ACK) {
        }
#endif
        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_DIS_PWR_CON);
        //val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        val = val | PWR_CLK_DIS;
        spm_write(SPM_DIS_PWR_CON, val);

        //spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

#if 0
        udelay(1); 
        if (spm_read(SPM_PWR_STATUS) & DIS_PWR_STA_MASK) { 
            err = 1;
        }
#else
        //while ((spm_read(SPM_PWR_STATUS) & DIS_PWR_STA_MASK)
        //        || (spm_read(SPM_PWR_STATUS_S) & DIS_PWR_STA_MASK)) {
        //}
#endif
    } else {    /* STA_POWER_ON */
        //spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_ON);
        //spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_ON_2ND);
#if 0
        udelay(1);
#else
        //while (!(spm_read(SPM_PWR_STATUS) & DIS_PWR_STA_MASK) 
        //        || !(spm_read(SPM_PWR_STATUS_S) & DIS_PWR_STA_MASK)) {
        //}
#endif
        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~PWR_ISO);
        //spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_RST_B);

        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~SRAM_PDN);

#if 0
        while ((spm_read(SPM_DIS_PWR_CON) & DIS_SRAM_ACK)) {
        }
#endif

#if 0
        udelay(1); 
        if (!(spm_read(SPM_PWR_STATUS) & DIS_PWR_STA_MASK)) { 
            err = 1;
        }
#endif
        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) & ~DISP_PROT_MASK);
        while (spm_read(TOPAXI_PROT_STA1) & DISP_PROT_MASK) {
        }
    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}

#else

int spm_mtcmos_ctrl_disp(int state)
{
    int err = 0;

    volatile unsigned int val;
    unsigned long flags;
    int count = 0;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {
        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) | DISP_PROT_MASK);
        while ((spm_read(TOPAXI_PROT_STA1) & DISP_PROT_MASK) != DISP_PROT_MASK) {
        }
        
        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | SRAM_PDN);

        while ((spm_read(SPM_DIS_PWR_CON) & DIS_SRAM_ACK) != DIS_SRAM_ACK) {
            count++;
            if (count > 1000 && count<1010) {
                printk("there is no fmm_clk, CLK_CFG_0 = 0x%x\n", spm_read(CLK_CFG_0));
            }
            if (count > 2000) {
                clk_stat_check(SYS_DIS);
                BUG();
            }       	
        }

        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_DIS_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_DIS_PWR_CON, val);

        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & DIS_PWR_STA_MASK)
                || (spm_read(SPM_PWR_STATUS_2ND) & DIS_PWR_STA_MASK)) {
        }
    } else {    /* STA_POWER_ON */
        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_ON);
        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & DIS_PWR_STA_MASK) 
                || !(spm_read(SPM_PWR_STATUS_2ND) & DIS_PWR_STA_MASK)) {
        }

        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_RST_B);

        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~SRAM_PDN);

        while ((spm_read(SPM_DIS_PWR_CON) & DIS_SRAM_ACK)) {
            count++;
            if (count > 1000 && count<1010) {
                printk("there is no fmm_clk, CLK_CFG_0 = 0x%x\n", spm_read(CLK_CFG_0));
            }
            if (count > 2000) {
                clk_stat_check(SYS_DIS);
                BUG();
            }    
        }

        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) & ~DISP_PROT_MASK);
        while (spm_read(TOPAXI_PROT_STA1) & DISP_PROT_MASK) {
        }
    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}
#endif

int spm_mtcmos_ctrl_mfg(int state)
{
    int err = 0;

    volatile unsigned int val;
    unsigned long flags;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {
        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) | MFG_2D_PROT_MASK);
        while ((spm_read(TOPAXI_PROT_STA1) & MFG_2D_PROT_MASK) != MFG_2D_PROT_MASK) {
        }

        spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) | MFG_SRAM_PDN);

        while ((spm_read(SPM_MFG_PWR_CON) & MFG_SRAM_ACK) != MFG_SRAM_ACK) {
        }

        spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_MFG_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_MFG_PWR_CON, val);

        spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & MFG_PWR_STA_MASK) 
                || (spm_read(SPM_PWR_STATUS_2ND) & MFG_PWR_STA_MASK)) {
        }
    } else {    /* STA_POWER_ON */
        spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) | PWR_ON);
        spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & MFG_PWR_STA_MASK) || 
                !(spm_read(SPM_PWR_STATUS_2ND) & MFG_PWR_STA_MASK)) {
        }

        spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) | PWR_RST_B);

        spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) & ~MFG_SRAM_PDN);

        while ((spm_read(SPM_MFG_PWR_CON) & MFG_SRAM_ACK)) {
        }

        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) & ~MFG_2D_PROT_MASK);
        while (spm_read(TOPAXI_PROT_STA1) & MFG_2D_PROT_MASK) {
        }
    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}

int spm_mtcmos_ctrl_mfg_2D(int state)
{
    int err = 0;

    volatile unsigned int val;
    unsigned long flags;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {

        spm_write(SPM_MFG_2D_PWR_CON, spm_read(SPM_MFG_2D_PWR_CON) | SRAM_PDN);

        while ((spm_read(SPM_MFG_2D_PWR_CON) & MFG_2D_SRAM_ACK) != MFG_2D_SRAM_ACK) {
        }

        spm_write(SPM_MFG_2D_PWR_CON, spm_read(SPM_MFG_2D_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_MFG_2D_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_MFG_2D_PWR_CON, val);

        spm_write(SPM_MFG_2D_PWR_CON, spm_read(SPM_MFG_2D_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & MFG_2D_PWR_STA_MASK) 
                || (spm_read(SPM_PWR_STATUS_2ND) & MFG_2D_PWR_STA_MASK)) {
        }
    } else {    /* STA_POWER_ON */
        spm_write(SPM_MFG_2D_PWR_CON, spm_read(SPM_MFG_2D_PWR_CON) | PWR_ON);
        spm_write(SPM_MFG_2D_PWR_CON, spm_read(SPM_MFG_2D_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & MFG_2D_PWR_STA_MASK) || 
                !(spm_read(SPM_PWR_STATUS_2ND) & MFG_2D_PWR_STA_MASK)) {
        }

        spm_write(SPM_MFG_2D_PWR_CON, spm_read(SPM_MFG_2D_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_MFG_2D_PWR_CON, spm_read(SPM_MFG_2D_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_MFG_2D_PWR_CON, spm_read(SPM_MFG_2D_PWR_CON) | PWR_RST_B);

        spm_write(SPM_MFG_2D_PWR_CON, spm_read(SPM_MFG_2D_PWR_CON) & ~SRAM_PDN);

        while ((spm_read(SPM_MFG_2D_PWR_CON) & MFG_2D_SRAM_ACK)) {
        }

    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}

int spm_mtcmos_ctrl_mfg_ASYNC(int state)
{
    int err = 0;

    volatile unsigned int val;
    unsigned long flags;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {

        spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) | SRAM_PDN);

//        while ((spm_read(MFG_ASYNC_PWR_CON) & MFG_SRAM_ACK) != MFG_SRAM_ACK) {
//        }

        spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_MFG_ASYNC_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_MFG_ASYNC_PWR_CON, val);

        spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & MFG_ASYNC_PWR_STA_MASK) 
                || (spm_read(SPM_PWR_STATUS_2ND) & MFG_ASYNC_PWR_STA_MASK)) {
        }
    } else {    /* STA_POWER_ON */
        spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) | PWR_ON);
        spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & MFG_ASYNC_PWR_STA_MASK) || 
                !(spm_read(SPM_PWR_STATUS_2ND) & MFG_ASYNC_PWR_STA_MASK)) {
        }

        spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) | PWR_RST_B);

        spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) & ~SRAM_PDN);

//        while ((spm_read(MFG_ASYNC_PWR_CON) & MFG_SRAM_ACK)) {
//        }

    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}

int spm_mtcmos_ctrl_mdsys1(int state)
{
    int err = 0;

    volatile unsigned int val;
    unsigned long flags;
    int count = 0;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {
        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) | MD1_PROT_MASK);
        while ((spm_read(TOPAXI_PROT_STA1) & MD1_PROT_MASK) != MD1_PROT_MASK) {
            count++;
            if(count>1000)
                break;
        }

        spm_write(SPM_MD_PWR_CON, spm_read(SPM_MD_PWR_CON) | MD_SRAM_PDN);

        spm_write(SPM_MD_PWR_CON, spm_read(SPM_MD_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_MD_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_MD_PWR_CON, val);

        spm_write(SPM_MD_PWR_CON, spm_read(SPM_MD_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & MD1_PWR_STA_MASK)
                || (spm_read(SPM_PWR_STATUS_2ND) & MD1_PWR_STA_MASK)) {
        }
//#ifdef MTK_LTE_SUPPORT
//        spm_write(AP_PLL_CON7, (spm_read(AP_PLL_CON7) | 0xF)); //force off LTE
//#endif
    } else {    /* STA_POWER_ON */
//#ifdef MTK_LTE_SUPPORT
//        spm_write(AP_PLL_CON7, (spm_read(AP_PLL_CON7) & (~0xF))); //turn on LTE
//#endif
        spm_write(SPM_MD_PWR_CON, spm_read(SPM_MD_PWR_CON) | PWR_ON);
        spm_write(SPM_MD_PWR_CON, spm_read(SPM_MD_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & MD1_PWR_STA_MASK) 
                || !(spm_read(SPM_PWR_STATUS_2ND) & MD1_PWR_STA_MASK)) {
        }

        spm_write(SPM_MD_PWR_CON, spm_read(SPM_MD_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_MD_PWR_CON, spm_read(SPM_MD_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_MD_PWR_CON, spm_read(SPM_MD_PWR_CON) | PWR_RST_B);

        spm_write(SPM_MD_PWR_CON, spm_read(SPM_MD_PWR_CON) & ~MD_SRAM_PDN);

        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) & ~MD1_PROT_MASK);
        while (spm_read(TOPAXI_PROT_STA1) & MD1_PROT_MASK) {
        }
    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}

int spm_mtcmos_ctrl_mjc(int state)
{
    int err = 0;

    volatile unsigned int val;
    unsigned long flags;
    int count = 0;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {

        spm_write(SPM_MJC_PWR_CON, spm_read(SPM_MJC_PWR_CON) | SRAM_PDN);

        while ((spm_read(SPM_MJC_PWR_CON) & MJC_SRAM_ACK) != MJC_SRAM_ACK) {
            count++;
            if (count > 1000 && count<1010) {
                printk("there is no fmjc_clk, CLK_CFG_5 = 0x%x\n", spm_read(CLK_CFG_5));
            }
            if (count > 2000)
                BUG();
        }

        spm_write(SPM_MJC_PWR_CON, spm_read(SPM_MJC_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_MJC_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_MJC_PWR_CON, val);

        spm_write(SPM_MJC_PWR_CON, spm_read(SPM_MJC_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & MJC_PWR_STA_MASK) 
                || (spm_read(SPM_PWR_STATUS_2ND) & MJC_PWR_STA_MASK)) {
        }
    } else {    /* STA_POWER_ON */
    	spm_write(SPM_MJC_PWR_CON, spm_read(SPM_MJC_PWR_CON) | PWR_ON);
        spm_write(SPM_MJC_PWR_CON, spm_read(SPM_MJC_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & MJC_PWR_STA_MASK) || 
                !(spm_read(SPM_PWR_STATUS_2ND) & MJC_PWR_STA_MASK)) {
        }

        spm_write(SPM_MJC_PWR_CON, spm_read(SPM_MJC_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_MJC_PWR_CON, spm_read(SPM_MJC_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_MJC_PWR_CON, spm_read(SPM_MJC_PWR_CON) | PWR_RST_B);

        spm_write(SPM_MJC_PWR_CON, spm_read(SPM_MJC_PWR_CON) & ~SRAM_PDN);

        while ((spm_read(SPM_MJC_PWR_CON) & MJC_SRAM_ACK)) {
            count++;
            if (count > 1000 && count<1010) {
                printk("there is no fmjc_clk, CLK_CFG_5 = 0x%x\n", spm_read(CLK_CFG_5));
            }
            if (count > 2000)
                BUG();	
        }
    }
 
    spm_mtcmos_noncpu_unlock(flags);

    return err;
}

int spm_mtcmos_ctrl_aud(int state)
{
    int err = 0;

    volatile unsigned int val;
    unsigned long flags;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {

        spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) | SRAM_PDN);

        while ((spm_read(SPM_AUDIO_PWR_CON) & AUD_SRAM_ACK) != AUD_SRAM_ACK) {
        }

        spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_AUDIO_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_AUDIO_PWR_CON, val);

        spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & AUD_PWR_STA_MASK) 
                || (spm_read(SPM_PWR_STATUS_2ND) & AUD_PWR_STA_MASK)) {
        }
    } else {    /* STA_POWER_ON */
    	spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) | PWR_ON);
        spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & AUD_PWR_STA_MASK) || 
                !(spm_read(SPM_PWR_STATUS_2ND) & AUD_PWR_STA_MASK)) {
        }

        spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) | PWR_RST_B);

        spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) & ~SRAM_PDN);
        
        while ((spm_read(SPM_AUDIO_PWR_CON) & AUD_SRAM_ACK)) {
        }
    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}

int spm_ctrl_e3tcm(int state)
{
    int err = 0;
    unsigned long flags;

    spm_mtcmos_noncpu_lock(flags);
	
	if (state == STA_POWER_DOWN) {
	    spm_write(SPM_PCM_PASR_DPD_2, spm_read(SPM_PCM_PASR_DPD_2) | (1<<14));
	    spm_write(SPM_PCM_PASR_DPD_2, spm_read(SPM_PCM_PASR_DPD_2) & ~(1<<15));
	    spm_write(SPM_PCM_PASR_DPD_2, spm_read(SPM_PCM_PASR_DPD_2) | E3TCM_SRAM_PDN);
        while ((spm_read(SPM_PCM_PASR_DPD_2) & E3TCM_SRAM_ACK) !=E3TCM_SRAM_ACK) {
        }
	} else {    /* STA_POWER_ON */
        spm_write(SPM_PCM_PASR_DPD_2, spm_read(SPM_PCM_PASR_DPD_2) & ~E3TCM_SRAM_PDN);
        while ((spm_read(SPM_PCM_PASR_DPD_2) & E3TCM_SRAM_ACK)) {
        }
        spm_write(SPM_PCM_PASR_DPD_2, spm_read(SPM_PCM_PASR_DPD_2) | (1<<15));
        spm_write(SPM_PCM_PASR_DPD_2, spm_read(SPM_PCM_PASR_DPD_2) & ~(1<<14));
    }
    
    spm_mtcmos_noncpu_unlock(flags);
    return err;
}

int spm_topaxi_prot(int bit, int en)
{
    unsigned long flags;	
    spm_mtcmos_noncpu_lock(flags);

    if (en == 1) {
        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) | (1<<bit));
        while ((spm_read(TOPAXI_PROT_STA1) & (1<<bit)) != (1<<bit)) {
        }
    } else {
   	    spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) & ~(1<<bit));
        while (spm_read(TOPAXI_PROT_STA1) & (1<<bit)) {
        }
    }    

    spm_mtcmos_noncpu_unlock(flags);    

    return 0;
}
