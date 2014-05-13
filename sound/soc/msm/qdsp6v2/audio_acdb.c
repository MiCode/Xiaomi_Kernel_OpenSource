/* Copyright (c) 2010-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/msm_ion.h>
#include <linux/mm.h>
#include <linux/msm_audio_ion.h>
#include "audio_acdb.h"
#include "q6voice.h"

#include <sound/q6adm-v2.h>
#include <sound/q6afe-v2.h>
#include <sound/q6asm-v2.h>
#include <sound/q6lsm.h>


#define MAX_NETWORKS			15
#define MAX_IOCTL_DATA			(MAX_NETWORKS * 2)
#define MAX_COL_SIZE			324

#define ACDB_BLOCK_SIZE			4096
#define NUM_VOCPROC_BLOCKS		(6 * MAX_NETWORKS)
#define ACDB_TOTAL_VOICE_ALLOCATION	(ACDB_BLOCK_SIZE * NUM_VOCPROC_BLOCKS)

#define MAX_HW_DELAY_ENTRIES	25

struct acdb_data {
	uint32_t		usage_count;

	struct mutex		acdb_mutex;

	/* ANC Cal */
	struct acdb_cal_block		anc_cal;

	/* AANC Cal */
	struct acdb_cal_block		aanc_cal;

	/* LSM Cal */
	struct acdb_cal_block		lsm_cal;

	/* AudProc Cal */
	uint32_t			asm_topology;
	uint32_t			adm_topology[MAX_AUDPROC_TYPES];
	struct acdb_cal_block		audproc_cal[MAX_AUDPROC_TYPES];
	struct acdb_cal_block		audstrm_cal[MAX_AUDPROC_TYPES];
	struct acdb_cal_block		audvol_cal[MAX_AUDPROC_TYPES];

	/* VocProc Cal */
	uint32_t			voice_rx_topology;
	uint32_t			voice_tx_topology;
	struct acdb_cal_block		vocproc_cal;
	struct acdb_cal_block		vocstrm_cal;
	struct acdb_cal_block		vocvol_cal;

	/* Voice Column data */
	struct acdb_cal_block	vocproc_col_cal[MAX_VOCPROC_TYPES];
	void *col_data[MAX_VOCPROC_TYPES];

	/* VocProc dev cfg cal*/
	struct acdb_cal_block		vocproc_dev_cal;

	/* Custom topology */
	struct acdb_cal_block		adm_custom_topology;
	struct acdb_cal_block		asm_custom_topology;
	uint32_t			valid_adm_custom_top;
	uint32_t			valid_asm_custom_top;

	/* AFE cal */
	struct acdb_cal_block		afe_cal[MAX_AUDPROC_TYPES];

	/* Sidetone Cal */
	struct sidetone_cal		sidetone_cal;

	/* Allocation information */
	struct ion_client		*ion_client;
	struct ion_handle		*ion_handle;
	uint32_t			map_handle;
	phys_addr_t			paddr;
	void				*kvaddr;
	uint64_t			mem_len;

	/* Speaker protection */
	struct msm_spk_prot_cfg spk_prot_cfg;

	/* Av sync delay info */
	struct hw_delay hw_delay_rx;
	struct hw_delay hw_delay_tx;
	struct meta_info_t metainfo;
};

static struct acdb_data		acdb_data;

uint32_t get_voice_rx_topology(void)
{
	return acdb_data.voice_rx_topology;
}

static void store_voice_rx_topology(uint32_t topology)
{
	acdb_data.voice_rx_topology = topology;
}

uint32_t get_voice_tx_topology(void)
{
	return acdb_data.voice_tx_topology;
}

static void store_voice_tx_topology(uint32_t topology)
{
	acdb_data.voice_tx_topology = topology;
}

uint32_t get_adm_rx_topology(void)
{
	return acdb_data.adm_topology[RX_CAL];
}

static void store_adm_rx_topology(uint32_t topology)
{
	acdb_data.adm_topology[RX_CAL] = topology;
}

uint32_t get_adm_tx_topology(void)
{
	return acdb_data.adm_topology[TX_CAL];
}

static void store_adm_tx_topology(uint32_t topology)
{
	acdb_data.adm_topology[TX_CAL] = topology;
}

uint32_t get_asm_topology(void)
{
	return acdb_data.asm_topology;
}

static void store_asm_topology(uint32_t topology)
{
	acdb_data.asm_topology = topology;
}

void reset_custom_topology_flags(void)
{
	mutex_lock(&acdb_data.acdb_mutex);
	acdb_data.valid_adm_custom_top = 1;
	acdb_data.valid_asm_custom_top = 1;
	mutex_unlock(&acdb_data.acdb_mutex);
}

int get_adm_custom_topology(struct acdb_cal_block *cal_block)
{
	int result = 0;
	pr_debug("%s\n", __func__);

	if (cal_block == NULL) {
		pr_err("ACDB=> NULL pointer sent to %s\n", __func__);
		result = -EINVAL;
		goto done;
	}

	mutex_lock(&acdb_data.acdb_mutex);
	/* Only return allow one access after memory registered */
	if (acdb_data.valid_adm_custom_top == 0) {
		cal_block->cal_size = 0;
		goto unlock;
	}
	acdb_data.valid_adm_custom_top = 0;

	cal_block->cal_size = acdb_data.adm_custom_topology.cal_size;
	cal_block->cal_paddr = acdb_data.adm_custom_topology.cal_paddr;
	cal_block->cal_kvaddr = acdb_data.adm_custom_topology.cal_kvaddr;
unlock:
	mutex_unlock(&acdb_data.acdb_mutex);
done:
	return result;
}

static int store_adm_custom_topology(struct cal_block *cal_block)
{
	int result = 0;
	pr_debug("%s,\n", __func__);

	if (cal_block->cal_offset > acdb_data.mem_len) {
		pr_err("%s: offset %d is > mem_len %llu\n",
			__func__, cal_block->cal_offset, acdb_data.mem_len);
		result = -EINVAL;
		goto done;
	}

	acdb_data.adm_custom_topology.cal_size = cal_block->cal_size;
	acdb_data.adm_custom_topology.cal_paddr =
		cal_block->cal_offset + acdb_data.paddr;
	acdb_data.adm_custom_topology.cal_kvaddr =
		cal_block->cal_offset + (u8 *)acdb_data.kvaddr;
done:
	return result;
}

int get_asm_custom_topology(struct acdb_cal_block *cal_block)
{
	int result = 0;
	pr_debug("%s,\n", __func__);

	if (cal_block == NULL) {
		pr_err("ACDB=> NULL pointer sent to %s\n", __func__);
		result = -EINVAL;
		goto done;
	}

	mutex_lock(&acdb_data.acdb_mutex);
	/* Only return allow one access after memory registered */
	if (acdb_data.valid_asm_custom_top == 0) {
		cal_block->cal_size = 0;
		goto unlock;
	}
	acdb_data.valid_asm_custom_top = 0;

	cal_block->cal_size = acdb_data.asm_custom_topology.cal_size;
	cal_block->cal_paddr = acdb_data.asm_custom_topology.cal_paddr;
	cal_block->cal_kvaddr = acdb_data.asm_custom_topology.cal_kvaddr;
unlock:
	mutex_unlock(&acdb_data.acdb_mutex);
done:
	return result;
}

