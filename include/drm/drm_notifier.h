#ifndef _DRM_NOTIFIER_H_
#define _DRM_NOTIFIER_H_

/*
 *	This include file is intended for touch panel that to receive
 *	blank/unblank event.
 *
 */
#include <linux/notifier.h>

#define		DRM_EARLY_EVENT_BLANK   0x01
#define		DRM_EVENT_BLANK         0x02
#define		DRM_R_EARLY_EVENT_BLANK 0x03
#define 	DRM_FOD_EVENT           0x04
#define 	DRM_FPS_EVENT           0xF628

enum {
	/* panel: power on */
	MSM_DRM_BLANK_UNBLANK,
	/* panel: power off */
	MSM_DRM_BLANK_POWERDOWN,
};

enum msm_drm_display_id {
	/* primary display */
	MSM_DRM_PRIMARY_DISPLAY,
	/* external display */
	MSM_DRM_EXTERNAL_DISPLAY,
	MSM_DRM_DISPLAY_MAX
};

enum {
	DRM_BLANK_UNBLANK = 0,
	DRM_BLANK_LP1,
	DRM_BLANK_LP2,
	DRM_BLANK_STANDBY,
	DRM_BLANK_SUSPEND,
	DRM_BLANK_POWERDOWN,
};

enum {
	FOD_FINGERDOWN = 0,
	FOD_FINGERUP,
};

struct drm_notify_data {
	bool is_primary;
	enum msm_drm_display_id id;
	void *data;
};

extern int drm_register_client(struct notifier_block *nb);
extern int drm_unregister_client(struct notifier_block *nb);
extern int drm_notifier_call_chain(unsigned long val, void *v);

#endif /*_DRM_NOTIFIER_H*/
