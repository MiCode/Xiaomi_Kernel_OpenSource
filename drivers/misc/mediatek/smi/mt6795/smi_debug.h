#ifndef __MT6795_SMI_DEBUG_H__
#define __MT6795_SMI_DEBUG_H__

#define SMI_DBG_DISPSYS (1<<0)
#define SMI_DBG_VDEC (1<<1)
#define SMI_DBG_IMGSYS (1<<2)
#define SMI_DBG_VENC (1<<3)
#define SMI_DBG_MJC (1<<4)

#define SMI_DGB_LARB_SELECT(smi_dbg_larb,n) ((smi_dbg_larb) & (1<<n))

#ifndef CONFIG_MTK_SMI
    #define smi_debug_bus_hanging_detect(larbs, show_dump) {}
#else
    int smi_debug_bus_hanging_detect(unsigned int larbs, int show_dump);
#endif


#endif //__MT6795_SMI_DEBUG_H__