static int store_asm_custom_topology(struct cal_block *cal_block)
{
	int result = 0;
	pr_debug("%s,\n", __func__);

	if (cal_block->cal_offset > acdb_data.mem_len) {
		pr_err("%s: offset %d is > mem_len %llu\n",
			__func__, cal_block->cal_offset, acdb_data.mem_len);
		result = -EINVAL;
		goto done;
	}

	acdb_data.asm_custom_topology.cal_size = cal_block->cal_size;
	acdb_data.asm_custom_topology.cal_paddr =
		cal_block->cal_offset + acdb_data.paddr;
	acdb_data.asm_custom_topology.cal_kvaddr =
		cal_block->cal_offset + (u8 *)acdb_data.kvaddr;
done:
	return result;
}

int get_voice_cal_allocation(struct acdb_cal_block *cal_block)
{
	int result = 0;
	pr_debug("%s,\n", __func__);

	if (cal_block == NULL) {
		pr_err("ACDB=> NULL pointer sent to %s\n", __func__);
		result = -EINVAL;
		goto done;
	}

	cal_block->cal_size = ACDB_TOTAL_VOICE_ALLOCATION;
	cal_block->cal_paddr = acdb_data.vocproc_cal.cal_paddr;
	cal_block->cal_kvaddr = acdb_data.vocproc_cal.cal_kvaddr;
done:
	return result;
}

int get_aanc_cal(struct acdb_cal_block *cal_block)
{
	int result = 0;
	pr_debug("%s,\n", __func__);

	if (cal_block == NULL) {
		pr_err("ACDB=> NULL pointer sent to %s\n", __func__);
		result = -EINVAL;
		goto done;
	}

	cal_block->cal_size = acdb_data.aanc_cal.cal_size;
	cal_block->cal_paddr = acdb_data.aanc_cal.cal_paddr;
	cal_block->cal_kvaddr = acdb_data.aanc_cal.cal_kvaddr;
done:
	return result;
}

static int store_aanc_cal(struct cal_block *cal_block)
{
	int result = 0;
	pr_debug("%s,\n", __func__);

	if (cal_block->cal_offset > acdb_data.mem_len) {
		pr_err("%s: offset %d is > mem_len %llu\n",
		 __func__, cal_block->cal_offset, acdb_data.mem_len);
		result = -EINVAL;
		goto done;
	}

	acdb_data.aanc_cal.cal_size = cal_block->cal_size;
	acdb_data.aanc_cal.cal_paddr =
		cal_block->cal_offset + acdb_data.paddr;
	acdb_data.aanc_cal.cal_kvaddr =
		cal_block->cal_offset + (u8 *)acdb_data.kvaddr;
done:
	return result;
}

int get_lsm_cal(struct acdb_cal_block *cal_block)
{
	int result = 0;
	pr_debug("%s,\n", __func__);

	if (cal_block == NULL) {
		pr_err("ACDB=> NULL pointer sent to %s\n", __func__);
		result = -EINVAL;
		goto done;
	}

	cal_block->cal_size = acdb_data.lsm_cal.cal_size;
	cal_block->cal_paddr = acdb_data.lsm_cal.cal_paddr;
	cal_block->cal_kvaddr = acdb_data.lsm_cal.cal_kvaddr;
done:
	return result;
}

static int store_lsm_cal(struct cal_block *cal_block)
{
	int result = 0;
	pr_debug("%s,\n", __func__);

	if (cal_block->cal_offset > acdb_data.mem_len) {
		pr_err("%s: offset %d is > mem_len %llu\n",
			__func__, cal_block->cal_offset, acdb_data.mem_len);
		result = -EINVAL;
		goto done;
	}

	acdb_data.lsm_cal.cal_size = cal_block->cal_size;
	acdb_data.lsm_cal.cal_paddr =
		cal_block->cal_offset + acdb_data.paddr;
	acdb_data.lsm_cal.cal_kvaddr =
		cal_block->cal_offset + (u8 *)acdb_data.kvaddr;
done:
	return result;
}

int get_hw_delay(int32_t path, struct hw_delay_entry *entry)
{
	int i, result = 0;
	struct hw_delay *delay = NULL;
	struct hw_delay_entry *info = NULL;
	pr_debug("%s,\n", __func__);

	if (entry == NULL) {
		pr_err("ACDB=> NULL pointer sent to %s\n", __func__);
		result = -EINVAL;
		goto ret;
	}
	if ((path >= MAX_AUDPROC_TYPES) || (path < 0)) {
		pr_err("ACDB=> Bad path sent to %s, path: %d\n",
		       __func__, path);
		result = -EINVAL;
		goto ret;
	}
	mutex_lock(&acdb_data.acdb_mutex);
	if (path == RX_CAL)
		delay = &acdb_data.hw_delay_rx;
	else if (path == TX_CAL)
		delay = &acdb_data.hw_delay_tx;

	if ((delay == NULL) || ((delay != NULL) && delay->num_entries == 0)) {
		pr_debug("ACDB=> %s Invalid delay/ delay entries\n", __func__);
		result = -EINVAL;
		goto done;
	}

	info = (struct hw_delay_entry *)(delay->delay_info);
	if (info == NULL) {
		pr_err("ACDB=> %s Delay entries info is NULL\n", __func__);
		result = -EFAULT;
		goto done;
	}
	for (i = 0; i < delay->num_entries; i++) {
		if (info[i].sample_rate == entry->sample_rate) {
			entry->delay_usec = info[i].delay_usec;
			break;
		}
	}
	if (i == delay->num_entries) {
		pr_err("ACDB=> %s: Unable to find delay for sample rate %d\n",
		       __func__, entry->sample_rate);
		result = -EFAULT;
	}

done:
	mutex_unlock(&acdb_data.acdb_mutex);
	pr_debug("ACDB=> %s: Path = %d samplerate = %u usec = %u status %d\n",
		 __func__, path, entry->sample_rate, entry->delay_usec, result);
ret:
	return result;
}

