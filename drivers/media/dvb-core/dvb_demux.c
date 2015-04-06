/*
 * dvb_demux.c - DVB kernel demux API
 *
 * Copyright (C) 2000-2001 Ralph  Metzler <ralph@convergence.de>
 *		       & Marcus Metzler <marcus@convergence.de>
 *			 for convergence integrated media GmbH
 *
 * Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/string.h>
#include <linux/crc32.h>
#include <asm/uaccess.h>
#include <asm/div64.h>

#include "dvb_demux.h"

#define NOBUFS
/*
** #define DVB_DEMUX_SECTION_LOSS_LOG to monitor payload loss in the syslog
*/
// #define DVB_DEMUX_SECTION_LOSS_LOG

static int dvb_demux_tscheck;
module_param(dvb_demux_tscheck, int, 0644);
MODULE_PARM_DESC(dvb_demux_tscheck,
		"enable transport stream continuity and TEI check");

static int dvb_demux_speedcheck;
module_param(dvb_demux_speedcheck, int, 0644);
MODULE_PARM_DESC(dvb_demux_speedcheck,
		"enable transport stream speed check");

static int dvb_demux_feed_err_pkts = 1;
module_param(dvb_demux_feed_err_pkts, int, 0644);
MODULE_PARM_DESC(dvb_demux_feed_err_pkts,
		 "when set to 0, drop packets with the TEI bit set (1 by default)");

/* counter advancing for each new dvb-demux device */
static int dvb_demux_index;

static int dvb_demux_performancecheck;
module_param(dvb_demux_performancecheck, int, 0644);
MODULE_PARM_DESC(dvb_demux_performancecheck,
		"enable transport stream performance check, reported through debugfs");

#define dprintk_tscheck(x...) do {                              \
		if (dvb_demux_tscheck && printk_ratelimit())    \
			printk(x);                              \
	} while (0)

static const struct dvb_dmx_video_patterns mpeg2_seq_hdr = {
	{0x00, 0x00, 0x01, 0xB3},
	{0xFF, 0xFF, 0xFF, 0xFF},
	4,
	DMX_IDX_MPEG_SEQ_HEADER
};

static const struct dvb_dmx_video_patterns mpeg2_gop = {
	{0x00, 0x00, 0x01, 0xB8},
	{0xFF, 0xFF, 0xFF, 0xFF},
	4,
	DMX_IDX_MPEG_GOP
};

static const struct dvb_dmx_video_patterns mpeg2_iframe = {
	{0x00, 0x00, 0x01, 0x00, 0x00, 0x08},
	{0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x38},
	6,
	DMX_IDX_MPEG_I_FRAME_START
};

static const struct dvb_dmx_video_patterns mpeg2_pframe = {
	{0x00, 0x00, 0x01, 0x00, 0x00, 0x10},
	{0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x38},
	6,
	DMX_IDX_MPEG_P_FRAME_START
};

static const struct dvb_dmx_video_patterns mpeg2_bframe = {
	{0x00, 0x00, 0x01, 0x00, 0x00, 0x18},
	{0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x38},
	6,
	DMX_IDX_MPEG_B_FRAME_START
};

static const struct dvb_dmx_video_patterns h264_sps = {
	{0x00, 0x00, 0x01, 0x07},
	{0xFF, 0xFF, 0xFF, 0x1F},
	4,
	DMX_IDX_H264_SPS
};

static const struct dvb_dmx_video_patterns h264_pps = {
	{0x00, 0x00, 0x01, 0x08},
	{0xFF, 0xFF, 0xFF, 0x1F},
	4,
	DMX_IDX_H264_PPS
};

static const struct dvb_dmx_video_patterns h264_idr = {
	{0x00, 0x00, 0x01, 0x05, 0x80},
	{0xFF, 0xFF, 0xFF, 0x1F, 0x80},
	5,
	DMX_IDX_H264_IDR_START
};

static const struct dvb_dmx_video_patterns h264_non_idr = {
	{0x00, 0x00, 0x01, 0x01, 0x80},
	{0xFF, 0xFF, 0xFF, 0x1F, 0x80},
	5,
	DMX_IDX_H264_NON_IDR_START
};

static const struct dvb_dmx_video_patterns h264_non_access_unit_del = {
	{0x00, 0x00, 0x01, 0x09},
	{0xFF, 0xFF, 0xFF, 0x1F},
	4,
	DMX_IDX_H264_ACCESS_UNIT_DEL
};

static const struct dvb_dmx_video_patterns h264_non_sei = {
	{0x00, 0x00, 0x01, 0x06},
	{0xFF, 0xFF, 0xFF, 0x1F},
	4,
	DMX_IDX_H264_SEI
};

static const struct dvb_dmx_video_patterns vc1_seq_hdr = {
	{0x00, 0x00, 0x01, 0x0F},
	{0xFF, 0xFF, 0xFF, 0xFF},
	4,
	DMX_IDX_VC1_SEQ_HEADER
};

static const struct dvb_dmx_video_patterns vc1_entry_point = {
	{0x00, 0x00, 0x01, 0x0E},
	{0xFF, 0xFF, 0xFF, 0xFF},
	4,
	DMX_IDX_VC1_ENTRY_POINT
};

static const struct dvb_dmx_video_patterns vc1_frame = {
	{0x00, 0x00, 0x01, 0x0D},
	{0xFF, 0xFF, 0xFF, 0xFF},
	4,
	DMX_IDX_VC1_FRAME_START
};


/******************************************************************************
 * static inlined helper functions
 ******************************************************************************/

static inline u16 section_length(const u8 *buf)
{
	return 3 + ((buf[1] & 0x0f) << 8) + buf[2];
}

static inline u8 ts_scrambling_ctrl(const u8 *buf)
{
	return (buf[3] >> 6) & 0x3;
}

static inline u8 payload(const u8 *tsp)
{
	if (!(tsp[3] & 0x10))	// no payload?
		return 0;

	if (tsp[3] & 0x20) {	// adaptation field?
		if (tsp[4] > 183)	// corrupted data?
			return 0;
		else
			return 184 - 1 - tsp[4];
	}

	return 184;
}

static u32 dvb_dmx_crc32(struct dvb_demux_feed *f, const u8 *src, size_t len)
{
	return (f->feed.sec.crc_val = crc32_be(f->feed.sec.crc_val, src, len));
}

static void dvb_dmx_memcopy(struct dvb_demux_feed *f, u8 *d, const u8 *s,
			    size_t len)
{
	memcpy(d, s, len);
}

static u32 dvb_dmx_calc_time_delta(struct timespec past_time)
{
	struct timespec curr_time, delta_time;
	u64 delta_time_us;

	curr_time = current_kernel_time();
	delta_time = timespec_sub(curr_time, past_time);
	delta_time_us = ((s64)delta_time.tv_sec * USEC_PER_SEC) +
					delta_time.tv_nsec / 1000;

	return (u32)delta_time_us;
}

/******************************************************************************
 * Software filter functions
 ******************************************************************************/

/*
 * Check if two patterns are identical, taking mask into consideration.
 * @pattern1: the first byte pattern to compare.
 * @pattern2: the second byte pattern to compare.
 * @mask: the bit mask to use.
 * @pattern_size: the length of both patterns and the mask, in bytes.
 *
 * Return: 1 if patterns match, 0 otherwise.
 */
static inline int dvb_dmx_patterns_match(const u8 *pattern1, const u8 *pattern2,
					const u8 *mask, size_t pattern_size)
{
	int i;

	/*
	 * Assumption: it is OK to access pattern1, pattern2 and mask.
	 * This function performs no sanity checks to keep things fast.
	 */

	for (i = 0; i < pattern_size; i++)
		if ((pattern1[i] & mask[i]) != (pattern2[i] & mask[i]))
			return 0;

	return 1;
}

/*
 * dvb_dmx_video_pattern_search -
 * search for framing patterns in a given buffer.
 *
 * Optimized version: first search for a common substring, e.g. 0x00 0x00 0x01.
 * If this string is found, go over all the given patterns (all must start
 * with this string) and search for their ending in the buffer.
 *
 * Assumption: the patterns we look for do not spread over more than two
 * buffers.
 *
 * @paterns: the full patterns information to look for.
 * @patterns_num: the number of patterns to look for.
 * @buf: the buffer to search.
 * @buf_size: the size of the buffer to search. we search the entire buffer.
 * @prefix_size_masks: a bit mask (per pattern) of possible prefix sizes to use
 * when searching for a pattern that started at the last buffer.
 * Updated in this function for use in the next lookup.
 * @results: lookup results (offset, type, used_prefix_size) per found pattern,
 * up to DVB_DMX_MAX_FOUND_PATTERNS.
 *
 * Return:
 *   Number of patterns found (up to DVB_DMX_MAX_FOUND_PATTERNS).
 *   0 if pattern was not found.
 *   error value on failure.
 */
int dvb_dmx_video_pattern_search(
		const struct dvb_dmx_video_patterns
			*patterns[DVB_DMX_MAX_SEARCH_PATTERN_NUM],
		int patterns_num,
		const u8 *buf,
		size_t buf_size,
		struct dvb_dmx_video_prefix_size_masks *prefix_size_masks,
		struct dvb_dmx_video_patterns_results *results)
{
	int i, j;
	unsigned int current_size;
	u32 prefix;
	int found = 0;
	int start_offset = 0;
	/* the starting common substring to look for */
	u8 string[] = {0x00, 0x00, 0x01};
	/* the mask for the starting string */
	u8 string_mask[] = {0xFF, 0xFF, 0xFF};
	/* the size of the starting string (in bytes) */
	size_t string_size = 3;

	if ((patterns == NULL) || (patterns_num <= 0) || (buf == NULL))
		return -EINVAL;

	memset(results, 0, sizeof(struct dvb_dmx_video_patterns_results));

	/*
	 * handle prefix - disregard string, simply check all patterns,
	 * looking for a matching suffix at the very beginning of the buffer.
	 */
	for (j = 0; (j < patterns_num) && !found; j++) {
		prefix = prefix_size_masks->size_mask[j];
		current_size = 32;
		while (prefix) {
			if (prefix & (0x1 << (current_size - 1))) {
				/*
				 * check that we don't look further
				 * than buf_size boundary
				 */
				if ((int)(patterns[j]->size - current_size) >
						buf_size)
					break;

				if (dvb_dmx_patterns_match(
					(patterns[j]->pattern + current_size),
					buf, (patterns[j]->mask + current_size),
					(patterns[j]->size - current_size))) {

					/*
					 * pattern found using prefix at the
					 * very beginning of the buffer, so
					 * offset is 0, but we already zeroed
					 * everything in the beginning of the
					 * function. that's why the next line
					 * is commented.
					 */
					/* results->info[found].offset = 0; */
					results->info[found].type =
							patterns[j]->type;
					results->info[found].used_prefix_size =
							current_size;
					found++;
					/*
					 * save offset to start looking from
					 * in the buffer, to avoid reusing the
					 * data of a pattern we already found.
					 */
					start_offset = (patterns[j]->size -
							current_size);

					if (found >= DVB_DMX_MAX_FOUND_PATTERNS)
						goto next_prefix_lookup;
					/*
					 * we don't want to search for the same
					 * pattern with several possible prefix
					 * sizes if we have already found it,
					 * so we break from the inner loop.
					 * since we incremented 'found', we
					 * will not search for additional
					 * patterns using a prefix - that would
					 * imply ambiguous patterns where one
					 * pattern can be included in another.
					 * the for loop will exit.
					 */
					break;
				}
			}
			prefix &= ~(0x1 << (current_size - 1));
			current_size--;
		}
	}

	/*
	 * Search buffer for entire pattern, starting with the string.
	 * Note the external for loop does not execute if buf_size is
	 * smaller than string_size (the cast to int is required, since
	 * size_t is unsigned).
	 */
	for (i = start_offset; i < (int)(buf_size - string_size + 1); i++) {
		if (dvb_dmx_patterns_match(string, (buf + i), string_mask,
							string_size)) {
			/* now search for patterns: */
			for (j = 0; j < patterns_num; j++) {
				/* avoid overflow to next buffer */
				if ((i + patterns[j]->size) > buf_size)
					continue;

				if (dvb_dmx_patterns_match(
					(patterns[j]->pattern + string_size),
					(buf + i + string_size),
					(patterns[j]->mask + string_size),
					(patterns[j]->size - string_size))) {

					results->info[found].offset = i;
					results->info[found].type =
						patterns[j]->type;
					/*
					 * save offset to start next prefix
					 * lookup, to avoid reusing the data
					 * of any pattern we already found.
					 */
					if ((i + patterns[j]->size) >
							start_offset)
						start_offset = (i +
							patterns[j]->size);
					/*
					 * did not use a prefix to find this
					 * pattern, but we zeroed everything
					 * in the beginning of the function.
					 * So no need to zero used_prefix_size
					 * for results->info[found]
					 */

					found++;
					if (found >= DVB_DMX_MAX_FOUND_PATTERNS)
						goto next_prefix_lookup;
					/*
					 * theoretically we don't have to break
					 * here, but we don't want to search
					 * for the other matching patterns on
					 * the very same same place in the
					 * buffer. That would mean the
					 * (pattern & mask) combinations are
					 * not unique. So we break from inner
					 * loop and move on to the next place
					 * in the buffer.
					 */
					break;
				}
			}
		}
	}

next_prefix_lookup:
	/* check for possible prefix sizes for the next buffer */
	for (j = 0; j < patterns_num; j++) {
		prefix_size_masks->size_mask[j] = 0;
		for (i = 1; i < patterns[j]->size; i++) {
			/*
			 * avoid looking outside of the buffer
			 * or reusing previously used data.
			 */
			if (i > (buf_size - start_offset))
				break;

			if (dvb_dmx_patterns_match(patterns[j]->pattern,
					(buf + buf_size - i),
					patterns[j]->mask, i)) {
				prefix_size_masks->size_mask[j] |=
						(1 << (i - 1));
			}
		}
	}

	return found;
}
EXPORT_SYMBOL(dvb_dmx_video_pattern_search);

/**
 * dvb_dmx_notify_section_event() - Notify demux event for all filters of a
 * specified section feed.
 *
 * @feed:		dvb_demux_feed object
 * @event:		demux event to notify
 * @should_lock:	specifies whether the function should lock the demux
 *
 * Caller is responsible for locking the demux properly, either by doing the
 * locking itself and setting 'should_lock' to 0, or have the function do it
 * by setting 'should_lock' to 1.
 */
int dvb_dmx_notify_section_event(struct dvb_demux_feed *feed,
	struct dmx_data_ready *event, int should_lock)
{
	struct dvb_demux_filter *f;

	if (feed == NULL || event == NULL || feed->type != DMX_TYPE_SEC)
		return -EINVAL;

	if (!should_lock && !spin_is_locked(&feed->demux->lock))
		return -EINVAL;

