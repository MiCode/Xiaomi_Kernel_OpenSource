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

/* #define DEBUG */
#define ALIGN_CPU

#include <linux/spinlock.h>
#include <linux/debugfs.h>
#include <linux/relay.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/sched.h>

#include "kgsl.h"
#include "kgsl_cffdump.h"
#include "kgsl_debugfs.h"
#include "kgsl_log.h"
#include "kgsl_sharedmem.h"
#include "adreno_pm4types.h"
#include "adreno.h"
#include "adreno_cp_parser.h"

static struct rchan	*chan;
static struct dentry	*dir;
static int		suspended;
static size_t		dropped;
static size_t		subbuf_size = 256*1024;
static size_t		n_subbufs = 64;

/* forward declarations */
static void destroy_channel(void);
static struct rchan *create_channel(unsigned subbuf_size, unsigned n_subbufs);

static spinlock_t cffdump_lock;
static ulong serial_nr;
static ulong total_bytes;
static ulong total_syncmem;
static long last_sec;

/* Some simulators have start address of gmem at this offset */
#define KGSL_CFF_GMEM_OFFSET	0x100000

#define MEMBUF_SIZE	64

#define CFF_OP_WRITE_REG        0x00000002
struct cff_op_write_reg {
	unsigned char op;
	uint addr;
	uint value;
} __packed;

#define CFF_OP_POLL_REG         0x00000004
struct cff_op_poll_reg {
	unsigned char op;
	uint addr;
	uint value;
	uint mask;
} __packed;

#define CFF_OP_WAIT_IRQ         0x00000005
struct cff_op_wait_irq {
	unsigned char op;
} __packed;

#define CFF_OP_RMW              0x0000000a

#define CFF_OP_WRITE_MEM        0x0000000b
struct cff_op_write_mem {
	unsigned char op;
	uint addr;
	uint value;
} __packed;

#define CFF_OP_WRITE_MEMBUF     0x0000000c
struct cff_op_write_membuf {
	unsigned char op;
	uint addr;
	ushort count;
	uint buffer[MEMBUF_SIZE];
} __packed;

#define CFF_OP_MEMORY_BASE	0x0000000d
struct cff_op_memory_base {
	unsigned char op;
	uint base;
	uint size;
	uint gmemsize;
} __packed;

#define CFF_OP_HANG		0x0000000e
struct cff_op_hang {
	unsigned char op;
} __packed;

#define CFF_OP_EOF              0xffffffff
struct cff_op_eof {
	unsigned char op;
} __packed;

#define CFF_OP_VERIFY_MEM_FILE  0x00000007
#define CFF_OP_WRITE_SURFACE_PARAMS 0x00000011
struct cff_op_user_event {
	unsigned char op;
	unsigned int op1;
	unsigned int op2;
	unsigned int op3;
	unsigned int op4;
	unsigned int op5;
} __packed;


