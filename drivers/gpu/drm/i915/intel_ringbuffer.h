#ifndef _INTEL_RINGBUFFER_H_
#define _INTEL_RINGBUFFER_H_

#include <linux/hashtable.h>

#define I915_CMD_HASH_ORDER 9

/* Early gen2 devices have a cacheline of just 32 bytes, using 64 is overkill,
 * but keeps the logic simple. Indeed, the whole purpose of this macro is just
 * to give some inclination as to some of the magic values used in the various
 * workarounds!
 */
#define CACHELINE_BYTES 64

/*
 * Gen2 BSpec "1. Programming Environment" / 1.4.4.6 "Ring Buffer Use"
 * Gen3 BSpec "vol1c Memory Interface Functions" / 2.3.4.5 "Ring Buffer Use"
 * Gen4+ BSpec "vol1c Memory Interface and Command Stream" / 5.3.4.5 "Ring Buffer Use"
 *
 * "If the Ring Buffer Head Pointer and the Tail Pointer are on the same
 * cacheline, the Head Pointer must not be greater than the Tail
 * Pointer."
 */
#define I915_RING_FREE_SPACE 64

struct  intel_hw_status_page {
	u32		*page_addr;
	unsigned int	gfx_addr;
	struct		drm_i915_gem_object *obj;
};

/*
 * Gen7:
 * These values must match the requirements of the ring save/restore functions
 * which may need to change for different versions of the chip
 */
#define GEN7_COMMON_RING_CTX_SIZE 6
#define GEN7_RCS_RING_CTX_SIZE 14
#define GEN7_VCS_RING_CTX_SIZE 10
#define GEN7_BCS_RING_CTX_SIZE 11
#define GEN7_VECS_RING_CTX_SIZE 8
#define MAX_CTX(a, b) (((a) > (b)) ? (a) : (b))

/* Largest of individual rings + common */
#define GEN7_RING_CONTEXT_SIZE (GEN7_COMMON_RING_CTX_SIZE +	 \
			MAX_CTX(MAX_CTX(GEN7_RCS_RING_CTX_SIZE,	 \
					GEN7_VCS_RING_CTX_SIZE), \
				GEN7_BCS_RING_CTX_SIZE))


/*
 * Gen8:
 * Ring state maintained throughout ring reset contains
 * the following registers:
 *     Head
 *     Tail
 *     Ring buffer control
 *
 * The remaining registers are reinitialized, not restored.
 */
#define GEN8_RING_CONTEXT_SIZE 3

#define I915_RING_CONTEXT_SIZE \
		MAX_CTX(GEN7_RING_CONTEXT_SIZE, \
			GEN8_RING_CONTEXT_SIZE)

#define WATCHDOG_ENABLE 0

#define I915_READ_TAIL(ring) I915_READ(RING_TAIL((ring)->mmio_base))
#define I915_WRITE_TAIL(ring, val) I915_WRITE(RING_TAIL((ring)->mmio_base), val)

#define I915_READ_START(ring) I915_READ(RING_START((ring)->mmio_base))
#define I915_WRITE_START(ring, val) I915_WRITE(RING_START((ring)->mmio_base), val)

#define I915_READ_HEAD(ring)  I915_READ(RING_HEAD((ring)->mmio_base))
#define I915_WRITE_HEAD(ring, val) I915_WRITE(RING_HEAD((ring)->mmio_base), val)

#define I915_READ_CTL(ring) I915_READ(RING_CTL((ring)->mmio_base))
#define I915_WRITE_CTL(ring, val) I915_WRITE(RING_CTL((ring)->mmio_base), val)

#define I915_READ_IMR(ring) I915_READ(RING_IMR((ring)->mmio_base))
#define I915_WRITE_IMR(ring, val) I915_WRITE(RING_IMR((ring)->mmio_base), val)

#define I915_READ_MODE(ring) I915_READ(RING_MI_MODE((ring)->mmio_base))
#define I915_WRITE_MODE(ring, val) I915_WRITE(RING_MI_MODE((ring)->mmio_base), val)