	if (should_lock)
		spin_lock(&feed->demux->lock);

	f = feed->filter;
	while (f && feed->feed.sec.is_filtering) {
		feed->data_ready_cb.sec(&f->filter, event);
		f = f->next;
	}

	if (should_lock)
		spin_unlock(&feed->demux->lock);

	return 0;
}
EXPORT_SYMBOL(dvb_dmx_notify_section_event);

static int dvb_dmx_check_pes_end(struct dvb_demux_feed *feed)
{
	struct dmx_data_ready data;

	if (!feed->pusi_seen)
		return 0;

	data.status = DMX_OK_PES_END;
	data.data_length = 0;
	data.pes_end.start_gap = 0;
	data.pes_end.actual_length = feed->peslen;
	data.pes_end.disc_indicator_set = 0;
	data.pes_end.pes_length_mismatch = 0;
	data.pes_end.stc = 0;
	data.pes_end.tei_counter = feed->pes_tei_counter;
	data.pes_end.cont_err_counter = feed->pes_cont_err_counter;
	data.pes_end.ts_packets_num = feed->pes_ts_packets_num;

	return feed->data_ready_cb.ts(&feed->feed.ts, &data);
}

static inline int dvb_dmx_swfilter_payload(struct dvb_demux_feed *feed,
					   const u8 *buf)
{
	int count = payload(buf);
	int p;
	int ccok;
	u8 cc;
	int ret;

	if (count == 0)
		return -1;

	p = 188 - count;

	cc = buf[3] & 0x0f;
	if (feed->first_cc)
		ccok = 1;
	else
		ccok = ((feed->cc + 1) & 0x0f) == cc;

	feed->first_cc = 0;
	feed->cc = cc;

	/* PUSI ? */
	if (buf[1] & 0x40) {
		dvb_dmx_check_pes_end(feed);
		feed->pusi_seen = 1;
		feed->peslen = 0;
		feed->pes_tei_counter = 0;
		feed->pes_cont_err_counter = 0;
		feed->pes_ts_packets_num = 0;
	}

	if (feed->pusi_seen == 0)
		return 0;

	ret = feed->cb.ts(&buf[p], count, NULL, 0, &feed->feed.ts, DMX_OK);

	/* Verify TS packet was copied successfully */
	if (!ret) {
		feed->pes_cont_err_counter += !ccok;
		feed->pes_tei_counter += (buf[1] & 0x80) ? 1 : 0;
		feed->pes_ts_packets_num++;
		feed->peslen += count;
	}

	return ret;
}

static int dvb_dmx_swfilter_sectionfilter(struct dvb_demux_feed *feed,
					  struct dvb_demux_filter *f)
{
	u8 neq = 0;
	int i;

	for (i = 0; i < DVB_DEMUX_MASK_MAX; i++) {
		u8 xor = f->filter.filter_value[i] ^ feed->feed.sec.secbuf[i];

		if (f->maskandmode[i] & xor)
			return 0;

		neq |= f->maskandnotmode[i] & xor;
	}

	if (f->doneq && !neq)
		return 0;

	return feed->cb.sec(feed->feed.sec.secbuf, feed->feed.sec.seclen,
			    NULL, 0, &f->filter, DMX_OK);
}

static inline int dvb_dmx_swfilter_section_feed(struct dvb_demux_feed *feed)
{
	struct dvb_demux *demux = feed->demux;
	struct dvb_demux_filter *f = feed->filter;
	struct dmx_section_feed *sec = &feed->feed.sec;
	int section_syntax_indicator;

	if (!sec->is_filtering)
		return 0;

	if (!f)
		return 0;

	if (sec->check_crc) {
		struct timespec pre_crc_time;

		if (dvb_demux_performancecheck)
			pre_crc_time = current_kernel_time();

		section_syntax_indicator = ((sec->secbuf[1] & 0x80) != 0);
		if (section_syntax_indicator &&
		    demux->check_crc32(feed, sec->secbuf, sec->seclen)) {
			if (dvb_demux_performancecheck)
				demux->total_crc_time +=
					dvb_dmx_calc_time_delta(pre_crc_time);

			/* Notify on CRC error */
			feed->cb.sec(NULL, 0, NULL, 0,
				&f->filter, DMX_CRC_ERROR);

			return -1;
		}

		if (dvb_demux_performancecheck)
			demux->total_crc_time +=
				dvb_dmx_calc_time_delta(pre_crc_time);
	}

	do {
		if (dvb_dmx_swfilter_sectionfilter(feed, f) < 0)
			return -1;
	} while ((f = f->next) && sec->is_filtering);

	sec->seclen = 0;

	return 0;
}

static void dvb_dmx_swfilter_section_new(struct dvb_demux_feed *feed)
{
	struct dmx_section_feed *sec = &feed->feed.sec;

#ifdef DVB_DEMUX_SECTION_LOSS_LOG
	if (sec->secbufp < sec->tsfeedp) {
		int i, n = sec->tsfeedp - sec->secbufp;

		/*
		 * Section padding is done with 0xff bytes entirely.
		 * Due to speed reasons, we won't check all of them
		 * but just first and last.
		 */
		if (sec->secbuf[0] != 0xff || sec->secbuf[n - 1] != 0xff) {
			printk("dvb_demux.c section ts padding loss: %d/%d\n",
			       n, sec->tsfeedp);
			printk("dvb_demux.c pad data:");
			for (i = 0; i < n; i++)
				printk(" %02x", sec->secbuf[i]);
			printk("\n");
		}
	}
#endif

	sec->tsfeedp = sec->secbufp = sec->seclen = 0;
	sec->secbuf = sec->secbuf_base;
}

/*
 * Losless Section Demux 1.4.1 by Emard
 * Valsecchi Patrick:
 *  - middle of section A  (no PUSI)
 *  - end of section A and start of section B
 *    (with PUSI pointing to the start of the second section)
 *
 *  In this case, without feed->pusi_seen you'll receive a garbage section
 *  consisting of the end of section A. Basically because tsfeedp
 *  is incemented and the use=0 condition is not raised
 *  when the second packet arrives.
 *
 * Fix:
 * when demux is started, let feed->pusi_seen = 0 to
 * prevent initial feeding of garbage from the end of
 * previous section. When you for the first time see PUSI=1
 * then set feed->pusi_seen = 1
 */
static int dvb_dmx_swfilter_section_copy_dump(struct dvb_demux_feed *feed,
					      const u8 *buf, u8 len)
{
	struct dvb_demux *demux = feed->demux;
	struct dmx_section_feed *sec = &feed->feed.sec;
	u16 limit, seclen, n;

	if (sec->tsfeedp >= DMX_MAX_SECFEED_SIZE)
		return 0;

	if (sec->tsfeedp + len > DMX_MAX_SECFEED_SIZE) {
#ifdef DVB_DEMUX_SECTION_LOSS_LOG
		printk("dvb_demux.c section buffer full loss: %d/%d\n",
		       sec->tsfeedp + len - DMX_MAX_SECFEED_SIZE,
		       DMX_MAX_SECFEED_SIZE);
#endif
		len = DMX_MAX_SECFEED_SIZE - sec->tsfeedp;
	}

	if (len <= 0)
		return 0;

	demux->memcopy(feed, sec->secbuf_base + sec->tsfeedp, buf, len);
	sec->tsfeedp += len;

	/*
	 * Dump all the sections we can find in the data (Emard)
	 */
	limit = sec->tsfeedp;
	if (limit > DMX_MAX_SECFEED_SIZE)
		return -1;	/* internal error should never happen */

	/* to be sure always set secbuf */
	sec->secbuf = sec->secbuf_base + sec->secbufp;

	for (n = 0; sec->secbufp + 2 < limit; n++) {
		seclen = section_length(sec->secbuf);
		if (seclen <= 0 || seclen > DMX_MAX_SECTION_SIZE
		    || seclen + sec->secbufp > limit)
			return 0;
		sec->seclen = seclen;
		sec->crc_val = ~0;
		/* dump [secbuf .. secbuf+seclen) */
		if (feed->pusi_seen)
			dvb_dmx_swfilter_section_feed(feed);
#ifdef DVB_DEMUX_SECTION_LOSS_LOG
		else
			printk("dvb_demux.c pusi not seen, discarding section data\n");
#endif
		sec->secbufp += seclen;	/* secbufp and secbuf moving together is */
		sec->secbuf += seclen;	/* redundant but saves pointer arithmetic */
	}

	return 0;
}

static int dvb_dmx_swfilter_section_one_packet(struct dvb_demux_feed *feed,
					   const u8 *buf)
{
	u8 p, count;
	int ccok, dc_i = 0;
	u8 cc;

	count = payload(buf);

	if (count == 0)		/* count == 0 if no payload or out of range */
		return -1;

	p = 188 - count;	/* payload start */

	cc = buf[3] & 0x0f;
	if (feed->first_cc)
		ccok = 1;
	else
		ccok = ((feed->cc + 1) & 0x0f) == cc;

	/* discard TS packets holding sections with TEI bit set */
	if (buf[1] & 0x80)
		return -EINVAL;

	feed->first_cc = 0;
	feed->cc = cc;

	if (buf[3] & 0x20) {
		/* adaption field present, check for discontinuity_indicator */
		if ((buf[4] > 0) && (buf[5] & 0x80))
			dc_i = 1;
	}

	if (!ccok || dc_i) {
#ifdef DVB_DEMUX_SECTION_LOSS_LOG
		printk("dvb_demux.c discontinuity detected %d bytes lost\n",
		       count);
		/*
		 * those bytes under sume circumstances will again be reported
		 * in the following dvb_dmx_swfilter_section_new
		 */
#endif
		/*
		 * Discontinuity detected. Reset pusi_seen = 0 to
		 * stop feeding of suspicious data until next PUSI=1 arrives
		 */
		feed->pusi_seen = 0;
		dvb_dmx_swfilter_section_new(feed);
	}

	if (buf[1] & 0x40) {
		/* PUSI=1 (is set), section boundary is here */
		if (count > 1 && buf[p] < count) {
			const u8 *before = &buf[p + 1];
			u8 before_len = buf[p];
			const u8 *after = &before[before_len];
			u8 after_len = count - 1 - before_len;

			dvb_dmx_swfilter_section_copy_dump(feed, before,
							   before_len);
			/* before start of new section, set pusi_seen = 1 */
			feed->pusi_seen = 1;
			dvb_dmx_swfilter_section_new(feed);
			dvb_dmx_swfilter_section_copy_dump(feed, after,
							   after_len);
		}
#ifdef DVB_DEMUX_SECTION_LOSS_LOG
		else if (count > 0)
			printk("dvb_demux.c PUSI=1 but %d bytes lost\n", count);
#endif
	} else {
		/* PUSI=0 (is not set), no section boundary */
		dvb_dmx_swfilter_section_copy_dump(feed, &buf[p], count);
	}

	return 0;
}

/*
 * dvb_dmx_swfilter_section_packet - wrapper for section filtering of single
 * TS packet.
 *
 * @feed: dvb demux feed
 * @buf: buffer containing the TS packet
 * @should_lock: specifies demux locking semantics: if not set, proper demux
 * locking is expected to have been done by the caller.
 *
 * Return error status
 */
int dvb_dmx_swfilter_section_packet(struct dvb_demux_feed *feed,
	   const u8 *buf, int should_lock)
{
	int ret;

	if (!should_lock && !spin_is_locked(&feed->demux->lock)) {
		pr_err("%s: demux spinlock should have been locked\n",
			__func__);
		return -EINVAL;
	}

	if (should_lock)
		spin_lock(&feed->demux->lock);

	ret = dvb_dmx_swfilter_section_one_packet(feed, buf);

	if (should_lock)
		spin_unlock(&feed->demux->lock);

	return ret;
}
EXPORT_SYMBOL(dvb_dmx_swfilter_section_packet);

static int dvb_demux_idx_event_sort(struct dmx_index_event_info *curr,
	struct dmx_index_event_info *new)
{
	if (curr->match_tsp_num > new->match_tsp_num)
		return 0;

	if (curr->match_tsp_num < new->match_tsp_num)
		return 1;
	/*
	 * In case TSP numbers are equal, sort according to event type giving
	 * priority to PUSI events first, then RAI and finally framing events.
	 */
	if ((curr->type & DMX_IDX_RAI && new->type & DMX_IDX_PUSI) ||
		(!(curr->type & DMX_IDX_PUSI) && !(curr->type & DMX_IDX_RAI) &&
			new->type & (DMX_IDX_PUSI | DMX_IDX_RAI)))
		return 0;

	return 1;
}

static int dvb_demux_save_idx_event(struct dvb_demux_feed *feed,
		struct dmx_index_event_info *idx_event,
		int traverse_from_tail)
{
	struct dmx_index_entry *idx_entry;
	struct dmx_index_entry *curr_entry;
	struct list_head *pos;

	/* get entry from free list */
	if (list_empty(&feed->rec_info->idx_info.free_list)) {
		printk(KERN_ERR "%s: index free list is empty\n", __func__);
		return -ENOMEM;
	}

	idx_entry = list_first_entry(&feed->rec_info->idx_info.free_list,
					struct dmx_index_entry, next);
	list_del(&idx_entry->next);

	idx_entry->event = *idx_event;

	pos = &feed->rec_info->idx_info.ready_list;
	if (traverse_from_tail) {
		list_for_each_entry_reverse(curr_entry,
			&feed->rec_info->idx_info.ready_list, next) {
			if (dvb_demux_idx_event_sort(&curr_entry->event,
				idx_event)) {
				pos = &curr_entry->next;
				break;
			}
		}
	} else {
		list_for_each_entry(curr_entry,
			&feed->rec_info->idx_info.ready_list, next) {
			if (!dvb_demux_idx_event_sort(&curr_entry->event,
				idx_event)) {
				pos = &curr_entry->next;
				break;
			}
		}
	}

	if (traverse_from_tail)
		list_add(&idx_entry->next, pos);
	else
		list_add_tail(&idx_entry->next, pos);

	return 0;
}

int dvb_demux_push_idx_event(struct dvb_demux_feed *feed,
		struct dmx_index_event_info *idx_event, int should_lock)
{
	int ret;

	if (!should_lock && !spin_is_locked(&feed->demux->lock))
		return -EINVAL;

	if (should_lock)
		spin_lock(&feed->demux->lock);
	ret = dvb_demux_save_idx_event(feed, idx_event, 1);
	if (should_lock)
		spin_unlock(&feed->demux->lock);

	return ret;
}
EXPORT_SYMBOL(dvb_demux_push_idx_event);

