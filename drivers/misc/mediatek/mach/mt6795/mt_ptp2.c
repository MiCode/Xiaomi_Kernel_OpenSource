#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <mach/pmic_mt6320_sw.h>
#include <mach/mt_spm.h>
#include <mach/mt_boot.h>

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

#define PTP2_BASEADDR            (0xF0200200)
#define APMIXED_BASE             (0xF0209000)
#define SLEEP_BASE               (0xF0006000)

extern u32 get_devinfo_with_index(u32 index); // TODO: FIXME #include "devinfo.h"


#define ARMCA15PLL_EN            (APMIXED_BASE+0x200)
#define ARMCA15PLL_CON1          (APMIXED_BASE+0x204)

#define PTP2_CTRL_REG_BASEADDR   (PTP2_BASEADDR + 0x70)
#define PTP2_CTRL_REG_0          (PTP2_CTRL_REG_BASEADDR)           // 0x70
#define PTP2_CTRL_REG_1          (PTP2_CTRL_REG_BASEADDR + 0x4)     // 0x74
#define PTP2_CTRL_REG_2          (PTP2_CTRL_REG_BASEADDR + 0x8)     // 0x78
#define PTP2_CTRL_REG_3          (PTP2_CTRL_REG_BASEADDR + 0xC)     // 0x7C
#define PTP2_CTRL_REG_4          (PTP2_CTRL_REG_BASEADDR + 0x10)    // 0x80
#define PTP2_CTRL_REG_5          (PTP2_CTRL_REG_BASEADDR + 0x14)    // 0x84
#define PTP2_CTRL_REG_6          (PTP2_CTRL_REG_BASEADDR + 0x18)    // 0x88
#define PTP2_DET_CPU_ENABLE_ADDR (PTP2_BASEADDR + 0x8C)
#define PTP2_DCC_ADDR            (PTP2_BASEADDR + 0x98)

#define PTP2_REG_NUM             8

#define ptp2_read(addr)          (*(volatile unsigned int *)(addr))
#define ptp2_write(addr, val)    (*(volatile unsigned int *)(addr) = (unsigned int)(val))

#define PTP2_DCC_STATUS          28:26
#define PTP2_DCC_CALDONE         25:25
#define PTP2_DCC_CALOUT          24:20
#define PTP2_DCC_CALRATE         16:15
#define PTP2_DCC_DCTARGET        14:7
#define PTP2_DCC_CALIN           6:2
#define PTP2_DCC_CALIBRATE       1:1
#define PTP2_DCC_BYPASS          0:0

#define PTP2_DET_RAMPSTART       15:14
#define PTP2_DET_DELAY           13:10
#define PTP2_DET_RAMPSTEP        9:6
#define PTP2_DET_AUTOSTOP_ENABLE 5:5
#define PTP2_DET_NOCPU_ENABLE    4:4
#define PTP2_DET_CPU_ENABLE      3:0

#define PTP2_CTRL_FBB_ENABLE        4:4
#define PTP2_CTRL_FBB_SW_ACK        4:4
#define PTP2_CTRL_FBB_SW_ENABLE     3:3

#define PTP2_CTRL_SPARK_SW_ENABLE   2:2
#define PTP2_CTRL_SW_ENABLE         1:1
#define PTP2_CTRL_SPM_ENABLE        0:0


//
// bit operation
//
#undef  BIT
#define BIT(bit)        (1U << (bit))

#define MSB(range)      (1 ? range)
#define LSB(range)      (0 ? range)
/**
 * Genearte a mask wher MSB to LSB are all 0b1
 * @r:  Range in the form of MSB:LSB
 */
#define BITMASK(r)      (((unsigned) -1 >> (31 - MSB(r))) & ~((1U << LSB(r)) - 1))

 /**
  * Set value at MSB:LSB. For example, BITS(7:3, 0x5A)
  * will return a value where bit 3 to bit 7 is 0x5A
  * @r: Range in the form of MSB:LSB
  */
