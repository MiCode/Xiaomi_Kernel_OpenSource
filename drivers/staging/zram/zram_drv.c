/*
 * Compressed RAM block device
 *
 * Copyright (C) 2008, 2009, 2010  Nitin Gupta
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This code is released using a dual license strategy: BSD/GPL
 * You can choose the licence that better fits your requirements.
 *
 * Released under the terms of 3-clause BSD License
 * Released under the terms of GNU General Public License Version 2.0
 *
 * Project home: http://compcache.googlecode.com
 */

#define KMSG_COMPONENT "zram"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#ifdef CONFIG_ZRAM_DEBUG
#define DEBUG
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/bio.h>
#include <linux/bitops.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/device.h>
#include <linux/genhd.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/lzo.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#ifdef CONFIG_ZSM
#include <linux/rbtree.h>
#include <linux/time.h>
#endif
#include "zram_drv.h"

#ifdef CONFIG_MT_ENG_BUILD
#define GUIDE_BYTES_LENGTH	64
#define GUIDE_BYTES_HALFLEN	32
#define GUIDE_BYTES		(0x0)
#endif

/* Globals */
static int zram_major;
struct zram *zram_devices = NULL;

/* Compression/Decompression hooks */
static comp_hook zram_compress = NULL;
static decomp_hook zram_decompress = NULL;
static const char *zram_comp = NULL;
#ifdef CONFIG_ZSM
static struct rb_root root_zram_tree = RB_ROOT;
static struct rb_root root_zram_tree_4k = RB_ROOT;
spinlock_t zram_node_mutex;
spinlock_t zram_node4k_mutex;

static int zram_test_flag(struct zram_meta *meta, u32 index,
                        enum zram_pageflags flag);

static void zram_set_flag(struct zram_meta *meta, u32 index,
                        enum zram_pageflags flag);

static void zram_clear_flag(struct zram_meta *meta, u32 index,
                        enum zram_pageflags flag);
static void zram_stat64_add(struct zram *zram, u64 *v, u64 inc);
static void zram_stat64_sub(struct zram *zram, u64 *v, u64 dec);
static void zram_stat64_inc(struct zram *zram, u64 *v);
static int zsm_test_flag(struct zram_meta *meta, struct table *node,
                        enum zram_pageflags flag)
{
        return node->flags & BIT(flag);
}

static void zsm_set_flag(struct zram_meta *meta, struct table *node,
                        enum zram_pageflags flag)
{
        node->flags |= BIT(flag);
}

static struct table * search_node_in_zram_list(struct zram *zram,struct zram_meta *meta,struct table *input_node,struct table *found_node,unsigned char *match_content)
{
	struct list_head *list_node = NULL;
	struct table *current_node = NULL;
	unsigned char *cmem;
	int one_node_in_list = 0;
	int compare_count = 0;
	int ret;

	list_node = found_node->head.next;
	if(list_node == &(found_node->head))
		one_node_in_list = 1;	
	while((list_node != &(found_node->head))||one_node_in_list)
	{
		one_node_in_list = 0;
		current_node  = list_entry(list_node, struct table, head);
		if((input_node->size != current_node->size)||!zsm_test_flag(meta, current_node, ZRAM_FIRST_NODE))
		{
			list_node = list_node->next;
		}
		else
		{
			cmem = zs_map_object(meta->mem_pool, current_node->handle, ZS_MM_RO);
			ret = memcmp(cmem,match_content,input_node->size);
			compare_count++;
			if(ret == 0)
			{
				zs_unmap_object(meta->mem_pool, current_node->handle);
				return current_node;
			}
			else
			{
				list_node = list_node->next;
			}
        		zs_unmap_object(meta->mem_pool, current_node->handle);
		}
	}
	return NULL;
}
static struct table *search_node_in_zram_tree(struct table *input_node,struct rb_node **parent_node,struct rb_node ***new_node, unsigned char *match_content,struct rb_root *local_root_zram_tree)
{
	struct rb_node **new = &(local_root_zram_tree->rb_node);
	struct table *current_node = NULL;
	struct rb_node *parent = NULL;

	current_node = rb_entry(*new,struct table, node);	
	if(input_node == NULL)
	{
		printk("[zram][search_node_in_zram_tree] input_node is NULL\n");
		return NULL;
	}
        if(current_node == NULL)
        {
                *new_node = new;
                *parent_node = NULL;
                return NULL;
        }