static inline void dvb_dmx_notify_indexing(struct dvb_demux_feed *feed)
{
	struct dmx_data_ready dmx_data_ready;
	struct dmx_index_entry *curr_entry;
	struct list_head *n, *pos;

	dmx_data_ready.status = DMX_OK_IDX;

	list_for_each_safe(pos, n, &feed->rec_info->idx_info.ready_list) {
		curr_entry = list_entry(pos, struct dmx_index_entry, next);

		if ((feed->rec_info->idx_info.min_pattern_tsp_num == (u64)-1) ||
			(curr_entry->event.match_tsp_num <=
			 feed->rec_info->idx_info.min_pattern_tsp_num)) {
			dmx_data_ready.idx_event = curr_entry->event;
			feed->data_ready_cb.ts(&feed->feed.ts, &dmx_data_ready);
			list_del(&curr_entry->next);
			list_add_tail(&curr_entry->next,
				&feed->rec_info->idx_info.free_list);
		}
	}
}

void dvb_dmx_notify_idx_events(struct dvb_demux_feed *feed, int should_lock)
{
	if (!should_lock && !spin_is_locked(&feed->demux->lock))
		return;

	if (should_lock)
		spin_lock(&feed->demux->lock);
	dvb_dmx_notify_indexing(feed);
	if (should_lock)
		spin_unlock(&feed->demux->lock);
}
EXPORT_SYMBOL(dvb_dmx_notify_idx_events);

static void dvb_dmx_process_pattern_result(struct dvb_demux_feed *feed,
		struct dvb_dmx_video_patterns_results *patterns, int pattern,
		u64 curr_stc, u64 prev_stc,
		u64 curr_match_tsp, u64 prev_match_tsp,
		u64 curr_pusi_tsp, u64 prev_pusi_tsp)
{
	int mpeg_frame_start;
	int h264_frame_start;
	int vc1_frame_start;
	int seq_start;
	u64 frame_end_in_seq;
	struct dmx_index_event_info idx_event;

	idx_event.pid = feed->pid;
	if (patterns->info[pattern].used_prefix_size) {
		idx_event.match_tsp_num = prev_match_tsp;
		idx_event.last_pusi_tsp_num = prev_pusi_tsp;
		idx_event.stc = prev_stc;
	} else {
		idx_event.match_tsp_num = curr_match_tsp;
		idx_event.last_pusi_tsp_num = curr_pusi_tsp;
		idx_event.stc = curr_stc;
	}

	/* notify on frame-end if needed */
	if (feed->prev_frame_valid) {
		if (feed->prev_frame_type & DMX_IDX_MPEG_I_FRAME_START) {
			idx_event.type = DMX_IDX_MPEG_I_FRAME_END;
			frame_end_in_seq = DMX_IDX_MPEG_FIRST_SEQ_FRAME_END;
		} else if (feed->prev_frame_type & DMX_IDX_MPEG_P_FRAME_START) {
			idx_event.type = DMX_IDX_MPEG_P_FRAME_END;
			frame_end_in_seq = DMX_IDX_MPEG_FIRST_SEQ_FRAME_END;
		} else if (feed->prev_frame_type & DMX_IDX_MPEG_B_FRAME_START) {
			idx_event.type = DMX_IDX_MPEG_B_FRAME_END;
			frame_end_in_seq = DMX_IDX_MPEG_FIRST_SEQ_FRAME_END;
		} else if (feed->prev_frame_type & DMX_IDX_H264_IDR_START) {
			idx_event.type = DMX_IDX_H264_IDR_END;
			frame_end_in_seq = DMX_IDX_H264_FIRST_SPS_FRAME_END;
		} else if (feed->prev_frame_type & DMX_IDX_H264_NON_IDR_START) {
			idx_event.type = DMX_IDX_H264_NON_IDR_END;
			frame_end_in_seq = DMX_IDX_H264_FIRST_SPS_FRAME_END;
		} else {
			idx_event.type = DMX_IDX_VC1_FRAME_END;
			frame_end_in_seq = DMX_IDX_VC1_FIRST_SEQ_FRAME_END;
		}

		if (feed->idx_params.types & idx_event.type)
			dvb_demux_save_idx_event(feed, &idx_event, 1);

		if (feed->first_frame_in_seq_notified &&
			feed->idx_params.types & frame_end_in_seq) {
			idx_event.type = frame_end_in_seq;
			dvb_demux_save_idx_event(feed, &idx_event, 1);
			feed->first_frame_in_seq_notified = 0;
		}
	}

	seq_start = patterns->info[pattern].type &
		(DMX_IDX_MPEG_SEQ_HEADER | DMX_IDX_H264_SPS |
		 DMX_IDX_VC1_SEQ_HEADER);

	/* did we find start of sequence/SPS? */
	if (seq_start) {
		feed->first_frame_in_seq = 1;
		feed->first_frame_in_seq_notified = 0;
		feed->prev_frame_valid = 0;
		idx_event.type = patterns->info[pattern].type;
		if (feed->idx_params.types & idx_event.type)
			dvb_demux_save_idx_event(feed, &idx_event, 1);
		return;
	}

	mpeg_frame_start = patterns->info[pattern].type &
		(DMX_IDX_MPEG_I_FRAME_START |
		 DMX_IDX_MPEG_P_FRAME_START |
		 DMX_IDX_MPEG_B_FRAME_START);

	h264_frame_start = patterns->info[pattern].type &
		(DMX_IDX_H264_IDR_START | DMX_IDX_H264_NON_IDR_START);

	vc1_frame_start = patterns->info[pattern].type &
		DMX_IDX_VC1_FRAME_START;

	if (!mpeg_frame_start && !h264_frame_start && !vc1_frame_start) {
		/* neither sequence nor frame, notify on the entry if needed */
		idx_event.type = patterns->info[pattern].type;
		if (feed->idx_params.types & idx_event.type)
			dvb_demux_save_idx_event(feed, &idx_event, 1);
		feed->prev_frame_valid = 0;
		return;
	}

	/* notify on first frame in sequence/sps if needed */
	if (feed->first_frame_in_seq) {
		feed->first_frame_in_seq = 0;
		feed->first_frame_in_seq_notified = 1;
		if (mpeg_frame_start)
			idx_event.type = DMX_IDX_MPEG_FIRST_SEQ_FRAME_START;
		else if (h264_frame_start)
			idx_event.type = DMX_IDX_H264_FIRST_SPS_FRAME_START;
		else
			idx_event.type = DMX_IDX_VC1_FIRST_SEQ_FRAME_START;

		if (feed->idx_params.types & idx_event.type)
			dvb_demux_save_idx_event(feed, &idx_event, 1);
	}

	/* notify on frame start if needed */
	idx_event.type = patterns->info[pattern].type;
	if (feed->idx_params.types & idx_event.type)
		dvb_demux_save_idx_event(feed, &idx_event, 1);

	feed->prev_frame_valid = 1;
	feed->prev_frame_type = patterns->info[pattern].type;
}

void dvb_dmx_process_idx_pattern(struct dvb_demux_feed *feed,
		struct dvb_dmx_video_patterns_results *patterns, int pattern,
		u64 curr_stc, u64 prev_stc,
		u64 curr_match_tsp, u64 prev_match_tsp,
		u64 curr_pusi_tsp, u64 prev_pusi_tsp)
{
	spin_lock(&feed->demux->lock);
	dvb_dmx_process_pattern_result(feed,
		patterns, pattern,
		curr_stc, prev_stc,
		curr_match_tsp, prev_match_tsp,
		curr_pusi_tsp, prev_pusi_tsp);
	spin_unlock(&feed->demux->lock);
}
EXPORT_SYMBOL(dvb_dmx_process_idx_pattern);

static void dvb_dmx_index(struct dvb_demux_feed *feed,
		const u8 *buf,
		const u8 timestamp[TIMESTAMP_LEN])
{
	int i;
	int p;
	u64 stc;
	int found_patterns;
	int count = payload(buf);
	u64 min_pattern_tsp_num;
	struct dvb_demux_feed *tmp_feed;
	struct dvb_demux *demux = feed->demux;
	struct dmx_index_event_info idx_event;
	struct dvb_dmx_video_patterns_results patterns;

	if (feed->demux->convert_ts)
		feed->demux->convert_ts(feed, timestamp, &stc);
	else
		stc = 0;

	idx_event.pid = feed->pid;
	idx_event.stc = stc;
	idx_event.match_tsp_num = feed->rec_info->ts_output_count;

	/* PUSI ? */
	if (buf[1] & 0x40) {
		feed->curr_pusi_tsp_num = feed->rec_info->ts_output_count;
		if (feed->idx_params.types & DMX_IDX_PUSI) {
			idx_event.type = DMX_IDX_PUSI;
			idx_event.last_pusi_tsp_num =
				feed->curr_pusi_tsp_num;
			dvb_demux_save_idx_event(feed, &idx_event, 1);
		}
	}

	/*
	 * if we still did not encounter a TS packet with PUSI indication,
	 * we cannot report index entries yet as we need to provide
	 * the TS packet number with PUSI indication preceding the TS
	 * packet pointed by the reported index entry.
	 */
	if (feed->curr_pusi_tsp_num == (u64)-1) {
		dvb_dmx_notify_indexing(feed);
		return;
	}

	if ((feed->idx_params.types & DMX_IDX_RAI) && /* index RAI? */
		(buf[3] & 0x20) && /* adaptation field exists? */
		(buf[4] > 0) && /* adaptation field len > 0 ? */
		(buf[5] & 0x40)) { /* RAI is set? */
		idx_event.type = DMX_IDX_RAI;
		idx_event.last_pusi_tsp_num =
			feed->curr_pusi_tsp_num;
		dvb_demux_save_idx_event(feed, &idx_event, 1);
	}

	/*
	 * if no pattern search is required, or the TS packet has no payload,
	 * pattern search is not executed.
	 */
	if (!feed->pattern_num || !count) {
		dvb_dmx_notify_indexing(feed);
		return;
	}

	p = 188 - count; /* payload start */

	found_patterns =
		dvb_dmx_video_pattern_search(feed->patterns,
				feed->pattern_num, &buf[p], count,
				&feed->prefix_size, &patterns);

	for (i = 0; i < found_patterns; i++)
		dvb_dmx_process_pattern_result(feed, &patterns, i,
			stc, feed->prev_stc,
			feed->rec_info->ts_output_count, feed->prev_tsp_num,
			feed->curr_pusi_tsp_num, feed->prev_pusi_tsp_num);

	feed->prev_tsp_num = feed->rec_info->ts_output_count;
	feed->prev_pusi_tsp_num = feed->curr_pusi_tsp_num;
	feed->prev_stc = stc;
	feed->last_pattern_tsp_num = feed->rec_info->ts_output_count;

	/*
	 * it is possible to have a TS packet that has a prefix of
	 * a video pattern but the video pattern is not identified yet
	 * until we get the next TS packet of that PID. When we get
	 * the next TS packet of that PID, pattern-search would
	 * detect that we have a new index entry that starts in the
	 * previous TS packet.
	 * In order to notify the user on index entries with match_tsp_num
	 * in ascending order, index events with match_tsp_num up to
	 * the last_pattern_tsp_num are notified now to the user,
	 * the rest can't be notified now as we might hit the above
	 * scenario and cause the events not to be notified with
	 * ascending order of match_tsp_num.
	 */
	if (feed->rec_info->idx_info.pattern_search_feeds_num == 1) {
		/*
		 * optimization for case we have only one PID
		 * with video pattern search, in this case
		 * min_pattern_tsp_num is simply updated to the new
		 * TS packet number of the PID with pattern search.
		 */
		feed->rec_info->idx_info.min_pattern_tsp_num =
			feed->last_pattern_tsp_num;
		dvb_dmx_notify_indexing(feed);
		return;
	}

	/*
	 * if we have more than one PID with pattern search,
	 * min_pattern_tsp_num needs to be updated now based on
	 * last_pattern_tsp_num of all PIDs with pattern search.
	 */
	min_pattern_tsp_num = (u64)-1;
	i = feed->rec_info->idx_info.pattern_search_feeds_num;
	list_for_each_entry(tmp_feed, &demux->feed_list, list_head) {
		if ((tmp_feed->state != DMX_STATE_GO) ||
			(tmp_feed->type != DMX_TYPE_TS) ||
			(tmp_feed->feed.ts.buffer.ringbuff !=
			 feed->feed.ts.buffer.ringbuff))
			continue;

		if ((tmp_feed->last_pattern_tsp_num != (u64)-1) &&
			((min_pattern_tsp_num == (u64)-1) ||
			 (tmp_feed->last_pattern_tsp_num <
			  min_pattern_tsp_num)))
			min_pattern_tsp_num = tmp_feed->last_pattern_tsp_num;

		if (tmp_feed->pattern_num) {
			i--;
			if (i == 0)
				break;
		}
	}

	feed->rec_info->idx_info.min_pattern_tsp_num = min_pattern_tsp_num;

	/* notify all index entries up to min_pattern_tsp_num */
	dvb_dmx_notify_indexing(feed);
}

static inline void dvb_dmx_swfilter_output_packet(
	struct dvb_demux_feed *feed,
	const u8 *buf,
	const u8 timestamp[TIMESTAMP_LEN])
{
	/*
	 * if we output 192 packet with timestamp at head of packet,
	 * output the timestamp now before the 188 TS packet
	 */
	if (feed->tsp_out_format == DMX_TSP_FORMAT_192_HEAD)
		feed->cb.ts(timestamp, TIMESTAMP_LEN, NULL,
			0, &feed->feed.ts, DMX_OK);

	feed->cb.ts(buf, 188, NULL, 0, &feed->feed.ts, DMX_OK);

	/*
	 * if we output 192 packet with timestamp at tail of packet,
	 * output the timestamp now after the 188 TS packet
	 */
	if (feed->tsp_out_format == DMX_TSP_FORMAT_192_TAIL)
		feed->cb.ts(timestamp, TIMESTAMP_LEN, NULL,
			0, &feed->feed.ts, DMX_OK);

	if (feed->idx_params.enable)
		dvb_dmx_index(feed, buf, timestamp);

	feed->rec_info->ts_output_count++;
}

static inline void dvb_dmx_configure_decoder_fullness(
						struct dvb_demux *demux,
						int initialize)
{
	struct dvb_demux_feed *feed;
	int j;

	for (j = 0; j < demux->feednum; j++) {
		feed = &demux->feed[j];

		if ((feed->state != DMX_STATE_GO) ||
			(feed->type != DMX_TYPE_TS) ||
			!(feed->ts_type & TS_DECODER))
			continue;

		if (initialize) {
			if (demux->decoder_fullness_init)
				demux->decoder_fullness_init(feed);
		} else {
			if (demux->decoder_fullness_abort)
				demux->decoder_fullness_abort(feed);
		}
	}
}