/* BITS(MSB:LSB, value) => Set value at MSB:LSB  */
#define BITS(r, val)    ((val << LSB(r)) & BITMASK(r))

#define ptp2_read_field(addr, range)     \
        ((ptp2_read(addr) & BITMASK(range)) >> LSB(range))
/**
 * Write a field of a register.
 * @addr:       Address of the register
 * @range:      The field bit range in the form of MSB:LSB
 * @val:        The value to be written to the field
 */
#define ptp2_write_field(addr, range, val)       \
        ptp2_write(addr, (ptp2_read(addr) & ~BITMASK(range)) | BITS(range, val))

/**
 * PTP2 rampstart rate
 */
enum {
    PTP2_RAMPSTART_0 = 0b00,
    PTP2_RAMPSTART_1 = 0b01,
    PTP2_RAMPSTART_2 = 0b10,
    PTP2_RAMPSTART_3 = 0b11
};

/**
 * PTP2 control register
 */
enum {
    PTP2_CTRL_CPU_SPARK_EN_REG  = 0,
    PTP2_CTRL_SPM_EN_REG        = 1,
    PTP2_CTRL_NOCPU_LO_TRIG_REG = 2,
    PTP2_CTRL_CPU0_LO_TRIG_REG  = 3,
    PTP2_CTRL_CPU1_LO_TRIG_REG  = 4,
    PTP2_CTRL_CPU2_LO_TRIG_REG  = 5,
    PTP2_CTRL_CPU3_LO_TRIG_REG  = 6,
    PTP2_CTRL_REG_NUM           = 7
};

/**
 * PTP2 LO trigger
 */
enum {
    PTP2_CORE_RESET  = 0,
    PTP2_DEBUG_RESET = 1,
    PTP2_STANDBYWFI  = 2,
    PTP2_STANDBYWFE  = 3,
    PTP2_STANDBYWFI2 = 4,
    PTP2_TRIG_NUM    = 5
};

/**
 * PTP2 register setting
 */
struct ptp2_data {
    unsigned int RAMPSTART;
    unsigned int DELAY;
    unsigned int RAMPSTEP;
    unsigned int AUTOSTOPENABLE;
    unsigned int NOCPUENABLE;       // L1 LO
    unsigned int CPUENABLE;         // L2 LO

    unsigned int volatile ctrl_regs[7];
};


// ptp2_status definition
#define PTP2_ENABLE_FBB_SW          (1 << 0)
#define PTP2_ENABLE_FBB_SPM         (1 << 1)
#define PTP2_ENABLE_SPARK_SW        (1 << 2)
#define PTP2_ENABLE_SPARK_SPM       (1 << 3)
#define PTP2_ENABLE_DCC_AUTO_CAL    (1 << 4)
#define PTP2_ENABLE_DCC_CALIN       (1 << 5)

// enable debug message
#define DEBUG   0

#define FBB_ENABLE_BY_EFUSE 1

static unsigned int ptp2_status = 0;    // PTP2_ENABLE_DCC_CALIN or PTP2_ENABLE_DCC_AUTO_CAL

static struct ptp2_data ptp2_data;
static volatile unsigned int ptp2_regs[8] = {0};

static int ptp2_lo_enable = 0;
static int ptp2_fbb_enable = 0;
static int ptp2_spark_enable = 0;
static int ptp2_vfbb = 2;   // default: 300mv
static int ptp2_spark_count = 0;
static CHIP_SW_VER ver = CHIP_SW_VER_01;
static int ptp2_initialized = 0;

void dump_dcc_regs(void)
{
#if DEBUG
    ptp2_info("reg 0x%x = 0x%x", PTP2_DCC_ADDR, ptp2_read(PTP2_DCC_ADDR));
    ptp2_info("ca15pll_con1 0x%x = 0x%x", ARMCA15PLL_CON1, ptp2_read(ARMCA15PLL_CON1));
    ptp2_info("ca15pll_en 0x%x = 0x%x", ARMCA15PLL_EN, ptp2_read(ARMCA15PLL_EN));
#endif
}

