// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <linux/gpio.h>

#include "main.h"
#include "comms.h"

#define READ_ACK_TIMEOUT_MS	10
#define READ_MSG_TIMEOUT_MS	70

static const u32 crc_table[] = {
	0x00000000, 0x04C11DB7, 0x09823B6E, 0x0D4326D9,
	0x130476DC, 0x17C56B6B, 0x1A864DB2, 0x1E475005,
	0x2608EDB8, 0x22C9F00F, 0x2F8AD6D6, 0x2B4BCB61,
	0x350C9B64, 0x31CD86D3, 0x3C8EA00A, 0x384FBDBD
};

static u32 crc32_word(u32 crc, u32 data, int cnt)
{
	int i;

	crc = crc ^ data;
	for (i = 0; i < cnt; i++)
		crc = (crc << 4) ^ crc_table[crc >> 28];

	return crc;
}

u32 crc32(const u8 *buffer, int length, u32 crc)
{
	u32 *data = (u32 *)buffer;
	u32 word;
	int i;

	/* word by word crc32 */
	for (i = 0; i < (length >> 2); i++)
		crc = crc32_word(crc, data[i], 8);

	/* zero pad last word if required */
	if (length & 0x3) {
		for (i *= 4, word = 0; i < length; i++)
			word |= buffer[i] << ((i & 0x3) * 8);
		crc = crc32_word(crc, word, 8);
	}

	return crc;
}

static inline size_t pad(size_t length)
{
	return (length + 3) & ~3;
}

static inline size_t tot_len(size_t length)
{
	/* [TYPE:1] [LENGTH:3] [DATA] [PAD:0-3] [CRC:4] */
	return sizeof(u32) + pad(length) + sizeof(u32);
}

static struct nanohub_packet_pad *packet_alloc(int flags)
{
	int len =
	    sizeof(struct nanohub_packet_pad) + MAX_UINT8 +
	    sizeof(struct nanohub_packet_crc);
	u8 *packet = kmalloc(len, flags);

	if (packet)
		memset(packet, 0xFF, len);
	return (struct nanohub_packet_pad *)packet;
}

static int packet_create(struct nanohub_packet *packet, u32 seq,
			 u32 reason, u8 len, const u8 *data,
			 bool user)
{
	struct nanohub_packet_crc crc;
	int ret = sizeof(struct nanohub_packet) + len +
	    sizeof(struct nanohub_packet_crc);

	if (packet) {
		packet->sync = COMMS_SYNC;
		packet->seq = seq;
		packet->reason = reason;
		packet->len = len;
		if (len > 0) {
			if (user) {
				if (copy_from_user(packet->data, data, len) !=
				    0)
					ret = ERROR_NACK;
			} else {
				memcpy(packet->data, data, len);
			}
		}
		crc.crc =
		    crc32((u8 *)packet,
			  sizeof(struct nanohub_packet) + len, ~0);
		memcpy(&packet->data[len], &crc.crc,
		       sizeof(struct nanohub_packet_crc));
	} else {
		ret = ERROR_NACK;
	}

	return ret;
}

static int packet_verify(struct nanohub_packet *packet)
{
	struct nanohub_packet_crc crc;
	int cmp;

	crc.crc =
	    crc32((u8 *)packet,
		  sizeof(struct nanohub_packet) + packet->len, ~0);

	cmp =
	    memcmp(&crc.crc, &packet->data[packet->len],
		   sizeof(struct nanohub_packet_crc));

	if (cmp != 0)
		pr_debug("nanohub: gen crc: %08x, got crc: %08x\n", crc.crc,
			 *(u32 *)&packet->data[packet->len]);

	return cmp;
}

static void packet_free(struct nanohub_packet_pad *packet)
{
	kfree(packet);
}

static int read_ack(struct nanohub_data *data,
		    struct nanohub_packet *response, int timeout)
{
	int ret, i;
	const int max_size = sizeof(struct nanohub_packet) + MAX_UINT8 +
	    sizeof(struct nanohub_packet_crc);
	unsigned long end = jiffies + msecs_to_jiffies(READ_ACK_TIMEOUT_MS);

