/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include "smd_private.h"

void set_state(volatile void *half_channel, unsigned data)
{
	((struct smd_half_channel *)(half_channel))->state = data;
}

unsigned get_state(volatile void *half_channel)
{
	return ((struct smd_half_channel *)(half_channel))->state;
}

void set_fDSR(volatile void *half_channel, unsigned char data)
{
	((struct smd_half_channel *)(half_channel))->fDSR = data;
}

unsigned get_fDSR(volatile void *half_channel)
{
	return ((struct smd_half_channel *)(half_channel))->fDSR;
}

void set_fCTS(volatile void *half_channel, unsigned char data)
{
	((struct smd_half_channel *)(half_channel))->fCTS = data;
}

unsigned get_fCTS(volatile void *half_channel)
{
	return ((struct smd_half_channel *)(half_channel))->fCTS;
}

void set_fCD(volatile void *half_channel, unsigned char data)
{
	((struct smd_half_channel *)(half_channel))->fCD = data;
}

unsigned get_fCD(volatile void *half_channel)
{
	return ((struct smd_half_channel *)(half_channel))->fCD;
}

void set_fRI(volatile void *half_channel, unsigned char data)
{
	((struct smd_half_channel *)(half_channel))->fRI = data;
}

unsigned get_fRI(volatile void *half_channel)
{
	return ((struct smd_half_channel *)(half_channel))->fRI;
}

void set_fHEAD(volatile void *half_channel, unsigned char data)
{
	((struct smd_half_channel *)(half_channel))->fHEAD = data;
}

unsigned get_fHEAD(volatile void *half_channel)
{
	return ((struct smd_half_channel *)(half_channel))->fHEAD;
}

void set_fTAIL(volatile void *half_channel, unsigned char data)
{
	((struct smd_half_channel *)(half_channel))->fTAIL = data;
}

unsigned get_fTAIL(volatile void *half_channel)
{
	return ((struct smd_half_channel *)(half_channel))->fTAIL;
}

void set_fSTATE(volatile void *half_channel, unsigned char data)
{
	((struct smd_half_channel *)(half_channel))->fSTATE = data;
}

unsigned get_fSTATE(volatile void *half_channel)
{
	return ((struct smd_half_channel *)(half_channel))->fSTATE;
}

void set_fBLOCKREADINTR(volatile void *half_channel, unsigned char data)
{
	((struct smd_half_channel *)(half_channel))->fBLOCKREADINTR = data;
}

unsigned get_fBLOCKREADINTR(volatile void *half_channel)
{
	return ((struct smd_half_channel *)(half_channel))->fBLOCKREADINTR;
}

void set_tail(volatile void *half_channel, unsigned data)
{
	((struct smd_half_channel *)(half_channel))->tail = data;
}

unsigned get_tail(volatile void *half_channel)
{
	return ((struct smd_half_channel *)(half_channel))->tail;
}

void set_head(volatile void *half_channel, unsigned data)
{
	((struct smd_half_channel *)(half_channel))->head = data;
}

unsigned get_head(volatile void *half_channel)
{
	return ((struct smd_half_channel *)(half_channel))->head;
}

void set_state_word_access(volatile void *half_channel, unsigned data)
{
	((struct smd_half_channel_word_access *)(half_channel))->state = data;
}

unsigned get_state_word_access(volatile void *half_channel)
{
	return ((struct smd_half_channel_word_access *)(half_channel))->state;
}

void set_fDSR_word_access(volatile void *half_channel, unsigned char data)
{
	((struct smd_half_channel_word_access *)(half_channel))->fDSR = data;
}

unsigned get_fDSR_word_access(volatile void *half_channel)
{
	return ((struct smd_half_channel_word_access *)(half_channel))->fDSR;
}

void set_fCTS_word_access(volatile void *half_channel, unsigned char data)
{
	((struct smd_half_channel_word_access *)(half_channel))->fCTS = data;
}

unsigned get_fCTS_word_access(volatile void *half_channel)
{
	return ((struct smd_half_channel_word_access *)(half_channel))->fCTS;
}

void set_fCD_word_access(volatile void *half_channel, unsigned char data)
{
	((struct smd_half_channel_word_access *)(half_channel))->fCD = data;
}

unsigned get_fCD_word_access(volatile void *half_channel)
{
	return ((struct smd_half_channel_word_access *)(half_channel))->fCD;
}

void set_fRI_word_access(volatile void *half_channel, unsigned char data)
{
	((struct smd_half_channel_word_access *)(half_channel))->fRI = data;
}

unsigned get_fRI_word_access(volatile void *half_channel)
{
	return ((struct smd_half_channel_word_access *)(half_channel))->fRI;
}