	while(*new)
	{
		current_node = rb_entry(*new,struct table, node);
		parent = *new;	
		if(input_node->checksum > current_node->checksum)
		{
			new = &parent->rb_right;
		}
		else if(input_node->checksum < current_node->checksum)
		{
			new = &parent->rb_left;
		}
		else
		{
			if(input_node->size > current_node->size)
			{
				new = &parent->rb_right;
			}
			else if(input_node->size < current_node->size)
			{
				new = &parent->rb_left;	
			}
			else
			{
				//printk("[zram]rb tree found 0x%x size\n",(unsigned int)&current_node->node,current_node->size);
				return current_node;
			}
		}
	}
	*parent_node = parent;
	*new_node = new;
	return NULL;	
}
static u32 insert_node_to_zram_tree(struct zram *zram,struct zram_meta *meta,u32 index, unsigned char *match_content,struct rb_root *local_root_zram_tree)
{
	struct table *current_node = NULL;
	struct table *node_in_list = NULL;
	struct rb_node *parent = NULL;
	struct rb_node **new = NULL;
	struct table *input_node;
	static int node_count = 0;

	input_node = &(meta->table[index]);
	current_node = search_node_in_zram_tree(input_node,&parent,&new,match_content,local_root_zram_tree);
	node_count++;

	//found node in zram_tree
	if(NULL != current_node)
	{
		if(!zsm_test_flag(meta,current_node,ZRAM_RB_NODE))
		{
			printk("[ZRAM]ERROR !!found wrong rb node 0x%x\n",(unsigned int)current_node);
			BUG_ON(1);	
		}

		//check if there is any other node in this position.
		node_in_list = search_node_in_zram_list(zram,meta,input_node,current_node,match_content);


		//found the same node in list
		if(NULL != node_in_list)
		{
			//insert node after the found node
			if(!zsm_test_flag(meta,current_node,ZRAM_FIRST_NODE))
                        {
                                printk("[ZRAM]ERROR !!found wrong first node 0x%x\n",(unsigned int)node_in_list);
				BUG_ON(1);
                        }
			input_node->next_index = node_in_list->next_index;
			node_in_list->next_index = index;
			input_node->copy_index = node_in_list->copy_index;

			//found the same node and add ref count 
			node_in_list->copy_count++;
			if (unlikely(input_node->size > max_zpage_size))
			{
				zram_stat64_add(zram,&zram->stats.zsm_saved4k, (u64)input_node->size);
			}
			else
			{
                               zram_stat64_add(zram,&zram->stats.zsm_saved, (u64)input_node->size);
			}
			input_node->handle = node_in_list->handle;
			list_add(&input_node->head,&node_in_list->head);
			return 1;
		}
		else  //can't found node in list
		{
			zram_set_flag(meta, index, ZRAM_FIRST_NODE);
			list_add(&input_node->head,&current_node->head);
		}
	}
	else
	{
		//insert node into rb tree
		zram_set_flag(meta, index, ZRAM_FIRST_NODE);
		zram_set_flag(meta, index, ZRAM_RB_NODE);
		rb_link_node(&(meta->table[index].node),parent,new);
		rb_insert_color(&(meta->table[index].node),local_root_zram_tree);
	}
	return 0;	
}
static int remove_node_from_zram_list(struct zram *zram,struct zram_meta *meta,u32 index)
{
	u32 next_index = 0xffffffff;
        u32 pre_index = 0xffffffff;
        u32 current_index = 0xffffffff;
        u32 copy_index = 0xffffffff;
        u32 i = 0;

        next_index  = meta->table[index].next_index;
	list_del(&(meta->table[index].head));
	
	//check if there is the same content in list
	if(index != next_index) //found the same page content
	{
		if(zram_test_flag(meta, index, ZRAM_FIRST_NODE))//delete the fist node of content
		{
			if(meta->table[index].copy_count <= 0)
			{
				printk("[ZRAM]ERROR !!count < 0\n ");
                                BUG_ON(1);
                                return 1;
			}
			current_index = meta->table[next_index].next_index;
			meta->table[next_index].copy_index = next_index;
			pre_index = next_index;
                        while(current_index != index)
                        {
                                i++;
				if(i>= 4096 && (i%1000 == 0))
                                {
					printk("[ZRAM]ERROR !!can't find meta->table[%d].size %d chunksum %x in list\n",index,meta->table[index].size,meta->table[index].checksum);
					if(i > meta->table[index].copy_count)
					{
                                        	BUG_ON(1);
                                        	break;
					}
                                }
                                meta->table[current_index].copy_index = next_index;
				pre_index = current_index;
                                current_index = meta->table[current_index].next_index;
		        }
			meta->table[pre_index].next_index = meta->table[index].next_index;
			meta->table[next_index].copy_count = meta->table[index].copy_count - 1;
			zram_clear_flag(meta, index, ZRAM_FIRST_NODE);
			zram_set_flag(meta, next_index, ZRAM_FIRST_NODE);
		}
		else
		{
			current_index  = meta->table[index].copy_index;
			pre_index = current_index;
			current_index = meta->table[current_index].next_index;
			while(index != current_index)
                        {
                                i++;
                                if(i>= 4096 && (i%1000 == 0))
                                {
					u32 tmp_index = 0;
	                                printk("[ZRAM]ERROR !!can't find2 meta->table[%d].size %d chunksum %d in list\n",index,meta->table[index].size,meta->table[index].checksum);
					tmp_index = meta->table[current_index].copy_count;
					if(i > meta->table[tmp_index].copy_count)
                                        {
                                                BUG_ON(1);
                                                break;
                                        }
                                }
                                pre_index = current_index;
                                current_index = meta->table[current_index].next_index;
                        }
			meta->table[pre_index].next_index = meta->table[index].next_index;
			copy_index = meta->table[index].copy_index;
			meta->table[copy_index].copy_count = meta->table[copy_index].copy_count - 1;	
		}
		if (unlikely(meta->table[index].size > max_zpage_size))
		{
			zram_stat64_sub(zram,&zram->stats.zsm_saved4k,(u64)meta->table[index].size);
		}
		else
		{
			zram_stat64_sub(zram,&zram->stats.zsm_saved,(u64)meta->table[index].size);
		}
		return 1;
	}
	else//can't found the same page content
	{
		if(zram_test_flag(meta, index, ZRAM_FIRST_NODE))
		{
			zram_clear_flag(meta, index, ZRAM_FIRST_NODE);
		}	
		else
		{
			printk("[ZRAM]ERROR !!index != next_index and flag != ZRAM_FIRST_NODE index %x\n ",index);
		}
		if(meta->table[index].copy_count != 0)
		{
			printk("[ZRAM]ERROR !!index != next_index and count != 0 index %x\n ",index);
		}
	}
	return 0;
}
static int remove_node_from_zram_tree(struct zram *zram,struct zram_meta *meta,u32 index,struct rb_root *local_root_zram_tree)
{
	int ret;

	//if it is rb node, choose other node from list and replace original node.
	if(zram_test_flag(meta, index, ZRAM_RB_NODE))
	{
		zram_clear_flag(meta, index, ZRAM_RB_NODE);

		//found next node in list
		if(&(meta->table[index].head) != meta->table[index].head.next)
		{
			struct table *next_table;
			next_table = list_entry(meta->table[index].head.next,struct table, head);
			rb_replace_node(&(meta->table[index].node),&(next_table->node),local_root_zram_tree);
			zsm_set_flag(meta,next_table, ZRAM_RB_NODE);
			ret = remove_node_from_zram_list(zram,meta,index);
			return ret;
		}
		else //if no other node can be found in list just remove node from rb tree and free handle
		{
			if(zram_test_flag(meta, index, ZRAM_FIRST_NODE))
	                {
       	                	zram_clear_flag(meta, index, ZRAM_FIRST_NODE);
                	}
                	else
                	{
                        	printk("[ZRAM]ERROR !!ZRAM_RB_NODR's flag != ZRAM_FIRST_NODE index %x\n ",index);
                	}
			rb_erase(&(meta->table[index].node),local_root_zram_tree);
			return 0;	
		}
	}
	else
	{
		ret = remove_node_from_zram_list(zram,meta,index);
		return ret;
	}
}
#endif
/* Set above hooks */
void zram_set_hooks(void *compress_func, void *decompress_func, const char *name)
{
#ifdef CONFIG_ZSM
	printk(KERN_ALERT "\nZSM only supports LZO1X now.\n\n");         	/* TODO: Add LZ4K or other algorithms. */
#else
	if (name != NULL) {
		printk(KERN_ALERT "[%s] Compress[%p] Decompress[%p]\n",name, compress_func, decompress_func);
		zram_comp = name;
	} else
		printk(KERN_ALERT "[UNKNOWN] Compress[%p] Decompress[%p]\n", compress_func, decompress_func);
	zram_compress = (comp_hook)compress_func;
	zram_decompress = (decomp_hook)decompress_func;
	printk(KERN_ALERT "[%s][%d] ZCompress[%p] ZDecompress[%p]\n", __FUNCTION__, __LINE__, zram_compress, zram_decompress);
#endif
}
EXPORT_SYMBOL(zram_set_hooks);

