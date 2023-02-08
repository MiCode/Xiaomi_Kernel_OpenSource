#ifndef __FPSENSOR_SPI_TEE_H
#define __FPSENSOR_SPI_TEE_H

#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/fb.h>
#include <linux/notifier.h>
//#include "fpsensor_wakelock.h"
#include <linux/pm_wakeup.h>

#define FPSENSOR_DEV_NAME           "fpsensor"
#define FPSENSOR_CLASS_NAME         "fpsensor"
#define FPSENSOR_DEV_MAJOR          0
#define N_SPI_MINORS                32    /* ... up to 256 */
#define FPSENSOR_NR_DEVS            1

#define ERR_LOG     (0)
#define INFO_LOG    (1)
#define DEBUG_LOG   (2)
/* debug log setting */
static u8 fpsensor_debug_level = DEBUG_LOG;
#define fpsensor_debug(level, fmt, args...) do { \
        if (fpsensor_debug_level >= level) {\
            printk("[fpsensor][SN=%d] " fmt, g_cmd_sn, ##args); \
        } \
    } while (0)
#define FUNC_ENTRY()  fpsensor_debug(DEBUG_LOG, "%s, %d, entry\n", __func__, __LINE__)
#define FUNC_EXIT()   fpsensor_debug(DEBUG_LOG, "%s, %d, exit\n", __func__, __LINE__)

/**********************IO Magic**********************/
#define FPSENSOR_IOC_MAGIC    0xf0    //CHIP

/**************** Custom device : platfotm  or spi **************/
//#define  USE_PLATFORM_BUS     1
#define  USE_SPI_BUS  1
/**************************************************************/

/* define commands */
#define FPSENSOR_IOC_INIT                       _IOWR(FPSENSOR_IOC_MAGIC,0,uint32_t)
#define FPSENSOR_IOC_EXIT                       _IOWR(FPSENSOR_IOC_MAGIC,1,uint32_t)
#define FPSENSOR_IOC_RESET                      _IOWR(FPSENSOR_IOC_MAGIC,2,uint32_t)
#define FPSENSOR_IOC_ENABLE_IRQ                 _IOWR(FPSENSOR_IOC_MAGIC,3,uint32_t)
#define FPSENSOR_IOC_DISABLE_IRQ                _IOWR(FPSENSOR_IOC_MAGIC,4,uint32_t)
#define FPSENSOR_IOC_GET_INT_VAL                _IOWR(FPSENSOR_IOC_MAGIC,5,uint32_t)
#define FPSENSOR_IOC_DISABLE_SPI_CLK            _IOWR(FPSENSOR_IOC_MAGIC,6,uint32_t)
#define FPSENSOR_IOC_ENABLE_SPI_CLK             _IOWR(FPSENSOR_IOC_MAGIC,7,uint32_t)
#define FPSENSOR_IOC_ENABLE_POWER               _IOWR(FPSENSOR_IOC_MAGIC,8,uint32_t)
#define FPSENSOR_IOC_DISABLE_POWER              _IOWR(FPSENSOR_IOC_MAGIC,9,uint32_t)
/* fp sensor has change to sleep mode while screen off */
#define FPSENSOR_IOC_ENTER_SLEEP_MODE           _IOWR(FPSENSOR_IOC_MAGIC,11,uint32_t)
#define FPSENSOR_IOC_REMOVE                     _IOWR(FPSENSOR_IOC_MAGIC,12,uint32_t)
#define FPSENSOR_IOC_CANCEL_WAIT                _IOWR(FPSENSOR_IOC_MAGIC,13,uint32_t)
#define FPSENSOR_IOC_GET_FP_STATUS              _IOWR(FPSENSOR_IOC_MAGIC,19,uint32_t)
#define FPSENSOR_IOC_ENABLE_REPORT_BLANKON      _IOWR(FPSENSOR_IOC_MAGIC,21,uint32_t)
#define FPSENSOR_IOC_UPDATE_DRIVER_SN           _IOWR(FPSENSOR_IOC_MAGIC,22,uint32_t)
typedef struct {
    dev_t devno;
    struct class *class;
    struct cdev cdev;
#if defined(USE_SPI_BUS)
    struct spi_device *spi;
#elif defined(USE_PLATFORM_BUS)
    struct platform_device *spi;
#endif
    unsigned int users;
    u8 device_available;    /* changed during fingerprint chip sleep and wakeup phase */
    u8 irq_enabled;
    volatile unsigned int RcvIRQ;
    int irq;
    int irq_gpio;
    //struct wake_lock ttw_wl;
    struct wakeup_source *ttw_wl;
    wait_queue_head_t wq_irq_return;
    int cancel;
    struct pinctrl *pinctrl1;
    struct pinctrl_state  *eint_as_int, *fp_rst_low, *fp_rst_high, *fp_cs_low, *fp_mo_low,
            *fp_mi_low,  *fp_ck_low, *fp_power_high, *fp_power_low;
    struct notifier_block notifier;
    u8 fb_status;
    int enable_report_blankon;
    int free_flag;
} fpsensor_data_t;
#define     FPSENSOR_RST_PIN      1  // not gpio, only macro,not need modified!!
#define     FPSENSOR_SPI_CS_PIN   2  // not gpio, only macro,not need modified!!     
#define     FPSENSOR_SPI_MO_PIN   3  // not gpio, only macro,not need modified!!   
#define     FPSENSOR_SPI_MI_PIN   4  // not gpio, only macro,not need modified!!   
#define     FPSENSOR_SPI_CK_PIN   5  // not gpio, only macro,not need modified!! 
#endif    /* __FPSENSOR_SPI_TEE_H */
