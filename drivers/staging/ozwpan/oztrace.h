/* -----------------------------------------------------------------------------
 * Copyright (c) 2011 Ozmo Inc
 * Released under the GNU General Public License Version 2 (GPLv2).
 * -----------------------------------------------------------------------------
 */
#ifndef _OZTRACE_H_
#define _OZTRACE_H_
#include <linux/usb.h>
#include <linux/netdevice.h>
#include "ozeventtrace.h"

extern struct device *g_oz_wpan_dev;

#define oz_trace(fmt, ...) \
	do { dev_dbg(g_oz_wpan_dev, fmt, ##__VA_ARGS__); } while (0)

void oz_trace_f_urb_out(struct urb *urb, int status);
void oz_trace_f_urb_in(struct urb *urb);
void oz_trace_f_skb(struct sk_buff *skb, char dir);
void oz_trace_f_dbg(void);
void trace_dbg_msg(int c, char *fmt, ...);
void trace_debug_log(char log_type, ...);

extern u32 g_debug;

#define TRC_A 'A'
#define TRC_B 'B'
#define TRC_C 'C'	/* urb Completion */
#define TRC_D 'D'	/* Debug */
#define TRC_E 'E'	/* urb Error */
#define TRC_F 'F'
#define TRC_G 'G'
#define TRC_H 'H'	/* Hcd message */
#define TRC_I 'I'	/* Isoc buffer depth */
#define TRC_J 'J'
#define TRC_K 'K'
#define TRC_L 'L'
#define TRC_M 'M'	/* Message */
#define TRC_N 'N'
#define TRC_O 'O'
#define TRC_P 'P'
#define TRC_Q 'Q'
#define TRC_R 'R'	/* Rx Ozmo frame */
#define TRC_S 'S'	/* urb Submission */
#define TRC_T 'T'	/* Tx ozmo frame */
#define TRC_U 'U'
#define TRC_V 'V'
#define TRC_W 'W'
#define TRC_X 'X'
#define TRC_Y 'Y'
#define TRC_Z 'Z'

#define TRC_FLG(f) (1<<((TRC_##f)-'A'))

#define oz_trace_urb_out(u, s) \
	do { if (!g_debug) \
		trace_urb_out(u, s); \
	else if ((g_debug & TRC_FLG(C)) ||\
			((g_debug & TRC_FLG(E)) && (s != 0))) \
		oz_trace_f_urb_out(u, s); } while (0)

#define oz_trace_urb_in(u) \
	do { if (!g_debug) \
		trace_urb_in(u); \
	else if (g_debug & TRC_FLG(S)) \
		oz_trace_f_urb_in(u); } while (0)

#define oz_trace_skb(u, d) \
	do { if ((!g_debug) && ('T' == d)) \
		trace_tx_frame(u); \
	else if ((!g_debug) && ('R' == d)) \
		trace_rx_frame(u); \
	else if ((('T' == d) && (g_debug & TRC_FLG(T))) || \
				(('R' == d) && (g_debug & TRC_FLG(R)))) \
		oz_trace_f_skb(u, d); } while(0)

#define oz_trace_msg(f, ...) \
	do { if (!g_debug) \
		trace_debug_log(TRC_##f, __VA_ARGS__); \
	else if (g_debug & TRC_FLG(f)) \
		printk("OZ " #f " " __VA_ARGS__); } while(0)

enum {
	TRACE_HCD_MSG,
	TRACE_ISOC_MSG,
	TRACE_INFO_MSG
};

#define trace_hcd_msg(fmt, ...)\
	trace_dbg_msg(TRACE_HCD_MSG, fmt, ##__VA_ARGS__)

#define trace_isoc_msg(fmt, ...)\
	trace_dbg_msg(TRACE_ISOC_MSG, fmt, ##__VA_ARGS__)

#define trace_info_msg(fmt, ...)\
	trace_dbg_msg(TRACE_INFO_MSG, fmt, ##__VA_ARGS__)

#endif /* Sentry */