/* Module params (documentation at end) */
static unsigned int num_devices = 4;

static void zram_stat64_add(struct zram *zram, u64 *v, u64 inc)
{
	spin_lock(&zram->stat64_lock);
	*v = *v + inc;
	spin_unlock(&zram->stat64_lock);
}

static void zram_stat64_sub(struct zram *zram, u64 *v, u64 dec)
{
	spin_lock(&zram->stat64_lock);
	*v = *v - dec;
	spin_unlock(&zram->stat64_lock);
}

static void zram_stat64_inc(struct zram *zram, u64 *v)
{
	zram_stat64_add(zram, v, 1);
}

static int zram_test_flag(struct zram_meta *meta, u32 index,
			enum zram_pageflags flag)
{
	return meta->table[index].flags & BIT(flag);
}

static void zram_set_flag(struct zram_meta *meta, u32 index,
			enum zram_pageflags flag)
{
	meta->table[index].flags |= BIT(flag);
}

static void zram_clear_flag(struct zram_meta *meta, u32 index,
			enum zram_pageflags flag)
{
	meta->table[index].flags &= ~BIT(flag);
}

static int page_zero_filled(void *ptr)
{
	unsigned int pos;
	unsigned long *page;

	page = (unsigned long *)ptr;

	for (pos = 0; pos != PAGE_SIZE / sizeof(*page); pos++) {
		if (page[pos])
			return 0;
	}

	return 1;
}

static void zram_free_page(struct zram *zram, size_t index)
{
	struct zram_meta *meta = zram->meta;
	unsigned long handle = meta->table[index].handle;
	u16 size = meta->table[index].size;
#ifdef CONFIG_ZSM
	int ret = 0;
#endif
	if (unlikely(!handle)) {
		/*
		 * No memory is allocated for zero filled pages.
		 * Simply clear zero page flag.
		 */
		if (zram_test_flag(meta, index, ZRAM_ZERO)) {
			zram_clear_flag(meta, index, ZRAM_ZERO);
			zram->stats.pages_zero--;
		}
		return;
	}

	if (unlikely(size > max_zpage_size))
		zram->stats.bad_compress--;
#ifdef CONFIG_ZSM
	if(!zram_test_flag(meta, index, ZRAM_ZERO))
	{
		if(meta->table[index].size == PAGE_SIZE)
		{
			spin_lock(&zram_node4k_mutex);
			ret = remove_node_from_zram_tree(zram,meta,index,&root_zram_tree_4k);
			spin_unlock(&zram_node4k_mutex);
		}
        	else 
        	{
			spin_lock(&zram_node_mutex);
                	ret = remove_node_from_zram_tree(zram,meta,index,&root_zram_tree);
			spin_unlock(&zram_node_mutex);
        	}
	}
	if(ret == 0)	
	{
	zs_free(meta->mem_pool, handle);
	}

#else
	zs_free(meta->mem_pool, handle);
#endif
	if (size <= PAGE_SIZE / 2)
		zram->stats.good_compress--;

	zram_stat64_sub(zram, &zram->stats.compr_size,
			meta->table[index].size);
	zram->stats.pages_stored--;

	meta->table[index].handle = 0;
	meta->table[index].size = 0;
}

