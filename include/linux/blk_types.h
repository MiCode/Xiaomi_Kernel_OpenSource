/*
 * Block data types and constants.  Directly include this file only to
 * break include dependency loop.
 */
#ifndef __LINUX_BLK_TYPES_H
#define __LINUX_BLK_TYPES_H

#include <linux/types.h>
#include <linux/bvec.h>

struct bio_set;
struct bio;
struct bio_integrity_payload;
struct page;
struct block_device;
struct io_context;
struct cgroup_subsys_state;
typedef void (bio_end_io_t) (struct bio *);

struct bio_crypt_ctx {
	unsigned int	bc_flags;
	unsigned int	bc_key_size;
	unsigned long	bc_fs_type;
	struct super_block	*bc_sb;
	unsigned long	bc_ino;
	unsigned long   bc_iv;
	struct key	*bc_keyring_key;
#ifdef CONFIG_HIE_DUMMY_CRYPT
	u32			dummy_crypt_key;
#endif
};

#ifdef CONFIG_BLOCK
/*
 * main unit of I/O for the block layer and lower layers (ie drivers and
 * stacking drivers)
 */
struct bio {
	struct bio		*bi_next;	/* request queue link */
	struct block_device	*bi_bdev;
	unsigned short		bi_write_hint;
	int			bi_error;
	unsigned int		bi_opf;		/* bottom bits req flags,
						 * top bits REQ_OP. Use
						 * accessors.
						 */
	unsigned short		bi_flags;	/* status, command, etc */
	unsigned short		bi_ioprio;

	struct bvec_iter	bi_iter;

	/* Number of segments in this BIO after
	 * physical address coalescing is performed.
	 */
	unsigned int		bi_phys_segments;

	/*
	 * To keep track of the max segment size, we account for the
	 * sizes of the first and last mergeable segments in this bio.
	 */
	unsigned int		bi_seg_front_size;
	unsigned int		bi_seg_back_size;

	atomic_t		__bi_remaining;

	bio_end_io_t		*bi_end_io;

	void			*bi_private;
#ifdef CONFIG_BLK_CGROUP
	/*
	 * Optional ioc and css associated with this bio.  Put on bio
	 * release.  Read comment on top of bio_associate_current().
	 */
	struct io_context	*bi_ioc;
	struct cgroup_subsys_state *bi_css;
#endif
	union {
#if defined(CONFIG_BLK_DEV_INTEGRITY)
		struct bio_integrity_payload *bi_integrity; /* data integrity */
#endif
	};

	unsigned short		bi_vcnt;	/* how many bio_vec's */

#ifdef CONFIG_MTK_HW_FDE
	/*
	 * MTK PATH:
	 *
	 * Indicating this bio request needs encryption or decryption by
	 * HW FDE (Full Disk Encryption) engine.
	 *
	 * Set by DM Crypt.
	 * Quried by HW FDE engine driver, e.g., eMMC/UFS.
	 */
	unsigned int		bi_hw_fde;
	unsigned int		bi_key_idx;
#endif

	/*
	 * Everything starting with bi_max_vecs will be preserved by bio_reset()
	 */

	unsigned short		bi_max_vecs;	/* max bvl_vecs we can hold */

	atomic_t		__bi_cnt;	/* pin count */

	struct bio_vec		*bi_io_vec;	/* the actual vec list */

	struct bio_set		*bi_pool;

	/* Encryption context. May contain secret key material. */
	struct bio_crypt_ctx	bi_crypt_ctx;

	/*
	 * We can inline a number of vecs at the end of the bio, to avoid
	 * double allocations for a small number of bio_vecs. This member
	 * MUST obviously be kept at the very end of the bio.
	 */
	struct bio_vec		bi_inline_vecs[0];
};

#define BIO_OP_SHIFT	(8 * FIELD_SIZEOF(struct bio, bi_opf) - REQ_OP_BITS)
#define bio_flags(bio)	((bio)->bi_opf & ((1 << BIO_OP_SHIFT) - 1))
#define bio_op(bio)	((bio)->bi_opf >> BIO_OP_SHIFT)

