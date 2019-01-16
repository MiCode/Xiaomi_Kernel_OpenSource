#ifndef _DISP_LCM_H_
#define _DISP_LCM_H_
#include "lcm_drv.h"

#define MAX_LCM_NUMBER	2


typedef struct
{
	LCM_PARAMS 			*params;
	LCM_DRIVER 			*drv;
	LCM_INTERFACE_ID	lcm_if_id;
	int					module;
	int					is_inited;
}extd_drv_handle, *pextd_drv_handle;

extd_drv_handle* extd_drv_probe(char* plcm_name, LCM_INTERFACE_ID lcm_id);
int extd_drv_init(extd_drv_handle *plcm);
LCM_PARAMS* extd_drv_get_params(extd_drv_handle *plcm);
LCM_INTERFACE_ID extd_drv_get_interface_id(extd_drv_handle *plcm);
int extd_drv_update(extd_drv_handle *plcm, int x, int y, int w, int h, int force);
int extd_drv_esd_check(extd_drv_handle *plcm);
int extd_drv_esd_recover(extd_drv_handle *plcm);
int extd_drv_suspend(extd_drv_handle *plcm);
int extd_drv_resume(extd_drv_handle *plcm);
int extd_drv_set_backlight(extd_drv_handle *plcm, int level);
int extd_drv_read_fb(extd_drv_handle *plcm);
int extd_drv_ioctl(extd_drv_handle *plcm, LCM_IOCTL ioctl, unsigned int arg);
int extd_drv_is_video_mode(extd_drv_handle *plcm);
int extd_drv_is_inited(extd_drv_handle *plcm);

#endif