static void handle_zero_page(struct bio_vec *bvec)
{
	struct page *page = bvec->bv_page;
	void *user_mem;

	user_mem = kmap_atomic(page);
	memset(user_mem + bvec->bv_offset, 0, bvec->bv_len);
	kunmap_atomic(user_mem);

	flush_dcache_page(page);
}

static inline int is_partial_io(struct bio_vec *bvec)
{
	return bvec->bv_len != PAGE_SIZE;
}

#ifdef CONFIG_MT_ENG_BUILD
static void zram_check_guidebytes(unsigned char *cmem, bool is_header)
{
	int idx;
	for (idx = 0; idx < GUIDE_BYTES_HALFLEN; idx++) {
		if (*cmem != (unsigned char)GUIDE_BYTES) {
			if (is_header)
				printk(KERN_ERR "<<HEADER>>\n");
			else
				printk(KERN_ERR "<<TAIL>>\n");

			cmem -= idx;
			for (idx = 0; idx < GUIDE_BYTES_HALFLEN; idx++) {
				printk(KERN_ERR "%x ",(int)*cmem++);
			}
			printk(KERN_ERR "\n<<END>>\n");
			/* Just return */
			return;
		}
		cmem++;
	}
}
#endif
static int zram_decompress_page(struct zram *zram, char *mem, u32 index)
{
	int ret = LZO_E_OK;
	size_t clen = PAGE_SIZE;
	unsigned char *cmem;
	struct zram_meta *meta = zram->meta;
	unsigned long handle = meta->table[index].handle;

	if (!handle || zram_test_flag(meta, index, ZRAM_ZERO)) {
		memset(mem, 0, PAGE_SIZE);
		return 0;
	}

	cmem = zs_map_object(meta->mem_pool, handle, ZS_MM_RO);
	if (meta->table[index].size == PAGE_SIZE)
		memcpy(mem, cmem, PAGE_SIZE);
	else
#ifdef CONFIG_MT_ENG_BUILD
	{
		/* Check header */
		zram_check_guidebytes(cmem, true);

		/* Move to the start of bitstream */
		cmem += GUIDE_BYTES_HALFLEN;
#endif
		ret = zram_decompress(cmem, meta->table[index].size,
						mem, &clen);
#ifdef CONFIG_MT_ENG_BUILD
		/* Check tail */
		zram_check_guidebytes(cmem + meta->table[index].size, false);
	}
#endif

	zs_unmap_object(meta->mem_pool, handle);

	/* Should NEVER happen. Return bio error if it does. */
	if (unlikely(ret != LZO_E_OK)) {
		pr_err("Decompression failed! err=%d, page=%u\n", ret, index);
		zram_stat64_inc(zram, &zram->stats.failed_reads);
#ifdef CONFIG_MT_ENG_BUILD
		{
			int idx;
			size_t tlen;
			printk(KERN_ALERT "\n@@@@@@@@@@\n");
			tlen = meta->table[index].size + GUIDE_BYTES_LENGTH;
			cmem = zs_map_object(meta->mem_pool, handle, ZS_MM_RO);
			/* Head guide bytes */
			for (idx = 0; idx < GUIDE_BYTES_HALFLEN; idx++) {
				printk(KERN_ALERT "%x ",(int)*cmem++);
			}
			printk(KERN_ALERT "\n=========\n");
			for (;idx < tlen; idx++) {
				printk(KERN_ALERT "%x ",(int)*cmem++);
			}
			zs_unmap_object(meta->mem_pool, handle);
			printk(KERN_ALERT "\n!!!!!!!!!\n");
		}
#endif
		return ret;
	}

	return 0;
}

static int zram_bvec_read(struct zram *zram, struct bio_vec *bvec,
			  u32 index, int offset, struct bio *bio)
{
	int ret;
	struct page *page;
	unsigned char *user_mem, *uncmem = NULL;
	struct zram_meta *meta = zram->meta;
	page = bvec->bv_page;

	if (unlikely(!meta->table[index].handle) ||
			zram_test_flag(meta, index, ZRAM_ZERO)) {
		handle_zero_page(bvec);
		return 0;
	}

	if (is_partial_io(bvec))
		/* Use  a temporary buffer to decompress the page */
		uncmem = kmalloc(PAGE_SIZE, GFP_NOIO);

	user_mem = kmap_atomic(page);
	if (!is_partial_io(bvec))
		uncmem = user_mem;

	if (!uncmem) {
		pr_info("Unable to allocate temp memory\n");
		ret = -ENOMEM;
		goto out_cleanup;
	}

	ret = zram_decompress_page(zram, uncmem, index);
	/* Should NEVER happen. Return bio error if it does. */
	if (unlikely(ret != LZO_E_OK))
		goto out_cleanup;

	if (is_partial_io(bvec))
		memcpy(user_mem + bvec->bv_offset, uncmem + offset,
				bvec->bv_len);

	flush_dcache_page(page);
	ret = 0;
out_cleanup:
	kunmap_atomic(user_mem);
	if (is_partial_io(bvec))
		kfree(uncmem);
	return ret;
}

