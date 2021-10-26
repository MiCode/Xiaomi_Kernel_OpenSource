#ifndef _MTK_DRM_ARR_H_
#define _MTK_DRM_ARR_H_

/*************************************************************************
 * mtk_dsi_lfr_con records LFR Related parameters belows:
 *
 *[ 1- 0] lfr_mode:     Low frame rate mode 0:Disable
 *[ 3- 2] lfr_type:     Low frame rate transmission type
 *[ 4- 4] lfr_enable:   Enables low frame rate
 *[ 5- 5] lfr_update:   Low frame rate update in dynamic mode
 *[ 6- 6] lfr_vse_dis:  Disables low frame rate VSE
 *[ 8-13] lfr_skip_num: Low frame rate skip frame number
 *
 *--lfr_mode   --	0: Disable, 1:static mode, 2:Dynamic mode, 3:Both
 *--lfr_type   --	0: LP mode, 1: Vsync, 2: Hsync, 3: Vsync + Hsync
 *--lfr_enable --	0: Disable, 1: Enable
 *--lfr_update --	0: No effect, 1: Update frame
 *--lfr_vse_dis--	0: Enable VSE packet, 1: Disable VSE packet

*************************************************************************/
enum LFR_MODE {
	LFR_MODE_DISABLE = 0,
	LFR_MODE_STATIC_MODE,
	LFR_MODE_DYNAMIC_MODE,
	LFR_MODE_BOTH_MODE,
	LFR_MODE_NUM
};

enum LFR_TYPE {
	LFR_TYPE_LP_MODE = 0,
	LFR_TYPE_VSYNC_ONLY,
	LFR_TYPE_HSYNC_ONLY,
	LFR_TYPE_BOTH_MODE,
	LFR_TYPE_NUM
};

struct mtk_dsi_lfr_con {
	unsigned int lfr_mode;
	unsigned int lfr_type;
	unsigned int lfr_enable;
	unsigned int lfr_update;
	unsigned int lfr_vse_dis;
	unsigned int lfr_skip_num;
	unsigned int lfr_mask;
};
/*************************************************************************
 * mtk_dsi_lfr_sta records LFR state and skip count:
 *
 *[ 5 - 0] lfr_skip_count: Low frame rate skip frame counter
 *[ 8 - 8] lfr_skip_sta: Low frame rate skip frame status

*************************************************************************/
struct mtk_dsi_lfr_sta {
	unsigned int lfr_skip_count;
	unsigned int lfr_skip_sta;
};

/*DISP_OPT_ARR_PHASE_1
 * register call back for fpsgo or other kernel modules
 * who want't to monitor frame rate changing
 */
 /*interface with fpsgo*/
typedef void (*FPS_CHG_CALLBACK)(unsigned int new_fps);
int drm_register_fps_chg_callback(
	FPS_CHG_CALLBACK fps_chg_cb);
int drm_unregister_fps_chg_callback(
	FPS_CHG_CALLBACK fps_chg_cb);
/*interface with primary_display*/
void drm_invoke_fps_chg_callbacks(unsigned int new_fps);

#endif