#define __INTEL_READ_CTX_OR_MMIO__(engine, ctx, outval, ctxreg, mmioreg) \
({ \
	int __ret = 0; \
	if (i915.enable_execlists) \
		__ret = intel_execlists_read_##ctxreg((engine), \
						      (ctx), \
						      &(outval)); \
	else \
		((outval) = I915_READ_##mmioreg(engine)); \
	__ret; \
})

#define __INTEL_WRITE_CTX_OR_MMIO__(engine, ctx, val, ctxreg, mmioreg) \
({ \
	int __ret = 0; \
	if (i915.enable_execlists) \
		__ret = intel_execlists_write_##ctxreg((engine), \
						       (ctx), \
						       (val)); \
	else \
		(I915_WRITE_##mmioreg((engine), (val))); \
	__ret; \
})

#define __INTEL_WRITE_CTX_AND_MMIO__(engine, ctx, val, ctxreg, mmioreg) \
({ \
	int __ret = 0; \
	(I915_WRITE_##mmioreg((engine), (val))); \
	if (i915.enable_execlists) \
		__ret = intel_execlists_write_##ctxreg((engine), \
						       (ctx), \
						       (val)); \
	__ret; \
})

/*
 * Unified context/MMIO register access macros
 *
 * Macros for accessing context registers or MMIO registers depending on mode.
 * The macros depend on the global i915.execlists_enabled flag to determine
 * which macro/function to use.
 *
 * engine: Engine that context belongs to
 * ctx: Context to access value from
 * val/outval: Name of scalar variable to be written/read
 *
 * WARNING! These macros are semantically different from their
 * legacy MMIO-register counterparts above in that these macros return
 * error codes (0 for OK or otherwise various kernel error codes) instead
 * of returning the read value as the return value.
 */
#define I915_READ_TAIL_CTX(engine, ctx, outval) \
	__INTEL_READ_CTX_OR_MMIO__((engine), (ctx), (outval), tail, TAIL)

#define I915_WRITE_TAIL_CTX(engine, ctx, val) \
	__INTEL_WRITE_CTX_OR_MMIO__((engine), (ctx), (val), tail, TAIL)

#define I915_READ_HEAD_CTX(engine, ctx, outval) \
	__INTEL_READ_CTX_OR_MMIO__((engine), (ctx), (outval), head, HEAD)

#define I915_WRITE_HEAD_CTX(engine, ctx, val) \
	__INTEL_WRITE_CTX_OR_MMIO__((engine), (ctx), (val), head, HEAD)

#define I915_READ_CTL_CTX(engine, ctx, outval) \
	__INTEL_READ_CTX_OR_MMIO__((engine), (ctx), (outval), buffer_ctl, CTL)

#define I915_WRITE_CTL_CTX(engine, ctx, val) \
	__INTEL_WRITE_CTX_OR_MMIO__((engine), (ctx), (val), buffer_ctl, CTL)

/*
 * Coordinated context/MMIO register write macros
 *
 * We need to do coordinated writes to both a context register and a MMIO
 * register in a number of places. These macros will write to both sets of
 * registers.
 *
 * engine: The engine to write to
 * ctx: Context to write to
 * val: Value to write
 */
#define I915_WRITE_TAIL_CTX_MMIO(engine, ctx, val) \
	__INTEL_WRITE_CTX_AND_MMIO__((engine), (ctx), (val), tail, TAIL)

#define I915_WRITE_HEAD_CTX_MMIO(engine, ctx, val) \
	__INTEL_WRITE_CTX_AND_MMIO__((engine), (ctx), (val), head, HEAD)

#define I915_WRITE_CTL_CTX_MMIO(engine, ctx, val) \
	__INTEL_WRITE_CTX_AND_MMIO__((engine), (ctx), (val), buffer_ctl, CTL)

enum intel_ring_hangcheck_action {
	HANGCHECK_IDLE = 0,
	HANGCHECK_WAIT,
	HANGCHECK_ACTIVE,
	HANGCHECK_KICK,
	HANGCHECK_HUNG,
};

#define RESET_HEAD_TAIL 0x1
#define FORCE_ADVANCE   0x2

