#ifndef __LINUX_MTFTL_H__
#define __LINUX_MTFTL_H__

#include <linux/mutex.h>
#include <linux/workqueue.h>
#include "linux/mtd/ubi.h"

/* #define PROFILE */
/*#define MTK_FTL_DEBUG */

#define MT_FTL_SUCCESS 0
#define MT_FTL_FAIL -1

#define MT_INVALID_BLOCKPAGE		0xFFFFFFFE
#define MT_PAGE_HAD_BEEN_READ		0x9EAD
#define MT_MAGIC_NUMBER			0x3105

#define NAND_DEFAULT_VALUE		0xFFFFFFFF
#define NAND_VOLUME			(8 * 1024 * 1024) /* Unit: kbytes */
#define NAND_PAGE_SIZE			(16 * 1024)
#define NAND_PAGE_NUM_PER_BLOCK		256
#define NAND_SECTOR_PER_PAGE		16
/* NAND_OOB_SIZE is not accurate, OOB size can be found from ubi_device->mtd->oobsize */
#define NAND_OOB_SIZE			(NAND_SECTOR_PER_PAGE * 8)
#define NAND_BLOCK_SIZE			(NAND_PAGE_SIZE * NAND_PAGE_NUM_PER_BLOCK)
#define NAND_TOTAL_PAGE_NUM		(NAND_VOLUME / (NAND_PAGE_SIZE >> 10))
#define NAND_TOTAL_BLOCK_NUM		(NAND_VOLUME / (NAND_BLOCK_SIZE >> 10))

#define FS_PAGE_SIZE			(4 * 1024)
#define FS_TOTAL_PAGE_NUM		(NAND_VOLUME / (FS_PAGE_SIZE >> 10))

/* Page Mapping per nand page (must be power of 2)*/
#define PM_PER_NANDPAGE			(NAND_PAGE_SIZE >> 2)
/*#define MAX_OFFSET_PER_BLOCK		(NAND_PAGE_SIZE * (NAND_PAGE_NUM_PER_BLOCK - 3))*/

#define PMT_ADDRESSES_PER_PAGE		4
#define PMT_INDICATOR_PAGE_SHIFT	10
#define PMT_INDICATOR_DIRTY_SHIFT	1
#define PMT_INDICATOR_CACHED_SHIFT	3
#define PMT_LEB_PAGE_INDICATOR_PAGE_SHIFT	12
#define PMT_PAGE_SHIFT			12
#define PMT_PART_SHIFT			12
#define PMT_DATACACHE_BUFF_NUM_SHIFT	4
#define PMT_TOTAL_CLUSTER_NUM		(FS_TOTAL_PAGE_NUM / PM_PER_NANDPAGE)
#define PMT_CACHE_NUM			4 /* Max: 7 */
#define PMT_BLOCK_NUM			3

#define CONFIG_START_BLOCK		0
#define REPLAY_BLOCK			2
#define PMT_START_BLOCK			3
#define DATA_START_BLOCK		(PMT_START_BLOCK + PMT_BLOCK_NUM)
#define MTKFTL_MAX_DATA_NUM_PER_PAGE	(1 << PMT_PART_SHIFT)

/**** MACROS ****/

/* mt_ftl error messages */
#define mt_ftl_err(dev, fmt, ...) pr_err("[MTK FTL][%s][0x%lx] " fmt "\n",      \
				 __func__, (unsigned long int)dev, ##__VA_ARGS__)
/*#define mt_ftl_err(fmt, ...) */

#define PMT_LEB_PAGE_INDICATOR_SET_BLOCKPAGE(p, blk, page) \
		((p) = (((blk) << PMT_LEB_PAGE_INDICATOR_PAGE_SHIFT) | (page)))

#define PMT_LEB_PAGE_INDICATOR_GET_BLOCK(p)	((p) >> PMT_LEB_PAGE_INDICATOR_PAGE_SHIFT)
#define PMT_LEB_PAGE_INDICATOR_GET_PAGE(p)	((p) & ((1 << PMT_LEB_PAGE_INDICATOR_PAGE_SHIFT) - 1))



#define PMT_INDICATOR_SET_BLOCKPAGE(p, blk, page, dirty, cache_num) \
		((p) = (((blk) << \
		(PMT_INDICATOR_PAGE_SHIFT + PMT_INDICATOR_DIRTY_SHIFT + PMT_INDICATOR_CACHED_SHIFT)) \
		| ((page) << (PMT_INDICATOR_DIRTY_SHIFT + PMT_INDICATOR_CACHED_SHIFT)) | \
		((dirty) << PMT_INDICATOR_CACHED_SHIFT) | ((cache_num) + 1)))

#define PMT_INDICATOR_GET_BLOCK(p)	((p) >> (PMT_INDICATOR_PAGE_SHIFT + PMT_INDICATOR_DIRTY_SHIFT \
					+ PMT_INDICATOR_CACHED_SHIFT))