	for (i = 0; time_before(jiffies, end); i++) {
		ret =
		    data->comms.read(data, (u8 *)response, max_size,
				     timeout);

		if (ret == 0) {
			pr_debug("nanohub: %s: %d: empty packet\n", __func__,
				 i);
			ret = ERROR_NACK;
			continue;
		} else if (ret < sizeof(struct nanohub_packet)) {
			pr_debug("nanohub %s: %d: too small\n", __func__, i);
			ret = ERROR_NACK;
			continue;
		} else if (ret <
			   sizeof(struct nanohub_packet) + response->len +
			   sizeof(struct nanohub_packet_crc)) {
			pr_debug("nanohub %s: %d: too small length\n",
				 __func__, i);
			ret = ERROR_NACK;
			continue;
		} else if (ret !=
			   sizeof(struct nanohub_packet) + response->len +
			   sizeof(struct nanohub_packet_crc)) {
			pr_debug("nanohub %s: %d: wrong length\n", __func__,
				 i);
			ret = ERROR_NACK;
			break;
		} else if (packet_verify(response) != 0) {
			pr_debug("nanohub %s: %d: invalid crc\n", __func__, i);
			ret = ERROR_NACK;
			break;
		}
		break;
	}

	return ret;
}

static int read_msg(struct nanohub_data *data,
		    struct nanohub_packet *response, int timeout)
{
	int ret, i;
	const int max_size = sizeof(struct nanohub_packet) + MAX_UINT8 +
	    sizeof(struct nanohub_packet_crc);
	unsigned long end = jiffies + msecs_to_jiffies(READ_MSG_TIMEOUT_MS);

	for (i = 0; time_before(jiffies, end); i++) {
		ret =
		    data->comms.read(data, (u8 *)response, max_size,
				     timeout);

		if (ret == 0) {
			pr_debug("nanohub: %s: %d: empty packet\n", __func__,
				 i);
			ret = ERROR_NACK;
			continue;
		} else if (ret < sizeof(struct nanohub_packet)) {
			pr_debug("nanohub: %s: %d: too small\n", __func__, i);
			ret = ERROR_NACK;
			continue;
		} else if (ret <
			   sizeof(struct nanohub_packet) + response->len +
			   sizeof(struct nanohub_packet_crc)) {
			pr_debug("nanohub: %s: %d: too small length\n",
				 __func__, i);
			ret = ERROR_NACK;
			continue;
		} else if (ret !=
			   sizeof(struct nanohub_packet) + response->len +
			   sizeof(struct nanohub_packet_crc)) {
			pr_debug("nanohub: %s: %d: wrong length\n", __func__,
				 i);
			ret = ERROR_NACK;
			break;
		} else if (packet_verify(response) != 0) {
			pr_debug("nanohub: %s: %d: invalid crc\n", __func__,
				 i);
			ret = ERROR_NACK;
			break;
		}
		break;
	}

	return ret;
}

static int get_reply(struct nanohub_data *data,
		     struct nanohub_packet *response, u32 seq)
{
	int ret;

	ret = read_ack(data, response, data->comms.timeout_ack);

	if (ret >= 0 && response->seq == seq) {
		if (response->reason == CMD_COMMS_ACK) {
			if (response->len == sizeof(data->interrupts))
				memcpy(data->interrupts, response->data,
				       response->len);
			ret =
			    read_msg(data, response, data->comms.timeout_reply);
			if (ret < 0)
				ret = ERROR_NACK;
		} else {
			if (response->reason == CMD_COMMS_NACK)
				ret = ERROR_NACK;
			else if (response->reason == CMD_COMMS_BUSY)
				ret = ERROR_BUSY;
		}

		if (response->seq != seq)
			ret = ERROR_NACK;
	} else {
		ret = ERROR_NACK;
	}
	return ret;
}

