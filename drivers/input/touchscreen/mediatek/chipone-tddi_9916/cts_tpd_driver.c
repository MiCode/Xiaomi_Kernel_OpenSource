#define TAG "TPD"

#include "tpd.h"
#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "../lct_tp_info.h"

#define CHECK_TOUCH_VENDOR

struct chipone_ts_data *chipone_ts_data = NULL;
extern char mtkfb_lcm_name[256];

extern int cts_driver_init(void);
extern void cts_driver_exit(void);
extern int cts_suspend(struct chipone_ts_data *cts_data);
extern int cts_resume(struct chipone_ts_data *cts_data);

static int chipone_tpd_local_init(void)
{
#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
    static int tpd_wb_start_local[TPD_WARP_CNT] = TPD_WARP_START;
    static int tpd_wb_end_local[TPD_WARP_CNT] = TPD_WARP_END;
#endif

#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
    static int tpd_def_calmat_local[8] = TPD_CALIBRATION_MATRIX;
#endif

    int ret;

#ifdef CHECK_TOUCH_VENDOR
    //Check TP vendor
    if (IS_ERR_OR_NULL(mtkfb_lcm_name)) {
        cts_err("mtkfb_lcm_name ERROR!");
        return -ENOMEM;
    } else {
            if (strcmp(mtkfb_lcm_name, "dsi_panel_c3s_36_0f_0c_fhdp_video") == 0) {
                cts_info("TP info: [Vendor]tianma [IC]icnl9916");
            } else {
                    cts_err("Unknown Touch");
                    return -ENODEV;
            }
    }
#endif

#ifdef CONFIG_CTS_CHECK_TOUCH_VENDOR
#define Holitech	"icnl9911c_vdo_hdp_holitech_drv"
#define CTC			"icnl9911c_vdo_hdp_ctc_drv"
	//Check TP vendor
	if (IS_ERR_OR_NULL(mtkfb_lcm_name)){
		cts_err("mtkfb_lcm_name ERROR!\n");
		ret = -ENOMEM;
		return -1;
	} else {
		if (strcmp(mtkfb_lcm_name,Holitech) == 0) {//Holiteck
			cts_vendor_info_val = CTS_VENDOR_HOLITECH;
			cts_err("TP info: [Vendor]Holitech [IC]icnl9911c\n");

		} else if (strcmp(mtkfb_lcm_name,CTC) == 0) {//Ctc
			cts_vendor_info_val = CTS_VENDOR_CTC;
			cts_err("TP info: [Vendor]CTC [IC]ICNL9911C\n");
		}
		else {
			cts_err("Unknow Touch\n");
			ret = -ENODEV;
			return -1;
		}
	}
#endif

    if ((ret = cts_driver_init()) != 0) {
        cts_err("Init driver failed %d", ret);
        return ret;
    }

    if (tpd_load_status == 0) {
        cts_err("Driver not load successfully, exit.");
        cts_driver_exit();
        return -1;
    }

    //create longcheer procfs node
	ret = init_lct_tp_info("[Vendor]unkown,[FW]unkown,[IC]unkown\n", NULL);
	if (ret < 0) {
		cts_info("init_lct_tp_info Failed!\n");
		goto err_init_lct_tp_info_failed;
	} else {
		cts_info("init_lct_tp_info Succeeded!\n");
	}

#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
    TPD_DO_WARP = 1;
    memcpy(tpd_wb_start, tpd_wb_start_local, TPD_WARP_CNT * 4);
    memcpy(tpd_wb_end, tpd_wb_start_local, TPD_WARP_CNT * 4);
#endif

#if (defined(CONFIG_TPD_HAVE_CALIBRATION) && !defined(CONFIG_TPD_CUSTOM_CALIBRATION))
    memcpy(tpd_calmat, tpd_def_calmat_local, 8 * 4);
    memcpy(tpd_def_calmat, tpd_def_calmat_local, 8 * 4);
#endif

#ifdef CONFIG_CTS_TP_WORK_IRQ
	cts_irq_status = true;
#endif

    tpd_type_cap = 1;

    return 0;

err_init_lct_tp_info_failed:
uninit_lct_tp_info();

	return ret;
}

/* TPD pass dev = NULL */
static void chipone_tpd_suspend(struct device *dev)
{
    cts_suspend(chipone_ts_data);
}

/* TPD pass dev = NULL */
static void chipone_tpd_resume(struct device *dev)
{
    cts_resume(chipone_ts_data);
}

static struct tpd_driver_t chipone_tpd_driver = {
    .tpd_device_name = CFG_CTS_DRIVER_NAME,
    .tpd_local_init = chipone_tpd_local_init,
    .suspend = chipone_tpd_suspend,
    .resume = chipone_tpd_resume,
    .tpd_have_button = 0,
    .attrs = {
        .attr = NULL,
        .num  = 0,
    }
};

static int __init chipone_tpd_driver_init(void)
{
    int ret;
    cts_info("Chipone TDDI TPD driver %s", CFG_CTS_DRIVER_VERSION);

    tpd_get_dts_info();
    if (tpd_dts_data.touch_max_num < 2) {
        tpd_dts_data.touch_max_num = 2;
    } else if (tpd_dts_data.touch_max_num > CFG_CTS_MAX_TOUCH_NUM) {
        tpd_dts_data.touch_max_num = CFG_CTS_MAX_TOUCH_NUM;
    }

    if((ret = tpd_driver_add(&chipone_tpd_driver)) < 0) {
        cts_err("Add TPD driver failed %d", ret);
        return ret;
    }

    return 0;
}

static void __exit chipone_tpd_driver_exit(void)
{
    cts_info("Chipone TPD driver exit");

    tpd_driver_remove(&chipone_tpd_driver);
}

late_initcall(chipone_tpd_driver_init);
module_exit(chipone_tpd_driver_exit);

#ifdef CFG_CTS_KERNEL_BUILTIN_FIRMWARE
MODULE_FIRMWARE(CFG_CTS_FIRMWARE_FILENAME_9916);
#endif /* CFG_CTS_KERNEL_BUILTIN_FIRMWARE */

MODULE_DESCRIPTION("Chipone TDDI TPD Driver for MTK platform");
MODULE_VERSION(CFG_CTS_DRIVER_VERSION);
MODULE_AUTHOR("Miao Defang <dfmiao@chiponeic.com>");
MODULE_LICENSE("GPL");