static int zram_bvec_write(struct zram *zram, struct bio_vec *bvec, u32 index,
			   int offset)
{
	int ret = 0;
#ifdef CONFIG_ZSM
	int checksum = 0;
#endif
	size_t clen;
	unsigned long handle;
	struct page *page;
	unsigned char *user_mem, *cmem, *src, *uncmem = NULL;
	struct zram_meta *meta = zram->meta;

	page = bvec->bv_page;
	src = meta->compress_buffer;

	if (is_partial_io(bvec)) {
		/*
		 * This is a partial IO. We need to read the full page
		 * before to write the changes.
		 */
		uncmem = kmalloc(PAGE_SIZE, GFP_NOIO);
		if (!uncmem) {
			ret = -ENOMEM;
			goto out;
		}
		ret = zram_decompress_page(zram, uncmem, index);
		if (ret)
			goto out;
#ifdef CONFIG_ZSM
	        if(!zram_test_flag(meta, index, ZRAM_ZERO))
		{
			if(meta->table[index].size == PAGE_SIZE)
			{
				spin_lock(&zram_node4k_mutex);
				ret = remove_node_from_zram_tree(zram,meta,index,&root_zram_tree_4k);
				spin_unlock(&zram_node4k_mutex);
			}
			else
			{
				spin_lock(&zram_node_mutex);
				ret = remove_node_from_zram_tree(zram,meta,index,&root_zram_tree);
				spin_unlock(&zram_node_mutex);
			}
        	}
#endif
	}

	/*
	 * System overwrites unused sectors. Free memory associated
	 * with this sector now.
	 */
	if (meta->table[index].handle ||
	    zram_test_flag(meta, index, ZRAM_ZERO))
		zram_free_page(zram, index);

	user_mem = kmap_atomic(page);

	if (is_partial_io(bvec)) {
		memcpy(uncmem + offset, user_mem + bvec->bv_offset,
		       bvec->bv_len);
		kunmap_atomic(user_mem);
		user_mem = NULL;
	} else {
		uncmem = user_mem;
	}

	if (page_zero_filled(uncmem)) {
		if (!is_partial_io(bvec))
			kunmap_atomic(user_mem);
		zram->stats.pages_zero++;
		zram_set_flag(meta, index, ZRAM_ZERO);
		ret = 0;
		goto out;
	}
#ifdef CONFIG_ZSM
	ret = zram_compress(uncmem, PAGE_SIZE, src, &clen,
                               meta->compress_workmem,&checksum);

#else
	ret = zram_compress(uncmem, PAGE_SIZE, src, &clen,
			       meta->compress_workmem);
#endif
	if (!is_partial_io(bvec)) {
		kunmap_atomic(user_mem);
		user_mem = NULL;
		uncmem = NULL;
	}

	if (unlikely(ret != LZO_E_OK)) {
		pr_err("Compression failed! err=%d\n", ret);
		goto out;
	}

	if (unlikely(clen > max_zpage_size)) {
		zram->stats.bad_compress++;
		clen = PAGE_SIZE;
		src = NULL;
		if (is_partial_io(bvec))
			src = uncmem;
#ifdef CONFIG_ZSM
		{
		int search_ret = 0;

		meta->table[index].checksum = checksum;
		meta->table[index].size = clen;
                meta->table[index].next_index = index;
                meta->table[index].copy_index = index;
                meta->table[index].copy_count = 0;
                //rb_init_node(&(meta->table[index].node));		
                INIT_LIST_HEAD(&(meta->table[index].head));
		if(src != NULL)
		{
			spin_lock(&zram_node4k_mutex);
                	search_ret = insert_node_to_zram_tree(zram,meta,index,src,&root_zram_tree_4k);
			spin_unlock(&zram_node4k_mutex);
		}
		else
		{
			src = kmap_atomic(page);
			spin_lock(&zram_node4k_mutex);
			search_ret = insert_node_to_zram_tree(zram,meta,index,src,&root_zram_tree_4k);
			spin_unlock(&zram_node4k_mutex);
			kunmap_atomic(src);		
		}

		if(search_ret)
                {
                        ret = 0;
                        goto out;
                }
		}
#endif
	}
#ifdef CONFIG_ZSM
	else
	{
		int search_ret = 0;

		meta->table[index].checksum = checksum;
		meta->table[index].size = clen;		
		meta->table[index].next_index = index;
		meta->table[index].copy_index = index;
		meta->table[index].copy_count = 0;

		INIT_LIST_HEAD(&(meta->table[index].head));
		spin_lock(&zram_node_mutex);
		search_ret = insert_node_to_zram_tree(zram,meta,index,src,&root_zram_tree);
		spin_unlock(&zram_node_mutex);
		if(search_ret)
		{
			ret = 0;
			goto out;
		}
	}
#endif

#ifdef CONFIG_MT_ENG_BUILD
	if (clen != PAGE_SIZE)
		clen += GUIDE_BYTES_LENGTH;
#endif

	handle = zs_malloc(meta->mem_pool, clen);
	if (!handle) {
		pr_info("Error allocating memory for compressed "
			"page: %u, size=%zu\n", index, clen);
		ret = -ENOMEM;
		goto out;
	}
	cmem = zs_map_object(meta->mem_pool, handle, ZS_MM_WO);

	if ((clen == PAGE_SIZE) && !is_partial_io(bvec))
		src = kmap_atomic(page);

#ifdef CONFIG_MT_ENG_BUILD
	/* Head guide bytes */
	if (clen != PAGE_SIZE) {
		int idx;
		for (idx = 0; idx < GUIDE_BYTES_HALFLEN; idx++) {
			*cmem = GUIDE_BYTES;
			cmem++;
		}
		clen -= GUIDE_BYTES_LENGTH;
	}
#endif

	memcpy(cmem, src, clen);

#ifdef CONFIG_MT_ENG_BUILD
	/* Tail guide bytes */
	if (clen != PAGE_SIZE) {
		int idx;
		cmem += clen;
		for (idx = 0; idx < GUIDE_BYTES_HALFLEN; idx++) {
			*cmem = GUIDE_BYTES;
			cmem++;
		}
	}
#endif

	if ((clen == PAGE_SIZE) && !is_partial_io(bvec))
		kunmap_atomic(src);

	zs_unmap_object(meta->mem_pool, handle);

	meta->table[index].handle = handle;
	meta->table[index].size = clen;

	/* Update stats */
	zram_stat64_add(zram, &zram->stats.compr_size, clen);
	zram->stats.pages_stored++;
	if (clen <= PAGE_SIZE / 2)
		zram->stats.good_compress++;

out:
	if (is_partial_io(bvec))
		kfree(uncmem);

	if (ret)
		zram_stat64_inc(zram, &zram->stats.failed_writes);
	return ret;
}