void dump_ptp2_ctrl_regs(void)
{
#if DEBUG
    int i;

    for (i = 0; i < PTP2_REG_NUM; i++)
        ptp2_info("reg 0x%x = 0x%x", PTP2_CTRL_REG_BASEADDR + (i << 2), ptp2_read(PTP2_CTRL_REG_BASEADDR + (i << 2)));
#endif
}

void dump_spm_regs(void)
{
#if DEBUG
    ptp2_info("reg 0x%x = 0x%x", SPM_SLEEP_PTPOD2_CON, spm_read(SPM_SLEEP_PTPOD2_CON));
    ptp2_info("reg 0x%x = 0x%x", SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON));
#endif
}

void ptp2_reset_data(struct ptp2_data *data)
{
    memset(data, 0, sizeof(struct ptp2_data));
}

void ptp2_apply(struct ptp2_data *data)
{
    int i;
    volatile unsigned int val = BITS(PTP2_DET_RAMPSTART, data->RAMPSTART) |
        BITS(PTP2_DET_DELAY, data->DELAY) |
        BITS(PTP2_DET_RAMPSTEP, data->RAMPSTEP) |
        BITS(PTP2_DET_AUTOSTOP_ENABLE, data->AUTOSTOPENABLE) |
        BITS(PTP2_DET_NOCPU_ENABLE, data->NOCPUENABLE) |
        BITS(PTP2_DET_CPU_ENABLE, data->CPUENABLE);

    for (i = PTP2_CTRL_NOCPU_LO_TRIG_REG; i < PTP2_CTRL_REG_NUM; i++) {
        ptp2_write(PTP2_CTRL_REG_BASEADDR + (i << 2), data->ctrl_regs[i]);
    }

    ptp2_write(PTP2_DET_CPU_ENABLE_ADDR, val);
}

int ptp2_set_rampstart(struct ptp2_data *data, int rampstart)
{
    if (rampstart & ~(0x3)) {
        ptp2_err("bad argument!! argument should be \"0\" ~ \"3\"\n");

        return -1;
    }

    data->RAMPSTART = rampstart;

    return 0;
}

int ptp2_set_delay(struct ptp2_data *data, int delay)
{
    if (delay & ~(0xF)) {
        ptp2_err("bad argument!! argument should be \"0\" ~ \"15\"\n");

        return -1;
    }

    data->DELAY = delay;

    return 0;
}

int ptp2_set_rampstep(struct ptp2_data *data, int rampstep)
{
    if (rampstep & ~(0xF)) {
        ptp2_err("bad argument!! argument should be \"0\" ~ \"15\"\n");

        return -1;
    }

    data->RAMPSTEP = rampstep;

    return 0;
}

int ptp2_set_autostop_enable(struct ptp2_data *data, int autostop_enable)
{
    if (autostop_enable & ~(0x1)) {
        ptp2_err("bad argument!! argument should be \"0\" ~ \"15\"\n");

        return -1;
    }

    data->AUTOSTOPENABLE = autostop_enable;

    return 0;
}

int ptp2_set_nocpu_enable(struct ptp2_data *data, int nocpu_enable)
{
    if (nocpu_enable & ~(0x1)) {
        ptp2_err("bad argument!! argument should be \"0\" ~ \"1\"\n");

        return -1;
    }

    data->NOCPUENABLE = nocpu_enable;

    return 0;
}

int ptp2_set_nocpu_trig_ctrl(struct ptp2_data *data)
{
    int val = 0;

    val = 0xffffffff;
    data->ctrl_regs[2] = val;

    return 0;
}

int ptp2_set_cpu_enable(struct ptp2_data *data, int cpu_enable)
{
    if (cpu_enable & ~(0xf)) {
        ptp2_err("bad argument!! argument should be \"0\" ~ \"15\"\n");

        return -1;
    }

    data->CPUENABLE = cpu_enable;
    //data->CPUENABLE = 0xf;

    return 0;
}

