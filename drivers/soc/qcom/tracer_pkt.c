/* Copyright (c) 2015, 2017, The Linux Foundation. All rights reserved.
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
#include <asm/arch_timer.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/types.h>
#include <soc/qcom/tracer_pkt.h>
#define CREATE_TRACE_POINTS
#include "tracer_pkt_private.h"

static unsigned qdss_tracing;
module_param_named(qdss_tracing_enable, qdss_tracing,
		   uint, S_IRUGO | S_IWUSR | S_IWGRP);

#define TRACER_PKT_VERSION 1
#define MAX_CC_WLEN 3
#define HEX_DUMP_HDR "Tracer Packet:"

/**
 * struct tracer_pkt_hdr - data structure defiining the tracer packet header
 * @version:		Tracer Packet version.
 * @reserved:		Reserved fields in the tracer packet.
 * @id_valid:		Indicates the presence of a subsytem & transport ID.
 * @qdss_tracing:	Enable the event logging to QDSS.
 * @ccl:		Client cookie/private information length in words.
 * @pkt_len:		Length of the tracer packet in words.
 * @pkt_offset:		Offset into the packet to log events, in words.
 * @clnt_event_cfg:	Client-specific event configuration bit mask.
 * @glink_event_cfg:	G-Link-specific event configuration bit mask.
 * @base_ts:		Base timestamp when the tracer packet is initialized.
 * @cc:			Client cookie/private information.
 */
struct tracer_pkt_hdr {
	uint16_t version:4;
	uint16_t reserved:8;
	uint16_t id_valid:1;
	uint16_t qdss_tracing:1;
	uint16_t ccl:2;
	uint16_t pkt_len;
	uint16_t pkt_offset;
	uint16_t clnt_event_cfg;
	uint32_t glink_event_cfg;
	u64 base_ts;
	uint32_t cc[MAX_CC_WLEN];
} __attribute__((__packed__));

/**
 * struct tracer_pkt_event - data structure defining the tracer packet event
 * @event_id:	Event ID.
 * @event_ts:	Timestamp at which the event occured.
 */
struct tracer_pkt_event {
	uint32_t event_id;
	uint32_t event_ts;
};

/**
 * tracer_pkt_init() - initialize the tracer packet
 * @data:		Pointer to the buffer to be initialized with a tracer
 *			packet.
 * @data_len:		Length of the buffer.
 * @client_event_cfg:	Client-specific event configuration mask.
 * @glink_event_cfg:	G-Link-specific event configuration mask.
 * @pkt_priv:		Private/Cookie information to be added to the tracer
 *			packet.
 * @pkt_priv_len:	Length of the private data.
 *
 * This function is used to initialize a buffer with the tracer packet header.
 * The tracer packet header includes the data as passed by the elements in the
 * parameters.
 *
 * Return: 0 on success, standard Linux error codes on failure.
 */
int tracer_pkt_init(void *data, size_t data_len,
		    uint16_t client_event_cfg, uint32_t glink_event_cfg,
		    void *pkt_priv, size_t pkt_priv_len)
{
	struct tracer_pkt_hdr *pkt_hdr;

	if (!data || !data_len)
		return -EINVAL;

	if (!IS_ALIGNED(data_len, sizeof(uint32_t)))
		return -EINVAL;

	if (data_len < sizeof(*pkt_hdr))
		return -ETOOSMALL;

	pkt_hdr = (struct tracer_pkt_hdr *)data;
	pkt_hdr->version = TRACER_PKT_VERSION;
	pkt_hdr->reserved = 0;
	pkt_hdr->id_valid = 0;
	pkt_hdr->qdss_tracing = qdss_tracing ? true : false;
	if (pkt_priv_len >= MAX_CC_WLEN * sizeof(uint32_t))
		pkt_hdr->ccl = MAX_CC_WLEN;
	else
		pkt_hdr->ccl = pkt_priv_len/sizeof(uint32_t) +
				(pkt_priv_len & (sizeof(uint32_t) - 1) ? 1 : 0);
	pkt_hdr->pkt_len = data_len / sizeof(uint32_t);
	pkt_hdr->pkt_offset = sizeof(*pkt_hdr) / sizeof(uint32_t);
	pkt_hdr->clnt_event_cfg = client_event_cfg;
	pkt_hdr->glink_event_cfg = glink_event_cfg;
	pkt_hdr->base_ts = arch_counter_get_cntvct();
	memcpy(pkt_hdr->cc, pkt_priv, pkt_hdr->ccl * sizeof(uint32_t));
	return 0;
}
EXPORT_SYMBOL(tracer_pkt_init);

/**
 * tracer_pkt_set_event_cfg() - set the event configuration mask in the tracer
 *				packet
 * @data:		Pointer to the buffer to be initialized with event
 *			configuration mask.
 * @client_event_cfg:	Client-specific event configuration mask.
 * @glink_event_cfg:	G-Link-specific event configuration mask.
 *
 * This function is used to initialize a buffer with the event configuration
 * mask as passed by the elements in the parameters.
 *
 * Return: 0 on success, standard Linux error codes on failure.
 */