#define PMT_INDICATOR_GET_PAGE(p)	(((p) >> (PMT_INDICATOR_DIRTY_SHIFT + PMT_INDICATOR_CACHED_SHIFT)) \
					& ((1 << PMT_INDICATOR_PAGE_SHIFT) - 1))

#define PMT_INDICATOR_IS_INCACHE(p)			((p) & ((1 << PMT_INDICATOR_CACHED_SHIFT) - 1))
#define PMT_INDICATOR_CACHE_BUF_NUM(p)			(((p) & ((1 << PMT_INDICATOR_CACHED_SHIFT) - 1)) - 1)
#define PMT_INDICATOR_IS_DIRTY(p)			(((p) & (1 << PMT_INDICATOR_CACHED_SHIFT)) \
								>> PMT_INDICATOR_CACHED_SHIFT)
#define PMT_INDICATOR_SET_CACHE_BUF_NUM(p, num)		((p) = (((p) & (~((1 << PMT_INDICATOR_CACHED_SHIFT) - 1))) \
							| ((num) + 1)))
#define PMT_INDICATOR_SET_DIRTY(p)	((p) |= (1 << PMT_INDICATOR_CACHED_SHIFT))
#define PMT_INDICATOR_RESET_INCACHE(p)	((p) &= (~((1 << PMT_INDICATOR_CACHED_SHIFT) - 1)))
#define PMT_INDICATOR_RESET_DIRTY(p)	((p) &= (~(1 << PMT_INDICATOR_CACHED_SHIFT)))

/*#define PMT_SET_BLOCKPAGE(p, blk, page, part, cache_num) \
		((p) = (((blk) << (PMT_PAGE_SHIFT + PMT_PART_SHIFT + PMT_DATACACHE_BUFF_NUM_SHIFT)) \
			| ((page) << (PMT_PART_SHIFT + PMT_DATACACHE_BUFF_NUM_SHIFT)) \
			| ((part) << PMT_DATACACHE_BUFF_NUM_SHIFT) | ((cache_num) + 1)))*/
#define PMT_SET_BLOCKPAGE(p, blk, page) \
		((p) = (((blk) << PMT_PAGE_SHIFT) | (page)))

#define META_PMT_SET_DATA(p, data_size, part, cache_num) \
		((p) = (((data_size) << (PMT_PART_SHIFT + PMT_DATACACHE_BUFF_NUM_SHIFT)) \
			| ((part) << PMT_DATACACHE_BUFF_NUM_SHIFT) \
			| ((cache_num) + 1)))

#define PMT_GET_BLOCK(p)	((p) >> PMT_PAGE_SHIFT)
#define PMT_GET_PAGE(p)		((p) & ((1 << PMT_PAGE_SHIFT) - 1))
#define PMT_GET_DATASIZE(p)	((p) >> (PMT_PAGE_SHIFT + PMT_DATACACHE_BUFF_NUM_SHIFT))
#define PMT_GET_PART(p)		(((p) >> PMT_DATACACHE_BUFF_NUM_SHIFT) & ((1 << PMT_PART_SHIFT) - 1))
#define PMT_GET_DATACACHENUM(p)	(((p) & ((1 << PMT_DATACACHE_BUFF_NUM_SHIFT) - 1)) - 1)
#define PMT_SET_DATACACHE_BUF_NUM(p, num)	((p) = (((p) & (~((1 << PMT_DATACACHE_BUFF_NUM_SHIFT) - 1))) \
						| ((num) + 1)))
#define PMT_IS_DATA_INCACHE(p)	((p) & ((1 << PMT_DATACACHE_BUFF_NUM_SHIFT) - 1))
#define PMT_RESET_DATA_INCACHE(p)	((p) &= (~((1 << PMT_DATACACHE_BUFF_NUM_SHIFT) - 1)))

/*
//#define BIT_UPDATE(p)			((p) += NAND_PAGE_SIZE)
//#define BIT_UPDATE_FSPAGE(p)		((p) += FS_PAGE_SIZE)
*/
#define BIT_UPDATE(p, size)			((p) += (size))
#define PAGE_GET_DATA_NUM(p)		((p) & 0x7FFFFFFF)
#define PAGE_BEEN_READ(p)			((p) & 0x80000000)
#define PAGE_SET_READ(p)			((p) |= 0x80000000)

struct mt_ftl_data_header {
	sector_t sector;
	unsigned int offset_len;	/* offset(2bytes):length(2bytes) */
};

struct mt_ftl_param {
	/* Indicate next used replay page. */
	unsigned int u4NextReplayOffsetIndicator;  /* 4 bytes */

	/* Indicate next used leb/page. leb(20bits):Page(12bits) */
	unsigned int u4NextLebPageIndicator;  /* 4 bytes */