#define bio_set_op_attrs(bio, op, op_flags) do {			\
	if (__builtin_constant_p(op))					\
		BUILD_BUG_ON((op) + 0U >= (1U << REQ_OP_BITS));		\
	else								\
		WARN_ON_ONCE((op) + 0U >= (1U << REQ_OP_BITS));		\
	if (__builtin_constant_p(op_flags))				\
		BUILD_BUG_ON((op_flags) + 0U >= (1U << BIO_OP_SHIFT));	\
	else								\
		WARN_ON_ONCE((op_flags) + 0U >= (1U << BIO_OP_SHIFT));	\
	(bio)->bi_opf = bio_flags(bio);					\
	(bio)->bi_opf |= (((op) + 0U) << BIO_OP_SHIFT);			\
	(bio)->bi_opf |= (op_flags);					\
} while (0)

#define BIO_RESET_BYTES		offsetof(struct bio, bi_max_vecs)

/*
 * bio flags
 */
#define BIO_SEG_VALID	1	/* bi_phys_segments valid */
#define BIO_CLONED	2	/* doesn't own data */
#define BIO_BOUNCED	3	/* bio is a bounce bio */
#define BIO_USER_MAPPED 4	/* contains user pages */
#define BIO_NULL_MAPPED 5	/* contains invalid user pages */
#define BIO_QUIET	6	/* Make BIO Quiet */
#define BIO_CHAIN	7	/* chained bio, ->bi_remaining in effect */
#define BIO_REFFED	8	/* bio has elevated ->bi_cnt */

/*
 * Flags starting here get preserved by bio_reset() - this includes
 * BVEC_POOL_IDX()
 */
#define BIO_RESET_BITS	10

/*
 * We support 6 different bvec pools, the last one is magic in that it
 * is backed by a mempool.
 */
#define BVEC_POOL_NR		6
#define BVEC_POOL_MAX		(BVEC_POOL_NR - 1)

/*
 * Top 4 bits of bio flags indicate the pool the bvecs came from.  We add
 * 1 to the actual index so that 0 indicates that there are no bvecs to be
 * freed.
 */
#define BVEC_POOL_BITS		(4)
#define BVEC_POOL_OFFSET	(16 - BVEC_POOL_BITS)
#define BVEC_POOL_IDX(bio)	((bio)->bi_flags >> BVEC_POOL_OFFSET)

#endif /* CONFIG_BLOCK */

/*
 * Request flags.  For use in the cmd_flags field of struct request, and in
 * bi_opf of struct bio.  Note that some flags are only valid in either one.
 */
enum rq_flag_bits {
	/* common flags */
	__REQ_FAILFAST_DEV,	/* no driver retries of device errors */
	__REQ_FAILFAST_TRANSPORT, /* no driver retries of transport errors */
	__REQ_FAILFAST_DRIVER,	/* no driver retries of driver errors */

	__REQ_SYNC,		/* request is sync (sync write or read) */
	__REQ_META,		/* metadata io request */
	__REQ_PRIO,		/* boost priority in cfq */

	__REQ_NOIDLE,		/* don't anticipate more IO after this one */
	__REQ_INTEGRITY,	/* I/O includes block integrity payload */
	__REQ_FUA,		/* forced unit access */
	__REQ_PREFLUSH,		/* request for cache flush */

	/* bio only flags */
	__REQ_RAHEAD,		/* read ahead, can fail anytime */
	__REQ_THROTTLED,	/* This bio has already been subjected to
				 * throttling rules. Don't do it again. */

