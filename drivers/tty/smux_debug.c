/* drivers/tty/smux_debug.c
 *
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/debugfs.h>
#include <linux/list.h>
#include <linux/ctype.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/termios.h>
#include <linux/smux.h>
#include "smux_private.h"

#define DEBUG_BUFMAX 4096



/**
 * smux_dump_ch() - Dumps the information of a channel to the screen.
 * @buf:  Buffer for status message.
 * @max: Size of status queue.
 * @lch_number:  Number of the logical channel.
 * @lch:  Pointer to the lch_number'th instance of struct smux_lch_t.
 *
 */
static int smux_dump_ch(char *buf, int max, struct smux_lch_t *lch)
{
	int bytes_written;
	long tiocm_bits;
	unsigned long flags;

	spin_lock_irqsave(&lch->state_lock_lhb1, flags);
	spin_lock(&lch->tx_lock_lhb2);

	tiocm_bits = msm_smux_tiocm_get_atomic(lch);

	bytes_written = scnprintf(
		buf, max,
		"ch%02d: "
		"%s(%s) "
		"%c%c%c%c%c  "
		"%d  "
		"%s(%s) "
		"%c%c\n",
		lch->lcid,
		local_lch_state(lch->local_state), lch_mode(lch->local_mode),
		(tiocm_bits & TIOCM_DSR) ? 'D' : 'd',
		(tiocm_bits & TIOCM_CTS) ? 'T' : 't',
		(tiocm_bits & TIOCM_RI) ? 'I' : 'i',
		(tiocm_bits & TIOCM_CD) ? 'C' : 'c',
		lch->tx_flow_control ? 'F' : 'f',
		lch->tx_pending_data_cnt,
		remote_lch_state(lch->remote_state), lch_mode(lch->remote_mode),
		(tiocm_bits & TIOCM_DTR) ? 'R' : 'r',
		(tiocm_bits & TIOCM_RTS) ? 'S' : 's'
		);

	spin_unlock(&lch->tx_lock_lhb2);
	spin_unlock_irqrestore(&lch->state_lock_lhb1, flags);

	return bytes_written;
}

/**
 * smux_dump_format_ch() - Informs user of format for channel dump
 * @buf:  Buffer for status message.
 * @max:  Size of status queue.
 *
 */
static int smux_dump_format_ch(char *buf, int max)
{
	return scnprintf(
		buf, max,
		"ch_id "
		"local state(mode) tiocm  "
		"tx_queue  "
		"remote state(mode) tiocm\n"
		"local tiocm: DSR(D) CTS(T) RI(I) DCD(C) FLOW_CONTROL(F)\n"
		"remote tiocm: DTR(R) RTS(S)\n"
		"A capital letter indicates set, otherwise, it is not set.\n\n"
		);
}

/**
 * smux_debug_ch() - Log following information about each channel
 * local open, local mode, remote open, remote mode,
 * tiocm bits, flow control state and transmit queue size.
 * Returns the number of bytes written to buf.
 * @buf Buffer for status message
 * @max Size of status queue
 *
 */
static int smux_debug_ch(char *buf, int max)
{
	int ch_id;

	int bytes_written = smux_dump_format_ch(buf, max);

	for (ch_id = 0; ch_id < SMUX_NUM_LOGICAL_CHANNELS; ch_id++) {
		bytes_written += smux_dump_ch(buf + bytes_written,
					max - bytes_written,
					&smux_lch[ch_id]);
	}

	return bytes_written;

}

static char debug_buffer[DEBUG_BUFMAX];

static ssize_t debug_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos)
{
	int (*fill)(char *buf, int max) = file->private_data;
	int bsize;

	if (*ppos != 0)
		return 0;

	bsize = fill(debug_buffer, DEBUG_BUFMAX);
	return simple_read_from_buffer(buf, count, ppos, debug_buffer, bsize);
}

static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static const struct file_operations debug_ops = {
	.read = debug_read,
	.open = debug_open,
};


static void debug_create(const char *name, mode_t mode,
			struct dentry *dent,
			int (*fill)(char *buf, int max))
{
	debugfs_create_file(name, mode, dent, fill, &debug_ops);
}


static int __init smux_debugfs_init(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("n_smux", 0);
	if (IS_ERR(dent))
		return PTR_ERR(dent);

	debug_create("ch", 0444, dent, smux_debug_ch);


	return 0;
}

late_initcall(smux_debugfs_init);
