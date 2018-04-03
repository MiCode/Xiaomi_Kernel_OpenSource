/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _NET_CNSS_LOGGER_H_
#define _NET_CNSS_LOGGER_H_

struct sk_buff;
struct wiphy;

#ifdef CONFIG_CNSS_LOGGER
int cnss_logger_event_register(int radio, int event,
			       int (*cb)(struct sk_buff *skb));
int cnss_logger_event_unregister(int radio, int event,
				 int (*cb)(struct sk_buff *skb));
int cnss_logger_device_register(struct wiphy *wiphy, const char *name);
int cnss_logger_device_unregister(int radio, struct wiphy *wiphy);
int cnss_logger_nl_ucast(struct sk_buff *skb, int portid, int flag);
int cnss_logger_nl_bcast(struct sk_buff *skb, int portid, int flag);
#else
static inline int cnss_logger_event_register(int radio, int event,
					     int (*cb)(struct sk_buff *skb))
{
	return 0;
}
static inline int cnss_logger_event_unregister(int radio, int event,
					       int (*cb)(struct sk_buff *skb))
{
	return 0;
}
static inline int cnss_logger_device_register(struct wiphy *wiphy,
					      const char *name)
{
	return 0;
}
static inline int cnss_logger_device_unregister(int radio, struct wiphy *wiphy)
{
	return 0;
}
static inline int cnss_logger_nl_ucast(struct sk_buff *skb, int portid,
				       int flag)
{
	return 0;
}
static inline int cnss_logger_nl_bcast(struct sk_buff *skb, int portid,
				       int flag)
{
	return 0;
}
#endif /* CONFIG_CNSS_LOGGER */
#endif /* _NET_CNSS_H_ */

