#include "mt_ftl.h"
#include "ubi/ubi.h"
#include <linux/crypto.h>
#ifdef PROFILE
#include <linux/time.h>
#endif				/* PROFILE */

#ifdef PROFILE
unsigned long profile_time[MT_FTL_PROFILE_TOTAL_PROFILE_NUM];

unsigned long getnstimenow(void)
{
	struct timespec tv;

	getnstimeofday(&tv);
	return tv.tv_sec * 1000000000 + tv.tv_nsec;
}
#endif				/* PROFILE */

/* The process for PMT should traverse PMT indicator
 * if block num of items in PMT indicator is equal to u4SrcInvalidLeb, copy the
 * corresponding page to new block, and update corresponding PMT indicator */
int mt_ftl_gc_pmt(struct mt_ftl_blk *dev, unsigned int u4SrcLeb,
		unsigned int u4DstLeb, int *offset_dst, bool isReplay)
{
	int ret = MT_FTL_SUCCESS, i = 0;
	int offset_src = 0;
	int page = 0, cache_num = 0;
	int isDirty = 0;

	struct ubi_volume_desc *desc = dev->desc;
	struct mt_ftl_param *param = dev->param;

	if (!isReplay) {
		ret = ubi_is_mapped(desc, u4DstLeb);
		if (ret == 0) {
			mt_ftl_err(dev, "leb%u is unmapped", u4DstLeb);
			ubi_leb_map(desc, u4DstLeb);
		}
	}

	for (i = 0; i < (PMT_TOTAL_CLUSTER_NUM); i++) {
		if (PMT_INDICATOR_GET_BLOCK(param->u4PMTIndicator[i]) == u4SrcLeb) {
			offset_src =
			    PMT_INDICATOR_GET_PAGE(param->u4PMTIndicator[i]) * NAND_PAGE_SIZE;
			if (!isReplay) {
				/* Copy PMT */
				ret = ubi_leb_read(desc, u4SrcLeb,
						(char *)param->gc_page_buffer,
						offset_src, NAND_PAGE_SIZE, 0);
				if (ret && ret != UBI_IO_BITFLIPS) {
					mt_ftl_err(dev,
						   "Copy PMT failed, leb = %u, offset=%d ret = %d",
						   u4SrcLeb, offset_src, ret);
					return MT_FTL_FAIL;
				}
				ret = ubi_leb_write(desc, u4DstLeb, param->gc_page_buffer,
						*offset_dst, NAND_PAGE_SIZE);
				if (ret) {
					mt_ftl_err(dev,
						   "Write PMT failed, leb = %u, offset=0x%x ret = %d",
						   u4DstLeb, (unsigned int)*offset_dst, ret);
					return MT_FTL_FAIL;
				}
				/* Copy Meta PMT */
				ret = ubi_leb_read(desc, u4SrcLeb, (char *)param->gc_page_buffer,
						offset_src + NAND_PAGE_SIZE, NAND_PAGE_SIZE, 0);
				if (ret && ret != UBI_IO_BITFLIPS) {
					mt_ftl_err(dev,
						   "Copy Meta PMT failed, leb = %u, offset=%d ret = %d",
						   u4SrcLeb, offset_src, ret);
					return MT_FTL_FAIL;
				}
				ret = ubi_leb_write(desc, u4DstLeb, param->gc_page_buffer,
						*offset_dst + NAND_PAGE_SIZE, NAND_PAGE_SIZE);
				if (ret) {
					mt_ftl_err(dev,
						   "Write Meta PMT failed, leb = %d, offset=0x%x ret = %d",
						   u4DstLeb, (unsigned int)*offset_dst + NAND_PAGE_SIZE, ret);
					return MT_FTL_FAIL;
				}
			}
			isDirty = PMT_INDICATOR_IS_DIRTY(param->u4PMTIndicator[i]);
			cache_num = PMT_INDICATOR_CACHE_BUF_NUM(param->u4PMTIndicator[i]);
			page = *offset_dst / NAND_PAGE_SIZE;
			PMT_INDICATOR_SET_BLOCKPAGE(param->u4PMTIndicator[i], u4DstLeb, page,
						    isDirty, cache_num);
			/* mt_ftl_err(dev,
				"u4PMTIndicator[%d] = 0x%x, isDirty = %d, cache_num = %d, page = %d\n",
				i, param->u4PMTIndicator[i], isDirty, cache_num, page);	 Temporary */
			*offset_dst += (NAND_PAGE_SIZE * 2);
		}
	}

	return ret;
}

/* The process for data is to get all the sectors in the source block
 * and compare to PMT, if the corresponding block/page/part in PMT is
 * the same as source block/page/part, then current page should be copied
 * destination block, and then update PMT
 */
int mt_ftl_gc_data(struct mt_ftl_blk *dev, unsigned int u4SrcLeb, unsigned int u4DstLeb,
		   int *offset_dst, bool isReplay)
{
	int ret = MT_FTL_SUCCESS, i = 0;
	int offset_src = 0;
	int page_src = 0, page_dst = 0;
	int pmt_block = 0, pmt_page = 0;
	unsigned int pmt = 0, meta_pmt = 0;

	sector_t sector = 0;
	unsigned int cluster = 0, sec_offset = 0;
	sector_t andSec = 0;
	unsigned int data_num = 0;
	unsigned int data_hdr_offset = 0;
	unsigned int page_been_read = 0;
	struct mt_ftl_data_header *header_buffer = NULL;

	int copy_page = 0, cache_num = 0;
	unsigned int invalid_num = 0;

	struct ubi_volume_desc *desc = dev->desc;
	struct mt_ftl_param *param = dev->param;

	const int max_offset_per_block = desc->vol->ubi->leb_size - NAND_PAGE_SIZE;

#ifdef PROFILE
	unsigned long start_time = 0, end_time = 0;

	start_time = getnstimenow();
#endif				/* PROFILE */
	if (!isReplay) {
		ret = ubi_is_mapped(desc, u4DstLeb);
		if (ret == 0) {
			mt_ftl_err(dev, "leb%d is unmapped", u4DstLeb);
			ubi_leb_map(desc, u4DstLeb);
		}
	}
	ret = ubi_is_mapped(desc, u4SrcLeb);
	ubi_assert(ret > 0);
	ubi_assert(u4DstLeb <= NAND_TOTAL_BLOCK_NUM);
	offset_src = 0;
	*offset_dst = 0;
	if (!isReplay) {
		ret = ubi_leb_read(desc, u4SrcLeb, (char *)param->gc_page_buffer, offset_src, NAND_PAGE_SIZE, 0);
	} else {
		/* When replay, the data has been copied to dst, so we read dst instead of src
		 * but we still use offset_src to control flow */
		ret = ubi_leb_read(desc, u4DstLeb, (char *)param->gc_page_buffer, offset_src, NAND_PAGE_SIZE, 0);
	}
	if (ret && ret != UBI_IO_BITFLIPS) {
		mt_ftl_err(dev, "GC data read failed, leb = %u, offset=%d ret = %d",
			   u4SrcLeb, offset_src, ret);
		return MT_FTL_FAIL;
	}

	page_been_read = PAGE_BEEN_READ(param->gc_page_buffer[(NAND_PAGE_SIZE >> 2) - 1]);

	if (isReplay) {
		if (page_been_read == 0) {
			mt_ftl_err(dev, "End of replay GC Data, offset_src = %d", offset_src);
			return ret;
		}
	}

	data_num = PAGE_GET_DATA_NUM(param->gc_page_buffer[(NAND_PAGE_SIZE >> 2) - 1]);
	data_hdr_offset = NAND_PAGE_SIZE - (data_num * sizeof(struct mt_ftl_data_header) + 4);
	if ((data_num * sizeof(struct mt_ftl_data_header)) >= NAND_PAGE_SIZE) {
		mt_ftl_err(dev,
			   "(data_num * sizeof(struct mt_ftl_data_header))(%lx) >= NAND_PAGE_SIZE(%d)",
			   (data_num * sizeof(struct mt_ftl_data_header)), NAND_PAGE_SIZE);
		mt_ftl_err(dev, "data_hdr_offset = %d, data_num = %d", data_hdr_offset, data_num);
	}
	header_buffer = (struct mt_ftl_data_header *)(&param->gc_page_buffer[data_hdr_offset >> 2]);

	/* mt_ftl_err(dev, "data_num = %d, data_hdr_offset = %d, header_buffer = 0x%lx,
	   gc_page_buffer = 0x%lx\n", data_num, data_hdr_offset, (unsigned long int)header_buffer,
	   (unsigned long int)param->gc_page_buffer); */

	andSec = NAND_DEFAULT_VALUE;
	for (i = 0; i < data_num; i++)
		andSec &= header_buffer[i].sector;

	if (andSec == NAND_DEFAULT_VALUE) {
		mt_ftl_err(dev, "all sectors are 0xFFFFFFFF");
		for (i = 0; i < data_num; i++)
			mt_ftl_err(dev, "header_buffer[%d].sector = 0x%lx", i,
				   (unsigned long int)header_buffer[i].sector);
	}
#ifdef PROFILE
	end_time = getnstimenow();
	if (end_time >= start_time)
		profile_time[MT_FTL_PROFILE_GC_DATA_READOOB] += (end_time - start_time) / 1000;
	else {
		mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time, start_time);
		profile_time[MT_FTL_PROFILE_GC_DATA_READOOB] +=
		    (end_time + 0xFFFFFFFF - start_time) / 1000;
	}
#endif				/* PROFILE */

	while ((andSec != NAND_DEFAULT_VALUE) && (offset_src <= max_offset_per_block)) {
		copy_page = 0;

#ifdef PROFILE
		start_time = getnstimenow();
#endif				/* PROFILE */

		/* Check the corresponding block/page of sector in PMT */
		invalid_num = 0;
		for (i = 0; i < data_num; i++) {
			/* Get sector in the page */
			sector = header_buffer[data_num - i - 1].sector;
			/*  NAND_DEFAULT_VALUE ULL*/
			if ((sector & NAND_DEFAULT_VALUE) == NAND_DEFAULT_VALUE)
				continue;

			/* Calculate clusters and sec_offsets */
			cluster =
			    ((unsigned long int)sector / (FS_PAGE_SIZE >> 9)) / PM_PER_NANDPAGE;
			sec_offset =
			    ((unsigned long int)sector / (FS_PAGE_SIZE >> 9)) & (PM_PER_NANDPAGE -
										 1);
			ubi_assert(cluster <= PMT_TOTAL_CLUSTER_NUM);

			/* Download PMT to read PMT cache */
			/* Don't use mt_ftl_updatePMT, that will cause PMT indicator mixed in replay */
			if (PMT_INDICATOR_IS_INCACHE(param->u4PMTIndicator[cluster])) {
				cache_num =
				    PMT_INDICATOR_CACHE_BUF_NUM(param->u4PMTIndicator[cluster]);
				ubi_assert(cache_num < PMT_CACHE_NUM);
				ubi_assert(sec_offset < PM_PER_NANDPAGE);
				pmt = param->u4PMTCache[cache_num * PM_PER_NANDPAGE + sec_offset];
				meta_pmt =
				    param->u4MetaPMTCache[cache_num * PM_PER_NANDPAGE + sec_offset];
			} else if (cluster == param->i4CurrentReadPMTClusterInCache) {
				/* mt_ftl_err(dev, "cluster == i4CurrentReadPMTClusterInCache (%d)",
				   param->i4CurrentReadPMTClusterInCache); */
				ubi_assert(sec_offset < PM_PER_NANDPAGE);
				pmt = param->u4ReadPMTCache[sec_offset];
				meta_pmt = param->u4ReadMetaPMTCache[sec_offset];
			} else {
				pmt_block = PMT_INDICATOR_GET_BLOCK(param->u4PMTIndicator[cluster]);
				pmt_page = PMT_INDICATOR_GET_PAGE(param->u4PMTIndicator[cluster]);

				if (unlikely(pmt_block == 0)) {
					mt_ftl_err(dev, "pmt_block == 0");
					memset(param->u4ReadPMTCache, 0xFF,
					       PM_PER_NANDPAGE * sizeof(unsigned int));
					memset(param->u4ReadMetaPMTCache, 0xFF,
					       PM_PER_NANDPAGE * sizeof(unsigned int));
					param->i4CurrentReadPMTClusterInCache = 0xFFFFFFFF;
				} else {
					mt_ftl_err(dev, "Get PMT of cluster (%d)", cluster);
					ret = ubi_leb_read(desc, pmt_block, (char *)param->u4ReadPMTCache,
						pmt_page * NAND_PAGE_SIZE, NAND_PAGE_SIZE, 0);
					if (ret && ret != UBI_IO_BITFLIPS) {
						mt_ftl_err(dev,
							   "u4CurrentPMTLebPageIndicator = 0x%x",
							   param->u4CurrentPMTLebPageIndicator);
						mt_ftl_err(dev, "u4PMTIndicator[%d] = 0x%x",
							   cluster, param->u4PMTIndicator[cluster]);
						mt_ftl_err(dev,
							   "[GET PMT] ubi_leb_read PMT failed, leb%d, offset=%d, ret = 0x%x\n",
							   pmt_block, pmt_page * NAND_PAGE_SIZE,
							   ret);
						return MT_FTL_FAIL;
					}
					ret = ubi_leb_read(desc, pmt_block, (char *)param->u4ReadMetaPMTCache,
							(pmt_page + 1) * NAND_PAGE_SIZE, NAND_PAGE_SIZE, 0);
					if (ret && ret != UBI_IO_BITFLIPS) {
						mt_ftl_err(dev,
							   "u4CurrentPMTLebPageIndicator = 0x%x",
							   param->u4CurrentPMTLebPageIndicator);
						mt_ftl_err(dev, "u4PMTIndicator[%d] = 0x%x",
							   cluster, param->u4PMTIndicator[cluster]);
						mt_ftl_err(dev,
							   "[GET PMT] ubi_leb_read Meta PMT failed, leb%d, offset=%d, ret = 0x%x\n",
							   pmt_block,
							   (pmt_page + 1) * NAND_PAGE_SIZE, ret);
						return MT_FTL_FAIL;
					}
					param->i4CurrentReadPMTClusterInCache = cluster;
				}
				ubi_assert(sec_offset <= PM_PER_NANDPAGE);
				pmt = param->u4ReadPMTCache[sec_offset];
				meta_pmt = param->u4ReadMetaPMTCache[sec_offset];
			}

			/* mt_ftl_updatePMT(dev, cluster, sec_offset, MT_INVALID_BLOCKPAGE, 0, i, isReplay); */
			/* Look up PMT in cache and get data from NAND flash */
			/* cache_num = PMT_INDICATOR_CACHE_BUF_NUM(param->u4PMTIndicator[cluster]); */
			/* pmt = param->u4PMTCache[cache_num * PM_PER_NANDPAGE + sec_offset]; */
			if (pmt == MT_INVALID_BLOCKPAGE) {
				mt_ftl_err(dev, "PMT of sector(0x%lx) is invalid",
					   (unsigned long int)sector);
				return MT_FTL_FAIL;
			}
			if (!isReplay) {
				if ((u4SrcLeb == PMT_GET_BLOCK(pmt)) &&
				    (page_src == PMT_GET_PAGE(pmt)) &&
				    (i == PMT_GET_PART(meta_pmt))) {
					/* Update PMT */
					mt_ftl_updatePMT(dev, cluster, sec_offset, u4DstLeb,
							 page_dst * NAND_PAGE_SIZE, i,
							 (header_buffer[data_num - i - 1].offset_len
							  & 0xFFFF), isReplay, 0);
					/* mt_ftl_err(dev, "header_buffer[%d].sector = 0x%x,
					   header_buffer[%d].offset_le= 0x%x",
					   data_num - i - 1, header_buffer[data_num - i - 1].sector,
					   data_num - i - 1, header_buffer[data_num - i - 1].offset_len); */
					copy_page++;
				} else {
					header_buffer[data_num - i - 1].sector = NAND_DEFAULT_VALUE;
					invalid_num +=
					    (header_buffer[data_num - i - 1].offset_len & 0xFFFF);
				}
			} else
				mt_ftl_updatePMT(dev, cluster, sec_offset, u4DstLeb,
						 page_dst * NAND_PAGE_SIZE, i,
						 (header_buffer[data_num - i - 1].offset_len &
						  0xFFFF), isReplay, 0);
		}

#ifdef PROFILE
		end_time = getnstimenow();
		if (end_time >= start_time)
			profile_time[MT_FTL_PROFILE_GC_DATA_READ_UPDATE_PMT] +=
			    (end_time - start_time) / 1000;
		else {
			mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time, start_time);
			profile_time[MT_FTL_PROFILE_GC_DATA_READ_UPDATE_PMT] +=
			    (end_time + 0xFFFFFFFF - start_time) / 1000;
		}

		start_time = getnstimenow();
#endif				/* PROFILE */
		if ((!isReplay) && copy_page) {
			if ((data_hdr_offset + data_num * sizeof(struct mt_ftl_data_header)) >=
			    NAND_PAGE_SIZE) {
				mt_ftl_err(dev,
					   "(data_hdr_offset + data_num * sizeof(struct mt_ftl_data_header))(0x%lx) >= NAND_PAGE_SIZE(%d)",
					   (data_hdr_offset +
					    data_num * sizeof(struct mt_ftl_data_header)),
					   NAND_PAGE_SIZE);
				mt_ftl_err(dev, "data_hdr_offset = %d, data_num = %d",
					   data_hdr_offset, data_num);
			}
			memcpy(&param->gc_page_buffer[data_hdr_offset >> 2], header_buffer,
			       data_num * sizeof(struct mt_ftl_data_header));
			memcpy(&param->gc_page_buffer[(NAND_PAGE_SIZE - 4) >> 2], &data_num, 4);
			PAGE_SET_READ(param->gc_page_buffer[(NAND_PAGE_SIZE >> 2) - 1]);
			ret = ubi_leb_write(desc, u4DstLeb, param->gc_page_buffer, *offset_dst, NAND_PAGE_SIZE);
			if (ret) {
				mt_ftl_err(dev,
					   "ubi_leb_write failed, leb = %d, offset = %d ret = 0x%x\n",
					   u4DstLeb, *offset_dst, ret);
				return MT_FTL_FAIL;
			}

			BIT_UPDATE(param->u4BIT[u4DstLeb], invalid_num);

			*offset_dst += NAND_PAGE_SIZE;
			page_dst++;
		} else if (isReplay) {
			*offset_dst += NAND_PAGE_SIZE;
			page_dst++;
		}
#ifdef PROFILE
		end_time = getnstimenow();
		if (end_time >= start_time)
			profile_time[MT_FTL_PROFILE_GC_DATA_WRITEOOB] +=
			    (end_time - start_time) / 1000;
		else {
			mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time, start_time);
			profile_time[MT_FTL_PROFILE_GC_DATA_WRITEOOB] +=
			    (end_time + 0xFFFFFFFF - start_time) / 1000;
		}

		start_time = getnstimenow();
