#ifndef CTS_PLAT_MTK_CONFIG_H
#define CTS_PLAT_MTK_CONFIG_H

#ifdef CONFIG_MTK_I2C_EXTENSION
#define TPD_SUPPORT_I2C_DMA
#define CFG_CTS_MAX_I2C_XFER_SIZE       (250)
#define CFG_CTS_MAX_I2C_FIFO_XFER_SIZE  (8)
#else
#define CFG_CTS_MAX_I2C_XFER_SIZE       (128)
#endif /* CONFIG_MTK_I2C_EXTENSION */

#define CFG_CTS_MAX_SPI_XFER_SIZE           (1400u)

#define CTS_FW_LOG_REDIRECT_SIGN            0x60
#define CTS_FW_LOG_BUF_LEN                  128

//#define CFG_MTK_LEGEND_PLATFORM

/* Swap X and Y cordinate */
//#define CFG_CTS_SWAP_XY

#ifdef CONFIG_MTK_LCM_PHYSICAL_ROTATION_HW
#ifdef CONFIG_TOUCHSCREEN_PHYSICAL_ROTATION_WITH_LCM
//#define CFG_CTS_WRAP_X
//#define CFG_CTS_WRAP_Y
#else   /* CONFIG_TOUCHSCREEN_PHYSICAL_ROTATION_WITH_LCM */
//#define CFG_CTS_WRAP_X
//#define CFG_CTS_WRAP_Y
#endif  /* CONFIG_TOUCHSCREEN_PHYSICAL_ROTATION_WITH_LCM */
#else   /* CONFIG_MTK_LCM_PHYSICAL_ROTATION_HW */
#ifdef CONFIG_TOUCHSCREEN_PHYSICAL_ROTATION_WITH_LCM
//#define CFG_CTS_WRAP_X
//#define CFG_CTS_WRAP_Y
#else   /* CONFIG_TOUCHSCREEN_PHYSICAL_ROTATION_WITH_LCM */
//#define CFG_CTS_WRAP_X
//#define CFG_CTS_WRAP_Y
#endif  /* CONFIG_TOUCHSCREEN_PHYSICAL_ROTATION_WITH_LCM */
#endif  /* CONFIG_MTK_LCM_PHYSICAL_ROTATION_HW */

#define CFG_CTS_DEVICE_NAME         TPD_DEVICE
#define CFG_CTS_DRIVER_NAME         "chipone-tddi"

#ifdef CONFIG_OF
#define CONFIG_CTS_OF
#endif
#ifdef CONFIG_CTS_OF
#define CFG_CTS_OF_DEVICE_ID_NAME   "mediatek,cap_touch"
#endif /* CONFIG_CTS_OF */

#if CFG_CTS_MAX_I2C_XFER_SIZE < 8
#error "I2C transfer size should large than 8"
#endif

#endif /* CTS_PLAT_MTK_CONFIG_H */