struct intel_ring_hangcheck {
	enum intel_ring_hangcheck_action action;

	/* The ring being monitored */
	u32 ringid;

	/* Parent drm_device */
	struct drm_device *dev;

	/* Work queue for this ring only */
	struct delayed_work work;
	struct workqueue_struct *wq;

	/* Count of consecutive hang detections
	 * (reset flag set once count exceeds threshold) */
#define DRM_I915_HANGCHECK_THRESHOLD 1
#define DRM_I915_MBOX_HANGCHECK_THRESHOLD 4
	u32 count;

	/* Last sampled head and active head */
	u32 last_acthd;
	u32 last_hd;

	/* Last recorded ring head index.
	* This is only ever a ring index where as active
	* head may be a graphics address in a ring buffer */
	u32 last_head;

	/* Last recorded instdone */
	u32 prev_instdone[I915_NUM_INSTDONE_REG];

	/* Flag to indicate if ring reset required */
#define DRM_I915_HANGCHECK_HUNG 0x01 /* Indicates this ring has hung */
#define DRM_I915_HANGCHECK_RESET 0x02 /* Indicates request to reset this ring */
	atomic_t flags;

	/* Keep a record of the last time the ring was reset */
	unsigned long last_reset;

	/* Number of times this ring has been
	* reset since boot*/
	u32 total;

	/* Number of TDR hang detections of r */
	u32 tdr_count;

	/* Number of watchdog hang detections for this ring */
	u32 watchdog_count;

	/*
	 * Ring seqno recorded by the most recent hang check.
	 * Used as a first, coarse step to determine ring
	 * idleness
	 */
	u32 last_seqno;

	/* Forced resubmission counter */
	u32 forced_resubmission_cnt;

	/*
	 * Number of detections before forced resubmission is
	 * carried out. Yes, this number is arbitrary and is based
	 * on empirical evidence.
	 */
#define DRM_I915_FORCED_RESUBMISSION_THRESHOLD 2

};

struct intel_ringbuffer {
	struct drm_i915_gem_object *obj;
	void __iomem *virtual_start;

	struct intel_engine_cs *ring;

	/*
	 * FIXME: This backpointer is an artifact of the history of how the
	 * execlist patches came into being. It will get removed once the basic
	 * code has landed.
	 */
	struct intel_context *FIXME_lrc_ctx;

	u32 head;
	u32 tail;
	int space;
	int size;
	int effective_size;

	/** We track the position of the requests in the ring buffer, and
	 * when each is retired we increment last_retired_head as the GPU
	 * must have finished processing the request and so we know we
	 * can advance the ringbuffer up to that position.
	 *
	 * last_retired_head is set to -1 after the value is consumed so
	 * we can detect new retirements.
	 */
	u32 last_retired_head;
};

struct i915_sync_timeline;

struct intel_engine_cs {
	const char	*name;
	enum intel_ring_id {
		RCS = 0x0,
		VCS,
		BCS,
		VECS,
		VCS2
	} id;
#define I915_NUM_RINGS 5
#define LAST_USER_RING (VECS + 1)
	u32		mmio_base;
	struct		drm_device *dev;
	struct intel_ringbuffer *buffer;

	struct intel_hw_status_page status_page;

	unsigned irq_refcount; /* protected by dev_priv->irq_lock */
	u32		irq_enable_mask;	/* bitmask to enable ring interrupt */
	struct drm_i915_gem_request *trace_irq_req;
	bool __must_check (*irq_get)(struct intel_engine_cs *ring);
	void		(*irq_put)(struct intel_engine_cs *ring);

	int		(*init)(struct intel_engine_cs *ring);

	int		(*init_context)(struct intel_engine_cs *ring,
					struct intel_context *ctx);