#endif				/* PROFILE */

		offset_src += NAND_PAGE_SIZE;
		page_src++;
		if (offset_src > max_offset_per_block)
			break;

		if (!isReplay)
			ret = ubi_leb_read(desc, u4SrcLeb, (char *)param->gc_page_buffer,
					offset_src, NAND_PAGE_SIZE, 0);
		else
			ret = ubi_leb_read(desc, u4DstLeb, (char *)param->gc_page_buffer,
					offset_src, NAND_PAGE_SIZE, 0);

		if (ret && ret != UBI_IO_BITFLIPS) {
			mt_ftl_err(dev,
				   "ubi_leb_read failed, leb = %d, offset=%d ret = 0x%x",
				   u4SrcLeb, offset_src, ret);
			return MT_FTL_FAIL;
		}

		page_been_read = PAGE_BEEN_READ(param->gc_page_buffer[(NAND_PAGE_SIZE >> 2) - 1]);

		data_num = PAGE_GET_DATA_NUM(param->gc_page_buffer[(NAND_PAGE_SIZE >> 2) - 1]);
		data_hdr_offset =
		    NAND_PAGE_SIZE - (data_num * sizeof(struct mt_ftl_data_header) + 4);
		if ((data_num * sizeof(struct mt_ftl_data_header)) >= NAND_PAGE_SIZE) {
			mt_ftl_err(dev,
				   "(data_num * sizeof(struct mt_ftl_data_header))(0x%lx) >= NAND_PAGE_SIZE(%d)",
				   (data_num * sizeof(struct mt_ftl_data_header)), NAND_PAGE_SIZE);
			mt_ftl_err(dev, "data_hdr_offset = %d, data_num = %d", data_hdr_offset,
				   data_num);
		}
		header_buffer =
		    (struct mt_ftl_data_header *)(&param->gc_page_buffer[data_hdr_offset >> 2]);

		andSec = NAND_DEFAULT_VALUE;
		for (i = 0; i < data_num; i++)
			andSec &= header_buffer[i].sector;

		if (andSec == NAND_DEFAULT_VALUE)
			mt_ftl_err(dev, "all sectors are 0xFFFFFFFF");	/* Temporary */

		if (isReplay) {
			if (page_been_read == 0) {
				mt_ftl_err(dev, "End of replay GC Data, offset_src = %d",
					   offset_src);
				return ret;
			}
		}
#ifdef PROFILE
		end_time = getnstimenow();
		if (end_time >= start_time) {
			profile_time[MT_FTL_PROFILE_GC_DATA_READOOB] +=
			    (end_time - start_time) / 1000;
			/* mt_ftl_err(dev, "read_oob time = %lu us", (end_time - start_time) / 1000); */
		} else {
			mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time, start_time);
			profile_time[MT_FTL_PROFILE_GC_DATA_READOOB] +=
			    (end_time + 0xFFFFFFFF - start_time) / 1000;
			/* mt_ftl_err(dev, "read_oob time = %lu us", (end_time + 0xFFFFFFFF - start_time) / 1000); */
		}
#endif				/* PROFILE */
	}

	/* mt_ftl_err(dev, "u4BIT[%d] = %d", u4DstLeb, param->u4BIT[u4DstLeb]);	 Temporary */

	return ret;
}

unsigned int mt_ftl_gc_findblock(struct mt_ftl_blk *dev, unsigned int u4StartLeb,
				 unsigned int u4EndLeb, bool *allInvalid)
{
	unsigned int u4MaxInvalidVolume = 0;
	unsigned int u4ReturnLeb = MT_INVALID_BLOCKPAGE;
	unsigned int i = 0;

	struct ubi_volume_desc *desc = dev->desc;
	struct mt_ftl_param *param = dev->param;

	if (u4StartLeb >= NAND_TOTAL_BLOCK_NUM || u4EndLeb >= NAND_TOTAL_BLOCK_NUM)
		mt_ftl_err(dev,
			   "u4StartLeb(%d)u4EndLeb(%d) is larger than NAND_TOTAL_BLOCK_NUM(%d)",
			   u4StartLeb, u4EndLeb, NAND_TOTAL_BLOCK_NUM);
	ubi_assert((u4StartLeb < NAND_TOTAL_BLOCK_NUM || u4EndLeb < NAND_TOTAL_BLOCK_NUM));

	for (i = u4StartLeb; i < u4EndLeb; i++) {
		if (param->u4BIT[i] == desc->vol->ubi->leb_size) {
			u4ReturnLeb = i;
			*allInvalid = true;
			mt_ftl_err(dev, "u4BIT[%d] = %d", u4ReturnLeb, param->u4BIT[u4ReturnLeb]);	/* Temporary */
			return u4ReturnLeb;
		}

		if (param->u4BIT[i] > u4MaxInvalidVolume) {
			u4MaxInvalidVolume = param->u4BIT[i];
			u4ReturnLeb = i;
		}
	}

	*allInvalid = false;

	return u4ReturnLeb;
}

int mt_ftl_gc(struct mt_ftl_blk *dev, int *updated_page, bool isPMT, bool isReplay, int *isCommit)
{
	int ret = MT_FTL_SUCCESS, i = 0;
	unsigned int u4SrcInvalidLeb = MT_INVALID_BLOCKPAGE;
	unsigned int u4ReturnLeb = 0;
	int offset_dst = 0;
	bool allInvalid = false;

	struct ubi_volume_desc *desc = dev->desc;
	struct mt_ftl_param *param = dev->param;

	const int page_num_per_block = desc->vol->ubi->leb_size / NAND_PAGE_SIZE;

#ifdef PROFILE
	unsigned long start_time = 0, end_time = 0;

	start_time = getnstimenow();
#endif				/* PROFILE */
	/* Get the most invalid block */
	/* TODO: Separate PMT block to other volume, otherwise, there is only 1 PMT block and afford at most 4G data */
	if (unlikely(isPMT)) {
		/* u4SrcInvalidLeb = PMT_LEB_PAGE_INDICATOR_GET_BLOCK(param->u4CurrentPMTLebPageIndicator); */
		u4SrcInvalidLeb =
		    mt_ftl_gc_findblock(dev, PMT_START_BLOCK, DATA_START_BLOCK - 1, &allInvalid);
		if (allInvalid)
			goto gc_end;
	} else {
		/* Leb 0/1 for storing backup dram data, leb2 for replay */
		u4SrcInvalidLeb = mt_ftl_gc_findblock(dev, DATA_START_BLOCK,
						      desc->vol->ubi->volumes[dev->vol_id]->reserved_pebs,
						      &allInvalid);
		if (allInvalid)
			goto gc_end;
	}

	if (u4SrcInvalidLeb == MT_INVALID_BLOCKPAGE) {
		mt_ftl_err(dev, "cannot find block for GC, isPMT = %d", isPMT);
		return MT_FTL_FAIL;
	}

	/* mt_ftl_err(dev, "u4BIT[%d] = %d, u4GCReservePMTLeb = 0x%x, u4GCReserveLeb = 0x%x",
			u4SrcInvalidLeb, param->u4BIT[u4SrcInvalidLeb],
			param->u4GCReservePMTLeb, param->u4GCReserveLeb);	 Temporary */

#ifdef PROFILE
	end_time = getnstimenow();
	if (end_time >= start_time)
		profile_time[MT_FTL_PROFILE_GC_FINDBLK] += (end_time - start_time) / 1000;
	else {
		mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time, start_time);
		profile_time[MT_FTL_PROFILE_GC_FINDBLK] +=
		    (end_time + 0xFFFFFFFF - start_time) / 1000;
	}
	start_time = getnstimenow();
#endif				/* PROFILE */

	/* Call sub function for pmt/data */
	if (unlikely(isPMT))
		ret =
		    mt_ftl_gc_pmt(dev, u4SrcInvalidLeb, param->u4GCReservePMTLeb, &offset_dst,
				  isReplay);
	else
		ret =
		    mt_ftl_gc_data(dev, u4SrcInvalidLeb, param->u4GCReserveLeb, &offset_dst,
				   isReplay);

	if (ret) {
		mt_ftl_err(dev,
			   "GC sub function failed, u4SrcInvalidLeb = %d, offset_dst=%d, ret = 0x%x",
			   u4SrcInvalidLeb, offset_dst, ret);
		return MT_FTL_FAIL;
	}
#ifdef PROFILE
	end_time = getnstimenow();
	if (end_time >= start_time)
		profile_time[MT_FTL_PROFILE_GC_CPVALID] += (end_time - start_time) / 1000;
	else {
		mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time, start_time);
		profile_time[MT_FTL_PROFILE_GC_CPVALID] +=
		    (end_time + 0xFFFFFFFF - start_time) / 1000;
	}
#endif				/* PROFILE */
gc_end:
#ifdef PROFILE
	start_time = getnstimenow();
#endif				/* PROFILE */

	if (!isReplay) {
		if (!isPMT) {
			if (NAND_PAGE_NUM_PER_BLOCK <= param->replay_blk_index) {
				mt_ftl_err(dev,
					   "NAND_PAGE_NUM_PER_BLOCK(%d) <= param->replay_blk_index(%d)",
					   NAND_PAGE_NUM_PER_BLOCK, param->replay_blk_index);
			}
			for (i = 0; i < param->replay_blk_index; i++) {
				if (u4SrcInvalidLeb == param->replay_blk_rec[i]) {
					mt_ftl_err(dev,
						   "u4SrcInvalidLeb (%d) == replay_blk_rec[%d] (%d)",
						   u4SrcInvalidLeb, i, param->replay_blk_rec[i]);
					*isCommit = 1;
					break;
				}
			}
		}

		ubi_leb_unmap(desc, u4SrcInvalidLeb);
		mt_ftl_err(dev, "leb%d has been unmapped, desc->vol->eba_tbl[%d] = %d",
			   u4SrcInvalidLeb, u4SrcInvalidLeb, desc->vol->eba_tbl[u4SrcInvalidLeb]);
		ubi_leb_map(desc, u4SrcInvalidLeb);
		mt_ftl_err(dev, "leb%d has been mapped, desc->vol->eba_tbl[%d] = %d",
			   u4SrcInvalidLeb, u4SrcInvalidLeb, desc->vol->eba_tbl[u4SrcInvalidLeb]);
	}

	/* TODO: Use this information for replay, instead of check MT_PAGE_HAD_BEEN_READ */
	*updated_page = offset_dst / NAND_PAGE_SIZE;
	if (*updated_page == page_num_per_block) {
		mt_ftl_err(dev, "There is no more free pages in the gathered block");
		mt_ftl_err(dev, "desc->vol->ubi->volumes[%d]->reserved_pebs = %d",
			   dev->vol_id, desc->vol->ubi->volumes[dev->vol_id]->reserved_pebs);
		for (i = PMT_START_BLOCK; i < desc->vol->ubi->volumes[dev->vol_id]->reserved_pebs;
		     i += 8) {
			mt_ftl_err(dev, "%d\t %d\t %d\t %d\t %d\t %d\t %d\t %d", param->u4BIT[i],
				   param->u4BIT[i + 1], param->u4BIT[i + 2], param->u4BIT[i + 3],
				   param->u4BIT[i + 4], param->u4BIT[i + 5], param->u4BIT[i + 6],
				   param->u4BIT[i + 7]);
		}
		return MT_FTL_FAIL;
	}

	if (unlikely(isPMT)) {
		u4ReturnLeb = param->u4GCReservePMTLeb;
		param->u4GCReservePMTLeb = u4SrcInvalidLeb;
		mt_ftl_err(dev, "u4GCReservePMTLeb = %d, *updated_page = %d, u4GCReserveLeb = %d",
				param->u4GCReservePMTLeb, *updated_page, param->u4GCReserveLeb);
		/* Temporary */
		if (param->u4GCReservePMTLeb >= NAND_TOTAL_BLOCK_NUM)
			mt_ftl_err(dev,
				   "param->u4GCReservePMTLeb(%d) is larger than NAND_TOTAL_BLOCK_NUM(%d)",
				   param->u4GCReservePMTLeb, NAND_TOTAL_BLOCK_NUM);
		param->u4BIT[param->u4GCReservePMTLeb] = 0;
		mt_ftl_err(dev, "u4BIT[%d] = %d", param->u4GCReservePMTLeb,
			   param->u4BIT[param->u4GCReservePMTLeb]);
	} else {
		u4ReturnLeb = param->u4GCReserveLeb;
		param->u4GCReserveLeb = u4SrcInvalidLeb;
		mt_ftl_err(dev, "u4GCReserveLeb = %d, *updated_page = %d", param->u4GCReserveLeb,
			   *updated_page);
		if (param->u4GCReserveLeb >= NAND_TOTAL_BLOCK_NUM)
			mt_ftl_err(dev,
				   "param->u4GCReserveLeb(%d) is larger than NAND_TOTAL_BLOCK_NUM(%d)",
				   param->u4GCReserveLeb, NAND_TOTAL_BLOCK_NUM);
		param->u4BIT[param->u4GCReserveLeb] = 0;
		mt_ftl_err(dev, "u4BIT[%d] = %d", param->u4GCReserveLeb,
			   param->u4BIT[param->u4GCReserveLeb]);
	}

#ifdef PROFILE
	end_time = getnstimenow();
	if (end_time >= start_time)
		profile_time[MT_FTL_PROFILE_GC_REMAP] += (end_time - start_time) / 1000;
	else {
		mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time, start_time);
		profile_time[MT_FTL_PROFILE_GC_REMAP] +=
		    (end_time + 0xFFFFFFFF - start_time) / 1000;
	}
#endif				/* PROFILE */

	return u4ReturnLeb;
}

int mt_ftl_leb_lastpage_offset(struct mt_ftl_blk *dev, int leb)
{
	int ret = MT_FTL_SUCCESS;
	int offset = 0;
	struct ubi_volume_desc *desc = dev->desc;
	struct mt_ftl_param *param = dev->param;
	const int max_offset_per_block = desc->vol->ubi->leb_size - NAND_PAGE_SIZE;

	while (offset <= max_offset_per_block) {
		ret = ubi_leb_read(desc, leb, (char *)param->tmp_page_buffer, offset, NAND_PAGE_SIZE, 0);
		if (ret && ret != UBI_IO_BITFLIPS) {
			mt_ftl_err(dev, "ubi_leb_read (leb%d, offset=%d) failed, ret = 0x%x",
				   leb, offset, ret);
			return MT_FTL_FAIL;
		}

		if (param->tmp_page_buffer[0] == 0xFFFFFFFF) {
			/* mt_ftl_err(dev, "[INFO] Get last page in leb:%d, page:%d", leb, offset / NAND_PAGE_SIZE); */
			break;
		}

		offset += NAND_PAGE_SIZE;
	}

	return offset;
}

/* TODO: Add new block to replay block, but have to consider the impact of original valid page to replay */
int mt_ftl_getfreeblock(struct mt_ftl_blk *dev, int *updated_page, bool isPMT, bool isReplay)
{
	int ret = MT_FTL_SUCCESS;
	unsigned int u4FreeLeb = 0;
	/*int offset = 0;*/
	int isCommit = 0;
	struct ubi_volume_desc *desc = dev->desc;
	struct mt_ftl_param *param = dev->param;
	const int max_offset_per_block = desc->vol->ubi->leb_size - NAND_PAGE_SIZE;

#ifdef PROFILE
	unsigned long start_time = 0, end_time = 0;

	start_time = getnstimenow();
#endif				/* PROFILE */
	if (isPMT) {
		if (param->u4NextFreePMTLebIndicator != MT_INVALID_BLOCKPAGE) {
			u4FreeLeb = param->u4NextFreePMTLebIndicator;
			if (!isReplay) {
				ret = ubi_is_mapped(desc, u4FreeLeb);
				if (ret == 0)
					ubi_leb_map(desc, u4FreeLeb);
				mt_ftl_err(dev, "leb%d is %s", u4FreeLeb,
					   ubi_is_mapped(desc, u4FreeLeb) ? "mapped" : "unmapped");
			}
			*updated_page = 0;
			param->u4NextFreePMTLebIndicator++;
			mt_ftl_err(dev, "u4NextFreePMTLebIndicator = %d",
				   param->u4NextFreePMTLebIndicator);
			/* The PMT_BLOCK_NUM + 2 block is reserved to param->u4GCReserveLeb */
			if (param->u4NextFreePMTLebIndicator >= DATA_START_BLOCK - 1) {
				/* mt_ftl_err(dev, "[INFO] u4NextFreePMTLebIndicator is in the end"); */
				param->u4NextFreePMTLebIndicator = MT_INVALID_BLOCKPAGE;
				mt_ftl_err(dev, "u4NextFreePMTLebIndicator = %d",
					   param->u4NextFreePMTLebIndicator);
			}
		} else
			u4FreeLeb = mt_ftl_gc(dev, updated_page, isPMT, isReplay, &isCommit);

#ifdef PROFILE
		end_time = getnstimenow();
		if (end_time >= start_time)
			profile_time[MT_FTL_PROFILE_GETFREEBLOCK_GETLEB] +=
			    (end_time - start_time) / 1000;
		else {
			mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time, start_time);
			profile_time[MT_FTL_PROFILE_GETFREEBLOCK_GETLEB] +=
			    (end_time + 0xFFFFFFFF - start_time) / 1000;
		}
#endif				/* PROFILE */

		return u4FreeLeb;
	}

	if (param->u4NextFreeLebIndicator != MT_INVALID_BLOCKPAGE) {
		u4FreeLeb = param->u4NextFreeLebIndicator;
		*updated_page = 0;
		if (!isReplay) {
			ret = ubi_is_mapped(desc, u4FreeLeb);
			if (ret == 0)
				ubi_leb_map(desc, u4FreeLeb);
		}
		param->u4NextFreeLebIndicator++;
		mt_ftl_err(dev, "u4NextFreeLebIndicator = %d", param->u4NextFreeLebIndicator);
		/* The last block is reserved to param->u4GCReserveLeb */
		if (param->u4NextFreeLebIndicator >=
		    (desc->vol->ubi->volumes[0]->reserved_pebs - 1)) {
			/* TODO: volume number need to change */
			/* mt_ftl_err(dev, "[INFO] u4NextFreeLebIndicator is in the end"); */
			param->u4NextFreeLebIndicator = MT_INVALID_BLOCKPAGE;
			mt_ftl_err(dev, "u4NextFreeLebIndicator = %d",
				   param->u4NextFreeLebIndicator);
		}
	} else
		u4FreeLeb = mt_ftl_gc(dev, updated_page, isPMT, isReplay, &isCommit);

#ifdef PROFILE
	end_time = getnstimenow();
	if (end_time >= start_time)
		profile_time[MT_FTL_PROFILE_GETFREEBLOCK_GETLEB] += (end_time - start_time) / 1000;
	else {
		mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time, start_time);
		profile_time[MT_FTL_PROFILE_GETFREEBLOCK_GETLEB] +=
		    (end_time + 0xFFFFFFFF - start_time) / 1000;
	}
	start_time = getnstimenow();
#endif				/* PROFILE */

	if (!isReplay) {
		/* memset(param->replay_page_buffer, 0, (NAND_PAGE_SIZE >> 2) * sizeof(unsigned int)); */
		param->replay_page_buffer[0] = MT_MAGIC_NUMBER;
		param->replay_page_buffer[1] = u4FreeLeb;
		/*
		   offset = mt_ftl_leb_lastpage_offset(dev, 2);
		   //if ((offset > max_offset_per_block) || isCommit) {
		 */
		if ((param->u4NextReplayOffsetIndicator > max_offset_per_block) || isCommit) {
			PMT_LEB_PAGE_INDICATOR_SET_BLOCKPAGE(param->u4NextLebPageIndicator,
							     u4FreeLeb, *updated_page);
			mt_ftl_err(dev, "u4NextLebPageIndicator = 0x%x",
				   param->u4NextLebPageIndicator);
			/*
			   //mt_ftl_commitPMT(dev, isReplay, false);
			   //mt_ftl_commit_indicators(dev);
			 */
			mt_ftl_commit(dev);
			*updated_page += 1;
			/* offset = 0; */
		}
		mt_ftl_err(dev, "u4NextReplayOffsetIndicator = %d",
			   param->u4NextReplayOffsetIndicator);
		ret = ubi_leb_write(desc, REPLAY_BLOCK, param->replay_page_buffer, param->u4NextReplayOffsetIndicator,
				 NAND_PAGE_SIZE);
		if (ret) {
			/*mt_ftl_err(dev,
				   "ubi_io_write failed, leb = %d, pnum = %d, offset=%d ret = 0x%x",
				   REPLAY_BLOCK, pnum, offset, ret);*/
			return MT_FTL_FAIL;
		}
		param->u4NextReplayOffsetIndicator += NAND_PAGE_SIZE;
	}
	if (NAND_PAGE_NUM_PER_BLOCK <= param->replay_blk_index) {
		mt_ftl_err(dev, "NAND_PAGE_NUM_PER_BLOCK(%d) <= param->replay_blk_index(%d)",
			   NAND_PAGE_NUM_PER_BLOCK, param->replay_blk_index);
		mt_ftl_commit_indicators(dev);
		/* param->replay_blk_index = 0;  commit process: mt_ftl_commit_indicators*/
	}
	param->replay_blk_rec[param->replay_blk_index] = u4FreeLeb;
	param->replay_blk_index++;

	/* mt_ftl_err(dev,
		   "u4FreeLeb = %d, u4NextFreeLebIndicator = %d, isPMT = %d, updated_page = %d",
		   u4FreeLeb, param->u4NextFreeLebIndicator, isPMT, *updated_page); */