int ptp2_set_cpu_trig_ctrl(struct ptp2_data *data, int cpu_en)
{
    int val = 0;
    int mask = 1;
    int i;

    if (cpu_en & ~(0xf)) {
        ptp2_err("bad argument for cpu!! argument should be \"0\" ~ \"f\"\n");

        return -1;
    }

    //val = 0xffffffff;   // WFE, WFI, CORERESET, CPUPORESET
    //val = 0xeeeeeeee;   // no WFE
    val = 0xdddddddd;   // no WFI
    //val = 0x11111111;   // WFE
    //val = 0x22222222;   // WFI
    //val = 0x44444444;   // CORERESET
    //val = 0x88888888;   // CPUPORESET
    for(i = 0; i < 4; i++) {
        if (cpu_en & mask) {
            data->ctrl_regs[i+3] = val;
        }
        mask <<= 1;
    }

    return 0;
}

void config_L1_LO(void)
{
    ptp2_reset_data(&ptp2_data);
    ptp2_set_rampstart(&ptp2_data, PTP2_RAMPSTART_3);
    ptp2_set_delay(&ptp2_data, 1);
    ptp2_set_rampstep(&ptp2_data, 1);
    ptp2_set_autostop_enable(&ptp2_data, 0);
    ptp2_set_nocpu_enable(&ptp2_data, 1);
    ptp2_set_nocpu_trig_ctrl(&ptp2_data);
    ptp2_apply(&ptp2_data);
}

void config_L2_LO(void)
{
    int cpu_en = 0xf;

    ptp2_reset_data(&ptp2_data);
    //ptp2_set_rampstart(&ptp2_data, PTP2_RAMPSTART_1); // cannot start
    ptp2_set_rampstart(&ptp2_data, PTP2_RAMPSTART_3);
    ptp2_set_delay(&ptp2_data, 1);
    ptp2_set_rampstep(&ptp2_data, 1);
    ptp2_set_autostop_enable(&ptp2_data, 0);
    ptp2_set_cpu_enable(&ptp2_data, cpu_en);
    ptp2_set_cpu_trig_ctrl(&ptp2_data, cpu_en);
    ptp2_apply(&ptp2_data);
}

void enable_FBB_SW(void)
{
    volatile unsigned int val_1;
    volatile unsigned int val_2;

    if (0 == (ptp2_status & PTP2_ENABLE_FBB_SW)) {
        val_1 = ptp2_read(PTP2_CTRL_REG_0) | 0x10;
        val_2 = ptp2_read(PTP2_CTRL_REG_1) | 0xa;
        ptp2_write(PTP2_CTRL_REG_0, val_1);
        ptp2_write(PTP2_CTRL_REG_1, val_2);
        ptp2_status |= PTP2_ENABLE_FBB_SW;
    }

    udelay(1);
}

void enable_FBB_SPM(void)
{
    volatile unsigned int val_1;
    volatile unsigned int val_2;

    if (0 == (ptp2_status & PTP2_ENABLE_FBB_SPM)) {
        val_1 = ptp2_read(PTP2_CTRL_REG_0) | 0x10;
        val_2 = ptp2_read(PTP2_CTRL_REG_1) | 0x1;

        // SPM FBBEN signal in ca15lcputop has to be set
        spm_write(SPM_SLEEP_PTPOD2_CON, (spm_read(SPM_SLEEP_PTPOD2_CON) | 0x100));
        ptp2_write(PTP2_CTRL_REG_0, val_1);
        ptp2_write(PTP2_CTRL_REG_1, val_2);
        ptp2_status |= PTP2_ENABLE_FBB_SPM;
    }

    udelay(1);
}

void disable_FBB_SW(void)
{
    volatile unsigned int val;

    if (ptp2_status & PTP2_ENABLE_FBB_SW) {
        val = ptp2_read(PTP2_CTRL_REG_1) & ~0x8;
        ptp2_write(PTP2_CTRL_REG_1, val);
        val = ptp2_read(PTP2_CTRL_REG_0) & ~0x10;
        ptp2_write(PTP2_CTRL_REG_0, val);
        ptp2_status &= ~PTP2_ENABLE_FBB_SW;
    }

    udelay(1);
}

