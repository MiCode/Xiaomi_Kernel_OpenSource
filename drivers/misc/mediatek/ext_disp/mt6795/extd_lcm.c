#include <linux/slab.h>

#include "extd_drv_log.h"
#include "lcm_drv.h"
#include "extd_platform.h"
#include "ddp_manager.h"
#include "extd_lcm.h"

// This macro and arrya is designed for multiple LCM support
// for multiple LCM, we should assign I/F Port id in lcm driver, such as DPI0, DSI0/1
static extd_drv_handle _extd_drv_driver[MAX_LCM_NUMBER] = {0};

// these 2 variables are defined in mt65xx_lcm_list.c

extern disp_ddp_path_config extd_dpi_params;
extern unsigned int lcm_count;
extern unsigned int hdmi_get_width();
extern unsigned int hdmi_get_height();

int _extd_drv_count(void)
{
	return 1;
}

int _is_extd_drv_inited(extd_drv_handle *plcm)
{
	if(plcm)
	{
		if(plcm->params)
			return 1;
	}
	else
	{
		DISPERR("WARNING, invalid lcm handle: %p\n", plcm);
		return 0;
	}
}

LCM_PARAMS *_get_extd_drv_params_by_handle(extd_drv_handle *plcm)
{
	if(plcm)
	{
		return plcm->params;
	}
	else
	{
		DISPERR("WARNING, invalid lcm handle: %p\n", plcm);
		return NULL;
	}
}

LCM_PARAMS *_get_extd_drv_driver_by_handle(extd_drv_handle *plcm)
{

	DISPERR("WARNING, invalid lcm handle: %p\n", plcm);
	return NULL;
}

void _dump_extd_drv_info(extd_drv_handle *plcm)
{
    int i = 0;
	 LCM_DRIVER *l = NULL;
    LCM_PARAMS *p = NULL;

	if(plcm == NULL)
	{
		DISPERR("plcm is null\n");
		return;
	}
	 
	 l = plcm->drv;
	 p = plcm->params;

	if(l && p)
	{
		DISPCHECK("[LCM] resolution: %d x %d\n", p->width, p->height);
		DISPCHECK("[LCM] physical size: %d x %d\n", p->physical_width, p->physical_height);
		DISPCHECK("[LCM] physical size: %d x %d\n", p->physical_width, p->physical_height);
		
		switch(p->lcm_if)
		{
			case LCM_INTERFACE_DSI0:
				DISPCHECK("[LCM] interface: DSI0\n");
				break;
			case LCM_INTERFACE_DSI1:
				DISPCHECK("[LCM] interface: DSI1\n");
				break;
			case LCM_INTERFACE_DPI0:
				DISPCHECK("[LCM] interface: DPI0\n");
				break;
			case LCM_INTERFACE_DPI1:
				DISPCHECK("[LCM] interface: DPI1\n");
				break;
			case LCM_INTERFACE_DBI0:
				DISPCHECK("[LCM] interface: DBI0\n");
				break;
			default:
				DISPCHECK("[LCM] interface: unknown\n");
				break;
		}

		switch(p->type)
		{
			case LCM_TYPE_DBI:
				DISPCHECK("[LCM] Type: DBI\n");
				break;
			case LCM_TYPE_DSI:
				DISPCHECK("[LCM] Type: DSI\n");

				break;
			case LCM_TYPE_DPI:
				DISPCHECK("[LCM] Type: DPI\n");
				break;
			default:
				DISPCHECK("[LCM] TYPE: unknown\n");
				break;
		}

		if(p->type == LCM_TYPE_DSI)
		{
			switch(p->dsi.mode)
			{
				case CMD_MODE:
					DISPCHECK("[LCM] DSI Mode: CMD_MODE\n");
					break;
				case SYNC_PULSE_VDO_MODE:
					DISPCHECK("[LCM] DSI Mode: SYNC_PULSE_VDO_MODE\n");
					break;
				case SYNC_EVENT_VDO_MODE:
					DISPCHECK("[LCM] DSI Mode: SYNC_EVENT_VDO_MODE\n");
					break;
				case BURST_VDO_MODE:
					DISPCHECK("[LCM] DSI Mode: BURST_VDO_MODE\n");
					break;
				default:
					DISPCHECK("[LCM] DSI Mode: Unknown\n");
					break;
			}		
		}
		
		if(p->type == LCM_TYPE_DSI)
		{
			///DISPCHECK("[LCM] LANE_NUM: %d,data_format: %d\n",p->dsi.LANE_NUM,p->dsi.data_format);
		#ifdef ROME_TODO
		#error
		#endif
			///DISPCHECK("[LCM] vact: %d, vbp: %d, vfp: %d, vact_line: %d, hact: %d, hbp: %d, hfp: %d, hblank: %d, hblank: %d\n",p->dsi.vertical_sync_active, p->dsi.vertical_backporch,p->dsi.vertical_frontporch,p->dsi.vertical_active_line,p->dsi.horizontal_sync_active,p->dsi.horizontal_backporch,p->dsi.horizontal_frontporch,p->dsi.horizontal_blanking_pixel);
			DISPCHECK("[LCM] pll_select: %d, pll_div1: %d, pll_div2: %d, fbk_div: %d,fbk_sel: %d, rg_bir: %d\n",p->dsi.pll_select,p->dsi.pll_div1,p->dsi.pll_div2,p->dsi.fbk_div,p->dsi.fbk_sel,p->dsi.rg_bir);
			DISPCHECK("[LCM] rg_bic: %d, rg_bp: %d,	PLL_CLOCK: %d, dsi_clock: %d, ssc_range: %d,	ssc_disable: %d, compatibility_for_nvk: %d, cont_clock: %d\n", p->dsi.rg_bic,	p->dsi.rg_bp,p->dsi.PLL_CLOCK,p->dsi.dsi_clock,p->dsi.ssc_range,p->dsi.ssc_disable,p->dsi.compatibility_for_nvk,p->dsi.cont_clock);
			DISPCHECK("[LCM] lcm_ext_te_enable: %d, noncont_clock: %d, noncont_clock_period: %d\n", p->dsi.lcm_ext_te_enable,p->dsi.noncont_clock,p->dsi.noncont_clock_period);
		}
	}

	return;
}