#ifdef PROFILE
	end_time = getnstimenow();
	if (end_time >= start_time)
		profile_time[MT_FTL_PROFILE_GETFREEBLOCK_PUTREPLAY_COMMIT] +=
		    (end_time - start_time) / 1000;
	else {
		mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time, start_time);
		profile_time[MT_FTL_PROFILE_GETFREEBLOCK_PUTREPLAY_COMMIT] +=
		    (end_time + 0xFFFFFFFF - start_time) / 1000;
	}
#endif				/* PROFILE */

	return u4FreeLeb;
}

/* sync *page_buffer to the end of dst_leb */
int mt_ftl_write_to_blk(struct mt_ftl_blk *dev, int dst_leb, unsigned int *page_buffer)
{
	int ret = MT_FTL_SUCCESS;
	int offset = 0;
	struct ubi_volume_desc *desc = dev->desc;
	/* struct mt_ftl_param *param = dev->param; */
	const int max_offset_per_block = desc->vol->ubi->leb_size - NAND_PAGE_SIZE;

	if (!ubi_is_mapped(desc, dst_leb))
		ubi_leb_map(desc, dst_leb);

	offset = mt_ftl_leb_lastpage_offset(dev, dst_leb);
	if (offset > max_offset_per_block) {
		ubi_leb_unmap(desc, dst_leb);
		mt_ftl_err(dev, "leb%d has been unmapped, desc->vol->eba_tbl[%d] = %d", dst_leb,
			   dst_leb, desc->vol->eba_tbl[dst_leb]);
		ubi_leb_map(desc, dst_leb);
		mt_ftl_err(dev, "leb%d has been mapped, desc->vol->eba_tbl[%d] = %d", dst_leb,
			   dst_leb, desc->vol->eba_tbl[dst_leb]);
		offset = 0;
	}

	/* mt_ftl_err(dev, "dst_leb = %d, offset = %d, page_buffer[0] = 0x%x", dst_leb, offset,
		   page_buffer[0]); */
	ret = ubi_leb_write(desc, dst_leb, page_buffer, offset, NAND_PAGE_SIZE);
	if (ret && ret != UBI_IO_BITFLIPS) {
		mt_ftl_err(dev, "ubi_leb_write (leb%d, offset=%d) failed, ret = 0x%x",
			   dst_leb, offset, ret);
		return MT_FTL_FAIL;
	}

	return ret;
}

int mt_ftl_write_page(struct mt_ftl_blk *dev)
{
	int ret = MT_FTL_SUCCESS, i = 0;
	int leb = 0, page = 0, cache_num = 0;
	sector_t sector = 0;
	unsigned int cluster = 0, sec_offset = 0;
	struct ubi_volume_desc *desc = dev->desc;
	struct mt_ftl_param *param = dev->param;
	const int page_num_per_block = desc->vol->ubi->leb_size / NAND_PAGE_SIZE;
	unsigned int data_hdr_offset = 0;

#ifdef PROFILE
	unsigned long start_time = 0, end_time = 0;

	start_time = getnstimenow();
#endif				/* PROFILE */
	leb = PMT_LEB_PAGE_INDICATOR_GET_BLOCK(param->u4NextLebPageIndicator);
	page = PMT_LEB_PAGE_INDICATOR_GET_PAGE(param->u4NextLebPageIndicator);

	/* mt_ftl_err(dev, "u4NextLebPageIndicator = 0x%x, leb = %d, page = %d",
	   param->u4NextLebPageIndicator, leb, page); */
	data_hdr_offset =
	    NAND_PAGE_SIZE - (param->u4DataNum * sizeof(struct mt_ftl_data_header) + 4);
	if ((data_hdr_offset + param->u4DataNum * sizeof(struct mt_ftl_data_header)) >=
	    NAND_PAGE_SIZE) {
		mt_ftl_err(dev,
			   "(data_hdr_offset + param->u4DataNum * sizeof(struct mt_ftl_data_header))(0x%lx) >= NAND_PAGE_SIZE(%d)"
			   , (data_hdr_offset + param->u4DataNum * sizeof(struct mt_ftl_data_header)), NAND_PAGE_SIZE);
		mt_ftl_err(dev, "data_hdr_offset = %d, param->u4DataNum = %d", data_hdr_offset,
			   param->u4DataNum);
	}
	if (MTKFTL_MAX_DATA_NUM_PER_PAGE < param->u4DataNum) {
		mt_ftl_err(dev, "MTKFTL_MAX_DATA_NUM_PER_PAGE(%d) < param->u4DataNum(%d)",
			   MTKFTL_MAX_DATA_NUM_PER_PAGE, param->u4DataNum);
	}
	memcpy(&param->u1DataCache[data_hdr_offset],
	       &param->u4Header[MTKFTL_MAX_DATA_NUM_PER_PAGE - param->u4DataNum],
	       param->u4DataNum * sizeof(struct mt_ftl_data_header));
	memcpy(&param->u1DataCache[NAND_PAGE_SIZE - 4], &param->u4DataNum, 4);
	ret = ubi_leb_write(desc, leb, param->u1DataCache, page * NAND_PAGE_SIZE, NAND_PAGE_SIZE);
	if (ret) {
		mt_ftl_err(dev, "ubi_leb_write failed, leb = %d, offset=%d ret = 0x%x",
			   leb, page * NAND_PAGE_SIZE, ret);
		return MT_FTL_FAIL;
	}
#ifdef PROFILE
	end_time = getnstimenow();
	if (end_time >= start_time)
		profile_time[MT_FTL_PROFILE_WRITE_PAGE_WRITEOOB] += (end_time - start_time) / 1000;
	else {
		mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time, start_time);
		profile_time[MT_FTL_PROFILE_WRITE_PAGE_WRITEOOB] +=
		    (end_time + 0xFFFFFFFF - start_time) / 1000;
	}
	start_time = getnstimenow();
#endif				/* PROFILE */

	for (i = MTKFTL_MAX_DATA_NUM_PER_PAGE - 1;
	     i >= MTKFTL_MAX_DATA_NUM_PER_PAGE - param->u4DataNum; i--) {
		sector = param->u4Header[i].sector;
		cluster = ((unsigned long int)sector / (FS_PAGE_SIZE >> 9)) / PM_PER_NANDPAGE;
		sec_offset =
		    ((unsigned long int)sector / (FS_PAGE_SIZE >> 9)) & (PM_PER_NANDPAGE - 1);
		ubi_assert(cluster <= PMT_TOTAL_CLUSTER_NUM);
		if (PMT_INDICATOR_IS_INCACHE(param->u4PMTIndicator[cluster])) {
			cache_num = PMT_INDICATOR_CACHE_BUF_NUM(param->u4PMTIndicator[cluster]);
			ubi_assert(cache_num < PMT_CACHE_NUM);
			ubi_assert(sec_offset < PM_PER_NANDPAGE);
			PMT_RESET_DATA_INCACHE(param->u4MetaPMTCache[cache_num * PM_PER_NANDPAGE +
								     sec_offset]);
		} else {
			mt_ftl_err(dev,
				   "u4PMTIndicator[%d](0x%x) doesn't in cache, sector = 0x%lx, i = %d",
				   cluster, param->u4PMTIndicator[cluster],
				   (unsigned long int)sector, i);
			/* return MT_FTL_FAIL; */
		}
	}

	page++;
	if (page == page_num_per_block) {
		leb = mt_ftl_getfreeblock(dev, &page, false, false);
		if (leb == MT_INVALID_BLOCKPAGE) {
			mt_ftl_err(dev, "mt_ftl_getfreeblock failed");
			return MT_FTL_FAIL;
		}
	}
	PMT_LEB_PAGE_INDICATOR_SET_BLOCKPAGE(param->u4NextLebPageIndicator, leb, page);
	/*mt_ftl_err(dev, "u4NextLebPageIndicator = 0x%x", param->u4NextLebPageIndicator);*/

#ifdef PROFILE
	end_time = getnstimenow();
	if (end_time >= start_time)
		profile_time[MT_FTL_PROFILE_WRITE_PAGE_GETFREEBLK] +=
		    (end_time - start_time) / 1000;
	else {
		mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time, start_time);
		profile_time[MT_FTL_PROFILE_WRITE_PAGE_GETFREEBLK] +=
		    (end_time + 0xFFFFFFFF - start_time) / 1000;
	}
	/* mt_ftl_err(dev, "write_page time = %lu", (end_time - start_time) / 1000); */
	start_time = getnstimenow();
#endif				/* PROFILE */

	memset(param->u1DataCache, 0xFF, NAND_PAGE_SIZE * sizeof(unsigned char));
	memset(param->u4Header, 0xFF,
	       MTKFTL_MAX_DATA_NUM_PER_PAGE * sizeof(struct mt_ftl_data_header));
	param->u4DataNum = 0;
	/* u4DataCacheIndex = 0; */

#ifdef PROFILE
	end_time = getnstimenow();
	if (end_time >= start_time)
		profile_time[MT_FTL_PROFILE_WRITE_PAGE_RESET] += (end_time - start_time) / 1000;
	else {
		mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time, start_time);
		profile_time[MT_FTL_PROFILE_WRITE_PAGE_RESET] +=
		    (end_time + 0xFFFFFFFF - start_time) / 1000;
	}
	/* mt_ftl_err(dev, "write_page time = %lu", (end_time - start_time) / 1000); */
#endif				/* PROFILE */

	return ret;
}

int mt_ftl_commitPMT(struct mt_ftl_blk *dev, bool isReplay, bool isCommitDataCache)
{
	int ret = MT_FTL_SUCCESS;
	int pmt_block = 0, pmt_page = 0;
	struct ubi_volume_desc *desc = dev->desc;
	struct mt_ftl_param *param = dev->param;
	const int page_num_per_block = desc->vol->ubi->leb_size / NAND_PAGE_SIZE;
	int i = 0;

#ifdef PROFILE
	unsigned long start_time = 0, end_time = 0;

	start_time = getnstimenow();
#endif				/* PROFILE */
	if ((!isReplay) && isCommitDataCache)
		ret = mt_ftl_write_page(dev);

	for (i = 0; i < PMT_CACHE_NUM; i++) {
		if (param->i4CurrentPMTClusterInCache[i] == 0xFFFFFFFF)
			continue;
		ubi_assert(param->i4CurrentPMTClusterInCache[i] <= PMT_TOTAL_CLUSTER_NUM);
		if (!PMT_INDICATOR_IS_INCACHE
		    (param->u4PMTIndicator[param->i4CurrentPMTClusterInCache[i]])) {
			mt_ftl_err(dev, "i4CurrentPMTClusterInCache (%d) is not in cache",
				   param->i4CurrentPMTClusterInCache[i]);
			return MT_FTL_FAIL;
		}
		if (!PMT_INDICATOR_IS_DIRTY
		    (param->u4PMTIndicator[param->i4CurrentPMTClusterInCache[i]])) {
			mt_ftl_err(dev, "u4PMTIndicator[%d] = 0x%x is not dirty",
				   param->i4CurrentPMTClusterInCache[i],
				   param->u4PMTIndicator[param->i4CurrentPMTClusterInCache[i]]);
			/* clear i4CurrentPMTClusterInCache */
			PMT_INDICATOR_RESET_INCACHE(param->u4PMTIndicator[param->i4CurrentPMTClusterInCache[i]]);
			param->i4CurrentPMTClusterInCache[i] = 0xFFFFFFFF;
			continue;
		}
		/* Update param->u4BIT of the block that is originally in param->u4PMTCache */
		pmt_block =
		    PMT_INDICATOR_GET_BLOCK(param->u4PMTIndicator
					    [param->i4CurrentPMTClusterInCache[i]]);
		pmt_page =
		    PMT_INDICATOR_GET_PAGE(param->u4PMTIndicator
					   [param->i4CurrentPMTClusterInCache[i]]);
		/*mt_ftl_err(dev, "i4CurrentPMTClusterInCache = %d, u4PMTIndicator[i4CurrentPMTClusterInCache] = 0x%x",
		   param->i4CurrentPMTClusterInCache, param->u4PMTIndicator[param->i4CurrentPMTClusterInCache]);
		   mt_ftl_err(dev, "pmt_block = %d, pmt_page = %d", pmt_block, pmt_page);       // Temporary */
		ubi_assert(pmt_block < NAND_TOTAL_BLOCK_NUM);
		if ((pmt_block != MT_INVALID_BLOCKPAGE) && (pmt_block != 0)) {
			BIT_UPDATE(param->u4BIT[pmt_block], (NAND_PAGE_SIZE * 2));
			mt_ftl_err(dev, "u4BIT[%d] = %d", pmt_block, param->u4BIT[pmt_block]);
		}

		/* Calculate new block/page from param->u4CurrentPMTLebPageIndicator
		 * Update param->u4CurrentPMTLebPageIndicator
		 * and check the correctness of param->u4CurrentPMTLebPageIndicator
		 * and if param->u4CurrentPMTLebPageIndicator is full
		 * need to call get free block/page function
		 * Write old param->u4PMTCache back to new block/page
		 * Update param->u4PMTIndicator of the block/page that is originally in param->u4PMTCache
		 */

		pmt_block = PMT_LEB_PAGE_INDICATOR_GET_BLOCK(param->u4CurrentPMTLebPageIndicator);
		pmt_page = PMT_LEB_PAGE_INDICATOR_GET_PAGE(param->u4CurrentPMTLebPageIndicator);
		pmt_page += 2;
		if (pmt_page >= page_num_per_block) {
			pmt_block = mt_ftl_getfreeblock(dev, &pmt_page, true, isReplay);
			if (pmt_block == MT_INVALID_BLOCKPAGE) {
				mt_ftl_err(dev, "mt_ftl_getfreeblock failed");
				return MT_FTL_FAIL;
			}
		}
		PMT_LEB_PAGE_INDICATOR_SET_BLOCKPAGE(param->u4CurrentPMTLebPageIndicator, pmt_block,
						     pmt_page);
		mt_ftl_err(dev, "u4CurrentPMTLebPageIndicator = 0x%x",
			   param->u4CurrentPMTLebPageIndicator);

		if (!isReplay) {
			/* Write param->u4PMTCache back to new block/page calculated from
			 * param->u4CurrentPMTLebPageIndicator */
			/* mt_ftl_err(dev, "u4CurrentPMTLebPageIndicator = 0x%x",
			   param->u4CurrentPMTLebPageIndicator); */
			/* mt_ftl_err(dev, "pmt_block = %d, pmt_page = %d", pmt_block, pmt_page);        // Temporary */
			if (ubi_is_mapped(desc, pmt_block)) {
				/* Temporary, Maybe could take it as general check, if performance can be improved */
				/* Debug */
				/*
				for (j = 0; j < PM_PER_NANDPAGE; j++) {
				   if ((param->u4PMTCache[i * PM_PER_NANDPAGE + j] > 0x00100000) &&
				   (param->u4PMTCache[i * PM_PER_NANDPAGE + j] != 0xFFFFFFFF)) {
				   mt_ftl_err(dev, "u4PMTCache[%d][%d] = 0x%x",
				   i, j, param->u4PMTCache[i * PM_PER_NANDPAGE + j]);
				   mt_ftl_err(dev, "u4MetaPMTCache[%d][%d] = 0x%x",
				   i, j, param->u4MetaPMTCache[i * PM_PER_NANDPAGE + j]);
				   }
				}
				*/
				/*=======*/
				if (i >= PMT_CACHE_NUM)
					mt_ftl_err(dev, "i(%d) is larger than PMT_CACHE_NUM(%d)", i,
						   PMT_CACHE_NUM);
				/*
				   //mt_ftl_err(dev, "u4CurrentPMTLebPageIndicator = 0x%x",
				   param->u4CurrentPMTLebPageIndicator);
				   //mt_ftl_err(dev, "pmt_block = %d, pmt_page = %d", pmt_block, pmt_page);
				 */
				ret = ubi_leb_write(desc, pmt_block, &param->u4PMTCache[i * PM_PER_NANDPAGE],
						 pmt_page * NAND_PAGE_SIZE, NAND_PAGE_SIZE);
				ret = ubi_leb_write(desc, pmt_block, &param->u4MetaPMTCache[i * PM_PER_NANDPAGE],
						 (pmt_page + 1) * NAND_PAGE_SIZE, NAND_PAGE_SIZE);
			} else {
				mt_ftl_err(dev, "pmt_block(%d) is not mapped", pmt_block);
				return MT_FTL_FAIL;
			}
		}

		PMT_INDICATOR_SET_BLOCKPAGE(param->u4PMTIndicator
					    [param->i4CurrentPMTClusterInCache[i]], pmt_block,
					    pmt_page, 0, i);
		/* mt_ftl_err(dev, "u4PMTIndicator[%d] = 0x%x", param->i4CurrentPMTClusterInCache[i],
			   param->u4PMTIndicator[param->i4CurrentPMTClusterInCache[i]]); */
		/* clear i4CurrentPMTClusterInCache */
		PMT_INDICATOR_RESET_INCACHE(param->u4PMTIndicator[param->i4CurrentPMTClusterInCache[i]]);
		param->i4CurrentPMTClusterInCache[i] = 0xFFFFFFFF;
	}

#ifdef PROFILE
	end_time = getnstimenow();
	if (end_time >= start_time)
		profile_time[MT_FTL_PROFILE_COMMIT_PMT] += (end_time - start_time) / 1000;
	else {
		mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time, start_time);
		profile_time[MT_FTL_PROFILE_COMMIT_PMT] +=
		    (end_time + 0xFFFFFFFF - start_time) / 1000;
	}
#endif				/* PROFILE */

	return ret;
}