	void		(*write_tail)(struct intel_engine_cs *ring,
				      u32 value);
	int __must_check (*flush)(struct intel_engine_cs *ring,
				  u32	invalidate_domains,
				  u32	flush_domains);
	int		(*add_request)(struct intel_engine_cs *ring);
	/* Some chipsets are not quite as coherent as advertised and need
	 * an expensive kick to force a true read of the up-to-date seqno.
	 * However, the up-to-date seqno is not always required and the last
	 * seen value is good enough. Note that the seqno will always be
	 * monotonic, even if not coherent.
	 */
	u32		(*get_seqno)(struct intel_engine_cs *ring,
				     bool lazy_coherency);
	void		(*set_seqno)(struct intel_engine_cs *ring,
				     u32 seqno);
	int		(*dispatch_execbuffer)(struct intel_engine_cs *ring,
					       u64 offset, u32 length,
					       unsigned dispatch_flags);
#define I915_DISPATCH_SECURE            (1 << 0)
#define I915_DISPATCH_PINNED            (1 << 1)
#define I915_DISPATCH_LAUNCH_CB2        (1 << 2)
	void		(*cleanup)(struct intel_engine_cs *ring);
	int (*enable)(struct intel_engine_cs *ring, struct intel_context *ctx);
	int (*disable)(struct intel_engine_cs *ring, struct intel_context *ctx);
	int (*start)(struct intel_engine_cs *ring);
	int (*stop)(struct intel_engine_cs *ring);
	int (*save)(struct intel_engine_cs *ring, struct intel_context *ctx,
		    uint32_t *data, uint32_t data_size, u32 flags);
	int (*restore)(struct intel_engine_cs *ring, struct intel_context *ctx,
		       uint32_t *data, uint32_t data_size);
	int (*invalidate_tlb)(struct intel_engine_cs *ring);

	struct {
		u32	sync_seqno[I915_NUM_RINGS-1];

		struct {
			/* our mbox written by others */
			u32		wait[I915_NUM_RINGS];
			/* mboxes this ring signals to */
			u32		signal[I915_NUM_RINGS];
		} mbox;

		/* AKA wait() */
		int	(*sync_to)(struct intel_engine_cs *ring,
				   struct intel_engine_cs *to,
				   u32 seqno);
		int	(*signal)(struct intel_engine_cs *signaller,
				  /* num_dwords needed by caller */
				  unsigned int num_dwords);
	} semaphore;

	/* Execlists */
	spinlock_t execlist_lock;
	struct list_head execlist_queue;
	struct list_head execlist_retired_req_list;
	u8 next_context_status_buffer;
	u32             irq_keep_mask; /* bitmask for interrupts that should not be masked */
	int		(*emit_request)(struct intel_ringbuffer *ringbuf);
	int		(*emit_flush)(struct intel_ringbuffer *ringbuf,
				      u32 invalidate_domains,
				      u32 flush_domains);
	int		(*emit_bb_start)(struct intel_ringbuffer *ringbuf,
					 u64 offset, unsigned flags);

	/**
	 * List of objects currently involved in rendering from the
	 * ringbuffer.
	 *
	 * Includes buffers having the contents of their GPU caches
	 * flushed, not necessarily primitives.  last_read_req
	 * represents when the rendering involved will be completed.
	 *
	 * A reference is held on the buffer while on this list.
	 */
	struct list_head active_list;

	/**
	 * List of breadcrumbs associated with GPU requests currently
	 * outstanding.
	 */
	struct list_head request_list;
	spinlock_t reqlist_lock;
	struct list_head delayed_free_list;

	/**
	 * Do we have some not yet emitted requests outstanding?
	 */
	struct drm_i915_gem_request *outstanding_lazy_request;
	bool gpu_caches_dirty;
	bool fbc_dirty;

	/* For optimising request completion events */
	u32 last_read_seqno;

	wait_queue_head_t irq_queue;

	struct intel_context *default_context;
	struct intel_context *last_context;

	struct intel_ring_hangcheck hangcheck;
	/*
	 * Area large enough to store all the register
	 * data associated with this ring
	 */
	u32 saved_state[I915_RING_CONTEXT_SIZE];
	uint32_t last_irq_seqno;

	/*
	 * Watchdog timer threshold values
	 * only RCS, VCS, VCS2 rings have watchdog timeout support
	 */
	uint32_t watchdog_threshold;

