/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM cfg80211

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_CFG80211_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_CFG80211_H

#include <trace/hooks/vendor_hooks.h>

#ifdef __GENKSYMS__
#include <net/cfg80211.h>
#endif

struct wiphy;
struct wireless_dev;

DECLARE_HOOK(android_vh_cfg80211_set_context,
	TP_PROTO(struct wiphy *wiphy, struct wireless_dev *wdev, int context_id,
		 const void *data),
	TP_ARGS(wiphy, wdev, context_id, data));

DECLARE_HOOK(android_vh_cfg80211_get_context,
	TP_PROTO(struct wiphy *wiphy, struct wireless_dev *wdev, int context_id,
		 void *data, size_t max_data_len),
	TP_ARGS(wiphy, wdev, context_id, data, max_data_len));

#endif /* _TRACE_HOOK_CFG80211_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