int mt_ftl_commit_indicators(struct mt_ftl_blk *dev)
{
	int ret = MT_FTL_SUCCESS;
	int offset = 0, index = 0;
	struct ubi_volume_desc *desc = dev->desc;
	struct mt_ftl_param *param = dev->param;
	const int max_offset_per_block = desc->vol->ubi->leb_size - NAND_PAGE_SIZE;

	memset(param->commit_page_buffer, 0, (NAND_PAGE_SIZE >> 2) * sizeof(unsigned int));
	param->commit_page_buffer[0] = MT_MAGIC_NUMBER;
	param->commit_page_buffer[1] = param->u4NextReplayOffsetIndicator;
	param->commit_page_buffer[2] = param->u4NextLebPageIndicator;
	param->commit_page_buffer[3] = param->u4CurrentPMTLebPageIndicator;
	param->commit_page_buffer[4] = param->u4NextFreeLebIndicator;
	param->commit_page_buffer[5] = param->u4NextFreePMTLebIndicator;
	param->commit_page_buffer[6] = param->u4GCReserveLeb;
	param->commit_page_buffer[7] = param->u4GCReservePMTLeb;
	index = 8;
	if (index + PMT_TOTAL_CLUSTER_NUM * sizeof(unsigned int) > NAND_PAGE_SIZE) {
		mt_ftl_err(dev,
			   "index + PMT_TOTAL_CLUSTER_NUM * sizeof(unsigned int)(0x%lx) > NAND_PAGE_SIZE(%d)",
			   index + PMT_TOTAL_CLUSTER_NUM * sizeof(unsigned int), NAND_PAGE_SIZE);
		mt_ftl_err(dev, "index = %d, PMT_TOTAL_CLUSTER_NUM = %d", index,
			   PMT_TOTAL_CLUSTER_NUM);
	}
	memcpy(&param->commit_page_buffer[index], param->u4PMTIndicator,
	       PMT_TOTAL_CLUSTER_NUM * sizeof(unsigned int));
	index += ((PMT_TOTAL_CLUSTER_NUM * sizeof(unsigned int)) >> 2);
	if (index + NAND_TOTAL_BLOCK_NUM * sizeof(unsigned int) > NAND_PAGE_SIZE) {
		mt_ftl_err(dev,
			   "index + NAND_TOTAL_BLOCK_NUM * sizeof(unsigned int)(0x%lx) > NAND_PAGE_SIZE(%d)",
			   index + NAND_TOTAL_BLOCK_NUM * sizeof(unsigned int), NAND_PAGE_SIZE);
		mt_ftl_err(dev, "index = %d, NAND_TOTAL_BLOCK_NUM = %d", index,
			   NAND_TOTAL_BLOCK_NUM);
	}
	memcpy(&param->commit_page_buffer[index], param->u4BIT,
	       NAND_TOTAL_BLOCK_NUM * sizeof(unsigned int));
	index += ((NAND_TOTAL_BLOCK_NUM * sizeof(unsigned int)) >> 2);
	if (index + PMT_CACHE_NUM * sizeof(unsigned int) > NAND_PAGE_SIZE) {
		mt_ftl_err(dev,
			   "index + PMT_CACHE_NUM * sizeof(unsigned int)(0x%lx) > NAND_PAGE_SIZE(%d)",
			   index + PMT_CACHE_NUM * sizeof(unsigned int), NAND_PAGE_SIZE);
		mt_ftl_err(dev, "index = %d, PMT_CACHE_NUM = %d", index, PMT_CACHE_NUM);
	}
	memcpy(&param->commit_page_buffer[index], param->i4CurrentPMTClusterInCache,
	       PMT_CACHE_NUM * sizeof(unsigned int));

	mt_ftl_err(dev, "u4NextReplayOffsetIndicator = 0x%x", param->u4NextReplayOffsetIndicator);
	mt_ftl_err(dev, "u4NextLebPageIndicator = 0x%x", param->u4NextLebPageIndicator);
	mt_ftl_err(dev, "u4CurrentPMTLebPageIndicator = 0x%x", param->u4CurrentPMTLebPageIndicator);
	mt_ftl_err(dev, "u4NextFreeLebIndicator = 0x%x", param->u4NextFreeLebIndicator);
	mt_ftl_err(dev, "u4NextFreePMTLebIndicator = 0x%x", param->u4NextFreePMTLebIndicator);
	mt_ftl_err(dev, "u4GCReserveLeb = 0x%x", param->u4GCReserveLeb);
	mt_ftl_err(dev, "u4GCReservePMTLeb = 0x%x", param->u4GCReservePMTLeb);
	mt_ftl_err(dev, "u4PMTIndicator = 0x%x, 0x%x, 0x%x, 0x%x",
		   param->u4PMTIndicator[0], param->u4PMTIndicator[1], param->u4PMTIndicator[2],
		   param->u4PMTIndicator[3]);
	mt_ftl_err(dev, "u4BIT = 0x%x, 0x%x, 0x%x, 0x%x", param->u4BIT[0], param->u4BIT[1],
		   param->u4BIT[2], param->u4BIT[3]);
	mt_ftl_err(dev, "i4CurrentPMTClusterInCache = 0x%x, 0x%x, 0x%x, 0x%x",
		   param->i4CurrentPMTClusterInCache[0], param->i4CurrentPMTClusterInCache[1],
		   param->i4CurrentPMTClusterInCache[2], param->i4CurrentPMTClusterInCache[3]);

	offset = mt_ftl_leb_lastpage_offset(dev, CONFIG_START_BLOCK);
	if (offset > max_offset_per_block) {
		ubi_leb_unmap(desc, CONFIG_START_BLOCK);
		mt_ftl_err(dev, "leb%d has been unmapped, desc->vol->eba_tbl[%d] = %d",
			   CONFIG_START_BLOCK, CONFIG_START_BLOCK,
			   desc->vol->eba_tbl[CONFIG_START_BLOCK]);
		ubi_leb_map(desc, CONFIG_START_BLOCK);
		mt_ftl_err(dev, "leb%d has been mapped, desc->vol->eba_tbl[%d] = %d",
			   CONFIG_START_BLOCK, CONFIG_START_BLOCK,
			   desc->vol->eba_tbl[CONFIG_START_BLOCK]);
		offset = 0;
	}
	ret = ubi_leb_write(desc, CONFIG_START_BLOCK, param->commit_page_buffer, offset, NAND_PAGE_SIZE);
	if (ret && ret != UBI_IO_BITFLIPS) {
		mt_ftl_err(dev, "ubi_leb_write (leb%d, offset=%d) failed, ret = 0x%x",
			   CONFIG_START_BLOCK, offset, ret);
		return MT_FTL_FAIL;
	}

	mt_ftl_write_to_blk(dev, CONFIG_START_BLOCK + 1, param->commit_page_buffer);

	/* Erase leb2 */
	ubi_leb_unmap(desc, REPLAY_BLOCK);
	mt_ftl_err(dev, "leb%d has been unmapped, desc->vol->eba_tbl[%d] = %d", REPLAY_BLOCK,
		   REPLAY_BLOCK, desc->vol->eba_tbl[REPLAY_BLOCK]);
	ubi_leb_map(desc, REPLAY_BLOCK);
	mt_ftl_err(dev, "leb%d has been mapped, desc->vol->eba_tbl[%d] = %d", REPLAY_BLOCK,
		   REPLAY_BLOCK, desc->vol->eba_tbl[REPLAY_BLOCK]);
	memset(param->replay_blk_rec, 0xFF, NAND_PAGE_NUM_PER_BLOCK * sizeof(unsigned int));
	param->u4NextReplayOffsetIndicator = 0;
	param->replay_blk_index = 0;
	return ret;
}

int mt_ftl_commit(struct mt_ftl_blk *dev)
{
	int ret = MT_FTL_SUCCESS, i = 0;
	struct ubi_volume_desc *desc = dev->desc;
	struct mt_ftl_param *param = dev->param;

#ifdef PROFILE
	unsigned long start_time = 0, end_time = 0;

	start_time = getnstimenow();
#endif				/* PROFILE */
	ret = ubi_is_mapped(desc, CONFIG_START_BLOCK);
	if (ret == 0)
		ubi_leb_map(desc, CONFIG_START_BLOCK);
	ret = ubi_is_mapped(desc, CONFIG_START_BLOCK + 1);
	if (ret == 0)
		ubi_leb_map(desc, CONFIG_START_BLOCK + 1);

	mt_ftl_commitPMT(dev, false, true);
	for (i = 0; i < PMT_CACHE_NUM; i++) {
		if (param->i4CurrentPMTClusterInCache[i] == 0xFFFFFFFF)
			continue;
		ubi_assert(param->i4CurrentPMTClusterInCache[i] <= PMT_TOTAL_CLUSTER_NUM);
		if (!PMT_INDICATOR_IS_INCACHE
		    (param->u4PMTIndicator[param->i4CurrentPMTClusterInCache[i]])) {
			mt_ftl_err(dev, "i4CurrentPMTClusterInCache (%d) is not in cache",
				   param->i4CurrentPMTClusterInCache[i]);
			return MT_FTL_FAIL;
		}

		PMT_INDICATOR_RESET_INCACHE(param->u4PMTIndicator
					    [param->i4CurrentPMTClusterInCache[i]]);
		mt_ftl_err(dev, "u4PMTIndicator[%d] = %d", param->i4CurrentPMTClusterInCache[i],
			   param->u4PMTIndicator[param->i4CurrentPMTClusterInCache[i]]);
		param->i4CurrentPMTClusterInCache[i] = 0xFFFFFFFF;
	}

	/* Force to store param->u1DataCache into flash */
	if (param->u4DataNum)
		mt_ftl_write_page(dev);

	ret = mt_ftl_commit_indicators(dev);

#ifdef PROFILE
	end_time = getnstimenow();
	if (end_time >= start_time)
		profile_time[MT_FTL_PROFILE_COMMIT] += (end_time - start_time) / 1000;
	else {
		mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time, start_time);
		profile_time[MT_FTL_PROFILE_COMMIT] += (end_time + 0xFFFFFFFF - start_time) / 1000;
	}
#endif				/* PROFILE */

	return ret;
}

int mt_ftl_downloadPMT(struct mt_ftl_blk *dev, int cluster, int cache_num)
{
	int ret = MT_FTL_SUCCESS, i = 0;
	int pmt_block = 0, pmt_page = 0;

	struct ubi_volume_desc *desc = dev->desc;
	struct mt_ftl_param *param = dev->param;

	ubi_assert(cluster <= PMT_TOTAL_CLUSTER_NUM);
	pmt_block = PMT_INDICATOR_GET_BLOCK(param->u4PMTIndicator[cluster]);
	pmt_page = PMT_INDICATOR_GET_PAGE(param->u4PMTIndicator[cluster]);
	if (param->u4MetaPMTCache[0] == 0xFFFFFFFF) {
		mt_ftl_err(dev,
			   "pmt_block = %d, cache_num = %d, cluster = %d, param->u4PMTCache[0] = 0x%x, param->u4MetaPMTCache[0] = 0x%x",
			   pmt_block, cache_num, cluster, param->u4PMTCache[0],
			   param->u4MetaPMTCache[0]);
	}
	ubi_assert(cache_num < PMT_CACHE_NUM);

	if (unlikely(pmt_block == 0) || unlikely(ubi_is_mapped(desc, pmt_block) == 0)) {
		memset(&param->u4PMTCache[cache_num * PM_PER_NANDPAGE], 0xFF,
		       PM_PER_NANDPAGE * sizeof(unsigned int));
		memset(&param->u4MetaPMTCache[cache_num * PM_PER_NANDPAGE], 0xFF,
		       PM_PER_NANDPAGE * sizeof(unsigned int));
	} else {
		ret = ubi_leb_read(desc, pmt_block, (char *)&param->u4PMTCache[cache_num * PM_PER_NANDPAGE],
				pmt_page * NAND_PAGE_SIZE , NAND_PAGE_SIZE, 0);
		if (ret && ret != UBI_IO_BITFLIPS) {
			mt_ftl_err(dev, "u4CurrentPMTLebPageIndicator = 0x%x",
				   param->u4CurrentPMTLebPageIndicator);
			mt_ftl_err(dev, "u4PMTIndicator[%d] = 0x%x", cluster,
				   param->u4PMTIndicator[cluster]);
			mt_ftl_err(dev,
				   "[GET PMT] ubi_leb_read get PMT failed, leb%d, offset=%d, ret = 0x%x",
				   pmt_block, pmt_page * NAND_PAGE_SIZE, ret);
			return MT_FTL_FAIL;
		}
		ret = ubi_leb_read(desc, pmt_block, (char *)&param->u4MetaPMTCache[cache_num * PM_PER_NANDPAGE],
			(pmt_page + 1) * NAND_PAGE_SIZE, NAND_PAGE_SIZE, 0);
		if (ret && ret != UBI_IO_BITFLIPS) {
			mt_ftl_err(dev, "u4CurrentPMTLebPageIndicator = 0x%x",
				   param->u4CurrentPMTLebPageIndicator);
			mt_ftl_err(dev, "u4PMTIndicator[%d] = 0x%x", cluster,
				   param->u4PMTIndicator[cluster]);
			mt_ftl_err(dev,
				   "[GET PMT]ubi_leb_read get Meta PMT failed, leb%d, offset=%d, ret = 0x%x",
				   pmt_block, (pmt_page + 1) * NAND_PAGE_SIZE, ret);
			return MT_FTL_FAIL;
		}
	}
	/* Debug */
	/*for (j = 0; j < PM_PER_NANDPAGE; j++) {
	   if ((param->u4PMTCache[cache_num * PM_PER_NANDPAGE + j] > 0x00100000) &&
	   (param->u4PMTCache[cache_num * PM_PER_NANDPAGE + j] != 0xFFFFFFFF)) {
	   mt_ftl_err(dev, "u4PMTIndicator[%d] = 0x%x", cluster, param->u4PMTIndicator[cluster]);
	   mt_ftl_err(dev, "u4PMTCache[%d][%d] = 0x%x",
	   cache_num, j, param->u4PMTCache[cache_num * PM_PER_NANDPAGE + j]);
	   mt_ftl_err(dev, "u4MetaPMTCache[%d][%d] = 0x%x",
	   cache_num, j, param->u4MetaPMTCache[cache_num * PM_PER_NANDPAGE + j]);
	   }
	   } */
	/*=======*/
	/* consider cluser if in cache */
	for (i = 0; i < PMT_CACHE_NUM; i++) {
		if (param->i4CurrentPMTClusterInCache[i] == cluster) {
			mt_ftl_err(dev, "[Bean]Tempory solution cluster is in cache already(%d)\n", i);
			dump_stack();
			break;
		}
	}
	/*if(i >= PMT_CACHE_NUM)  */
	param->i4CurrentPMTClusterInCache[cache_num] = cluster;
	mt_ftl_err(dev, "i4CurrentPMTClusterInCache[%d] = %d", cache_num, param->i4CurrentPMTClusterInCache[cache_num]);
	PMT_INDICATOR_SET_CACHE_BUF_NUM(param->u4PMTIndicator[cluster], cache_num);
	mt_ftl_err(dev, "u4PMTIndicator[%d] = 0x%x", cluster, param->u4PMTIndicator[cluster]);

	return ret;
}

int mt_ftl_updatePMT(struct mt_ftl_blk *dev, int cluster, int sec_offset, int leb, int offset,
		     int part, unsigned int cmpr_data_size, bool isReplay, bool isCommitDataCache)
{
	int ret = MT_FTL_SUCCESS, i = 0;
	unsigned int *pmt = NULL;
	unsigned int *meta_pmt = NULL;
	int old_leb = 0, old_data_size = 0;
	/* struct ubi_volume_desc *desc = dev->desc; */
	struct mt_ftl_param *param = dev->param;

#ifdef PROFILE
	unsigned long start_time = 0, end_time = 0;
#endif				/* PROFILE */
	ubi_assert(cluster <= PMT_TOTAL_CLUSTER_NUM);
	if (!PMT_INDICATOR_IS_INCACHE(param->u4PMTIndicator[cluster])) {	/* cluster is not in cache */
#ifdef PROFILE
		start_time = getnstimenow();
#endif				/* PROFILE */
		for (i = 0; i < PMT_CACHE_NUM; i++) {
			if (param->i4CurrentPMTClusterInCache[i] == 0xFFFFFFFF)
				break;
			ubi_assert(param->i4CurrentPMTClusterInCache[i] <= PMT_TOTAL_CLUSTER_NUM);
			if (!PMT_INDICATOR_IS_INCACHE(param->u4PMTIndicator[param->i4CurrentPMTClusterInCache[i]])) {
				/* Cluster download PMT CLUSTER cache, but i4CurrentPMTClusterInCache not to update */
				mt_ftl_err(dev, "i4CurrentPMTClusterInCache (%d) is not in cache",
					   param->i4CurrentPMTClusterInCache[i]);
				dump_stack();
				return MT_FTL_FAIL;
			}
			if (!PMT_INDICATOR_IS_DIRTY
			    (param->u4PMTIndicator[param->i4CurrentPMTClusterInCache[i]]))
				break;
		}
		if (i == PMT_CACHE_NUM) {
			mt_ftl_err(dev, "All PMT cache are dirty, start commit PMT");
			/* Just for downloading corresponding PMT in cache */
			if ((leb == MT_INVALID_BLOCKPAGE) || (!isCommitDataCache))
				mt_ftl_commitPMT(dev, isReplay, false);
			else
				mt_ftl_commitPMT(dev, isReplay, true);
			i = 0;
		}
		if (param->i4CurrentPMTClusterInCache[i] != 0xFFFFFFFF) {
			PMT_INDICATOR_RESET_INCACHE(param->u4PMTIndicator
						    [param->i4CurrentPMTClusterInCache[i]]);
			mt_ftl_err(dev, "Reset i(%d) u4PMTIndicator[%d] = 0x%x", i,
				   param->i4CurrentPMTClusterInCache[i],
				   param->u4PMTIndicator[param->i4CurrentPMTClusterInCache[i]]);
			param->i4CurrentPMTClusterInCache[i] = 0xFFFFFFFF;
		}
#ifdef PROFILE
		end_time = getnstimenow();
		if (end_time >= start_time)
			profile_time[MT_FTL_PROFILE_UPDATE_PMT_FINDCACHE_COMMITPMT] +=
			    (end_time - start_time) / 1000;
		else {
			mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time, start_time);
			profile_time[MT_FTL_PROFILE_UPDATE_PMT_FINDCACHE_COMMITPMT] +=
			    (end_time + 0xFFFFFFFF - start_time) / 1000;
		}

		start_time = getnstimenow();
#endif				/* PROFILE */

		/* Download PMT from the block/page in param->u4PMTIndicator[cluster] */
		ret = mt_ftl_downloadPMT(dev, cluster, i);
		if (ret)
			return ret;

#ifdef PROFILE
		end_time = getnstimenow();
		if (end_time >= start_time)
			profile_time[MT_FTL_PROFILE_UPDATE_PMT_DOWNLOADPMT] +=
			    (end_time - start_time) / 1000;
		else {
			mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time, start_time);
			profile_time[MT_FTL_PROFILE_UPDATE_PMT_DOWNLOADPMT] +=
			    (end_time + 0xFFFFFFFF - start_time) / 1000;
		}

#endif				/* PROFILE */
	} else
		i = PMT_INDICATOR_CACHE_BUF_NUM(param->u4PMTIndicator[cluster]);

#ifdef PROFILE
	start_time = getnstimenow();
#endif				/* PROFILE */

	if (leb == MT_INVALID_BLOCKPAGE) {	/* Just for downloading corresponding PMT in cache */
#ifdef PROFILE
		end_time = getnstimenow();
		if (end_time >= start_time)
			profile_time[MT_FTL_PROFILE_UPDATE_PMT_MODIFYPMT] +=
			    (end_time - start_time) / 1000;
		else {
			mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time, start_time);
			profile_time[MT_FTL_PROFILE_UPDATE_PMT_MODIFYPMT] +=
			    (end_time + 0xFFFFFFFF - start_time) / 1000;
		}