static int store_hw_delay(int32_t path, void *arg)
{
	int result = 0;
	struct hw_delay delay;
	struct hw_delay *delay_dest = NULL;
	pr_debug("%s,\n", __func__);

	if ((path >= MAX_AUDPROC_TYPES) || (path < 0) || (arg == NULL)) {
		pr_err("ACDB=> Bad path/ pointer sent to %s, path: %d\n",
		      __func__, path);
		result = -EINVAL;
		goto done;
	}
	result = copy_from_user((void *)&delay, (void *)arg,
				sizeof(struct hw_delay));
	if (result) {
		pr_err("ACDB=> %s failed to copy hw delay: result=%d path=%d\n",
		       __func__, result, path);
		result = -EFAULT;
		goto done;
	}
	if ((delay.num_entries <= 0) ||
		(delay.num_entries > MAX_HW_DELAY_ENTRIES)) {
		pr_debug("ACDB=> %s incorrect no of hw delay entries: %d\n",
		       __func__, delay.num_entries);
		result = -EINVAL;
		goto done;
	}
	if ((path >= MAX_AUDPROC_TYPES) || (path < 0)) {
		pr_err("ACDB=> Bad path sent to %s, path: %d\n",
		__func__, path);
		result = -EINVAL;
		goto done;
	}

	pr_debug("ACDB=> %s : Path = %d num_entries = %d\n",
		 __func__, path, delay.num_entries);

	if (path == RX_CAL)
		delay_dest = &acdb_data.hw_delay_rx;
	else if (path == TX_CAL)
		delay_dest = &acdb_data.hw_delay_tx;

	delay_dest->num_entries = delay.num_entries;

	result = copy_from_user(delay_dest->delay_info,
				delay.delay_info,
				(sizeof(struct hw_delay_entry)*
				delay.num_entries));
	if (result) {
		pr_err("ACDB=> %s failed to copy hw delay info res=%d path=%d",
		       __func__, result, path);
		result = -EFAULT;
	}
done:
	return result;
}

int get_anc_cal(struct acdb_cal_block *cal_block)
{
	int result = 0;
	pr_debug("%s,\n", __func__);

	if (cal_block == NULL) {
		pr_err("ACDB=> NULL pointer sent to %s\n", __func__);
		result = -EINVAL;
		goto done;
	}

	cal_block->cal_size = acdb_data.anc_cal.cal_size;
	cal_block->cal_paddr = acdb_data.anc_cal.cal_paddr;
	cal_block->cal_kvaddr = acdb_data.anc_cal.cal_kvaddr;
done:
	return result;
}

static int store_anc_cal(struct cal_block *cal_block)
{
	int result = 0;
	pr_debug("%s,\n", __func__);

	if (cal_block->cal_offset > acdb_data.mem_len) {
		pr_err("%s: offset %d is > mem_len %llu\n",
			__func__, cal_block->cal_offset, acdb_data.mem_len);
		result = -EINVAL;
		goto done;
	}

	acdb_data.anc_cal.cal_size = cal_block->cal_size;
	acdb_data.anc_cal.cal_paddr =
		cal_block->cal_offset + acdb_data.paddr;
	acdb_data.anc_cal.cal_kvaddr =
		cal_block->cal_offset + (u8 *)acdb_data.kvaddr;
done:
	return result;
}

static int store_afe_cal(int32_t path, struct cal_block *cal_block)
{
	int result = 0;
	pr_debug("%s, path = %d\n", __func__, path);

	if (cal_block->cal_offset > acdb_data.mem_len) {
		pr_err("%s: offset %d is > mem_len %llu\n",
			__func__, cal_block->cal_offset, acdb_data.mem_len);
		result = -EINVAL;
		goto done;
	}
	if ((path >= MAX_AUDPROC_TYPES) || (path < 0)) {
		pr_err("ACDB=> Bad path sent to %s, path: %d\n",
			__func__, path);
		result = -EINVAL;
		goto done;
	}

	acdb_data.afe_cal[path].cal_size = cal_block->cal_size;
	acdb_data.afe_cal[path].cal_paddr =
		cal_block->cal_offset + acdb_data.paddr;
	acdb_data.afe_cal[path].cal_kvaddr =
		cal_block->cal_offset + (u8 *)acdb_data.kvaddr;
done:
	return result;
}

int get_afe_cal(int32_t path, struct acdb_cal_block *cal_block)
{
	int result = 0;
	pr_debug("%s, path = %d\n", __func__, path);

	if (cal_block == NULL) {
		pr_err("ACDB=> NULL pointer sent to %s\n", __func__);
		result = -EINVAL;
		goto done;
	}
	if ((path >= MAX_AUDPROC_TYPES) || (path < 0)) {
		pr_err("ACDB=> Bad path sent to %s, path: %d\n",
			__func__, path);
		result = -EINVAL;
		goto done;
	}

	cal_block->cal_size = acdb_data.afe_cal[path].cal_size;
	cal_block->cal_paddr = acdb_data.afe_cal[path].cal_paddr;
	cal_block->cal_kvaddr = acdb_data.afe_cal[path].cal_kvaddr;
done:
	return result;
}

static int store_audproc_cal(int32_t path, struct cal_block *cal_block)
{
	int result = 0;
	pr_debug("%s, path = %d\n", __func__, path);

	if (cal_block->cal_offset > acdb_data.mem_len) {
		pr_err("%s: offset %d is > mem_len %llu\n",
			__func__, cal_block->cal_offset, acdb_data.mem_len);
		result = -EINVAL;
		goto done;
	}
	if (path >= MAX_AUDPROC_TYPES) {
		pr_err("ACDB=> Bad path sent to %s, path: %d\n",
			__func__, path);
		result = -EINVAL;
		goto done;
	}

	acdb_data.audproc_cal[path].cal_size = cal_block->cal_size;
	acdb_data.audproc_cal[path].cal_paddr =
		cal_block->cal_offset + acdb_data.paddr;
	acdb_data.audproc_cal[path].cal_kvaddr =
		cal_block->cal_offset + (u8 *)acdb_data.kvaddr;
done:
	return result;
}

int get_audproc_cal(int32_t path, struct acdb_cal_block *cal_block)
{
	int result = 0;
	pr_debug("%s, path = %d\n", __func__, path);

	if (cal_block == NULL) {
		pr_err("ACDB=> NULL pointer sent to %s\n", __func__);
		result = -EINVAL;
		goto done;
	}
	if (path >= MAX_AUDPROC_TYPES) {
		pr_err("ACDB=> Bad path sent to %s, path: %d\n",
			__func__, path);
		result = -EINVAL;
		goto done;
	}

	cal_block->cal_size = acdb_data.audproc_cal[path].cal_size;
	cal_block->cal_paddr = acdb_data.audproc_cal[path].cal_paddr;
	cal_block->cal_kvaddr = acdb_data.audproc_cal[path].cal_kvaddr;
done:
	return result;
}

static int store_audstrm_cal(int32_t path, struct cal_block *cal_block)
{
	int result = 0;
	pr_debug("%s, path = %d\n", __func__, path);

	if (cal_block->cal_offset > acdb_data.mem_len) {
		pr_err("%s: offset %d is > mem_len %llu\n",
			__func__, cal_block->cal_offset, acdb_data.mem_len);
		result = -EINVAL;
		goto done;
	}
	if (path >= MAX_AUDPROC_TYPES) {
		pr_err("ACDB=> Bad path sent to %s, path: %d\n",
			__func__, path);
		result = -EINVAL;
		goto done;
	}

	acdb_data.audstrm_cal[path].cal_size = cal_block->cal_size;
	acdb_data.audstrm_cal[path].cal_paddr =
		cal_block->cal_offset + acdb_data.paddr;
	acdb_data.audstrm_cal[path].cal_kvaddr =
		cal_block->cal_offset + (u8 *)acdb_data.kvaddr;
done:
	return result;
}