static int nanohub_comms_tx_rx(struct nanohub_data *data,
			       struct nanohub_packet_pad *pad, int packet_size,
			       u32 seq, u8 *rx, size_t rx_len)
{
	int ret;

	ret = data->comms.write(data, (u8 *)&pad->packet, packet_size,
				data->comms.timeout_write);

	if (ret == packet_size) {
		ret = get_reply(data, &pad->packet, seq);

		if (ret >= 0) {
			if (pad->packet.len > 0) {
				if (pad->packet.len > rx_len) {
					memcpy(rx, pad->packet.data, rx_len);
					ret = rx_len;
				} else {
					memcpy(rx, pad->packet.data,
					       pad->packet.len);
					ret = pad->packet.len;
				}
			} else {
				ret = 0;
			}
		}
	} else {
		ret = ERROR_NACK;
	}

	return ret;
}

int nanohub_comms_rx_retrans_boottime(struct nanohub_data *data, u32 cmd,
				      u8 *rx, size_t rx_len,
				      int retrans_cnt,
				      int retrans_delay)
{
	int packet_size = 0;
	struct nanohub_packet_pad *pad = packet_alloc(GFP_KERNEL);
	int delay = 0;
	int ret;
	u32 seq;
	s64 boottime;

	if (!pad)
		return ERROR_NACK;

	seq = data->comms.seq++;

	do {
		data->comms.open(data);
		boottime = ktime_get_boottime_ns();
		packet_size =
		    packet_create(&pad->packet, seq, cmd, sizeof(boottime),
				  (u8 *)&boottime, false);

		ret =
		    nanohub_comms_tx_rx(data, pad, packet_size, seq, rx,
					rx_len);

		if (nanohub_wakeup_eom(data,
				       ret == ERROR_BUSY ||
				       (ret == ERROR_NACK && retrans_cnt >= 0)))
			ret = -EFAULT;

		data->comms.close(data);

		if (ret == ERROR_NACK) {
			retrans_cnt--;
			delay += retrans_delay;
			if (retrans_cnt >= 0)
				udelay(retrans_delay);
		} else if (ret == ERROR_BUSY) {
			usleep_range(100000, 200000);
		}
	} while ((ret == ERROR_BUSY) ||
		(ret == ERROR_NACK && retrans_cnt >= 0));

	packet_free(pad);

	return ret;
}

int nanohub_comms_tx_rx_retrans(struct nanohub_data *data, u32 cmd,
				const u8 *tx, u8 tx_len,
				u8 *rx, size_t rx_len, bool user,
				int retrans_cnt, int retrans_delay)
{
	int packet_size = 0;
	struct nanohub_packet_pad *pad = packet_alloc(GFP_KERNEL);
	int delay = 0;
	int ret;
	u32 seq;

	if (!pad)
		return ERROR_NACK;
	seq = data->comms.seq++;

	do {
		packet_size =
		    packet_create(&pad->packet, seq, cmd, tx_len, tx, user);

		data->comms.open(data);
		ret =
		    nanohub_comms_tx_rx(data, pad, packet_size, seq, rx,
					rx_len);

		if (nanohub_wakeup_eom(data,
				       ret == ERROR_BUSY ||
				       (ret == ERROR_NACK && retrans_cnt >= 0)))
			ret = -EFAULT;

		data->comms.close(data);

		if (ret == ERROR_NACK) {
			retrans_cnt--;
			delay += retrans_delay;
			if (retrans_cnt >= 0)
				udelay(retrans_delay);
		} else if (ret == ERROR_BUSY) {
			usleep_range(100000, 200000);
		}
	} while ((ret == ERROR_BUSY) ||
		(ret == ERROR_NACK && retrans_cnt >= 0));

	packet_free(pad);

	return ret;
}

struct firmware_header {
	u32 size;
	u32 crc;
	u8 type;
} __packed;

struct firmware_chunk {
	u32 offset;
	u8 data[252 - sizeof(u32)];
} __packed;

