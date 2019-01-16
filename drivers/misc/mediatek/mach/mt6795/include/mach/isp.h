#ifndef _MT_ISP_H
#define _MT_ISP_H

#include <linux/ioctl.h>

/*******************************************************************************
*
********************************************************************************/
#define ISP_DEV_MAJOR_NUMBER    251
#define ISP_MAGIC               'k'
/*******************************************************************************
*
********************************************************************************/
#define ISP_INT_EXP_DONE        ((u32)0x1)
#define ISP_INT_IDLE            ((u32)0x1 << 3)
#define ISP_INT_ISP_DONE        ((u32)0x1 << 4)
#define ISP_INT_VSYNC           ((u32)0x1 << 10)
#define ISP_INT_STNR            ((u32)0x1 << 29)
#define ISP_INT_CLEAR_ALL       ((u32)0x1 << 30)
#define ISP_INT_CLEAR_WAIT      ((u32)0x1 << 31)

/*******************************************************************************
*
********************************************************************************/
typedef struct mt_isp_reg_s {
    unsigned long addr;   // register's addr
    unsigned long val;    // register's value
} mt_isp_reg_t;

typedef struct mt_isp_reg_io_s {
    unsigned long data;   // pointer to mt_isp_reg_t
    unsigned long count;  // count
} mt_isp_reg_io_t;

typedef struct mt_isp_wait_irq_s {
    unsigned long mode;     // Mode for wait irq
    unsigned long timeout;  // Timeout for wait irq, uint:ms
} mt_isp_wait_irq_t;
/*******************************************************************************
*
********************************************************************************/
//IOCTRL(inode * ,file * ,cmd ,arg )
//S means "set through a ptr"
//T means "tell by a arg value"
//G means "get by a ptr"
//Q means "get by return a value"
//X means "switch G and S atomically"
//H means "switch T and Q atomically"
// ioctrl commands
// Reset
#define MT_ISP_IOC_T_RESET      _IO  (ISP_MAGIC, 1)
// Read register from driver
#define MT_ISP_IOC_G_READ_REG   _IOWR(ISP_MAGIC, 2, mt_isp_reg_io_t)
// Write register to driver
#define MT_ISP_IOC_S_WRITE_REG  _IOWR(ISP_MAGIC, 3, mt_isp_reg_io_t)
// Hold reg write to hw, on/off
#define MT_ISP_IOC_T_HOLD_REG   _IOW (ISP_MAGIC, 4, u32)
// MT_ISP_IOC_T_RUN : Tell ISP to run/stop
#define MT_ISP_IOC_T_RUN        _IOW (ISP_MAGIC, 5, u32)
// Wait IRQ
#define MT_ISP_IOC_T_WAIT_IRQ   _IOW (ISP_MAGIC, 6, u32) //seanlin 111223 fix conpilier error mt_isp_wait_irq_t)
// Dump ISP registers , for debug usage
#define MT_ISP_IOC_T_DUMP_REG   _IO  (ISP_MAGIC, 7)
// Dump message level
#define MT_ISP_IOC_T_DBG_FLAG   _IOW (ISP_MAGIC, 8, u32)
// Reset SW Buffer
#define MT_ISP_IOC_T_RESET_BUF  _IO  (ISP_MAGIC, 9)
// enable cam gate clock
#define MT_ISP_IOC_T_ENABLE_CAM_CLOCK  _IO  (ISP_MAGIC, 10)
/*******************************************************************************
*
********************************************************************************/
void mt_isp_mclk_ctrl(int en);
//
#endif