int get_audstrm_cal(int32_t path, struct acdb_cal_block *cal_block)
{
	int result = 0;
	pr_debug("%s, path = %d\n", __func__, path);

	if (cal_block == NULL) {
		pr_err("ACDB=> NULL pointer sent to %s\n", __func__);
		result = -EINVAL;
		goto done;
	}
	if (path >= MAX_AUDPROC_TYPES) {
		pr_err("ACDB=> Bad path sent to %s, path: %d\n",
			__func__, path);
		result = -EINVAL;
		goto done;
	}

	cal_block->cal_size = acdb_data.audstrm_cal[path].cal_size;
	cal_block->cal_paddr = acdb_data.audstrm_cal[path].cal_paddr;
	cal_block->cal_kvaddr = acdb_data.audstrm_cal[path].cal_kvaddr;
done:
	return result;
}

static int store_audvol_cal(int32_t path, struct cal_block *cal_block)
{
	int result = 0;
	pr_debug("%s, path = %d\n", __func__, path);

	if (cal_block->cal_offset > acdb_data.mem_len) {
		pr_err("%s: offset %d is > mem_len %llu\n",
			__func__, cal_block->cal_offset, acdb_data.mem_len);
		result = -EINVAL;
		goto done;
	}
	if (path >= MAX_AUDPROC_TYPES) {
		pr_err("ACDB=> Bad path sent to %s, path: %d\n",
			__func__, path);
		result = -EINVAL;
		goto done;
	}

	acdb_data.audvol_cal[path].cal_size = cal_block->cal_size;
	acdb_data.audvol_cal[path].cal_paddr =
		cal_block->cal_offset + acdb_data.paddr;
	acdb_data.audvol_cal[path].cal_kvaddr =
		cal_block->cal_offset + (u8 *)acdb_data.kvaddr;
done:
	return result;
}

int get_audvol_cal(int32_t path, struct acdb_cal_block *cal_block)
{
	int result = 0;
	pr_debug("%s, path = %d\n", __func__, path);

	if (cal_block == NULL) {
		pr_err("ACDB=> NULL pointer sent to %s\n", __func__);
		result = -EINVAL;
		goto done;
	}
	if (path >= MAX_AUDPROC_TYPES || path < 0) {
		pr_err("ACDB=> Bad path sent to %s, path: %d\n",
			__func__, path);
		result = -EINVAL;
		goto done;
	}

	cal_block->cal_size = acdb_data.audvol_cal[path].cal_size;
	cal_block->cal_paddr = acdb_data.audvol_cal[path].cal_paddr;
	cal_block->cal_kvaddr = acdb_data.audvol_cal[path].cal_kvaddr;
done:
	return result;
}

static int store_voice_col_data(uint32_t vocproc_type, uint32_t cal_size,
			  uint32_t *cal_block)
{
	int result = 0;
	pr_debug("%s,\n", __func__);

	if (cal_block == NULL) {
		pr_err("ACDB=> NULL pointer sent to %s\n", __func__);
		result = -EINVAL;
		goto done;
	}
	if (cal_size > MAX_COL_SIZE) {
		pr_err("%s: col size is to big %d\n", __func__, cal_size);
		result = -EINVAL;
		goto done;
	}
	if (acdb_data.col_data[vocproc_type] == NULL) {
		pr_err("%s: vocproc_type %d data not allocated!\n",
			__func__, vocproc_type);
		result = -EINVAL;
		goto done;
	}
	if (copy_from_user(acdb_data.col_data[vocproc_type],
			(void *)((uint8_t *)cal_block + sizeof(cal_size)),
			cal_size)) {
		pr_err("%s: fail to copy col size %d\n", __func__, cal_size);
		result = -EINVAL;
		goto done;
	}
	acdb_data.vocproc_col_cal[vocproc_type].cal_size = cal_size;
done:
	return result;
}

int get_voice_col_data(uint32_t vocproc_type,
			struct acdb_cal_block *cal_block)
{
	int result = 0;
	pr_debug("%s,\n", __func__);

	if (cal_block == NULL) {
		pr_err("ACDB=> NULL pointer sent to %s\n", __func__);
		result = -EINVAL;
		goto done;
	}
	if (acdb_data.col_data[vocproc_type] == NULL) {
		pr_err("%s: vocproc_type %d data not allocated!\n",
			__func__, vocproc_type);
		result = -EINVAL;
		goto done;
	}

	cal_block->cal_size = acdb_data.
		vocproc_col_cal[vocproc_type].cal_size;
	cal_block->cal_paddr = acdb_data.
		vocproc_col_cal[vocproc_type].cal_paddr;
	cal_block->cal_kvaddr = acdb_data.
		vocproc_col_cal[vocproc_type].cal_kvaddr;
done:
	return result;
}

static int store_vocproc_dev_cfg_cal(struct cal_block *cal_block)
{
	int result = 0;
	pr_debug("%s,\n", __func__);


	if (cal_block->cal_offset >
				acdb_data.mem_len) {
		pr_err("%s: offset %d is > mem_len %llu\n",
			__func__, cal_block->cal_offset, acdb_data.mem_len);
		acdb_data.vocproc_dev_cal.cal_size = 0;
		result = -EINVAL;
		goto done;
	}

	acdb_data.vocproc_dev_cal.cal_size = cal_block->cal_size;
	acdb_data.vocproc_dev_cal.cal_paddr =
		cal_block->cal_offset + acdb_data.paddr;
	acdb_data.vocproc_dev_cal.cal_kvaddr =
		cal_block->cal_offset + (u8 *)acdb_data.kvaddr;
done:
	return result;
}

int get_vocproc_dev_cfg_cal(struct acdb_cal_block *cal_block)
{
	int result = 0;
	pr_debug("%s,\n", __func__);

	if (cal_block == NULL) {
		pr_err("ACDB=> NULL pointer sent to %s\n", __func__);
		result = -EINVAL;
		goto done;
	}

	cal_block->cal_size = acdb_data.vocproc_dev_cal.cal_size;
	cal_block->cal_paddr = acdb_data.vocproc_dev_cal.cal_paddr;
	cal_block->cal_kvaddr = acdb_data.vocproc_dev_cal.cal_kvaddr;
done:
	return result;
}



static int store_vocproc_cal(struct cal_block *cal_block)
{
	int result = 0;
	pr_debug("%s,\n", __func__);

	if (cal_block->cal_offset >
				acdb_data.mem_len) {
		pr_err("%s: offset %d is > mem_len %llu\n",
			__func__, cal_block->cal_offset, acdb_data.mem_len);
		acdb_data.vocproc_cal.cal_size = 0;
		result = -EINVAL;
		goto done;
	}

	acdb_data.vocproc_cal.cal_size = cal_block->cal_size;
	acdb_data.vocproc_cal.cal_paddr =
		cal_block->cal_offset + acdb_data.paddr;
	acdb_data.vocproc_cal.cal_kvaddr =
		cal_block->cal_offset + (u8 *)acdb_data.kvaddr;
done:
	return result;
}

