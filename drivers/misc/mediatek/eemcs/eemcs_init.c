#include <linux/module.h>
#include "eemcs_debug.h"
#include "eemcs_kal.h"
#include "eccmni.h"
#include "eemcs_char.h"
#include "eemcs_ipc.h"
#include "eemcs_boot.h"
#include "eemcs_boot_trace.h"
#include "eemcs_state.h"
#include "eemcs_fs_ut.h"
#include "eemcs_rpc.h"
#include "eemcs_expt.h"
#include "eemcs_sysmsg.h"
#include "eemcs_statistics.h"

#include "lte_main.h"

/*eemcs driver version define*/
#define EEMCS_VERSION "v1.0 20131001"


int eemcs_ccci_init(void){
    int ret = CCCI_ERR_MODULE_INIT_OK ;

    DBGLOG(INIT, INF, "====> %s ", FUNC_NAME);  

    if ((ret = eemcs_expt_mod_init()) != KAL_SUCCESS){
        DBGLOG(INIT, ERR, "eemcs_expt_mod_init fail");  
        ret = -CCCI_ERR_INIT_PLATFORM_FAIL;
        goto INIT_CCCI_EXPT_FAIL;
    }

    if ((ret = eemcs_ccci_mod_init()) != KAL_SUCCESS){
        DBGLOG(INIT, ERR, "eemcs_ccci_mod_init fail");  
        ret = -CCCI_ERR_INIT_PLATFORM_FAIL;
        goto INIT_CCCI_FAIL;
    }
    
    if ((ret = eccmni_mod_init()) != KAL_SUCCESS){
        DBGLOG(INIT, ERR, "eccmni_mod_init fail");  
        ret = -CCCI_ERR_INIT_CCMNI_FAIL;
        goto INIT_NI_FAIL;
    }

    if ((ret = eemcs_char_mod_init()) != KAL_SUCCESS){
        DBGLOG(INIT, ERR, "eemcs_char_mod_init fail");  
        ret = -CCCI_ERR_INIT_CHAR_DEV_FAIL;
        goto INIT_CDEV_FAIL;
    }

    if ((ret = eemcs_ipc_mod_init()) != KAL_SUCCESS){
        DBGLOG(INIT, ERR, "eemcs_ipc_mod_init fail");  
        ret = -CCCI_ERR_INIT_IPC_FAIL;
        goto INIT_IPC_FAIL;
    }

    if ((ret = eemcs_rpc_mod_init()) != KAL_SUCCESS){
        DBGLOG(INIT, ERR, "eemcs_rpc_mod_init fail");
        ret = -CCCI_ERR_INIT_RPC_FAIL;
        goto INIT_RPC_FAIL;
    }

    if ((ret = eemcs_sysmsg_mod_init()) != KAL_SUCCESS){
        DBGLOG(INIT, ERR, "eemcs_sysmsg_mod_init fail");
        ret = -CCCI_ERR_INIT_SYSMSG_FAIL;
        goto INIT_SYSMSG_FAIL;
    }

    if ((ret = eemcs_statistics_init()) != KAL_SUCCESS){
        DBGLOG(INIT, ERR, "eemcs_statistics_init fail");  
        ret = -CCCI_ERR_INIT_PLATFORM_FAIL;
        goto INIT_STATISTICS_FAIL;
    }

    DBGLOG(INIT, INF, "<==== %s ", FUNC_NAME); 
    return ret;
    
INIT_STATISTICS_FAIL:
    eemcs_statistics_exit();
INIT_SYSMSG_FAIL:  
    eemcs_sysmsg_exit();
INIT_RPC_FAIL:	
    eemcs_ipc_exit();
INIT_IPC_FAIL:
    eemcs_char_exit();    
INIT_CDEV_FAIL:
    eccmni_deinit_mod_exit();
INIT_NI_FAIL:
    eemcs_ccci_exit();
INIT_CCCI_FAIL:
    eemcs_expt_exit();
INIT_CCCI_EXPT_FAIL:
    
    return ret;    
}

void eemcs_ccci_remove(void){
    
    eemcs_rpc_exit();
    eemcs_ipc_exit();
    eemcs_char_exit();
    eccmni_deinit_mod_exit();
    eemcs_ccci_exit();
    eemcs_expt_exit();
    
    return;
}

static int __init eemcs_init(void)
{
    int ret = CCCI_ERR_MODULE_INIT_OK ;

    LOG_FUNC("[EEMCS/INIT] Ver. %s, @ %s %s\n", EEMCS_VERSION, __DATE__, __TIME__);
    LOG_FUNC("[EEMCS/INIT] ====> %s", FUNC_NAME);

    if ((ret = eemcs_debug_mod_init()) != KAL_SUCCESS){
        DBGLOG(INIT, ERR, "%s: eemcs_debug_mod_init fail", FUNC_NAME);  
        ret = -CCCI_ERR_INIT_PLATFORM_FAIL;
        goto INIT_CCCI_DBG_FAIL;
    }

    eemcs_state_callback_init();
    change_device_state(EEMCS_GATE);

    if ((ret = mtlte_sys_sdio_driver_init()) != KAL_SUCCESS){
        DBGLOG(INIT, ERR, "%s: mtlte_sys_sdio_driver_init fail", FUNC_NAME);  
        ret = -CCCI_ERR_INIT_PLATFORM_FAIL;
        goto INIT_SDIO_DRIVER_FAIL;
    }
    if ((ret = eemcs_ccci_init()) != KAL_SUCCESS){
        DBGLOG(INIT, ERR, "%s: eemcs_boot_mod_init fail", FUNC_NAME);  
        goto INIT_CCCI_MOD_FAIL;
    }
    if ((ret = eemcs_boot_trace_init()) != KAL_SUCCESS){
        DBGLOG(INIT, ERR, "%s: eemcs_boot_trace_init fail", FUNC_NAME);  
        ret = -CCCI_ERR_INIT_PLATFORM_FAIL;
        goto INIT_BOOT_TRACE_FAIL;
    }
    if ((ret = eemcs_boot_mod_init()) != KAL_SUCCESS){
        DBGLOG(INIT, ERR, "%s: eemcs_boot_mod_init fail", FUNC_NAME);  
        ret = -CCCI_ERR_INIT_MD_CTRL_FAIL;
        goto INIT_BOOT_FAIL;
    }
    
    change_device_state(EEMCS_INIT);

    LOG_FUNC("[EEMCS/INIT] <==== %s", FUNC_NAME);
    return ret;
	
INIT_BOOT_FAIL:
    eemcs_boot_trace_deinit();
INIT_BOOT_TRACE_FAIL:
    eemcs_ccci_remove();
INIT_CCCI_MOD_FAIL:
    mtlte_sys_sdio_driver_exit();	
INIT_SDIO_DRIVER_FAIL:
    eemcs_debug_deinit();
INIT_CCCI_DBG_FAIL:
	
    return ret;
}

static void __exit eemcs_deinit(void)
{
    eemcs_statistics_exit();
    eemcs_boot_trace_deinit();
    eemcs_boot_exit();
    eemcs_ccci_remove();
    eemcs_debug_deinit();
    mtlte_sys_sdio_driver_exit();
    return;
}


late_initcall(eemcs_init);
//module_init(eemcs_init);
//module_exit(eemcs_deinit);
MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("Evolved EMCS for MT6290");
MODULE_LICENSE("GPL");