static inline int dvb_dmx_swfilter_buffer_check(
					struct dvb_demux *demux,
					u16 pid)
{
	int desired_space;
	int ret;
	struct dmx_ts_feed *ts;
	struct dvb_demux_filter *f;
	struct dvb_demux_feed *feed;
	int was_locked;
	int i, j;

	if (likely(spin_is_locked(&demux->lock)))
		was_locked = 1;
	else
		was_locked = 0;

	/*
	 * Check that there's enough free space for data output.
	 * If there no space, wait for it (block).
	 * Since this function is called while spinlock
	 * is aquired, the lock should be released first.
	 * Once we get control back, lock is aquired back
	 * and checks that the filter is still valid.
	 */
	for (j = 0; j < demux->feednum; j++) {
		feed = &demux->feed[j];

		if (demux->sw_filter_abort)
			return -ENODEV;

		if ((feed->state != DMX_STATE_GO) ||
			((feed->pid != pid) && (feed->pid != 0x2000)))
			continue;

		if (feed->secure_mode.is_secured &&
			!dvb_dmx_is_rec_feed(feed))
			return 0;

		if (feed->type == DMX_TYPE_TS) {
			desired_space = 192; /* upper bound */
			ts = &feed->feed.ts;

			if (feed->ts_type & TS_PACKET) {
				if (likely(was_locked))
					spin_unlock(&demux->lock);

				ret = demux->buffer_ctrl.ts(ts,
					desired_space, 1);

				if (likely(was_locked))
					spin_lock(&demux->lock);

				if (ret < 0)
					continue;
			}

			if (demux->sw_filter_abort)
				return -ENODEV;

			if (!ts->is_filtering)
				continue;

			if ((feed->ts_type & TS_DECODER) &&
				(demux->decoder_fullness_wait)) {
				if (likely(was_locked))
					spin_unlock(&demux->lock);

				ret = demux->decoder_fullness_wait(
								feed,
								desired_space);

				if (likely(was_locked))
					spin_lock(&demux->lock);

				if (ret < 0)
					continue;
			}

			continue;
		}

		/* else - section case */
		desired_space = feed->feed.sec.tsfeedp + 188; /* upper bound */
		for (i = 0; i < demux->filternum; i++) {
			if (demux->sw_filter_abort)
				return -EPERM;

			if (!feed->feed.sec.is_filtering)
				continue;

			f = &demux->filter[i];
			if (f->feed != feed)
				continue;

			if (likely(was_locked))
				spin_unlock(&demux->lock);

			ret = demux->buffer_ctrl.sec(&f->filter,
				desired_space, 1);

			if (likely(was_locked))
				spin_lock(&demux->lock);

			if (ret < 0)
				break;
		}
	}

	return 0;
}

static inline void dvb_dmx_swfilter_packet_type(struct dvb_demux_feed *feed,
			const u8 *buf, const u8 timestamp[TIMESTAMP_LEN])
{
	u16 pid = ts_pid(buf);
	u8 scrambling_bits = ts_scrambling_ctrl(buf);
	struct dmx_data_ready dmx_data_ready;

	/*
	 * Notify on scrambling status change only when we move
	 * from clear (0) to non-clear and vise-versa
	 */
	if ((scrambling_bits && !feed->scrambling_bits) ||
		(!scrambling_bits && feed->scrambling_bits)) {
		dmx_data_ready.status = DMX_OK_SCRAMBLING_STATUS;
		dmx_data_ready.data_length = 0;
		dmx_data_ready.scrambling_bits.pid = pid;
		dmx_data_ready.scrambling_bits.old_value =
			feed->scrambling_bits;
		dmx_data_ready.scrambling_bits.new_value = scrambling_bits;

		if (feed->type == DMX_TYPE_SEC)
			dvb_dmx_notify_section_event(feed, &dmx_data_ready, 0);
		else if (feed->feed.ts.is_filtering)
			feed->data_ready_cb.ts(&feed->feed.ts, &dmx_data_ready);
	}

	feed->scrambling_bits = scrambling_bits;

	switch (feed->type) {
	case DMX_TYPE_TS:
		if (!feed->feed.ts.is_filtering)
			break;
		if (feed->ts_type & TS_PACKET) {
			if (feed->ts_type & TS_PAYLOAD_ONLY) {
				if (!feed->secure_mode.is_secured)
					dvb_dmx_swfilter_payload(feed, buf);
			} else {
				dvb_dmx_swfilter_output_packet(feed,
						buf, timestamp);
			}
		}
		if ((feed->ts_type & TS_DECODER) &&
			!feed->secure_mode.is_secured)
			if (feed->demux->write_to_decoder)
				feed->demux->write_to_decoder(feed, buf, 188);
		break;

	case DMX_TYPE_SEC:
		if (!feed->feed.sec.is_filtering ||
			feed->secure_mode.is_secured)
			break;
		if (dvb_dmx_swfilter_section_one_packet(feed, buf) < 0)
			feed->feed.sec.seclen = feed->feed.sec.secbufp = 0;
		break;

	default:
		break;
	}
}

#define DVR_FEED(f)							\
	(((f)->type == DMX_TYPE_TS) &&					\
	((f)->feed.ts.is_filtering) &&					\
	(((f)->ts_type & (TS_PACKET | TS_DEMUX)) == TS_PACKET))

static void dvb_dmx_swfilter_one_packet(struct dvb_demux *demux, const u8 *buf,
				const u8 timestamp[TIMESTAMP_LEN])
{
	struct dvb_demux_feed *feed;
	u16 pid = ts_pid(buf);
	int dvr_done = 0;

	if (dvb_demux_speedcheck) {
		struct timespec cur_time, delta_time;
		u64 speed_bytes, speed_timedelta;

		demux->speed_pkts_cnt++;

		/* show speed every SPEED_PKTS_INTERVAL packets */
		if (!(demux->speed_pkts_cnt % SPEED_PKTS_INTERVAL)) {
			cur_time = current_kernel_time();

			if (demux->speed_last_time.tv_sec != 0 &&
					demux->speed_last_time.tv_nsec != 0) {
				delta_time = timespec_sub(cur_time,
						demux->speed_last_time);
				speed_bytes = (u64)demux->speed_pkts_cnt
					* 188 * 8;
				/* convert to 1024 basis */
				speed_bytes = 1000 * div64_u64(speed_bytes,
						1024);
				speed_timedelta =
					(u64)timespec_to_ns(&delta_time);
				speed_timedelta = div64_u64(speed_timedelta,
						1000000); /* nsec -> usec */
				printk(KERN_INFO "TS speed %llu Kbits/sec \n",
						div64_u64(speed_bytes,
							speed_timedelta));
			}

			demux->speed_last_time = cur_time;
			demux->speed_pkts_cnt = 0;
		}
	}

	if (buf[1] & 0x80) {
		dprintk_tscheck("TEI detected. "
				"PID=0x%x data1=0x%x\n",
				pid, buf[1]);
		/* data in this packet cant be trusted - drop it unless
		 * module option dvb_demux_feed_err_pkts is set */
		if (!dvb_demux_feed_err_pkts)
			return;
	} else /* if TEI bit is set, pid may be wrong- skip pkt counter */
		if (demux->cnt_storage && dvb_demux_tscheck) {
			/* check pkt counter */
			if (pid < MAX_PID) {
				if (buf[3] & 0x10)
					demux->cnt_storage[pid] =
						(demux->cnt_storage[pid] + 1) & 0xf;

				if ((buf[3] & 0xf) != demux->cnt_storage[pid]) {
					dprintk_tscheck("TS packet counter mismatch. PID=0x%x expected 0x%x got 0x%x\n",
						pid, demux->cnt_storage[pid],
						buf[3] & 0xf);
					demux->cnt_storage[pid] = buf[3] & 0xf;
				}
			}
			/* end check */
		}

	if (demux->playback_mode == DMX_PB_MODE_PULL)
		if (dvb_dmx_swfilter_buffer_check(demux, pid) < 0)
			return;

	list_for_each_entry(feed, &demux->feed_list, list_head) {
		if ((feed->pid != pid) && (feed->pid != 0x2000))
			continue;

		/* copy each packet only once to the dvr device, even
		 * if a PID is in multiple filters (e.g. video + PCR) */
		if ((DVR_FEED(feed)) && (dvr_done++))
			continue;

		if (feed->pid == pid)
			dvb_dmx_swfilter_packet_type(feed, buf, timestamp);
		else if ((feed->pid == 0x2000) &&
			     (feed->feed.ts.is_filtering))
			dvb_dmx_swfilter_output_packet(feed, buf, timestamp);
	}
}

void dvb_dmx_swfilter_packet(struct dvb_demux *demux, const u8 *buf,
				const u8 timestamp[TIMESTAMP_LEN])
{
	spin_lock(&demux->lock);
	dvb_dmx_swfilter_one_packet(demux, buf, timestamp);
	spin_unlock(&demux->lock);
}
EXPORT_SYMBOL(dvb_dmx_swfilter_packet);

void dvb_dmx_swfilter_packets(struct dvb_demux *demux, const u8 *buf,
			      size_t count)
{
	struct timespec pre_time;
	u8 timestamp[TIMESTAMP_LEN] = {0};

	if (dvb_demux_performancecheck)
		pre_time = current_kernel_time();

	spin_lock(&demux->lock);

	demux->sw_filter_abort = 0;
	dvb_dmx_configure_decoder_fullness(demux, 1);

	while (count--) {
		if (buf[0] == 0x47)
			dvb_dmx_swfilter_one_packet(demux, buf, timestamp);
		buf += 188;
	}

	spin_unlock(&demux->lock);

	if (dvb_demux_performancecheck)
		demux->total_process_time += dvb_dmx_calc_time_delta(pre_time);
}

EXPORT_SYMBOL(dvb_dmx_swfilter_packets);

static inline int find_next_packet(const u8 *buf, int pos, size_t count,
				   const int pktsize, const int leadingbytes)
{
	int start = pos, lost;

	while (pos < count) {
		if ((buf[pos] == 0x47 && !leadingbytes) ||
		    (pktsize == 204 && buf[pos] == 0xB8) ||
			(pktsize == 192 && leadingbytes &&
			 (pos+leadingbytes < count) &&
				buf[pos+leadingbytes] == 0x47))
			break;
		pos++;
	}

	lost = pos - start;
	if (lost) {
		/* This garbage is part of a valid packet? */
		int backtrack = pos - pktsize;
		if (backtrack >= 0 && (buf[backtrack] == 0x47 ||
		    (pktsize == 204 && buf[backtrack] == 0xB8) ||
			(pktsize == 192 &&
			buf[backtrack+leadingbytes] == 0x47)))
			return backtrack;
	}

	return pos;
}

/* Filter all pktsize= 188 or 204 sized packets and skip garbage. */
static inline void _dvb_dmx_swfilter(struct dvb_demux *demux, const u8 *buf,
		size_t count, const int pktsize, const int leadingbytes)
{
	int p = 0, i, j;
	const u8 *q;
	struct timespec pre_time;
	u8 timestamp[TIMESTAMP_LEN];

	if (dvb_demux_performancecheck)
		pre_time = current_kernel_time();

	spin_lock(&demux->lock);

	demux->sw_filter_abort = 0;
	dvb_dmx_configure_decoder_fullness(demux, 1);

	if (demux->tsbufp) { /* tsbuf[0] is now 0x47. */
		i = demux->tsbufp;
		j = pktsize - i;
		if (count < j) {
			memcpy(&demux->tsbuf[i], buf, count);
			demux->tsbufp += count;
			goto bailout;
		}
		memcpy(&demux->tsbuf[i], buf, j);

		if (pktsize == 192) {
			if (leadingbytes)
				memcpy(timestamp, &demux->tsbuf[p],
					TIMESTAMP_LEN);
			else
				memcpy(timestamp, &demux->tsbuf[188],
					TIMESTAMP_LEN);
		} else {
			memset(timestamp, 0, TIMESTAMP_LEN);
		}

		if (pktsize == 192 &&
			leadingbytes &&
			demux->tsbuf[leadingbytes] == 0x47)  /* double check */
			dvb_dmx_swfilter_one_packet(demux,
				demux->tsbuf + TIMESTAMP_LEN, timestamp);
		else if (demux->tsbuf[0] == 0x47) /* double check */
			dvb_dmx_swfilter_one_packet(demux,
					demux->tsbuf, timestamp);
		demux->tsbufp = 0;
		p += j;
	}

	while (1) {
		p = find_next_packet(buf, p, count, pktsize, leadingbytes);

		if (demux->sw_filter_abort)
			goto bailout;

		if (p >= count)
			break;
		if (count - p < pktsize)
			break;

		q = &buf[p];

		if (pktsize == 204 && (*q == 0xB8)) {
			memcpy(demux->tsbuf, q, 188);
			demux->tsbuf[0] = 0x47;
			q = demux->tsbuf;
		}

		if (pktsize == 192) {
			if (leadingbytes) {
				q = &buf[p+leadingbytes];
				memcpy(timestamp, &buf[p], TIMESTAMP_LEN);
			} else {
				memcpy(timestamp, &buf[p+188], TIMESTAMP_LEN);
			}
		} else {
			memset(timestamp, 0, TIMESTAMP_LEN);
		}

		dvb_dmx_swfilter_one_packet(demux, q, timestamp);
		p += pktsize;
	}

	i = count - p;
	if (i) {
		memcpy(demux->tsbuf, &buf[p], i);
		demux->tsbufp = i;
		if (pktsize == 204 && demux->tsbuf[0] == 0xB8)
			demux->tsbuf[0] = 0x47;
	}

bailout:
	spin_unlock(&demux->lock);

	if (dvb_demux_performancecheck)
		demux->total_process_time += dvb_dmx_calc_time_delta(pre_time);
}

void dvb_dmx_swfilter(struct dvb_demux *demux, const u8 *buf, size_t count)
{
	_dvb_dmx_swfilter(demux, buf, count, 188, 0);
}
EXPORT_SYMBOL(dvb_dmx_swfilter);

void dvb_dmx_swfilter_204(struct dvb_demux *demux, const u8 *buf, size_t count)
{
	_dvb_dmx_swfilter(demux, buf, count, 204, 0);
}
EXPORT_SYMBOL(dvb_dmx_swfilter_204);

void dvb_dmx_swfilter_raw(struct dvb_demux *demux, const u8 *buf, size_t count)
{
	spin_lock(&demux->lock);

	demux->feed->cb.ts(buf, count, NULL, 0, &demux->feed->feed.ts, DMX_OK);

	spin_unlock(&demux->lock);
}
EXPORT_SYMBOL(dvb_dmx_swfilter_raw);