int get_vocproc_cal(struct acdb_cal_block *cal_block)
{
	int result = 0;
	pr_debug("%s,\n", __func__);

	if (cal_block == NULL) {
		pr_err("ACDB=> NULL pointer sent to %s\n", __func__);
		result = -EINVAL;
		goto done;
	}

	cal_block->cal_size = acdb_data.vocproc_cal.cal_size;
	cal_block->cal_paddr = acdb_data.vocproc_cal.cal_paddr;
	cal_block->cal_kvaddr = acdb_data.vocproc_cal.cal_kvaddr;
done:
	return result;
}

static int store_vocstrm_cal(struct cal_block *cal_block)
{
	int result = 0;
	pr_debug("%s,\n", __func__);

	if (cal_block->cal_offset >
			acdb_data.mem_len) {
		pr_err("%s: offset %d is > mem_len %llu\n",
			__func__, cal_block->cal_offset, acdb_data.mem_len);
		acdb_data.vocstrm_cal.cal_size = 0;
		result = -EINVAL;
		goto done;
	}

	acdb_data.vocstrm_cal.cal_size = cal_block->cal_size;
	acdb_data.vocstrm_cal.cal_paddr =
		cal_block->cal_offset + acdb_data.paddr;
	acdb_data.vocstrm_cal.cal_kvaddr =
		cal_block->cal_offset + (u8 *)acdb_data.kvaddr;
done:
	return result;
}

int get_vocstrm_cal(struct acdb_cal_block *cal_block)
{
	int result = 0;
	pr_debug("%s,\n", __func__);

	if (cal_block == NULL) {
		pr_err("ACDB=> NULL pointer sent to %s\n", __func__);
		result = -EINVAL;
		goto done;
	}

	cal_block->cal_size = acdb_data.vocstrm_cal.cal_size;
	cal_block->cal_paddr = acdb_data.vocstrm_cal.cal_paddr;
	cal_block->cal_kvaddr = acdb_data.vocstrm_cal.cal_kvaddr;
done:
	return result;
}

static int store_vocvol_cal(struct cal_block *cal_block)
{
	int result = 0;
	pr_debug("%s,\n", __func__);

	if (cal_block->cal_offset > acdb_data.mem_len) {
		pr_err("%s: offset %d is > mem_len %llu\n",
			__func__, cal_block->cal_offset, acdb_data.mem_len);
		acdb_data.vocvol_cal.cal_size = 0;
		result = -EINVAL;
		goto done;
	}

	acdb_data.vocvol_cal.cal_size = cal_block->cal_size;
	acdb_data.vocvol_cal.cal_paddr =
		cal_block->cal_offset + acdb_data.paddr;
	acdb_data.vocvol_cal.cal_kvaddr =
		cal_block->cal_offset + (u8 *)acdb_data.kvaddr;
done:
	return result;
}

int get_vocvol_cal(struct acdb_cal_block *cal_block)
{
	int result = 0;
	pr_debug("%s,\n", __func__);

	if (cal_block == NULL) {
		pr_err("ACDB=> NULL pointer sent to %s\n", __func__);
		result = -EINVAL;
		goto done;
	}

	cal_block->cal_size = acdb_data.vocvol_cal.cal_size;
	cal_block->cal_paddr = acdb_data.vocvol_cal.cal_paddr;
	cal_block->cal_kvaddr = acdb_data.vocvol_cal.cal_kvaddr;
done:
	return result;
}

static void store_sidetone_cal(struct sidetone_cal *cal_data)
{
	pr_debug("%s,\n", __func__);

	acdb_data.sidetone_cal.enable = cal_data->enable;
	acdb_data.sidetone_cal.gain = cal_data->gain;
}

int get_sidetone_cal(struct sidetone_cal *cal_data)
{
	int result = 0;
	pr_debug("%s,\n", __func__);

	if (cal_data == NULL) {
		pr_err("ACDB=> NULL pointer sent to %s\n", __func__);
		result = -EINVAL;
		goto done;
	}

	mutex_lock(&acdb_data.acdb_mutex);
	cal_data->enable = acdb_data.sidetone_cal.enable;
	cal_data->gain = acdb_data.sidetone_cal.gain;
	mutex_unlock(&acdb_data.acdb_mutex);
done:
	return result;
}

static int store_meta_info(void *arg)
{
	int result = 0;

	result = copy_from_user((void *)&(acdb_data.metainfo.nKeyValue),
				(void *)arg, sizeof(uint32_t));
	if (result) {
		pr_err("ACDB=> %s failed to copy metaInfo Key: result=%d\n",
			__func__, result);
		result = -EFAULT;
		goto done;
	}
	result = copy_from_user((void *)&(acdb_data.metainfo.nBufferLength),
		(void *)(arg + sizeof(uint32_t)), sizeof(uint32_t));
	if (result) {
		pr_err("ACDB=> %s failed to copy metaInfo size: result=%d\n",
			__func__, result);
		result = -EFAULT;
		goto done;
	}
	if (acdb_data.metainfo.nBufferLength > MAX_META_INFO_SIZE) {
		pr_err("ACDB=> %s metaInfo size too large (%d)\n",
			__func__, acdb_data.metainfo.nBufferLength);
		result = -EFAULT;
		goto done;

	}
	if (acdb_data.metainfo.nBuffer != NULL) {
		pr_err("ACDB=> %s metaInfo already there\n",
			__func__);
		result = -EEXIST;
		goto done;
	}
	acdb_data.metainfo.nBuffer =
			kmalloc(acdb_data.metainfo.nBufferLength, GFP_KERNEL);
	if (acdb_data.metainfo.nBuffer == NULL) {
		pr_err("%s : Failed to allocate metaInfo\n",
			__func__);
		result = -ENOMEM;
		goto done;
	}

	result = copy_from_user((void *)(acdb_data.metainfo.nBuffer),
		 (void *)(*(uint32_t **)(arg + sizeof(uint32_t)*2)),
		 acdb_data.metainfo.nBufferLength);

	if (result) {
		pr_err("ACDB=> %s failed to copy metaInfo : result=%d\n",
			__func__, result);
		kfree(acdb_data.metainfo.nBuffer);
		result = -EFAULT;
		goto done;
	}

done:
	return result;
}

int get_meta_info_size(uint32_t key, uint32_t *size)
{
	int result = 0;

	mutex_lock(&acdb_data.acdb_mutex);
	if (key == acdb_data.metainfo.nKeyValue)
		*size = acdb_data.metainfo.nBufferLength;
	else
		result = -EINVAL;

	mutex_unlock(&acdb_data.acdb_mutex);

	return result;
}

