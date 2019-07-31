// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
 */

#include <linux/device.h>
#include <linux/bitmap.h>
#include <linux/io.h>
#include "coresight-ost.h"
#include <linux/sched/clock.h>
#include <linux/coresight-stm.h>

#define STM_USERSPACE_HEADER_SIZE	(8)
#define STM_USERSPACE_MAGIC1_VAL	(0xf0)
#define STM_USERSPACE_MAGIC2_VAL	(0xf1)

#define OST_TOKEN_STARTSIMPLE		(0x10)
#define OST_VERSION_MIPI1		(16)

#define STM_MAKE_VERSION(ma, mi)	((ma << 8) | mi)
#define STM_HEADER_MAGIC		(0x5953)

#define STM_FLAG_MARKED			BIT(4)

#define STM_TRACE_BUF_SIZE		4096

static struct stm_drvdata *stmdrvdata;

static uint32_t stm_channel_alloc(void)
{
	struct stm_drvdata *drvdata = stmdrvdata;
	uint32_t ch, off, num_ch_per_cpu;
	int cpu;

	num_ch_per_cpu = drvdata->numsp/num_present_cpus();

	cpu = get_cpu();

	off = num_ch_per_cpu * cpu;
	ch = find_next_zero_bit(drvdata->chs.bitmap,
				drvdata->numsp, off);
	if (unlikely(ch >= (off + num_ch_per_cpu))) {
		put_cpu();
		return drvdata->numsp;
	}

	set_bit(ch, drvdata->chs.bitmap);
	put_cpu();

	return ch;
}

static int stm_ost_send(void __iomem *addr, const void *data, uint32_t size)
{
	uint32_t len = size;

	if (((unsigned long)data & 0x1) && (size >= 1)) {
		writeb_relaxed_no_log(*(uint8_t *)data, addr);
		data++;
		size--;
	}
	if (((unsigned long)data & 0x2) && (size >= 2)) {
		writew_relaxed_no_log(*(uint16_t *)data, addr);
		data += 2;
		size -= 2;
	}

	/* now we are 32bit aligned */
	while (size >= 4) {
		writel_relaxed_no_log(*(uint32_t *)data, addr);
		data += 4;
		size -= 4;
	}

	if (size >= 2) {
		writew_relaxed_no_log(*(uint16_t *)data, addr);
		data += 2;
		size -= 2;
	}
	if (size >= 1) {
		writeb_relaxed_no_log(*(uint8_t *)data, addr);
		data++;
		size--;
	}

	return len;
}

static void stm_channel_free(uint32_t ch)
{
	struct stm_drvdata *drvdata = stmdrvdata;

	clear_bit(ch, drvdata->chs.bitmap);
}

static int stm_trace_ost_header(void __iomem *ch_addr, uint32_t flags,
				uint8_t entity_id, uint8_t proto_id)
{
	void __iomem *addr;
	uint32_t header;
	char *hdr;

	hdr = (char *)&header;

	hdr[0] = OST_TOKEN_STARTSIMPLE;
	hdr[1] = OST_VERSION_MIPI1;
	hdr[2] = entity_id;
	hdr[3] = proto_id;

	/* header is expected to be D32M type */
	flags |= STM_FLAG_MARKED;
	flags &= ~STM_FLAG_TIMESTAMPED;
	addr = (void __iomem *)(ch_addr +
		stm_channel_off(STM_PKT_TYPE_DATA, flags));

	return stm_ost_send(addr, &header, sizeof(header));
}

static int stm_trace_data_header(void __iomem *addr)
{
	char hdr[24];
	int len = 0;

	*(uint16_t *)(hdr) = STM_MAKE_VERSION(0, 2);
	*(uint16_t *)(hdr + 2) = STM_HEADER_MAGIC;
	*(uint32_t *)(hdr + 4) = raw_smp_processor_id();
	*(uint64_t *)(hdr + 8) = sched_clock();
	*(uint64_t *)(hdr + 16) = task_tgid_nr(get_current());

	len += stm_ost_send(addr, hdr, sizeof(hdr));
	len += stm_ost_send(addr, current->comm, TASK_COMM_LEN);

	return len;
}

static int stm_trace_data(void __iomem *ch_addr, uint32_t flags,
			uint32_t entity_id, const void *data, uint32_t size)
{
	void __iomem *addr;
	int len = 0;

	flags &= ~STM_FLAG_TIMESTAMPED;
	addr = (void __iomem *)(ch_addr +
		stm_channel_off(STM_PKT_TYPE_DATA, flags));

	/* OST_ENTITY_DIAG no need to send the data header */
	if (entity_id != OST_ENTITY_DIAG)
		len += stm_trace_data_header(addr);

	/* send the actual data */
	len += stm_ost_send(addr, data, size);

	return len;
}