void disable_FBB_SPM(void)
{
    volatile unsigned int val;

    if (ptp2_status & PTP2_ENABLE_FBB_SPM) {
        val = ptp2_read(PTP2_CTRL_REG_1) & ~0x1;
        // SPM FBBEN signal in ca15lcputop has to be set
        spm_write(SPM_SLEEP_PTPOD2_CON, (spm_read(SPM_SLEEP_PTPOD2_CON) & ~0x100));
        ptp2_write(PTP2_CTRL_REG_1, val);
        val = ptp2_read(PTP2_CTRL_REG_0) & ~0x10;
        ptp2_write(PTP2_CTRL_REG_0, val);
        ptp2_status &= ~PTP2_ENABLE_FBB_SPM;
    }

    udelay(1);
}

void enable_SPARK_SW(void)
{
    volatile unsigned int val_1;
    volatile unsigned int val_2;

    if (0 == (ptp2_status & PTP2_ENABLE_SPARK_SW)) {
        val_1 = ptp2_read(PTP2_CTRL_REG_0) | 0xf;
        val_2 = ptp2_read(PTP2_CTRL_REG_1) | 0x6;
        ptp2_write(PTP2_CTRL_REG_0, val_1);
        ptp2_write(PTP2_CTRL_REG_1, val_2);
        ptp2_status |= PTP2_ENABLE_SPARK_SW;
    }
}

void enable_SPARK_SPM(void)
{
    volatile unsigned int val_1;
    volatile unsigned int val_2;

    if (0 == (ptp2_status & PTP2_ENABLE_SPARK_SPM)) {
        val_1 = ptp2_read(PTP2_CTRL_REG_0) | 0xf;
        val_2 = ptp2_read(PTP2_CTRL_REG_1) | 0x1;

        // SPM FBBEN signal in ca15lcputop has to be set
        spm_write(SPM_SLEEP_PTPOD2_CON, (spm_read(SPM_SLEEP_PTPOD2_CON) | 0x200));
        ptp2_write(PTP2_CTRL_REG_0, val_1);
        ptp2_write(PTP2_CTRL_REG_1, val_2);
        ptp2_status |= PTP2_ENABLE_SPARK_SPM;
    }
}

void disable_SPARK_SW(void)
{
    volatile unsigned int val;

    if (ptp2_status & PTP2_ENABLE_SPARK_SW) {
        val = ptp2_read(PTP2_CTRL_REG_1) & ~0x4;
        ptp2_write(PTP2_CTRL_REG_1, val);
        val = ptp2_read(PTP2_CTRL_REG_0) & ~0xf;
        ptp2_write(PTP2_CTRL_REG_0, val);
        ptp2_status &= ~PTP2_ENABLE_SPARK_SW;
    }
}

void disable_SPARK_SPM(void)
{
    volatile unsigned int val;

    if (ptp2_status & PTP2_ENABLE_SPARK_SPM) {
        val = ptp2_read(PTP2_CTRL_REG_1) & ~0x1;

        // SPM FBBEN signal in ca15lcputop has to be set
        spm_write(SPM_SLEEP_PTPOD2_CON, (spm_read(SPM_SLEEP_PTPOD2_CON) & ~0x200));
        ptp2_write(PTP2_CTRL_REG_1, val);
        val = ptp2_read(PTP2_CTRL_REG_0) & ~0xf;
        ptp2_write(PTP2_CTRL_REG_0, val);
        ptp2_status &= ~PTP2_ENABLE_SPARK_SPM;
    }
}

void config_DCC_Calin(void)
{
    volatile unsigned int val;
    unsigned int calin = 0x15;  // should be read from efuse

    if (0 == (ptp2_status & PTP2_ENABLE_DCC_CALIN))
        return;

    val = ptp2_read(PTP2_DCC_ADDR) | (calin << 2);
    ptp2_write(PTP2_DCC_ADDR, val);

    udelay(100);
}

