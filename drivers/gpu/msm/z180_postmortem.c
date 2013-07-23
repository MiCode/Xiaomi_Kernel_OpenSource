/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include "kgsl.h"
#include "kgsl_device.h"
#include "z180.h"
#include "z180_reg.h"

#define Z180_STREAM_PACKET_CALL 0x7C000275

/* Postmortem Dump formatted Output parameters */

/* Number of Words per dump data line */
#define WORDS_PER_LINE 8

/* Number of spaces per dump data line */
#define NUM_SPACES (WORDS_PER_LINE - 1)

/*
 * Output dump data is formatted as string, hence number of chars
 * per line for line string allocation
 */
#define CHARS_PER_LINE  \
	((WORDS_PER_LINE * (2*sizeof(unsigned int))) + NUM_SPACES + 1)

/* Z180 registers (byte offsets) to be dumped */
static const unsigned int regs_to_dump[] = {
		ADDR_VGC_VERSION,
		ADDR_VGC_SYSSTATUS,
		ADDR_VGC_IRQSTATUS,
		ADDR_VGC_IRQENABLE,
		ADDR_VGC_IRQ_ACTIVE_CNT,
		ADDR_VGC_CLOCKEN,
		ADDR_VGC_MH_DATA_ADDR,
		ADDR_VGC_GPR0,
		ADDR_VGC_GPR1,
		ADDR_VGC_BUSYCNT,
		ADDR_VGC_FIFOFREE,
};

/**
 * z180_dump_regs - Dumps all of Z180 external registers. Prints the word offset
 * of the register in each output line.
 * @device: kgsl_device pointer to the Z180 core
 */
static void z180_dump_regs(struct kgsl_device *device)
{
	unsigned int i;
	unsigned int reg_val;

	z180_idle(device);

	KGSL_LOG_DUMP(device, "Z180 Register Dump\n");
	for (i = 0; i < ARRAY_SIZE(regs_to_dump); i++) {
		kgsl_regread(device,
				regs_to_dump[i]/sizeof(unsigned int), &reg_val);
		KGSL_LOG_DUMP(device, "REG: %04X: %08X\n",
				regs_to_dump[i]/sizeof(unsigned int), reg_val);
	}
}

/**
 * z180_dump_ringbuffer - Dumps the Z180 core's ringbuffer contents
 * @device: kgsl_device pointer to the z180 core
 */
static void z180_dump_ringbuffer(struct kgsl_device *device)
{
	unsigned int rb_size;
	unsigned int *rb_hostptr;
	unsigned int rb_words;
	unsigned int rb_gpuaddr;
	struct z180_device *z180_dev = Z180_DEVICE(device);
	unsigned int i;
	char linebuf[CHARS_PER_LINE];

	KGSL_LOG_DUMP(device, "Z180 ringbuffer dump\n");

	rb_hostptr = (unsigned int *) z180_dev->ringbuffer.cmdbufdesc.hostptr;

	rb_size = Z180_RB_SIZE;
	rb_gpuaddr = z180_dev->ringbuffer.cmdbufdesc.gpuaddr;

	rb_words = rb_size/sizeof(unsigned int);

	KGSL_LOG_DUMP(device, "ringbuffer size: %u\n", rb_size);

	KGSL_LOG_DUMP(device, "rb_words: %d\n", rb_words);

	for (i = 0; i < rb_words; i += WORDS_PER_LINE) {
		hex_dump_to_buffer(rb_hostptr+i,
				rb_size - i*sizeof(unsigned int),
				WORDS_PER_LINE*sizeof(unsigned int),
				sizeof(unsigned int), linebuf,
				sizeof(linebuf), false);
		KGSL_LOG_DUMP(device, "RB: %04X: %s\n",
				rb_gpuaddr + i*sizeof(unsigned int), linebuf);
	}
}