#endif				/* PROFILE */
		return ret;
	}

	ubi_assert(i < PMT_CACHE_NUM);
	ubi_assert(sec_offset < PM_PER_NANDPAGE);

	pmt = &param->u4PMTCache[i * PM_PER_NANDPAGE + sec_offset];
	meta_pmt = &param->u4MetaPMTCache[i * PM_PER_NANDPAGE + sec_offset];

	/* Update param->u4BIT */
	if (*pmt != NAND_DEFAULT_VALUE) {
		/* BIT_UPDATE_FSPAGE(param->u4BIT[PMT_GET_BLOCK(*pmt)]); */
		old_leb = PMT_GET_BLOCK(*pmt);
		old_data_size = PMT_GET_DATASIZE(*meta_pmt);
		ubi_assert(old_leb < NAND_TOTAL_BLOCK_NUM);
		BIT_UPDATE(param->u4BIT[old_leb], old_data_size);
		if (old_data_size == 0) {
			mt_ftl_err(dev, "pmt = 0x%x, meta_pmt = 0x%x, u4PMTIndicator[%d] = 0x%x",
				   *pmt, *meta_pmt, cluster, param->u4PMTIndicator[cluster]);
		}
		if ((param->u4BIT[old_leb] & 0x3FFFF) == 0)
			mt_ftl_err(dev, "u4BIT[%d] = %d", old_leb, param->u4BIT[old_leb]);
	}

	/* Update param->u4PMTCache and param->u4MetaPMTCache */
	PMT_SET_BLOCKPAGE(*pmt, leb, offset / NAND_PAGE_SIZE);
	META_PMT_SET_DATA(*meta_pmt, cmpr_data_size, part, -1);	/* Data not in cache */
	PMT_INDICATOR_SET_DIRTY(param->u4PMTIndicator[cluster]);
	/* mt_ftl_err(dev, "u4PMTIndicator[%d] = 0x%x", cluster, param->u4PMTIndicator[cluster]);        // Temporary */

#ifdef PROFILE
	end_time = getnstimenow();
	if (end_time >= start_time)
		profile_time[MT_FTL_PROFILE_UPDATE_PMT_MODIFYPMT] += (end_time - start_time) / 1000;
	else {
		mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time, start_time);
		profile_time[MT_FTL_PROFILE_UPDATE_PMT_MODIFYPMT] +=
		    (end_time + 0xFFFFFFFF - start_time) / 1000;
	}
#endif				/* PROFILE */

	return ret;
}

/* Suppose FS_PAGE_SIZE for each write */
int mt_ftl_write(struct mt_ftl_blk *dev, const char *buffer, sector_t sector, int len)
{
	int ret = MT_FTL_SUCCESS;
	int leb = 0, page = 0;
	unsigned int cluster = 0, sec_offset = 0;
	int cache_num = 0;
	int *meta_pmt = NULL;
	unsigned int cmpr_len = 0;
	unsigned int data_offset = 0;
	unsigned int total_consumed_size = 0;
	/* struct ubi_volume_desc *desc = dev->desc; */
	struct mt_ftl_param *param = dev->param;

#ifdef PROFILE
	unsigned long start_time = 0, end_time = 0;
	unsigned long start_time_all = 0, end_time_all = 0;

	start_time = getnstimenow();
	start_time_all = getnstimenow();
#endif				/* PROFILE */

	/* TODO: if the sector has been in cache, just modify it in cache, instead of write into nand */

	ret = crypto_comp_compress(param->cc, buffer, len, param->cmpr_page_buffer, &cmpr_len);
	if (ret) {
		mt_ftl_err(dev, "ret = %d, cmpr_len = %d, len = 0x%x", ret, cmpr_len, len);
		mt_ftl_err(dev, "cc = 0x%lx, buffer = 0x%lx", (unsigned long int)param->cc, (unsigned long int)buffer);
		mt_ftl_err(dev, "cmpr_page_buffer = 0x%lx", (unsigned long int)param->cmpr_page_buffer);
		return MT_FTL_FAIL;
	}
#ifdef PROFILE
	end_time = getnstimenow();
	if (end_time >= start_time)
		profile_time[MT_FTL_PROFILE_WRITE_COMPRESS] += (end_time - start_time) / 1000;
	else {
		mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time, start_time);
		profile_time[MT_FTL_PROFILE_WRITE_COMPRESS] +=
		    (end_time + 0xFFFFFFFF - start_time) / 1000;
	}
	start_time = getnstimenow();
#endif				/* PROFILE */

	/* mt_ftl_err(dev, "cmpr_len = %d", cmpr_len);   // Temporary */
	if (MTKFTL_MAX_DATA_NUM_PER_PAGE < param->u4DataNum) {
		mt_ftl_err(dev, "MTKFTL_MAX_DATA_NUM_PER_PAGE(%d) < param->u4DataNum(%d)",
			   MTKFTL_MAX_DATA_NUM_PER_PAGE, param->u4DataNum);
	}

	if (param->u4DataNum > 0) {
		data_offset =
		    ((param->
		      u4Header[MTKFTL_MAX_DATA_NUM_PER_PAGE -
			       param->u4DataNum].offset_len >> 16) & 0xFFFF)
		    +
		    (param->
		     u4Header[MTKFTL_MAX_DATA_NUM_PER_PAGE - param->u4DataNum].offset_len & 0xFFFF);
	}
	total_consumed_size =
	    data_offset + cmpr_len + (param->u4DataNum + 1) * sizeof(struct mt_ftl_data_header) + 4;

	if ((total_consumed_size > NAND_PAGE_SIZE)
	    || (param->u4DataNum >= MTKFTL_MAX_DATA_NUM_PER_PAGE)) {
		ret = mt_ftl_write_page(dev);
		data_offset = 0;
	}
#ifdef PROFILE
	end_time = getnstimenow();
	if (end_time >= start_time)
		profile_time[MT_FTL_PROFILE_WRITE_WRITEPAGE] += (end_time - start_time) / 1000;
	else {
		mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time, start_time);
		profile_time[MT_FTL_PROFILE_WRITE_WRITEPAGE] +=
		    (end_time + 0xFFFFFFFF - start_time) / 1000;
	}
	start_time = getnstimenow();
#endif				/* PROFILE */

	leb = PMT_LEB_PAGE_INDICATOR_GET_BLOCK(param->u4NextLebPageIndicator);
	page = PMT_LEB_PAGE_INDICATOR_GET_PAGE(param->u4NextLebPageIndicator);

	param->u4Header[MTKFTL_MAX_DATA_NUM_PER_PAGE - param->u4DataNum - 1].sector =
	    (sector / (FS_PAGE_SIZE >> 9)) * (FS_PAGE_SIZE >> 9);
	param->u4Header[MTKFTL_MAX_DATA_NUM_PER_PAGE - param->u4DataNum - 1].offset_len =
	    (data_offset << 16) | cmpr_len;
	/* memcpy(&param->u1DataCache[u4DataCacheIndex], buffer, len); */
	if ((data_offset + cmpr_len) >= NAND_PAGE_SIZE) {
		mt_ftl_err(dev, "(data_offset + cmpr_len)(%d) >= NAND_PAGE_SIZE(%d)",
			   (data_offset + cmpr_len), NAND_PAGE_SIZE);
		mt_ftl_err(dev, "data_offset = %d, cmpr_len = %d", data_offset, cmpr_len);
		ubi_assert(false);
	}
	memcpy(&param->u1DataCache[data_offset], param->cmpr_page_buffer, cmpr_len);
	param->u4DataNum++;

#ifdef PROFILE
	end_time = getnstimenow();
	if (end_time >= start_time)
		profile_time[MT_FTL_PROFILE_WRITE_COPYTOCACHE] += (end_time - start_time) / 1000;
	else {
		mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time, start_time);
		profile_time[MT_FTL_PROFILE_WRITE_COPYTOCACHE] +=
		    (end_time + 0xFFFFFFFF - start_time) / 1000;
	}
	/* mt_ftl_err(dev, "write_page time = %lu", (end_time - start_time) / 1000); */
	start_time = getnstimenow();
#endif				/* PROFILE */

	cluster = ((unsigned long int)sector / (FS_PAGE_SIZE >> 9)) / PM_PER_NANDPAGE;
	sec_offset = ((unsigned long int)sector / (FS_PAGE_SIZE >> 9)) & (PM_PER_NANDPAGE - 1);
	ret = mt_ftl_updatePMT(dev, cluster, sec_offset, leb, page * NAND_PAGE_SIZE, param->u4DataNum - 1,
			 cmpr_len, 0, 1);
	if (ret < 0) {
		mt_ftl_err(dev, "mt_ftl_updatePMT cluster(%d) offset(%d) leb(%d) page(%d) fail\n",
			cluster, sec_offset, leb, page);
		return ret;
	}

#ifdef PROFILE
	end_time = getnstimenow();
	if (end_time >= start_time)
		profile_time[MT_FTL_PROFILE_WRITE_UPDATEPMT] += (end_time - start_time) / 1000;
	else {
		mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time, start_time);
		profile_time[MT_FTL_PROFILE_WRITE_UPDATEPMT] +=
		    (end_time + 0xFFFFFFFF - start_time) / 1000;
	}
	start_time = getnstimenow();
#endif				/* PROFILE */
	ubi_assert(cluster <= PMT_TOTAL_CLUSTER_NUM);
	cache_num = PMT_INDICATOR_CACHE_BUF_NUM(param->u4PMTIndicator[cluster]);
	ubi_assert(cache_num < PMT_CACHE_NUM);
	ubi_assert(sec_offset < PM_PER_NANDPAGE);
	meta_pmt = &param->u4MetaPMTCache[cache_num * PM_PER_NANDPAGE + sec_offset];
	if (param->u4DataNum != 0)
		PMT_SET_DATACACHE_BUF_NUM(*meta_pmt, 0);	/* Data is in cache 0 */

	PMT_INDICATOR_SET_DIRTY(param->u4PMTIndicator[cluster]);	/* Set corresponding PMT cache to dirty */

	/* TODO: if sync, write into nand */
	/*if (dev->sync == 1) {
	   mt_ftl_err(dev, "write sync");
	   ret = mt_ftl_write_page(dev);
	   } */

#ifdef PROFILE
	end_time = getnstimenow();
	end_time_all = getnstimenow();
	if (end_time >= start_time)
		profile_time[MT_FTL_PROFILE_WRITE_WRITEPAGE] += (end_time - start_time) / 1000;
	else {
		mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time, start_time);
		profile_time[MT_FTL_PROFILE_WRITE_WRITEPAGE] +=
		    (end_time + 0xFFFFFFFF - start_time) / 1000;
	}
	if (end_time_all >= start_time_all)
		profile_time[MT_FTL_PROFILE_WRITE_ALL] += (end_time_all - start_time_all) / 1000;
	else {
		mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time_all, start_time_all);
		profile_time[MT_FTL_PROFILE_WRITE_ALL] +=
		    (end_time_all + 0xFFFFFFFF - start_time_all) / 1000;
	}
#endif				/* PROFILE */

	return ret;
}

/* Suppose FS_PAGE_SIZE for each read */
int mt_ftl_read(struct mt_ftl_blk *dev, const char *buffer, sector_t sector, int len)
{
	int ret = MT_FTL_SUCCESS;
	int leb = 0, page = 0, part = 0;
	unsigned int cluster = 0, sec_offset = 0;
	int pmt = 0, meta_pmt = 0;
	int offset_in_pagebuf = 0;
	int pmt_block = 0, pmt_page = 0;
	int cache_num = 0, data_cache_num = 0;
	unsigned int decmpr_len = 0;
	unsigned int data_num = 0, data_num_offset = 0;
	struct ubi_volume_desc *desc = dev->desc;
	struct mt_ftl_param *param = dev->param;
	unsigned int data_hdr_offset =
	    NAND_PAGE_SIZE - (param->u4DataNum * sizeof(struct mt_ftl_data_header) + 4);
	unsigned char *page_buffer = NULL;
	struct mt_ftl_data_header *header_buffer = NULL;

#ifdef PROFILE
	unsigned long start_time = 0, end_time = 0;
	unsigned long start_time_all = 0, end_time_all = 0;
	unsigned long start_time_test = 0, end_time_test = 0;	/* Temporary */

	start_time = getnstimenow();
	start_time_all = getnstimenow();
#endif				/* PROFILE */

	cluster = ((unsigned long int)sector / (FS_PAGE_SIZE >> 9)) / PM_PER_NANDPAGE;
	sec_offset = ((unsigned long int)sector / (FS_PAGE_SIZE >> 9)) & (PM_PER_NANDPAGE - 1);

	ubi_assert(sec_offset <= PM_PER_NANDPAGE);
	ubi_assert(cluster <= PMT_TOTAL_CLUSTER_NUM);
	/* Download corresponding PMT to cache */
	if (PMT_INDICATOR_IS_INCACHE(param->u4PMTIndicator[cluster])) {
		cache_num = PMT_INDICATOR_CACHE_BUF_NUM(param->u4PMTIndicator[cluster]);
		if (cache_num >= PMT_CACHE_NUM)
			mt_ftl_err(dev, "cache_num(%d) is larger than PMT_CACHE_NUM(%d)", cache_num,
				   PMT_CACHE_NUM);
		if (sec_offset >= PM_PER_NANDPAGE)
			mt_ftl_err(dev, "sec_offset(%d) is larger than PM_PER_NANDPAGE(%d)",
				   sec_offset, PM_PER_NANDPAGE);
		pmt = param->u4PMTCache[cache_num * PM_PER_NANDPAGE + sec_offset];
		meta_pmt = param->u4MetaPMTCache[cache_num * PM_PER_NANDPAGE + sec_offset];
		/*mt_ftl_err(dev,
		  "cluster is in write cache, cache_num = %d, pmt = 0x%x, meta_pmt = 0x%x, u4PMTIndicator[%d] = 0x%x",
		  cache_num, pmt, meta_pmt, cluster, param->u4PMTIndicator[cluster]);  Temporary */
	} else if (cluster == param->i4CurrentReadPMTClusterInCache) {
		/*mt_ftl_err(dev, "cluster == i4CurrentReadPMTClusterInCache (%d)",
		   param->i4CurrentReadPMTClusterInCache);  */
		pmt = param->u4ReadPMTCache[sec_offset];
		meta_pmt = param->u4ReadMetaPMTCache[sec_offset];
		mt_ftl_err(dev, "cluster==(%d) is in read cache, pmt = 0x%x, meta_pmt = 0x%x, sec_offset = %d",
		   cluster, pmt, meta_pmt, sec_offset);
	} else {
		pmt_block = PMT_INDICATOR_GET_BLOCK(param->u4PMTIndicator[cluster]);
		pmt_page = PMT_INDICATOR_GET_PAGE(param->u4PMTIndicator[cluster]);

		ret = ubi_is_mapped(desc, pmt_block);
		if (ret == 0) {
			mt_ftl_err(dev, "leb%d is unmapped", pmt_block);
			return MT_FTL_FAIL;
		}

		if (unlikely(pmt_block == 0)) {
			mt_ftl_err(dev, "pmt_block == 0");
			memset(param->u4ReadPMTCache, 0xFF, PM_PER_NANDPAGE * sizeof(unsigned int));
			memset(param->u4ReadMetaPMTCache, 0xFF,
			       PM_PER_NANDPAGE * sizeof(unsigned int));
			param->i4CurrentReadPMTClusterInCache = 0xFFFFFFFF;
		} else {
			/* mt_ftl_err(dev, "Get PMT of cluster (%d)", cluster); */
			ret = ubi_leb_read(desc, pmt_block, (char *)param->u4ReadPMTCache, pmt_page * NAND_PAGE_SIZE,
				NAND_PAGE_SIZE, 0);
			if (ret && ret != UBI_IO_BITFLIPS) {
				mt_ftl_err(dev, "u4CurrentPMTLebPageIndicator = 0x%x",
					   param->u4CurrentPMTLebPageIndicator);
				mt_ftl_err(dev, "u4PMTIndicator[%d] = 0x%x", cluster,
					   param->u4PMTIndicator[cluster]);
				mt_ftl_err(dev,
					   "[GET PMT]ubi_leb_read PMT failed, leb%d, offset=%d, ret = %d",
					   pmt_block, pmt_page * NAND_PAGE_SIZE, ret);
				return MT_FTL_FAIL;
			}
			ret = ubi_leb_read(desc, pmt_block, (char *)param->u4ReadMetaPMTCache,
				(pmt_page + 1) * NAND_PAGE_SIZE, NAND_PAGE_SIZE, 0);
			if (ret && ret != UBI_IO_BITFLIPS) {
				mt_ftl_err(dev, "u4CurrentPMTLebPageIndicator = 0x%x",
					   param->u4CurrentPMTLebPageIndicator);
				mt_ftl_err(dev, "u4PMTIndicator[%d] = 0x%x", cluster,
					   param->u4PMTIndicator[cluster]);
				mt_ftl_err(dev,
					   "[GET PMT]ubi_leb_read Meta PMT failed, leb%d, offset=%d, ret = %d",
					   pmt_block, (pmt_page + 1) * NAND_PAGE_SIZE, ret);
				return MT_FTL_FAIL;
			}
			param->i4CurrentReadPMTClusterInCache = cluster;
		}
		pmt = param->u4ReadPMTCache[sec_offset];
		meta_pmt = param->u4ReadMetaPMTCache[sec_offset];
		/* mt_ftl_err(dev,
		"Read cluster(%d) to read cache, sec_offset = %d, pmt = 0x%x, meta_pmt = 0x%x,
		u4PMTIndicator[%d] = 0x%x",cluster, sec_offset, pmt, meta_pmt, cluster,
		param->u4PMTIndicator[cluster]);         Temporary */
	}

#ifdef PROFILE
	end_time = getnstimenow();
	if (end_time >= start_time)
		profile_time[MT_FTL_PROFILE_READ_GETPMT] += (end_time - start_time) / 1000;
	else {
		mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time, start_time);
		profile_time[MT_FTL_PROFILE_READ_GETPMT] +=
		    (end_time + 0xFFFFFFFF - start_time) / 1000;
	}
	start_time = getnstimenow();
#endif				/* PROFILE */

/*	mt_ftl_updatePMT(dev, cluster, sec_offset, MT_INVALID_BLOCKPAGE, 0, 0, 0);
	if (PMT_INDICATOR_IS_INCACHE(param->u4PMTIndicator[cluster])) {
		cache_num = PMT_INDICATOR_CACHE_BUF_NUM(param->u4PMTIndicator[cluster]);
		pmt = param->u4PMTCache[cache_num * PM_PER_NANDPAGE + sec_offset];
		mt_ftl_err(dev, "cluster is in write cache, cache_num = %d, pmt = 0x%x, u4PMTIndicator[%d] = 0x%x",
			cache_num, pmt, cluster, param->u4PMTIndicator[cluster]);	// Temporary
	} else {
		mt_ftl_err(dev, "u4PMTIndicator[%d] (0x%x) doesn't in cache after mt_ftl_updatePMT",
				cluster, param->u4PMTIndicator[cluster]);
		return MT_FTL_FAIL;
	}*/

	/* Look up PMT in cache and get data from NAND flash */
	if (pmt == NAND_DEFAULT_VALUE) {
		/* TODO: modify this log */
		mt_ftl_err(dev, "PMT of sector(0x%lx) is invalid", (unsigned long int)sector);
		memset((void *)buffer, 0x0, len);
		return MT_FTL_SUCCESS;
	}
	leb = PMT_GET_BLOCK(pmt);
	page = PMT_GET_PAGE(pmt);
	part = PMT_GET_PART(meta_pmt);

	/* mt_ftl_err(dev, "Copy to cache"); */

	if (MTKFTL_MAX_DATA_NUM_PER_PAGE < part) {
		mt_ftl_err(dev, "MTKFTL_MAX_DATA_NUM_PER_PAGE(%d) < part(%d)",
			   MTKFTL_MAX_DATA_NUM_PER_PAGE, part);
	}

	if (PMT_IS_DATA_INCACHE(meta_pmt)) {
		mt_ftl_err(dev, "[INFO] Use data in cache");
		data_cache_num = PMT_GET_DATACACHENUM(meta_pmt);	/* Not used yet */
		header_buffer = &param->u4Header[MTKFTL_MAX_DATA_NUM_PER_PAGE - part - 1];
		offset_in_pagebuf = header_buffer->offset_len >> 16;
		if ((offset_in_pagebuf + (header_buffer->offset_len & 0xFFFF)) >= NAND_PAGE_SIZE) {
			mt_ftl_err(dev,
				   "(offset_in_pagebuf + (header_buffer->offset_len & 0xFFFF))(%d) >= NAND_PAGE_SIZE(%d)"
				   , (offset_in_pagebuf + (header_buffer->offset_len & 0xFFFF)), NAND_PAGE_SIZE);
			mt_ftl_err(dev,
				   "offset_in_pagebuf = %d, (header_buffer->offset_len & 0xFFFF) = %d",
				   offset_in_pagebuf, (header_buffer->offset_len & 0xFFFF));
		}
		page_buffer = &param->u1DataCache[offset_in_pagebuf];
	} else {
#ifdef PROFILE
		start_time_test = getnstimenow();
#endif				/* PROFILE */

		data_num_offset = (page + 1) * NAND_PAGE_SIZE - 4;
		data_hdr_offset = data_num_offset - (part + 1) * sizeof(struct mt_ftl_data_header);

		ret = ubi_leb_read(desc, leb, (char *)param->u4ReadHeader, data_hdr_offset,
				sizeof(struct mt_ftl_data_header), 0);
		if (ret && ret != UBI_IO_BITFLIPS) {
			mt_ftl_err(dev,
				   "ubi_leb_read data header failed, leb = %d, offset=%d ret = 0x%x",
				   leb, data_hdr_offset, ret);
			return MT_FTL_FAIL;
		}

		header_buffer = &param->u4ReadHeader[0];

#ifdef PROFILE
		end_time_test = getnstimenow();
		if (end_time_test >= start_time_test)
			profile_time[MT_FTL_PROFILE_READ_DATATOCACHE_TEST2] +=
			    (end_time_test - start_time_test) / 1000;
		else {
			mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time_test,
				   start_time_test);
			profile_time[MT_FTL_PROFILE_READ_DATATOCACHE_TEST2] +=
			    (end_time_test + 0xFFFFFFFF - start_time_test) / 1000;
		}
		start_time_test = getnstimenow();