void dvb_dmx_swfilter_format(
			struct dvb_demux *demux,
			const u8 *buf,
			size_t count,
			enum dmx_tsp_format_t tsp_format)
{
	switch (tsp_format) {
	case DMX_TSP_FORMAT_188:
		_dvb_dmx_swfilter(demux, buf, count, 188, 0);
		break;

	case DMX_TSP_FORMAT_192_TAIL:
		_dvb_dmx_swfilter(demux, buf, count, 192, 0);
		break;

	case DMX_TSP_FORMAT_192_HEAD:
		_dvb_dmx_swfilter(demux, buf, count, 192, TIMESTAMP_LEN);
		break;

	case DMX_TSP_FORMAT_204:
		_dvb_dmx_swfilter(demux, buf, count, 204, 0);
		break;

	default:
		printk(KERN_ERR "%s: invalid TS packet format (format=%d)\n",
			   __func__,
			   tsp_format);
		break;
	}
}
EXPORT_SYMBOL(dvb_dmx_swfilter_format);

static struct dvb_demux_filter *dvb_dmx_filter_alloc(struct dvb_demux *demux)
{
	int i;

	for (i = 0; i < demux->filternum; i++)
		if (demux->filter[i].state == DMX_STATE_FREE)
			break;

	if (i == demux->filternum)
		return NULL;

	demux->filter[i].state = DMX_STATE_ALLOCATED;

	return &demux->filter[i];
}

static struct dvb_demux_feed *dvb_dmx_feed_alloc(struct dvb_demux *demux)
{
	int i;

	for (i = 0; i < demux->feednum; i++)
		if (demux->feed[i].state == DMX_STATE_FREE)
			break;

	if (i == demux->feednum)
		return NULL;

	demux->feed[i].state = DMX_STATE_ALLOCATED;

	return &demux->feed[i];
}

const struct dvb_dmx_video_patterns *dvb_dmx_get_pattern(u64 dmx_idx_pattern)
{
	switch (dmx_idx_pattern) {
	case DMX_IDX_MPEG_SEQ_HEADER:
		return &mpeg2_seq_hdr;

	case DMX_IDX_MPEG_GOP:
		return &mpeg2_gop;

	case DMX_IDX_MPEG_I_FRAME_START:
		return &mpeg2_iframe;

	case DMX_IDX_MPEG_P_FRAME_START:
		return &mpeg2_pframe;

	case DMX_IDX_MPEG_B_FRAME_START:
		return &mpeg2_bframe;

	case DMX_IDX_H264_SPS:
		return &h264_sps;

	case DMX_IDX_H264_PPS:
		return &h264_pps;

	case DMX_IDX_H264_IDR_START:
		return &h264_idr;

	case DMX_IDX_H264_NON_IDR_START:
		return &h264_non_idr;

	case DMX_IDX_H264_ACCESS_UNIT_DEL:
		return &h264_non_access_unit_del;

	case DMX_IDX_H264_SEI:
		return &h264_non_sei;

	case DMX_IDX_VC1_SEQ_HEADER:
		return &vc1_seq_hdr;

	case DMX_IDX_VC1_ENTRY_POINT:
		return &vc1_entry_point;

	case DMX_IDX_VC1_FRAME_START:
		return &vc1_frame;

	default:
		return NULL;
	}
}
EXPORT_SYMBOL(dvb_dmx_get_pattern);

static void dvb_dmx_init_idx_state(struct dvb_demux_feed *feed)
{
	feed->prev_tsp_num = (u64)-1;
	feed->curr_pusi_tsp_num = (u64)-1;
	feed->prev_pusi_tsp_num = (u64)-1;
	feed->prev_frame_valid = 0;
	feed->first_frame_in_seq = 0;
	feed->first_frame_in_seq_notified = 0;
	feed->last_pattern_tsp_num = (u64)-1;
	feed->pattern_num = 0;
	memset(&feed->prefix_size, 0,
		sizeof(struct dvb_dmx_video_prefix_size_masks));

	if (feed->idx_params.types &
		(DMX_IDX_MPEG_SEQ_HEADER |
		 DMX_IDX_MPEG_FIRST_SEQ_FRAME_START |
		 DMX_IDX_MPEG_FIRST_SEQ_FRAME_END)) {
		feed->patterns[feed->pattern_num] =
			dvb_dmx_get_pattern(DMX_IDX_MPEG_SEQ_HEADER);
		feed->pattern_num++;
	}

	if ((feed->pattern_num < DVB_DMX_MAX_SEARCH_PATTERN_NUM) &&
		(feed->idx_params.types & DMX_IDX_MPEG_GOP)) {
		feed->patterns[feed->pattern_num] =
			dvb_dmx_get_pattern(DMX_IDX_MPEG_GOP);
		feed->pattern_num++;
	}

	/* MPEG2 I-frame */
	if ((feed->pattern_num < DVB_DMX_MAX_SEARCH_PATTERN_NUM) &&
		(feed->idx_params.types &
		 (DMX_IDX_MPEG_I_FRAME_START | DMX_IDX_MPEG_I_FRAME_END |
		  DMX_IDX_MPEG_P_FRAME_END | DMX_IDX_MPEG_B_FRAME_END |
		  DMX_IDX_MPEG_FIRST_SEQ_FRAME_START |
		  DMX_IDX_MPEG_FIRST_SEQ_FRAME_END))) {
		feed->patterns[feed->pattern_num] =
			dvb_dmx_get_pattern(DMX_IDX_MPEG_I_FRAME_START);
		feed->pattern_num++;
	}

	/* MPEG2 P-frame */
	if ((feed->pattern_num < DVB_DMX_MAX_SEARCH_PATTERN_NUM) &&
		(feed->idx_params.types &
		 (DMX_IDX_MPEG_P_FRAME_START | DMX_IDX_MPEG_P_FRAME_END |
		  DMX_IDX_MPEG_I_FRAME_END | DMX_IDX_MPEG_B_FRAME_END |
		  DMX_IDX_MPEG_FIRST_SEQ_FRAME_START |
		  DMX_IDX_MPEG_FIRST_SEQ_FRAME_END))) {
		feed->patterns[feed->pattern_num] =
			dvb_dmx_get_pattern(DMX_IDX_MPEG_P_FRAME_START);
		feed->pattern_num++;
	}

	/* MPEG2 B-frame */
	if ((feed->pattern_num < DVB_DMX_MAX_SEARCH_PATTERN_NUM) &&
		(feed->idx_params.types &
		 (DMX_IDX_MPEG_B_FRAME_START | DMX_IDX_MPEG_B_FRAME_END |
		  DMX_IDX_MPEG_I_FRAME_END | DMX_IDX_MPEG_P_FRAME_END |
		  DMX_IDX_MPEG_FIRST_SEQ_FRAME_START |
		  DMX_IDX_MPEG_FIRST_SEQ_FRAME_END))) {
		feed->patterns[feed->pattern_num] =
			dvb_dmx_get_pattern(DMX_IDX_MPEG_B_FRAME_START);
		feed->pattern_num++;
	}

	if ((feed->pattern_num < DVB_DMX_MAX_SEARCH_PATTERN_NUM) &&
		(feed->idx_params.types &
		 (DMX_IDX_H264_SPS |
		  DMX_IDX_H264_FIRST_SPS_FRAME_START |
		  DMX_IDX_H264_FIRST_SPS_FRAME_END))) {
		feed->patterns[feed->pattern_num] =
			dvb_dmx_get_pattern(DMX_IDX_H264_SPS);
		feed->pattern_num++;
	}

	if ((feed->pattern_num < DVB_DMX_MAX_SEARCH_PATTERN_NUM) &&
		(feed->idx_params.types & DMX_IDX_H264_PPS)) {
		feed->patterns[feed->pattern_num] =
			dvb_dmx_get_pattern(DMX_IDX_H264_PPS);
		feed->pattern_num++;
	}

	/* H264 IDR */
	if ((feed->pattern_num < DVB_DMX_MAX_SEARCH_PATTERN_NUM) &&
		(feed->idx_params.types &
		 (DMX_IDX_H264_IDR_START | DMX_IDX_H264_IDR_END |
		  DMX_IDX_H264_NON_IDR_END |
		  DMX_IDX_H264_FIRST_SPS_FRAME_START |
		  DMX_IDX_H264_FIRST_SPS_FRAME_END))) {
		feed->patterns[feed->pattern_num] =
			dvb_dmx_get_pattern(DMX_IDX_H264_IDR_START);
		feed->pattern_num++;
	}

	/* H264 non-IDR */
	if ((feed->pattern_num < DVB_DMX_MAX_SEARCH_PATTERN_NUM) &&
		(feed->idx_params.types &
		 (DMX_IDX_H264_NON_IDR_START | DMX_IDX_H264_NON_IDR_END |
		  DMX_IDX_H264_IDR_END |
		  DMX_IDX_H264_FIRST_SPS_FRAME_START |
		  DMX_IDX_H264_FIRST_SPS_FRAME_END))) {
		feed->patterns[feed->pattern_num] =
			dvb_dmx_get_pattern(DMX_IDX_H264_NON_IDR_START);
		feed->pattern_num++;
	}

	if ((feed->pattern_num < DVB_DMX_MAX_SEARCH_PATTERN_NUM) &&
		(feed->idx_params.types & DMX_IDX_H264_ACCESS_UNIT_DEL)) {
		feed->patterns[feed->pattern_num] =
			dvb_dmx_get_pattern(DMX_IDX_H264_ACCESS_UNIT_DEL);
		feed->pattern_num++;
	}

	if ((feed->pattern_num < DVB_DMX_MAX_SEARCH_PATTERN_NUM) &&
		(feed->idx_params.types & DMX_IDX_H264_SEI)) {
		feed->patterns[feed->pattern_num] =
			dvb_dmx_get_pattern(DMX_IDX_H264_SEI);
		feed->pattern_num++;
	}

	if ((feed->pattern_num < DVB_DMX_MAX_SEARCH_PATTERN_NUM) &&
		(feed->idx_params.types &
		 (DMX_IDX_VC1_SEQ_HEADER |
		  DMX_IDX_VC1_FIRST_SEQ_FRAME_START |
		  DMX_IDX_VC1_FIRST_SEQ_FRAME_END))) {
		feed->patterns[feed->pattern_num] =
			dvb_dmx_get_pattern(DMX_IDX_VC1_SEQ_HEADER);
		feed->pattern_num++;
	}

	if ((feed->pattern_num < DVB_DMX_MAX_SEARCH_PATTERN_NUM) &&
		(feed->idx_params.types & DMX_IDX_VC1_ENTRY_POINT)) {
		feed->patterns[feed->pattern_num] =
			dvb_dmx_get_pattern(DMX_IDX_VC1_ENTRY_POINT);
		feed->pattern_num++;
	}

	/* VC1 frame */
	if ((feed->pattern_num < DVB_DMX_MAX_SEARCH_PATTERN_NUM) &&
		(feed->idx_params.types &
		 (DMX_IDX_VC1_FRAME_START | DMX_IDX_VC1_FRAME_END |
		  DMX_IDX_VC1_FIRST_SEQ_FRAME_START |
		  DMX_IDX_VC1_FIRST_SEQ_FRAME_END))) {
		feed->patterns[feed->pattern_num] =
			dvb_dmx_get_pattern(DMX_IDX_VC1_FRAME_START);
		feed->pattern_num++;
	}

	if (feed->pattern_num)
		feed->rec_info->idx_info.pattern_search_feeds_num++;
}

static struct dvb_demux_rec_info *dvb_dmx_alloc_rec_info(
					struct dmx_ts_feed *ts_feed)
{
	int i;
	struct dvb_demux_feed *feed = (struct dvb_demux_feed *)ts_feed;
	struct dvb_demux *demux = feed->demux;
	struct dvb_demux_rec_info *rec_info;
	struct dvb_demux_feed *tmp_feed;

	/* check if this feed share recording buffer with other active feeds */
	list_for_each_entry(tmp_feed, &demux->feed_list, list_head) {
		if ((tmp_feed->state == DMX_STATE_GO) &&
			(tmp_feed->type == DMX_TYPE_TS) &&
			(tmp_feed != feed) &&
			(tmp_feed->feed.ts.buffer.ringbuff ==
			 ts_feed->buffer.ringbuff)) {
			/* indexing information is shared between the feeds */
			tmp_feed->rec_info->ref_count++;
			return tmp_feed->rec_info;
		}
	}

	/* Need to allocate a new indexing info */
	for (i = 0; i < demux->feednum; i++)
		if (!demux->rec_info_pool[i].ref_count)
			break;

	if (i == demux->feednum)
		return NULL;

	rec_info = &demux->rec_info_pool[i];
	rec_info->ref_count++;
	INIT_LIST_HEAD(&rec_info->idx_info.free_list);
	INIT_LIST_HEAD(&rec_info->idx_info.ready_list);

	for (i = 0; i < DMX_IDX_EVENT_QUEUE_SIZE; i++)
		list_add(&rec_info->idx_info.events[i].next,
			&rec_info->idx_info.free_list);

	rec_info->ts_output_count = 0;
	rec_info->idx_info.min_pattern_tsp_num = (u64)-1;
	rec_info->idx_info.pattern_search_feeds_num = 0;
	rec_info->idx_info.indexing_feeds_num = 0;

	return rec_info;
}

static void dvb_dmx_free_rec_info(struct dmx_ts_feed *ts_feed)
{
	struct dvb_demux_feed *feed = (struct dvb_demux_feed *)ts_feed;

	if (!feed->rec_info || !feed->rec_info->ref_count) {
		printk(KERN_ERR "%s: invalid idx info state\n", __func__);
		return;
	}

	feed->rec_info->ref_count--;

	return;
}

static int dvb_demux_feed_find(struct dvb_demux_feed *feed)
{
	struct dvb_demux_feed *entry;

	list_for_each_entry(entry, &feed->demux->feed_list, list_head)
		if (entry == feed)
			return 1;

	return 0;
}

static void dvb_demux_feed_add(struct dvb_demux_feed *feed)
{
	spin_lock_irq(&feed->demux->lock);
	if (dvb_demux_feed_find(feed)) {
		printk(KERN_ERR "%s: feed already in list (type=%x state=%x pid=%x)\n",
		       __func__, feed->type, feed->state, feed->pid);
		goto out;
	}

	list_add(&feed->list_head, &feed->demux->feed_list);
out:
	spin_unlock_irq(&feed->demux->lock);
}

static void dvb_demux_feed_del(struct dvb_demux_feed *feed)
{
	spin_lock_irq(&feed->demux->lock);
	if (!(dvb_demux_feed_find(feed))) {
		printk(KERN_ERR "%s: feed not in list (type=%x state=%x pid=%x)\n",
		       __func__, feed->type, feed->state, feed->pid);
		goto out;
	}

	list_del(&feed->list_head);
out:
	spin_unlock_irq(&feed->demux->lock);
}