	/* request only flags */
	__REQ_SORTED,		/* elevator knows about this request */
	__REQ_SOFTBARRIER,	/* may not be passed by ioscheduler */
	__REQ_NOMERGE,		/* don't touch this for merging */
	__REQ_STARTED,		/* drive already may have started this one */
	__REQ_DONTPREP,		/* don't call prep for this one */
	__REQ_QUEUED,		/* uses queueing */
	__REQ_ELVPRIV,		/* elevator private data attached */
	__REQ_FAILED,		/* set if the request failed */
	__REQ_QUIET,		/* don't worry about errors */
	__REQ_PREEMPT,		/* set for "ide_preempt" requests and also
				   for requests for which the SCSI "quiesce"
				   state must be ignored. */
	__REQ_ALLOCED,		/* request came from our alloc pool */
	__REQ_COPY_USER,	/* contains copies of user pages */
	__REQ_FLUSH_SEQ,	/* request for flush sequence */
	__REQ_IO_STAT,		/* account I/O stat */
	__REQ_MIXED_MERGE,	/* merge of different types, fail separately */
	__REQ_PM,		/* runtime pm request */
	__REQ_HASHED,		/* on IO scheduler merge hash */
	__REQ_MQ_INFLIGHT,	/* track inflight for MQ */
#ifdef MTK_UFS_HQA
	__REQ_POWER_LOSS,	/* MTK PATCH for SPOH */
#endif
	__REQ_DEV_STARTED,	/* MTK PATCH: submitted to storage device */
	__REQ_NR_BITS,		/* stops here */
};

#define REQ_FAILFAST_DEV	(1ULL << __REQ_FAILFAST_DEV)
#define REQ_FAILFAST_TRANSPORT	(1ULL << __REQ_FAILFAST_TRANSPORT)
#define REQ_FAILFAST_DRIVER	(1ULL << __REQ_FAILFAST_DRIVER)
#define REQ_SYNC		(1ULL << __REQ_SYNC)
#define REQ_META		(1ULL << __REQ_META)
#define REQ_PRIO		(1ULL << __REQ_PRIO)
#define REQ_NOIDLE		(1ULL << __REQ_NOIDLE)
#define REQ_INTEGRITY		(1ULL << __REQ_INTEGRITY)

#define REQ_FAILFAST_MASK \
	(REQ_FAILFAST_DEV | REQ_FAILFAST_TRANSPORT | REQ_FAILFAST_DRIVER)
#define REQ_COMMON_MASK \
	(REQ_FAILFAST_MASK | REQ_SYNC | REQ_META | REQ_PRIO | REQ_NOIDLE | \
	 REQ_PREFLUSH | REQ_FUA | REQ_INTEGRITY | REQ_NOMERGE)
#define REQ_CLONE_MASK		REQ_COMMON_MASK

/* This mask is used for both bio and request merge checking */
#define REQ_NOMERGE_FLAGS \
	(REQ_NOMERGE | REQ_STARTED | REQ_SOFTBARRIER | REQ_PREFLUSH | REQ_FUA | REQ_FLUSH_SEQ)

#define REQ_RAHEAD		(1ULL << __REQ_RAHEAD)
#define REQ_THROTTLED		(1ULL << __REQ_THROTTLED)

#define REQ_SORTED		(1ULL << __REQ_SORTED)
#define REQ_SOFTBARRIER		(1ULL << __REQ_SOFTBARRIER)
#define REQ_FUA			(1ULL << __REQ_FUA)
#define REQ_NOMERGE		(1ULL << __REQ_NOMERGE)
#define REQ_STARTED		(1ULL << __REQ_STARTED)
#define REQ_DONTPREP		(1ULL << __REQ_DONTPREP)
#define REQ_QUEUED		(1ULL << __REQ_QUEUED)
#define REQ_ELVPRIV		(1ULL << __REQ_ELVPRIV)
#define REQ_FAILED		(1ULL << __REQ_FAILED)
#define REQ_QUIET		(1ULL << __REQ_QUIET)
#define REQ_PREEMPT		(1ULL << __REQ_PREEMPT)
#define REQ_ALLOCED		(1ULL << __REQ_ALLOCED)
#define REQ_COPY_USER		(1ULL << __REQ_COPY_USER)
#define REQ_PREFLUSH		(1ULL << __REQ_PREFLUSH)
#define REQ_FLUSH_SEQ		(1ULL << __REQ_FLUSH_SEQ)
#define REQ_IO_STAT		(1ULL << __REQ_IO_STAT)
#define REQ_MIXED_MERGE		(1ULL << __REQ_MIXED_MERGE)
#define REQ_PM			(1ULL << __REQ_PM)
#define REQ_HASHED		(1ULL << __REQ_HASHED)
#define REQ_MQ_INFLIGHT		(1ULL << __REQ_MQ_INFLIGHT)
#ifdef MTK_UFS_HQA
/* MTK PATCH for SPOH */
#define REQ_POWER_LOSS		(1ULL << __REQ_POWER_LOSS)
#endif
#define REQ_DEV_STARTED		(1ULL << __REQ_DEV_STARTED) /* MTK PATCH */