static int nanohub_comms_download(struct nanohub_data *data,
				  const u8 *image, size_t length,
				  u8 type)
{
	u8 accepted;
	struct firmware_header header;
	struct firmware_chunk chunk;
	int max_chunk_size = sizeof(chunk.data);
	int chunk_size;
	u32 offset = 0;
	int ret;
	u8 chunk_reply = 0, upload_reply = 0;
	u32 clear_interrupts[8] = { 0x00000008 };

	header.type = type;
	header.size = cpu_to_le32(length);
	header.crc = cpu_to_le32(~crc32(image, length, ~0));

	if (request_wakeup(data))
		return -ERESTARTSYS;
	ret = nanohub_comms_tx_rx_retrans(data, CMD_COMMS_START_KERNEL_UPLOAD,
					  (const u8 *)&header,
					  sizeof(header), &accepted,
					  sizeof(accepted), false, 10, 10);
	release_wakeup(data);

	if (ret == 1 && accepted == 1) {
		do {
			if (request_wakeup(data))
				continue;

			chunk.offset = cpu_to_le32(offset);
			if (offset + max_chunk_size > length)
				chunk_size = length - offset;
			else
				chunk_size = max_chunk_size;
			memcpy(chunk.data, image + offset, chunk_size);

			ret =
			    nanohub_comms_tx_rx_retrans(data,
							CMD_COMMS_KERNEL_CHUNK,
							(const u8 *)&chunk,
							sizeof(u32) +
							chunk_size,
							&chunk_reply,
							sizeof(chunk_reply),
							false, 10, 10);

			pr_debug("nanohub: ret=%d, chunk_reply=%d, offset=%d\n",
				 ret, chunk_reply, offset);
			if (ret == sizeof(chunk_reply)) {
				if (chunk_reply == CHUNK_REPLY_ACCEPTED) {
					offset += chunk_size;
				} else if (chunk_reply == CHUNK_REPLY_WAIT) {
					ret = nanohub_wait_for_interrupt(data);
					if (ret < 0) {
						release_wakeup(data);
						continue;
					}
					nanohub_comms_tx_rx_retrans
						(data, CMD_COMMS_CLR_GET_INTR,
						 (u8 *)clear_interrupts,
						 sizeof(clear_interrupts),
						 (u8 *)data->interrupts,
						 sizeof(data->interrupts),
						 false, 10, 0);
				} else if (chunk_reply == CHUNK_REPLY_RESEND) {
					;
				} else if (chunk_reply == CHUNK_REPLY_RESTART) {
					offset = 0;
				} else if (chunk_reply == CHUNK_REPLY_CANCEL ||
					(chunk_reply ==
					CHUNK_REPLY_CANCEL_NO_RETRY)) {
					release_wakeup(data);
					break;
				}
			} else if (ret <= 0) {
				release_wakeup(data);
				break;
			}
			release_wakeup(data);
		} while (offset < length);
	}

	do {
		if (request_wakeup(data)) {
			ret = sizeof(upload_reply);
			upload_reply = UPLOAD_REPLY_PROCESSING;
			continue;
		}
		ret = nanohub_comms_tx_rx_retrans
			(data,
			 CMD_COMMS_FINISH_KERNEL_UPLOAD, NULL, 0,
			 &upload_reply, sizeof(upload_reply), false, 10, 10);
		release_wakeup(data);
	} while (ret == sizeof(upload_reply) &&
		 upload_reply == UPLOAD_REPLY_PROCESSING);

	pr_info("nanohub: %s: ret=%d, upload_reply=%d\n", __func__,
		ret, upload_reply);

	return 0;
}

int nanohub_comms_kernel_download(struct nanohub_data *data,
				  const u8 *image, size_t length)
{
	return nanohub_comms_download(data, image, length,
				      COMMS_FLASH_KERNEL_ID);
}

int nanohub_comms_app_download(struct nanohub_data *data, const u8 *image,
			       size_t length)
{
	return nanohub_comms_download(data, image, length, COMMS_FLASH_APP_ID);
}