int get_meta_info(struct meta_info_t *metainfo)
{
	int result = 0;

	if (metainfo == NULL || metainfo->nBuffer == NULL) {
		pr_err("ACDB=> NULL pointer sent to %s\n", __func__);
		result = -EINVAL;
		goto done;
	}
	mutex_lock(&acdb_data.acdb_mutex);
	if (metainfo->nKeyValue == acdb_data.metainfo.nKeyValue &&
	    metainfo->nBufferLength == acdb_data.metainfo.nBufferLength &&
	    acdb_data.metainfo.nBuffer  != NULL)
		memcpy(metainfo->nBuffer, acdb_data.metainfo.nBuffer,
			acdb_data.metainfo.nBufferLength);
	else {
		pr_err("ACDB=> wrong data %s\n", __func__);
		result = -EINVAL;
	}
	mutex_unlock(&acdb_data.acdb_mutex);
done:
	return result;
}

int get_spk_protection_cfg(struct msm_spk_prot_cfg *prot_cfg)
{
	int result = 0;
	pr_debug("%s,\n", __func__);

	mutex_lock(&acdb_data.acdb_mutex);
	if (prot_cfg) {
		prot_cfg->mode = acdb_data.spk_prot_cfg.mode;
		prot_cfg->r0 = acdb_data.spk_prot_cfg.r0;
		prot_cfg->t0 = acdb_data.spk_prot_cfg.t0;
	} else {
		pr_err("%s prot_cfg is NULL\n", __func__);
		result = -EINVAL;
	}
	mutex_unlock(&acdb_data.acdb_mutex);

	return result;
}

static int get_spk_protection_status(struct msm_spk_prot_status *status)
{
	int					result = 0;
	struct afe_spkr_prot_get_vi_calib	calib_resp;
	pr_debug("%s,\n", __func__);

	/*Call AFE function here to query the status*/
	if (status) {
		status->status = -EINVAL;
		if (!afe_spk_prot_get_calib_data(&calib_resp)) {
			if (calib_resp.res_cfg.th_vi_ca_state == 1)
				status->status = -EAGAIN;
			else if (calib_resp.res_cfg.th_vi_ca_state == 2) {
				status->status = 0;
				status->r0 = calib_resp.res_cfg.r0_cali_q24;
			}
		 }
	} else {
		pr_err("%s invalid params\n", __func__);
		result =  -EINVAL;
	}

	return result;
}

static int register_vocvol_table(void)
{
	int result = 0;
	pr_debug("%s\n", __func__);

	result = voc_register_vocproc_vol_table();
	if (result < 0) {
		pr_err("%s: Register vocproc vol failed!\n", __func__);
		goto done;
	}

done:
	return result;
}

static int deregister_vocvol_table(void)
{
	int result = 0;
	pr_debug("%s\n", __func__);

	result = voc_deregister_vocproc_vol_table();
	if (result < 0) {
		pr_err("%s: Deregister vocproc vol failed!\n", __func__);
		goto done;
	}

done:
	return result;
}

static int acdb_open(struct inode *inode, struct file *f)
{
	s32 result = 0;
	pr_debug("%s\n", __func__);

	mutex_lock(&acdb_data.acdb_mutex);
	if (acdb_data.mem_len)
		pr_debug("%s: ACDB opened but memory allocated, using existing allocation!\n",
			__func__);

	acdb_data.valid_adm_custom_top = 1;
	acdb_data.valid_asm_custom_top = 1;
	acdb_data.metainfo.nBuffer  = NULL;
	acdb_data.usage_count++;
	mutex_unlock(&acdb_data.acdb_mutex);

	return result;
}

static void deallocate_hw_delay_entries(void)
{
	kfree(acdb_data.hw_delay_rx.delay_info);
	kfree(acdb_data.hw_delay_tx.delay_info);

	acdb_data.hw_delay_rx.delay_info = NULL;
	acdb_data.hw_delay_tx.delay_info = NULL;
}

static int allocate_hw_delay_entries(void)
{
	int	result = 0;

	/* Allocate memory for hw delay entries */
	acdb_data.hw_delay_rx.num_entries = 0;
	acdb_data.hw_delay_tx.num_entries = 0;
	acdb_data.hw_delay_rx.delay_info =
				kmalloc(sizeof(struct hw_delay_entry)*
					MAX_HW_DELAY_ENTRIES,
					GFP_KERNEL);
	if (acdb_data.hw_delay_rx.delay_info == NULL) {
		pr_err("%s : Failed to allocate av sync delay entries rx\n",
			__func__);
		result = -ENOMEM;
		goto done;
	}
	acdb_data.hw_delay_tx.delay_info =
				kmalloc(sizeof(struct hw_delay_entry)*
					MAX_HW_DELAY_ENTRIES,
					GFP_KERNEL);
	if (acdb_data.hw_delay_tx.delay_info == NULL) {
		pr_err("%s : Failed to allocate av sync delay entries tx\n",
			__func__);
		deallocate_hw_delay_entries();
		result = -ENOMEM;
		goto done;
	}
done:
	return result;
}

static void deallocate_col_data(void)
{
	int	i;

	for (i = 0; i < MAX_VOCPROC_TYPES; i++) {
		kfree(acdb_data.col_data[i]);
		acdb_data.col_data[i] = NULL;
	}
}

static int allocate_col_data(void)
{
	int	result = 0;
	int	i;

	for (i = 0; i < MAX_VOCPROC_TYPES; i++) {
		acdb_data.col_data[i] = kmalloc(MAX_COL_SIZE, GFP_KERNEL);
		if (acdb_data.col_data[i] == NULL) {
			pr_err("%s: kmalloc column data failed, type = %d\n",
				__func__, i);
			deallocate_col_data();
			result = -ENOMEM;
			goto done;
		}
		acdb_data.vocproc_col_cal[i].cal_kvaddr =
			acdb_data.col_data[i];
	}

done:
	return result;
}

static int unmap_cal_tables(void)
{
	int	result = 0;
	int	result2 = 0;

	result2 = adm_unmap_cal_blocks();
	if (result2 < 0) {
		pr_err("%s: adm_unmap_cal_blocks failed, err = %d\n",
			__func__, result2);
		result = result2;
	}

	result2 = afe_unmap_cal_blocks();
	if (result2 < 0) {
		pr_err("%s: afe_unmap_cal_blocks failed, err = %d\n",
			__func__, result2);
		result = result2;
	}

	result2 = q6asm_unmap_cal_blocks();
	if (result2 < 0) {
		pr_err("%s: asm_unmap_cal_blocks failed, err = %d\n",
			__func__, result2);
		result = result2;
	}

	result2 = voc_unmap_cal_blocks();
	if (result2 < 0) {
		pr_err("%s: voice_unmap_cal_blocks failed, err = %d\n",
			__func__, result2);
		result = result2;
	}

	return result;
}

