/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#ifndef _USB_BAM_H_
#define _USB_BAM_H_

/**
 * Connect USB-to-Periperal SPS connection.
 *
 * This function returns the allocated pipes number.
 *
 * @idx - Connection index.
 *
 * @src_pipe_idx - allocated pipe index - USB as a
 *  source (output)
 *
 * @dst_pipe_idx - allocated pipe index - USB as a
 * destination (output)
 *
 * @return 0 on success, negative value on error
 *
 */
#ifdef CONFIG_USB_BAM
int usb_bam_connect(u8 idx, u8 *src_pipe_idx, u8 *dst_pipe_idx);
#else
int usb_bam_connect(u8 idx, u8 *src_pipe_idx, u8 *dst_pipe_idx)
{
	return -ENODEV;
}
#endif
#endif				/* _USB_BAM_H_ */
