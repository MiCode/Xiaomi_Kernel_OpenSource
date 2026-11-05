// SPDX-License-Identifier: GPL-2.0
//
// cl_dsp.c -- DSP Control for non-ALSA Cirrus Logic Devices
//
// Copyright 2021 Cirrus Logic, Inc.
//
// Author: Fred Treven <fred.treven@cirrus.com>
#define DEBUG

#include <linux/firmware/cirrus/cl_dsp.h>

struct cl_dsp_memchunk cl_dsp_memchunk_create(void *data, int size)
{
	struct cl_dsp_memchunk ch = {
		.data = data,
		.max = data + size,
	};

	return ch;
}
EXPORT_SYMBOL_GPL(cl_dsp_memchunk_create);

inline bool cl_dsp_memchunk_end(struct cl_dsp_memchunk *ch)
{
	return ch->data == ch->max;
}
EXPORT_SYMBOL_GPL(cl_dsp_memchunk_end);

static inline bool cl_dsp_memchunk_valid_addr(struct cl_dsp_memchunk *ch,
		void *addr)
{
	return (u8 *)addr <= ch->max;
}

int cl_dsp_memchunk_read(struct cl_dsp *dsp, struct cl_dsp_memchunk *ch,
		int nbits, void *val)
{
	int nbytes = nbits / 8, nread, i;
	u32 result = 0;

	if (nbits > 32) {
		dev_err(dsp->dev, "Exceeded maximum read length: %d > 32\n", nbits);
		return -EINVAL;
	}

	while (nbits) {
		if (!ch->cachebits) {
			if (cl_dsp_memchunk_end(ch)) {
				dev_err(dsp->dev, "Read past end of memory chunk\n");
				return -ENOSPC;
			}

			ch->cache = 0;
			ch->cachebits = 24;

			for (i = 0; i < sizeof(ch->cache); i++, ch->cache <<= 8)
				ch->cache |= *ch->data++;

			ch->bytes += sizeof(ch->cache);
		}

		nread = min(ch->cachebits, nbits);
		nbits -= nread;

		result |= ((ch->cache >> (32 - nread)) << nbits);
		ch->cache <<= nread;
		ch->cachebits -= nread;
	}

	if (val)
		memcpy(val, &result, nbytes);

	return 0;
}
EXPORT_SYMBOL_GPL(cl_dsp_memchunk_read);

int cl_dsp_memchunk_write(struct cl_dsp_memchunk *ch, int nbits, u32 val)
{
	int nwrite, i;

	nwrite = min(24 - ch->cachebits, nbits);

	ch->cache <<= nwrite;
	ch->cache |= val >> (nbits - nwrite);
	ch->cachebits += nwrite;
	nbits -= nwrite;

	if (ch->cachebits == 24) {
		if (cl_dsp_memchunk_end(ch))
			return -ENOSPC;

		ch->cache &= 0xFFFFFF;
		for (i = 0; i < sizeof(ch->cache); i++, ch->cache <<= 8)
			*ch->data++ = (ch->cache & 0xFF000000) >> 24;

		ch->bytes += sizeof(ch->cache);
		ch->cachebits = 0;
	}

	if (nbits)
		return cl_dsp_memchunk_write(ch, nbits, val);

	return 0;
}
EXPORT_SYMBOL_GPL(cl_dsp_memchunk_write);

int cl_dsp_memchunk_flush(struct cl_dsp_memchunk *ch)
{
	if (!ch->cachebits)
		return 0;

	return cl_dsp_memchunk_write(ch, 24 - ch->cachebits, 0);
}
EXPORT_SYMBOL_GPL(cl_dsp_memchunk_flush);