static int zram_bvec_rw(struct zram *zram, struct bio_vec *bvec, u32 index,
			int offset, struct bio *bio, int rw)
{
	int ret;

	if (rw == READ) {
		down_read(&zram->lock);
		ret = zram_bvec_read(zram, bvec, index, offset, bio);
		up_read(&zram->lock);
	} else {
		down_write(&zram->lock);
		ret = zram_bvec_write(zram, bvec, index, offset);
		up_write(&zram->lock);
	}

	return ret;
}

static void update_position(u32 *index, int *offset, struct bio_vec *bvec)
{
	if (*offset + bvec->bv_len >= PAGE_SIZE)
		(*index)++;
	*offset = (*offset + bvec->bv_len) % PAGE_SIZE;
}

static void __zram_make_request(struct zram *zram, struct bio *bio, int rw)
{
	int i, offset;
	u32 index;
	struct bio_vec *bvec;

	switch (rw) {
	case READ:
		zram_stat64_inc(zram, &zram->stats.num_reads);
		break;
	case WRITE:
		zram_stat64_inc(zram, &zram->stats.num_writes);
		break;
	}

	index = bio->bi_sector >> SECTORS_PER_PAGE_SHIFT;
	offset = (bio->bi_sector & (SECTORS_PER_PAGE - 1)) << SECTOR_SHIFT;

	bio_for_each_segment(bvec, bio, i) {
		int max_transfer_size = PAGE_SIZE - offset;

		if (bvec->bv_len > max_transfer_size) {
			/*
			 * zram_bvec_rw() can only make operation on a single
			 * zram page. Split the bio vector.
			 */
			struct bio_vec bv;

			bv.bv_page = bvec->bv_page;
			bv.bv_len = max_transfer_size;
			bv.bv_offset = bvec->bv_offset;

			if (zram_bvec_rw(zram, &bv, index, offset, bio, rw) < 0)
				goto out;

			bv.bv_len = bvec->bv_len - max_transfer_size;
			bv.bv_offset += max_transfer_size;
			if (zram_bvec_rw(zram, &bv, index+1, 0, bio, rw) < 0)
				goto out;
		} else
			if (zram_bvec_rw(zram, bvec, index, offset, bio, rw)
			    < 0)
				goto out;

		update_position(&index, &offset, bvec);
	}

	set_bit(BIO_UPTODATE, &bio->bi_flags);
	bio_endio(bio, 0);
	return;

out:
	bio_io_error(bio);
}

/*
 * Check if request is within bounds and aligned on zram logical blocks.
 */
static inline int valid_io_request(struct zram *zram, struct bio *bio)
{
	u64 start, end, bound;

	/* unaligned request */
	if (unlikely(bio->bi_sector & (ZRAM_SECTOR_PER_LOGICAL_BLOCK - 1)))
		return 0;
	if (unlikely(bio->bi_size & (ZRAM_LOGICAL_BLOCK_SIZE - 1)))
		return 0;

	start = bio->bi_sector;
	end = start + (bio->bi_size >> SECTOR_SHIFT);
	bound = zram->disksize >> SECTOR_SHIFT;
	/* out of range range */
	if (unlikely(start >= bound || end > bound || start > end))
		return 0;

	/* I/O request is valid */
	return 1;
}

/*
 * Handler function for all zram I/O requests.
 */
static void zram_make_request(struct request_queue *queue, struct bio *bio)
{
	struct zram *zram = queue->queuedata;

	down_read(&zram->init_lock);
	if (unlikely(!zram->init_done))
		goto error;

	if (!valid_io_request(zram, bio)) {
		zram_stat64_inc(zram, &zram->stats.invalid_io);
		goto error;
	}

	__zram_make_request(zram, bio, bio_data_dir(bio));
	up_read(&zram->init_lock);

	return;

error:
	up_read(&zram->init_lock);
	bio_io_error(bio);
}

static void __zram_reset_device(struct zram *zram)
{
	size_t index;
	struct zram_meta *meta;

	if (!zram->init_done)
		return;

	meta = zram->meta;
	zram->init_done = 0;

	/* Free all pages that are still in this zram device */
	for (index = 0; index < zram->disksize >> PAGE_SHIFT; index++) {
		unsigned long handle = meta->table[index].handle;
		if (!handle)
			continue;

		zs_free(meta->mem_pool, handle);
	}

	zram_meta_free(zram->meta);
	zram->meta = NULL;
	/* Reset stats */
	memset(&zram->stats, 0, sizeof(zram->stats));

	zram->disksize = 0;
	set_capacity(zram->disk, 0);
}