#endif				/* PROFILE */

		offset_in_pagebuf = page * NAND_PAGE_SIZE + (header_buffer->offset_len >> 16);
		ret = ubi_leb_read(desc, leb, (char *)param->general_page_buffer, offset_in_pagebuf,
				(header_buffer->offset_len & 0xFFFF), 0);
		if (ret && ret != UBI_IO_BITFLIPS) {
			mt_ftl_err(dev,
				   "ubi_leb_read data failed, leb = %d, offset=%d ret = 0x%x",
				   leb, offset_in_pagebuf, ret);
			return MT_FTL_FAIL;
		}
		page_buffer = (unsigned char *)param->general_page_buffer;

#ifdef PROFILE
		end_time_test = getnstimenow();
		if (end_time_test >= start_time_test)
			profile_time[MT_FTL_PROFILE_READ_DATATOCACHE_TEST3] +=
			    (end_time_test - start_time_test) / 1000;
		else {
			mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time_test,
				   start_time_test);
			profile_time[MT_FTL_PROFILE_READ_DATATOCACHE_TEST3] +=
			    (end_time_test + 0xFFFFFFFF - start_time_test) / 1000;
		}
#endif				/* PROFILE */
	}

#ifdef PROFILE
	end_time = getnstimenow();
	if (end_time >= start_time)
		profile_time[MT_FTL_PROFILE_READ_DATATOCACHE] += (end_time - start_time) / 1000;
	else {
		mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time, start_time);
		profile_time[MT_FTL_PROFILE_READ_DATATOCACHE] +=
		    (end_time + 0xFFFFFFFF - start_time) / 1000;
	}
	start_time = getnstimenow();
#endif				/* PROFILE */

	/* mt_ftl_err(dev, "Check sector"); */

	if (header_buffer->sector != ((sector / (FS_PAGE_SIZE >> 9)) * (FS_PAGE_SIZE >> 9))) {
		if ((header_buffer->sector == 0xFFFFFFFF) && (page_buffer[0] == 0xFF)) {
			mt_ftl_err(dev, "sector(0x%lx) hasn't been written",
				   (unsigned long int)sector);
			mt_ftl_err(dev, "leb = %d, page = %d, part = %d", leb, page, part);
			memset((void *)buffer, 0xFF, len);
			return MT_FTL_SUCCESS;
		}
		ret = ubi_leb_read(desc, leb, (char *)param->tmp_page_buffer, page * NAND_PAGE_SIZE, NAND_PAGE_SIZE, 0);
		if (ret && ret != UBI_IO_BITFLIPS) {
			mt_ftl_err(dev,
				   "ubi_leb_read data_num failed, leb = %d, offset=%d ret = 0x%x",
				   leb, 0, ret);
			return MT_FTL_FAIL;
		}
		data_num = PAGE_GET_DATA_NUM(param->tmp_page_buffer[(NAND_PAGE_SIZE >> 2) - 1]);
		mt_ftl_err(dev,
			   "header_buffer[%d].sector(0x%lx) != sector (0x%lx), header_buffer[%d].offset_len = 0x%x",
			   part, (unsigned long int)header_buffer->sector,
			   (unsigned long int)sector, part, header_buffer->offset_len);
		mt_ftl_err(dev, "page_buffer[0] = 0x%x, u4PMTIndicator[%d] = 0x%x, data_num = %d",
			   page_buffer[0], cluster, param->u4PMTIndicator[cluster], data_num);
		mt_ftl_err(dev,
			   "pmt = 0x%x, meta_pmt = 0x%x, leb = %d, page = %d, part = %d",
			   pmt, meta_pmt, leb, page, part);
		mt_ftl_err(dev, "data_num_offset = %d, data_hdr_offset = %d", data_num_offset,
			   data_hdr_offset);
		mt_ftl_err(dev, "0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x",
			   param->tmp_page_buffer[(NAND_PAGE_SIZE >> 2) - 1],
			   param->tmp_page_buffer[(NAND_PAGE_SIZE >> 2) - 2],
			   param->tmp_page_buffer[(NAND_PAGE_SIZE >> 2) - 3],
			   param->tmp_page_buffer[(NAND_PAGE_SIZE >> 2) - 4],
			   param->tmp_page_buffer[(NAND_PAGE_SIZE >> 2) - 5],
			   param->tmp_page_buffer[(NAND_PAGE_SIZE >> 2) - 6],
			   param->tmp_page_buffer[(NAND_PAGE_SIZE >> 2) - 7],
			   param->tmp_page_buffer[(NAND_PAGE_SIZE >> 2) - 8],
			   param->tmp_page_buffer[(NAND_PAGE_SIZE >> 2) - 9]);
		/*=====================Debug==========================*/
		/* Calculate clusters and sec_offsets */
		cluster =
		    ((unsigned long int)header_buffer->sector / (FS_PAGE_SIZE >> 9)) /
		    PM_PER_NANDPAGE;
		sec_offset =
		    ((unsigned long int)header_buffer->sector /
		     (FS_PAGE_SIZE >> 9)) & (PM_PER_NANDPAGE - 1);
		mt_ftl_err(dev, "cluster = %d", cluster);
		mt_ftl_err(dev, "u4PMTIndicator[%d] = 0x%x", cluster,
			   param->u4PMTIndicator[cluster]);
		ubi_assert(sec_offset <= PM_PER_NANDPAGE);

		/* Download PMT to read PMT cache */
		/* Don't use mt_ftl_updatePMT, that will cause PMT indicator mixed in replay */
		if (PMT_INDICATOR_IS_INCACHE(param->u4PMTIndicator[cluster])) {
			cache_num = PMT_INDICATOR_CACHE_BUF_NUM(param->u4PMTIndicator[cluster]);
			ubi_assert(cache_num < PMT_CACHE_NUM);
			ubi_assert(sec_offset < PM_PER_NANDPAGE);
			pmt = param->u4PMTCache[cache_num * PM_PER_NANDPAGE + sec_offset];
			meta_pmt = param->u4MetaPMTCache[cache_num * PM_PER_NANDPAGE + sec_offset];
			/*mt_ftl_err(dev, "[Debug] cluster is in write cache, cache_num = %d, pmt = 0x%x,
			  u4PMTIndicator[%d] = 0x%x",
			   cache_num, pmt, cluster, param->u4PMTIndicator[cluster]);    // Temporary */
		} else if (cluster == param->i4CurrentReadPMTClusterInCache) {
			/* mt_ftl_err(dev, "[Debug] cluster == i4CurrentReadPMTClusterInCache (%d)",
			   param->i4CurrentReadPMTClusterInCache); */
			pmt = param->u4ReadPMTCache[sec_offset];
			meta_pmt = param->u4ReadMetaPMTCache[sec_offset];
		} else {
			pmt_block = PMT_INDICATOR_GET_BLOCK(param->u4PMTIndicator[cluster]);
			pmt_page = PMT_INDICATOR_GET_PAGE(param->u4PMTIndicator[cluster]);

			if (unlikely(pmt_block == 0)) {
				mt_ftl_err(dev, "pmt_block == 0");
				/* memset(param->u4ReadPMTCache, 0xFF, PM_PER_NANDPAGE * sizeof(unsigned int)); */
				pmt = 0xFFFFFFFF;
				meta_pmt = 0xFFFFFFFF;
			} else {
				mt_ftl_err(dev, "Get PMT of cluster (%d)", cluster);
				ret = ubi_leb_read(desc, pmt_block, (char *)param->u4ReadPMTCache,
						pmt_page * NAND_PAGE_SIZE, NAND_PAGE_SIZE, 0);
				if (ret && ret != UBI_IO_BITFLIPS) {
					mt_ftl_err(dev, "u4CurrentPMTLebPageIndicator = 0x%x",
						   param->u4CurrentPMTLebPageIndicator);
					mt_ftl_err(dev, "u4PMTIndicator[%d] = 0x%x", cluster,
						   param->u4PMTIndicator[cluster]);
					mt_ftl_err(dev,
						   "[GET PMT]ubi_leb_read PMT failed, leb%d, offset=%d, ret = 0x%x",
						   pmt_block, pmt_page * NAND_PAGE_SIZE, ret);
					return MT_FTL_FAIL;
				}
				ret = ubi_leb_read(desc, pmt_block, (char *)param->u4ReadMetaPMTCache,
					(pmt_page + 1) * NAND_PAGE_SIZE, NAND_PAGE_SIZE, 0);
				if (ret && ret != UBI_IO_BITFLIPS) {
					mt_ftl_err(dev, "u4CurrentPMTLebPageIndicator = 0x%x",
						   param->u4CurrentPMTLebPageIndicator);
					mt_ftl_err(dev, "u4PMTIndicator[%d] = 0x%x", cluster,
						   param->u4PMTIndicator[cluster]);
					mt_ftl_err(dev,
						   "[GET PMT]ubi_leb_read Meta PMT failed, leb%d, offset=%d, ret = 0x%x"
						   , pmt_block, (pmt_page + 1) * NAND_PAGE_SIZE, ret);
					return MT_FTL_FAIL;
				}
				param->i4CurrentReadPMTClusterInCache = cluster;
				pmt = param->u4ReadPMTCache[sec_offset];
				meta_pmt = param->u4ReadMetaPMTCache[sec_offset];
			}
		}

		leb = PMT_GET_BLOCK(pmt);
		page = PMT_GET_PAGE(pmt);
		part = PMT_GET_PART(meta_pmt);
		mt_ftl_err(dev,
			   "for sector (0x%lx), pmt = 0x%x, meta_pmt = 0x%x, leb = %d, page = %d, part = %d",
			   (unsigned long int)header_buffer->sector, pmt, meta_pmt, leb, page,
			   part);
		/*====================================================*/
		return MT_FTL_FAIL;
	}
#ifdef PROFILE
	end_time = getnstimenow();
	if (end_time >= start_time)
		profile_time[MT_FTL_PROFILE_READ_ADDRNOMATCH] += (end_time - start_time) / 1000;
	else {
		mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time, start_time);
		profile_time[MT_FTL_PROFILE_READ_ADDRNOMATCH] +=
		    (end_time + 0xFFFFFFFF - start_time) / 1000;
	}
	start_time = getnstimenow();
#endif				/* PROFILE */

	decmpr_len = NAND_PAGE_SIZE;
	ret =
	    crypto_comp_decompress(param->cc, page_buffer, (header_buffer->offset_len & 0xFFFF),
				   param->cmpr_page_buffer, &decmpr_len);
	if (ret) {
		ret = ubi_leb_read(desc, leb, (char *)param->tmp_page_buffer, page * NAND_PAGE_SIZE, NAND_PAGE_SIZE, 0);
		if (ret && ret != UBI_IO_BITFLIPS) {
			mt_ftl_err(dev,
				   "ubi_leb_read data_num failed, leb = %d, offset=%d ret = 0x%x",
				   leb, 0, ret);
			return MT_FTL_FAIL;
		}
		mt_ftl_err(dev, "part = %d", part);
		mt_ftl_err(dev, "0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x",
			   param->tmp_page_buffer[(NAND_PAGE_SIZE >> 2) -
						  ((part + 2) * sizeof(struct mt_ftl_data_header) +
						   1)],
			   param->tmp_page_buffer[(NAND_PAGE_SIZE >> 2) -
						  ((part + 2) * sizeof(struct mt_ftl_data_header) +
						   1) + 1],
			   param->tmp_page_buffer[(NAND_PAGE_SIZE >> 2) -
						  ((part + 2) * sizeof(struct mt_ftl_data_header) +
						   1) + 2],
			   param->tmp_page_buffer[(NAND_PAGE_SIZE >> 2) -
						  ((part + 2) * sizeof(struct mt_ftl_data_header) +
						   1) + 3],
			   param->tmp_page_buffer[(NAND_PAGE_SIZE >> 2) -
						  ((part + 2) * sizeof(struct mt_ftl_data_header) +
						   1) + 4],
			   param->tmp_page_buffer[(NAND_PAGE_SIZE >> 2) -
						  ((part + 2) * sizeof(struct mt_ftl_data_header) +
						   1) + 5],
			   param->tmp_page_buffer[(NAND_PAGE_SIZE >> 2) -
						  ((part + 2) * sizeof(struct mt_ftl_data_header) +
						   1) + 6]);
		mt_ftl_err(dev, "ret = %d, decmpr_len = %d, header_buffer->offset_len = 0x%x",
			   ret, decmpr_len, header_buffer->offset_len);
		mt_ftl_err(dev, "cc = 0x%lx, page_buffer = 0x%lx",
			   (unsigned long int)param->cc, (unsigned long int)page_buffer);
		mt_ftl_err(dev, "cmpr_page_buffer = 0x%lx", (unsigned long int)param->cmpr_page_buffer);
		return MT_FTL_FAIL;
	}
#ifdef PROFILE
	end_time = getnstimenow();
	if (end_time >= start_time)
		profile_time[MT_FTL_PROFILE_READ_DECOMP] += (end_time - start_time) / 1000;
	else {
		mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time, start_time);
		profile_time[MT_FTL_PROFILE_READ_DECOMP] +=
		    (end_time + 0xFFFFFFFF - start_time) / 1000;
	}
	start_time = getnstimenow();
#endif				/* PROFILE */

	offset_in_pagebuf = ((sector % (FS_PAGE_SIZE >> 9)) << 9);
	if ((offset_in_pagebuf + len) > decmpr_len) {
		mt_ftl_err(dev, "offset_in_pagebuf (%d) + len (%d) > decmpr_len (%d)",
			   offset_in_pagebuf, len, decmpr_len);
		return MT_FTL_FAIL;
	}
	/* mt_ftl_err(dev, "Copy to buffer"); */
	if (offset_in_pagebuf + len > NAND_PAGE_SIZE) {
		mt_ftl_err(dev, "offset_in_pagebuf(%d) + len(%d) > NAND_PAGE_SIZE(%d)",
			   offset_in_pagebuf, len, NAND_PAGE_SIZE);
	}
	memcpy((void *)buffer, &param->cmpr_page_buffer[offset_in_pagebuf], len);

#ifdef PROFILE
	end_time = getnstimenow();
	end_time_all = getnstimenow();
	if (end_time >= start_time)
		profile_time[MT_FTL_PROFILE_READ_COPYTOBUFF] += (end_time - start_time) / 1000;
	else {
		mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time, start_time);
		profile_time[MT_FTL_PROFILE_READ_COPYTOBUFF] +=
		    (end_time + 0xFFFFFFFF - start_time) / 1000;
	}
	if (end_time_all >= start_time_all)
		profile_time[MT_FTL_PROFILE_READ_ALL] += (end_time_all - start_time_all) / 1000;
	else {
		mt_ftl_err(dev, "end_time = %lu, start_time = %lu", end_time_all, start_time_all);
		profile_time[MT_FTL_PROFILE_READ_ALL] +=
		    (end_time_all + 0xFFFFFFFF - start_time_all) / 1000;
	}
	/* mt_ftl_err(dev, "write_page time = %lu", (end_time - start_time) / 1000); */
#endif				/* PROFILE */

	return ret;
}

