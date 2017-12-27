/*
 * u_uvc.h
 *
 * Utility definitions for the uvc function
 *
 * Copyright (c) 2013-2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Andrzej Pietrasiewicz <andrzej.p@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef U_UVC_H
#define U_UVC_H

#include <linux/usb/composite.h>

/* module parameters specific to the Video streaming endpoint */
#define USB_VIDEO_MODULE_PARAMETERS()					\
	static unsigned int streaming_interval = 1;			\
	module_param(streaming_interval, uint, S_IRUGO|S_IWUSR);	\
	MODULE_PARM_DESC(streaming_interval, "1 - 16");			\
									\
	static unsigned int streaming_maxpacket = 3072;			\
	module_param(streaming_maxpacket, uint, S_IRUGO|S_IWUSR);	\
	MODULE_PARM_DESC(streaming_maxpacket, "1-1023 (FS), 1-3072 (hs/ss)"); \
									\
	static unsigned int streaming_maxburst = 2;			\
	module_param(streaming_maxburst, uint, S_IRUGO|S_IWUSR);	\
	MODULE_PARM_DESC(streaming_maxburst, "0 - 15 (ss only)");	\
									\
	static unsigned int trace;					\
	module_param(trace, uint, S_IRUGO|S_IWUSR);			\
	MODULE_PARM_DESC(trace, "Trace level bitmask")

#define to_f_uvc_opts(f)	container_of(f, struct f_uvc_opts, func_inst)

struct f_uvc_opts {
	struct usb_function_instance			func_inst;
	unsigned int					uvc_gadget_trace_param;
	unsigned int					streaming_interval;
	unsigned int					streaming_maxpacket;
	unsigned int					streaming_maxburst;
	const struct uvc_descriptor_header * const	*fs_control;
	const struct uvc_descriptor_header * const	*ss_control;
	const struct uvc_descriptor_header * const	*fs_streaming;
	const struct uvc_descriptor_header * const	*hs_streaming;
	const struct uvc_descriptor_header * const	*ss_streaming;
};

void uvc_set_trace_param(unsigned int trace);

#endif /* U_UVC_H */

