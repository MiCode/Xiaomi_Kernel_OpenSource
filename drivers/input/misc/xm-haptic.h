#ifndef _XM_HAPTIC_H_
#define _XM_HAPTIC_H_

#include <linux/module.h>
#include <linux/kernel.h>
#include <stdarg.h>
#include <linux/device/class.h>
#include <linux/printk.h>
#if IS_ENABLED(CONFIG_MIEV)
#include "miev/mievent.h"
#endif

static bool xm_hap_exception_enable = false;

// Exception Code
#define XM_HAP_REGISTER_EXCEPTION_CODE	   906201001
#define XM_HAP_HBOOST_EXCEPTION_CODE	   906201002
#define XM_HAP_F0_PROTECT_EXCEPTION_CODE   906202001
#define XM_HAP_F0_CAL_EXCEPTION_CODE	   906202002

// Exception Trigger
#define XM_HAP_REGISTER_EXCEPTION(type, keyword) \
	xm_hap_driver_exception_handler(XM_HAP_REGISTER_EXCEPTION_CODE, type, keyword)
#define XM_HAP_HBOOST_EXCEPTION(raceObject, keyword) \
	xm_hap_driver_exception_handler(XM_HAP_HBOOST_EXCEPTION_CODE, raceObject, keyword)
#define XM_HAP_F0_PROTECT_EXCEPTION(calVal, keyword) \
	xm_hap_driver_exception_handler(XM_HAP_F0_PROTECT_EXCEPTION_CODE, calVal, keyword)
#define XM_HAP_F0_CAL_EXCEPTION(calVal, defaultVal, keyword) \
	xm_hap_driver_exception_handler(XM_HAP_F0_CAL_EXCEPTION_CODE, calVal, defaultVal, keyword)

// Exception Event Format
#define XM_HAP_REGISTER_EXCEPTION_EVENT(event, type, keyword) \
	cdev_tevent_add_str(event, "type", type); \
	cdev_tevent_add_str(event, "keyword", keyword)
#define XM_HAP_HBOOST_EXCEPTION_EVENT(event, raceObject, keyword) \
	cdev_tevent_add_str(event, "raceObject", raceObject); \
	cdev_tevent_add_str(event, "keyword", keyword)
#define XM_HAP_F0_PROTECT_EXCEPTION_EVENT(event, calVal, keyword) \
	cdev_tevent_add_int(event, "calVal", calVal); \
	cdev_tevent_add_str(event, "keyword", keyword)
#define XM_HAP_F0_CAL_EXCEPTION_EVENT(event, calVal, defaultVal,keyword) \
	cdev_tevent_add_int(event, "calVal", calVal); \
	cdev_tevent_add_int(event, "defaultVal", defaultVal); \
	cdev_tevent_add_str(event, "keyword", keyword)

#define SEND_EVENT_AND_RELEASE(event) \
	cdev_tevent_write(event); \
	cdev_tevent_destroy(event)

static void xm_hap_driver_exception_handler(int exception_code, ...) {

#if IS_ENABLED(CONFIG_MIEV)
	struct misight_mievent *event  = cdev_tevent_alloc(exception_code);
	va_list	args;

	if (!xm_hap_exception_enable)
		return;

	va_start(args, exception_code);

	switch (exception_code) {
		case XM_HAP_REGISTER_EXCEPTION_CODE:
			XM_HAP_REGISTER_EXCEPTION_EVENT(event, va_arg(args, char *), va_arg(args, char *));
			break;
		case XM_HAP_HBOOST_EXCEPTION_CODE:
			XM_HAP_HBOOST_EXCEPTION_EVENT(event, va_arg(args, char *), va_arg(args, char *));
			break;
		case XM_HAP_F0_PROTECT_EXCEPTION_CODE:
			XM_HAP_F0_PROTECT_EXCEPTION_EVENT(event, va_arg(args, int), va_arg(args, char *));
			break;
		case XM_HAP_F0_CAL_EXCEPTION_CODE:
			XM_HAP_F0_CAL_EXCEPTION_EVENT(event, va_arg(args, int), va_arg(args, int), va_arg(args, char *));
			break;
		default:
			pr_err("%s: exception code %d not found", __func__, exception_code);
			break;
	}

	va_end(args);

	SEND_EVENT_AND_RELEASE(event);

#endif

	pr_info("%s: exception code %d", __func__, exception_code);
}

static void xm_hap_driver_init(bool enable) {
	xm_hap_exception_enable = enable;
	pr_info("%s: TODO.", __func__);
}

#endif