int mt_ftl_replay_single_block(struct mt_ftl_blk *dev, int leb, int page)
{
	int ret = MT_FTL_SUCCESS, i = 0;
	int offset = 0;
	sector_t sector = 0;
	int cluster = 0, sec_offset = 0;
	unsigned int data_num = 0, data_hdr_offset = 0;
	struct mt_ftl_data_header *header_buffer = NULL;
	struct ubi_volume_desc *desc = dev->desc;
	struct mt_ftl_param *param = dev->param;
	const int max_offset_per_block = desc->vol->ubi->leb_size - NAND_PAGE_SIZE;

	ret = ubi_is_mapped(desc, leb);
	if (ret == 0) {
		mt_ftl_err(dev, "leb%d is unmapped", leb);
		return MT_FTL_FAIL;
	}
	/* Check if the block is PMT block
	 * If yes, check the correctness of param->u4CurrentPMTLebPageIndicator
	 */
	offset = page * NAND_PAGE_SIZE;
	ret = ubi_leb_read(desc, leb, (char *)param->general_page_buffer, offset, NAND_PAGE_SIZE, 0);
	if (ret && ret != UBI_IO_BITFLIPS) {
		mt_ftl_err(dev, "ubi_leb_read failed, leb = %d, offset=%d ret = 0x%x",
			   leb, offset, ret);
		return MT_FTL_FAIL;
	}

	data_num = PAGE_GET_DATA_NUM(param->general_page_buffer[(NAND_PAGE_SIZE >> 2) - 1]);
	if (data_num == 0x7FFFFFFF) {
		mt_ftl_err(dev, "End of block");
		return MT_FTL_SUCCESS;
	}
	data_hdr_offset = NAND_PAGE_SIZE - (data_num * sizeof(struct mt_ftl_data_header) + 4);
	if (NAND_PAGE_SIZE <= data_hdr_offset) {
		mt_ftl_err(dev, "NAND_PAGE_SIZE(%d) <= data_hdr_offset(%d)",
			   NAND_PAGE_SIZE, data_hdr_offset);
	}
	header_buffer =
	    (struct mt_ftl_data_header *)(&param->general_page_buffer[data_hdr_offset >> 2]);

	mt_ftl_err(dev, "leb = %d, page = %d, data_num = %d", leb, page, data_num);
	while (data_num && (offset <= max_offset_per_block)) {
		/* Update param->u4NextLebPageIndicator
		 * check the correctness of param->u4NextLebPageIndicator &
		 * if param->u4NextLebPageIndicator is full, need to call get free block & page function
		 */

		/* Update param->u4PMTCache and param->u4PMTIndicator and param->u4BIT */
		/* If the page is copied in GC, that means the page should not be replayed */
		if (PAGE_BEEN_READ(param->general_page_buffer[(NAND_PAGE_SIZE >> 2) - 1]) == 0) {
			for (i = 0; i < data_num; i++) {
				/* Get sector in the page */
				sector = header_buffer[data_num - i - 1].sector;
				if (sector == NAND_DEFAULT_VALUE) {
					mt_ftl_err(dev,
						   "header_buffer[%d].sector == 0xFFFFFFFF, leb = %d, page = %d",
						   i, leb, offset / NAND_PAGE_SIZE);
					continue;
				}

				/* Calculate clusters and sec_offsets */
				cluster =
				    ((unsigned long int)sector / (FS_PAGE_SIZE >> 9)) /
				    PM_PER_NANDPAGE;
				sec_offset =
				    ((unsigned long int)sector /
				     (FS_PAGE_SIZE >> 9)) & (PM_PER_NANDPAGE - 1);

				mt_ftl_updatePMT(dev, cluster, sec_offset, leb, offset, i,
						 (header_buffer[data_num - i - 1].offset_len &
						  0xFFFF), 1, 1);
			}

			offset += NAND_PAGE_SIZE;
			PMT_LEB_PAGE_INDICATOR_SET_BLOCKPAGE(param->u4NextLebPageIndicator, leb,
							     offset / NAND_PAGE_SIZE);
			/* mt_ftl_err(dev, "u4NextLebPageIndicator = 0x%x", param->u4NextLebPageIndicator);     */
		} else {
			mt_ftl_err(dev, "This page has been read in gc, page = %d",
				   offset / NAND_PAGE_SIZE);
			offset += NAND_PAGE_SIZE;
			PMT_LEB_PAGE_INDICATOR_SET_BLOCKPAGE(param->u4NextLebPageIndicator, leb,
							     offset / NAND_PAGE_SIZE);
		}

		if (offset > max_offset_per_block)
			break;

		ret = ubi_leb_read(desc, leb, (char *)param->general_page_buffer, offset, NAND_PAGE_SIZE, 0);
		if (ret && ret != UBI_IO_BITFLIPS) {
			mt_ftl_err(dev,
				   "ubi_leb_read failed, leb = %d, offset=%d ret = 0x%x",
				   leb, offset, ret);
			return MT_FTL_FAIL;
		}

		data_num = PAGE_GET_DATA_NUM(param->general_page_buffer[(NAND_PAGE_SIZE >> 2) - 1]);
		data_hdr_offset =
		    NAND_PAGE_SIZE - (data_num * sizeof(struct mt_ftl_data_header) + 4);
		if (NAND_PAGE_SIZE <= data_hdr_offset) {
			mt_ftl_err(dev, "NAND_PAGE_SIZE(%d) <= data_hdr_offset(%d)",
				   NAND_PAGE_SIZE, data_hdr_offset);
		}
		header_buffer =
		    (struct mt_ftl_data_header
		     *)(&param->general_page_buffer[data_hdr_offset >> 2]);

		if (data_num == 0x7FFFFFFF)
			break;
	}

	/* mt_ftl_err(dev, "offset = %d at the end, max_offset_per_block = %d", offset,
		   max_offset_per_block); */

	return ret;
}

int mt_ftl_replay(struct mt_ftl_blk *dev)
{
	int ret = MT_FTL_SUCCESS;
	int leb = 0, page = 0, offset = 0;
	int nextleb_in_replay = MT_INVALID_BLOCKPAGE;

	struct ubi_volume_desc *desc = dev->desc;
	struct mt_ftl_param *param = dev->param;

	const int max_offset_per_block = desc->vol->ubi->leb_size - NAND_PAGE_SIZE;
	/* const int page_num_per_block = desc->vol->ubi->leb_size / NAND_PAGE_SIZE; */

	param->replay_blk_index = 0;
	/* Replay leb/page of param->u4NextLebPageIndicator */
	leb = PMT_LEB_PAGE_INDICATOR_GET_BLOCK(param->u4NextLebPageIndicator);
	page = PMT_LEB_PAGE_INDICATOR_GET_PAGE(param->u4NextLebPageIndicator);

	param->replay_blk_rec[param->replay_blk_index] = leb;
	param->replay_blk_index++;
	ret = mt_ftl_replay_single_block(dev, leb, page);
	if (ret)
		return ret;

	/* Get the successive lebs to replay */
	ret = ubi_is_mapped(desc, REPLAY_BLOCK);
	if (ret == 0) {
		mt_ftl_err(dev, "leb%d is unmapped", REPLAY_BLOCK);
		return MT_FTL_FAIL;
	}
	ret = ubi_leb_read(desc, REPLAY_BLOCK, (char *)param->replay_page_buffer, offset, sizeof(unsigned int) * 2, 0);
	if (ret && ret != UBI_IO_BITFLIPS) {
		mt_ftl_err(dev, "ubi_leb_read (leb%d, offset=%d) failed, ret = 0x%x",
			   REPLAY_BLOCK, offset, ret);
		return MT_FTL_FAIL;
	}
#ifdef MTK_FTL_DEBUG
	mt_ftl_err(dev, "replay_page_buffer[0] = 0x%x, replay_page_buffer[1] = 0x%x, offset = %d",
		   param->replay_page_buffer[0], param->replay_page_buffer[1], offset);
#endif

	if (param->replay_page_buffer[0] == MT_MAGIC_NUMBER) {
		/* If the 1st leb is the same as leb of param->u4NextLebPageIndicator
		 * Get the next leb to replay */
		if (leb == param->replay_page_buffer[1]) {
			offset += NAND_PAGE_SIZE;
			ret = ubi_leb_read(desc, REPLAY_BLOCK, (char *)param->replay_page_buffer, offset,
					  sizeof(unsigned int) * 2, 0);
			if (ret && ret != UBI_IO_BITFLIPS) {
				mt_ftl_err(dev, "ubi_leb_read (leb%d, offset=%d) failed, ret = 0x%x",
					   REPLAY_BLOCK, offset, ret);
				return MT_FTL_FAIL;
			}
#ifdef MTK_FTL_DEBUG
			mt_ftl_err(dev,
				   "replay_page_buffer[0] = 0x%x, replay_page_buffer[1] = 0x%x, offset = %d",
				   param->replay_page_buffer[0], param->replay_page_buffer[1],
				   offset);
#endif
		}

		if (param->replay_page_buffer[0] == MT_MAGIC_NUMBER) {
			nextleb_in_replay = param->replay_page_buffer[1];
			leb = mt_ftl_getfreeblock(dev, &page, false, true);
			if (leb != nextleb_in_replay) {
				mt_ftl_err(dev, "leb(%d) != nextleb_in_replay(%d)", leb,
					   nextleb_in_replay);
				return MT_FTL_FAIL;
			}
		}
	}

	/* TODO: Consider the case - if replay items are more than the page number per block */
	while (param->replay_page_buffer[0] == MT_MAGIC_NUMBER) {
		/* Get block num info in the page */
		leb = nextleb_in_replay;

		if (NAND_PAGE_NUM_PER_BLOCK <= param->replay_blk_index) {
			mt_ftl_err(dev,
				   "NAND_PAGE_NUM_PER_BLOCK(%d) <= param->replay_blk_index(%d)",
				   NAND_PAGE_NUM_PER_BLOCK, param->replay_blk_index);
		}
		ret = mt_ftl_replay_single_block(dev, leb, 0);
		if (ret)
			return ret;

		offset += NAND_PAGE_SIZE;
		if (offset >= max_offset_per_block)
			break;

		ret = ubi_leb_read(desc, REPLAY_BLOCK, (char *)param->replay_page_buffer,
				offset, sizeof(unsigned int) * 2, 0);
		if (ret && ret != UBI_IO_BITFLIPS) {
			mt_ftl_err(dev, "ubi_leb_read (leb%d, offset=%d) failed, ret = 0x%x",
				   REPLAY_BLOCK, offset, ret);
			return MT_FTL_FAIL;
		}
#ifdef MTK_FTL_DEBUG
		mt_ftl_err(dev,
			   "replay_page_buffer[0] = 0x%x, replay_page_buffer[1] = 0x%x, offset = %d",
			   param->replay_page_buffer[0], param->replay_page_buffer[1], offset);
#endif
		if (param->replay_page_buffer[0] == MT_MAGIC_NUMBER) {
			nextleb_in_replay = param->replay_page_buffer[1];
			leb = mt_ftl_getfreeblock(dev, &page, false, true);
			if (leb != nextleb_in_replay) {
				mt_ftl_err(dev, "leb(%d) != nextleb_in_replay(%d)", leb,
					   nextleb_in_replay);
				return MT_FTL_FAIL;
			}
		}
	}

	param->u4NextReplayOffsetIndicator = offset;

	/* mt_ftl_err(dev, "u4DataNum = %d (Suppose to be 0)", param->u4DataNum); */

	return ret;
}

int mt_ftl_alloc_single_buffer(unsigned int **buf, int size, char *str)
{
	if (*buf == NULL) {
		*buf = kzalloc(size, GFP_KERNEL);
		if (!*buf) {
			mt_ftl_err(buf, "%s allocate memory fail", str);
			return -ENOMEM;
		}
	}
	memset(*buf, 0xFF, size);

	return 0;
}

int mt_ftl_alloc_buffers(struct mt_ftl_param *param)
{
	int ret = MT_FTL_SUCCESS;

	ret = mt_ftl_alloc_single_buffer(&param->u4PMTIndicator,
					 PMT_TOTAL_CLUSTER_NUM * sizeof(unsigned int),
					 "param->u4PMTIndicator");
	if (ret)
		return ret;

	ret = mt_ftl_alloc_single_buffer(&param->u4PMTCache,
					 PM_PER_NANDPAGE * PMT_CACHE_NUM * sizeof(unsigned int),
					 "param->u4PMTCache");
	if (ret)
		return ret;

	ret = mt_ftl_alloc_single_buffer(&param->u4MetaPMTCache,
					 PM_PER_NANDPAGE * PMT_CACHE_NUM * sizeof(unsigned int),
					 "param->u4MetaPMTCache");
	if (ret)
		return ret;

	ret = mt_ftl_alloc_single_buffer((unsigned int **)&param->i4CurrentPMTClusterInCache,
					 PMT_CACHE_NUM * sizeof(unsigned int),
					 "param->i4CurrentPMTClusterInCache");
	if (ret)
		return ret;

	ret = mt_ftl_alloc_single_buffer(&param->u4ReadPMTCache,
					 PM_PER_NANDPAGE * sizeof(unsigned int),
					 "param->u4ReadPMTCache");
	if (ret)
		return ret;

	ret = mt_ftl_alloc_single_buffer(&param->u4ReadMetaPMTCache,
					 PM_PER_NANDPAGE * sizeof(unsigned int),
					 "param->u4ReadMetaPMTCache");
	if (ret)
		return ret;

	ret = mt_ftl_alloc_single_buffer(&param->u4BIT,
					 NAND_TOTAL_BLOCK_NUM * sizeof(unsigned int),
					 "param->u4BIT");
	if (ret)
		return ret;

	ret = mt_ftl_alloc_single_buffer((unsigned int **)&param->u1DataCache,
					 NAND_PAGE_SIZE * sizeof(unsigned char),
					 "param->u1DataCache");
	if (ret)
		return ret;

	ret = mt_ftl_alloc_single_buffer((unsigned int **)&param->u4Header,
					 MTKFTL_MAX_DATA_NUM_PER_PAGE *
					 sizeof(struct mt_ftl_data_header), "param->u4Header");
	if (ret)
		return ret;

	ret = mt_ftl_alloc_single_buffer((unsigned int **)&param->u4ReadHeader,
					 MTKFTL_MAX_DATA_NUM_PER_PAGE *
					 sizeof(struct mt_ftl_data_header), "param->u4ReadHeader");
	if (ret)
		return ret;

	ret = mt_ftl_alloc_single_buffer(&param->replay_blk_rec,
					 NAND_PAGE_NUM_PER_BLOCK * sizeof(unsigned int),
					 "param->replay_blk_rec");
	if (ret)
		return ret;

	ret = mt_ftl_alloc_single_buffer(&param->general_page_buffer,
					 (NAND_PAGE_SIZE >> 2) * sizeof(unsigned int),
					 "param->general_page_buffer");
	if (ret)
		return ret;

	ret = mt_ftl_alloc_single_buffer(&param->replay_page_buffer,
					 (NAND_PAGE_SIZE >> 2) * sizeof(unsigned int),
					 "param->replay_page_buffer");
	if (ret)
		return ret;

	ret = mt_ftl_alloc_single_buffer(&param->commit_page_buffer,
					 (NAND_PAGE_SIZE >> 2) * sizeof(unsigned int),
					 "param->commit_page_buffer");
	if (ret)
		return ret;

	ret = mt_ftl_alloc_single_buffer(&param->gc_page_buffer,
					 (NAND_PAGE_SIZE >> 2) * sizeof(unsigned int),
					 "param->gc_page_buffer");
	if (ret)
		return ret;

	ret = mt_ftl_alloc_single_buffer((unsigned int **)&param->cmpr_page_buffer,
					 NAND_PAGE_SIZE * sizeof(unsigned char),
					 "param->cmpr_page_buffer");
	if (ret)
		return ret;

	ret = mt_ftl_alloc_single_buffer(&param->tmp_page_buffer,
					 (NAND_PAGE_SIZE >> 2) * sizeof(unsigned int),
					 "param->tmp_page_buffer");
	if (ret)
		return ret;

	return ret;
}

static int mt_ftl_check_img_reload(struct mt_ftl_blk *dev, int leb)
{
	int ret = MT_FTL_SUCCESS;
	struct ubi_volume_desc *desc = dev->desc;
	struct mt_ftl_param *param = dev->param;
	int offset = 0;
	const int max_offset_per_block = desc->vol->ubi->leb_size - NAND_PAGE_SIZE;

	ret = ubi_is_mapped(desc, leb);
	if (ret == 0) {
		mt_ftl_err(dev, "leb%d is unmapped", leb);
		return MT_FTL_FAIL;
	}
	while (offset <= max_offset_per_block) {
		ret = ubi_leb_read(desc, leb, (char *)param->tmp_page_buffer, offset, NAND_PAGE_SIZE, 0);
		if (ret && ret != UBI_IO_BITFLIPS) {
			mt_ftl_err(dev, "ubi_leb_read (leb%d, offset=%d) failed, ret = 0x%x",
				   leb, offset, ret);
			return MT_FTL_FAIL;
		}
		if (param->tmp_page_buffer[0] == 0x00000000) {
			mt_ftl_err(dev, "image reloaded, offset = %d", offset);
			return 1;
		}
		offset += NAND_PAGE_SIZE;
	}

	/* mt_ftl_err(dev, "image not reloaded offset = %d", offset); */
	return 0;
}

static int mt_ftl_recover_blk(struct mt_ftl_blk *dev)
{
	int ret = MT_FTL_SUCCESS, i = 0;
	int offset = 0;
	struct ubi_volume_desc *desc = dev->desc;
	struct mt_ftl_param *param = dev->param;
	const int max_offset_per_block = desc->vol->ubi->leb_size - NAND_PAGE_SIZE;
	/* Recover Config Block */
	ret = ubi_leb_read(desc, CONFIG_START_BLOCK, (char *)param->general_page_buffer, offset, NAND_PAGE_SIZE, 0);
	if (ret && ret != UBI_IO_BITFLIPS) {
		mt_ftl_err(dev, "ubi_leb_read (leb%d, offset=%d) failed, ret = 0x%x",
			   CONFIG_START_BLOCK, offset, ret);
		return MT_FTL_FAIL;
	}
	mt_ftl_err(dev, "param->general_page_buffer[0] = 0x%x, MT_MAGIC_NUMBER = 0x%x",
		   param->general_page_buffer[0], MT_MAGIC_NUMBER);
	ubi_leb_unmap(desc, CONFIG_START_BLOCK);
	ubi_leb_map(desc, CONFIG_START_BLOCK);
	mt_ftl_write_to_blk(dev, CONFIG_START_BLOCK, param->general_page_buffer);

	/* Recover Backup Config Block */
	ubi_leb_unmap(desc, CONFIG_START_BLOCK + 1);
	ubi_leb_map(desc, CONFIG_START_BLOCK + 1);

	/* Recover Replay Blocks */
	ubi_leb_unmap(desc, REPLAY_BLOCK);
	ubi_leb_map(desc, REPLAY_BLOCK);

	/* Recover PMT Blocks */
	for (i = PMT_START_BLOCK + 1; i < PMT_START_BLOCK + PMT_BLOCK_NUM; i++) {
		ubi_leb_unmap(desc, i);
		ubi_leb_map(desc, i);
	}
	offset = 2 * NAND_PAGE_SIZE;
	while (offset <= max_offset_per_block) {
		ret = ubi_leb_read(desc, PMT_START_BLOCK, (char *)param->general_page_buffer,
				offset, NAND_PAGE_SIZE, 0);
		if (ret && ret != UBI_IO_BITFLIPS) {
			mt_ftl_err(dev, "ubi_leb_read (leb%d, offset=%d) failed, ret = 0x%x",
				   PMT_START_BLOCK, offset, ret);
			return MT_FTL_FAIL;
		}
		if (param->general_page_buffer[0] == 0x00000000) {
			mt_ftl_err(dev, "offset = %d, page = %d", offset, offset / NAND_PAGE_SIZE);
			break;
		}
		ret = ubi_leb_write(desc, PMT_START_BLOCK + 1, param->general_page_buffer, offset, NAND_PAGE_SIZE);
		if (ret) {
			mt_ftl_err(dev, "ubi_leb_write (leb%d, offset=%d) failed, ret = 0x%x",
				   PMT_START_BLOCK + 1, offset, ret);
			return MT_FTL_FAIL;
		}
		offset += NAND_PAGE_SIZE;
	}
	ubi_leb_unmap(desc, PMT_START_BLOCK);
	ubi_leb_map(desc, PMT_START_BLOCK);
	offset = 2 * NAND_PAGE_SIZE;
	while (offset <= max_offset_per_block) {
		ret = ubi_leb_read(desc, PMT_START_BLOCK + 1, (char *)param->general_page_buffer, offset,
			NAND_PAGE_SIZE, 0);
		if (ret && ret != UBI_IO_BITFLIPS) {
			mt_ftl_err(dev, "ubi_leb_read (leb%d, offset=%d) failed, ret = 0x%x",
				   PMT_START_BLOCK + 1, offset, ret);
			return MT_FTL_FAIL;
		}
		if (param->general_page_buffer[0] == 0xFFFFFFFF) {
			mt_ftl_err(dev, "offset = %d, page = %d", offset, offset / NAND_PAGE_SIZE);
			break;
		}
		ret = ubi_leb_write(desc, PMT_START_BLOCK, param->general_page_buffer, offset, NAND_PAGE_SIZE);
		if (ret) {
			mt_ftl_err(dev, "ubi_leb_write (leb%d, offset=%d) failed, ret = 0x%x",
				   PMT_START_BLOCK, offset, ret);
			return MT_FTL_FAIL;
		}
		offset += NAND_PAGE_SIZE;
	}
	ubi_leb_unmap(desc, PMT_START_BLOCK + 1);
	ubi_leb_map(desc, PMT_START_BLOCK + 1);

	return ret;
}

