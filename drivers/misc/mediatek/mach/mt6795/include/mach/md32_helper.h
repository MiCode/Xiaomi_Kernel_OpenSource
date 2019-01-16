#ifndef __MD32_HELPER_H__
#define __MD32_HELPER_H__

struct md32_regs
{
    void __iomem *sram;
    void __iomem *dtcm;
    void __iomem *cfg;
#if 0
    void __iomem *clkctrl;
    void __iomem *mad;
    void __iomem *intc;
    void __iomem *timer;
    void __iomem *i2c0;
    void __iomem *i2c1;
    void __iomem *i2c2;
    void __iomem *gpio;
    void __iomem *uart;
    void __iomem *eint;
    void __iomem *pmicwp;
    void __iomem *spm;
#endif
//    struct device *dev;
    int irq;
};

extern struct md32_regs md32reg;

#define MD32_PTCM_SIZE 0x4000
#define MD32_DTCM_SIZE 0x4000
#define MD32_CFGREG_SIZE 0x100

#define MD32_PTCM      (md32reg.sram)
#define MD32_DTCM      (md32reg.dtcm)

#define MD32_BASE      (md32reg.cfg)
#define MD32_SEMAPHORE (MD32_BASE  + 0x90)

#define MD32_AED_PHY_SIZE (MD32_PTCM_SIZE + MD32_DTCM_SIZE + MD32_CFGREG_SIZE)
#define MD32_AED_STR_LEN 512

#define MD32_JTAG_GPIO_DVT 0

#ifdef CONFIG_MTK_LDVT
  #define MD32_DATA_IMAGE_PATH "/system/etc/firmware/md32_d.bin"
  #define MD32_PROGRAM_IMAGE_PATH "/system/etc/firmware/md32_p.bin"
#else
  #define MD32_DATA_IMAGE_PATH "/etc/firmware/md32_d.bin"
  #define MD32_PROGRAM_IMAGE_PATH "/etc/firmware/md32_p.bin"
#endif
/* Support dynamic TCM data/program swap macro */
//#define DYNAMIC_TCM_SWAP
#define MAX_GROUP_NUM 10

/* Group IP */
enum group_id
{
  GROUP_BASIC,
  GROUP_A,
  GROUP_B,
  NR_GROUP,
};

enum info_num
{
  INFO_GROUP,
  INFO_PREPARE_SWAP,
  INFO_REQUEST_SWAP,
  INFO_SELF_TRIGGER_SWAP,
  NR_INFO_TYPE,
};

/* This structre need to sync with MD32-side */
typedef struct {
  unsigned int md32_log_buf_addr;
  unsigned int md32_log_start_idx_addr;
  unsigned int md32_log_end_idx_addr;
  unsigned int md32_log_lock_addr;
  unsigned int md32_log_buf_len_addr;
  unsigned int enable_md32_mobile_log_addr;
} MD32_LOG_INFO;

/* This structre need to sync with MD32-side */
typedef struct {
  int info_type;
  unsigned int app_ptr_d;
  unsigned int app_ptr_p;
  unsigned int swap_lock_addr;
  int group_num;
  unsigned int app_d_area;
  unsigned int app_p_area;
}MD32_GROUP_INFO;

/* This structre need to sync with MD32-side */
typedef struct {
  int info_type;
  int group_start;
}MD32_PREPARE_SWAP;

/* This structre need to sync with MD32-side */
typedef struct {
  int info_type;
  int current_group;
  int group_start;
  int prepare_result;
}MD32_REQUEST_SWAP;

/* MD32 notify event */
enum MD32_NOTIFY_EVENT
{
  MD32_SELF_TRIGGER_TCM_SWAP = 0,
  APP_TRIGGER_TCM_SWAP_START,
  APP_TRIGGER_TCM_SWAP_DONE,
  APP_TRIGGER_TCM_SWAP_FAIL,
  APP_TRIGGER_APP_FINISHED,
};

enum SEMAPHORE_FLAG{
  SEMAPHORE_CLK_CFG_5 = 0,
  SEMAPHORE_PTP,
  NR_FLAG = 8,
};

