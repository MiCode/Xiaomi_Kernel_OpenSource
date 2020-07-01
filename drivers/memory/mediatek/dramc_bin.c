// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/vmalloc.h>

#define MEM_TEST_SIZE	0x2000
#define PATTERN1	0x5A5A5A5A
#define PATTERN2	0xA5A5A5A5

int mtk_dramc_binning_test(void)
{
	unsigned char *mem8_base;
	unsigned short *mem16_base;
	unsigned int *mem32_base;
	unsigned int *mem_base;
	unsigned long mem_ptr;
	unsigned char pattern8;
	unsigned short pattern16;
	unsigned int i, j, size, pattern32;
	unsigned int value;
	unsigned int len = MEM_TEST_SIZE;
	void *ptr;
	int ret = 1;

	ptr = vmalloc(len);

	if (!ptr) {
		ret = -24;
		goto fail;
	}

	mem8_base = (unsigned char *)ptr;
	mem16_base = (unsigned short *)ptr;
	mem32_base = (unsigned int *)ptr;
	mem_base = (unsigned int *)ptr;
	/* pr_info("Test DRAM start address 0x%lx\n", (unsigned long)ptr); */
	pr_info("Test DRAM start address %p\n", ptr);
	pr_info("Test DRAM SIZE 0x%x\n", len);
	size = len >> 2;

	/* === test the tied bits (tied high) === */
	for (i = 0; i < size; i++)
		mem32_base[i] = 0;

	for (i = 0; i < size; i++) {
		if (mem32_base[i] != 0) {
			/* return -1; */
			ret = -1;
			goto fail;
		} else
			mem32_base[i] = 0xffffffff;
	}

	/* === test the tied bits (tied low) === */
	for (i = 0; i < size; i++) {
		if (mem32_base[i] != 0xffffffff) {
			/* return -2; */
			ret = -2;
			goto fail;
		} else
			mem32_base[i] = 0x00;
	}

	/* === test pattern 1 (0x00~0xff) === */
	pattern8 = 0x00;
	for (i = 0; i < len; i++)
		mem8_base[i] = pattern8++;
	pattern8 = 0x00;
	for (i = 0; i < len; i++) {
		if (mem8_base[i] != pattern8++) {
			/* return -3; */
			ret = -3;
			goto fail;
		}
	}

	/* === test pattern 2 (0x00~0xff) === */
	pattern8 = 0x00;
	for (i = j = 0; i < len; i += 2, j++) {
		if (mem8_base[i] == pattern8)
			mem16_base[j] = pattern8;
		if (mem16_base[j] != pattern8) {
			/* return -4; */
			ret = -4;
			goto fail;
		}
		pattern8 += 2;
	}

	/* === test pattern 3 (0x00~0xffff) === */
	pattern16 = 0x00;
	for (i = 0; i < (len >> 1); i++)
		mem16_base[i] = pattern16++;
	pattern16 = 0x00;
	for (i = 0; i < (len >> 1); i++) {
		if (mem16_base[i] != pattern16++) {
			/* return -5; */
			ret = -5;
			goto fail;
		}
	}

	/* === test pattern 4 (0x00~0xffffffff) === */
	pattern32 = 0x00;
	for (i = 0; i < (len >> 2); i++)
		mem32_base[i] = pattern32++;
	pattern32 = 0x00;
	for (i = 0; i < (len >> 2); i++) {
		if (mem32_base[i] != pattern32++) {
			/* return -6; */
			ret = -6;
			goto fail;
		}
	}

	/* === Pattern 5: Filling memory range with 0x44332211 === */
	for (i = 0; i < size; i++)
		mem32_base[i] = 0x44332211;

	/* === Read Check then Fill Memory with a5a5a5a5 Pattern === */
	for (i = 0; i < size; i++) {
		if (mem32_base[i] != 0x44332211) {
			/* return -7; */
			ret = -7;
			goto fail;
		} else {
			mem32_base[i] = 0xa5a5a5a5;
		}
	}

	/* === Read Check then Fill Memory with */
	/* 00 Byte Pattern at offset 0h === */
	for (i = 0; i < size; i++) {
		if (mem32_base[i] != 0xa5a5a5a5) {
			/* return -8; */
			ret = -8;
			goto fail;
		} else {
			mem8_base[i * 4] = 0x00;
		}
	}

	/* === Read Check then Fill Memory with */
	/* 00 Byte Pattern at offset 2h === */
	for (i = 0; i < size; i++) {
		if (mem32_base[i] != 0xa5a5a500) {
			/* return -9; */
			ret = -9;
			goto fail;
		} else {
			mem8_base[i * 4 + 2] = 0x00;
		}
	}

	/* === Read Check then Fill Memory with */
	/* 00 Byte Pattern at offset 1h === */
	for (i = 0; i < size; i++) {
		if (mem32_base[i] != 0xa500a500) {
			/* return -10; */
			ret = -10;
			goto fail;
		} else {
			mem8_base[i * 4 + 1] = 0x00;
		}
	}

	/* === Read Check then Fill Memory with */
	/* 00 Byte Pattern at offset 3h === */
	for (i = 0; i < size; i++) {
		if (mem32_base[i] != 0xa5000000) {
			/* return -11; */
			ret = -11;
			goto fail;
		} else {
			mem8_base[i * 4 + 3] = 0x00;
		}
	}

	/* === Read Check then Fill Memory with ffff */
	/* Word Pattern at offset 1h == */
	for (i = 0; i < size; i++) {
		if (mem32_base[i] != 0x00000000) {
			/* return -12; */
			ret = -12;
			goto fail;
		} else {
			mem16_base[i * 2 + 1] = 0xffff;
		}
	}

	/* === Read Check then Fill Memory with ffff */
	/* Word Pattern at offset 0h == */
	for (i = 0; i < size; i++) {
		if (mem32_base[i] != 0xffff0000) {
			/* return -13; */
			ret = -13;
			goto fail;
		} else {
			mem16_base[i * 2] = 0xffff;
		}
	}
    /*===  Read Check === */
	for (i = 0; i < size; i++) {
		if (mem32_base[i] != 0xffffffff) {
			/* return -14; */
			ret = -14;
			goto fail;
		}
	}

    /************************************************
     * Additional verification
     ************************************************/
	/* === stage 1 => write 0 === */

	for (i = 0; i < size; i++)
		mem_base[i] = PATTERN1;

	/* === stage 2 => read 0, write 0xF === */
	for (i = 0; i < size; i++) {
		value = mem_base[i];

		if (value != PATTERN1) {
			/* return -15; */
			ret = -15;
			goto fail;
		}
		mem_base[i] = PATTERN2;
	}

	/* === stage 3 => read 0xF, write 0 === */
	for (i = 0; i < size; i++) {
		value = mem_base[i];
		if (value != PATTERN2) {
			/* return -16; */
			ret = -16;
			goto fail;
		}
		mem_base[i] = PATTERN1;
	}

	/* === stage 4 => read 0, write 0xF === */
	for (i = 0; i < size; i++) {
		value = mem_base[i];
		if (value != PATTERN1) {
			/* return -17; */
			ret = -17;
			goto fail;
		}
		mem_base[i] = PATTERN2;
	}

	/* === stage 5 => read 0xF, write 0 === */
	for (i = 0; i < size; i++) {
		value = mem_base[i];
		if (value != PATTERN2) {
			/* return -18; */
			ret = -18;
			goto fail;
		}
		mem_base[i] = PATTERN1;
	}

	/* === stage 6 => read 0 === */
	for (i = 0; i < size; i++) {
		value = mem_base[i];
		if (value != PATTERN1) {
			/* return -19; */
			ret = -19;
			goto fail;
		}
	}

	/* === 1/2/4-byte combination test === */
	mem_ptr = (unsigned long)mem_base;
	while (mem_ptr < ((unsigned long)mem_base + (size << 2))) {
		*((unsigned char *)mem_ptr) = 0x78;
		mem_ptr += 1;
		*((unsigned char *)mem_ptr) = 0x56;
		mem_ptr += 1;
		*((unsigned short *)mem_ptr) = 0x1234;
		mem_ptr += 2;
		*((unsigned int *)mem_ptr) = 0x12345678;
		mem_ptr += 4;
		*((unsigned short *)mem_ptr) = 0x5678;
		mem_ptr += 2;
		*((unsigned char *)mem_ptr) = 0x34;
		mem_ptr += 1;
		*((unsigned char *)mem_ptr) = 0x12;
		mem_ptr += 1;
		*((unsigned int *)mem_ptr) = 0x12345678;
		mem_ptr += 4;
		*((unsigned char *)mem_ptr) = 0x78;
		mem_ptr += 1;
		*((unsigned char *)mem_ptr) = 0x56;
		mem_ptr += 1;
		*((unsigned short *)mem_ptr) = 0x1234;
		mem_ptr += 2;
		*((unsigned int *)mem_ptr) = 0x12345678;
		mem_ptr += 4;
		*((unsigned short *)mem_ptr) = 0x5678;
		mem_ptr += 2;
		*((unsigned char *)mem_ptr) = 0x34;
		mem_ptr += 1;
		*((unsigned char *)mem_ptr) = 0x12;
		mem_ptr += 1;
		*((unsigned int *)mem_ptr) = 0x12345678;
		mem_ptr += 4;
	}
	for (i = 0; i < size; i++) {
		value = mem_base[i];
		if (value != 0x12345678) {
			/* return -20; */
			ret = -20;
			goto fail;
		}
	}

	/* === Verify pattern 1 (0x00~0xff) === */
	pattern8 = 0x00;
	mem8_base[0] = pattern8;
	for (i = 0; i < size * 4; i++) {
		unsigned char waddr8, raddr8;

		waddr8 = i + 1;
		raddr8 = i;
		if (i < size * 4 - 1)
			mem8_base[waddr8] = pattern8 + 1;
		if (mem8_base[raddr8] != pattern8) {
			/* return -21; */
			ret = -21;
			goto fail;
		}
		pattern8++;
	}

	/* === Verify pattern 2 (0x00~0xffff) === */
	pattern16 = 0x00;
	mem16_base[0] = pattern16;
	for (i = 0; i < size * 2; i++) {
		if (i < size * 2 - 1)
			mem16_base[i + 1] = pattern16 + 1;
		if (mem16_base[i] != pattern16) {
			/* return -22; */
			ret = -22;
			goto fail;
		}
		pattern16++;
	}
	/* === Verify pattern 3 (0x00~0xffffffff) === */
	pattern32 = 0x00;
	mem32_base[0] = pattern32;
	for (i = 0; i < size; i++) {
		if (i < size - 1)
			mem32_base[i + 1] = pattern32 + 1;
		if (mem32_base[i] != pattern32) {
			/* return -23; */
			ret = -23;
			goto fail;
		}
		pattern32++;
	}
	pr_info("complex R/W mem test pass\n");

fail:
	vfree(ptr);
	return ret;
}

MODULE_AUTHOR("Mediatek Corporation");
MODULE_DESCRIPTION("DRAMC BINNING Test");
MODULE_LICENSE("GPL v2");