	struct {
		struct drm_i915_gem_object *obj;
		u32 gtt_offset;
		volatile u32 *cpu_page;
	} scratch;

	bool needs_cmd_parser;

	/*
	 * Table of commands the command parser needs to know about
	 * for this ring.
	 */
	DECLARE_HASHTABLE(cmd_hash, I915_CMD_HASH_ORDER);

	/*
	 * Table of registers allowed in commands that read/write registers.
	 */
	const u32 *reg_table;
	int reg_count;

	/*
	 * Table of registers allowed in commands that read/write registers, but
	 * only from the DRM master.
	 */
	const u32 *master_reg_table;
	int master_reg_count;

	/*
	 * Returns the bitmask for the length field of the specified command.
	 * Return 0 for an unrecognized/invalid command.
	 *
	 * If the command parser finds an entry for a command in the ring's
	 * cmd_tables, it gets the command's length based on the table entry.
	 * If not, it calls this function to determine the per-ring length field
	 * encoding for the command (i.e. certain opcode ranges use certain bits
	 * to encode the command length in the header).
	 */
	u32 (*get_cmd_length_mask)(u32 cmd_header);

#ifdef CONFIG_DRM_I915_SYNC
	struct i915_sync_timeline *timeline;
	u32 active_seqno; /* Contains the failing seqno on ring timeout. */
#endif
};

bool intel_ring_initialized(struct intel_engine_cs *ring);

static inline unsigned
intel_ring_flag(struct intel_engine_cs *ring)
{
	return 1 << ring->id;
}

static inline u32
intel_ring_sync_index(struct intel_engine_cs *ring,
		      struct intel_engine_cs *other)
{
	int idx;

	/*
	 * cs -> 0 = vcs, 1 = bcs
	 * vcs -> 0 = bcs, 1 = cs,
	 * bcs -> 0 = cs, 1 = vcs.
	 */

	idx = (other - ring) - 1;
	if (idx < 0)
		idx += I915_NUM_RINGS;

	return idx;
}

static inline u32
intel_read_status_page(struct intel_engine_cs *ring,
		       int reg)
{
	/* Ensure that the compiler doesn't optimize away the load. */
	barrier();
	return ring->status_page.page_addr[reg];
}

static inline void
intel_write_status_page(struct intel_engine_cs *ring,
			int reg, u32 value)
{
	ring->status_page.page_addr[reg] = value;
}

/**
 * Reads a dword out of the status page, which is written to from the command
 * queue by automatic updates, MI_REPORT_HEAD, MI_STORE_DATA_INDEX, or
 * MI_STORE_DATA_IMM.
 *
 * The following dwords have a reserved meaning:
 * 0x00: ISR copy, updated when an ISR bit not set in the HWSTAM changes.
 * 0x04: ring 0 head pointer
 * 0x05: ring 1 head pointer (915-class)
 * 0x06: ring 2 head pointer (915-class)
 * 0x10-0x1b: Context status DWords (GM45)
 * 0x1f: Last written status offset. (GM45)
 *
 * The area from dword 0x20 to 0x3ff is available for driver usage.
 */
#define I915_GEM_HWS_INDEX		0x20
#define I915_GEM_HWS_SCRATCH_INDEX	0x30
#define I915_GEM_HWS_SCRATCH_ADDR (I915_GEM_HWS_SCRATCH_INDEX << MI_STORE_DWORD_INDEX_SHIFT)
#define I915_GEM_ACTIVE_SEQNO_INDEX     0x34
#define I915_GEM_PGFLIP_INDEX           0x35

void intel_unpin_ringbuffer_obj(struct intel_ringbuffer *ringbuf);
int intel_pin_and_map_ringbuffer_obj(struct drm_device *dev,
				     struct intel_ringbuffer *ringbuf);
void intel_destroy_ringbuffer_obj(struct intel_ringbuffer *ringbuf);
int intel_alloc_ringbuffer_obj(struct drm_device *dev,
			       struct intel_ringbuffer *ringbuf);

void intel_stop_ring_buffer(struct intel_engine_cs *ring);
void intel_cleanup_ring_buffer(struct intel_engine_cs *ring);
int __must_check intel_ring_alloc_request(struct intel_engine_cs *ring);

