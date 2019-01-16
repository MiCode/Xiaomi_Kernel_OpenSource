
#define LOG_TAG "MMP"

#include "ddp_mmp.h"
#include "ddp_reg.h"
#include "ddp_log.h"

#include <mach/m4u.h>
#include "DpDataType.h"

#ifdef DEFAULT_MMP_ENABLE
extern void MMProfileEnable(int enable);
extern void MMProfileStart(int start);
#endif

static DDP_MMP_Events_t DDP_MMP_Events;

void init_ddp_mmp_events(void)
{
    	if (DDP_MMP_Events.DDP == 0)
    	{
		DDP_MMP_Events.DDP = MMProfileRegisterEvent(MMP_RootEvent, "Display");
		DDP_MMP_Events.primary_Parent =      MMProfileRegisterEvent(DDP_MMP_Events.DDP, "primary_disp");
		DDP_MMP_Events.primary_trigger =     MMProfileRegisterEvent(DDP_MMP_Events.primary_Parent, "trigger");
		DDP_MMP_Events.primary_config  =     MMProfileRegisterEvent(DDP_MMP_Events.primary_Parent, "ovl_config");
        DDP_MMP_Events.primary_rdma_config  = MMProfileRegisterEvent(DDP_MMP_Events.primary_Parent, "rdma_config");
        DDP_MMP_Events.primary_wdma_config  = MMProfileRegisterEvent(DDP_MMP_Events.primary_Parent, "wdma_config");
		DDP_MMP_Events.primary_set_dirty=    MMProfileRegisterEvent(DDP_MMP_Events.primary_Parent, "set_dirty");
		DDP_MMP_Events.primary_cmdq_flush=   MMProfileRegisterEvent(DDP_MMP_Events.primary_Parent, "cmdq_flush");
		DDP_MMP_Events.primary_cmdq_done =  MMProfileRegisterEvent(DDP_MMP_Events.primary_Parent, "cmdq_done");
		DDP_MMP_Events.primary_display_cmd =  MMProfileRegisterEvent(DDP_MMP_Events.primary_Parent, "display_io");
		DDP_MMP_Events.primary_suspend =  MMProfileRegisterEvent(DDP_MMP_Events.primary_Parent, "suspend");
		DDP_MMP_Events.primary_resume =  MMProfileRegisterEvent(DDP_MMP_Events.primary_Parent, "resume");
		DDP_MMP_Events.primary_cache_sync =  MMProfileRegisterEvent(DDP_MMP_Events.primary_Parent, "cache_sync");
		DDP_MMP_Events.primary_wakeup =  MMProfileRegisterEvent(DDP_MMP_Events.primary_Parent, "wakeup");
		DDP_MMP_Events.interface_trigger=  MMProfileRegisterEvent(DDP_MMP_Events.primary_Parent, "interface_trigger");
		DDP_MMP_Events.primary_switch_mode=  MMProfileRegisterEvent(DDP_MMP_Events.primary_Parent, "switch_session_mode");

		DDP_MMP_Events.primary_seq_info = MMProfileRegisterEvent(DDP_MMP_Events.primary_Parent, "seq_info");
		DDP_MMP_Events.primary_seq_insert = MMProfileRegisterEvent(DDP_MMP_Events.primary_seq_info, "seq_insert");
		DDP_MMP_Events.primary_seq_config=  MMProfileRegisterEvent(DDP_MMP_Events.primary_seq_info, "seq_config");
		DDP_MMP_Events.primary_seq_trigger=  MMProfileRegisterEvent(DDP_MMP_Events.primary_seq_info, "seq_trigger");
		DDP_MMP_Events.primary_seq_rdma_irq=  MMProfileRegisterEvent(DDP_MMP_Events.primary_seq_info, "seq_rdma_irq");
		DDP_MMP_Events.primary_seq_release=  MMProfileRegisterEvent(DDP_MMP_Events.primary_seq_info, "seq_release");

        DDP_MMP_Events.primary_ovl_fence_release=  MMProfileRegisterEvent(DDP_MMP_Events.primary_Parent, "ovl_fence_r");
        DDP_MMP_Events.primary_wdma_fence_release=  MMProfileRegisterEvent(DDP_MMP_Events.primary_Parent, "wdma_fence_r");

        DDP_MMP_Events.present_fence_release =  MMProfileRegisterEvent(DDP_MMP_Events.primary_Parent, "preset_fence_release");
        DDP_MMP_Events.present_fence_get =  MMProfileRegisterEvent(DDP_MMP_Events.primary_Parent, "preset_fence_get");
        DDP_MMP_Events.present_fence_set =  MMProfileRegisterEvent(DDP_MMP_Events.primary_Parent, "preset_fence_set");
        
        DDP_MMP_Events.idlemgr =  MMProfileRegisterEvent(DDP_MMP_Events.primary_Parent, "idlemgr");
        DDP_MMP_Events.primary_error =  MMProfileRegisterEvent(DDP_MMP_Events.primary_Parent, "primary_error");

#ifdef CONFIG_MTK_HDMI_SUPPORT
        DDP_MMP_Events.Extd_Parent =      MMProfileRegisterEvent(DDP_MMP_Events.DDP, "ext_disp");
        DDP_MMP_Events.Extd_State = MMProfileRegisterEvent(DDP_MMP_Events.Extd_Parent, "ext_State");
        DDP_MMP_Events.Extd_DevInfo = MMProfileRegisterEvent(DDP_MMP_Events.Extd_Parent, "ext_DevInfo");
        DDP_MMP_Events.Extd_ErrorInfo = MMProfileRegisterEvent(DDP_MMP_Events.Extd_Parent, "ext_ErrorInfo");
        DDP_MMP_Events.Extd_Mutex = MMProfileRegisterEvent(DDP_MMP_Events.Extd_Parent, "ext_Mutex");        
        DDP_MMP_Events.Extd_ImgDump = MMProfileRegisterEvent(DDP_MMP_Events.Extd_Parent, "ext_ImgDump");
        DDP_MMP_Events.Extd_IrqStatus = MMProfileRegisterEvent(DDP_MMP_Events.Extd_Parent, "ext_IrqStatus");
        DDP_MMP_Events.Extd_UsedBuff = MMProfileRegisterEvent(DDP_MMP_Events.Extd_Parent, "ext_UsedBuf");
        DDP_MMP_Events.Extd_trigger =     MMProfileRegisterEvent(DDP_MMP_Events.Extd_Parent, "ext_trigger");
		DDP_MMP_Events.Extd_config  =     MMProfileRegisterEvent(DDP_MMP_Events.Extd_Parent, "ext_config");
		DDP_MMP_Events.Extd_set_dirty=    MMProfileRegisterEvent(DDP_MMP_Events.Extd_Parent, "ext_set_dirty");
		DDP_MMP_Events.Extd_cmdq_flush=   MMProfileRegisterEvent(DDP_MMP_Events.Extd_Parent, "ext_cmdq_flush");
		DDP_MMP_Events.Extd_cmdq_done =  MMProfileRegisterEvent(DDP_MMP_Events.Extd_Parent, "ext_cmdq_done");

#endif

		DDP_MMP_Events.primary_display_aalod_trigger = MMProfileRegisterEvent(DDP_MMP_Events.primary_Parent, "primary_aal_trigger");	
		DDP_MMP_Events.ESD_Parent          = MMProfileRegisterEvent(DDP_MMP_Events.DDP, "ESD");
		DDP_MMP_Events.esd_check_t         = MMProfileRegisterEvent(DDP_MMP_Events.ESD_Parent, "ESD_Check");
		DDP_MMP_Events.esd_recovery_t      = MMProfileRegisterEvent(DDP_MMP_Events.ESD_Parent, "ESD_Recovery");
		DDP_MMP_Events.esd_extte           = MMProfileRegisterEvent(DDP_MMP_Events.esd_check_t, "ESD_Check_EXT_TE");
		DDP_MMP_Events.esd_rdlcm           = MMProfileRegisterEvent(DDP_MMP_Events.esd_check_t, "ESD_Check_RD_LCM");
		DDP_MMP_Events.esd_vdo_eint        = MMProfileRegisterEvent(DDP_MMP_Events.esd_extte, "ESD_Vdo_EINT");
		DDP_MMP_Events.primary_set_bl      = MMProfileRegisterEvent(DDP_MMP_Events.primary_Parent, "set_BL_LCM");
		DDP_MMP_Events.dprec_cpu_write_reg = DDP_MMP_Events.MutexParent = MMProfileRegisterEvent(DDP_MMP_Events.DDP, "dprec_cpu_write_reg");		
		DDP_MMP_Events.session_Parent =      MMProfileRegisterEvent(DDP_MMP_Events.DDP, "session");
		
		DDP_MMP_Events.ovl_trigger=  MMProfileRegisterEvent(DDP_MMP_Events.session_Parent, "ovl2mem");
		DDP_MMP_Events.layerParent = MMProfileRegisterEvent(DDP_MMP_Events.DDP, "Layer");
		DDP_MMP_Events.layer[0] = MMProfileRegisterEvent(DDP_MMP_Events.layerParent, "Layer0");
		DDP_MMP_Events.layer[1] = MMProfileRegisterEvent(DDP_MMP_Events.layerParent, "Layer1");
		DDP_MMP_Events.layer[2] = MMProfileRegisterEvent(DDP_MMP_Events.layerParent, "Layer2");
		DDP_MMP_Events.layer[3] = MMProfileRegisterEvent(DDP_MMP_Events.layerParent, "Layer3");

        DDP_MMP_Events.ovl1_layer[0] = MMProfileRegisterEvent(DDP_MMP_Events.layerParent, "Ovl1_Layer0");
        DDP_MMP_Events.ovl1_layer[1] = MMProfileRegisterEvent(DDP_MMP_Events.layerParent, "Ovl1_Layer1");
        DDP_MMP_Events.ovl1_layer[2] = MMProfileRegisterEvent(DDP_MMP_Events.layerParent, "Ovl1_Layer2");
        DDP_MMP_Events.ovl1_layer[3] = MMProfileRegisterEvent(DDP_MMP_Events.layerParent, "Ovl1_Layer3");
        
        DDP_MMP_Events.layer_dump_parent = MMProfileRegisterEvent(DDP_MMP_Events.layerParent, "layerBmpDump");           
		DDP_MMP_Events.layer_dump[0] = MMProfileRegisterEvent(DDP_MMP_Events.layer_dump_parent, "layer0_dump");
		DDP_MMP_Events.layer_dump[1] = MMProfileRegisterEvent(DDP_MMP_Events.layer_dump_parent, "Layer1_dump");
		DDP_MMP_Events.layer_dump[2] = MMProfileRegisterEvent(DDP_MMP_Events.layer_dump_parent, "Layer2_dump");
		DDP_MMP_Events.layer_dump[3] = MMProfileRegisterEvent(DDP_MMP_Events.layer_dump_parent, "Layer3_dump");

		DDP_MMP_Events.ovl1layer_dump[0] = MMProfileRegisterEvent(DDP_MMP_Events.layer_dump_parent, "ovl1layer0_dump");
		DDP_MMP_Events.ovl1layer_dump[1] = MMProfileRegisterEvent(DDP_MMP_Events.layer_dump_parent, "ovl1layer1_dump");
		DDP_MMP_Events.ovl1layer_dump[2] = MMProfileRegisterEvent(DDP_MMP_Events.layer_dump_parent, "ovl1layer2_dump");
		DDP_MMP_Events.ovl1layer_dump[3] = MMProfileRegisterEvent(DDP_MMP_Events.layer_dump_parent, "ovl1layer3_dump");

		DDP_MMP_Events.wdma_dump[0] = MMProfileRegisterEvent(DDP_MMP_Events.layer_dump_parent, "wdma0_dump");
		DDP_MMP_Events.wdma_dump[1] = MMProfileRegisterEvent(DDP_MMP_Events.layer_dump_parent, "wdma1_dump");

		DDP_MMP_Events.rdma_dump[0] = MMProfileRegisterEvent(DDP_MMP_Events.layer_dump_parent, "rdma0_dump");
		DDP_MMP_Events.rdma_dump[1] = MMProfileRegisterEvent(DDP_MMP_Events.layer_dump_parent, "rdma1_dump");

		DDP_MMP_Events.DDP_IRQ = MMProfileRegisterEvent(DDP_MMP_Events.DDP, "DDP_IRQ");
		DDP_MMP_Events.MutexParent = MMProfileRegisterEvent(DDP_MMP_Events.DDP_IRQ, "Mutex");
		DDP_MMP_Events.MUTEX_IRQ[0] = MMProfileRegisterEvent(DDP_MMP_Events.MutexParent, "Mutex0");
		DDP_MMP_Events.MUTEX_IRQ[1] = MMProfileRegisterEvent(DDP_MMP_Events.MutexParent, "Mutex1");
		DDP_MMP_Events.MUTEX_IRQ[2] = MMProfileRegisterEvent(DDP_MMP_Events.MutexParent, "Mutex2");
		DDP_MMP_Events.MUTEX_IRQ[3] = MMProfileRegisterEvent(DDP_MMP_Events.MutexParent, "Mutex3");
		DDP_MMP_Events.MUTEX_IRQ[4] = MMProfileRegisterEvent(DDP_MMP_Events.MutexParent, "Mutex4"); 
		DDP_MMP_Events.OVL_IRQ_Parent = MMProfileRegisterEvent(DDP_MMP_Events.DDP_IRQ, "OVL_IRQ");
		DDP_MMP_Events.OVL_IRQ[0] = MMProfileRegisterEvent(DDP_MMP_Events.OVL_IRQ_Parent, "OVL_IRQ_0");
		DDP_MMP_Events.OVL_IRQ[1] = MMProfileRegisterEvent(DDP_MMP_Events.OVL_IRQ_Parent, "OVL_IRQ_1");
		DDP_MMP_Events.WDMA_IRQ_Parent = MMProfileRegisterEvent(DDP_MMP_Events.DDP_IRQ, "WDMA_IRQ");
		DDP_MMP_Events.WDMA_IRQ[0] = MMProfileRegisterEvent(DDP_MMP_Events.WDMA_IRQ_Parent, "WDMA_IRQ_0");
		DDP_MMP_Events.WDMA_IRQ[1] = MMProfileRegisterEvent(DDP_MMP_Events.WDMA_IRQ_Parent, "WDMA_IRQ_1");
		DDP_MMP_Events.RDMA_IRQ_Parent = MMProfileRegisterEvent(DDP_MMP_Events.DDP_IRQ, "RDMA_IRQ");
		DDP_MMP_Events.RDMA_IRQ[0] = MMProfileRegisterEvent(DDP_MMP_Events.RDMA_IRQ_Parent, "RDMA_IRQ_0");
		DDP_MMP_Events.RDMA_IRQ[1] = MMProfileRegisterEvent(DDP_MMP_Events.RDMA_IRQ_Parent, "RDMA_IRQ_1");
		DDP_MMP_Events.RDMA_IRQ[2] = MMProfileRegisterEvent(DDP_MMP_Events.RDMA_IRQ_Parent, "RDMA_IRQ_2");
		DDP_MMP_Events.SCREEN_UPDATE[0] = MMProfileRegisterEvent(DDP_MMP_Events.session_Parent, "SCREEN_UPDATE_0");
		DDP_MMP_Events.SCREEN_UPDATE[1] = MMProfileRegisterEvent(DDP_MMP_Events.RDMA_IRQ_Parent, "SCREEN_UPDATE_1");
		DDP_MMP_Events.SCREEN_UPDATE[2] = MMProfileRegisterEvent(DDP_MMP_Events.RDMA_IRQ_Parent, "SCREEN_UPDATE_2");
		DDP_MMP_Events.DSI_IRQ_Parent = MMProfileRegisterEvent(DDP_MMP_Events.DDP_IRQ, "DSI_IRQ");
		DDP_MMP_Events.DSI_IRQ[0] = MMProfileRegisterEvent(DDP_MMP_Events.DSI_IRQ_Parent, "DSI_IRQ_0");
		DDP_MMP_Events.DSI_IRQ[1] = MMProfileRegisterEvent(DDP_MMP_Events.DSI_IRQ_Parent, "DSI_IRQ_1");
		DDP_MMP_Events.primary_sw_mutex = MMProfileRegisterEvent(DDP_MMP_Events.DDP, "primary_sw_mutex");	   
		DDP_MMP_Events.session_release = MMProfileRegisterEvent(DDP_MMP_Events.session_Parent, "session_release");	   

		DDP_MMP_Events.MonitorParent = MMProfileRegisterEvent(DDP_MMP_Events.DDP, "Monitor");
		DDP_MMP_Events.rdma_underflow = MMProfileRegisterEvent(DDP_MMP_Events.MonitorParent, "RDMA Underflow");
		DDP_MMP_Events.cg_mode = MMProfileRegisterEvent(DDP_MMP_Events.MonitorParent, "SPM CG Mode");
		DDP_MMP_Events.power_down_mode = MMProfileRegisterEvent(DDP_MMP_Events.MonitorParent, "SPM Power Down Mode");
		DDP_MMP_Events.sodi_disable = MMProfileRegisterEvent(DDP_MMP_Events.MonitorParent, "Request CG");
		DDP_MMP_Events.sodi_enable = MMProfileRegisterEvent(DDP_MMP_Events.MonitorParent, "Request Power Down");
		DDP_MMP_Events.vsync_count = MMProfileRegisterEvent(DDP_MMP_Events.MonitorParent, "Vsync Ticket");
		
		DDP_MMP_Events.dal_clean= MMProfileRegisterEvent(DDP_MMP_Events.DDP, "DAL Clean");
		DDP_MMP_Events.dal_printf= MMProfileRegisterEvent(DDP_MMP_Events.DDP, "DAL Printf");
		DDP_MMP_Events.tmp_debug= MMProfileRegisterEvent(DDP_MMP_Events.DDP, "tmp_debug");
		

		MMProfileEnableEventRecursive(DDP_MMP_Events.DDP, 1);
    	MMProfileEnableEventRecursive(DDP_MMP_Events.layerParent, 1);       
    	MMProfileEnableEventRecursive(DDP_MMP_Events.MutexParent, 1);
    	MMProfileEnableEventRecursive(DDP_MMP_Events.DDP_IRQ, 1);

    	MMProfileEnableEvent(DDP_MMP_Events.primary_sw_mutex, 0);
    	MMProfileEnableEventRecursive(DDP_MMP_Events.primary_seq_info, 0);
	}
}