extd_drv_handle* extd_drv_probe(char* plcm_name, LCM_INTERFACE_ID lcm_id)
{
	///DISPFUNC();
	
	int ret = 0;
	bool isLCMFound = false;
	bool isLCMInited = false;

    LCM_DRIVER *lcm_drv = NULL;
	LCM_PARAMS *lcm_param = NULL;
	extd_drv_handle *plcm = NULL;
	DISPMSG("plcm_name=%s\n", plcm_name);
	
	if(_extd_drv_count() == 0)
	{
		DISPERR("no lcm driver defined in linux kernel driver\n");
		return NULL;
	}
	else if(_extd_drv_count() == 1)
	{
		if(plcm_name == NULL)
		{
			lcm_drv = NULL;

			isLCMFound = true;
			isLCMInited = true;
		}
		else
		{
			lcm_drv = NULL;
			
			isLCMInited = true;
			isLCMFound = true;
		}
	}
	
	if(isLCMFound == false)
	{
		DISPERR("FATAL ERROR!!!No LCM Driver defined\n");
		return NULL;
	}

    plcm = kzalloc(sizeof(uint8_t*) *sizeof(extd_drv_handle), GFP_KERNEL);
    lcm_param = kzalloc(sizeof(uint8_t*) *sizeof(LCM_PARAMS), GFP_KERNEL);
    
	if(plcm && lcm_param)
	{	    
	    lcm_param->type = LCM_TYPE_DPI;
	    lcm_param->ctrl = LCM_CTRL_NONE;
	    lcm_param->lcm_if = LCM_INTERFACE_DPI0;	    
	    lcm_param->width = hdmi_get_width();
	    lcm_param->height = hdmi_get_height();
	    lcm_param->physical_width = lcm_param->width;
	    lcm_param->physical_height = lcm_param->height;
	    
		plcm->params = lcm_param;
		plcm->drv = NULL;
		plcm->is_inited = isLCMInited;
		plcm->lcm_if_id = LCM_INTERFACE_DPI0;
	}
	else
	{
		DISPERR("FATAL ERROR!!!kzalloc plcm and plcm->params failed\n");
		goto FAIL;
	}
	
	{
		lcm_id = plcm->params->lcm_if;

		// below code is for lcm driver forward compatible
		if(plcm->params->type == LCM_TYPE_DSI && plcm->params->lcm_if == LCM_INTERFACE_NOTDEFINED) plcm->lcm_if_id = LCM_INTERFACE_DSI0;
		if(plcm->params->type == LCM_TYPE_DPI && plcm->params->lcm_if == LCM_INTERFACE_NOTDEFINED) plcm->lcm_if_id = LCM_INTERFACE_DPI0;
		if(plcm->params->type == LCM_TYPE_DBI && plcm->params->lcm_if == LCM_INTERFACE_NOTDEFINED) plcm->lcm_if_id = LCM_INTERFACE_DBI0;

		if((lcm_id == LCM_INTERFACE_NOTDEFINED) || lcm_id == plcm->lcm_if_id)
		{
			_dump_extd_drv_info(plcm);
			return plcm;
		}
		else
		{
			DISPERR("the specific LCM Interface [%d] didn't define any lcm driver\n", lcm_id);
			goto FAIL;
		}
	}

FAIL:
	
	if(plcm) kfree(plcm);
	if(lcm_param) kfree(lcm_param);
	return NULL;
}