	/* Indicate current used leb/page for PMT indicator. leb(20bits):Page(12bits) */
	/* Leb From 3 to PMT_BLOCK_NUM + 3 - 1 */
	unsigned int u4CurrentPMTLebPageIndicator;  /* 4 bytes */

	/* Indicate next free Leb or invalid number for no free leb.
	 * It is at most desc->vol->ubi->volumes[0]->reserved_pebs - 1 */	/* TODO: volume number need to change */
	unsigned int u4NextFreeLebIndicator;  /* 4 bytes */
	/* Leb From 3 to PMT_BLOCK_NUM + 3 - 1 */
	unsigned int u4NextFreePMTLebIndicator;  /* 4 bytes */

	/* Reserved leb for Garbage Collection */
	unsigned int u4GCReserveLeb; /* 4 bytes */
	/* Leb From 3 to PMT_BLOCK_NUM + 3 - 1 */
	unsigned int u4GCReservePMTLeb; /* 4 bytes */

	/* Compressor handler */
	struct crypto_comp *cc;

	/* Page Mapping Table Indicator, used to indicate PMT position in NAND.
	 * (Block_num(18bits):Page_num(10bits):Dirty(1bit):CachedBuffer(3bits))*/
	unsigned int *u4PMTIndicator;   /* 2K bytes */
	/* TODO: need to adjust and consider to use information from ubi */

	/* Page Mapping Table in cache. */
	/* Block_num(20bits):Page_num(12bits) */
	unsigned int *u4PMTCache;   /* 16K * 4 */
	/* Data_size(16bits):Part_num(12bits):CachedBuffer(4bits) */
	unsigned int *u4MetaPMTCache;   /* 16K * 4 */
	int *i4CurrentPMTClusterInCache;
	unsigned int *u4ReadPMTCache;   /* 16K */
	unsigned int *u4ReadMetaPMTCache;   /* 16K */
	int i4CurrentReadPMTClusterInCache;

	/* Block Invalid Table. Store invalid page amount of a block
	 * Unit of content: bytes */
	unsigned int *u4BIT;   /* 8K */

	/* Block Data Cache Buffer, collect data and store to NAND flash when it is full */
	unsigned char *u1DataCache;	/* 16K */

	/* Data information, including address (4bytes), page offset (2bytes) and data length (2bytes) */
	struct mt_ftl_data_header *u4Header;
	struct mt_ftl_data_header *u4ReadHeader;
	unsigned int u4DataNum;

	/* Replay Block Record */
	unsigned int *replay_blk_rec;
	int replay_blk_index;

	/* Page buffers */
	unsigned int *general_page_buffer;	/* 16K */
	unsigned int *replay_page_buffer;	/* 16K */
	unsigned int *commit_page_buffer;	/* 16K */
	unsigned int *gc_page_buffer;	/* 16K */
	unsigned char *cmpr_page_buffer;	/* 16K */
	unsigned int *tmp_page_buffer;	/* 16K */
};

struct mt_ftl_blk {
	struct ubi_volume_desc *desc;
	struct mt_ftl_param *param;
	int ubi_num;
	int vol_id;
	int refcnt;
	int leb_size;

	struct gendisk *gd;
	struct request_queue *rq;

	struct workqueue_struct *wq;
	struct work_struct work;

	struct mutex dev_mutex;
	spinlock_t queue_lock;
	struct list_head list;

	enum { STATE_EMPTY, STATE_CLEAN, STATE_DIRTY } cache_state;
	void *cache;
	int cache_leb_num;
	int sync;
};

#ifdef PROFILE
enum {
	MT_FTL_PROFILE_WRITE_ALL,
	MT_FTL_PROFILE_WRITE_COPYTOCACHE,
	MT_FTL_PROFILE_WRITE_UPDATEPMT,
	MT_FTL_PROFILE_UPDATE_PMT_MODIFYPMT,
	MT_FTL_PROFILE_UPDATE_PMT_FINDCACHE_COMMITPMT,
	MT_FTL_PROFILE_COMMIT_PMT,
	MT_FTL_PROFILE_UPDATE_PMT_DOWNLOADPMT,
	MT_FTL_PROFILE_WRITE_COMPRESS,
	MT_FTL_PROFILE_WRITE_WRITEPAGE,
	MT_FTL_PROFILE_WRITE_PAGE_WRITEOOB,
	MT_FTL_PROFILE_WRITE_PAGE_GETFREEBLK,
	MT_FTL_PROFILE_GETFREEBLOCK_GETLEB,
	MT_FTL_PROFILE_GC_FINDBLK,
	MT_FTL_PROFILE_GC_CPVALID,
	MT_FTL_PROFILE_GC_DATA_READOOB,
	MT_FTL_PROFILE_GC_DATA_READ_UPDATE_PMT,
	MT_FTL_PROFILE_GC_DATA_WRITEOOB,
	MT_FTL_PROFILE_GC_REMAP,
	MT_FTL_PROFILE_GETFREEBLOCK_PUTREPLAY_COMMIT,
	MT_FTL_PROFILE_COMMIT,
	MT_FTL_PROFILE_WRITE_PAGE_RESET,
	MT_FTL_PROFILE_READ_ALL,
	MT_FTL_PROFILE_READ_GETPMT,
	MT_FTL_PROFILE_READ_DATATOCACHE,
	MT_FTL_PROFILE_READ_DATATOCACHE_TEST1,
	MT_FTL_PROFILE_READ_DATATOCACHE_TEST2,
	MT_FTL_PROFILE_READ_DATATOCACHE_TEST3,
	MT_FTL_PROFILE_READ_ADDRNOMATCH,
	MT_FTL_PROFILE_READ_DECOMP,
	MT_FTL_PROFILE_READ_COPYTOBUFF,
	MT_FTL_PROFILE_TOTAL_PROFILE_NUM
};