void ddp_mmp_ovl_layer(OVL_CONFIG_STRUCT* pLayer,unsigned int down_sample_x,unsigned int down_sample_y,unsigned int session/*1:primary, 2:external, 3:memory*/)
{
    MMP_MetaDataBitmap_t Bitmap;
    MMP_MetaData_t meta;
    int raw = 0;

	if(session == 1)
    	MMProfileLogEx(DDP_MMP_Events.layer_dump_parent,MMProfileFlagStart, pLayer->layer, pLayer->layer_en);
	else if(session == 2)
		MMProfileLogEx(DDP_MMP_Events.Extd_layer_dump_parent,MMProfileFlagStart, pLayer->layer, pLayer->layer_en);

    if (pLayer->layer_en)
    {
        Bitmap.data1 = pLayer->vaddr;
        Bitmap.width = pLayer->dst_w;
        Bitmap.height = pLayer->dst_h;
        switch (pLayer->fmt)
        {
            case eRGB565:
            case eBGR565: 
                Bitmap.format = MMProfileBitmapRGB565; 
                Bitmap.bpp = 16;
                break;
            case eRGB888: 
                Bitmap.format = MMProfileBitmapRGB888; 
                Bitmap.bpp = 24;
                break;
            case eBGRA8888:   
                Bitmap.format = MMProfileBitmapBGRA8888; 
                Bitmap.bpp = 32;
                break;
            case eBGR888: 
                Bitmap.format = MMProfileBitmapBGR888; 
                Bitmap.bpp = 24; 
                break;
            case eRGBA8888:
                Bitmap.format = MMProfileBitmapRGBA8888; 
                Bitmap.bpp = 32; 
                break;
            default:
              DDPERR("ddp_mmp_ovl_layer(), unknow fmt=%d, dump raw\n", pLayer->fmt);
              raw = 1;
        }
        if(!raw)
        {
            Bitmap.start_pos =0;
            Bitmap.pitch = pLayer->src_pitch;
            Bitmap.data_size = Bitmap.pitch * Bitmap.height;
            Bitmap.down_sample_x = down_sample_x;
            Bitmap.down_sample_y = down_sample_y;
            if (m4u_mva_map_kernel(pLayer->addr, Bitmap.data_size,(unsigned long*)&Bitmap.pData, &Bitmap.data_size)==0)
            {
            	if(session == 1)
                	MMProfileLogMetaBitmap(DDP_MMP_Events.layer_dump[pLayer->layer], MMProfileFlagPulse, &Bitmap);
				else if(session == 2)
					MMProfileLogMetaBitmap(DDP_MMP_Events.ovl1layer_dump[pLayer->layer], MMProfileFlagPulse, &Bitmap);
                m4u_mva_unmap_kernel(pLayer->addr, Bitmap.data_size, (unsigned long)Bitmap.pData);
            }
            else
            {
                DDPERR("ddp_mmp_ovl_layer(),fail to dump rgb(0x%x)\n", pLayer->fmt);
            }
        }
        else
        {
            meta.data_type = MMProfileMetaRaw;
            meta.size = pLayer->src_pitch * pLayer->src_h;
            if(m4u_mva_map_kernel(pLayer->addr, meta.size, (unsigned long*)&meta.pData, &meta.size)==0)
            {
            	if(session == 1)
                	MMProfileLogMeta(DDP_MMP_Events.layer_dump[pLayer->layer], MMProfileFlagPulse, &meta);
				else if(session == 2)
					MMProfileLogMeta(DDP_MMP_Events.ovl1layer_dump[pLayer->layer], MMProfileFlagPulse, &meta);
                m4u_mva_unmap_kernel(pLayer->addr, meta.size, (unsigned long)meta.pData);
            }
            else
            {
                DDPERR("ddp_mmp_ovl_layer(),fail to dump raw(0x%x)\n", pLayer->fmt);
            }            
        }
    }

	if(session == 1)
    	MMProfileLogEx(DDP_MMP_Events.layer_dump_parent,MMProfileFlagEnd, pLayer->fmt, pLayer->addr);
	else if(session == 2)
		MMProfileLogEx(DDP_MMP_Events.Extd_layer_dump_parent,MMProfileFlagEnd, pLayer->fmt, pLayer->addr);
	
    return ;
}