static void b64_encodeblock(unsigned char in[3], unsigned char out[4], int len)
{
	static const char tob64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmno"
		"pqrstuvwxyz0123456789+/";

	out[0] = tob64[in[0] >> 2];
	out[1] = tob64[((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4)];
	out[2] = (unsigned char) (len > 1 ? tob64[((in[1] & 0x0f) << 2)
		| ((in[2] & 0xc0) >> 6)] : '=');
	out[3] = (unsigned char) (len > 2 ? tob64[in[2] & 0x3f] : '=');
}

static void b64_encode(const unsigned char *in_buf, int in_size,
	unsigned char *out_buf, int out_bufsize, int *out_size)
{
	unsigned char in[3], out[4];
	int i, len;

	*out_size = 0;
	while (in_size > 0) {
		len = 0;
		for (i = 0; i < 3; ++i) {
			if (in_size-- > 0) {
				in[i] = *in_buf++;
				++len;
			} else
				in[i] = 0;
		}
		if (len) {
			b64_encodeblock(in, out, len);
			if (out_bufsize < 4) {
				pr_warn("kgsl: cffdump: %s: out of buffer\n",
					__func__);
				return;
			}
			for (i = 0; i < 4; ++i)
				*out_buf++ = out[i];
			*out_size += 4;
			out_bufsize -= 4;
		}
	}
}

#define KLOG_TMPBUF_SIZE (1024)
static void klog_printk(const char *fmt, ...)
{
	/* per-cpu klog formatting temporary buffer */
	static char klog_buf[NR_CPUS][KLOG_TMPBUF_SIZE];

	va_list args;
	int len;
	char *cbuf;
	unsigned long flags;

	local_irq_save(flags);
	cbuf = klog_buf[smp_processor_id()];
	va_start(args, fmt);
	len = vsnprintf(cbuf, KLOG_TMPBUF_SIZE, fmt, args);
	total_bytes += len;
	va_end(args);
	relay_write(chan, cbuf, len);
	local_irq_restore(flags);
}

static struct cff_op_write_membuf cff_op_write_membuf;
static void cffdump_membuf(int id, unsigned char *out_buf, int out_bufsize)
{
	void *data;
	int len, out_size;
	struct cff_op_write_mem cff_op_write_mem;

	uint addr = cff_op_write_membuf.addr
		- sizeof(uint)*cff_op_write_membuf.count;

	if (!cff_op_write_membuf.count) {
		pr_warn("kgsl: cffdump: membuf: count == 0, skipping");
		return;
	}

	if (cff_op_write_membuf.count != 1) {
		cff_op_write_membuf.op = CFF_OP_WRITE_MEMBUF;
		cff_op_write_membuf.addr = addr;
		len = sizeof(cff_op_write_membuf) -
			sizeof(uint)*(MEMBUF_SIZE - cff_op_write_membuf.count);
		data = &cff_op_write_membuf;
	} else {
		cff_op_write_mem.op = CFF_OP_WRITE_MEM;
		cff_op_write_mem.addr = addr;
		cff_op_write_mem.value = cff_op_write_membuf.buffer[0];
		data = &cff_op_write_mem;
		len = sizeof(cff_op_write_mem);
	}
	b64_encode(data, len, out_buf, out_bufsize, &out_size);
	out_buf[out_size] = 0;
	klog_printk("%ld:%d;%s\n", ++serial_nr, id, out_buf);
	cff_op_write_membuf.count = 0;
	cff_op_write_membuf.addr = 0;
}

static void cffdump_printline(int id, uint opcode, uint op1, uint op2,
	uint op3, uint op4, uint op5)
{
	struct cff_op_write_reg cff_op_write_reg;
	struct cff_op_poll_reg cff_op_poll_reg;
	struct cff_op_wait_irq cff_op_wait_irq;
	struct cff_op_memory_base cff_op_memory_base;
	struct cff_op_hang cff_op_hang;
	struct cff_op_eof cff_op_eof;
	struct cff_op_user_event cff_op_user_event;
	unsigned char out_buf[sizeof(cff_op_write_membuf)/3*4 + 16];
	void *data;
	int len = 0, out_size;
	long cur_secs;

	spin_lock(&cffdump_lock);
	if (opcode == CFF_OP_WRITE_MEM) {
		if ((cff_op_write_membuf.addr != op1 &&
			cff_op_write_membuf.count)
			|| (cff_op_write_membuf.count == MEMBUF_SIZE))
			cffdump_membuf(id, out_buf, sizeof(out_buf));

		cff_op_write_membuf.buffer[cff_op_write_membuf.count++] = op2;
		cff_op_write_membuf.addr = op1 + sizeof(uint);
		spin_unlock(&cffdump_lock);
		return;
	} else if (cff_op_write_membuf.count)
		cffdump_membuf(id, out_buf, sizeof(out_buf));
	spin_unlock(&cffdump_lock);

	switch (opcode) {
	case CFF_OP_WRITE_REG:
		cff_op_write_reg.op = opcode;
		cff_op_write_reg.addr = op1;
		cff_op_write_reg.value = op2;
		data = &cff_op_write_reg;
		len = sizeof(cff_op_write_reg);
		break;

	case CFF_OP_POLL_REG:
		cff_op_poll_reg.op = opcode;
		cff_op_poll_reg.addr = op1;
		cff_op_poll_reg.value = op2;
		cff_op_poll_reg.mask = op3;
		data = &cff_op_poll_reg;
		len = sizeof(cff_op_poll_reg);
		break;

	case CFF_OP_WAIT_IRQ:
		cff_op_wait_irq.op = opcode;
		data = &cff_op_wait_irq;
		len = sizeof(cff_op_wait_irq);
		break;

	case CFF_OP_MEMORY_BASE:
		cff_op_memory_base.op = opcode;
		cff_op_memory_base.base = op1;
		cff_op_memory_base.size = op2;
		cff_op_memory_base.gmemsize = op3;
		data = &cff_op_memory_base;
		len = sizeof(cff_op_memory_base);
		break;

	case CFF_OP_HANG:
		cff_op_hang.op = opcode;
		data = &cff_op_hang;
		len = sizeof(cff_op_hang);
		break;

	case CFF_OP_EOF:
		cff_op_eof.op = opcode;
		data = &cff_op_eof;
		len = sizeof(cff_op_eof);
		break;

	case CFF_OP_WRITE_SURFACE_PARAMS:
	case CFF_OP_VERIFY_MEM_FILE:
		cff_op_user_event.op = opcode;
		cff_op_user_event.op1 = op1;
		cff_op_user_event.op2 = op2;
		cff_op_user_event.op3 = op3;
		cff_op_user_event.op4 = op4;
		cff_op_user_event.op5 = op5;
		data = &cff_op_user_event;
		len = sizeof(cff_op_user_event);
		break;
	}

	if (len) {
		b64_encode(data, len, out_buf, sizeof(out_buf), &out_size);
		out_buf[out_size] = 0;
		klog_printk("%ld:%d;%s\n", ++serial_nr, id, out_buf);
	} else
		pr_warn("kgsl: cffdump: unhandled opcode: %d\n", opcode);

	cur_secs = get_seconds();
	if ((cur_secs - last_sec) > 10 || (last_sec - cur_secs) > 10) {
		pr_info("kgsl: cffdump: total [bytes:%lu kB, syncmem:%lu kB], "
			"seq#: %lu\n", total_bytes/1024, total_syncmem/1024,
			serial_nr);
		last_sec = cur_secs;
	}
}

void kgsl_cffdump_init()
{
	struct dentry *debugfs_dir = kgsl_get_debugfs_dir();

#ifdef ALIGN_CPU
	cpumask_t mask;

	cpumask_clear(&mask);
	cpumask_set_cpu(0, &mask);
	sched_setaffinity(0, &mask);
#endif
	if (!debugfs_dir || IS_ERR(debugfs_dir)) {
		KGSL_CORE_ERR("Debugfs directory is bad\n");
		return;
	}

	spin_lock_init(&cffdump_lock);

	dir = debugfs_create_dir("cff", debugfs_dir);
	if (!dir) {
		KGSL_CORE_ERR("debugfs_create_dir failed\n");
		return;
	}

	chan = create_channel(subbuf_size, n_subbufs);
}

void kgsl_cffdump_destroy()
{
	if (chan)
		relay_flush(chan);
	destroy_channel();
	if (dir)
		debugfs_remove(dir);
}

void kgsl_cffdump_open(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	if (!device->cff_dump_enable)
		return;

	/* Set the maximum possible address range */
	kgsl_cffdump_memory_base(device,
				adreno_dev->gmem_size + KGSL_CFF_GMEM_OFFSET,
				0xFFFFFFFF -
				(adreno_dev->gmem_size + KGSL_CFF_GMEM_OFFSET),
				adreno_dev->gmem_size);
}

void kgsl_cffdump_memory_base(struct kgsl_device *device, unsigned int base,
			      unsigned int range, unsigned gmemsize)
{
	if (!device->cff_dump_enable)
		return;
	cffdump_printline(device->id, CFF_OP_MEMORY_BASE, base,
			range, gmemsize, 0, 0);
}

void kgsl_cffdump_hang(struct kgsl_device *device)
{
	if (!device->cff_dump_enable)
		return;
	cffdump_printline(device->id, CFF_OP_HANG, 0, 0, 0, 0, 0);
}

void kgsl_cffdump_close(struct kgsl_device *device)
{
	if (!device->cff_dump_enable)
		return;
	cffdump_printline(device->id, CFF_OP_EOF, 0, 0, 0, 0, 0);
}


void kgsl_cffdump_user_event(struct kgsl_device *device,
		unsigned int cff_opcode, unsigned int op1,
		unsigned int op2, unsigned int op3,
		unsigned int op4, unsigned int op5)
{
	if (!device->cff_dump_enable)
		return;
	cffdump_printline(-1, cff_opcode, op1, op2, op3, op4, op5);
}

void kgsl_cffdump_syncmem(struct kgsl_device *device,
			  struct kgsl_memdesc *memdesc, uint gpuaddr,
			  size_t sizebytes, bool clean_cache)
{
	const void *src;

	if (!device->cff_dump_enable)
		return;

	BUG_ON(memdesc == NULL);

	total_syncmem += sizebytes;

	src = kgsl_gpuaddr_to_vaddr(memdesc, gpuaddr);
	if (memdesc->hostptr == NULL) {
		KGSL_CORE_ERR(
		"no kernel map for gpuaddr: 0x%08x, m->host: 0x%p, phys: %pa\n",
		gpuaddr, memdesc->hostptr, &memdesc->physaddr);
		return;
	}

	if (clean_cache) {
		/* Ensure that this memory region is not read from the
		 * cache but fetched fresh */

		mb();

		kgsl_cache_range_op((struct kgsl_memdesc *)memdesc,
				KGSL_CACHE_OP_INV);
	}

	while (sizebytes > 3) {
		cffdump_printline(-1, CFF_OP_WRITE_MEM, gpuaddr, *(uint *)src,
			0, 0, 0);
		gpuaddr += 4;
		src += 4;
		sizebytes -= 4;
	}
	if (sizebytes > 0)
		cffdump_printline(-1, CFF_OP_WRITE_MEM, gpuaddr, *(uint *)src,
			0, 0, 0);
	/* Unmap memory since kgsl_gpuaddr_to_vaddr was called */
	kgsl_memdesc_unmap(memdesc);
}

void kgsl_cffdump_setmem(struct kgsl_device *device,
			uint addr, uint value, uint sizebytes)
{
	if (!device || !device->cff_dump_enable)
		return;

	while (sizebytes > 3) {
		/* Use 32bit memory writes as long as there's at least
		 * 4 bytes left */
		cffdump_printline(-1, CFF_OP_WRITE_MEM, addr, value,
				0, 0, 0);
		addr += 4;
		sizebytes -= 4;
	}
	if (sizebytes > 0)
		cffdump_printline(-1, CFF_OP_WRITE_MEM, addr, value,
				0, 0, 0);
}

void kgsl_cffdump_regwrite(struct kgsl_device *device, uint addr,
	uint value)
{
	if (!device->cff_dump_enable)
		return;

	cffdump_printline(device->id, CFF_OP_WRITE_REG, addr, value,
			0, 0, 0);
}

void kgsl_cffdump_regpoll(struct kgsl_device *device, uint addr,
	uint value, uint mask)
{
	if (!device->cff_dump_enable)
		return;

	cffdump_printline(device->id, CFF_OP_POLL_REG, addr, value,
			mask, 0, 0);
}

void kgsl_cffdump_slavewrite(struct kgsl_device *device, uint addr, uint value)
{
	if (!device->cff_dump_enable)
		return;

	cffdump_printline(-1, CFF_OP_WRITE_REG, addr, value, 0, 0, 0);
}

int kgsl_cffdump_waitirq(struct kgsl_device *device)
{
	if (!device->cff_dump_enable)
		return 0;

	cffdump_printline(-1, CFF_OP_WAIT_IRQ, 0, 0, 0, 0, 0);

	return 1;
}
EXPORT_SYMBOL(kgsl_cffdump_waitirq);

static int subbuf_start_handler(struct rchan_buf *buf,
	void *subbuf, void *prev_subbuf, size_t prev_padding)
{
	pr_debug("kgsl: cffdump: subbuf_start_handler(subbuf=%p, prev_subbuf"
		"=%p, prev_padding=%08zx)\n", subbuf, prev_subbuf,
		 prev_padding);

	if (relay_buf_full(buf)) {
		if (!suspended) {
			suspended = 1;
			pr_warn("kgsl: cffdump: relay: cpu %d buffer full!!!\n",
				smp_processor_id());
		}
		dropped++;
		return 0;
	} else if (suspended) {
		suspended = 0;
		pr_warn("kgsl: cffdump: relay: cpu %d buffer no longer full.\n",
			smp_processor_id());
	}

	subbuf_start_reserve(buf, 0);
	return 1;
}

static struct dentry *create_buf_file_handler(const char *filename,
	struct dentry *parent, unsigned short mode, struct rchan_buf *buf,
	int *is_global)
{
	return debugfs_create_file(filename, mode, parent, buf,
				       &relay_file_operations);
}

/*
 * file_remove() default callback.  Removes relay file in debugfs.
 */
static int remove_buf_file_handler(struct dentry *dentry)
{
	pr_info("kgsl: cffdump: %s()\n", __func__);
	debugfs_remove(dentry);
	return 0;
}

/*
 * relay callbacks
 */
static struct rchan_callbacks relay_callbacks = {
	.subbuf_start = subbuf_start_handler,
	.create_buf_file = create_buf_file_handler,
	.remove_buf_file = remove_buf_file_handler,
};

/**
 *	create_channel - creates channel /debug/klog/cpuXXX
 *
 *	Creates channel along with associated produced/consumed control files
 *
 *	Returns channel on success, NULL otherwise
 */
static struct rchan *create_channel(unsigned subbuf_size, unsigned n_subbufs)
{
	struct rchan *chan;

	pr_info("kgsl: cffdump: relay: create_channel: subbuf_size %u, "
		"n_subbufs %u, dir 0x%p\n", subbuf_size, n_subbufs, dir);

	chan = relay_open("cpu", dir, subbuf_size,
			  n_subbufs, &relay_callbacks, NULL);
	if (!chan) {
		KGSL_CORE_ERR("relay_open failed\n");
		return NULL;
	}

	suspended = 0;
	dropped = 0;

	return chan;
}

/**
 *	destroy_channel - destroys channel /debug/kgsl/cff/cpuXXX
 *
 *	Destroys channel along with associated produced/consumed control files
 */
static void destroy_channel(void)
{
	pr_info("kgsl: cffdump: relay: destroy_channel\n");
	if (chan) {
		relay_close(chan);
		chan = NULL;
	}
}

int kgsl_cff_dump_enable_set(void *data, u64 val)
{
	int ret = 0;
	struct kgsl_device *device = (struct kgsl_device *)data;
	int i;

	mutex_lock(&kgsl_driver.devlock);
	if (val) {
		/* Check if CFF is on for some other device already */
		for (i = 0; i < KGSL_DEVICE_MAX; i++) {
			if (kgsl_driver.devp[i]) {
				struct kgsl_device *device_temp =
						kgsl_driver.devp[i];
				if (device_temp->cff_dump_enable &&
					device != device_temp) {
					KGSL_CORE_ERR(
					"CFF is on for another device %d\n",
					device_temp->id);
					ret = -EINVAL;
					goto done;
				}
			}
		}
		if (!device->cff_dump_enable) {
			device->cff_dump_enable = 1;
			/*
			 * put device to slumber so that we ensure that the
			 * start opcode in CFF is present
			 */
			kgsl_mutex_lock(&device->mutex, &device->mutex_owner);
			ret = kgsl_pwrctrl_slumber(device);
			if (ret)
				device->cff_dump_enable = 0;
			kgsl_mutex_unlock(&device->mutex, &device->mutex_owner);
		}
	} else if (device->cff_dump_enable && !val) {
		device->cff_dump_enable = 0;
	}
done:
	mutex_unlock(&kgsl_driver.devlock);
	return ret;
}
EXPORT_SYMBOL(kgsl_cff_dump_enable_set);

int kgsl_cff_dump_enable_get(void *data, u64 *val)
{
	struct kgsl_device *device = (struct kgsl_device *)data;
	*val = device->cff_dump_enable;
	return 0;
}
EXPORT_SYMBOL(kgsl_cff_dump_enable_get);

/*
 * kgsl_cffdump_capture_adreno_ib_cff() - Capture CFF for an IB
 * @device: Device for which CFF is to be captured
 * @ptbase: The pagetable in which the IB is mapped
 * @gpuaddr: Address of IB
 * @dwords: Size of the IB
 *
 * Dumps the CFF format of the IB including all objects in it like, IB2,
 * shaders, etc.
 *
 * Returns 0 on success else error code
 */
static int kgsl_cffdump_capture_adreno_ib_cff(struct kgsl_device *device,
				phys_addr_t ptbase,
				unsigned int gpuaddr, unsigned int dwords)
{
	int ret;
	struct adreno_ib_object_list *ib_obj_list;
	struct adreno_ib_object *ib_obj;
	int i;

	if (!device->cff_dump_enable)
		return 0;

	ret = adreno_ib_create_object_list(device, ptbase, gpuaddr, dwords,
		&ib_obj_list);

	if (ret) {
		KGSL_DRV_ERR(device,
		"Fail to create object list for IB %x, size(dwords) %x\n",
		gpuaddr, dwords);
		return ret;
	}

	for (i = 0; i < ib_obj_list->num_objs; i++) {
		ib_obj = &(ib_obj_list->obj_list[i]);
		kgsl_cffdump_syncmem(device, &(ib_obj->entry->memdesc),
					ib_obj->gpuaddr, ib_obj->size, false);
	}
	adreno_ib_destroy_obj_list(ib_obj_list);
	return 0;
}

/*
 * kgsl_cffdump_capture_ib_desc() - Capture CFF for a list of IB's
 * @device: Device for which CFF is to be captured
 * @context: The context under which the IB list executes on device
 * @ibdesc: The IB list
 * @numibs: Number of IB's in ibdesc
 *
 * Returns 0 on success else error code
 */
int kgsl_cffdump_capture_ib_desc(struct kgsl_device *device,
				struct kgsl_context *context,
				struct kgsl_ibdesc *ibdesc,
				unsigned int numibs)
{
	int ret = 0;
	unsigned int ptbase;
	int i;

	if (!device->cff_dump_enable)
		return 0;
	/* Dump CFF for IB and all objects in it */
	ptbase = kgsl_mmu_get_pt_base_addr(&device->mmu,
					context->proc_priv->pagetable);
	if (!ptbase) {
		ret = -EINVAL;
		goto done;
	}
	for (i = 0; i < numibs; i++) {
		ret = kgsl_cffdump_capture_adreno_ib_cff(
			device, ptbase, ibdesc[i].gpuaddr,
			ibdesc[i].sizedwords);
		if (ret) {
			KGSL_DRV_ERR(device,
			"Fail cff capture, IB %lx, size %zx\n",
			ibdesc[i].gpuaddr,
			ibdesc[i].sizedwords << 2);
			break;
		}
	}
done:
	return ret;
}
EXPORT_SYMBOL(kgsl_cffdump_capture_ib_desc);
