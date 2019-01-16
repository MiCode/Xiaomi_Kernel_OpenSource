/*******************************************************************************
 * File : eemcs_debug.c
 * [Functions]
 *        1. Create p_d_dentry nodes for runtime change p_d_dentry level
 * [TODO]
 *        
 *******************************************************************************/
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include "eemcs_debug.h"
#include "eemcs_kal.h"
#include "eemcs_ccci.h"


KAL_UINT64 g_eemcs_dbg_m[DBG_MODULE_NUM];
struct dentry* g_eemcs_dbg_dentry;

static void eemcs_debug_level_init(void);

/*******************************************************************************
 * /d/eemcs/p_d_dentry
 *******************************************************************************/
#define __debugfs_register(name,parent) \
do {    \
    p_f_dentry = debugfs_create_u64(#name,0600,parent,&g_eemcs_dbg_m[DBG_##name##_IDX]); \
    result = PTR_ERR(p_f_dentry); \
    if(IS_ERR(p_f_dentry)&& result != -ENODEV) \
    {   \
        DBGLOG(INIT,ERR, "create eemcs debug folder fail: %d", result); \
        goto eemcs_dbg_init_fail;}  \
}while(0)

KAL_INT32 eemcs_debug_mod_init(void)
{
    KAL_INT32 result = KAL_FAIL;
    struct dentry *p_f_dentry, *p_e_dentry, *p_d_dentry;

    eemcs_debug_level_init();
    
    DEBUG_LOG_FUNCTION_ENTRY;

    p_e_dentry = debugfs_create_dir("eemcs", NULL);
    if(!p_e_dentry){
        DBGLOG(INIT, ERR, "create eemcs folder fail");
        DEBUG_LOG_FUNCTION_LEAVE;
        return -ENOENT;
    }
    
    g_eemcs_dbg_dentry = p_e_dentry; 

    p_d_dentry = debugfs_create_dir("debug",p_e_dentry);
    if(!p_d_dentry)
    {
        DBGLOG(INIT, ERR, "create debug sub folder fail");    
        goto eemcs_dbg_init_fail;
    }

    __debugfs_register(INIT,p_d_dentry);
    __debugfs_register(MSDC,p_d_dentry);    
    __debugfs_register(SDIO,p_d_dentry);
    __debugfs_register(CCCI,p_d_dentry);
    __debugfs_register(NETD,p_d_dentry);
    __debugfs_register(FUNC,p_d_dentry);
    __debugfs_register(CHAR,p_d_dentry);
    __debugfs_register(BOOT,p_d_dentry);
    __debugfs_register(IPCD,p_d_dentry);
    __debugfs_register(FSUT,p_d_dentry);
    __debugfs_register(RPCD,p_d_dentry);
    __debugfs_register(EXPT,p_d_dentry);
    __debugfs_register(SMSG,p_d_dentry);
    __debugfs_register(PORE,p_d_dentry); 
    
    DEBUG_LOG_FUNCTION_LEAVE; 
    return KAL_SUCCESS;

eemcs_dbg_init_fail:
    DBGLOG(INIT, ERR, "eemcs_debug_mod_init fail");  
    debugfs_remove_recursive(g_eemcs_dbg_dentry);

    DEBUG_LOG_FUNCTION_LEAVE;
    return -ENOENT;
}

void eemcs_debug_deinit(void)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    debugfs_remove_recursive(g_eemcs_dbg_dentry);
    DEBUG_LOG_FUNCTION_LEAVE;
    return;
}
static void eemcs_debug_level_init(void){

#if 0
    g_eemcs_dbg_m[DBG_INIT_IDX]=0xff;
    g_eemcs_dbg_m[DBG_MSDC_IDX]=0xff;
    g_eemcs_dbg_m[DBG_SDIO_IDX]=0xff;
    g_eemcs_dbg_m[DBG_CCCI_IDX]=0xff;
    g_eemcs_dbg_m[DBG_FUNC_IDX]=0xff;
    g_eemcs_dbg_m[DBG_NETD_IDX]=0xff;
    g_eemcs_dbg_m[DBG_CHAR_IDX]=0xff;
    g_eemcs_dbg_m[DBG_BOOT_IDX]=0xff;
    g_eemcs_dbg_m[DBG_FSUT_IDX]=0xff;
    g_eemcs_dbg_m[DBG_RPCD_IDX]=0xff;
    g_eemcs_dbg_m[DBG_EXPT_IDX]=0xff;
    g_eemcs_dbg_m[DBG_PORE_IDX]=0xff;     
#else
    g_eemcs_dbg_m[DBG_INIT_IDX]=(DBG_LEVEL_ERR|DBG_LEVEL_WAR|DBG_LEVEL_STA|DBG_LEVEL_INF);
    g_eemcs_dbg_m[DBG_MSDC_IDX]=(DBG_LEVEL_ERR|DBG_LEVEL_WAR);
    g_eemcs_dbg_m[DBG_SDIO_IDX]=(DBG_LEVEL_ERR|DBG_LEVEL_WAR);
    g_eemcs_dbg_m[DBG_CCCI_IDX]=(DBG_LEVEL_ERR|DBG_LEVEL_WAR|DBG_LEVEL_INF);
    g_eemcs_dbg_m[DBG_FUNC_IDX]=(DBG_LEVEL_ERR|DBG_LEVEL_WAR);
    g_eemcs_dbg_m[DBG_NETD_IDX]=(DBG_LEVEL_ERR|DBG_LEVEL_WAR);
    g_eemcs_dbg_m[DBG_CHAR_IDX]=(DBG_LEVEL_ERR|DBG_LEVEL_WAR|DBG_LEVEL_INF);
    g_eemcs_dbg_m[DBG_IPCD_IDX]=(DBG_LEVEL_ERR|DBG_LEVEL_WAR|DBG_LEVEL_INF);
    g_eemcs_dbg_m[DBG_BOOT_IDX]=(DBG_LEVEL_ERR|DBG_LEVEL_WAR|DBG_LEVEL_STA|DBG_LEVEL_INF);
    g_eemcs_dbg_m[DBG_FSUT_IDX]=0xFF;
    g_eemcs_dbg_m[DBG_RPCD_IDX]=(DBG_LEVEL_ERR|DBG_LEVEL_WAR);
    g_eemcs_dbg_m[DBG_EXPT_IDX]=0xFF;
    g_eemcs_dbg_m[DBG_SMSG_IDX]=(DBG_LEVEL_ERR|DBG_LEVEL_WAR|DBG_LEVEL_INF);
    /* PORE char dev log enable = 1 << port_id */
    g_eemcs_dbg_m[DBG_PORE_IDX]=0;
    g_eemcs_dbg_m[DBG_SYSF_IDX]=(DBG_LEVEL_ERR|DBG_LEVEL_WAR|DBG_LEVEL_INF);
    
#endif

}
