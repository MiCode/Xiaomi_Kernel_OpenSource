#include <linux/mutex.h>
#include "mtk_drm_arr.h"
#include "mtk_log.h"


static DEFINE_MUTEX(cb_table_lock);
#define DISP_MAX_FPSCHG_CALLBACK 5
static FPS_CHG_CALLBACK fps_chg_callback_table[DISP_MAX_FPSCHG_CALLBACK];

/****************ARR function start************************/
int drm_register_fps_chg_callback(FPS_CHG_CALLBACK fps_chg_cb)
{
	int ret = 0;
	int i = 0;
	int j = 0;

	mutex_lock(&cb_table_lock);

	for (i = 0; i < DISP_MAX_FPSCHG_CALLBACK; i++) {
		if (fps_chg_callback_table[i] == fps_chg_cb) {
			DDPMSG("[fps]:%s re-register cb\n",
				__func__);
			mutex_unlock(&cb_table_lock);
			return ret;
		}
	}
	for (j = 0; j < DISP_MAX_FPSCHG_CALLBACK; j++) {
		if (fps_chg_callback_table[j] == NULL) {
			fps_chg_callback_table[j] = fps_chg_cb;
			DDPMSG("[fps]: %s, entry[%d] done!\n", __func__, j);
			break;
		}
	}
	if (j == DISP_MAX_FPSCHG_CALLBACK) {
		DDPPR_ERR("[fps]: %s no entries left for new cb!\n", __func__);
		ret = -1;
	}
	mutex_unlock(&cb_table_lock);
	return ret;
}

int drm_unregister_fps_chg_callback(FPS_CHG_CALLBACK fps_chg_cb)
{
	int ret = 0;
	int i = 0;

	mutex_lock(&cb_table_lock);
	for (i = 0; i < DISP_MAX_FPSCHG_CALLBACK; i++) {
		if (fps_chg_callback_table[i] == fps_chg_cb) {
			fps_chg_callback_table[i] = NULL;
			DDPMSG("[fps]: %s, register cb %p\n",
				__func__, fps_chg_cb);
		}
	}
	if (i == DISP_MAX_FPSCHG_CALLBACK) {
		DDPPR_ERR("[fps]: %s, haven't registered cb %p\n",
			__func__, fps_chg_cb);
		ret = -1;
	}
	mutex_unlock(&cb_table_lock);
	return ret;
}

void drm_invoke_fps_chg_callbacks(unsigned int new_fps)
{
	unsigned int i = 0;

	DDPMSG("[fps]: %s,new_fps =%d\n", __func__, new_fps);
	mutex_lock(&cb_table_lock);
	for (i = 0; i < DISP_MAX_FPSCHG_CALLBACK; i++) {
		if (fps_chg_callback_table[i]) {
			fps_chg_callback_table[i](new_fps);
			DDPINFO("%s callback %u\n", __func__, i);
		}
	}
	mutex_unlock(&cb_table_lock);
}
