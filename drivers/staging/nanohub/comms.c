/*
 * Copyright (C) 2016 Google, Inc.
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

static const uint32_t crc_table[] = {
	0x00000000, 0x04C11DB7, 0x09823B6E, 0x0D4326D9,
	0x130476DC, 0x17C56B6B, 0x1A864DB2, 0x1E475005,
	0x2608EDB8, 0x22C9F00F, 0x2F8AD6D6, 0x2B4BCB61,
	0x350C9B64, 0x31CD86D3, 0x3C8EA00A, 0x384FBDBD
};

static uint32_t crc32_word(uint32_t crc, uint32_t data, int cnt)
{
	int i;

	crc = crc ^ data;
	for (i = 0; i < cnt; i++)
		crc = (crc << 4) ^ crc_table[crc >> 28];

	return crc;
}

uint32_t crc32(const uint8_t *buffer, int length, uint32_t crc)
{
	uint32_t *data = (uint32_t *)buffer;
	uint32_t word;
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
	return sizeof(uint32_t) + pad(length) + sizeof(uint32_t);
}

static struct nanohub_packet_pad *packet_alloc(int flags)
{
	int len =
	    sizeof(struct nanohub_packet_pad) + MAX_UINT8 +
	    sizeof(struct nanohub_packet_crc);
	uint8_t *packet = kmalloc(len, flags);

	memset(packet, 0xFF, len);
	return (struct nanohub_packet_pad *)packet;
}

static int packet_create(struct nanohub_packet *packet, uint32_t seq,
			 uint32_t reason, uint8_t len, const uint8_t *data,
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
		    crc32((uint8_t *) packet,
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
	    crc32((uint8_t *) packet,
		  sizeof(struct nanohub_packet) + packet->len, ~0);

	cmp =
	    memcmp(&crc.crc, &packet->data[packet->len],
		   sizeof(struct nanohub_packet_crc));

	if (cmp != 0) {
		uint8_t *ptr = (uint8_t *)packet;

		pr_debug("nanohub: gen crc: %08x, got crc: %08x\n", crc.crc,
			 *(uint32_t *)&packet->data[packet->len]);
		pr_debug(
		    "nanohub: %02x [%02x %02x %02x %02x] [%02x %02x %02x %02x] [%02x] [%02x %02x %02x %02x\n",
		    ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6],
		    ptr[7], ptr[8], ptr[9], ptr[10], ptr[11], ptr[12],
		    ptr[13]);
	}

	return cmp;
}

static void packet_free(struct nanohub_packet_pad *packet)
{
	kfree(packet);
}

static int read_ack(struct nanohub_data *data, struct nanohub_packet *response,
		    int timeout)
{
	int ret, i;
	const int max_size = sizeof(struct nanohub_packet) + MAX_UINT8 +
	    sizeof(struct nanohub_packet_crc);
	unsigned long end = jiffies + msecs_to_jiffies(READ_ACK_TIMEOUT_MS);

	for (i = 0; time_before(jiffies, end); i++) {
		ret =
		    data->comms.read(data, (uint8_t *) response, max_size,
				     timeout);

		if (ret == 0) {
			pr_debug("nanohub: read_ack: %d: empty packet\n", i);
			ret = ERROR_NACK;
			continue;
		} else if (ret < sizeof(struct nanohub_packet)) {
			pr_debug("nanohub: read_ack: %d: too small\n", i);
			ret = ERROR_NACK;
			continue;
		} else if (ret <
			   sizeof(struct nanohub_packet) + response->len +
			   sizeof(struct nanohub_packet_crc)) {
			pr_debug("nanohub: read_ack: %d: too small length\n",
				 i);
			ret = ERROR_NACK;
			continue;
		} else if (ret !=
			   sizeof(struct nanohub_packet) + response->len +
			   sizeof(struct nanohub_packet_crc)) {
			pr_debug("nanohub: read_ack: %d: wrong length\n", i);
			ret = ERROR_NACK;
			break;
		} else if (packet_verify(response) != 0) {
			pr_debug("nanohub: read_ack: %d: invalid crc\n", i);
			ret = ERROR_NACK;
			break;
		}
		break;
	}

	return ret;
}

static int read_msg(struct nanohub_data *data, struct nanohub_packet *response,
		    int timeout)
{
	int ret, i;
	const int max_size = sizeof(struct nanohub_packet) + MAX_UINT8 +
	    sizeof(struct nanohub_packet_crc);
	unsigned long end = jiffies + msecs_to_jiffies(READ_MSG_TIMEOUT_MS);

	for (i = 0; time_before(jiffies, end); i++) {
		ret =
		    data->comms.read(data, (uint8_t *) response, max_size,
				     timeout);

		if (ret == 0) {
			pr_debug("nanohub: read_msg: %d: empty packet\n", i);
			ret = ERROR_NACK;
			continue;
		} else if (ret < sizeof(struct nanohub_packet)) {
			pr_debug("nanohub: read_msg: %d: too small\n", i);
			ret = ERROR_NACK;
			continue;
		} else if (ret <
			   sizeof(struct nanohub_packet) + response->len +
			   sizeof(struct nanohub_packet_crc)) {
			pr_debug("nanohub: read_msg: %d: too small length\n",
				 i);
			ret = ERROR_NACK;
			continue;
		} else if (ret !=
			   sizeof(struct nanohub_packet) + response->len +
			   sizeof(struct nanohub_packet_crc)) {
			pr_debug("nanohub: read_msg: %d: wrong length\n", i);
			ret = ERROR_NACK;
			break;
		} else if (packet_verify(response) != 0) {
			pr_debug("nanohub: read_msg: %d: invalid crc\n", i);
			ret = ERROR_NACK;
			break;
		}
		break;
	}

	return ret;
}

static int get_reply(struct nanohub_data *data, struct nanohub_packet *response,
		     uint32_t seq)
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
			int i;
			uint8_t *b = (uint8_t *) response;

			for (i = 0; i < ret; i += 25)
				pr_debug(
				    "nanohub: %d: %d: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
				    ret, i, b[i], b[i + 1], b[i + 2], b[i + 3],
				    b[i + 4], b[i + 5], b[i + 6], b[i + 7],
				    b[i + 8], b[i + 9], b[i + 10], b[i + 11],
				    b[i + 12], b[i + 13], b[i + 14], b[i + 15],
				    b[i + 16], b[i + 17], b[i + 18], b[i + 19],
				    b[i + 20], b[i + 21], b[i + 22], b[i + 23],
				    b[i + 24]);
			if (response->reason == CMD_COMMS_NACK)
				ret = ERROR_NACK;
			else if (response->reason == CMD_COMMS_BUSY)
				ret = ERROR_BUSY;
		}

		if (response->seq != seq)
			ret = ERROR_NACK;
	} else {
		if (ret >= 0) {
			int i;
			uint8_t *b = (uint8_t *) response;

			for (i = 0; i < ret; i += 25)
				pr_debug(
				    "nanohub: %d: %d: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
				    ret, i, b[i], b[i + 1], b[i + 2], b[i + 3],
				    b[i + 4], b[i + 5], b[i + 6], b[i + 7],
				    b[i + 8], b[i + 9], b[i + 10], b[i + 11],
				    b[i + 12], b[i + 13], b[i + 14], b[i + 15],
				    b[i + 16], b[i + 17], b[i + 18], b[i + 19],
				    b[i + 20], b[i + 21], b[i + 22], b[i + 23],
				    b[i + 24]);
		}
		ret = ERROR_NACK;
	}

	return ret;
}

static int nanohub_comms_tx_rx(struct nanohub_data *data,
			       struct nanohub_packet_pad *pad, int packet_size,
			       uint32_t seq, uint8_t *rx, size_t rx_len)
{
	int ret;

	ret = data->comms.write(data, (uint8_t *)&pad->packet, packet_size,
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

int nanohub_comms_rx_retrans_boottime(struct nanohub_data *data, uint32_t cmd,
				      uint8_t *rx, size_t rx_len,
				      int retrans_cnt, int retrans_delay)
{
	int packet_size = 0;
	struct nanohub_packet_pad *pad = packet_alloc(GFP_KERNEL);
	int delay = 0;
	int ret;
	uint32_t seq;
	struct timespec ts;
	s64 boottime;

	if (pad == NULL)
		return ERROR_NACK;

	seq = data->comms.seq++;

	do {
		data->comms.open(data);
		get_monotonic_boottime(&ts);
		boottime = timespec_to_ns(&ts);
		packet_size =
		    packet_create(&pad->packet, seq, cmd, sizeof(boottime),
				  (uint8_t *)&boottime, false);

		ret =
		    nanohub_comms_tx_rx(data, pad, packet_size, seq, rx,
					rx_len);

		if (nanohub_wakeup_eom(data,
				       (ret == ERROR_BUSY) ||
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
	} while ((ret == ERROR_BUSY)
		 || (ret == ERROR_NACK && retrans_cnt >= 0));

	packet_free(pad);

	return ret;
}

int nanohub_comms_tx_rx_retrans(struct nanohub_data *data, uint32_t cmd,
				const uint8_t *tx, uint8_t tx_len,
				uint8_t *rx, size_t rx_len, bool user,
				int retrans_cnt, int retrans_delay)
{
	int packet_size = 0;
	struct nanohub_packet_pad *pad = packet_alloc(GFP_KERNEL);
	int delay = 0;
	int ret;
	uint32_t seq;

	if (pad == NULL)
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
				       (ret == ERROR_BUSY) ||
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
	} while ((ret == ERROR_BUSY)
		 || (ret == ERROR_NACK && retrans_cnt >= 0));

	packet_free(pad);

	return ret;
}

struct firmware_header {
	uint32_t size;
	uint32_t crc;
	uint8_t type;
} __packed;

struct firmware_chunk {
	uint32_t offset;
	uint8_t data[252 - sizeof(uint32_t)];
}
__packed;

static int nanohub_comms_download(struct nanohub_data *data,
				  const uint8_t *image, size_t length,
				  uint8_t type)
{
	uint8_t accepted;
	struct firmware_header header;
	struct firmware_chunk chunk;
	int max_chunk_size = sizeof(chunk.data);
	int chunk_size;
	uint32_t offset = 0;
	int ret;
	uint8_t chunk_reply, upload_reply = 0;
	uint32_t clear_interrupts[8] = { 0x00000008 };

	header.type = type;
	header.size = cpu_to_le32(length);
	header.crc = cpu_to_le32(~crc32(image, length, ~0));

	if (request_wakeup(data))
		return -ERESTARTSYS;
	ret = nanohub_comms_tx_rx_retrans(data, CMD_COMMS_START_KERNEL_UPLOAD,
					  (const uint8_t *)&header,
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
							(const uint8_t *)&chunk,
							sizeof(uint32_t) +
							chunk_size,
							&chunk_reply,
							sizeof(chunk_reply),
							false, 10, 10);

			pr_debug("nanohub: ret=%d, chunk_reply=%d, offset=%d\n",
				 ret, chunk_reply, offset);
			if (ret == sizeof(chunk_reply)) {
				if (chunk_reply == CHUNK_REPLY_ACCEPTED)
					offset += chunk_size;
				else if (chunk_reply == CHUNK_REPLY_WAIT) {
					ret = nanohub_wait_for_interrupt(data);
					if (ret < 0) {
						release_wakeup(data);
						continue;
					}
					nanohub_comms_tx_rx_retrans(data,
					    CMD_COMMS_CLR_GET_INTR,
					    (uint8_t *)clear_interrupts,
					    sizeof(clear_interrupts),
					    (uint8_t *)data->interrupts,
					    sizeof(data->interrupts),
					    false, 10, 0);
				} else if (chunk_reply == CHUNK_REPLY_RESEND)
					;
				else if (chunk_reply == CHUNK_REPLY_RESTART)
					offset = 0;
				else if (chunk_reply == CHUNK_REPLY_CANCEL ||
				    chunk_reply ==
				    CHUNK_REPLY_CANCEL_NO_RETRY) {
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
		ret = nanohub_comms_tx_rx_retrans(data,
					CMD_COMMS_FINISH_KERNEL_UPLOAD,
					NULL, 0,
					&upload_reply, sizeof(upload_reply),
					false, 10, 10);
		release_wakeup(data);
	} while (ret == sizeof(upload_reply) &&
		 upload_reply == UPLOAD_REPLY_PROCESSING);

	pr_info("nanohub: nanohub_comms_download: ret=%d, upload_reply=%d\n",
		ret, upload_reply);

	return 0;
}

int nanohub_comms_kernel_download(struct nanohub_data *data,
				  const uint8_t *image, size_t length)
{
	return nanohub_comms_download(data, image, length,
				      COMMS_FLASH_KERNEL_ID);
}

int nanohub_comms_app_download(struct nanohub_data *data, const uint8_t *image,
			       size_t length)
{
	return nanohub_comms_download(data, image, length, COMMS_FLASH_APP_ID);
}
