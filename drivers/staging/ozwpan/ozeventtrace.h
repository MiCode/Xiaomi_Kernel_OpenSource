/* -----------------------------------------------------------------------------
 * Copyright (c) 2011 Ozmo Inc
 * Released under the GNU General Public License Version 2 (GPLv2).
 * -----------------------------------------------------------------------------
 */


#undef TRACE_SYSTEM
#define TRACE_SYSTEM ozwpan

#if !defined(_OZEVENTTRACE_H) || defined(TRACE_HEADER_MULTI_READ)

#define _OZEVENTTRACE_H

#include <linux/tracepoint.h>
#include <linux/usb.h>

#define MAX_URB_LEN 16
#define MAX_FRAME_LEN 32
#define MAX_MSG_LEN 128

TRACE_EVENT(urb_in,

	TP_PROTO(struct urb *oz_urb),

	TP_ARGS(oz_urb),

	TP_STRUCT__entry(
	__field(uintptr_t, urb)
	__field(u32, endpoint)
	__field(u32, buffer_length)
	__field(u32, inc_length)
	__array(u8, buffer, MAX_URB_LEN)
	),

	TP_fast_assign(
		__entry->urb		=	(uintptr_t)oz_urb;
		__entry->endpoint	=	usb_pipeendpoint(oz_urb->pipe);
		if (usb_pipein(oz_urb->pipe))
			__entry->endpoint |= 0x80;
		__entry->buffer_length = oz_urb->transfer_buffer_length;
		__entry->inc_length = oz_urb->transfer_buffer_length
		 <= MAX_URB_LEN ? oz_urb->transfer_buffer_length : MAX_URB_LEN;
		if ((__entry->endpoint == 0x00) ||
					(__entry->endpoint == 0x80)) {
			__entry->buffer_length = 8;
			__entry->inc_length = 8;
			memcpy(__entry->buffer, oz_urb->setup_packet, 8);
		} else {
			memcpy(__entry->buffer, oz_urb->transfer_buffer,
						__entry->inc_length);
		}
	),

	TP_printk("%08x,%02x,%03x,%03x,"
	 "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
	(u32)__entry->urb, __entry->endpoint, __entry->buffer_length,
	__entry->inc_length, __entry->buffer[0], __entry->buffer[1],
	__entry->buffer[2], __entry->buffer[3], __entry->buffer[4],
	__entry->buffer[5], __entry->buffer[6], __entry->buffer[7],
	__entry->buffer[8], __entry->buffer[9], __entry->buffer[10],
	__entry->buffer[11], __entry->buffer[12], __entry->buffer[13],
	__entry->buffer[14], __entry->buffer[15])
);

TRACE_EVENT(urb_out,

	TP_PROTO(struct urb *oz_urb, int status),

	TP_ARGS(oz_urb, status),

	TP_STRUCT__entry(
	__field(uintptr_t, urb)
	__field(u32, endpoint)
	__field(u32, status)
	__field(u32, actual_length)
	__field(u32, inc_length)
	__array(u8, buffer, MAX_URB_LEN)
	),

	TP_fast_assign(
		__entry->urb		=	(uintptr_t)oz_urb;
		__entry->endpoint	=	usb_pipeendpoint(oz_urb->pipe);
		__entry->status		=	status;
		if (usb_pipein(oz_urb->pipe))
			__entry->endpoint |= 0x80;
		__entry->actual_length = oz_urb->actual_length;
		__entry->inc_length = oz_urb->actual_length
		 <= MAX_URB_LEN ? oz_urb->actual_length : MAX_URB_LEN;
		if (usb_pipein(oz_urb->pipe))
			memcpy(__entry->buffer, oz_urb->transfer_buffer,
						__entry->inc_length);
	),

	TP_printk("%08x,%08x,%02x,%03x,%03x,"
	 "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
	(u32)__entry->urb, __entry->status, __entry->endpoint,
	__entry->actual_length,	__entry->inc_length, __entry->buffer[0],
	__entry->buffer[1], __entry->buffer[2], __entry->buffer[3],
	__entry->buffer[4], __entry->buffer[5], __entry->buffer[6],
	__entry->buffer[7], __entry->buffer[8], __entry->buffer[9],
	__entry->buffer[10], __entry->buffer[11], __entry->buffer[12],
	__entry->buffer[13], __entry->buffer[14], __entry->buffer[15])
);