int tracer_pkt_set_event_cfg(void *data, uint16_t client_event_cfg,
			     uint32_t glink_event_cfg)
{
	struct tracer_pkt_hdr *pkt_hdr;

	if (!data)
		return -EINVAL;

	pkt_hdr = (struct tracer_pkt_hdr *)data;
	if (unlikely(pkt_hdr->version != TRACER_PKT_VERSION))
		return -EINVAL;

	pkt_hdr->clnt_event_cfg = client_event_cfg;
	pkt_hdr->glink_event_cfg = glink_event_cfg;
	return 0;
}
EXPORT_SYMBOL(tracer_pkt_set_event_cfg);

/**
 * tracer_pkt_log_event() - log an event specific to the tracer packet
 * @data:	Pointer to the buffer containing tracer packet.
 * @event_id:	Event ID to be logged.
 *
 * This function is used to log an event specific to the tracer packet.
 * The event is logged either into the tracer packet itself or a different
 * tracing mechanism as configured.
 *
 * Return: 0 on success, standard Linux error codes on failure.
 */
int tracer_pkt_log_event(void *data, uint32_t event_id)
{
	struct tracer_pkt_hdr *pkt_hdr;
	struct tracer_pkt_event event;

	if (!data)
		return -EINVAL;

	pkt_hdr = (struct tracer_pkt_hdr *)data;
	if (unlikely(pkt_hdr->version != TRACER_PKT_VERSION))
		return -EINVAL;

	if (qdss_tracing) {
		trace_tracer_pkt_event(event_id, pkt_hdr->cc);
		return 0;
	}

	if (unlikely((pkt_hdr->pkt_len - pkt_hdr->pkt_offset) *
	    sizeof(uint32_t) < sizeof(event)))
		return -ETOOSMALL;

	event.event_id = event_id;
	event.event_ts = (uint32_t)arch_counter_get_cntvct();
	memcpy(data + (pkt_hdr->pkt_offset * sizeof(uint32_t)),
		&event, sizeof(event));
	pkt_hdr->pkt_offset += sizeof(event)/sizeof(uint32_t);
	return 0;
}
EXPORT_SYMBOL(tracer_pkt_log_event);

/**
 * tracer_pkt_calc_hex_dump_size() - calculate the hex dump size of a tracer
 *				     packet
 * @data:	Pointer to the buffer containing tracer packet.
 * @data_len:	Length of the tracer packet buffer.
 *
 * This function is used to calculate the length of the buffer required to
 * hold the hex dump of the tracer packet.
 *
 * Return: 0 on success, standard Linux error codes on failure.
 */
size_t tracer_pkt_calc_hex_dump_size(void *data, size_t data_len)
{
	size_t hex_dump_size;
	struct tracer_pkt_hdr *pkt_hdr;

	if (!data || data_len <= 0)
		return -EINVAL;

	pkt_hdr = (struct tracer_pkt_hdr *)data;
	if (unlikely(pkt_hdr->version != TRACER_PKT_VERSION))
		return -EINVAL;

	/*
	 * Hex Dump Prefix + newline
	 * 0x<first_word> + newline
	 * ...
	 * 0x<last_word> + newline + null-termination character.
	 */
	hex_dump_size = strlen(HEX_DUMP_HDR) + 1 + (pkt_hdr->pkt_len * 11) + 1;
	return hex_dump_size;
}
EXPORT_SYMBOL(tracer_pkt_calc_hex_dump_size);

/**
 * tracer_pkt_hex_dump() - hex dump the tracer packet into a buffer
 * @buf:	Buffer to contain the hex dump of the tracer packet.
 * @buf_len:	Length of the hex dump buffer.
 * @data:	Buffer containing the tracer packet.
 * @data_len:	Length of the buffer containing the tracer packet.
 *
 * This function is used to dump the contents of the tracer packet into
 * a buffer in a specific hexadecimal format. The hex dump buffer can then
 * be dumped through debugfs.
 *
 * Return: 0 on success, standard Linux error codes on failure.
 */
int tracer_pkt_hex_dump(void *buf, size_t buf_len, void *data, size_t data_len)
{
	int i, j = 0;
	char *dst = (char *)buf;

	if (!buf || buf_len <= 0 || !data || data_len <= 0)
		return -EINVAL;

	if (buf_len < tracer_pkt_calc_hex_dump_size(data, data_len))
		return -EINVAL;

	j = scnprintf(dst, buf_len, "%s\n", HEX_DUMP_HDR);
	for (i = 0; i < data_len/sizeof(uint32_t); i++)
		j += scnprintf(dst + j, buf_len - j, "0x%08x\n",
				*((uint32_t *)data + i));
	dst[j] = '\0';
	return 0;
}
EXPORT_SYMBOL(tracer_pkt_hex_dump);
