#ifndef _DRM_NOTIFIER_H_
#define _DRM_NOTIFIER_H_

/*
 *	This include file is intended for touch panel that to receive
 *	blank/unblank event.
 *
 */

#define		DRM_EARLY_EVENT_BLANK   0x01
#define		DRM_EVENT_BLANK         0x02
#define		DRM_R_EARLY_EVENT_BLANK 0x03
#define		DRM_FOD_EVENT           0x04

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
	void *data;
};

extern int drm_register_client(struct notifier_block *nb);
extern int drm_unregister_client(struct notifier_block *nb);
extern int drm_notifier_call_chain(unsigned long val, void *v);

#endif /*_DRM_NOTIFIER_H*/
