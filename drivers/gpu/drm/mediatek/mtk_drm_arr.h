#ifndef _MTK_DRM_ARR_H_
#define _MTK_DRM_ARR_H_

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