/* md32 ocd register */
#define MD32_OCD_BYPASS_JTAG_REG   (*(volatile unsigned int *)(MD32_BASE + 0x0040))
#define MD32_OCD_INSTR_WR_REG      (*(volatile unsigned int *)(MD32_BASE + 0x0044))
#define MD32_OCD_INSTR_REG         (*(volatile unsigned int *)(MD32_BASE + 0x0048))
#define MD32_OCD_DATA_WR_REG       (*(volatile unsigned int *)(MD32_BASE + 0x004C))
#define MD32_OCD_DATA_PI_REG       (*(volatile unsigned int *)(MD32_BASE + 0x0050))
#define MD32_OCD_DATA_PO_REG       (*(volatile unsigned int *)(MD32_BASE + 0x0054))
#define MD32_OCD_READY_REG         (*(volatile unsigned int *)(MD32_BASE + 0x0058))


#define MD32_OCD_CURRENT_CID      0x1
#define MD32_OCD_CMD(x)           ((MD32_OCD_CURRENT_CID << 11) | (x))

#define DBG_DATA_REG_INSTR      0x000
#define DBG_ADDR_REG_INSTR      0x001
#define DBG_INSTR_REG_INSTR     0x002
#define DBG_STATUS_REG_INSTR    0x003
#define DBG_REQUEST_INSTR       0x011
#define DBG_RESUME_INSTR        0x012
#define DBG_RESET_INSTR         0x013
#define DBG_STEP_INSTR          0x014
#define DBG_EXECUTE_INSTR       0x015
#define DBG_BP0_ENABLE_INSTR    0x020
#define DBG_BP0_DISABLE_INSTR   0x022
#define DBG_BP1_ENABLE_INSTR    0x024
#define DBG_BP1_DISABLE_INSTR   0x026
#define DBG_BP2_ENABLE_INSTR    0x028
#define DBG_BP2_DISABLE_INSTR   0x02a
#define DBG_BP3_ENABLE_INSTR    0x02c
#define DBG_BP3_DISABLE_INSTR   0x02e
#define DBG_PMb_LOAD_INSTR      0x040
#define DBG_PMb_STORE_INSTR     0x041
#define DBG_DMb_LOAD_INSTR      0x042
#define DBG_DMb_STORE_INSTR     0x043

#define DBG_MODE_INDX           0
#define DBG_BP_HIT_INDX         5
#define DBG_SWBREAK_INDX        8

enum cmd_md32_ocd
{
    CMD_MD32_OCD_STOP = 0,
    CMD_MD32_OCD_RESUME,
    CMD_MD32_OCD_STEP,
    CMD_MD32_OCD_READ_MEM,
    CMD_MD32_OCD_WRITE_MEM,
    CMD_MD32_OCD_WRITE_REG,
    CMD_MD32_OCD_HELP,
    CMD_MD32_OCD_BREAKPOINT,
    CMD_MD32_OCD_STATUS,
    CMD_MD32_OCD_DW,
    CMD_MD32_OCD_DR,
    CMD_MD32_OCD_IW,
    CMD_MD32_OCD_TEST,
};

enum jtag_gpio_mode
{
    JTAG_GPIO_MODE_IO = 0,
    JTAG_GPIO_MODE_AP,
    JTAG_GPIO_MODE_MFG,
    JTAG_GPIO_MODE_TDD,
    JTAG_GPIO_MODE_LTE,
    JTAG_GPIO_MODE_MD32,
    JTAG_GPIO_MODE_DFD,
    JTAG_GPIO_MODE_TOTAL,
};

typedef struct {
    enum cmd_md32_ocd cmd;
    u32               addr;
    u32               data;
    u32               break_en;
    int               success;
    spinlock_t		  spinlock;
} MD32_OCD_CMD_CFG;

extern struct device_attribute dev_attr_md32_ocd;
extern struct device_attribute dev_attr_md32_jtag_switch;
extern struct device_attribute dev_attr_md32_jtag_gpio_dvt;

extern void md32_ocd_init(void);


/* @group_id: the group want to swap in tcm and run. */

void md32_prepare_swap(int group_id);
int md32_tcm_swap(int group_id);
int get_current_group(void);
int get_md32_semaphore(int flag);
int release_md32_semaphore(int flag);
int reboot_load_md32(void);
extern ssize_t md32_get_log_buf(unsigned char *md32_log_buf, size_t b_len);
#endif