enum req_op {
	REQ_OP_READ,
	REQ_OP_WRITE,
	REQ_OP_DISCARD,		/* request to discard sectors */
	REQ_OP_SECURE_ERASE,	/* request to securely erase sectors */
	REQ_OP_WRITE_SAME,	/* write same block many times */
	REQ_OP_FLUSH,		/* request for cache flush */
};

#define REQ_OP_BITS 3

typedef unsigned int blk_qc_t;
#define BLK_QC_T_NONE	-1U
#define BLK_QC_T_SHIFT	16

static inline bool blk_qc_t_valid(blk_qc_t cookie)
{
	return cookie != BLK_QC_T_NONE;
}

static inline blk_qc_t blk_tag_to_qc_t(unsigned int tag, unsigned int queue_num)
{
	return tag | (queue_num << BLK_QC_T_SHIFT);
}

static inline unsigned int blk_qc_t_to_queue_num(blk_qc_t cookie)
{
	return cookie >> BLK_QC_T_SHIFT;
}

static inline unsigned int blk_qc_t_to_tag(blk_qc_t cookie)
{
	return cookie & ((1u << BLK_QC_T_SHIFT) - 1);
}


/*
 * block crypt flags
 */
enum bc_flags_bits {
	__BC_CRYPT,        /* marks the request needs crypt */
	__BC_IV_PAGE_IDX,  /* use page index as iv. */
	__BC_IV_CTX,       /* use the iv saved in crypt context */
	__BC_AES_128_XTS,  /* crypt algorithms */
	__BC_AES_192_XTS,
	__BC_AES_256_XTS,
	__BC_AES_128_CBC,
	__BC_AES_256_CBC,
	__BC_AES_128_ECB,
	__BC_AES_256_ECB,
};

#define BC_CRYPT	(1UL << __BC_CRYPT)
#define BC_IV_PAGE_IDX  (1UL << __BC_IV_PAGE_IDX)
#define BC_IV_CTX       (1UL << __BC_IV_CTX)
#define BC_AES_128_XTS	(1UL << __BC_AES_128_XTS)
#define BC_AES_192_XTS	(1UL << __BC_AES_192_XTS)
#define BC_AES_256_XTS	(1UL << __BC_AES_256_XTS)
#define BC_AES_128_CBC	(1UL << __BC_AES_128_CBC)
#define BC_AES_256_CBC	(1UL << __BC_AES_256_CBC)
#define BC_AES_128_ECB	(1UL << __BC_AES_128_ECB)
#define BC_AES_256_ECB	(1UL << __BC_AES_256_ECB)

#define BC_INVALD_IV    (~0UL)

static inline void bio_bcf_set(struct bio *bio, unsigned int flag)
{
	if (bio)
		bio->bi_crypt_ctx.bc_flags |= flag;
}

static inline void bio_bcf_clear(struct bio *bio, unsigned int flag)
{
	if (bio)
		bio->bi_crypt_ctx.bc_flags &= (~flag);
}

static inline bool bio_bcf_test(struct bio *bio, unsigned int flag)
{
	return bio ? (bio->bi_crypt_ctx.bc_flags & flag) : 0;
}

static inline bool bio_encrypted(struct bio *bio)
{
	return bio_bcf_test(bio, BC_CRYPT);
}

static inline unsigned long bio_bc_ino(struct bio *bio)
{
	return bio->bi_crypt_ctx.bc_ino;
}

static inline void *bio_bc_sb(struct bio *bio)
{
	return (void *)bio->bi_crypt_ctx.bc_sb;
}

static inline
void bio_bc_iv_set(struct bio *bio, unsigned long iv)
{
	bio->bi_crypt_ctx.bc_iv = iv;
	bio_bcf_set(bio, BC_IV_CTX);
}

unsigned long bio_bc_iv_get(struct bio *bio);

#endif /* __LINUX_BLK_TYPES_H */