void set_fHEAD_word_access(volatile void *half_channel, unsigned char data)
{
	((struct smd_half_channel_word_access *)(half_channel))->fHEAD = data;
}

unsigned get_fHEAD_word_access(volatile void *half_channel)
{
	return ((struct smd_half_channel_word_access *)(half_channel))->fHEAD;
}

void set_fTAIL_word_access(volatile void *half_channel, unsigned char data)
{
	((struct smd_half_channel_word_access *)(half_channel))->fTAIL = data;
}

unsigned get_fTAIL_word_access(volatile void *half_channel)
{
	return ((struct smd_half_channel_word_access *)(half_channel))->fTAIL;
}

void set_fSTATE_word_access(volatile void *half_channel, unsigned char data)
{
	((struct smd_half_channel_word_access *)(half_channel))->fSTATE = data;
}

unsigned get_fSTATE_word_access(volatile void *half_channel)
{
	return ((struct smd_half_channel_word_access *)(half_channel))->fSTATE;
}

void set_fBLOCKREADINTR_word_access(volatile void *half_channel,
							unsigned char data)
{
	((struct smd_half_channel_word_access *)
					(half_channel))->fBLOCKREADINTR = data;
}

unsigned get_fBLOCKREADINTR_word_access(volatile void *half_channel)
{
	return ((struct smd_half_channel_word_access *)
						(half_channel))->fBLOCKREADINTR;
}

void set_tail_word_access(volatile void *half_channel, unsigned data)
{
	((struct smd_half_channel_word_access *)(half_channel))->tail = data;
}

unsigned get_tail_word_access(volatile void *half_channel)
{
	return ((struct smd_half_channel_word_access *)(half_channel))->tail;
}

void set_head_word_access(volatile void *half_channel, unsigned data)
{
	((struct smd_half_channel_word_access *)(half_channel))->head = data;
}

unsigned get_head_word_access(volatile void *half_channel)
{
	return ((struct smd_half_channel_word_access *)(half_channel))->head;
}

int is_word_access_ch(unsigned ch_type)
{
	if (ch_type == SMD_APPS_RPM || ch_type == SMD_MODEM_RPM ||
		ch_type == SMD_QDSP_RPM || ch_type == SMD_WCNSS_RPM)
		return 1;
	else
		return 0;
}

struct smd_half_channel_access *get_half_ch_funcs(unsigned ch_type)
{
	static struct smd_half_channel_access byte_access = {
		.set_state = set_state,
		.get_state = get_state,
		.set_fDSR = set_fDSR,
		.get_fDSR = get_fDSR,
		.set_fCTS = set_fCTS,
		.get_fCTS = get_fCTS,
		.set_fCD = set_fCD,
		.get_fCD = get_fCD,
		.set_fRI = set_fRI,
		.get_fRI = get_fRI,
		.set_fHEAD = set_fHEAD,
		.get_fHEAD = get_fHEAD,
		.set_fTAIL = set_fTAIL,
		.get_fTAIL = get_fTAIL,
		.set_fSTATE = set_fSTATE,
		.get_fSTATE = get_fSTATE,
		.set_fBLOCKREADINTR = set_fBLOCKREADINTR,
		.get_fBLOCKREADINTR = get_fBLOCKREADINTR,
		.set_tail = set_tail,
		.get_tail = get_tail,
		.set_head = set_head,
		.get_head = get_head,
	};
	static struct smd_half_channel_access word_access = {
		.set_state = set_state_word_access,
		.get_state = get_state_word_access,
		.set_fDSR = set_fDSR_word_access,
		.get_fDSR = get_fDSR_word_access,
		.set_fCTS = set_fCTS_word_access,
		.get_fCTS = get_fCTS_word_access,
		.set_fCD = set_fCD_word_access,
		.get_fCD = get_fCD_word_access,
		.set_fRI = set_fRI_word_access,
		.get_fRI = get_fRI_word_access,
		.set_fHEAD = set_fHEAD_word_access,
		.get_fHEAD = get_fHEAD_word_access,
		.set_fTAIL = set_fTAIL_word_access,
		.get_fTAIL = get_fTAIL_word_access,
		.set_fSTATE = set_fSTATE_word_access,
		.get_fSTATE = get_fSTATE_word_access,
		.set_fBLOCKREADINTR = set_fBLOCKREADINTR_word_access,
		.get_fBLOCKREADINTR = get_fBLOCKREADINTR_word_access,
		.set_tail = set_tail_word_access,
		.get_tail = get_tail_word_access,
		.set_head = set_head_word_access,
		.get_head = get_head_word_access,
	};

	if (is_word_access_ch(ch_type))
		return &word_access;
	else
		return &byte_access;
}