int extd_drv_init(extd_drv_handle *plcm)
{
	DISPFUNC();
	LCM_DRIVER *lcm_drv = NULL;
	return 0;
	
	if(_is_extd_drv_inited(plcm))
	{
		lcm_drv = plcm->drv;
		if(lcm_drv->init)
		{
			lcm_drv->init();
		}
		else
		{
			DISPERR("FATAL ERROR, lcm_drv->init is null\n");
			return -1;
		}
		
		return 0;
	}
	else
	{
		DISPERR("plcm is null\n");
		return -1;
	}
}

LCM_PARAMS* extd_drv_get_params(extd_drv_handle *plcm)
{
	///DISPFUNC();

    if((plcm)&& (plcm->params))
    {
        plcm->params->width = hdmi_get_width();
        plcm->params->height = hdmi_get_height();
        memcpy(&(plcm->params->dpi), &(extd_dpi_params.dispif_config), sizeof(LCM_PARAMS));
    }
    
	if(_is_extd_drv_inited(plcm))
		return plcm->params;
	else
		return NULL;
}

LCM_INTERFACE_ID extd_drv_get_interface_id(extd_drv_handle *plcm)
{
	DISPFUNC();

	if(_is_extd_drv_inited(plcm))
		return plcm->lcm_if_id;
	else
		return LCM_INTERFACE_NOTDEFINED;
}

int extd_drv_update(extd_drv_handle *plcm, int x, int y, int w, int h, int force)
{
    ///DISPDBGFUNC();
		
    return 0;
}


int extd_drv_esd_check(extd_drv_handle *plcm)
{
	DISPFUNC();

	return 0;
}



int extd_drv_esd_recover(extd_drv_handle *plcm)
{
	DISPFUNC();
		
	return 0;
}




int extd_drv_suspend(extd_drv_handle *plcm)
{
	DISPFUNC();
	LCM_DRIVER *lcm_drv = NULL;
	return 0;
	
	if(_is_extd_drv_inited(plcm))
	{
		lcm_drv = plcm->drv;
		if(lcm_drv->suspend)
		{
			lcm_drv->suspend();
		}
		else
		{
			DISPERR("FATAL ERROR, lcm_drv->esd_check is null\n");
			return -1;
		}
		
		return 0;
	}
	else
	{
		DISPERR("lcm_drv is null\n");
		return -1;
	}
}




int extd_drv_resume(extd_drv_handle *plcm)
{
	DISPFUNC();
	LCM_DRIVER *lcm_drv = NULL;
	return 0;
	
	if(_is_extd_drv_inited(plcm))
	{
		lcm_drv = plcm->drv;
		if(lcm_drv->resume)
		{
			lcm_drv->resume();
		}
		else
		{
			DISPERR("FATAL ERROR, lcm_drv->esd_check is null\n");
			return -1;
		}
		
		return 0;
	}
	else
	{
		DISPERR("lcm_drv is null\n");
		return -1;
	}
}

int extd_drv_set_backlight(extd_drv_handle *plcm, int level)
{
	DISPFUNC();
		
	return 0;
}




int extd_drv_ioctl(extd_drv_handle *plcm, LCM_IOCTL ioctl, unsigned int arg)
{
	return 0;
}

int extd_drv_is_inited(extd_drv_handle *plcm)
{
	if(_is_extd_drv_inited(plcm))
		return plcm->is_inited;
	else
		return 0;
}

int extd_drv_is_video_mode(extd_drv_handle *plcm)
{
	///DISPFUNC();
	LCM_PARAMS *lcm_param = NULL;
	LCM_INTERFACE_ID lcm_id = LCM_INTERFACE_NOTDEFINED;

	return true;
	
}

