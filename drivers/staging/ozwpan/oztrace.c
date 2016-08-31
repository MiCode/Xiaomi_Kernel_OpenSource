/* -----------------------------------------------------------------------------
 * Copyright (c) 2011 Ozmo Inc
 * Released under the GNU General Public License Version 2 (GPLv2).
 * -----------------------------------------------------------------------------
 */
#include "oztrace.h"
#define CREATE_TRACE_POINTS
#include "ozeventtrace.h"

#define OZ_TRACE_DUMP_SKB_LEN_MAX 32
#define OZ_TRACE_DUMP_URB_LEN_MAX 16

u32 g_debug =
#ifdef WANT_TRACE_DATA_FLOW
	TRC_FLG(M)|TRC_FLG(R)|TRC_FLG(T)|
	TRC_FLG(S)|TRC_FLG(E)|TRC_FLG(C);
#else
	0;
#endif

void (*func[]) (char *fmt, va_list arg) = {
	trace_hcd_msg_evt,
	trace_isoc_msg_evt,
	trace_info_msg_evt
};

void oz_dump_data(char *buf, unsigned char *data, int len, int lmt)
{
	int i = 0;
	if (len > lmt)
		len = lmt;
	while (len--) {
		*buf = (*data>>4) + '0';
		if (*data > (0xA0-1))
			*buf += 'A' - '9' - 1;
		*++buf = (*data++&0xF) + '0';
		if (*buf > '9')
			*buf += 'A' - '9' - 1;
		if (buf++ && !(++i%4))
			*buf++ = ' ';
	}
	*buf++ = '\n';
	*buf   = 0;
}

void oz_trace_f_urb_in(struct urb *urb)
{
	int  i = 0;
	char buf[128*2];
	int endpoint = usb_pipeendpoint(urb->pipe);

	if (usb_pipein(urb->pipe))
		endpoint |= 0x80;

	if (endpoint == 0x00 || endpoint == 0x80) {
		i += sprintf(&buf[i], "OZ S %08X %02X %02X ",
			 (unsigned int)((uintptr_t)urb), endpoint,
			urb->transfer_buffer_length);

		oz_dump_data(&buf[i], urb->setup_packet, 8, 8);

	} else {
		i += sprintf(&buf[i], "OZ S %08X %02X %02X ",
			(unsigned int)((uintptr_t)urb), endpoint,
			urb->transfer_buffer_length);
		if (!usb_pipein(urb->pipe)) {
			oz_dump_data(&buf[i], (u8 *)(urb->transfer_buffer),
				urb->transfer_buffer_length,
				OZ_TRACE_DUMP_URB_LEN_MAX);

		} else {
			oz_dump_data(&buf[i], NULL, 0, 0);
		}

	}
	printk(buf);
}

void oz_trace_f_urb_out(struct urb *urb, int status)
{
	int  i = 0;
	char buf[128*2];
	int endpoint = usb_pipeendpoint(urb->pipe);
	int length = urb->actual_length;

	if (usb_pipeisoc(urb->pipe))
		length = urb->transfer_buffer_length;

	if (usb_pipein(urb->pipe))
		endpoint |= 0x80;

	if (status != 0) {
		printk("OZ E %08X %08X\n",
			(unsigned int)((uintptr_t)urb), status);
	} else {
		i += sprintf(&buf[i], "OZ C %08X %02X %02X ",
			(unsigned int)((uintptr_t)urb),
			endpoint, urb->actual_length);

		if (usb_pipein(urb->pipe)) {
			oz_dump_data(&buf[i],
			(u8 *)(urb->transfer_buffer),
			urb->actual_length,
			OZ_TRACE_DUMP_URB_LEN_MAX);
		} else {
			oz_dump_data(&buf[i], NULL, 0, 0);
		}
		printk(buf);
	}
}

void oz_trace_f_skb(struct sk_buff *skb, char dir)
{
	int  i = 0;
	char buf[128*2];
	int len = skb->len;

	if (dir == 'T')
		len -= 14;

	i += sprintf(&buf[i], "OZ %c %04X ", dir, len);
	oz_dump_data(&buf[i], (u8 *)skb_network_header(skb),
			len, OZ_TRACE_DUMP_SKB_LEN_MAX);
	printk(buf);
}

void oz_trace_f_dbg(void)
{
}

void trace_dbg_msg(int c, char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	func[c](fmt, arg);
	va_end(arg);
}

void trace_debug_log(char log_type, ...)
{
	va_list arg;
	char *fmt;

	va_start(arg, log_type);
	fmt = va_arg(arg, char *);
	switch (log_type) {
	case 'H':
		trace_hcd_msg_evt(fmt, arg);
		break;
	case 'I':
		trace_isoc_msg_evt(fmt, arg);
		break;
	 default:
		trace_info_msg_evt(fmt, arg);
		break;
	}
	va_end(arg);
}