void zram_reset_device(struct zram *zram)
{
	down_write(&zram->init_lock);
	__zram_reset_device(zram);
	up_write(&zram->init_lock);
}

void zram_meta_free(struct zram_meta *meta)
{
	zs_destroy_pool(meta->mem_pool);
	kfree(meta->compress_workmem);
	free_pages((unsigned long)meta->compress_buffer, 1);
	vfree(meta->table);
	kfree(meta);
}

struct zram_meta *zram_meta_alloc(u64 disksize)
{
	size_t num_pages;
	struct zram_meta *meta = kmalloc(sizeof(*meta), GFP_KERNEL);
	if (!meta)
		goto out;

#if defined(CONFIG_64BIT) && defined(CONFIG_LZ4K)
	meta->compress_workmem = kzalloc((LZO1X_MEM_COMPRESS << 1), GFP_KERNEL);
#else
	meta->compress_workmem = kzalloc(LZO1X_MEM_COMPRESS, GFP_KERNEL);
#endif
	if (!meta->compress_workmem)
		goto free_meta;

	meta->compress_buffer =
		(void *)__get_free_pages(GFP_KERNEL | __GFP_ZERO, 1);
	if (!meta->compress_buffer) {
		pr_err("Error allocating compressor buffer space\n");
		goto free_workmem;
	}

	num_pages = disksize >> PAGE_SHIFT;
	meta->table = vzalloc(num_pages * sizeof(*meta->table));
	if (!meta->table) {
		pr_err("Error allocating zram address table\n");
		goto free_buffer;
	}

	meta->mem_pool = zs_create_pool(GFP_NOIO | __GFP_HIGHMEM | __GFP_NOMTKPASR);
	if (!meta->mem_pool) {
		pr_err("Error creating memory pool\n");
		goto free_table;
	}

	return meta;

free_table:
	vfree(meta->table);
free_buffer:
	free_pages((unsigned long)meta->compress_buffer, 1);
free_workmem:
	kfree(meta->compress_workmem);
free_meta:
	kfree(meta);
	meta = NULL;
out:
	return meta;
}

void zram_init_device(struct zram *zram, struct zram_meta *meta)
{
	if (zram->disksize > 2 * (totalram_pages << PAGE_SHIFT)) {
		pr_info(
		"There is little point creating a zram of greater than "
		"twice the size of memory since we expect a 2:1 compression "
		"ratio. Note that zram uses about 0.1%% of the size of "
		"the disk when not in use so a huge zram is "
		"wasteful.\n"
		"\tMemory Size: %lu kB\n"
		"\tSize you selected: %llu kB\n"
		"Continuing anyway ...\n",
		(totalram_pages << PAGE_SHIFT) >> 10, zram->disksize >> 10
		);
	}

	/* zram devices sort of resembles non-rotational disks */
	queue_flag_set_unlocked(QUEUE_FLAG_NONROT, zram->disk->queue);

	zram->meta = meta;
	zram->init_done = 1;

	pr_debug("Initialization done!\n");
}

static void zram_slot_free_notify(struct block_device *bdev,
				unsigned long index)
{
	struct zram *zram;
	zram = bdev->bd_disk->private_data;
	/* down_write(&zram->lock); */
	zram_free_page(zram, index);
	/* up_write(&zram->lock); */
	zram_stat64_inc(zram, &zram->stats.notify_free);

}

static const struct block_device_operations zram_devops = {
	.swap_slot_free_notify = zram_slot_free_notify,
	.owner = THIS_MODULE
};

static int create_device(struct zram *zram, int device_id)
{
	int ret = -ENOMEM;

	init_rwsem(&zram->lock);
	init_rwsem(&zram->init_lock);
	spin_lock_init(&zram->stat64_lock);
#ifdef CONFIG_ZSM
	spin_lock_init(&zram_node_mutex);
	spin_lock_init(&zram_node4k_mutex);
#endif
	zram->queue = blk_alloc_queue(GFP_KERNEL);
	if (!zram->queue) {
		pr_err("Error allocating disk queue for device %d\n",
			device_id);
		goto out;
	}

	blk_queue_make_request(zram->queue, zram_make_request);
	zram->queue->queuedata = zram;

	 /* gendisk structure */
	zram->disk = alloc_disk(1);
	if (!zram->disk) {
		pr_warn("Error allocating disk structure for device %d\n",
			device_id);
		goto out_free_queue;
	}

	zram->disk->major = zram_major;
	zram->disk->first_minor = device_id;
	zram->disk->fops = &zram_devops;
	zram->disk->queue = zram->queue;
	zram->disk->private_data = zram;
	snprintf(zram->disk->disk_name, 16, "zram%d", device_id);

	/* Actual capacity set using syfs (/sys/block/zram<id>/disksize */
	set_capacity(zram->disk, 0);

	/*
	 * To ensure that we always get PAGE_SIZE aligned
	 * and n*PAGE_SIZED sized I/O requests.
	 */
	blk_queue_physical_block_size(zram->disk->queue, PAGE_SIZE);
	blk_queue_logical_block_size(zram->disk->queue,
					ZRAM_LOGICAL_BLOCK_SIZE);
	blk_queue_io_min(zram->disk->queue, PAGE_SIZE);
	blk_queue_io_opt(zram->disk->queue, PAGE_SIZE);

	add_disk(zram->disk);

	ret = sysfs_create_group(&disk_to_dev(zram->disk)->kobj,
				&zram_disk_attr_group);
	if (ret < 0) {
		pr_warn("Error creating sysfs group");
		goto out_free_disk;
	}

	zram->init_done = 0;
	return 0;

out_free_disk:
	del_gendisk(zram->disk);
	put_disk(zram->disk);
out_free_queue:
	blk_cleanup_queue(zram->queue);
out:
	return ret;
}