static char *mtk_ftl_profile_message[MT_FTL_PROFILE_TOTAL_PROFILE_NUM] = {
	"MT_FTL_PROFILE_WRITE_ALL",
	"MT_FTL_PROFILE_WRITE_COPYTOCACHE",
	"MT_FTL_PROFILE_WRITE_UPDATEPMT",
	"MT_FTL_PROFILE_UPDATE_PMT_MODIFYPMT",
	"MT_FTL_PROFILE_UPDATE_PMT_FINDCACHE_COMMITPMT",
	"MT_FTL_PROFILE_COMMIT_PMT",
	"MT_FTL_PROFILE_UPDATE_PMT_DOWNLOADPMT",
	"MT_FTL_PROFILE_WRITE_COMPRESS",
	"MT_FTL_PROFILE_WRITE_WRITEPAGE",
	"MT_FTL_PROFILE_WRITE_PAGE_WRITEOOB",
	"MT_FTL_PROFILE_WRITE_PAGE_GETFREEBLK",
	"MT_FTL_PROFILE_GETFREEBLOCK_GETLEB",
	"MT_FTL_PROFILE_GC_FINDBLK",
	"MT_FTL_PROFILE_GC_CPVALID",
	"MT_FTL_PROFILE_GC_DATA_READOOB",
	"MT_FTL_PROFILE_GC_DATA_READ_UPDATE_PMT",
	"MT_FTL_PROFILE_GC_DATA_WRITEOOB",
	"MT_FTL_PROFILE_GC_REMAP",
	"MT_FTL_PROFILE_GETFREEBLOCK_PUTREPLAY_COMMIT",
	"MT_FTL_PROFILE_COMMIT",
	"MT_FTL_PROFILE_WRITE_PAGE_RESET",
	"MT_FTL_PROFILE_READ_ALL",
	"MT_FTL_PROFILE_READ_GETPMT",
	"MT_FTL_PROFILE_READ_DATATOCACHE",
	"MT_FTL_PROFILE_READ_DATATOCACHE_TEST1",
	"MT_FTL_PROFILE_READ_DATATOCACHE_TEST2",
	"MT_FTL_PROFILE_READ_DATATOCACHE_TEST3",
	"MT_FTL_PROFILE_READ_ADDRNOMATCH",
	"MT_FTL_PROFILE_READ_DECOMP",
	"MT_FTL_PROFILE_READ_COPYTOBUFF",
};
#endif	/* PROFILE */


int mt_ftl_create(struct mt_ftl_blk *dev);
int mt_ftl_remove(struct mt_ftl_blk *dev);
int mt_ftl_write(struct mt_ftl_blk *dev, const char *buffer, sector_t address, int len);
int mt_ftl_read(struct mt_ftl_blk *dev, const char *buffer, sector_t address, int len);
int mt_ftl_commit(struct mt_ftl_blk *dev);

int mt_ftl_blk_create(struct ubi_volume_desc *desc);
int mt_ftl_blk_remove(struct ubi_volume_info *vi);
int mt_ftl_discard(struct mt_ftl_blk *dev, unsigned long sector, unsigned nr_sects);


int mt_ftl_updatePMT(struct mt_ftl_blk *dev, int cluster, int sec_offset, int leb, int offset, int part,
		unsigned int cmpr_data_size, bool isReplay, bool isCommitDataCache);	/* Temporary */
int mt_ftl_commit_indicators(struct mt_ftl_blk *dev);	/* Temporary */
int mt_ftl_commitPMT(struct mt_ftl_blk *dev, bool isReplay, bool isCommitDataCache);	/* Temporary */
int mt_ftl_commit(struct mt_ftl_blk *dev);	/* Temporary */

#endif /* !__LINUX_MTFTL_H__ */