void config_DCC_Auto_Calibrate(void)
{
    if (0 == (ptp2_status & PTP2_ENABLE_DCC_AUTO_CAL))
        return;

    ptp2_write(PTP2_DCC_ADDR,
            ptp2_read(PTP2_DCC_ADDR) |
            (BITS(PTP2_DCC_CALRATE, 0x3) |
             BITS(PTP2_DCC_DCTARGET, 0x80) |
             BITS(PTP2_DCC_CALIN, 0x0) |
             BITS(PTP2_DCC_CALIBRATE, 0x1) |
             BITS(PTP2_DCC_BYPASS, 0x0)
            ));

    udelay(100);
}

void disable_LO(void)
{
    volatile unsigned int val;

    spm_write(SPM_CA15_CPU0_IRQ_MASK, 1);
    spm_write(SPM_CA15_CPU1_IRQ_MASK, 1);
    spm_write(SPM_CA15_CPU2_IRQ_MASK, 1);
    spm_write(SPM_CA15_CPU3_IRQ_MASK, 1);
    udelay(1);
    val = ptp2_read(PTP2_DET_CPU_ENABLE_ADDR);
    val &= ~0xf;
    ptp2_write(PTP2_DET_CPU_ENABLE_ADDR, val);
    spm_write(SPM_CA15_CPU0_IRQ_MASK, 0);
    spm_write(SPM_CA15_CPU1_IRQ_MASK, 0);
    spm_write(SPM_CA15_CPU2_IRQ_MASK, 0);
    spm_write(SPM_CA15_CPU3_IRQ_MASK, 0);
}

int get_LO_status(void)
{
    volatile unsigned int val = 0;

    val = ptp2_read(PTP2_DET_CPU_ENABLE_ADDR);
    val &= 0xf;
    if (val != 0)
        return 1;

    return 0;
}

void set_VFBB(void)
{
    pmic_config_interface(0x013E,0x8A0,0xFFFF,0); // send 00 013E 8A0
    pmic_config_interface(0x55C,0x2000,0xFFFF,0); // send 00 55C 2000
    pmic_config_interface(0x576,ptp2_vfbb,0x7,6); // set RG_VFBB_VOSEL
    pmic_config_interface(0x576,0x3,0x3,4); // set RG_VFBB_SLEW_CNTRL
    udelay(10);
}

void turn_on_FBB(void)
{
    if (0 == ptp2_fbb_enable)
        return;

    ptp2_dbg("turn on FBB\n");
    enable_FBB_SW();
    set_VFBB();
}

void turn_off_FBB(void)
{
    if (0 == ptp2_fbb_enable)
        return;

    ptp2_dbg("turn off FBB\n");
    disable_FBB_SW();
}

void turn_on_SPARK(void)
{
    if (0 == ptp2_spark_enable)
        return;

    ptp2_dbg("turn on SPARK\n");
    ptp2_spark_count++;
    enable_SPARK_SW();
}

void turn_off_SPARK(void)
{
    if (0 == ptp2_spark_enable)
        return;

    ptp2_dbg("turn off SPARK\n");
    disable_SPARK_SW();
}

void turn_on_LO(void)
{
    if (0 == ptp2_lo_enable)
        return;

    config_L2_LO();
    //config_L1_LO();
}

void turn_off_LO(void)
{
    if (0 == ptp2_lo_enable)
        return;

    disable_LO();
}

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
        config_L2_LO();
        ptp2_lo_enable = 1;
    } else {
        ptp2_lo_enable = 0;
        disable_LO();
    }

    free_page((unsigned int)buf);

    return count;
}

/* ptp2_fbb_enable */
static int ptp2_fbb_enable_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "ptp2_fbb_enable = %d\n", ptp2_fbb_enable);

    return 0;
}

static ssize_t ptp2_fbb_enable_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
    int val = 0;

    char *buf = _copy_from_user_for_proc(buffer, count);

    if (!buf)
        return -EINVAL;

    sscanf(buf, "%d", &val);
    if (val == 1) {
        ptp2_fbb_enable = 1;
    } else {
        ptp2_fbb_enable = 0;
        disable_FBB_SW();
    }

    free_page((unsigned int)buf);

    return count;
}