static void destroy_device(struct zram *zram)
{
	sysfs_remove_group(&disk_to_dev(zram->disk)->kobj,
			&zram_disk_attr_group);

	if (zram->disk) {
		del_gendisk(zram->disk);
		put_disk(zram->disk);
	}

	if (zram->queue)
		blk_cleanup_queue(zram->queue);
}

unsigned int zram_get_num_devices(void)
{
	return num_devices;
}

static int zraminfo_proc_show(struct seq_file *m, void *v)
{
	if (zram_devices->init_done)
    {
#define P2K(x) (((unsigned long)x) << (PAGE_SHIFT - 10))
#define B2K(x) (((unsigned long)x) >> (10))
        seq_printf(m,
            "DiskSize:       %8lu kB\n"
            "OrigSize:       %8lu kB\n"
            "ComprSize:      %8lu kB\n"
            "MemUsed:        %8lu kB\n"
            "GoodCompr:      %8lu kB\n"
            "BadCompr:       %8lu kB\n"
            "ZeroPage:       %8lu kB\n"
            "NotifyFree:     %8lu kB\n"
            "NumReads:       %8lu kB\n"
            "NumWrites:      %8lu kB\n"
#ifdef CONFIG_ZSM
	    "ZSM saved:      %8lu kB\n"
	    "ZSM4k saved:    %8lu kB\n"
#endif
            "InvalidIO:      %8lu kB\n"
            ,
            B2K(zram_devices->disksize),
            P2K(zram_devices->stats.pages_stored),
            B2K(zram_devices->stats.compr_size),
            B2K(zs_get_total_size_bytes(zram_devices->meta->mem_pool)),
            P2K(zram_devices->stats.good_compress),
            P2K(zram_devices->stats.bad_compress),
            P2K(zram_devices->stats.pages_zero),
            P2K(zram_devices->stats.notify_free),
            P2K(zram_devices->stats.num_reads),
            P2K(zram_devices->stats.num_writes),
#ifdef CONFIG_ZSM
	    B2K(zram_devices->stats.zsm_saved),
	    B2K(zram_devices->stats.zsm_saved4k),
#endif
            P2K(zram_devices->stats.invalid_io)
        	);
#undef P2K
#undef B2K
	seq_printf(m, "Algorithm: [%s]\n", (zram_comp != NULL)? zram_comp : "LZO");
    }
    return 0;
}

static int zraminfo_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, zraminfo_proc_show, NULL);
}

static const struct file_operations zraminfo_proc_fops = {
	.open		= zraminfo_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init zram_init(void)
{
	int ret, dev_id;

	if (num_devices > max_num_devices) {
		pr_warn("Invalid value for num_devices: %u\n",
				num_devices);
		ret = -EINVAL;
		goto out;
	}

	zram_major = register_blkdev(0, "zram");
	if (zram_major <= 0) {
		pr_warn("Unable to get major number\n");
		ret = -EBUSY;
		goto out;
	}

	/* Allocate the device array and initialize each one */
	zram_devices = kzalloc(num_devices * sizeof(struct zram), GFP_KERNEL);
	if (!zram_devices) {
		ret = -ENOMEM;
		goto unregister;
	}

	for (dev_id = 0; dev_id < num_devices; dev_id++) {
		ret = create_device(&zram_devices[dev_id], dev_id);
		if (ret)
			goto free_devices;
	}

	/* Set compression/decompression hooks - Use LZO1X by default */
	if (!zram_compress || !zram_decompress) {
#ifdef CONFIG_ZSM
		zram_compress = &lzo1x_1_compress_zram;
#else
		zram_compress = &lzo1x_1_compress;
#endif
		zram_decompress = &lzo1x_decompress_safe;
	}
	printk(KERN_ALERT "[%s][%d] ZCompress[%p] ZDecompress[%p]\n", __FUNCTION__, __LINE__, zram_compress, zram_decompress);
	proc_create("zraminfo", 0, NULL, &zraminfo_proc_fops);
	pr_info("Created %u device(s) ...\n", num_devices);

	return 0;

free_devices:
	while (dev_id)
		destroy_device(&zram_devices[--dev_id]);
	kfree(zram_devices);
	zram_devices = NULL;
unregister:
	unregister_blkdev(zram_major, "zram");
out:
	return ret;
}

static void __exit zram_exit(void)
{
	int i;
	struct zram *zram;

	for (i = 0; i < num_devices; i++) {
		zram = &zram_devices[i];

		get_disk(zram->disk);
		destroy_device(zram);
		zram_reset_device(zram);
		put_disk(zram->disk);
	}

	unregister_blkdev(zram_major, "zram");

	kfree(zram_devices);
	zram_devices = NULL;    
	pr_debug("Cleanup done!\n");
}

module_param(num_devices, uint, 0);
MODULE_PARM_DESC(num_devices, "Number of zram devices");

module_init(zram_init);
module_exit(zram_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Nitin Gupta <ngupta@vflare.org>");
MODULE_DESCRIPTION("Compressed RAM Block Device");