static int stm_trace_ost_tail(void __iomem *ch_addr, uint32_t flags)
{
	void __iomem *addr;
	uint32_t tail = 0x0;

	addr = (void __iomem *)(ch_addr +
		stm_channel_off(STM_PKT_TYPE_FLAG, flags));

	return stm_ost_send(addr, &tail, sizeof(tail));
}

static inline int __stm_trace(uint32_t flags, uint8_t entity_id,
			      uint8_t proto_id, const void *data, uint32_t size)
{
	struct stm_drvdata *drvdata = stmdrvdata;
	int len = 0;
	uint32_t ch;
	void __iomem *ch_addr;

	if (!(drvdata && drvdata->master_enable))
		return 0;

	/* allocate channel and get the channel address */
	ch = stm_channel_alloc();
	if (unlikely(ch >= drvdata->numsp)) {
		drvdata->ch_alloc_fail_count++;
		dev_err_ratelimited(drvdata->dev,
				    "Channel allocation failed %d",
				    drvdata->ch_alloc_fail_count);
		return 0;
	}

	ch_addr = (void __iomem *)stm_channel_addr(drvdata, ch);

	/* send the ost header */
	len += stm_trace_ost_header(ch_addr, flags, entity_id,
				    proto_id);

	/* send the payload data */
	len += stm_trace_data(ch_addr, flags, entity_id, data, size);

	/* send the ost tail */
	len += stm_trace_ost_tail(ch_addr, flags);

	/* we are done, free the channel */
	stm_channel_free(ch);

	return len;
}

/*
 * stm_trace - trace the binary or string data through STM
 * @flags: tracing options - guaranteed, timestamped, etc
 * @entity_id: entity representing the trace data
 * @proto_id: protocol id to distinguish between different binary formats
 * @data: pointer to binary or string data buffer
 * @size: size of data to send
 *
 * Packetizes the data as the payload to an OST packet and sends it over STM
 *
 * CONTEXT:
 * Can be called from any context.
 *
 * RETURNS:
 * number of bytes transferred over STM
 */
int stm_trace(uint32_t flags, uint8_t entity_id, uint8_t proto_id,
			const void *data, uint32_t size)
{
	struct stm_drvdata *drvdata = stmdrvdata;

	/* we don't support sizes more than 24bits (0 to 23) */
	if (!(drvdata && drvdata->enable &&
	      test_bit(entity_id, drvdata->entities) && size &&
	      (size < 0x1000000)))
		return 0;

	return __stm_trace(flags, entity_id, proto_id, data, size);
}
EXPORT_SYMBOL(stm_trace);

ssize_t stm_ost_packet(struct stm_data *stm_data,
				  unsigned int size,
				  const unsigned char *buf)
{
	struct stm_drvdata *drvdata = container_of(stm_data,
						   struct stm_drvdata, stm);

	uint8_t entity_id, proto_id;
	uint32_t flags;

	if (!drvdata->enable || !size)
		return -EINVAL;

	if (size > STM_TRACE_BUF_SIZE)
		size = STM_TRACE_BUF_SIZE;

	if (size >= STM_USERSPACE_HEADER_SIZE &&
	    buf[0] == STM_USERSPACE_MAGIC1_VAL &&
	    buf[1] == STM_USERSPACE_MAGIC2_VAL) {

		entity_id = buf[2];
		proto_id = buf[3];
		flags = *(uint32_t *)(buf + 4);

		if (!test_bit(entity_id, drvdata->entities) ||
		    !(size - STM_USERSPACE_HEADER_SIZE)) {
			return size;
		}

		__stm_trace(flags, entity_id, proto_id,
			    buf + STM_USERSPACE_HEADER_SIZE,
			    size - STM_USERSPACE_HEADER_SIZE);
	} else {
		if (!test_bit(OST_ENTITY_DEV_NODE, drvdata->entities))
			return size;

		__stm_trace(STM_FLAG_TIMESTAMPED, OST_ENTITY_DEV_NODE, 0,
			    buf, size);
	}

	return size;
}
EXPORT_SYMBOL(stm_ost_packet);

int stm_set_ost_params(struct stm_drvdata *drvdata, size_t bitmap_size)
{
	drvdata->chs.bitmap = devm_kzalloc(drvdata->dev, bitmap_size,
					   GFP_KERNEL);
	if (!drvdata->chs.bitmap)
		return -ENOMEM;

	bitmap_fill(drvdata->entities, OST_ENTITY_MAX);
	stmdrvdata = drvdata;

	return 0;
}
EXPORT_SYMBOL(stm_set_ost_params);

