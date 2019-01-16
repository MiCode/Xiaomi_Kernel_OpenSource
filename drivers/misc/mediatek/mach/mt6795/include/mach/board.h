#ifndef __ARCH_ARM_MACH_BOARD_H
#define __ARCH_ARM_MACH_BOARD_H

#include <generated/autoconf.h>
#include <linux/pm.h>
#include <board-custom.h>

typedef void (*sdio_irq_handler_t)(void*);  /* external irq handler */
typedef void (*pm_callback_t)(pm_message_t state, void *data);

#define MSDC_CD_PIN_EN      (1 << 0)  /* card detection pin is wired   */
#define MSDC_WP_PIN_EN      (1 << 1)  /* write protection pin is wired */
#define MSDC_RST_PIN_EN     (1 << 2)  /* emmc reset pin is wired       */
#define MSDC_SDIO_IRQ       (1 << 3)  /* use internal sdio irq (bus)   */
#define MSDC_EXT_SDIO_IRQ   (1 << 4)  /* use external sdio irq         */
#define MSDC_REMOVABLE      (1 << 5)  /* removable slot                */
#define MSDC_SYS_SUSPEND    (1 << 6)  /* suspended by system           */
#define MSDC_HIGHSPEED      (1 << 7)  /* high-speed mode support       */
#define MSDC_UHS1           (1 << 8)  /* uhs-1 mode support            */
#define MSDC_DDR            (1 << 9)  /* ddr mode support              */
#define MSDC_INTERNAL_CLK   (1 <<11)  /* Force Internal clock          */
#ifdef CONFIG_MTK_EMMC_CACHE
#define MSDC_CACHE          (1 <<12)  /* eMMC cache feature            */
#endif
#define MSDC_HS400          (1 <<13)  /* HS400 speed mode support      */

#define MSDC_SD_NEED_POWER  (1 << 31) /* for Yecon board, need SD power always on!! or cannot recognize the sd card*/

#define MSDC_SMPL_RISING    (0)
#define MSDC_SMPL_FALLING   (1)

#define MSDC_CMD_PIN        (0)
#define MSDC_DAT_PIN        (1)
#define MSDC_CD_PIN         (2)
#define MSDC_WP_PIN         (3)
#define MSDC_RST_PIN        (4)

#define MSDC_DATA1_INT      (1)
enum {
    MSDC_CLKSRC_200MHZ = 0, 
    MSDC_CLKSRC_400MHZ = 1, 
    MSDC_CLKSRC_800MHZ = 2
};
#define MSDC_BOOT_EN        (1)
#define MSDC_CD_HIGH        (1)
#define MSDC_CD_LOW         (0)
enum {
    MSDC_EMMC = 0,
    MSDC_SD   = 1,
    MSDC_SDIO = 2
};

struct msdc_ett_settings {
    unsigned int speed_mode; 
#define MSDC_DEFAULT_MODE (0)
#define MSDC_SDR50_MODE   (1)
#define MSDC_DDR50_MODE   (2)
#define MSDC_HS200_MODE   (3)
#define MSDC_HS400_MODE   (4)
    unsigned int reg_addr; 
    unsigned int reg_offset; 
    unsigned int value; 
}; 

struct msdc_hw {
    unsigned char  clk_src;                /* host clock source */
    unsigned char  cmd_edge;               /* command latch edge */
    unsigned char  rdata_edge;             /* read data latch edge */
    unsigned char  wdata_edge;             /* write data latch edge */
    unsigned char  clk_drv;                /* clock pad driving */
    unsigned char  cmd_drv;                /* command pad driving */
    unsigned char  dat_drv;                /* data pad driving */
    unsigned char  rst_drv;                /* RST-N pad driving */
    unsigned char  ds_drv;                 /* eMMC5.0 DS pad driving */
    unsigned char  clk_drv_sd_18;          /* clock pad driving for SD card at 1.8v sdr104 mode */
    unsigned char  cmd_drv_sd_18;          /* command pad driving for SD card at 1.8v sdr104 mode */
    unsigned char  dat_drv_sd_18;          /* data pad driving for SD card at 1.8v sdr104 mode */
    unsigned char  clk_drv_sd_18_sdr50;    /* clock pad driving for SD card at 1.8v sdr50 mode */
    unsigned char  cmd_drv_sd_18_sdr50;    /* command pad driving for SD card at 1.8v sdr50 mode */
    unsigned char  dat_drv_sd_18_sdr50;    /* data pad driving for SD card at 1.8v sdr50 mode */
    unsigned char  clk_drv_sd_18_ddr50;    /* clock pad driving for SD card at 1.8v ddr50 mode */
    unsigned char  cmd_drv_sd_18_ddr50;    /* command pad driving for SD card at 1.8v ddr50 mode */
    unsigned char  dat_drv_sd_18_ddr50;    /* data pad driving for SD card at 1.8v ddr50 mode */
    unsigned long  flags;                  /* hardware capability flags */
    unsigned long  data_pins;              /* data pins */
    unsigned long  data_offset;            /* data address offset */
    