static int deregister_memory(void)
{
	int	result = 0;
	pr_debug("%s\n", __func__);

	if (acdb_data.mem_len == 0)
		goto done;

	pr_debug("Remove existing memory\n");
	acdb_data.mem_len = 0;

	/* unmap all cal data */
	result = unmap_cal_tables();
	if (result < 0)
		pr_err("%s: unmap_cal_tables failed, err = %d\n",
			__func__, result);

	msm_audio_ion_free(acdb_data.ion_client, acdb_data.ion_handle);
	acdb_data.ion_client = NULL;
	acdb_data.ion_handle = NULL;

	deallocate_col_data();
	deallocate_hw_delay_entries();
done:
	return result;
}

static int register_memory(void)
{
	int			result;
	ion_phys_addr_t		paddr;
	void                    *kvptr;
	unsigned long		kvaddr;
	size_t			mem_len;
	pr_debug("%s\n", __func__);

	result = allocate_col_data();
	if (result) {
		pr_err("%s: allocate_col_data failed, rc = %d\n",
			__func__, result);
		goto err_done;
	}

	result = allocate_hw_delay_entries();
	if (result) {
		pr_err("%s: allocate_hw_delay_entries failed, rc = %d\n",
			__func__, result);
		goto err_col;
	}

	result = msm_audio_ion_import("audio_acdb_client",
				&acdb_data.ion_client,
				&acdb_data.ion_handle,
				acdb_data.map_handle,
				NULL, 0,
				&paddr, &mem_len, &kvptr);
	if (result) {
		pr_err("%s: audio ION alloc failed, rc = %d\n",
			__func__, result);
		goto err_hw_delay;
	}

	kvaddr = (unsigned long)kvptr;
	acdb_data.paddr = paddr;
	acdb_data.kvaddr = (void *)kvaddr;
	acdb_data.mem_len = mem_len;

	pr_debug("%s done! paddr = 0x%pa, kvaddr = 0x%p, len = 0x%llx\n",
		 __func__, &acdb_data.paddr, acdb_data.kvaddr,
		 acdb_data.mem_len);

	return result;
err_hw_delay:
	deallocate_hw_delay_entries();
err_col:
	deallocate_col_data();
err_done:
	acdb_data.mem_len = 0;
	return result;
}
static long acdb_ioctl(struct file *f,
		unsigned int cmd, unsigned long arg)
{
	int32_t			result = 0;
	int32_t			size;
	int32_t			map_fd;
	uint32_t		topology;
	uint32_t		data[MAX_IOCTL_DATA];
	struct msm_spk_prot_status prot_status;
	struct msm_spk_prot_status acdb_spk_status;
	pr_debug("%s\n", __func__);

	mutex_lock(&acdb_data.acdb_mutex);
	switch (cmd) {
	case AUDIO_REGISTER_PMEM:
		pr_debug("AUDIO_REGISTER_PMEM\n");
		result = deregister_memory();
		if (result < 0)
			pr_err("%s: deregister_memory failed returned %d!\n",
				__func__, result);

		if (copy_from_user(&map_fd, (void *)arg, sizeof(map_fd))) {
			pr_err("%s: fail to copy memory handle!\n", __func__);
			result = -EFAULT;
		} else {
			acdb_data.map_handle = map_fd;
			result = register_memory();
		}
		goto done;
	case AUDIO_DEREGISTER_PMEM:
		pr_debug("AUDIO_DEREGISTER_PMEM\n");
		result = deregister_memory();
		goto done;
	case AUDIO_SET_VOICE_RX_TOPOLOGY:
		if (copy_from_user(&topology, (void *)arg,
				sizeof(topology))) {
			pr_err("%s: fail to copy topology!\n", __func__);
			result = -EFAULT;
		}
		store_voice_rx_topology(topology);
		goto done;
	case AUDIO_SET_VOICE_TX_TOPOLOGY:
		if (copy_from_user(&topology, (void *)arg,
				sizeof(topology))) {
			pr_err("%s: fail to copy topology!\n", __func__);
			result = -EFAULT;
		}
		store_voice_tx_topology(topology);
		goto done;
	case AUDIO_SET_ADM_RX_TOPOLOGY:
		if (copy_from_user(&topology, (void *)arg,
				sizeof(topology))) {
			pr_err("%s: fail to copy topology!\n", __func__);
			result = -EFAULT;
		}
		store_adm_rx_topology(topology);
		goto done;
	case AUDIO_SET_ADM_TX_TOPOLOGY:
		if (copy_from_user(&topology, (void *)arg,
				sizeof(topology))) {
			pr_err("%s: fail to copy topology!\n", __func__);
			result = -EFAULT;
		}
		store_adm_tx_topology(topology);
		goto done;
	case AUDIO_SET_ASM_TOPOLOGY:
		if (copy_from_user(&topology, (void *)arg,
				sizeof(topology))) {
			pr_err("%s: fail to copy topology!\n", __func__);
			result = -EFAULT;
		}
		store_asm_topology(topology);
		goto done;
	case AUDIO_SET_SPEAKER_PROT:
		if (copy_from_user(&acdb_data.spk_prot_cfg, (void *)arg,
				sizeof(acdb_data.spk_prot_cfg))) {
			pr_err("%s fail to copy spk_prot_cfg\n", __func__);
			result = -EFAULT;
		}
		goto done;
	case AUDIO_GET_SPEAKER_PROT:
		/*Indicates calibration was succesfull*/
		if (acdb_data.spk_prot_cfg.mode == MSM_SPKR_PROT_CALIBRATED) {
			prot_status.r0 = acdb_data.spk_prot_cfg.r0;
			prot_status.status = 0;
		} else if (acdb_data.spk_prot_cfg.mode ==
				   MSM_SPKR_PROT_CALIBRATION_IN_PROGRESS) {
			/*Call AFE to query the status*/
			acdb_spk_status.status = -EINVAL;
			acdb_spk_status.r0 = -1;
			get_spk_protection_status(&acdb_spk_status);
			prot_status.r0 = acdb_spk_status.r0;
			prot_status.status = acdb_spk_status.status;
			if (!acdb_spk_status.status) {
				acdb_data.spk_prot_cfg.mode =
					MSM_SPKR_PROT_CALIBRATED;
				acdb_data.spk_prot_cfg.r0 = prot_status.r0;
			}
		} else {
			/*Indicates calibration data is invalid*/
			prot_status.status = -EINVAL;
			prot_status.r0 = -1;
		}
		if (copy_to_user((void *)arg, &prot_status,
			sizeof(prot_status))) {
			pr_err("%s: Failed to update prot_status\n", __func__);
		}
		goto done;
	case AUDIO_REGISTER_VOCPROC_VOL_TABLE:
		result = register_vocvol_table();
		goto done;
	case AUDIO_DEREGISTER_VOCPROC_VOL_TABLE:
		result = deregister_vocvol_table();
		goto done;
	case AUDIO_SET_HW_DELAY_RX:
		result = store_hw_delay(RX_CAL, (void *)arg);
		goto done;
	case AUDIO_SET_HW_DELAY_TX:
		result = store_hw_delay(TX_CAL, (void *)arg);
		goto done;
	case AUDIO_SET_META_INFO:
		result = store_meta_info((void *)arg);
		goto done;
	}

	if (copy_from_user(&size, (void *) arg, sizeof(size))) {

		result = -EFAULT;
		goto done;
	}

	if ((size <= 0) || (size > sizeof(data))) {
		pr_err("%s: Invalid size sent to driver: %d\n",
			__func__, size);
		result = -EFAULT;
		goto done;
	}

	switch (cmd) {
	case AUDIO_SET_VOCPROC_COL_CAL:
		result = store_voice_col_data(VOCPROC_CAL,
						size, (uint32_t *)arg);
		goto done;
	case AUDIO_SET_VOCSTRM_COL_CAL:
		result = store_voice_col_data(VOCSTRM_CAL,
						size, (uint32_t *)arg);
		goto done;
	case AUDIO_SET_VOCVOL_COL_CAL:
		result = store_voice_col_data(VOCVOL_CAL,
						size, (uint32_t *)arg);
		goto done;
	}

	if (copy_from_user(data, (void *)(arg + sizeof(size)), size)) {

		pr_err("%s: fail to copy table size %d\n", __func__, size);
		result = -EFAULT;
		goto done;
	}

	if (data == NULL) {
		pr_err("%s: NULL pointer sent to driver!\n", __func__);
		result = -EFAULT;
		goto done;
	}

	if (size > sizeof(struct cal_block))
		pr_err("%s: More cal data for ioctl 0x%x then expected, size received: %d\n",
			__func__, cmd, size);

	switch (cmd) {
	case AUDIO_SET_AUDPROC_TX_CAL:
		result = store_audproc_cal(TX_CAL, (struct cal_block *)data);
		goto done;
	case AUDIO_SET_AUDPROC_RX_CAL:
		result = store_audproc_cal(RX_CAL, (struct cal_block *)data);
		goto done;
	case AUDIO_SET_AUDPROC_TX_STREAM_CAL:
		result = store_audstrm_cal(TX_CAL, (struct cal_block *)data);
		goto done;
	case AUDIO_SET_AUDPROC_RX_STREAM_CAL:
		result = store_audstrm_cal(RX_CAL, (struct cal_block *)data);
		goto done;
	case AUDIO_SET_AUDPROC_TX_VOL_CAL:
		result = store_audvol_cal(TX_CAL, (struct cal_block *)data);
		goto done;
	case AUDIO_SET_AUDPROC_RX_VOL_CAL:
		result = store_audvol_cal(RX_CAL, (struct cal_block *)data);
		goto done;
	case AUDIO_SET_AFE_TX_CAL:
		result = store_afe_cal(TX_CAL, (struct cal_block *)data);
		goto done;
	case AUDIO_SET_AFE_RX_CAL:
		result = store_afe_cal(RX_CAL, (struct cal_block *)data);
		goto done;
	case AUDIO_SET_VOCPROC_CAL:
		result = store_vocproc_cal((struct cal_block *)data);
		goto done;
	case AUDIO_SET_VOCPROC_STREAM_CAL:
		result = store_vocstrm_cal((struct cal_block *)data);
		goto done;
	case AUDIO_SET_VOCPROC_VOL_CAL:
		result = store_vocvol_cal((struct cal_block *)data);
		goto done;
	case AUDIO_SET_VOCPROC_DEV_CFG_CAL:
		result = store_vocproc_dev_cfg_cal((struct cal_block *)data);
		goto done;
	case AUDIO_SET_SIDETONE_CAL:
		store_sidetone_cal((struct sidetone_cal *)data);
		goto done;
	case AUDIO_SET_ANC_CAL:
		result = store_anc_cal((struct cal_block *)data);
		goto done;
	case AUDIO_SET_LSM_CAL:
		result = store_lsm_cal((struct cal_block *)data);
		goto done;
	case AUDIO_SET_ADM_CUSTOM_TOPOLOGY:
		result = store_adm_custom_topology((struct cal_block *)data);
		goto done;
	case AUDIO_SET_ASM_CUSTOM_TOPOLOGY:
		result = store_asm_custom_topology((struct cal_block *)data);
		goto done;
	case AUDIO_SET_AANC_CAL:
		result = store_aanc_cal((struct cal_block *)data);
		goto done;
	default:
		pr_err("ACDB=> ACDB ioctl not found!\n");
		result = -EFAULT;
		goto done;
	}

done:
	mutex_unlock(&acdb_data.acdb_mutex);
	return result;
}