static int mt_ftl_show_param(struct mt_ftl_blk *dev)
{
	struct mt_ftl_param *param = dev->param;

	mt_ftl_err(dev, "u4NextReplayOffsetIndicator = 0x%x", param->u4NextReplayOffsetIndicator);
	mt_ftl_err(dev, "u4NextLebPageIndicator = 0x%x", param->u4NextLebPageIndicator);
	mt_ftl_err(dev, "u4CurrentPMTLebPageIndicator = 0x%x", param->u4CurrentPMTLebPageIndicator);
	mt_ftl_err(dev, "u4NextFreeLebIndicator = 0x%x", param->u4NextFreeLebIndicator);
	mt_ftl_err(dev, "u4NextFreePMTLebIndicator = 0x%x", param->u4NextFreePMTLebIndicator);
	mt_ftl_err(dev, "u4GCReserveLeb = 0x%x", param->u4GCReserveLeb);
	mt_ftl_err(dev, "u4GCReservePMTLeb = 0x%x", param->u4GCReservePMTLeb);
	mt_ftl_err(dev, "u4PMTIndicator = 0x%x, 0x%x, 0x%x, 0x%x",
		   param->u4PMTIndicator[0],
		   param->u4PMTIndicator[1], param->u4PMTIndicator[2], param->u4PMTIndicator[3]);
	mt_ftl_err(dev, "u4BIT = 0x%x, 0x%x, 0x%x, 0x%x",
		   param->u4BIT[0], param->u4BIT[1], param->u4BIT[2], param->u4BIT[3]);
	mt_ftl_err(dev, "i4CurrentPMTClusterInCache = 0x%x, 0x%x, 0x%x, 0x%x",
		   param->i4CurrentPMTClusterInCache[0],
		   param->i4CurrentPMTClusterInCache[1],
		   param->i4CurrentPMTClusterInCache[2], param->i4CurrentPMTClusterInCache[3]);

	return MT_FTL_SUCCESS;
}

/* To Do*/
int mt_ftl_discard(struct mt_ftl_blk *dev, unsigned long sector, unsigned nr_sects)
{
	int ret = MT_FTL_SUCCESS, i = 0;
	struct ubi_volume_desc *desc = dev->desc;
	struct mt_ftl_param *param = dev->param;

	param->u4DataNum = 0;
	param->replay_blk_index = 0;
	param->i4CurrentReadPMTClusterInCache = 0xFFFFFFFF;
	param->u4NextReplayOffsetIndicator = 0;

	/* There are some download information stored in some blocks
	   So unmap the blocks at first */
	for (i = 0; i < desc->vol->ubi->volumes[dev->vol_id]->reserved_pebs; i++)
		ubi_leb_unmap(desc, i);

	ret = ubi_is_mapped(desc, CONFIG_START_BLOCK);
	if (ret == 0)
		ubi_leb_map(desc, CONFIG_START_BLOCK);

	ret = ubi_is_mapped(desc, CONFIG_START_BLOCK + 1);
	if (ret == 0)
		ubi_leb_map(desc, CONFIG_START_BLOCK + 1);

	ret = ubi_is_mapped(desc, REPLAY_BLOCK);
	if (ret == 0)
		ubi_leb_map(desc, REPLAY_BLOCK);

	PMT_LEB_PAGE_INDICATOR_SET_BLOCKPAGE(param->u4NextLebPageIndicator, DATA_START_BLOCK, 0);
	mt_ftl_err(dev, "u4NextLebPageIndicator = 0x%x", param->u4NextLebPageIndicator);
	ret = ubi_is_mapped(desc, DATA_START_BLOCK);
	if (ret == 0)
		ubi_leb_map(desc, DATA_START_BLOCK);

	PMT_LEB_PAGE_INDICATOR_SET_BLOCKPAGE(param->u4CurrentPMTLebPageIndicator, PMT_START_BLOCK,
					     0);
	mt_ftl_err(dev, "u4CurrentPMTLebPageIndicator = 0x%x", param->u4CurrentPMTLebPageIndicator);
	ret = ubi_is_mapped(desc, PMT_START_BLOCK);
	if (ret == 0)
		ubi_leb_map(desc, PMT_START_BLOCK);

	param->u4NextFreeLebIndicator = DATA_START_BLOCK + 1;
	mt_ftl_err(dev, "u4NextFreeLebIndicator = 0x%x", param->u4NextFreeLebIndicator);
	param->u4NextFreePMTLebIndicator = PMT_START_BLOCK + 1;
	mt_ftl_err(dev, "u4NextFreePMTLebIndicator = %d", param->u4NextFreePMTLebIndicator);
	param->u4GCReserveLeb = desc->vol->ubi->volumes[dev->vol_id]->reserved_pebs - 1;
	mt_ftl_err(dev, "u4GCReserveLeb = %d", param->u4GCReserveLeb);
	ret = ubi_is_mapped(desc, param->u4GCReserveLeb);
	if (ret == 0)
		ubi_leb_map(desc, param->u4GCReserveLeb);
	param->u4GCReservePMTLeb = DATA_START_BLOCK - 1;
	ret = ubi_is_mapped(desc, param->u4GCReservePMTLeb);
	if (ret == 0)
		ubi_leb_map(desc, param->u4GCReservePMTLeb);
	memset(param->u4PMTIndicator, 0, PMT_TOTAL_CLUSTER_NUM * sizeof(unsigned int));
	memset(param->u4BIT, 0, NAND_TOTAL_BLOCK_NUM * sizeof(unsigned int));
	/* add memory reset */
	memset(param->u4PMTCache, 0xFF, PM_PER_NANDPAGE * PMT_CACHE_NUM * sizeof(unsigned int));
	memset(param->u4MetaPMTCache, 0xFF, PM_PER_NANDPAGE * PMT_CACHE_NUM * sizeof(unsigned int));
	memset(param->i4CurrentPMTClusterInCache, 0xFF, PMT_CACHE_NUM * sizeof(unsigned int));
	memset(param->u4ReadPMTCache, 0xFF, PM_PER_NANDPAGE * sizeof(unsigned int));
	memset(param->u4ReadMetaPMTCache, 0xFF, PM_PER_NANDPAGE * sizeof(unsigned int));
	memset(param->u1DataCache, 0xFF, NAND_PAGE_SIZE * sizeof(unsigned char));
	memset(param->u4Header, 0xFF, MTKFTL_MAX_DATA_NUM_PER_PAGE*sizeof(struct mt_ftl_data_header));
	memset(param->u4ReadHeader, 0xFF, MTKFTL_MAX_DATA_NUM_PER_PAGE*sizeof(struct mt_ftl_data_header));
	memset(param->replay_blk_rec, 0xFF, NAND_PAGE_NUM_PER_BLOCK * sizeof(unsigned int));
	memset(param->general_page_buffer, 0xFF, (NAND_PAGE_SIZE >> 2) * sizeof(unsigned int));
	memset(param->replay_page_buffer, 0xFF, (NAND_PAGE_SIZE >> 2) * sizeof(unsigned int));
	memset(param->commit_page_buffer, 0xFF, (NAND_PAGE_SIZE >> 2) * sizeof(unsigned int));
	memset(param->gc_page_buffer, 0xFF, (NAND_PAGE_SIZE >> 2) * sizeof(unsigned int));
	memset(param->cmpr_page_buffer, 0xFF, NAND_PAGE_SIZE * sizeof(unsigned char));
	memset(param->tmp_page_buffer, 0xFF, (NAND_PAGE_SIZE >> 2) * sizeof(unsigned int));

	mt_ftl_show_param(dev);
	return 0;
}

static int mt_ftl_param_default(struct mt_ftl_blk *dev)
{
	int ret = MT_FTL_SUCCESS, i = 0;
	struct ubi_volume_desc *desc = dev->desc;
	struct mt_ftl_param *param = dev->param;

	param->u4DataNum = 0;
	param->replay_blk_index = 0;
	param->i4CurrentReadPMTClusterInCache = 0xFFFFFFFF;
	param->cc = crypto_alloc_comp("lzo", 0, 0);
	param->u4NextReplayOffsetIndicator = 0;

	/* There are some download information stored in some blocks
	   So unmap the blocks at first */
	for (i = 0; i < desc->vol->ubi->volumes[dev->vol_id]->reserved_pebs; i++)
		ubi_leb_unmap(desc, i);

	ret = ubi_is_mapped(desc, CONFIG_START_BLOCK);
	if (ret == 0)
		ubi_leb_map(desc, CONFIG_START_BLOCK);

	ret = ubi_is_mapped(desc, CONFIG_START_BLOCK + 1);
	if (ret == 0)
		ubi_leb_map(desc, CONFIG_START_BLOCK + 1);

	ret = ubi_is_mapped(desc, REPLAY_BLOCK);
	if (ret == 0)
		ubi_leb_map(desc, REPLAY_BLOCK);

	PMT_LEB_PAGE_INDICATOR_SET_BLOCKPAGE(param->u4NextLebPageIndicator, DATA_START_BLOCK, 0);
	mt_ftl_err(dev, "u4NextLebPageIndicator = 0x%x", param->u4NextLebPageIndicator);
	ret = ubi_is_mapped(desc, DATA_START_BLOCK);
	if (ret == 0)
		ubi_leb_map(desc, DATA_START_BLOCK);

	PMT_LEB_PAGE_INDICATOR_SET_BLOCKPAGE(param->u4CurrentPMTLebPageIndicator, PMT_START_BLOCK,
					     0);
	mt_ftl_err(dev, "u4CurrentPMTLebPageIndicator = 0x%x", param->u4CurrentPMTLebPageIndicator);
	ret = ubi_is_mapped(desc, PMT_START_BLOCK);
	if (ret == 0)
		ubi_leb_map(desc, PMT_START_BLOCK);

	param->u4NextFreeLebIndicator = DATA_START_BLOCK + 1;
	mt_ftl_err(dev, "u4NextFreeLebIndicator = 0x%x", param->u4NextFreeLebIndicator);
	param->u4NextFreePMTLebIndicator = PMT_START_BLOCK + 1;
	mt_ftl_err(dev, "u4NextFreePMTLebIndicator = %d", param->u4NextFreePMTLebIndicator);
	param->u4GCReserveLeb = desc->vol->ubi->volumes[dev->vol_id]->reserved_pebs - 1;
	mt_ftl_err(dev, "u4GCReserveLeb = %d", param->u4GCReserveLeb);
	ret = ubi_is_mapped(desc, param->u4GCReserveLeb);
	if (ret == 0)
		ubi_leb_map(desc, param->u4GCReserveLeb);
	param->u4GCReservePMTLeb = DATA_START_BLOCK - 1;
	ret = ubi_is_mapped(desc, param->u4GCReservePMTLeb);
	if (ret == 0)
		ubi_leb_map(desc, param->u4GCReservePMTLeb);
	memset(param->u4PMTIndicator, 0, PMT_TOTAL_CLUSTER_NUM * sizeof(unsigned int));
	memset(param->u4BIT, 0, NAND_TOTAL_BLOCK_NUM * sizeof(unsigned int));

	mt_ftl_show_param(dev);

	/* Replay */
	ret = mt_ftl_replay(dev);
	if (ret) {
		mt_ftl_err(dev, "mt_ftl_replay fail, ret = %d", ret);
		return ret;
	}

	mt_ftl_show_param(dev);

	/* Commit indicators */
	mt_ftl_commit_indicators(dev);
	if (ret) {
		mt_ftl_err(dev, "mt_ftl_commit_indicators fail, ret = %d", ret);
		return ret;
	}

	return ret;
}

static int mt_ftl_param_init(struct mt_ftl_blk *dev, unsigned int *buffer)
{
	int ret = MT_FTL_SUCCESS;
	int index = 0;
	struct ubi_volume_desc *desc = dev->desc;
	struct mt_ftl_param *param = dev->param;

	param->u4DataNum = 0;
	param->replay_blk_index = 0;
	param->i4CurrentReadPMTClusterInCache = 0xFFFFFFFF;
	param->cc = crypto_alloc_comp("lzo", 0, 0);

	param->u4NextReplayOffsetIndicator = buffer[1];
	param->u4NextLebPageIndicator = buffer[2];
	param->u4CurrentPMTLebPageIndicator = buffer[3];
	param->u4NextFreeLebIndicator = buffer[4];
	param->u4NextFreePMTLebIndicator = buffer[5];
	param->u4GCReserveLeb = buffer[6];
	if (param->u4GCReserveLeb == NAND_DEFAULT_VALUE)
		param->u4GCReserveLeb = desc->vol->ubi->volumes[dev->vol_id]->reserved_pebs - 1;
	param->u4GCReservePMTLeb = buffer[7];
	if (param->u4GCReservePMTLeb == NAND_DEFAULT_VALUE)
		param->u4GCReservePMTLeb = DATA_START_BLOCK - 1;

	index = 8;
	if ((index + PMT_TOTAL_CLUSTER_NUM) * sizeof(unsigned int) > NAND_PAGE_SIZE) {
		mt_ftl_err(dev,
			   "(index + PMT_TOTAL_CLUSTER_NUM) * sizeof(unsigned int)(0x%lx) > NAND_PAGE_SIZE(%d)",
			   (index + PMT_TOTAL_CLUSTER_NUM) * sizeof(unsigned int), NAND_PAGE_SIZE);
		mt_ftl_err(dev, "index = %d, PMT_TOTAL_CLUSTER_NUM = %d", index,
			   PMT_TOTAL_CLUSTER_NUM);
		return MT_FTL_FAIL;
	}
	memcpy(param->u4PMTIndicator, &buffer[index], PMT_TOTAL_CLUSTER_NUM * sizeof(unsigned int));

	index += ((PMT_TOTAL_CLUSTER_NUM * sizeof(unsigned int)) >> 2);
	if ((index + NAND_TOTAL_BLOCK_NUM) * sizeof(unsigned int) > NAND_PAGE_SIZE) {
		mt_ftl_err(dev,
			   "(index + NAND_TOTAL_BLOCK_NUM) * sizeof(unsigned int)(0x%lx) > NAND_PAGE_SIZE(%d)",
			   (index + NAND_TOTAL_BLOCK_NUM) * sizeof(unsigned int), NAND_PAGE_SIZE);
		mt_ftl_err(dev, "index = %d, NAND_TOTAL_BLOCK_NUM = %d", index,
			   NAND_TOTAL_BLOCK_NUM);
		return MT_FTL_FAIL;
	}
	memcpy(param->u4BIT, &buffer[index], NAND_TOTAL_BLOCK_NUM * sizeof(unsigned int));

	index += ((NAND_TOTAL_BLOCK_NUM * sizeof(unsigned int)) >> 2);
	if ((index + PMT_CACHE_NUM) * sizeof(unsigned int) > NAND_PAGE_SIZE) {
		mt_ftl_err(dev,
			   "(index + PMT_CACHE_NUM) * sizeof(unsigned int)(0x%lx) > NAND_PAGE_SIZE(%d)",
			   (index + PMT_CACHE_NUM) * sizeof(unsigned int), NAND_PAGE_SIZE);
		mt_ftl_err(dev, "index = %d, PMT_CACHE_NUM = %d", index, PMT_CACHE_NUM);
		return MT_FTL_FAIL;
	}
	memcpy(param->i4CurrentPMTClusterInCache, &buffer[index],
	       PMT_CACHE_NUM * sizeof(unsigned int));

	mt_ftl_show_param(dev);

	/* Replay */
	ret = mt_ftl_replay(dev);
	if (ret) {
		mt_ftl_err(dev, "mt_ftl_replay fail, ret = %d", ret);
		return ret;
	}

	mt_ftl_show_param(dev);

	return ret;
}

int mt_ftl_create(struct mt_ftl_blk *dev)
{
	int ret = MT_FTL_SUCCESS;
	int leb = 0;
	int offset = 0;
	int img_reload = 0;
	struct ubi_volume_desc *desc = dev->desc;
	struct mt_ftl_param *param = dev->param;

#ifdef PROFILE
	memset(profile_time, 0, sizeof(profile_time));
#endif

	/* Allocate buffers for FTL usage */
	ret = mt_ftl_alloc_buffers(param);
	if (ret)
		return ret;

	/* Check the mapping of CONFIG and REPLAY block */
	ret = ubi_is_mapped(desc, CONFIG_START_BLOCK);
	if (ret == 0) {
		ret = ubi_is_mapped(desc, CONFIG_START_BLOCK + 1);
		if (ret)
			leb = CONFIG_START_BLOCK + 1;
		else {
			mt_ftl_err(dev, "leb%d/leb%d are both unmapped", CONFIG_START_BLOCK,
				   CONFIG_START_BLOCK + 1);
			ubi_leb_map(desc, CONFIG_START_BLOCK);
			ubi_leb_map(desc, CONFIG_START_BLOCK + 1);
			leb = CONFIG_START_BLOCK;
		}
	} else {
		leb = CONFIG_START_BLOCK;
	}

	ret = ubi_is_mapped(desc, REPLAY_BLOCK);
	if (ret == 0)
		ubi_leb_map(desc, REPLAY_BLOCK);

	/* Check if system.img/usrdata.img is just reloaded */
	img_reload = mt_ftl_check_img_reload(dev, CONFIG_START_BLOCK);
	if (img_reload < 0) {
		mt_ftl_err(dev, "mt_ftl_check_img_reload fail, ret = %d", img_reload);
		return MT_FTL_FAIL;
	}

	if (img_reload) {
		mt_ftl_err(dev, "system or usrdata image is reloaded");
		ret = mt_ftl_recover_blk(dev);
		if (ret) {
			mt_ftl_err(dev, "recover block fail");
			return ret;
		}
	}

	/* Get lastest config page */
	offset = mt_ftl_leb_lastpage_offset(dev, leb);

	if (offset == 0) {
		if ((leb == CONFIG_START_BLOCK) && ubi_is_mapped(desc, CONFIG_START_BLOCK + 1))
			leb = CONFIG_START_BLOCK + 1;
		offset = mt_ftl_leb_lastpage_offset(dev, leb);
		if (offset == 0) {
			mt_ftl_err(dev, "Config blocks are empty");
			ret = mt_ftl_param_default(dev);
			if (ret)
				mt_ftl_err(dev, "mt_ftl_param_default fail, ret = %d", ret);
			return ret;
		}
	}

	offset -= NAND_PAGE_SIZE;

	/* Grab configs */
	mt_ftl_err(dev, "Get config page, leb:%d, page:%d", leb, offset / NAND_PAGE_SIZE);
	ret = ubi_leb_read(desc, leb, (char *)param->general_page_buffer, offset, NAND_PAGE_SIZE, 0);
	if (ret && ret != UBI_IO_BITFLIPS) {
		mt_ftl_err(dev, "ubi_leb_read (leb%d, offset=%d) failed, ret = 0x%x",
			   leb, offset, ret);
		return MT_FTL_FAIL;
	}

	/* Sync to backup CONFIG block */
	if (leb == CONFIG_START_BLOCK)
		mt_ftl_write_to_blk(dev, CONFIG_START_BLOCK + 1, param->general_page_buffer);
	else
		mt_ftl_write_to_blk(dev, CONFIG_START_BLOCK, param->general_page_buffer);

	/* Init param */
	ret = mt_ftl_param_init(dev, param->general_page_buffer);
	if (ret) {
		mt_ftl_err(dev, "mt_ftl_param_init fail, ret = %d", ret);
		return ret;
	}

	return ret;
}

/* TODO: Tracking remove process to make sure mt_ftl_remove can be called during shut down */
int mt_ftl_remove(struct mt_ftl_blk *dev)
{
	int ret = MT_FTL_SUCCESS;
#ifdef PROFILE
	int i = 0;
#endif				/* PROFILE */

	mt_ftl_err(dev, "Enter");

#ifdef PROFILE
	for (i = 0; i < MT_FTL_PROFILE_TOTAL_PROFILE_NUM; i++)
		mt_ftl_err(dev, "%s = %lu ms", mtk_ftl_profile_message[i], profile_time[i] / 1000);
#endif				/* PROFILE */

	mt_ftl_commit(dev);
	crypto_free_comp(dev->param->cc);

	mt_ftl_err(dev, "mt_ftl_commit done");

	return ret;
}
