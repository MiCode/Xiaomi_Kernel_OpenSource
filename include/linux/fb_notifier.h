#ifndef _FB_DRM_NOTIFIER_H_
#define _FB_DRM_NOTIFIER_H_

/*
 *	This include file is intended for touch panel that to receive
 *	blank/unblank event.
 *
 */

#define		FB_DRM_EARLY_EVENT_BLANK   0x10
#define		FB_DRM_EVENT_BLANK         0x09

enum {
	FB_DRM_BLANK_UNBLANK = 0,
	FB_DRM_BLANK_POWERDOWN = 1,
};


struct fb_drm_notify_data {
	bool is_primary;
	void *data;
};

extern int fb_drm_register_client(struct notifier_block *nb);
extern int fb_drm_unregister_client(struct notifier_block *nb);
extern int fb_drm_notifier_call_chain(unsigned long val, void *v);

#endif /*_FB_DRM_NOTIFIER_H*/