static int dmx_ts_feed_set(struct dmx_ts_feed *ts_feed, u16 pid, int ts_type,
			   enum dmx_ts_pes pes_type,
			   size_t circular_buffer_size, struct timespec timeout)
{
	struct dvb_demux_feed *feed = (struct dvb_demux_feed *)ts_feed;
	struct dvb_demux *demux = feed->demux;

	if (pid > DMX_MAX_PID)
		return -EINVAL;

	if (mutex_lock_interruptible(&demux->mutex))
		return -ERESTARTSYS;

	if (ts_type & TS_DECODER) {
		if (pes_type >= DMX_PES_OTHER) {
			mutex_unlock(&demux->mutex);
			return -EINVAL;
		}

		if (demux->pesfilter[pes_type] &&
		    demux->pesfilter[pes_type] != feed) {
			mutex_unlock(&demux->mutex);
			return -EINVAL;
		}

		demux->pesfilter[pes_type] = feed;
		demux->pids[pes_type] = pid;
	}

	dvb_demux_feed_add(feed);

	feed->pid = pid;
	feed->buffer_size = circular_buffer_size;
	feed->timeout = timeout;
	feed->ts_type = ts_type;
	feed->pes_type = pes_type;

	if (feed->buffer_size) {
#ifdef NOBUFS
		feed->buffer = NULL;
#else
		feed->buffer = vmalloc(feed->buffer_size);
		if (!feed->buffer) {
			mutex_unlock(&demux->mutex);
			return -ENOMEM;
		}
#endif
	}

	feed->state = DMX_STATE_READY;
	mutex_unlock(&demux->mutex);

	return 0;
}

static int dmx_ts_feed_start_filtering(struct dmx_ts_feed *ts_feed)
{
	struct dvb_demux_feed *feed = (struct dvb_demux_feed *)ts_feed;
	struct dvb_demux *demux = feed->demux;
	int ret;

	if (mutex_lock_interruptible(&demux->mutex))
		return -ERESTARTSYS;

	if (feed->state != DMX_STATE_READY || feed->type != DMX_TYPE_TS) {
		mutex_unlock(&demux->mutex);
		return -EINVAL;
	}

	if (!demux->start_feed) {
		mutex_unlock(&demux->mutex);
		return -ENODEV;
	}

	feed->first_cc = 1;
	feed->scrambling_bits = 0;

	if ((feed->ts_type & TS_PACKET) &&
		!(feed->ts_type & TS_PAYLOAD_ONLY)) {
		feed->rec_info = dvb_dmx_alloc_rec_info(ts_feed);
		if (!feed->rec_info) {
			mutex_unlock(&demux->mutex);
			return -ENOMEM;
		}
		if (feed->idx_params.enable) {
			dvb_dmx_init_idx_state(feed);
			feed->rec_info->idx_info.indexing_feeds_num++;
			if (demux->set_indexing)
				demux->set_indexing(feed);
		}
	} else {
		feed->pattern_num = 0;
		feed->rec_info = NULL;
	}

	if ((ret = demux->start_feed(feed)) < 0) {
		if ((feed->ts_type & TS_PACKET) &&
		    !(feed->ts_type & TS_PAYLOAD_ONLY)) {
			dvb_dmx_free_rec_info(ts_feed);
			feed->rec_info = NULL;
		}
		mutex_unlock(&demux->mutex);
		return ret;
	}

	spin_lock_irq(&demux->lock);
	ts_feed->is_filtering = 1;
	feed->state = DMX_STATE_GO;
	spin_unlock_irq(&demux->lock);
	mutex_unlock(&demux->mutex);

	return 0;
}

static int dmx_ts_feed_stop_filtering(struct dmx_ts_feed *ts_feed)
{
	struct dvb_demux_feed *feed = (struct dvb_demux_feed *)ts_feed;
	struct dvb_demux *demux = feed->demux;
	int ret;

	mutex_lock(&demux->mutex);

	if (feed->state < DMX_STATE_GO) {
		mutex_unlock(&demux->mutex);
		return -EINVAL;
	}

	if (!demux->stop_feed) {
		mutex_unlock(&demux->mutex);
		return -ENODEV;
	}

	ret = demux->stop_feed(feed);

	spin_lock_irq(&demux->lock);
	ts_feed->is_filtering = 0;
	feed->state = DMX_STATE_ALLOCATED;
	spin_unlock_irq(&demux->lock);

	if (feed->rec_info) {
		if (feed->pattern_num)
			feed->rec_info->idx_info.pattern_search_feeds_num--;
		if (feed->idx_params.enable)
			feed->rec_info->idx_info.indexing_feeds_num--;
		dvb_dmx_free_rec_info(ts_feed);
		feed->rec_info = NULL;
	}

	mutex_unlock(&demux->mutex);

	return ret;
}

static int dmx_ts_feed_decoder_buff_status(struct dmx_ts_feed *ts_feed,
			struct dmx_buffer_status *dmx_buffer_status)
{
	struct dvb_demux_feed *feed = (struct dvb_demux_feed *)ts_feed;
	struct dvb_demux *demux = feed->demux;
	int ret;

	mutex_lock(&demux->mutex);

	if (feed->state < DMX_STATE_GO) {
		mutex_unlock(&demux->mutex);
		return -EINVAL;
	}

	if (!demux->decoder_buffer_status) {
		mutex_unlock(&demux->mutex);
		return -ENODEV;
	}

	ret = demux->decoder_buffer_status(feed, dmx_buffer_status);

	mutex_unlock(&demux->mutex);

	return ret;
}

static int dmx_ts_feed_reuse_decoder_buffer(struct dmx_ts_feed *ts_feed,
						int cookie)
{
	struct dvb_demux_feed *feed = (struct dvb_demux_feed *)ts_feed;
	struct dvb_demux *demux = feed->demux;
	int ret;

	mutex_lock(&demux->mutex);

	if (feed->state < DMX_STATE_GO) {
		mutex_unlock(&demux->mutex);
		return -EINVAL;
	}

	if (!demux->reuse_decoder_buffer) {
		mutex_unlock(&demux->mutex);
		return -ENODEV;
	}

	ret = demux->reuse_decoder_buffer(feed, cookie);

	mutex_unlock(&demux->mutex);

	return ret;
}

static int dmx_ts_feed_data_ready_cb(struct dmx_ts_feed *feed,
				dmx_ts_data_ready_cb callback)
{
	struct dvb_demux_feed *dvbdmxfeed = (struct dvb_demux_feed *)feed;
	struct dvb_demux *dvbdmx = dvbdmxfeed->demux;

	mutex_lock(&dvbdmx->mutex);

	if (dvbdmxfeed->state == DMX_STATE_GO) {
		mutex_unlock(&dvbdmx->mutex);
		return -EINVAL;
	}

	dvbdmxfeed->data_ready_cb.ts = callback;

	mutex_unlock(&dvbdmx->mutex);
	return 0;
}

static int dmx_ts_set_secure_mode(struct dmx_ts_feed *feed,
				struct dmx_secure_mode *secure_mode)
{
	struct dvb_demux_feed *dvbdmxfeed = (struct dvb_demux_feed *)feed;
	struct dvb_demux *dvbdmx = dvbdmxfeed->demux;

	if (mutex_lock_interruptible(&dvbdmx->mutex))
		return -ERESTARTSYS;

	if (dvbdmxfeed->state == DMX_STATE_GO) {
		mutex_unlock(&dvbdmx->mutex);
		return -EBUSY;
	}

	dvbdmxfeed->secure_mode = *secure_mode;
	mutex_unlock(&dvbdmx->mutex);
	return 0;
}

static int dmx_ts_set_cipher_ops(struct dmx_ts_feed *feed,
				struct dmx_cipher_operations *cipher_ops)
{
	struct dvb_demux_feed *dvbdmxfeed = (struct dvb_demux_feed *)feed;
	struct dvb_demux *dvbdmx = dvbdmxfeed->demux;
	int ret = 0;

	if (mutex_lock_interruptible(&dvbdmx->mutex))
		return -ERESTARTSYS;

	if ((dvbdmxfeed->state == DMX_STATE_GO) &&
		dvbdmx->set_cipher_op)
		ret = dvbdmx->set_cipher_op(dvbdmxfeed, cipher_ops);

	if (!ret)
		dvbdmxfeed->cipher_ops = *cipher_ops;

	mutex_unlock(&dvbdmx->mutex);
	return ret;
}

static int dmx_ts_set_video_codec(
	struct dmx_ts_feed *ts_feed,
	enum dmx_video_codec video_codec)
{
	struct dvb_demux_feed *feed = (struct dvb_demux_feed *)ts_feed;

	feed->video_codec = video_codec;

	return 0;
}

static int dmx_ts_set_idx_params(struct dmx_ts_feed *ts_feed,
	struct dmx_indexing_params *idx_params)
{
	struct dvb_demux_feed *feed = (struct dvb_demux_feed *)ts_feed;
	struct dvb_demux *dvbdmx = feed->demux;
	int idx_enabled;
	int ret = 0;

	mutex_lock(&dvbdmx->mutex);

	if ((feed->state == DMX_STATE_GO) &&
		!feed->rec_info) {
		mutex_unlock(&dvbdmx->mutex);
		return -EINVAL;
	}

	idx_enabled = feed->idx_params.enable;
	feed->idx_params = *idx_params;

	if (feed->state == DMX_STATE_GO) {
		spin_lock_irq(&dvbdmx->lock);
		if (feed->pattern_num)
			feed->rec_info->idx_info.pattern_search_feeds_num--;
		if (idx_enabled && !idx_params->enable)
			feed->rec_info->idx_info.indexing_feeds_num--;
		if (!idx_enabled && idx_params->enable)
			feed->rec_info->idx_info.indexing_feeds_num++;
		dvb_dmx_init_idx_state(feed);
		spin_unlock_irq(&dvbdmx->lock);

		if (dvbdmx->set_indexing)
			ret = dvbdmx->set_indexing(feed);
	}

	mutex_unlock(&dvbdmx->mutex);

	return ret;
}

static int dvbdmx_ts_feed_oob_cmd(struct dmx_ts_feed *ts_feed,
		struct dmx_oob_command *cmd)
{
	struct dvb_demux_feed *feed = (struct dvb_demux_feed *)ts_feed;
	struct dmx_data_ready data;
	struct dvb_demux *dvbdmx = feed->demux;
	int ret = 0;
	int secure_non_rec = feed->secure_mode.is_secured &&
		!dvb_dmx_is_rec_feed(feed);

	mutex_lock(&dvbdmx->mutex);

	if (feed->state != DMX_STATE_GO) {
		mutex_unlock(&dvbdmx->mutex);
		return -EINVAL;
	}

	/* Decoder & non-recording secure feeds are handled by plug-in */
	if ((feed->ts_type & TS_DECODER) || secure_non_rec) {
		if (feed->demux->oob_command)
			ret = feed->demux->oob_command(feed, cmd);
	}

	if (!(feed->ts_type & (TS_PAYLOAD_ONLY | TS_PACKET)) ||
		secure_non_rec) {
		mutex_unlock(&dvbdmx->mutex);
		return ret;
	}

	data.data_length = 0;

	switch (cmd->type) {
	case DMX_OOB_CMD_EOS:
		if (feed->ts_type & TS_PAYLOAD_ONLY)
			dvb_dmx_check_pes_end(feed);

		data.status = DMX_OK_EOS;
		ret = feed->data_ready_cb.ts(&feed->feed.ts, &data);
		break;

	case DMX_OOB_CMD_MARKER:
		data.status = DMX_OK_MARKER;
		data.marker.id = cmd->params.marker.id;
		ret = feed->data_ready_cb.ts(&feed->feed.ts, &data);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&dvbdmx->mutex);
	return ret;
}

static int dvbdmx_ts_get_scrambling_bits(struct dmx_ts_feed *ts_feed,
			u8 *value)
{
	struct dvb_demux_feed *feed = (struct dvb_demux_feed *)ts_feed;
	struct dvb_demux *demux = feed->demux;

	spin_lock(&demux->lock);

	if (!ts_feed->is_filtering) {
		spin_unlock(&demux->lock);
		return -EINVAL;
	}

	*value = feed->scrambling_bits;
	spin_unlock(&demux->lock);

	return 0;
}

static int dvbdmx_ts_insertion_insert_buffer(struct dmx_ts_feed *ts_feed,
			char *data, size_t size)
{
	struct dvb_demux_feed *feed = (struct dvb_demux_feed *)ts_feed;
	struct dvb_demux *demux = feed->demux;

	spin_lock(&demux->lock);
	if (!ts_feed->is_filtering) {
		spin_unlock(&demux->lock);
		return 0;
	}

	feed->cb.ts(data, size, NULL, 0, ts_feed, DMX_OK);

	spin_unlock(&demux->lock);

	return 0;
}

static int dmx_ts_set_tsp_out_format(
	struct dmx_ts_feed *ts_feed,
	enum dmx_tsp_format_t tsp_format)
{
	struct dvb_demux_feed *feed = (struct dvb_demux_feed *)ts_feed;
	struct dvb_demux *dvbdmx = feed->demux;

	mutex_lock(&dvbdmx->mutex);

	if (feed->state == DMX_STATE_GO) {
		mutex_unlock(&dvbdmx->mutex);
		return -EINVAL;
	}

	feed->tsp_out_format = tsp_format;
	mutex_unlock(&dvbdmx->mutex);
	return 0;
}

/**
 * dvbdmx_ts_reset_pes_state() - Reset the current PES length and PES counters
 *
 * @feed: dvb demux feed object
 */
void dvbdmx_ts_reset_pes_state(struct dvb_demux_feed *feed)
{
	unsigned long flags;

	/*
	 * Reset PES state.
	 * PUSI seen indication is kept so we can get partial PES.
	 */
	spin_lock_irqsave(&feed->demux->lock, flags);

	feed->peslen = 0;
	feed->pes_tei_counter = 0;
	feed->pes_cont_err_counter = 0;
	feed->pes_ts_packets_num = 0;

	spin_unlock_irqrestore(&feed->demux->lock, flags);
}
EXPORT_SYMBOL(dvbdmx_ts_reset_pes_state);

static int dvbdmx_ts_flush_buffer(struct dmx_ts_feed *ts_feed, size_t length)
{
	struct dvb_demux_feed *feed = (struct dvb_demux_feed *)ts_feed;
	struct dvb_demux *demux = feed->demux;
	int ret = 0;

	if (mutex_lock_interruptible(&demux->mutex))
		return -ERESTARTSYS;

	dvbdmx_ts_reset_pes_state(feed);

	if ((feed->ts_type & TS_DECODER) && demux->flush_decoder_buffer)
		/* Call decoder specific flushing if one exists */
		ret = demux->flush_decoder_buffer(feed, length);

	mutex_unlock(&demux->mutex);
	return ret;
}

