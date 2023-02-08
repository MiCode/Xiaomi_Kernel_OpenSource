#ifndef CTS_PLAT_QCOM_CONFIG_H
#define CTS_PLAT_QCOM_CONFIG_H

#define CONFIG_CTS_PM_FB_NOTIFIER

#ifdef CONFIG_CTS_PM_FB_NOTIFIER
/*
 * #ifdef CONFIG_DRM
 * #define CFG_CTS_DRM_NOTIFIER
 * #endif
 */
#ifdef CONFIG_DRM
#define CFG_CTS_DRM_NOTIFIER
#endif /*CONFIG_DRM_MSM */
#else /*CONFIG_CTS_PM_FB_NOTIFIER */
#if defined(CONFIG_PM_SLEEP) && defined(CONFIG_PM_SUSPEND)
    /* #define CONFIG_CTS_PM_GENERIC */
#endif /* CONFIG_PM_SLEEP */

#if !defined(CONFIG_CTS_PM_GENERIC)
#define CONFIG_CTS_PM_LEGACY
#endif /*CONFIG_CTS_PM_GENERIC */
#endif /*CONFIG_CTS_PM_FB_NOTIFIER */

#define CFG_CTS_MAX_I2C_XFER_SIZE           (48u)
#define CFG_CTS_MAX_SPI_XFER_SIZE           (8192u)

#define CTS_FW_LOG_REDIRECT_SIGN            0x60
#define CTS_FW_LOG_BUF_LEN                  128

/**
 * #define CFG_CTS_SWAP_XY
 * #define CFG_CTS_WRAP_X
 * #define CFG_CTS_WRAP_Y
 */

#define CFG_CTS_DEVICE_NAME                 "chipone-tddi"
#define CFG_CTS_DRIVER_NAME                 "chipone-tddi"

#if CFG_CTS_MAX_I2C_XFER_SIZE < 8
#error "I2C transfer size should large than 8"
#endif

#ifdef CONFIG_OF
#define CONFIG_CTS_OF
#endif
#ifdef CONFIG_CTS_OF
#define CFG_CTS_OF_DEVICE_ID_NAME		"chipone-tddi"

#define CFG_CTS_OF_INT_GPIO_NAME		"chipone,irq-gpio"
#define CFG_CTS_OF_RST_GPIO_NAME		"chipone,rst-gpio"

#ifdef CFG_CTS_MANUAL_CS
#define CFG_CTS_OF_CS_GPIO_NAME			"chipone,cs-gpio"
#endif

#define CFG_CTS_OF_X_RESOLUTION_NAME	"chipone,x-res"
#define CFG_CTS_OF_Y_RESOLUTION_NAME	"chipone,y-res"

#ifdef CFG_CTS_FW_UPDATE_SYS
#define CFG_CTS_OF_PANEL_SUPPLIER		"chipone,panel-supplier"
#endif
#endif /* CONFIG_CTS_OF */

#endif /* CTS_PLAT_QCOM_CONFIG_H */
