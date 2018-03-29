/*
 * YAFFS: Yet Another Flash File System. A NAND-flash specific file system.
 *
 * Copyright (C) 2002-2010 Aleph One Ltd.
 *   for Toby Churchill Ltd and Brightstar Engineering
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "yaffs_trace.h"

#include "yaffs_nand.h"
#include "yaffs_tagscompat.h"
#include "yaffs_tagsvalidity.h"

#include "yaffs_getblockinfo.h"

int yaffs_rd_chunk_tags_nand(struct yaffs_dev *dev, int nand_chunk,
			     u8 *buffer, struct yaffs_ext_tags *tags)
{
	int result;
	struct yaffs_ext_tags local_tags;

	int realigned_chunk = nand_chunk - dev->chunk_offset;

	dev->n_page_reads++;

	/* If there are no tags provided, use local tags to get prioritised gc working */
	if (!tags)
		tags = &local_tags;

	if (dev->param.read_chunk_tags_fn)
		result =
		    dev->param.read_chunk_tags_fn(dev, realigned_chunk, buffer,
						  tags);
	else
		result = yaffs_tags_compat_rd(dev,
					      realigned_chunk, buffer, tags);
	if (tags && tags->ecc_result > YAFFS_ECC_RESULT_NO_ERROR) {

		struct yaffs_block_info *bi;

		bi = yaffs_get_block_info(dev,
					  nand_chunk /
					  dev->param.chunks_per_block);
		yaffs_handle_chunk_error(dev, bi);
	}

	return result;
}

int yaffs_wr_chunk_tags_nand(struct yaffs_dev *dev,
			     int nand_chunk,
			     const u8 *buffer, struct yaffs_ext_tags *tags)
{

	dev->n_page_writes++;

	nand_chunk -= dev->chunk_offset;

	if (tags) {
		tags->seq_number = dev->seq_number;
		tags->chunk_used = 1;
		if (!yaffs_validate_tags(tags)) {
			yaffs_trace(YAFFS_TRACE_ERROR, "Writing uninitialised tags");
			YBUG();
		}
		yaffs_trace(YAFFS_TRACE_WRITE,
			"Writing chunk %d tags %d %d",
			nand_chunk, tags->obj_id, tags->chunk_id);
	} else {
		yaffs_trace(YAFFS_TRACE_ERROR, "Writing with no tags");
		YBUG();
	}

	if (dev->param.write_chunk_tags_fn)
		return dev->param.write_chunk_tags_fn(dev, nand_chunk, buffer,
						      tags);
	else
		return yaffs_tags_compat_wr(dev, nand_chunk, buffer, tags);
}

int yaffs_mark_bad(struct yaffs_dev *dev, int block_no)
{
	block_no -= dev->block_offset;

	if (dev->param.bad_block_fn)
		return dev->param.bad_block_fn(dev, block_no);
	else
		return yaffs_tags_compat_mark_bad(dev, block_no);
}

int yaffs_query_init_block_state(struct yaffs_dev *dev,
				 int block_no,
				 enum yaffs_block_state *state,
				 u32 *seq_number)
{
	block_no -= dev->block_offset;

	if (dev->param.query_block_fn)
		return dev->param.query_block_fn(dev, block_no, state,
						 seq_number);
	else
		return yaffs_tags_compat_query_block(dev, block_no,
						     state, seq_number);
}

#ifdef YAFFS_MVG_TEST_ERASECHEKFF
static int yaffs_ScanCheckEraseClean(struct yaffs_dev *dev,
				int chunkInNAND, int page)
{
	int retval = YAFFS_OK;
	u8 *data = yaffs_get_temp_buffer(dev, __LINE__);
	struct yaffs_ext_tags tags;
	int result;

	result = yaffs_rd_chunk_tags_nand(dev, chunkInNAND, data, &tags);

	if (tags.ecc_result > YAFFS_ECC_RESULT_NO_ERROR)
		retval = YAFFS_FAIL;

	if (!yaffs_check_ff(data, dev->data_bytes_per_chunk) || tags.chunk_used) {
		pr_err("Chunk %d not erased", chunkInNAND);

		retval = YAFFS_FAIL;
/*add debug by jinling.ke*/
		pr_err("yaffsdebug Scan CheckChunk chunk:%d addr:0x%x chunkUsed:%d page:%d\n",
			chunkInNAND, chunkInNAND * dev->data_bytes_per_chunk, tags.chunk_used, page);

		mtk_dump_byte(&tags, sizeof(struct yaffs_ext_tags), 0);
	}

	yaffs_release_temp_buffer(dev, data, __LINE__);

	return retval;

}
#endif
int yaffs_erase_block(struct yaffs_dev *dev, int flash_block)
{
	int result;

	flash_block -= dev->block_offset;

	dev->n_erasures++;

	result = dev->param.erase_fn(dev, flash_block);

#ifdef YAFFS_MVG_TEST_ERASECHEKFF
	for (c = 0; c < dev->param.chunks_per_block; c++)
		yaffs_ScanCheckEraseClean(dev, (flash_block + dev->block_offset) * dev->param.chunks_per_block + c, c);
#endif
	return result;
}
#if 0
int yaffs_erase_block(struct yaffs_dev *dev, int flash_block)
{
	int result;

	flash_block -= dev->block_offset;

	dev->n_erasures++;

	result = dev->param.erase_fn(dev, flash_block);

	return result;
}
#endif
int yaffs_init_nand(struct yaffs_dev *dev)
{
	if (dev->param.initialise_flash_fn)
		return dev->param.initialise_flash_fn(dev);
	return YAFFS_OK;
}
