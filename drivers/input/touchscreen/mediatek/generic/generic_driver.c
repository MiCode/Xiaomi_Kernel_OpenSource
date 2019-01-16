#include "tpd_custom_generic.h"
#include "tpd.h"

extern int tpd_driver_local_init(void);
extern void tpd_driver_suspend(struct early_suspend *h);
extern void tpd_driver_resume(struct early_suspend *h); 

#ifdef TPD_HAVE_BUTTON 
static int tpd_keys[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif
#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
static int tpd_wb_start_local[TPD_WARP_CNT] = TPD_WARP_START;
static int tpd_wb_end_local[TPD_WARP_CNT]   = TPD_WARP_END;
#endif
#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
static int tpd_calmat_local[8]     = TPD_CALIBRATION_MATRIX;
static int tpd_def_calmat_local[8] = TPD_CALIBRATION_MATRIX;
#endif
/* invoke by tpd_init, initialize r-type touchpanel */
static int tpd_local_init(void) {
    tpd_driver_local_init();
#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))    
    TPD_DO_WARP = 1;
    memcpy(tpd_wb_start, tpd_wb_start_local, TPD_WARP_CNT*4);
    memcpy(tpd_wb_end, tpd_wb_start_end, TPD_WARP_CNT*4);
#endif
#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
    memcpy(tpd_calmat, tpd_calmat_local, 8*4);
    memcpy(tpd_def_calmat, tpd_def_calmat_local, 8*4);		
#endif   
		tpd_type_cap = 0;
    return 0;
}

static void tpd_suspend(struct early_suspend *h)
{
	 tpd_driver_suspend(h);
//	 return 0;
}
static void tpd_resume(struct early_suspend *h)
{
	tpd_driver_resume(h);
//	return 0;
}

static struct tpd_driver_t tpd_device_driver = {
		.tpd_device_name = "generic",
		.tpd_local_init = tpd_local_init,
		.suspend = tpd_suspend,
		.resume = tpd_resume,
#ifdef TPD_HAVE_BUTTON
		.tpd_have_button =1,
#else
		.tpd_have_button = 0,
#endif		
};
/* called when loaded into kernel */
static int __init tpd_driver_init(void) {
    printk("MediaTek generic touch panel driver init\n");
		if(tpd_driver_add(&tpd_device_driver) < 0)
			TPD_DMESG("add generic driver failed\n");
    return 0;
}

/* should never be called */
static void __exit tpd_driver_exit(void) {
    TPD_DMESG("MediaTek generic touch panel driver exit\n");
    //input_unregister_device(tpd->dev);
    tpd_driver_remove(&tpd_device_driver);
}

module_init(tpd_driver_init);
module_exit(tpd_driver_exit);