int cl_dsp_raw_write(struct cl_dsp *dsp, unsigned int reg,
		const void *val, size_t val_len, size_t limit)
{
	int i, ret = 0;

	if (!dsp)
		return -EPERM;

	/* Restrict write length to limit value */
	for (i = 0; i < val_len; i += limit) {
		ret = regmap_raw_write(dsp->regmap, (reg + i), (val + i),
				(val_len - i) > limit ? limit : (val_len - i));
		if (ret)
			break;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(cl_dsp_raw_write);

static struct cl_dsp_coeff_desc *cl_dsp_get_coeff(struct cl_dsp *dsp, const char *coeff_name,
		const unsigned int block_type, const unsigned int algo_id)
{
	struct cl_dsp_coeff_desc *coeff_desc;
	unsigned int mem_region_prefix;

	if (!dsp)
		return ERR_PTR(-EPERM);

	if (list_empty(&dsp->coeff_desc_head)) {
		dev_err(dsp->dev, "Coefficient list is empty\n");
		return ERR_PTR(-ENODATA);
	}

	list_for_each_entry(coeff_desc, &dsp->coeff_desc_head, list) {
		if (strncmp(coeff_desc->name, coeff_name, CL_DSP_COEFF_NAME_LEN_MAX))
			continue;
		if (coeff_desc->block_type != block_type)
			continue;
		if ((coeff_desc->parent_id & 0xFFFF) != (algo_id & 0xFFFF))
			continue;

		if (coeff_desc->reg == 0) {
			dev_err(dsp->dev, "No control %s for block type 0x%X\n",
					coeff_name, block_type);
			return ERR_PTR(-EINVAL);
		}
		break;
	}

	/* verify register found in expected region */
	switch (block_type) {
	case CL_DSP_XM_PACKED_TYPE:
		mem_region_prefix = CL_DSP_MEM_REGION_PREFIX(CL_DSP_HALO_XMEM_PACKED_BASE);
		break;
	case CL_DSP_XM_UNPACKED_TYPE:
		mem_region_prefix = CL_DSP_MEM_REGION_PREFIX(CL_DSP_HALO_XMEM_UNPACKED24_BASE);
		break;
	case CL_DSP_YM_PACKED_TYPE:
		mem_region_prefix = CL_DSP_MEM_REGION_PREFIX(CL_DSP_HALO_YMEM_PACKED_BASE);
		break;
	case CL_DSP_YM_UNPACKED_TYPE:
		mem_region_prefix = CL_DSP_MEM_REGION_PREFIX(CL_DSP_HALO_YMEM_UNPACKED24_BASE);
		break;
	case CL_DSP_PM_PACKED_TYPE:
		mem_region_prefix = CL_DSP_MEM_REGION_PREFIX(CL_DSP_HALO_PMEM_BASE);
		break;
	default:
		dev_err(dsp->dev, "Unrecognized block type: 0x%X\n", block_type);
		return ERR_PTR(-EINVAL);
	}

	if (CL_DSP_MEM_REGION_PREFIX(coeff_desc->reg) != mem_region_prefix) {
		dev_err(dsp->dev, "DSP control %s at 0x%X found in unexpected region\n",
				coeff_name, coeff_desc->reg);

		return ERR_PTR(-EFAULT);
	}

	return coeff_desc;
}

int cl_dsp_get_reg(struct cl_dsp *dsp, const char *coeff_name, const unsigned int block_type,
		const unsigned int algo_id, unsigned int *reg)
{
	struct cl_dsp_coeff_desc *coeff_desc;

	coeff_desc = cl_dsp_get_coeff(dsp, coeff_name, block_type, algo_id);
	if (IS_ERR_OR_NULL(coeff_desc))
		return coeff_desc ? PTR_ERR(coeff_desc) : -ENXIO;

	*reg = coeff_desc->reg;

	return 0;
}
EXPORT_SYMBOL_GPL(cl_dsp_get_reg);

int cl_dsp_get_flags(struct cl_dsp *dsp, const char *coeff_name, const unsigned int block_type,
		const unsigned int algo_id, unsigned int *flags)
{
	struct cl_dsp_coeff_desc *coeff_desc;

	coeff_desc = cl_dsp_get_coeff(dsp, coeff_name, block_type, algo_id);
	if (IS_ERR_OR_NULL(coeff_desc))
		return coeff_desc ? PTR_ERR(coeff_desc) : -ENXIO;

	*flags = coeff_desc->flags;

	return 0;
}
EXPORT_SYMBOL_GPL(cl_dsp_get_flags);

int cl_dsp_get_length(struct cl_dsp *dsp, const char *coeff_name, const unsigned int block_type,
		const unsigned int algo_id, size_t *length)
{
	struct cl_dsp_coeff_desc *coeff_desc;

	coeff_desc = cl_dsp_get_coeff(dsp, coeff_name, block_type, algo_id);
	if (IS_ERR_OR_NULL(coeff_desc))
		return coeff_desc ? PTR_ERR(coeff_desc) : -EINVAL;

	*length = (size_t) coeff_desc->length;

	return 0;
}
EXPORT_SYMBOL_GPL(cl_dsp_get_length);

bool cl_dsp_algo_is_present(struct cl_dsp *dsp, const unsigned int algo_id)
{
	int i;

	if (!dsp)
		return false;

	for (i = 0; i < dsp->num_algos; i++) {
		if ((GENMASK(15, 0) & dsp->algo_info[i].id) ==
						(GENMASK(15, 0) & algo_id))
			return true;
	}

	return false;
}
EXPORT_SYMBOL_GPL(cl_dsp_algo_is_present);

static int cl_dsp_process_data_be(const u8 *data,
		const unsigned int num_bytes, unsigned int *val)
{
	int i;

	if (num_bytes > sizeof(unsigned int))
		return -EINVAL;

	*val = 0;
	for (i = 0; i < num_bytes; i++)
		*val += *(data + i) << (i * CL_DSP_BITS_PER_BYTE);

	return 0;
}

static int cl_dsp_owt_init(struct cl_dsp *dsp, const struct firmware *bin)
{
	if (!dsp->wt_desc) {
		dev_err(dsp->dev, "Wavetable not supported by core driver\n");
		return -EPERM;
	}

	dsp->wt_desc->owt.raw_data = devm_kzalloc(dsp->dev, bin->size,
			GFP_KERNEL);
	if (!dsp->wt_desc->owt.raw_data)
		return -ENOMEM;

	memcpy(dsp->wt_desc->owt.raw_data, &bin->data[0], bin->size);

	return 0;
}

static int cl_dsp_read_wt(struct cl_dsp *dsp, int pos, int size)
{
	struct cl_dsp_owt_header *entry = dsp->wt_desc->owt.waves;
	void *buf = (void *)(dsp->wt_desc->owt.raw_data + pos);
	struct cl_dsp_memchunk ch = cl_dsp_memchunk_create(buf, size);
	u32 *wbuf = buf, *max = buf;
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(dsp->wt_desc->owt.waves); i++, entry++) {
		ret = cl_dsp_memchunk_read(dsp, &ch, 16, &entry->flags);
		if (ret)
			return ret;

		ret = cl_dsp_memchunk_read(dsp, &ch, 8, &entry->type);
		if (ret)
			return ret;

		if (entry->type == WT_TYPE_TERMINATOR) {
			dsp->wt_desc->owt.nwaves = i;
			dsp->wt_desc->owt.bytes = max(ch.bytes,
					(int)((void *)max - buf));

			return dsp->wt_desc->owt.bytes;
		}

		ret = cl_dsp_memchunk_read(dsp, &ch, 24, &entry->offset);
		if (ret)
			return ret;

		ret = cl_dsp_memchunk_read(dsp, &ch, 24, &entry->size);
		if (ret)
			return ret;

		entry->data = wbuf + entry->offset;

		if (wbuf + entry->offset + entry->size > max) {
			max = wbuf + entry->offset + entry->size;

			if (!cl_dsp_memchunk_valid_addr(&ch, max))
				return -EINVAL;
		}
	}

	dev_err(dsp->dev, "Maximum number of wavetable entries exceeded\n");
	return -E2BIG;
}

static int cl_dsp_coeff_header_parse(struct cl_dsp *dsp,
		union cl_dsp_wmdr_header header)
{
	struct device *dev = dsp->dev;

	if (memcmp(header.magic, CL_DSP_WMDR_MAGIC_ID, CL_DSP_MAGIC_ID_SIZE)) {
		dev_err(dev, "Failed to recognize coefficient file\n");
		return -EINVAL;
	}

	if (header.header_len != CL_DSP_COEFF_FILE_HEADER_SIZE) {
		dev_err(dev, "Malformed coeff. header\n");
		return -EINVAL;
	}

	if (header.fw_revision != dsp->algo_info[0].rev) {
		dev_warn(dev,
			"Coeff. rev. 0x%06X mismatches 0x%06X, continuing..\n",
			header.fw_revision, dsp->algo_info[0].rev);
	}

	if (header.file_format_version < CL_DSP_COEFF_MIN_FORMAT_VERSION) {
		dev_err(dev, "File format version 0x%02X is outdated\n",
				header.file_format_version);
		return -EINVAL;
	}

	if (header.api_revision != CL_DSP_COEFF_API_REV_HALO &&
			header.api_revision != CL_DSP_COEFF_API_REV_ADSP2) {
		dev_err(dev, "API Revision 0x%06X is not compatible\n",
				header.api_revision);
		return -EINVAL;
	}

	if (header.target_core != CL_DSP_TARGET_CORE_ADSP2 &&
			header.target_core != CL_DSP_TARGET_CORE_HALO) {
		dev_err(dev, "Target core 0x%02X is not compatible\n",
				header.target_core);
		return -EINVAL;
	}

	return 0;
}

static void cl_dsp_coeff_handle_info_text(struct cl_dsp *dsp, const u8 *data,
		u32 len)
{
	char *info_str;

	info_str = kzalloc(len + 1, GFP_KERNEL);
	if (!info_str)
		return;

	memcpy(info_str, data, len);

	dev_dbg(dsp->dev, "WMDR Info: %s\n", info_str);

	kfree(info_str);
}

static int cl_dsp_wavetable_check(struct cl_dsp *dsp, const struct firmware *fw,
		unsigned int reg, unsigned int pos, u32 data_len, u32 type)
{
	u32 data_len_bytes = data_len / 4 * 3;
	unsigned int limit, wt_reg;
	bool is_xm;
	int ret;

	if (type == CL_DSP_XM_UNPACKED_TYPE) {
		is_xm = true;
		limit = dsp->wt_desc->wt_limit_xm;
		ret = cl_dsp_get_reg(dsp, dsp->wt_desc->wt_name_xm,
				type, dsp->wt_desc->id, &wt_reg);
	} else if (type == CL_DSP_YM_UNPACKED_TYPE) {
		is_xm = false;
		limit = dsp->wt_desc->wt_limit_ym;
		ret = cl_dsp_get_reg(dsp, dsp->wt_desc->wt_name_ym,
				type, dsp->wt_desc->id, &wt_reg);
	} else {
		dev_err(dsp->dev, "Invalid wavetable memory type 0x%04X\n",
				type);
		ret = -EINVAL;
	}
	if (ret)
		return ret;

	if (reg == wt_reg) {
		if (data_len > limit) {
			dev_err(dsp->dev, "%s too large: %d bytes\n",
				is_xm ? "XM" : "YM", data_len_bytes);
			return -EFBIG;
		}

		ret = cl_dsp_owt_init(dsp, fw);
		if (ret)
			return ret;

		ret = cl_dsp_read_wt(dsp, pos, data_len);
		if (ret < 0)
			return ret;

		dsp->wt_desc->is_xm = is_xm;

		dev_info(dsp->dev, "Wavetable found: %d bytes (XM)\n",
				data_len_bytes);
	}

	return 0;
}

int cl_dsp_coeff_file_parse(struct cl_dsp *dsp, const struct firmware *fw)
{
	unsigned int pos = CL_DSP_COEFF_FILE_HEADER_SIZE;
	bool wt_found = false;
	int ret = -EINVAL;
	struct cl_dsp_coeff_data_block data_block;
	union cl_dsp_wmdr_header wmdr_header;
	char wt_date[CL_DSP_WMDR_DATE_LEN];
	unsigned int reg, algo_rev;
	u16 algo_id, parent_id;
	struct device *dev;
	u32 data_len;
	int i;

	if (!dsp)
		return -EPERM;

	dev = dsp->dev;

	*wt_date = '\0';

	memcpy(wmdr_header.data, fw->data, CL_DSP_COEFF_FILE_HEADER_SIZE);

	if (fw->size % CL_DSP_BYTES_PER_WORD) {
		dev_err(dev, "Coefficient file is not word-aligned\n");
		return ret;
	}

	ret = cl_dsp_coeff_header_parse(dsp, wmdr_header);
	if (ret)
		return ret;

	while (pos < fw->size) {
		memcpy(data_block.header.data, &fw->data[pos],
				CL_DSP_COEFF_DBLK_HEADER_SIZE);
		pos += CL_DSP_COEFF_DBLK_HEADER_SIZE;

		data_len = data_block.header.data_len;
		data_block.payload = kvmalloc(data_len, GFP_KERNEL);
		if (!data_block.payload)
			return -ENOMEM;

		memcpy(data_block.payload, &fw->data[pos], data_len);

		algo_id = data_block.header.algo_id & 0xFFFF;

		if (data_block.header.block_type != CL_DSP_WMDR_NAME_TYPE &&
			data_block.header.block_type != CL_DSP_WMDR_INFO_TYPE) {
			for (i = 0; i < dsp->num_algos; i++) {
				parent_id = dsp->algo_info[i].id & 0xFFFF;
				if (algo_id == parent_id)
					break;
			}

			if (i == dsp->num_algos) {
				dev_err(dev, "Invalid algo. ID: 0x%06X\n",
						data_block.header.algo_id);
				ret = -EINVAL;
				goto err_free;
			}

			algo_rev = data_block.header.algo_rev >>
					CL_DSP_REV_OFFSET_SHIFT;

			if (CL_DSP_GET_MAJOR(algo_rev) !=
				CL_DSP_GET_MAJOR(dsp->algo_info[i].rev)) {
				dev_err(dev,
				"Invalid algo. rev.: %d.%d.%d (0x%06X)\n",
					(int) CL_DSP_GET_MAJOR(algo_rev),
					(int) CL_DSP_GET_MINOR(algo_rev),
					(int) CL_DSP_GET_PATCH(algo_rev),
					data_block.header.algo_id);

				ret = -EINVAL;
				goto err_free;
			}

			wt_found = (algo_id == (dsp->wt_desc->id & 0xFFFF));
		}

		switch (data_block.header.block_type) {
		case CL_DSP_WMDR_NAME_TYPE:
		case CL_DSP_WMDR_INFO_TYPE:
			reg = 0;

			cl_dsp_coeff_handle_info_text(dsp, data_block.payload,
					data_len);

			if (data_len < CL_DSP_WMDR_DATE_LEN)
				break;

			if (memcmp(&fw->data[pos], CL_DSP_WMDR_DATE_PREFIX,
					CL_DSP_WMDR_DATE_PREFIX_LEN))
				break; /* date already recorded */

			memcpy(wt_date,
				&fw->data[pos + CL_DSP_WMDR_DATE_PREFIX_LEN],
				CL_DSP_WMDR_DATE_LEN -
				CL_DSP_WMDR_DATE_PREFIX_LEN);

			wt_date[CL_DSP_WMDR_DATE_LEN -
					CL_DSP_WMDR_DATE_PREFIX_LEN] = '\0';
			break;
		case CL_DSP_XM_UNPACKED_TYPE:
			reg = CL_DSP_HALO_XMEM_UNPACKED24_BASE +
					data_block.header.start_offset +
					dsp->algo_info[i].xm_base *
					CL_DSP_BYTES_PER_WORD;

			if (wt_found) {
				ret = cl_dsp_wavetable_check(dsp,
						fw, reg, pos, data_len,
						CL_DSP_XM_UNPACKED_TYPE);
				if (ret)
					goto err_free;
			}
			break;
		case CL_DSP_XM_PACKED_TYPE:
			reg = (CL_DSP_HALO_XMEM_PACKED_BASE +
					data_block.header.start_offset +
					dsp->algo_info[i].xm_base *
					CL_DSP_PACKED_NUM_BYTES) &
					~CL_DSP_ALIGN;
			break;
		case CL_DSP_YM_UNPACKED_TYPE:
			reg = CL_DSP_HALO_YMEM_UNPACKED24_BASE +
					data_block.header.start_offset +
					dsp->algo_info[i].ym_base *
					CL_DSP_UNPACKED_NUM_BYTES;
			if (wt_found) {
				ret = cl_dsp_wavetable_check(dsp,
						fw, reg, pos, data_len,
						CL_DSP_YM_UNPACKED_TYPE);
				if (ret)
					goto err_free;
			}
			break;
		case CL_DSP_YM_PACKED_TYPE:
			reg = (CL_DSP_HALO_YMEM_PACKED_BASE +
					data_block.header.start_offset +
					dsp->algo_info[i].ym_base *
					CL_DSP_PACKED_NUM_BYTES) &
					~CL_DSP_ALIGN;
		break;
		default:
			dev_err(dev, "Unexpected block type: 0x%04X\n",
					data_block.header.block_type);
			ret = -EINVAL;
			goto err_free;
		}
		if (reg) {
			ret = cl_dsp_raw_write(dsp, reg, &fw->data[pos],
					data_len, CL_DSP_MAX_WLEN);
			if (ret) {
				dev_err(dev, "Failed to write coefficients\n");
				goto err_free;
			}
		}

		/* Blocks are word-aligned */
		pos += (data_len + 3) & ~CL_DSP_ALIGN;

		kvfree(data_block.payload);
	}

	if (wt_found) {
		if (*wt_date != '\0')
			strscpy(dsp->wt_desc->wt_date, wt_date,
					CL_DSP_WMDR_DATE_LEN);
		else
			strscpy(dsp->wt_desc->wt_date,
					CL_DSP_WMDR_FILE_DATE_MISSING,
					CL_DSP_WMDR_DATE_LEN);
	}

	return 0;

err_free:
	kvfree(data_block.payload);

	return ret;
}
EXPORT_SYMBOL_GPL(cl_dsp_coeff_file_parse);

static int cl_dsp_algo_parse(struct cl_dsp *dsp, const unsigned char *data)
{
	struct cl_dsp_coeff_desc *coeff_desc;
	u8 algo_name_len;
	u16 algo_desc_len, block_offset, block_type;
	u32 algo_id, block_len;
	char *algo_name;
	unsigned int coeff_count, val, pos = 0, header_pos = 0;
	unsigned int coeff_name_len, coeff_fullname_len, coeff_desc_len;
	unsigned int coeff_flags, coeff_len;
	int i, ret;

	ret = cl_dsp_process_data_be(&data[pos], CL_DSP_ALGO_ID_SIZE,
			&algo_id);
	if (ret) {
		dev_err(dsp->dev, "Failed to read data\n");
		return ret;
	}
	pos += CL_DSP_ALGO_ID_SIZE;

	ret = cl_dsp_process_data_be(&data[pos], CL_DSP_ALGO_NAME_LEN_SIZE,
			&val);
	if (ret) {
		dev_err(dsp->dev, "Failed to read data\n");
		return ret;
	}
	algo_name_len = (u8) (val & CL_DSP_BYTE_MASK);

	algo_name = kzalloc(algo_name_len, GFP_KERNEL);
	if (!algo_name)
		return -ENOMEM;

	memcpy(algo_name, &data[pos + CL_DSP_ALGO_NAME_LEN_SIZE],
			algo_name_len);
	pos += CL_DSP_WORD_ALIGN(algo_name_len);

	ret = cl_dsp_process_data_be(&data[pos], CL_DSP_ALGO_DESC_LEN_SIZE,
			&val);
	if (ret) {
		dev_err(dsp->dev, "Failed to read data\n");
		goto err_free;
	}

	/* Nothing required for algo. description, so it is skipped */
	algo_desc_len = (u16) (val & CL_DSP_NIBBLE_MASK);
	pos += CL_DSP_WORD_ALIGN(algo_desc_len);

	ret = cl_dsp_process_data_be(&data[pos], CL_DSP_COEFF_COUNT_SIZE,
			&coeff_count);
	if (ret) {
		dev_err(dsp->dev, "Failed to read data\n");
		goto err_free;
	}
	pos += CL_DSP_COEFF_COUNT_SIZE;

	for (i = 0; i < coeff_count; i++) {
		ret = cl_dsp_process_data_be(&data[pos],
				CL_DSP_COEFF_OFFSET_SIZE, &val);
		if (ret) {
			dev_err(dsp->dev, "Failed to read data\n");
			goto err_free;
		}

		block_offset = (u16) (val & CL_DSP_NIBBLE_MASK);
		pos += CL_DSP_COEFF_OFFSET_SIZE;

		ret = cl_dsp_process_data_be(&data[pos],
				CL_DSP_COEFF_TYPE_SIZE, &val);
		if (ret) {
			dev_err(dsp->dev, "Failed to read data\n");
			goto err_free;
		}

		block_type = (u16) (val & CL_DSP_NIBBLE_MASK);
		pos += CL_DSP_COEFF_TYPE_SIZE;

		ret = cl_dsp_process_data_be(&data[pos], CL_DSP_COEFF_LEN_SIZE,
				&block_len);
		if (ret) {
			dev_err(dsp->dev, "Failed to read data\n");
			goto err_free;
		}
		pos += CL_DSP_COEFF_LEN_SIZE;
		header_pos = pos;

		coeff_desc = devm_kzalloc(dsp->dev, sizeof(*coeff_desc),
				GFP_KERNEL);
		if (!coeff_desc) {
			ret = -ENOMEM;
			goto err_free;
		}

		coeff_desc->parent_id = algo_id;
		coeff_desc->parent_name = algo_name;
		coeff_desc->block_offset = block_offset;
		coeff_desc->block_type = block_type;

		memcpy(coeff_desc->name, data + pos + 1, *(data + pos));
		coeff_desc->name[*(data + pos)] = '\0';

		ret = cl_dsp_process_data_be(&data[pos],
				CL_DSP_COEFF_NAME_LEN_SIZE, &coeff_name_len);
		if (ret) {
			dev_err(dsp->dev, "Failed to read data\n");
			return ret;
		}
		pos += CL_DSP_WORD_ALIGN(coeff_name_len);

		ret = cl_dsp_process_data_be(&data[pos],
				CL_DSP_COEFF_FULLNAME_LEN_SIZE,
				&coeff_fullname_len);
		if (ret) {
			dev_err(dsp->dev, "Failed to read data\n");
			return ret;
		}
		pos += CL_DSP_WORD_ALIGN(coeff_fullname_len);

		ret = cl_dsp_process_data_be(&data[pos],
				CL_DSP_COEFF_DESC_LEN_SIZE, &coeff_desc_len);

		if (ret) {
			dev_err(dsp->dev, "Failed to read data\n");
			return ret;
		}
		pos += CL_DSP_WORD_ALIGN(coeff_desc_len);

		ret = cl_dsp_process_data_be(&data[pos],
				CL_DSP_COEFF_FLAGS_SIZE, &coeff_flags);
		if (ret) {
			dev_err(dsp->dev, "Failed to read data\n");
			return ret;
		}
		pos += CL_DSP_COEFF_FLAGS_SIZE;

		ret = cl_dsp_process_data_be(&data[pos], CL_DSP_COEFF_LEN_SIZE,
				&coeff_len);
		if (ret) {
			dev_err(dsp->dev, "Failed to read data\n");
			return ret;
		}
		pos += CL_DSP_COEFF_LEN_SIZE;

		coeff_desc->flags = coeff_flags >> CL_DSP_COEFF_FLAGS_SHIFT;
		coeff_desc->length = coeff_len;

		list_add(&coeff_desc->list, &dsp->coeff_desc_head);

		pos = header_pos + block_len;
	}

err_free:
	kfree(algo_name);

	return ret;
}

int cl_dsp_fw_id_get(struct cl_dsp *dsp, unsigned int *id)
{
	int ret = 0;

	ret = regmap_read(dsp->regmap, CL_DSP_HALO_XM_FW_ID_REG, id);
	if (ret)
		dev_err(dsp->dev, "Failed to read firmware ID\n");

	return ret;
}
EXPORT_SYMBOL_GPL(cl_dsp_fw_id_get);

int cl_dsp_fw_rev_get(struct cl_dsp *dsp, unsigned int *rev)
{
	int ret = 0;

	ret = regmap_read(dsp->regmap, CL_DSP_HALO_XM_FW_ID_REG +
			CL_DSP_HALO_ALGO_REV_OFFSET, rev);
	if (ret)
		dev_err(dsp->dev, "Failed to read firmware revision\n");

	return ret;
}
EXPORT_SYMBOL_GPL(cl_dsp_fw_rev_get);

static int cl_dsp_coeff_init(struct cl_dsp *dsp)
{
	unsigned int reg = CL_DSP_HALO_XM_FW_ID_REG;
	struct cl_dsp_coeff_desc *coeff_desc;
	struct regmap *regmap;
	struct device *dev;
	unsigned int val;
	int ret, i;

	if (!dsp)
		return -EPERM;

	dev = dsp->dev;
	regmap = dsp->regmap;

	ret = regmap_read(regmap, CL_DSP_HALO_NUM_ALGOS_REG, &val);
	if (ret) {
		dev_err(dev, "Failed to read number of algorithms\n");
		return ret;
	}

	if (val > CL_DSP_NUM_ALGOS_MAX) {
		dev_err(dev, "Invalid number of algorithms: %d\n", val);
		return -EINVAL;
	}
	dsp->num_algos = val + 1;

	for (i = 0; i < dsp->num_algos; i++) {
		ret = regmap_read(regmap, reg, &dsp->algo_info[i].id);
		if (ret) {
			dev_err(dev, "Failed to read algo. %d ID\n", i);
			return ret;
		}

		ret = regmap_read(regmap, reg + CL_DSP_HALO_ALGO_REV_OFFSET,
				&dsp->algo_info[i].rev);
		if (ret) {
			dev_err(dev, "Failed to read algo. %d revision\n", i);
			return ret;
		}

		ret = regmap_read(regmap, reg +
				CL_DSP_HALO_ALGO_XM_BASE_OFFSET,
				&dsp->algo_info[i].xm_base);
		if (ret) {
			dev_err(dev, "Failed to read algo. %d XM_BASE\n", i);
			return ret;
		}

		ret = regmap_read(regmap, reg +
				CL_DSP_HALO_ALGO_XM_SIZE_OFFSET,
				&dsp->algo_info[i].xm_size);
		if (ret) {
			dev_err(dev, "Failed to read algo. %d XM_SIZE\n", i);
			return ret;
		}

		ret = regmap_read(regmap, reg +
				CL_DSP_HALO_ALGO_YM_BASE_OFFSET,
				&dsp->algo_info[i].ym_base);
		if (ret) {
			dev_err(dev, "Failed to read algo. %d YM_BASE\n", i);
			return ret;
		}

		ret = regmap_read(regmap, reg +
				CL_DSP_HALO_ALGO_YM_SIZE_OFFSET,
				&dsp->algo_info[i].ym_size);
		if (ret) {
			dev_err(dev, "Failed to read algo. %d YM_SIZE\n", i);
			return ret;
		}

		list_for_each_entry(coeff_desc, &dsp->coeff_desc_head, list) {
			if (coeff_desc->parent_id != dsp->algo_info[i].id)
				continue;

			switch (coeff_desc->block_type) {
			case CL_DSP_XM_UNPACKED_TYPE:
				coeff_desc->reg =
						CL_DSP_HALO_XMEM_UNPACKED24_BASE
						+ dsp->algo_info[i].xm_base
						* CL_DSP_UNPACKED_NUM_BYTES
						+ coeff_desc->block_offset
						* CL_DSP_UNPACKED_NUM_BYTES;

				if (dsp->wt_desc && !strncmp(coeff_desc->name,
						dsp->wt_desc->wt_name_xm,
						CL_DSP_COEFF_NAME_LEN_MAX))
					dsp->wt_desc->wt_limit_xm =
						(dsp->algo_info[i].xm_size -
						coeff_desc->block_offset) *
						CL_DSP_UNPACKED_NUM_BYTES;
				break;
			case CL_DSP_YM_UNPACKED_TYPE:
				coeff_desc->reg =
					CL_DSP_HALO_YMEM_UNPACKED24_BASE +
					dsp->algo_info[i].ym_base *
					CL_DSP_BYTES_PER_WORD +
					coeff_desc->block_offset *
					CL_DSP_BYTES_PER_WORD;

				if (dsp->wt_desc && !strncmp(coeff_desc->name,
						dsp->wt_desc->wt_name_ym,
						CL_DSP_COEFF_NAME_LEN_MAX))
					dsp->wt_desc->wt_limit_ym =
						(dsp->algo_info[i].ym_size -
						coeff_desc->block_offset) *
						CL_DSP_UNPACKED_NUM_BYTES;
				break;
			}
		}

		/* System algo. contains one extra register (num. algos.) */
		if (i)
			reg += CL_DSP_ALGO_ENTRY_SIZE;
		else
			reg += (CL_DSP_ALGO_ENTRY_SIZE +
					CL_DSP_BYTES_PER_WORD);
	}

	ret = regmap_read(regmap, reg, &val);
	if (ret) {
		dev_err(dev, "Failed to read list terminator\n");
		return ret;
	}

	if (val != CL_DSP_ALGO_LIST_TERM) {
		dev_err(dev, "Invalid list terminator: 0x%X\n", val);
		return -EINVAL;
	}

	if (dsp->wt_desc)
		dev_info(dev,
			"Max. wavetable size: %d bytes (XM), %d bytes (YM)\n",
			dsp->wt_desc->wt_limit_xm / 4 * 3,
			dsp->wt_desc->wt_limit_ym / 4 * 3);

	return 0;
}

static void cl_dsp_coeff_free(struct cl_dsp *dsp)
{
	struct cl_dsp_coeff_desc *coeff_desc;

	if (!dsp)
		return;

	while (!list_empty(&dsp->coeff_desc_head)) {
		coeff_desc = list_first_entry(&dsp->coeff_desc_head,
				struct cl_dsp_coeff_desc, list);
		list_del(&coeff_desc->list);
		devm_kfree(dsp->dev, coeff_desc);
	}
}

static int cl_dsp_header_parse(struct cl_dsp *dsp,
		union cl_dsp_wmfw_header header)
{
	struct device *dev = dsp->dev;

	if (memcmp(header.magic, CL_DSP_WMFW_MAGIC_ID, CL_DSP_MAGIC_ID_SIZE)) {
		dev_err(dev, "Failed to recognize firmware file\n");
		return -EINVAL;
	}

	if (header.header_len != CL_DSP_FW_FILE_HEADER_SIZE) {
		dev_err(dev, "Malformed fimrware header\n");
		return -EINVAL;
	}

	if (header.api_revision != CL_DSP_API_REVISION) {
		dev_err(dev, "Firmware API Revision Incompatible with Core\n");
		return -EINVAL;
	}

	if (header.target_core != CL_DSP_TARGET_CORE_HALO) {
		dev_err(dev, "Unexpected target core type: 0x%02X\n",
				header.target_core);
		return -EINVAL;
	}

	if (header.file_format_version < CL_DSP_MIN_FORMAT_VERSION) {
		dev_err(dev, "File Format Version 0x%02X is outdated",
				header.file_format_version);
		return -EINVAL;
	}

	dev_info(dev,
		"Loading memory (bytes): XM: %u, YM: %u, PM: %u, ZM: %u\n",
		header.xm_size, header.ym_size, header.pm_size, header.zm_size);

	return 0;
}

static void cl_dsp_handle_info_text(struct cl_dsp *dsp,
		const u8 *data, u32 len)
{
	char *info_str;

	info_str = kzalloc(len + 1, GFP_KERNEL);
	if (!info_str)
		/* info block is empty or memory not allocated */
		return;

	memcpy(info_str, data, len);

	dev_info(dsp->dev, "WMFW Info: %s\n", info_str);

	kfree(info_str);
}

int cl_dsp_firmware_parse(struct cl_dsp *dsp, const struct firmware *fw,
		bool write_fw)
{
	unsigned int pos = CL_DSP_FW_FILE_HEADER_SIZE, reg = 0;
	struct cl_dsp_data_block data_block;
	union cl_dsp_wmfw_header wmfw_header;
	struct device *dev;
	int ret;

	if (!dsp)
		return -EPERM;

	dev = dsp->dev;

	memcpy(wmfw_header.data, fw->data, CL_DSP_FW_FILE_HEADER_SIZE);

	ret = cl_dsp_header_parse(dsp, wmfw_header);
	if (ret)
		return ret;

	if (fw->size % CL_DSP_BYTES_PER_WORD) {
		dev_err(dev, "Firmware file is not word-aligned\n");
		return -EINVAL;
	}

	while (pos < fw->size) {
		memcpy(data_block.header.data, &fw->data[pos],
				CL_DSP_DBLK_HEADER_SIZE);

		pos += CL_DSP_DBLK_HEADER_SIZE;

		data_block.payload =
			kvmalloc(data_block.header.data_len, GFP_KERNEL);
		memcpy(data_block.payload, &fw->data[pos],
				data_block.header.data_len);

		switch (data_block.header.block_type) {
		case CL_DSP_WMFW_INFO_TYPE:
			reg = 0;
			cl_dsp_handle_info_text(dsp, data_block.payload,
					data_block.header.data_len);
			break;
		case CL_DSP_PM_PACKED_TYPE:
			reg = CL_DSP_HALO_PMEM_BASE +
					data_block.header.start_offset *
					CL_DSP_PM_NUM_BYTES;
			break;
		case CL_DSP_XM_PACKED_TYPE:
			reg = CL_DSP_HALO_XMEM_PACKED_BASE +
					data_block.header.start_offset *
					CL_DSP_PACKED_NUM_BYTES;
			break;
		case CL_DSP_XM_UNPACKED_TYPE:
			reg = CL_DSP_HALO_XMEM_UNPACKED24_BASE +
					data_block.header.start_offset *
					CL_DSP_UNPACKED_NUM_BYTES;
			break;
		case CL_DSP_YM_PACKED_TYPE:
			reg = CL_DSP_HALO_YMEM_PACKED_BASE +
					data_block.header.start_offset *
					CL_DSP_PACKED_NUM_BYTES;
			break;
		case CL_DSP_YM_UNPACKED_TYPE:
			reg = CL_DSP_HALO_YMEM_UNPACKED24_BASE +
					data_block.header.start_offset *
					CL_DSP_UNPACKED_NUM_BYTES;
			break;
		case CL_DSP_ALGO_INFO_TYPE:
			reg = 0;

			ret = cl_dsp_algo_parse(dsp, data_block.payload);
			if (ret)
				goto err_free;
			break;
		default:
			dev_err(dev, "Unexpected block type : 0x%02X\n",
					data_block.header.block_type);
			ret = -EINVAL;
			goto err_free;
		}

		if (write_fw && reg) {
			ret = cl_dsp_raw_write(dsp, reg, data_block.payload,
					data_block.header.data_len,
					CL_DSP_MAX_WLEN);
			if (ret) {
				dev_err(dev,
					"Failed to write to base 0x%X\n", reg);
				goto err_free;
			}
		}

		/* Blocks are word-aligned */
		pos += (data_block.header.data_len + 3) & ~CL_DSP_ALIGN;

		kvfree(data_block.payload);
	}

	return cl_dsp_coeff_init(dsp);

err_free:
	kvfree(data_block.payload);

	return ret;
}
EXPORT_SYMBOL_GPL(cl_dsp_firmware_parse);

int cl_dsp_wavetable_create(struct cl_dsp *dsp, unsigned int id,
		const char *wt_name_xm, const char *wt_name_ym,
		const char *wt_file)
{
	struct cl_dsp_wt_desc *wt_desc;

	if (!dsp)
		return -EPERM;

	wt_desc = devm_kzalloc(dsp->dev, sizeof(struct cl_dsp_wt_desc),
			GFP_KERNEL);
	if (!wt_desc)
		return -ENOMEM;

	wt_desc->id = id;
	strscpy(wt_desc->wt_name_xm, wt_name_xm, strlen(wt_name_xm) + 1);
	strscpy(wt_desc->wt_name_ym, wt_name_ym, strlen(wt_name_ym) + 1);
	strscpy(wt_desc->wt_file, wt_file, strlen(wt_file) + 1);

	dsp->wt_desc = wt_desc;

	return 0;
}
EXPORT_SYMBOL_GPL(cl_dsp_wavetable_create);

struct cl_dsp *cl_dsp_create(struct device *dev, struct regmap *regmap)
{
	struct cl_dsp *dsp;

	dsp = devm_kzalloc(dev, sizeof(struct cl_dsp), GFP_KERNEL);
	if (!dsp)
		return ERR_PTR(-ENOMEM);

	dsp->dev = dev;
	dsp->regmap = regmap;

	INIT_LIST_HEAD(&dsp->coeff_desc_head);

	return dsp;
}
EXPORT_SYMBOL_GPL(cl_dsp_create);

int cl_dsp_destroy(struct cl_dsp *dsp)
{
	if (!dsp)
		return -EPERM;

	if (!list_empty(&dsp->coeff_desc_head))
		cl_dsp_coeff_free(dsp);

	if (dsp->wt_desc)
		devm_kfree(dsp->dev, dsp->wt_desc);

	devm_kfree(dsp->dev, dsp);

	return 0;
}
EXPORT_SYMBOL_GPL(cl_dsp_destroy);

MODULE_DESCRIPTION("Cirrus Logic DSP Firmware Driver");
MODULE_AUTHOR("Fred Treven, Cirrus Logic Inc, <fred.treven@cirrus.com>");
MODULE_LICENSE("GPL");