TRACE_EVENT(rx_frame,

	TP_PROTO(struct sk_buff *skb),

	TP_ARGS(skb),

	TP_STRUCT__entry(
	__field(u32, inc_len)
	__field(u32, orig_len)
	__array(u8, data, MAX_FRAME_LEN)
	),

	TP_fast_assign(
		__entry->orig_len	=	skb->len;
		__entry->inc_len	=	skb->len < MAX_FRAME_LEN ?
						skb->len : MAX_FRAME_LEN;
		memcpy(__entry->data, (u8 *)skb_network_header(skb),
				__entry->inc_len);
	),

	TP_printk("%03x,%03x,%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
	"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
	"%02x%02x%02x", __entry->orig_len,
	__entry->inc_len, __entry->data[0], __entry->data[1],
	__entry->data[2], __entry->data[3], __entry->data[4],
	__entry->data[5], __entry->data[6], __entry->data[7],
	__entry->data[8], __entry->data[9], __entry->data[10],
	__entry->data[11], __entry->data[12], __entry->data[13],
	__entry->data[14], __entry->data[15], __entry->data[16],
	__entry->data[17], __entry->data[18], __entry->data[19],
	__entry->data[20], __entry->data[21], __entry->data[22],
	__entry->data[23], __entry->data[24], __entry->data[25],
	__entry->data[26], __entry->data[27], __entry->data[28],
	__entry->data[29], __entry->data[30], __entry->data[31])
);

TRACE_EVENT(tx_frame,

	TP_PROTO(struct sk_buff *skb),

	TP_ARGS(skb),

	TP_STRUCT__entry(
	__field(u32, inc_len)
	__field(u32, orig_len)
	__array(u8, data, MAX_FRAME_LEN)
	),

	TP_fast_assign(
		__entry->orig_len	=	skb->len - 14;
		__entry->inc_len	=	__entry->orig_len
						 < MAX_FRAME_LEN ?
						__entry->orig_len
						: MAX_FRAME_LEN;
		memcpy(__entry->data, (u8 *)skb_network_header(skb),
				__entry->inc_len);
	),

	TP_printk("%03x,%03x,%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
	"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
	"%02x%02x%02x", __entry->orig_len,
	__entry->inc_len, __entry->data[0], __entry->data[1],
	__entry->data[2], __entry->data[3], __entry->data[4],
	__entry->data[5], __entry->data[6], __entry->data[7],
	__entry->data[8], __entry->data[9], __entry->data[10],
	__entry->data[11], __entry->data[12], __entry->data[13],
	__entry->data[14], __entry->data[15], __entry->data[16],
	__entry->data[17], __entry->data[18], __entry->data[19],
	__entry->data[20], __entry->data[21], __entry->data[22],
	__entry->data[23], __entry->data[24], __entry->data[25],
	__entry->data[26], __entry->data[27], __entry->data[28],
	__entry->data[29], __entry->data[30], __entry->data[31])
);

DECLARE_EVENT_CLASS(debug_msg,

	TP_PROTO(char *fmt, va_list arg),

	TP_ARGS(fmt, arg),

	TP_STRUCT__entry(
	__array(char, msg, MAX_MSG_LEN)
	),

	TP_fast_assign(
		snprintf(__entry->msg, MAX_MSG_LEN, fmt, arg);
	),

	TP_printk("%s", __entry->msg)
);

DEFINE_EVENT(debug_msg, hcd_msg_evt,
	TP_PROTO(char *fmt, va_list arg),
	TP_ARGS(fmt, arg)
);

DEFINE_EVENT(debug_msg, isoc_msg_evt,
	TP_PROTO(char *fmt, va_list arg),
	TP_ARGS(fmt, arg)
);

DEFINE_EVENT(debug_msg, info_msg_evt,
	TP_PROTO(char *fmt, va_list arg),
	TP_ARGS(fmt, arg)
);


#endif /*_OZEVENTTRACE_H*/

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE ozeventtrace
#include <trace/define_trace.h>