int __must_check intel_ring_begin(struct intel_engine_cs *ring, int n);
int __must_check intel_ring_cacheline_align(struct intel_engine_cs *ring);
static inline void intel_ring_emit(struct intel_engine_cs *ring,
				   u32 data)
{
	struct intel_ringbuffer *ringbuf = ring->buffer;
	iowrite32(data, ringbuf->virtual_start + ringbuf->tail);
	ringbuf->tail += 4;
}
static inline void intel_ring_advance(struct intel_engine_cs *ring)
{
	struct intel_ringbuffer *ringbuf = ring->buffer;
	ringbuf->tail &= ringbuf->size - 1;
}

int __intel_ring_space(int head, int tail, int size);
void intel_ring_update_space(struct intel_ringbuffer *ringbuf);
int intel_ring_space(struct intel_ringbuffer *ringbuf);
bool intel_ring_stopped(struct intel_engine_cs *ring);
void __intel_ring_advance(struct intel_engine_cs *ring);
void intel_gpu_engine_reset_resample(struct intel_engine_cs *ring,
		struct intel_context *ctx);
void intel_gpu_reset_resample(struct intel_engine_cs *ring,
		struct intel_context *ctx);
int intel_ring_disable(struct intel_engine_cs *ring,
		struct intel_context *ctx);
int intel_ring_enable(struct intel_engine_cs *ring,
		struct intel_context *ctx);
int intel_ring_save(struct intel_engine_cs *ring,
		struct intel_context *ctx, u32 flags);
int intel_ring_restore(struct intel_engine_cs *ring,
		struct intel_context *ctx);
int intel_ring_invalidate_tlb(struct intel_engine_cs *ring);

static inline int intel_ring_supports_watchdog(struct intel_engine_cs *ring)
{
	/* Return 1 if the ring supports watchdog reset, otherwise 0 */
	if (ring) {
		int ret;

		ret = (ring->id == RCS || ring->id == VCS || ring->id == VCS2);
		DRM_DEBUG_TDR("%s %s support watchdog timeout\n",
				ring->name, ret?"does":"does NOT");
		return ret;
	}

	return 0;
}
int intel_ring_start_watchdog(struct intel_engine_cs *ring);
int intel_ring_stop_watchdog(struct intel_engine_cs *ring);

int __must_check intel_ring_idle(struct intel_engine_cs *ring);
void intel_ring_init_seqno(struct intel_engine_cs *ring, u32 seqno);
int intel_ring_flush_all_caches(struct intel_engine_cs *ring);
int intel_ring_invalidate_all_caches(struct intel_engine_cs *ring);

void intel_fini_pipe_control(struct intel_engine_cs *ring);
int intel_init_pipe_control(struct intel_engine_cs *ring);

int intel_init_render_ring_buffer(struct drm_device *dev);
int intel_init_bsd_ring_buffer(struct drm_device *dev);
int intel_init_bsd2_ring_buffer(struct drm_device *dev);
int intel_init_blt_ring_buffer(struct drm_device *dev);
int intel_init_vebox_ring_buffer(struct drm_device *dev);

u64 intel_ring_get_active_head(struct intel_engine_cs *ring);
void intel_ring_setup_status_page(struct intel_engine_cs *ring);
u32 get_pipe_control_scratch_addr(struct intel_engine_cs *ring);
int init_workarounds_ring(struct intel_engine_cs *ring);


static inline u32 intel_ring_get_tail(struct intel_ringbuffer *ringbuf)
{
	return ringbuf->tail;
}

int i915_write_active_request(struct intel_engine_cs *ring,
			      struct drm_i915_gem_request *req);

static inline struct drm_i915_gem_request *
intel_ring_get_request(struct intel_engine_cs *ring)
{
	BUG_ON(ring->outstanding_lazy_request == NULL);
	return ring->outstanding_lazy_request;
}

/* DRI warts */
int intel_render_ring_init_dri(struct drm_device *dev, u64 start, u32 size);

#endif /* _INTEL_RINGBUFFER_H_ */