static int acdb_mmap(struct file *file, struct vm_area_struct *vma)
{
	int result = 0;
	size_t size = vma->vm_end - vma->vm_start;

	pr_debug("%s\n", __func__);

	mutex_lock(&acdb_data.acdb_mutex);
	if (acdb_data.mem_len) {
		if (size <= acdb_data.mem_len) {
			vma->vm_page_prot = pgprot_noncached(
						vma->vm_page_prot);
			result = remap_pfn_range(vma,
				vma->vm_start,
				acdb_data.paddr >> PAGE_SHIFT,
				size,
				vma->vm_page_prot);
		} else {
			pr_err("%s: Not enough memory!\n", __func__);
			result = -ENOMEM;
		}
	} else {
		pr_err("%s: memory is not allocated, yet!\n", __func__);
		result = -ENODEV;
	}
	mutex_unlock(&acdb_data.acdb_mutex);

	return result;
}

static int acdb_release(struct inode *inode, struct file *f)
{
	int result = 0;
	pr_debug("%s\n", __func__);

	mutex_lock(&acdb_data.acdb_mutex);
	acdb_data.usage_count--;

	pr_debug("%s: ref count %d!\n", __func__, acdb_data.usage_count);

	if (acdb_data.usage_count > 0) {
		result = -EBUSY;
		goto done;
	}
	if (acdb_data.metainfo.nBuffer != NULL)
		kfree(acdb_data.metainfo.nBuffer);

	result = deregister_memory();
done:
	mutex_unlock(&acdb_data.acdb_mutex);
	return result;
}

static const struct file_operations acdb_fops = {
	.owner = THIS_MODULE,
	.open = acdb_open,
	.release = acdb_release,
	.unlocked_ioctl = acdb_ioctl,
	.mmap = acdb_mmap,
};

struct miscdevice acdb_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_acdb",
	.fops	= &acdb_fops,
};

static int __init acdb_init(void)
{
	memset(&acdb_data, 0, sizeof(acdb_data));
	/*Speaker protection disabled*/
	acdb_data.spk_prot_cfg.mode = MSM_SPKR_PROT_DISABLED;
	mutex_init(&acdb_data.acdb_mutex);
	acdb_data.usage_count = 0;
	acdb_data.valid_adm_custom_top = 1;
	acdb_data.valid_asm_custom_top = 1;

	return misc_register(&acdb_misc);
}

static void __exit acdb_exit(void)
{
}

module_init(acdb_init);
module_exit(acdb_exit);

MODULE_DESCRIPTION("SoC QDSP6v2 Audio ACDB driver");
MODULE_LICENSE("GPL v2");
