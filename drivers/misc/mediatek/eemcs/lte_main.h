#ifndef __LTE_MAIN_H__
#define __LTE_MAIN_H__

#define MT_LTE_SDIO_KBUILD_MODNAME	"mtlte_sdio"

/* RX Q Threshold */
#define MT_LTE_RXQ0_MAX_PKT_REPORT_NUM	(16)

#define MT_LTE_RXQ1_MAX_PKT_REPORT_NUM	(16)

#define MT_LTE_RXQ2_MAX_PKT_REPORT_NUM	(16)

#define MT_LTE_RXQ3_MAX_PKT_REPORT_NUM	(16)


//int mtlte_sys_sdio_probe( struct sdio_func *func, const struct sdio_device_id *id);
//void mtlte_sys_sdio_remove(struct sdio_func *func);


int mtlte_sys_sdio_driver_init(void);
void mtlte_sys_sdio_driver_exit(void);

void mtlte_sys_sdio_driver_init_after_phase2(void);

#ifdef CONFIG_MTK_SDIOAUTOK_SUPPORT
// temp turn off auto-k feature on KK
#define MT_LTE_AUTO_CALIBRATION
#define NATIVE_AUTOK

//#define MT_LTE_ONLINE_TUNE_SUPPORT
#endif  // MTK_SDIOAUTOK_SUPPORT

#ifdef MT_LTE_AUTO_CALIBRATION
void mtlte_sys_sdio_wait_probe_done(void);
#endif

#endif