/* ptp2_spark_enable */
static int ptp2_spark_enable_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "ptp2_spark_enable = %d\n", ptp2_spark_enable);

    return 0;
}

static ssize_t ptp2_spark_enable_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
    int val = 0;

    char *buf = _copy_from_user_for_proc(buffer, count);

    if (!buf)
        return -EINVAL;

    sscanf(buf, "%d", &val);
    if (val == 1) {
        ptp2_spark_enable = 1;
    } else {
        ptp2_spark_enable = 0;
        disable_SPARK_SW();
    }

    free_page((unsigned int)buf);

    return count;
}

/* ptp2_vfbb */
static int ptp2_vfbb_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "ptp2_vfbb = %d\n", ptp2_vfbb);
    seq_printf(m, "0: 200mv\n1: 250mv\n2: 300mv\n3: 350mv\n4: 400mv\n5: 450mv\n6: 500mv\n7: 550mv\n");

    return 0;
}

static ssize_t ptp2_vfbb_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
    int val = ptp2_vfbb;
    char *buf = _copy_from_user_for_proc(buffer, count);

    if (!buf)
        return -EINVAL;

    sscanf(buf, "%d", &val);
    if (val >= 0 && val <= 7) {
        ptp2_vfbb = val;
    } else {
        ptp2_err("wrong ptp2_vfbb value %d\n", val);
    }

    free_page((unsigned int)buf);

    return count;
}

/* ptp2_dump */
static int ptp2_dump_proc_show(struct seq_file *m, void *v)
{
    int i;

    for (i = 0; i < PTP2_REG_NUM; i++)
        seq_printf(m, "%x\n", ptp2_read(PTP2_CTRL_REG_BASEADDR + (i << 2)));
    seq_printf(m, "%x\n\n", ptp2_read(PTP2_DCC_ADDR));
#if 0
    seq_printf(m, "%x\n", ptp2_read(ARMCA15PLL_CON1));
    seq_printf(m, "%x\n", ptp2_read(ARMCA15PLL_EN));
    seq_printf(m, "%x\n", spm_read(SPM_SLEEP_PTPOD2_CON));
#endif

    return 0;
}

/* ptp2_spark_count */
static int ptp2_spark_count_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "ptp2_spark_count = %d\n", ptp2_spark_count);

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
PROC_FOPS_RW(ptp2_fbb_enable);
PROC_FOPS_RW(ptp2_spark_enable);
PROC_FOPS_RW(ptp2_vfbb);
PROC_FOPS_RO(ptp2_dump);
PROC_FOPS_RO(ptp2_spark_count);

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
        PROC_ENTRY(ptp2_fbb_enable),
        PROC_ENTRY(ptp2_spark_enable),
        PROC_ENTRY(ptp2_vfbb),
        PROC_ENTRY(ptp2_dump),
        PROC_ENTRY(ptp2_spark_count),
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

int ptp2_pre_init(void)
{
    int val = 0;

    if (ptp2_initialized == 1)
        return 0;

    /* M_HW_RES4 */
    val = get_devinfo_with_index(15);

    ver = mt_get_chip_sw_ver();
    ptp2_lo_enable = get_LO_status();

    if (ver >= CHIP_SW_VER_02) {
#if FBB_ENABLE_BY_EFUSE
        if (val & 0x4) {
            ptp2_fbb_enable = 1;
        }
#endif
        ptp2_lo_enable = 1;
        // FIXME: force turn off for E2 bring up
        ptp2_spark_enable = 0;
    }
    else if (ver >= CHIP_SW_VER_01) {
#ifdef MTK_FORCE_CLUSTER1
#if FBB_ENABLE_BY_EFUSE
        if (val & 0x4) {
            ptp2_fbb_enable = 1;
        }
#endif
        ptp2_spark_enable = 0;
#endif
    }

    turn_on_LO();
    turn_on_FBB();
    ptp2_initialized = 1;
}

static int __init ptp2_init(void)
{
    int err = 0;

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