static int dvbdmx_allocate_ts_feed(struct dmx_demux *dmx,
				   struct dmx_ts_feed **ts_feed,
				   dmx_ts_cb callback)
{
	struct dvb_demux *demux = (struct dvb_demux *)dmx;
	struct dvb_demux_feed *feed;

	if (mutex_lock_interruptible(&demux->mutex))
		return -ERESTARTSYS;

	if (!(feed = dvb_dmx_feed_alloc(demux))) {
		mutex_unlock(&demux->mutex);
		return -EBUSY;
	}

	feed->type = DMX_TYPE_TS;
	feed->cb.ts = callback;
	feed->demux = demux;
	feed->pid = 0xffff;
	feed->peslen = 0;
	feed->pes_tei_counter = 0;
	feed->pes_ts_packets_num = 0;
	feed->pes_cont_err_counter = 0;
	feed->secure_mode.is_secured = 0;
	feed->buffer = NULL;
	feed->tsp_out_format = DMX_TSP_FORMAT_188;
	feed->idx_params.enable = 0;

	/* default behaviour - pass first PES data even if it is
	 * partial PES data from previous PES that we didn't receive its header.
	 * Override this to 0 in your start_feed function in order to handle
	 * first PES differently.
	 */
	feed->pusi_seen = 1;

	(*ts_feed) = &feed->feed.ts;
	(*ts_feed)->parent = dmx;
	(*ts_feed)->priv = NULL;
	(*ts_feed)->is_filtering = 0;
	(*ts_feed)->start_filtering = dmx_ts_feed_start_filtering;
	(*ts_feed)->stop_filtering = dmx_ts_feed_stop_filtering;
	(*ts_feed)->set = dmx_ts_feed_set;
	(*ts_feed)->set_video_codec = dmx_ts_set_video_codec;
	(*ts_feed)->set_idx_params = dmx_ts_set_idx_params;
	(*ts_feed)->set_tsp_out_format = dmx_ts_set_tsp_out_format;
	(*ts_feed)->get_decoder_buff_status = dmx_ts_feed_decoder_buff_status;
	(*ts_feed)->reuse_decoder_buffer = dmx_ts_feed_reuse_decoder_buffer;
	(*ts_feed)->data_ready_cb = dmx_ts_feed_data_ready_cb;
	(*ts_feed)->notify_data_read = NULL;
	(*ts_feed)->set_secure_mode = dmx_ts_set_secure_mode;
	(*ts_feed)->set_cipher_ops = dmx_ts_set_cipher_ops;
	(*ts_feed)->oob_command = dvbdmx_ts_feed_oob_cmd;
	(*ts_feed)->get_scrambling_bits = dvbdmx_ts_get_scrambling_bits;
	(*ts_feed)->ts_insertion_init = NULL;
	(*ts_feed)->ts_insertion_terminate = NULL;
	(*ts_feed)->ts_insertion_insert_buffer =
		dvbdmx_ts_insertion_insert_buffer;
	(*ts_feed)->flush_buffer = dvbdmx_ts_flush_buffer;

	if (!(feed->filter = dvb_dmx_filter_alloc(demux))) {
		feed->state = DMX_STATE_FREE;
		mutex_unlock(&demux->mutex);
		return -EBUSY;
	}

	feed->filter->type = DMX_TYPE_TS;
	feed->filter->feed = feed;
	feed->filter->state = DMX_STATE_READY;

	mutex_unlock(&demux->mutex);

	return 0;
}

static int dvbdmx_release_ts_feed(struct dmx_demux *dmx,
				  struct dmx_ts_feed *ts_feed)
{
	struct dvb_demux *demux = (struct dvb_demux *)dmx;
	struct dvb_demux_feed *feed = (struct dvb_demux_feed *)ts_feed;

	mutex_lock(&demux->mutex);

	if (feed->state == DMX_STATE_FREE) {
		mutex_unlock(&demux->mutex);
		return -EINVAL;
	}
#ifndef NOBUFS
	vfree(feed->buffer);
	feed->buffer = NULL;
#endif

	feed->state = DMX_STATE_FREE;
	feed->filter->state = DMX_STATE_FREE;
	ts_feed->priv = NULL;
	dvb_demux_feed_del(feed);

	feed->pid = 0xffff;

	if (feed->ts_type & TS_DECODER && feed->pes_type < DMX_PES_OTHER)
		demux->pesfilter[feed->pes_type] = NULL;

	mutex_unlock(&demux->mutex);
	return 0;
}

/******************************************************************************
 * dmx_section_feed API calls
 ******************************************************************************/

static int dmx_section_feed_allocate_filter(struct dmx_section_feed *feed,
					    struct dmx_section_filter **filter)
{
	struct dvb_demux_feed *dvbdmxfeed = (struct dvb_demux_feed *)feed;
	struct dvb_demux *dvbdemux = dvbdmxfeed->demux;
	struct dvb_demux_filter *dvbdmxfilter;

	if (mutex_lock_interruptible(&dvbdemux->mutex))
		return -ERESTARTSYS;

	dvbdmxfilter = dvb_dmx_filter_alloc(dvbdemux);
	if (!dvbdmxfilter) {
		mutex_unlock(&dvbdemux->mutex);
		return -EBUSY;
	}

	spin_lock_irq(&dvbdemux->lock);
	*filter = &dvbdmxfilter->filter;
	(*filter)->parent = feed;
	(*filter)->priv = NULL;
	dvbdmxfilter->feed = dvbdmxfeed;
	dvbdmxfilter->type = DMX_TYPE_SEC;
	dvbdmxfilter->state = DMX_STATE_READY;
	dvbdmxfilter->next = dvbdmxfeed->filter;
	dvbdmxfeed->filter = dvbdmxfilter;
	spin_unlock_irq(&dvbdemux->lock);

	mutex_unlock(&dvbdemux->mutex);
	return 0;
}

static int dmx_section_feed_set(struct dmx_section_feed *feed,
				u16 pid, size_t circular_buffer_size,
				int check_crc)
{
	struct dvb_demux_feed *dvbdmxfeed = (struct dvb_demux_feed *)feed;
	struct dvb_demux *dvbdmx = dvbdmxfeed->demux;

	if (pid > 0x1fff)
		return -EINVAL;

	if (mutex_lock_interruptible(&dvbdmx->mutex))
		return -ERESTARTSYS;

	dvb_demux_feed_add(dvbdmxfeed);

	dvbdmxfeed->pid = pid;
	dvbdmxfeed->buffer_size = circular_buffer_size;
	dvbdmxfeed->feed.sec.check_crc = check_crc;

#ifdef NOBUFS
	dvbdmxfeed->buffer = NULL;
#else
	dvbdmxfeed->buffer = vmalloc(dvbdmxfeed->buffer_size);
	if (!dvbdmxfeed->buffer) {
		mutex_unlock(&dvbdmx->mutex);
		return -ENOMEM;
	}
#endif

	dvbdmxfeed->state = DMX_STATE_READY;
	mutex_unlock(&dvbdmx->mutex);
	return 0;
}

static void prepare_secfilters(struct dvb_demux_feed *dvbdmxfeed)
{
	int i;
	struct dvb_demux_filter *f;
	struct dmx_section_filter *sf;
	u8 mask, mode, doneq;

	if (!(f = dvbdmxfeed->filter))
		return;
	do {
		sf = &f->filter;
		doneq = 0;
		for (i = 0; i < DVB_DEMUX_MASK_MAX; i++) {
			mode = sf->filter_mode[i];
			mask = sf->filter_mask[i];
			f->maskandmode[i] = mask & mode;
			doneq |= f->maskandnotmode[i] = mask & ~mode;
		}
		f->doneq = doneq ? 1 : 0;
	} while ((f = f->next));
}

static int dmx_section_feed_start_filtering(struct dmx_section_feed *feed)
{
	struct dvb_demux_feed *dvbdmxfeed = (struct dvb_demux_feed *)feed;
	struct dvb_demux *dvbdmx = dvbdmxfeed->demux;
	int ret;

	if (mutex_lock_interruptible(&dvbdmx->mutex))
		return -ERESTARTSYS;

	if (feed->is_filtering) {
		mutex_unlock(&dvbdmx->mutex);
		return -EBUSY;
	}

	if (!dvbdmxfeed->filter) {
		mutex_unlock(&dvbdmx->mutex);
		return -EINVAL;
	}

	dvbdmxfeed->feed.sec.tsfeedp = 0;
	dvbdmxfeed->feed.sec.secbuf = dvbdmxfeed->feed.sec.secbuf_base;
	dvbdmxfeed->feed.sec.secbufp = 0;
	dvbdmxfeed->feed.sec.seclen = 0;
	dvbdmxfeed->first_cc = 1;
	dvbdmxfeed->scrambling_bits = 0;

	if (!dvbdmx->start_feed) {
		mutex_unlock(&dvbdmx->mutex);
		return -ENODEV;
	}

	prepare_secfilters(dvbdmxfeed);

	if ((ret = dvbdmx->start_feed(dvbdmxfeed)) < 0) {
		mutex_unlock(&dvbdmx->mutex);
		return ret;
	}

	spin_lock_irq(&dvbdmx->lock);
	feed->is_filtering = 1;
	dvbdmxfeed->state = DMX_STATE_GO;
	spin_unlock_irq(&dvbdmx->lock);

	mutex_unlock(&dvbdmx->mutex);
	return 0;
}

static int dmx_section_feed_stop_filtering(struct dmx_section_feed *feed)
{
	struct dvb_demux_feed *dvbdmxfeed = (struct dvb_demux_feed *)feed;
	struct dvb_demux *dvbdmx = dvbdmxfeed->demux;
	int ret;

	mutex_lock(&dvbdmx->mutex);

	if (!dvbdmx->stop_feed) {
		mutex_unlock(&dvbdmx->mutex);
		return -ENODEV;
	}

	ret = dvbdmx->stop_feed(dvbdmxfeed);

	spin_lock_irq(&dvbdmx->lock);
	dvbdmxfeed->state = DMX_STATE_READY;
	feed->is_filtering = 0;
	spin_unlock_irq(&dvbdmx->lock);

	mutex_unlock(&dvbdmx->mutex);
	return ret;
}


static int dmx_section_feed_data_ready_cb(struct dmx_section_feed *feed,
				dmx_section_data_ready_cb callback)
{
	struct dvb_demux_feed *dvbdmxfeed = (struct dvb_demux_feed *)feed;
	struct dvb_demux *dvbdmx = dvbdmxfeed->demux;

	mutex_lock(&dvbdmx->mutex);

	if (dvbdmxfeed->state == DMX_STATE_GO) {
		mutex_unlock(&dvbdmx->mutex);
		return -EINVAL;
	}

	dvbdmxfeed->data_ready_cb.sec = callback;

	mutex_unlock(&dvbdmx->mutex);
	return 0;
}

static int dmx_section_set_secure_mode(struct dmx_section_feed *feed,
				struct dmx_secure_mode *secure_mode)
{
	struct dvb_demux_feed *dvbdmxfeed = (struct dvb_demux_feed *)feed;
	struct dvb_demux *dvbdmx = dvbdmxfeed->demux;

	mutex_lock(&dvbdmx->mutex);

	if (dvbdmxfeed->state == DMX_STATE_GO) {
		mutex_unlock(&dvbdmx->mutex);
		return -EBUSY;
	}

	dvbdmxfeed->secure_mode = *secure_mode;
	mutex_unlock(&dvbdmx->mutex);
	return 0;
}

static int dmx_section_set_cipher_ops(struct dmx_section_feed *feed,
				struct dmx_cipher_operations *cipher_ops)
{
	struct dvb_demux_feed *dvbdmxfeed = (struct dvb_demux_feed *)feed;
	struct dvb_demux *dvbdmx = dvbdmxfeed->demux;
	int ret = 0;

	if (mutex_lock_interruptible(&dvbdmx->mutex))
		return -ERESTARTSYS;

	if ((dvbdmxfeed->state == DMX_STATE_GO) &&
		dvbdmx->set_cipher_op) {
		ret = dvbdmx->set_cipher_op(dvbdmxfeed, cipher_ops);
	}

	if (!ret)
		dvbdmxfeed->cipher_ops = *cipher_ops;

	mutex_unlock(&dvbdmx->mutex);
	return ret;
}

static int dmx_section_feed_release_filter(struct dmx_section_feed *feed,
					   struct dmx_section_filter *filter)
{
	struct dvb_demux_filter *dvbdmxfilter = (struct dvb_demux_filter *)filter, *f;
	struct dvb_demux_feed *dvbdmxfeed = (struct dvb_demux_feed *)feed;
	struct dvb_demux *dvbdmx = dvbdmxfeed->demux;

	mutex_lock(&dvbdmx->mutex);

	if (dvbdmxfilter->feed != dvbdmxfeed) {
		mutex_unlock(&dvbdmx->mutex);
		return -EINVAL;
	}

	if (feed->is_filtering)
		feed->stop_filtering(feed);

	spin_lock_irq(&dvbdmx->lock);
	f = dvbdmxfeed->filter;

	if (f == dvbdmxfilter) {
		dvbdmxfeed->filter = dvbdmxfilter->next;
	} else {
		while (f->next != dvbdmxfilter)
			f = f->next;
		f->next = f->next->next;
	}

	filter->priv = NULL;
	dvbdmxfilter->state = DMX_STATE_FREE;
	spin_unlock_irq(&dvbdmx->lock);
	mutex_unlock(&dvbdmx->mutex);
	return 0;
}

static int dvbdmx_section_feed_oob_cmd(struct dmx_section_feed *section_feed,
		struct dmx_oob_command *cmd)
{
	struct dvb_demux_feed *feed = (struct dvb_demux_feed *)section_feed;
	struct dvb_demux *dvbdmx = feed->demux;
	struct dmx_data_ready data;
	int ret = 0;

	data.data_length = 0;

	mutex_lock(&dvbdmx->mutex);

	if (feed->state != DMX_STATE_GO) {
		mutex_unlock(&dvbdmx->mutex);
		return -EINVAL;
	}

	/* Secure section feeds are handled by the plug-in */
	if (feed->secure_mode.is_secured) {
		if (feed->demux->oob_command)
			ret = feed->demux->oob_command(feed, cmd);
		else
			ret = 0;

		mutex_unlock(&dvbdmx->mutex);
		return ret;
	}

	switch (cmd->type) {
	case DMX_OOB_CMD_EOS:
		data.status = DMX_OK_EOS;
		break;

	case DMX_OOB_CMD_MARKER:
		data.status = DMX_OK_MARKER;
		data.marker.id = cmd->params.marker.id;
		break;

	default:
		ret = -EINVAL;
		break;
	}

	if (!ret)
		ret = dvb_dmx_notify_section_event(feed, &data, 1);

	mutex_unlock(&dvbdmx->mutex);
	return ret;
}

static int dvbdmx_section_get_scrambling_bits(
	struct dmx_section_feed *section_feed, u8 *value)
{
	struct dvb_demux_feed *feed = (struct dvb_demux_feed *)section_feed;
	struct dvb_demux *demux = feed->demux;

	spin_lock(&demux->lock);