void ddp_mmp_wdma_layer(WDMA_CONFIG_STRUCT *wdma_layer,unsigned int wdma_num, unsigned int down_sample_x,unsigned int down_sample_y)
{
    if(wdma_num > 1)
    {
        DDPERR("dprec_mmp_dump_wdma_layer is error %d\n",wdma_num);
        return;
    }
    
    MMP_MetaDataBitmap_t Bitmap;
    MMP_MetaData_t meta;
    int raw = 0;

    Bitmap.data1 = wdma_layer->dstAddress;
    Bitmap.width = wdma_layer->srcWidth;
    Bitmap.height = wdma_layer->srcHeight;
    switch (wdma_layer->outputFormat)
    {
        case eRGB565:
        case eBGR565: 
            Bitmap.format = MMProfileBitmapRGB565; 
            Bitmap.bpp = 16;
            break;
        case eRGB888: 
            Bitmap.format = MMProfileBitmapRGB888; 
            Bitmap.bpp = 24;
            break;
        case eBGRA8888:   
            Bitmap.format = MMProfileBitmapBGRA8888; 
            Bitmap.bpp = 32;
            break;
        case eBGR888: 
            Bitmap.format = MMProfileBitmapBGR888; 
            Bitmap.bpp = 24; 
            break;
        case eRGBA8888:
            Bitmap.format = MMProfileBitmapRGBA8888; 
            Bitmap.bpp = 32; 
            break;
        default:
          DDPERR("dprec_mmp_dump_wdma_layer(), unknow fmt=%d, dump raw\n", wdma_layer->outputFormat);
          raw = 1;
    }
    if(!raw)
    {
        Bitmap.start_pos =0;
        Bitmap.pitch = wdma_layer->dstPitch;
        Bitmap.data_size = Bitmap.pitch * Bitmap.height;
        Bitmap.down_sample_x = down_sample_x;
        Bitmap.down_sample_y = down_sample_y;
        if (m4u_mva_map_kernel(wdma_layer->dstAddress, Bitmap.data_size,(unsigned long*)&Bitmap.pData, &Bitmap.data_size)==0)
        {
            MMProfileLogMetaBitmap(DDP_MMP_Events.wdma_dump[wdma_num], MMProfileFlagPulse, &Bitmap);
            m4u_mva_unmap_kernel(wdma_layer->dstAddress, Bitmap.data_size, (unsigned long)Bitmap.pData);
        }
        else
        {
            DDPERR("dprec_mmp_dump_wdma_layer(),fail to dump rgb(0x%x)\n", wdma_layer->outputFormat);
        }
    }
    else
    {
        meta.data_type = MMProfileMetaRaw;
        meta.size = wdma_layer->dstPitch * wdma_layer->srcHeight;
        if(m4u_mva_map_kernel(wdma_layer->dstAddress, meta.size, (unsigned long*)&meta.pData, &meta.size)==0)
        {
            MMProfileLogMeta(DDP_MMP_Events.wdma_dump[wdma_num], MMProfileFlagPulse, &meta);
        }
        else
        {
            DDPERR("dprec_mmp_dump_wdma_layer(),fail to dump raw(0x%x)\n", wdma_layer->outputFormat);
        }            
    }
    
    return 0;

}

DDP_MMP_Events_t * ddp_mmp_get_events(void)
{
    return &DDP_MMP_Events;
}

void ddp_mmp_init(void)
{
#ifdef DEFAULT_MMP_ENABLE
    DDPMSG("ddp_mmp_init\n");
    MMProfileEnable(1);
    init_ddp_mmp_events();
    MMProfileStart(1);
#endif
}
