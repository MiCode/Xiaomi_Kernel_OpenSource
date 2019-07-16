/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#include <linux/ioctl.h>
#include <linux/types.h>

extern struct usb_function_instance *alloc_inst_mtp_ptp(bool mtp_config);
extern struct usb_function *function_alloc_mtp_ptp(
			struct usb_function_instance *fi, bool mtp_config);

struct mtp_file_range {
    /* file descriptor for file to transfer */
	int         fd;
    /* offset in file for start of transfer */
	loff_t      offset;
    /* number of bytes to transfer */
	int64_t     length;
    /* MTP command ID for data header,
     * used only for MTP_SEND_FILE_WITH_HEADER
     */
	uint16_t    command;
    /* MTP transaction ID for data header,
     * used only for MTP_SEND_FILE_WITH_HEADER
     */
	uint32_t    transaction_id;
};

struct mtp_event {
    /* size of the event */
	size_t      length;
    /* event data to send */
	void        *data;
};

/* Sends the specified file range to the host */
#define MTP_SEND_FILE              _IOW('M', 0, struct mtp_file_range)
/* Receives data from the host and writes it to a file.
 * The file is created if it does not exist.
 */
#define MTP_RECEIVE_FILE           _IOW('M', 1, struct mtp_file_range)
/* Sends an event to the host via the interrupt endpoint */
#define MTP_SEND_EVENT             _IOW('M', 3, struct mtp_event)
/* Sends the specified file range to the host,
 * with a 12 byte MTP data packet header at the beginning.
 */
#define MTP_SEND_FILE_WITH_HEADER  _IOW('M', 4, struct mtp_file_range)

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#ifdef __KERNEL__
#ifdef CONFIG_COMPAT
struct __compat_mtp_event {
	compat_size_t   length;
	compat_caddr_t  data;
};

#endif
#endif

#define COMPAT_MTP_SEND_EVENT   _IOW('M', 3, struct __compat_mtp_event)