	if (!section_feed->is_filtering) {
		spin_unlock(&demux->lock);
		return -EINVAL;
	}

	*value = feed->scrambling_bits;
	spin_unlock(&demux->lock);

	return 0;
}

static int dvbdmx_allocate_section_feed(struct dmx_demux *demux,
					struct dmx_section_feed **feed,
					dmx_section_cb callback)
{
	struct dvb_demux *dvbdmx = (struct dvb_demux *)demux;
	struct dvb_demux_feed *dvbdmxfeed;

	if (mutex_lock_interruptible(&dvbdmx->mutex))
		return -ERESTARTSYS;

	if (!(dvbdmxfeed = dvb_dmx_feed_alloc(dvbdmx))) {
		mutex_unlock(&dvbdmx->mutex);
		return -EBUSY;
	}

	dvbdmxfeed->type = DMX_TYPE_SEC;
	dvbdmxfeed->cb.sec = callback;
	dvbdmxfeed->demux = dvbdmx;
	dvbdmxfeed->pid = 0xffff;
	dvbdmxfeed->secure_mode.is_secured = 0;
	dvbdmxfeed->tsp_out_format = DMX_TSP_FORMAT_188;
	dvbdmxfeed->feed.sec.secbuf = dvbdmxfeed->feed.sec.secbuf_base;
	dvbdmxfeed->feed.sec.secbufp = dvbdmxfeed->feed.sec.seclen = 0;
	dvbdmxfeed->feed.sec.tsfeedp = 0;
	dvbdmxfeed->filter = NULL;
	dvbdmxfeed->buffer = NULL;
	dvbdmxfeed->idx_params.enable = 0;

	(*feed) = &dvbdmxfeed->feed.sec;
	(*feed)->is_filtering = 0;
	(*feed)->parent = demux;
	(*feed)->priv = NULL;

	(*feed)->set = dmx_section_feed_set;
	(*feed)->allocate_filter = dmx_section_feed_allocate_filter;
	(*feed)->start_filtering = dmx_section_feed_start_filtering;
	(*feed)->stop_filtering = dmx_section_feed_stop_filtering;
	(*feed)->release_filter = dmx_section_feed_release_filter;
	(*feed)->data_ready_cb = dmx_section_feed_data_ready_cb;
	(*feed)->notify_data_read = NULL;
	(*feed)->set_secure_mode = dmx_section_set_secure_mode;
	(*feed)->set_cipher_ops = dmx_section_set_cipher_ops;
	(*feed)->oob_command = dvbdmx_section_feed_oob_cmd;
	(*feed)->get_scrambling_bits = dvbdmx_section_get_scrambling_bits;
	(*feed)->flush_buffer = NULL;

	mutex_unlock(&dvbdmx->mutex);
	return 0;
}

static int dvbdmx_release_section_feed(struct dmx_demux *demux,
				       struct dmx_section_feed *feed)
{
	struct dvb_demux_feed *dvbdmxfeed = (struct dvb_demux_feed *)feed;
	struct dvb_demux *dvbdmx = (struct dvb_demux *)demux;

	mutex_lock(&dvbdmx->mutex);

	if (dvbdmxfeed->state == DMX_STATE_FREE) {
		mutex_unlock(&dvbdmx->mutex);
		return -EINVAL;
	}
#ifndef NOBUFS
	vfree(dvbdmxfeed->buffer);
	dvbdmxfeed->buffer = NULL;
#endif
	dvbdmxfeed->state = DMX_STATE_FREE;
	feed->priv = NULL;
	dvb_demux_feed_del(dvbdmxfeed);

	dvbdmxfeed->pid = 0xffff;

	mutex_unlock(&dvbdmx->mutex);
	return 0;
}

/******************************************************************************
 * dvb_demux kernel data API calls
 ******************************************************************************/

static int dvbdmx_open(struct dmx_demux *demux)
{
	struct dvb_demux *dvbdemux = (struct dvb_demux *)demux;

	if (dvbdemux->users >= MAX_DVB_DEMUX_USERS)
		return -EUSERS;

	dvbdemux->users++;
	return 0;
}

static int dvbdmx_close(struct dmx_demux *demux)
{
	struct dvb_demux *dvbdemux = (struct dvb_demux *)demux;

	if (dvbdemux->users == 0)
		return -ENODEV;

	dvbdemux->users--;
	//FIXME: release any unneeded resources if users==0
	return 0;
}

static int dvbdmx_write(struct dmx_demux *demux, const char *buf, size_t count)
{
	struct dvb_demux *dvbdemux = (struct dvb_demux *)demux;

	if (!demux->frontend || !buf || demux->dvr_input_protected ||
		(demux->frontend->source != DMX_MEMORY_FE)) {
		return -EINVAL;
	}

	dvb_dmx_swfilter_format(dvbdemux, buf, count, dvbdemux->tsp_format);

	if (signal_pending(current))
		return -EINTR;
	return count;
}

static int dvbdmx_write_cancel(struct dmx_demux *demux)
{
	struct dvb_demux *dvbdmx = (struct dvb_demux *)demux;

	spin_lock_irq(&dvbdmx->lock);

	/* cancel any pending wait for decoder's buffers */
	dvbdmx->sw_filter_abort = 1;
	dvbdmx->tsbufp = 0;
	dvb_dmx_configure_decoder_fullness(dvbdmx, 0);

	spin_unlock_irq(&dvbdmx->lock);

	return 0;
}

static int dvbdmx_set_playback_mode(struct dmx_demux *demux,
				 enum dmx_playback_mode_t mode,
				 dmx_ts_fullness ts_fullness_callback,
				 dmx_section_fullness sec_fullness_callback)
{
	struct dvb_demux *dvbdmx = (struct dvb_demux *)demux;

	mutex_lock(&dvbdmx->mutex);

	dvbdmx->playback_mode = mode;
	dvbdmx->buffer_ctrl.ts = ts_fullness_callback;
	dvbdmx->buffer_ctrl.sec = sec_fullness_callback;

	mutex_unlock(&dvbdmx->mutex);

	return 0;
}

static int dvbdmx_add_frontend(struct dmx_demux *demux,
			       struct dmx_frontend *frontend)
{
	struct dvb_demux *dvbdemux = (struct dvb_demux *)demux;
	struct list_head *head = &dvbdemux->frontend_list;

	list_add(&(frontend->connectivity_list), head);

	return 0;
}

static int dvbdmx_remove_frontend(struct dmx_demux *demux,
				  struct dmx_frontend *frontend)
{
	struct dvb_demux *dvbdemux = (struct dvb_demux *)demux;
	struct list_head *pos, *n, *head = &dvbdemux->frontend_list;

	list_for_each_safe(pos, n, head) {
		if (DMX_FE_ENTRY(pos) == frontend) {
			list_del(pos);
			return 0;
		}
	}

	return -ENODEV;
}

static struct list_head *dvbdmx_get_frontends(struct dmx_demux *demux)
{
	struct dvb_demux *dvbdemux = (struct dvb_demux *)demux;

	if (list_empty(&dvbdemux->frontend_list))
		return NULL;

	return &dvbdemux->frontend_list;
}

static int dvbdmx_connect_frontend(struct dmx_demux *demux,
				   struct dmx_frontend *frontend)
{
	struct dvb_demux *dvbdemux = (struct dvb_demux *)demux;

	if (demux->frontend)
		return -EINVAL;

	mutex_lock(&dvbdemux->mutex);

	demux->frontend = frontend;
	mutex_unlock(&dvbdemux->mutex);
	return 0;
}

static int dvbdmx_disconnect_frontend(struct dmx_demux *demux)
{
	struct dvb_demux *dvbdemux = (struct dvb_demux *)demux;

	mutex_lock(&dvbdemux->mutex);
	dvbdemux->sw_filter_abort = 0;
	demux->frontend = NULL;
	mutex_unlock(&dvbdemux->mutex);
	return 0;
}

static int dvbdmx_get_pes_pids(struct dmx_demux *demux, u16 * pids)
{
	struct dvb_demux *dvbdemux = (struct dvb_demux *)demux;

	memcpy(pids, dvbdemux->pids, 5 * sizeof(u16));
	return 0;
}

static int dvbdmx_get_tsp_size(struct dmx_demux *demux)
{
	int tsp_size;
	struct dvb_demux *dvbdemux = (struct dvb_demux *)demux;

	mutex_lock(&dvbdemux->mutex);
	tsp_size = dvbdemux->ts_packet_size;
	mutex_unlock(&dvbdemux->mutex);

	return tsp_size;
}

static int dvbdmx_set_tsp_format(
	struct dmx_demux *demux,
	enum dmx_tsp_format_t tsp_format)
{
	struct dvb_demux *dvbdemux = (struct dvb_demux *)demux;

	if ((tsp_format > DMX_TSP_FORMAT_204) ||
		(tsp_format < DMX_TSP_FORMAT_188))
		return -EINVAL;

	mutex_lock(&dvbdemux->mutex);

	dvbdemux->tsp_format = tsp_format;
	switch (tsp_format) {
	case DMX_TSP_FORMAT_188:
		dvbdemux->ts_packet_size = 188;
		break;
	case DMX_TSP_FORMAT_192_TAIL:
	case DMX_TSP_FORMAT_192_HEAD:
		dvbdemux->ts_packet_size = 192;
		break;
	case DMX_TSP_FORMAT_204:
		dvbdemux->ts_packet_size = 204;
		break;
	}

	mutex_unlock(&dvbdemux->mutex);
	return 0;
}

int dvb_dmx_init(struct dvb_demux *dvbdemux)
{
	int i;
	struct dmx_demux *dmx = &dvbdemux->dmx;

	dvbdemux->cnt_storage = NULL;
	dvbdemux->users = 0;
	dvbdemux->filter = vmalloc(dvbdemux->filternum * sizeof(struct dvb_demux_filter));

	if (!dvbdemux->filter)
		return -ENOMEM;

	dvbdemux->feed = vmalloc(dvbdemux->feednum * sizeof(struct dvb_demux_feed));
	if (!dvbdemux->feed) {
		vfree(dvbdemux->filter);
		dvbdemux->filter = NULL;
		return -ENOMEM;
	}

	dvbdemux->rec_info_pool = vmalloc(dvbdemux->feednum *
		sizeof(struct dvb_demux_rec_info));
	if (!dvbdemux->rec_info_pool) {
		vfree(dvbdemux->feed);
		vfree(dvbdemux->filter);
		dvbdemux->feed = NULL;
		dvbdemux->filter = NULL;
		return -ENOMEM;
	}

	dvbdemux->sw_filter_abort = 0;
	dvbdemux->total_process_time = 0;
	dvbdemux->total_crc_time = 0;
	snprintf(dvbdemux->alias,
			MAX_DVB_DEMUX_NAME_LEN,
			"demux%d",
			dvb_demux_index++);

	dvbdemux->dmx.debugfs_demux_dir =
		debugfs_create_dir(dvbdemux->alias, NULL);

	if (dvbdemux->dmx.debugfs_demux_dir != NULL) {
		debugfs_create_u32(
			"total_processing_time",
			S_IRUGO | S_IWUSR | S_IWGRP,
			dvbdemux->dmx.debugfs_demux_dir,
			&dvbdemux->total_process_time);

		debugfs_create_u32(
			"total_crc_time",
			S_IRUGO | S_IWUSR | S_IWGRP,
			dvbdemux->dmx.debugfs_demux_dir,
			&dvbdemux->total_crc_time);
	}

	for (i = 0; i < dvbdemux->filternum; i++) {
		dvbdemux->filter[i].state = DMX_STATE_FREE;
		dvbdemux->filter[i].index = i;
	}

	for (i = 0; i < dvbdemux->feednum; i++) {
		dvbdemux->feed[i].state = DMX_STATE_FREE;
		dvbdemux->feed[i].index = i;

		dvbdemux->rec_info_pool[i].ref_count = 0;
	}

	dvbdemux->cnt_storage = vmalloc(MAX_PID + 1);
	if (!dvbdemux->cnt_storage)
		printk(KERN_WARNING "Couldn't allocate memory for TS/TEI check. Disabling it\n");

	INIT_LIST_HEAD(&dvbdemux->frontend_list);

	for (i = 0; i < DMX_PES_OTHER; i++) {
		dvbdemux->pesfilter[i] = NULL;
		dvbdemux->pids[i] = 0xffff;
	}

	INIT_LIST_HEAD(&dvbdemux->feed_list);

	dvbdemux->playing = 0;
	dvbdemux->recording = 0;
	dvbdemux->tsbufp = 0;

	dvbdemux->tsp_format = DMX_TSP_FORMAT_188;
	dvbdemux->ts_packet_size = 188;

	if (!dvbdemux->check_crc32)
		dvbdemux->check_crc32 = dvb_dmx_crc32;

	if (!dvbdemux->memcopy)
		dvbdemux->memcopy = dvb_dmx_memcopy;

	dmx->frontend = NULL;
	dmx->priv = dvbdemux;
	dmx->open = dvbdmx_open;
	dmx->close = dvbdmx_close;
	dmx->write = dvbdmx_write;
	dmx->write_cancel = dvbdmx_write_cancel;
	dmx->set_playback_mode = dvbdmx_set_playback_mode;
	dmx->allocate_ts_feed = dvbdmx_allocate_ts_feed;
	dmx->release_ts_feed = dvbdmx_release_ts_feed;
	dmx->allocate_section_feed = dvbdmx_allocate_section_feed;
	dmx->release_section_feed = dvbdmx_release_section_feed;
	dmx->map_buffer = NULL;
	dmx->unmap_buffer = NULL;

	dmx->add_frontend = dvbdmx_add_frontend;
	dmx->remove_frontend = dvbdmx_remove_frontend;
	dmx->get_frontends = dvbdmx_get_frontends;
	dmx->connect_frontend = dvbdmx_connect_frontend;
	dmx->disconnect_frontend = dvbdmx_disconnect_frontend;
	dmx->get_pes_pids = dvbdmx_get_pes_pids;

	dmx->set_tsp_format = dvbdmx_set_tsp_format;
	dmx->get_tsp_size = dvbdmx_get_tsp_size;

	mutex_init(&dvbdemux->mutex);
	spin_lock_init(&dvbdemux->lock);

	return 0;
}

EXPORT_SYMBOL(dvb_dmx_init);

void dvb_dmx_release(struct dvb_demux *dvbdemux)
{
	if (dvbdemux->dmx.debugfs_demux_dir != NULL)
		debugfs_remove_recursive(dvbdemux->dmx.debugfs_demux_dir);

	dvb_demux_index--;
	vfree(dvbdemux->cnt_storage);
	vfree(dvbdemux->filter);
	vfree(dvbdemux->feed);
	vfree(dvbdemux->rec_info_pool);
}

EXPORT_SYMBOL(dvb_dmx_release);