static void z180_dump_ib(struct kgsl_device *device)
{
	unsigned int rb_size;
	unsigned int *rb_hostptr;
	unsigned int rb_words;
	unsigned int rb_gpuaddr;
	unsigned int ib_gpuptr = 0;
	unsigned int ib_size = 0;
	void *ib_hostptr = NULL;
	int rb_slot_num = -1;
	struct z180_device *z180_dev = Z180_DEVICE(device);
	struct kgsl_mem_entry *entry = NULL;
	phys_addr_t pt_base;
	unsigned int i;
	unsigned int j;
	char linebuf[CHARS_PER_LINE];
	unsigned int current_ib_slot;
	unsigned int len;
	unsigned int rowsize;
	KGSL_LOG_DUMP(device, "Z180 IB dump\n");

	rb_hostptr = (unsigned int *) z180_dev->ringbuffer.cmdbufdesc.hostptr;

	rb_size = Z180_RB_SIZE;
	rb_gpuaddr = z180_dev->ringbuffer.cmdbufdesc.gpuaddr;

	rb_words = rb_size/sizeof(unsigned int);

	KGSL_LOG_DUMP(device, "Ringbuffer size (bytes): %u\n", rb_size);

	KGSL_LOG_DUMP(device, "rb_words: %d\n", rb_words);

	pt_base = kgsl_mmu_get_current_ptbase(&device->mmu);

	/* Dump the current IB */
	for (i = 0; i < rb_words; i++) {
		if (rb_hostptr[i] == Z180_STREAM_PACKET_CALL) {

			rb_slot_num++;
			current_ib_slot =
				z180_dev->current_timestamp % Z180_PACKET_COUNT;
			if (rb_slot_num != current_ib_slot)
				continue;

			ib_gpuptr = rb_hostptr[i+1];

			entry = kgsl_get_mem_entry(device, pt_base, ib_gpuptr,
							1);

			if (entry == NULL) {
				KGSL_LOG_DUMP(device,
				"IB mem entry not found for ringbuffer slot#: %d\n",
				rb_slot_num);
				continue;
			}

			ib_hostptr = kgsl_memdesc_map(&entry->memdesc);

			if (ib_hostptr == NULL) {
				KGSL_LOG_DUMP(device,
				"Could not map IB to kernel memory, Ringbuffer Slot: %d\n",
				rb_slot_num);
				kgsl_mem_entry_put(entry);
				continue;
			}

			ib_size = entry->memdesc.size;
			KGSL_LOG_DUMP(device,
				"IB size: %dbytes, IB size in words: %d\n",
				ib_size,
				ib_size/sizeof(unsigned int));

			for (j = 0; j < ib_size; j += WORDS_PER_LINE) {
				len = ib_size - j*sizeof(unsigned int);
				rowsize = WORDS_PER_LINE*sizeof(unsigned int);
				hex_dump_to_buffer(ib_hostptr+j, len, rowsize,
						sizeof(unsigned int), linebuf,
						sizeof(linebuf), false);
				KGSL_LOG_DUMP(device, "IB%d: %04X: %s\n",
						rb_slot_num,
						(rb_gpuaddr +
						j*sizeof(unsigned int)),
						linebuf);
			}
			KGSL_LOG_DUMP(device, "IB Dump Finished\n");
			kgsl_mem_entry_put(entry);
		}
	}
}


/**
 * z180_dump - Dumps the Z180 ringbuffer and registers (and IBs if asked for)
 * for postmortem
 * analysis.
 * @device: kgsl_device pointer to the Z180 core
 */
int z180_dump(struct kgsl_device *device, int manual)
{
	struct z180_device *z180_dev = Z180_DEVICE(device);

	mb();

	KGSL_LOG_DUMP(device, "Retired Timestamp: %d\n", z180_dev->timestamp);
	KGSL_LOG_DUMP(device,
			"Current Timestamp: %d\n", z180_dev->current_timestamp);

	/* Dump ringbuffer */
	z180_dump_ringbuffer(device);

	/* Dump registers */
	z180_dump_regs(device);

	/* Dump IBs, if asked for */
	if (device->pm_ib_enabled)
		z180_dump_ib(device);

	/* Get the stack trace if the dump was automatic */
	if (!manual)
		BUG_ON(1);

	return 0;
}