    unsigned char  ddlsel;                 // data line delay line fine tune selecion
    unsigned char  rdsplsel;               // read: data line rising or falling latch fine tune selection 
    unsigned char  wdsplsel;               // write: data line rising or falling latch fine tune selection

    unsigned char  dat0rddly;              //read; range: 0~31
    unsigned char  dat1rddly;              //read; range: 0~31
    unsigned char  dat2rddly;              //read; range: 0~31
    unsigned char  dat3rddly;              //read; range: 0~31
    unsigned char  dat4rddly;              //read; range: 0~31
    unsigned char  dat5rddly;              //read; range: 0~31
    unsigned char  dat6rddly;              //read; range: 0~31
    unsigned char  dat7rddly;              //read; range: 0~31
    unsigned char  datwrddly;              //write; range: 0~31
    unsigned char  cmdrrddly;              //cmd; range: 0~31
    unsigned char  cmdrddly;               //cmd; range: 0~31

	unsigned char  cmdrtactr_sdr50;   // command response turn around counter, sdr 50 mode
	unsigned char  wdatcrctactr_sdr50;   // write data crc turn around counter, sdr 50 mode
	unsigned char  intdatlatcksel_sdr50;   // internal data latch CK select, sdr 50 mode
	unsigned char  cmdrtactr_sdr200;   // command response turn around counter, sdr 200 mode
	unsigned char  wdatcrctactr_sdr200;   // write data crc turn around counter, sdr 200 mode
	unsigned char  intdatlatcksel_sdr200;   // internal data latch CK select, sdr 200 mode

    struct msdc_ett_settings *ett_settings; 
    unsigned int ett_count; 
    unsigned long  host_function;          /* define host function*/
    bool           boot;                   /* define boot host*/ 
    bool           cd_level;               /* card detection level*/  
      
    /* config gpio pull mode */
    void (*config_gpio_pin)(int type, int pull);

    /* external power control for card */
    void (*ext_power_on)(void);
    void (*ext_power_off)(void);

    /* external sdio irq operations */
    void (*request_sdio_eirq)(sdio_irq_handler_t sdio_irq_handler, void *data);
    void (*enable_sdio_eirq)(void);
    void (*disable_sdio_eirq)(void);

    /* external cd irq operations */
    void (*request_cd_eirq)(sdio_irq_handler_t cd_irq_handler, void *data);
    void (*enable_cd_eirq)(void);
    void (*disable_cd_eirq)(void);
    int  (*get_cd_status)(void);
    
    /* power management callback for external module */
    void (*register_pm)(pm_callback_t pm_cb, void *data);
};

extern struct msdc_hw msdc0_hw;
extern struct msdc_hw msdc1_hw;
extern struct msdc_hw msdc2_hw;
extern struct msdc_hw msdc3_hw;

/*GPS driver*/
#define GPS_FLAG_FORCE_OFF  0x0001
struct mt3326_gps_hardware {
    int (*ext_power_on)(int);
    int (*ext_power_off)(int);
};
extern struct mt3326_gps_hardware mt3326_gps_hw;

/* NAND driver */
struct mtk_nand_host_hw {
    unsigned int nfi_bus_width;            /* NFI_BUS_WIDTH */ 
    unsigned int nfi_access_timing;        /* NFI_ACCESS_TIMING */  
    unsigned int nfi_cs_num;               /* NFI_CS_NUM */
    unsigned int nand_sec_size;            /* NAND_SECTOR_SIZE */
    unsigned int nand_sec_shift;           /* NAND_SECTOR_SHIFT */
    unsigned int nand_ecc_size;
    unsigned int nand_ecc_bytes;
    unsigned int nand_ecc_mode;
};
extern struct mtk_nand_host_hw mtk_nand_hw;

#endif /* __ARCH_ARM_MACH_BOARD_H */

