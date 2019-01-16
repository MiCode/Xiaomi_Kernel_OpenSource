
static void test_instr_NEON(int printlog)
{
	unsigned long copy_size;
	unsigned long flags;
	unsigned long i, j, avg, pld_dst;
	unsigned long temp;
	unsigned long result[10];
	copy_size = 256;
	/* copy_size = 1024*8; */
	/* copy_size = 1024*64; */
	/* copy_size = 1024*1024; */

	if (printlog == 1)
		pr_err("\n\n\r == Start test Pattern 1 ===\n\r");
	/* for(i = 0; i< 256 + 16 + 4; i++, copy_size += 256){ */
	i = 0;
	while (i < 256 + 16 + 4) {
		if (i < 256) {
			copy_size = 256 + i * 256;	/* inc 256 byte from 0~64 KBytes */
		} else if (i < 256 + 16) {
			copy_size = 1024 * 64 + (i - 256) * 1024 * 64;	/* inc 64Kbyte form 64KB~1MB */
		} else if (i < 256 + 16 + 4) {
			copy_size = 1024 * 1024 + (i - 256 - 16) * 1024 * 1024;	/* inc 1MB from 1MB~4MB */
		}
		i++;
		mdelay(5);
		preempt_disable();
		local_irq_save(flags);
		/* for(i = 0; i< 8; i++, copy_size += 1024*8){ */
		/* for(i = 0; i< 16; i++, copy_size += 1024*64){ */
		/* for(i = 0; i< 4; i++, copy_size += 1024*1024){ */
		inner_dcache_flush_all();
		if (printlog == 1)
			pr_err(" %lu :", copy_size);
		avg = 0;
		/* no pld */
		for (j = 0; j < 8; j++) {
			mdelay(3);
			/* if(flush_cache == 1) */
			inner_dcache_flush_all();
#if 1
			temp = 0;

			/* enable ARM CPU PMU */
			asm volatile ("mov %0, #0\n" "MRC p15, 0, %0, c9, c12, 0\n" "BIC %0, %0, #1 << 0\n"	/* disable */
				      "ORR %0, %0, #1 << 2\n"	/* reset cycle count */
				      "BIC %0, %0, #1 << 3\n"	/* count every clock cycle */
				      "MCR p15, 0, %0, c9, c12, 0\n":"+r" (temp)
 :  : "cc");
			asm volatile ("MRC p15, 0, %0, c9, c12, 0\n" "ORR %0, %0, #1 << 0\n"	/* enable */
				      "MCR p15, 0, %0, c9, c12, 0\n"
				      "MRC p15, 0, %0, c9, c12, 1\n"
				      "ORR %0, %0, #1 << 31\n"
				      "MCR p15, 0, %0, c9, c12, 1\n":"+r" (temp)
 :  : "cc");
#endif

			asm volatile ("push {r0,r1,r2,r9}\n"
				      "stmfd        sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
				      "mov r0, %0\n"
				      "mov r1, %1\n"
				      "mov r2, %2\n"
				      "mov r3, %3\n" "subs        r2, r2, #64\n" "1:\n"
				      /* The main loop copies 64 bytes at a time. */
				      ".word 0xf421020d\n"
				      /* "vld1.8      {d0  - d3},   [r1]!\n" */
				      ".word 0xf421420d\n"
				      /* "vld1.8      {d4  - d7},   [r1]!\n" */
				      /* ".word 0xf5d1f100\n" */
				      /* "pld         [r1, #(64*4)]\n" */
				      /* ".word 0xe2522040\n" */
				      "subs        r2, r2, #64\n" ".word 0xf400022d\n"
				      /* "vst1.8      {d0  - d3},   [r0, :128]!\n" */
				      ".word 0xf400422d\n"
				      /* "vst1.8      {d4  - d7},   [r0, :128]!\n" */
				      "bge         1b\n"
				      "ldmfd       sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
				      "pop {r0,r1,r2, r9}\n"
				      /* "pop r2\n" */
 :  : "r" (&buffer_dst), "r"(&buffer_src), "r"(copy_size),
				      "r"(pld_dst)
 : );

			/* #if defined(DEBUG_DRAMC_CALIB) */
#if 1
			/* get CPU cycle count from the ARM CPU PMU */
			asm volatile ("MRC p15, 0, %0, c9, c12, 0\n" "BIC %0, %0, #1 << 0\n"	/* disable */
				      "MCR p15, 0, %0, c9, c12, 0\n"
				      "MRC p15, 0, %0, c9, c13, 0\n":"+r" (temp)
 :  : "cc");
			result[j] = temp;
#endif
		}		/* 10 times loop */
		avg = 0;
		for (j = 0; j < 8; j++) {
			avg += result[j];
		}
		avg = avg >> 3;
		if (printlog == 1)
			pr_debug(" %lu ", avg);
		for (pld_dst = 0; pld_dst < 64 * 16; pld_dst += 64) {
			avg = 0;
			for (j = 0; j < 8; j++) {
				mdelay(3);
				if (flush_cache == 1)
					inner_dcache_flush_all();
#if 1
				temp = 0;

				/* enable ARM CPU PMU */
				asm volatile ("mov %0, #0\n" "MRC p15, 0, %0, c9, c12, 0\n" "BIC %0, %0, #1 << 0\n"	/* disable */
					      "ORR %0, %0, #1 << 2\n"	/* reset cycle count */
					      "BIC %0, %0, #1 << 3\n"	/* count every clock cycle */
					      "MCR p15, 0, %0, c9, c12, 0\n":"+r" (temp)
 :  : "cc");
				asm volatile ("MRC p15, 0, %0, c9, c12, 0\n" "ORR %0, %0, #1 << 0\n"	/* enable */
					      "MCR p15, 0, %0, c9, c12, 0\n"
					      "MRC p15, 0, %0, c9, c12, 1\n"
					      "ORR %0, %0, #1 << 31\n"
					      "MCR p15, 0, %0, c9, c12, 1\n":"+r" (temp)
 :  : "cc");
#endif

				asm volatile ("push {r0,r1,r2,r9}\n"
					      "stmfd        sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
					      "mov r0, %0\n"
					      "mov r1, %1\n"
					      "mov r2, %2\n"
					      "mov r3, %3\n" "subs        r2, r2, #64\n" "1:\n"
					      /* The main loop copies 64 bytes at a time. */
					      ".word 0xf421020d\n"
					      /* "vld1.8      {d0  - d3},   [r1]!\n" */
					      ".word 0xf421420d\n"
					      /* "vld1.8      {d4  - d7},   [r1]!\n" */
					      /* ".word 0xf5d1f100\n" */
					      /* "pld         [r1, r3]\n" */
					      "subs        r2, r2, #64\n" ".word 0xf400022d\n"
					      /* "vst1.8      {d0  - d3},   [r0, :128]!\n" */
					      ".word 0xf400422d\n"
					      /* "vst1.8      {d4  - d7},   [r0, :128]!\n" */
					      /* ".word 0xf590f100\n" //pldw */
					      "bge         1b\n"
					      "ldmfd       sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
					      "pop {r0,r1,r2, r9}\n"
					      /* "pop r2\n" */
 :  : "r" (&buffer_dst), "r"(&buffer_src), "r"(copy_size),
					      "r"(pld_dst) : );

				/* #if defined(DEBUG_DRAMC_CALIB) */
#if 1
				/* get CPU cycle count from the ARM CPU PMU */
				asm volatile ("MRC p15, 0, %0, c9, c12, 0\n" "BIC %0, %0, #1 << 0\n"	/* disable */
					      "MCR p15, 0, %0, c9, c12, 0\n"
					      "MRC p15, 0, %0, c9, c13, 0\n":"+r" (temp)
 :  : "cc");
				/* pr_err("%lu bytes takes %d CPU cycles\n\r", copy_size,temp); */
				/* pr_err(" %d ", temp); */
				/* pr_debug(" %d ", temp); */
				result[j] = temp;
				/* pr_debug(" result[%d]=%d :", j, result[j]); */
#endif
			}	/* 10 times loop */
			avg = 0;
			for (j = 0; j < 8; j++) {
				avg += result[j];
			}
			avg = avg >> 3;
			if (printlog == 1)
				pr_debug(" %d ", avg);
		}		/* pld dist loop */
		local_irq_restore(flags);
		preempt_enable();
	}
	if (printlog == 1)
		pr_err("\n\r ====NEON instruction test done ==== flush_cache:%d\n", flush_cache);
}

static void test_instr_only_pld(int printlog)
{
	unsigned long copy_size;
	unsigned long flags;
	int i, j, avg, pld_dst;
	int k;
	int temp;
	int result[10];
	copy_size = 256;
	/* copy_size = 1024*8; */
	/* copy_size = 1024*64; */
	/* copy_size = 1024*1024; */
	if (printlog == 1)
		pr_err("\n\n\r == Start test Pattern PLD only ===\n\r");
	/* for(i = 0; i< 256; i++, copy_size += 256){ */
	i = 0;
	while (i < 256 + 16 + 4) {
		if (i < 256) {
			copy_size = 256 + i * 256;	/* inc 256 byte from 0~64 KBytes */
		} else if (i < 256 + 16) {
			copy_size = 1024 * 64 + (i - 256) * 1024 * 64;	/* inc 64Kbyte form 64KB~1MB */
		} else if (i < 256 + 16 + 4) {
			copy_size = 1024 * 1024 + (i - 256 - 16) * 1024 * 1024;	/* inc 1MB from 1MB~4MB */
		}
		i++;
		mdelay(5);
		preempt_disable();
		local_irq_save(flags);
		/* for(i = 0; i< 64; i++, copy_size += 1024){ */
		/* for(i = 0; i< 8; i++, copy_size += 1024*8){ */
		/* for(i = 0; i< 16; i++, copy_size += 1024*64){ */
		/* for(i = 0; i< 4; i++, copy_size += 1024*1024){ */
		/* pr_err("\n\r %lu ",copy_size); */
		inner_dcache_flush_all();
		if (printlog == 1)
			pr_err(" %lu :", copy_size);
		avg = 0;
		for (j = 0; j < 8; j++) {
			mdelay(3);
			if (flush_cache == 1)
				inner_dcache_flush_all();
#if 1
			/* reset RA mode */
#if 0
			asm volatile ("MRC p15, 0, %0, c1, c0, 1\n" "ORR %0, %0, #1 << 11\n"	/* disable */
				      "ORR %0, %0, #1 << 12\n"	/* disable */
				      "MCR p15, 0, %0, c1, c0, 1\n" "MRC p15, 0, %0, c1, c0, 1\n" "BIC %0, %0, #1 << 11\n"	/* enable */
				      "BIC %0, %0, #1 << 12\n"	/* enable */
				      "MCR p15, 0, %0, c1, c0, 1\n":"+r" (temp)
 :  : "cc");
#endif
			/* enable ARM CPU PMU */
			asm volatile ("MRC p15, 0, %0, c9, c12, 0\n" "BIC %0, %0, #1 << 0\n"	/* disable */
				      "ORR %0, %0, #1 << 2\n"	/* reset cycle count */
				      "BIC %0, %0, #1 << 3\n"	/* count every clock cycle */
				      "MCR p15, 0, %0, c9, c12, 0\n":"+r" (temp)
 :  : "cc");
			asm volatile ("MRC p15, 0, %0, c9, c12, 0\n" "ORR %0, %0, #1 << 0\n"	/* enable */
				      "MCR p15, 0, %0, c9, c12, 0\n"
				      "MRC p15, 0, %0, c9, c12, 1\n"
				      "ORR %0, %0, #1 << 31\n"
				      "MCR p15, 0, %0, c9, c12, 1\n":"+r" (temp)
 :  : "cc");
#endif
			asm volatile ("push {r0,r1,r2,r9}\n"
				      "mov r0, %0\n"
				      "mov r1, %1\n"
				      "mov r2, %2\n"
				      "mov r9, %3\n"
				      "stmfd        sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n" "1:\n"
				      /* "pld         [r1, r9]\n" */
				      /* "pldw        [r0, r9]\n" */
				      /* ".word 0xf790f009\n" */
				      "ldmia       r1!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
				      "subs        r2, r2, #32 \n"
				      "stmia       r0!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
				      "blt 2f\n"
				      "ldmia       r1!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
				      "subs        r2, r2, #32 \n"
				      "stmia       r0!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
				      "bge 1b\n"
				      "2:\n"
				      "ldmfd       sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
				      "pop {r0,r1,r2,r9}\n" :  : "r" (&buffer_dst), "r"(&buffer_src),
				      "r"(copy_size), "r"(pld_dst)
 : );


			/* #if defined(DEBUG_DRAMC_CALIB) */
#if 1
			/* get CPU cycle count from the ARM CPU PMU */
			asm volatile ("MRC p15, 0, %0, c9, c12, 0\n" "BIC %0, %0, #1 << 0\n"	/* disable */
				      "MCR p15, 0, %0, c9, c12, 0\n"
				      "MRC p15, 0, %0, c9, c13, 0\n":"+r" (temp)
 :  : "cc");
#endif
			result[j] = temp;
			/* pr_debug(" %d ", temp); */
		}
		avg = 0;
		for (j = 0; j < 8; j++) {
			avg += result[j];
		}
		avg = avg >> 3;
		if (printlog == 1)
			pr_debug(" %d ", avg);
		for (pld_dst = 0; pld_dst < 64 * 16; pld_dst += 64) {
			for (j = 0; j < 8; j++) {
				mdelay(3);
				if (flush_cache == 1)
					inner_dcache_flush_all();
#if 1
#if 0
				/* reset RA mode */
				asm volatile ("MRC p15, 0, %0, c1, c0, 1\n" "ORR %0, %0, #1 << 11\n"	/* disable */
					      "ORR %0, %0, #1 << 12\n"	/* disable */
					      "MCR p15, 0, %0, c1, c0, 1\n" "MRC p15, 0, %0, c1, c0, 1\n" "BIC %0, %0, #1 << 11\n"	/* enable */
					      "BIC %0, %0, #1 << 12\n"	/* enable */
					      "MCR p15, 0, %0, c1, c0, 1\n":"+r" (temp)
 :  : "cc");
#endif
				/* enable ARM CPU PMU */
				asm volatile ("MRC p15, 0, %0, c9, c12, 0\n" "BIC %0, %0, #1 << 0\n"	/* disable */
					      "ORR %0, %0, #1 << 2\n"	/* reset cycle count */
					      "BIC %0, %0, #1 << 3\n"	/* count every clock cycle */
					      "MCR p15, 0, %0, c9, c12, 0\n":"+r" (temp)
 :  : "cc");
				asm volatile ("MRC p15, 0, %0, c9, c12, 0\n" "ORR %0, %0, #1 << 0\n"	/* enable */
					      "MCR p15, 0, %0, c9, c12, 0\n"
					      "MRC p15, 0, %0, c9, c12, 1\n"
					      "ORR %0, %0, #1 << 31\n"
					      "MCR p15, 0, %0, c9, c12, 1\n":"+r" (temp)
 :  : "cc");
#endif
				asm volatile ("push {r0,r1,r2,r9}\n"
					      "mov r0, %0\n"
					      "mov r1, %1\n"
					      "mov r2, %2\n"
					      "mov r9, %3\n"
					      "stmfd        sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
					      "1:\n" "pld         [r1, r9]\n"
					      /* "pldw        [r0, r9]\n" */
					      /* ".word 0xf790f009\n" */
					      "ldmia       r1!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
					      "subs        r2, r2, #32 \n"
					      "stmia       r0!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
					      "blt 2f\n"
					      "ldmia       r1!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
					      "subs        r2, r2, #32 \n"
					      "stmia       r0!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
					      "bge 1b\n"
					      "2:\n"
					      "ldmfd       sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
					      "pop {r0,r1,r2,r9}\n" :  : "r" (&buffer_dst),
					      "r"(&buffer_src), "r"(copy_size), "r"(pld_dst)
 : );


				/* #if defined(DEBUG_DRAMC_CALIB) */
#if 1
				/* get CPU cycle count from the ARM CPU PMU */
				asm volatile ("MRC p15, 0, %0, c9, c12, 0\n" "BIC %0, %0, #1 << 0\n"	/* disable */
					      "MCR p15, 0, %0, c9, c12, 0\n"
					      "MRC p15, 0, %0, c9, c13, 0\n":"+r" (temp)
 :  : "cc");
#endif
				result[j] = temp;
				/* pr_debug(" %d ", temp); */
			}
			avg = 0;
			for (j = 0; j < 8; j++) {
				avg += result[j];
			}
			avg = avg >> 3;
			if (printlog == 1)
				pr_debug(" %d ", avg);
		}
		local_irq_restore(flags);
		preempt_enable();
	}
	if (printlog == 1)
		pr_err("\n\r ==== test done only pld ==== flush_cache:%d\n", flush_cache);
}

static void test_instr_pld_pldw(int printlog)
{
	unsigned long copy_size;
	unsigned long flags;
	int i, j, avg, pld_dst;
	int k;
	int temp;
	int result[10];
	copy_size = 1024 * 64;
	if (printlog == 1)
		pr_err("\n\n\r == Start test pld+pldw ===\n\r");
	/* for(i = 0; i< 256; i++, copy_size += 256){ */
	/* for(i = 0; i< 4; i++, copy_size += 1024*1024){ */
	/* for(i = 0; i< 8; i++, copy_size += 1024*8){ */
	/* for(i = 0; i< 16; i++, copy_size += 1024*64){ */
	i = 0;
	while (i < 256 + 16 + 4) {
		if (i < 256) {
			copy_size = 256 + i * 256;	/* inc 256 byte from 0~64 KBytes */
		} else if (i < 256 + 16) {
			copy_size = 1024 * 64 + (i - 256) * 1024 * 64;	/* inc 64Kbyte form 64KB~1MB */
		} else if (i < 256 + 16 + 4) {
			copy_size = 1024 * 1024 + (i - 256 - 16) * 1024 * 1024;	/* inc 1MB from 1MB~4MB */
		}
		i++;
		mdelay(5);
		preempt_disable();
		local_irq_save(flags);
		/* pr_err("\n\r %lu ",copy_size); */
		inner_dcache_flush_all();
		if (printlog == 1)
			pr_err(" %lu :", copy_size);
		avg = 0;
		for (j = 0; j < 8; j++) {
			mdelay(3);
			if (flush_cache == 1)
				inner_dcache_flush_all();
#if 1
			/* reset RA mode */
#if 0
			asm volatile ("MRC p15, 0, %0, c1, c0, 1\n" "ORR %0, %0, #1 << 11\n"	/* disable */
				      "ORR %0, %0, #1 << 12\n"	/* disable */
				      "MCR p15, 0, %0, c1, c0, 1\n" "MRC p15, 0, %0, c1, c0, 1\n" "BIC %0, %0, #1 << 11\n"	/* enable */
				      "BIC %0, %0, #1 << 12\n"	/* enable */
				      "MCR p15, 0, %0, c1, c0, 1\n":"+r" (temp)
 :  : "cc");
#endif
			/* enable ARM CPU PMU */
			asm volatile ("MRC p15, 0, %0, c9, c12, 0\n" "BIC %0, %0, #1 << 0\n"	/* disable */
				      "ORR %0, %0, #1 << 2\n"	/* reset cycle count */
				      "BIC %0, %0, #1 << 3\n"	/* count every clock cycle */
				      "MCR p15, 0, %0, c9, c12, 0\n":"+r" (temp)
 :  : "cc");
			asm volatile ("MRC p15, 0, %0, c9, c12, 0\n" "ORR %0, %0, #1 << 0\n"	/* enable */
				      "MCR p15, 0, %0, c9, c12, 0\n"
				      "MRC p15, 0, %0, c9, c12, 1\n"
				      "ORR %0, %0, #1 << 31\n"
				      "MCR p15, 0, %0, c9, c12, 1\n":"+r" (temp)
 :  : "cc");
#endif
			asm volatile ("push {r0,r1,r2,r9}\n"
				      "mov r0, %0\n"
				      "mov r1, %1\n"
				      "mov r2, %2\n"
				      "mov r9, %3\n"
				      "stmfd        sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n" "1:\n"
				      /* "pld         [r1, r9]\n" */
				      /* "pldw        [r0, r9]\n" */
				      /* ".word 0xf790f009\n" */
				      "ldmia       r1!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
				      "subs        r2, r2, #32 \n"
				      "stmia       r0!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
				      "blt 2f\n"
				      "ldmia       r1!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
				      "subs        r2, r2, #32 \n"
				      "stmia       r0!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
				      "bge 1b\n"
				      "2:\n"
				      "ldmfd       sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
				      "pop {r0,r1,r2,r9}\n" :  : "r" (&buffer_dst), "r"(&buffer_src),
				      "r"(copy_size), "r"(pld_dst)
 : );


			/* #if defined(DEBUG_DRAMC_CALIB) */
#if 1
			/* get CPU cycle count from the ARM CPU PMU */
			asm volatile ("MRC p15, 0, %0, c9, c12, 0\n" "BIC %0, %0, #1 << 0\n"	/* disable */
				      "MCR p15, 0, %0, c9, c12, 0\n"
				      "MRC p15, 0, %0, c9, c13, 0\n":"+r" (temp)
 :  : "cc");
#endif
			result[j] = temp;
			/* pr_debug(" %d ", temp); */
		}
		avg = 0;
		for (j = 0; j < 8; j++) {
			avg += result[j];
		}
		avg = avg >> 3;
		if (printlog == 1)
			pr_debug(" %d ", avg);
		for (pld_dst = 0; pld_dst < 64 * 16; pld_dst += 64) {
			/* for(pld_dst = 0; pld_dst<64*16*3; pld_dst+=64*3){ */
			for (j = 0; j < 8; j++) {
				mdelay(3);
				if (flush_cache == 1)
					inner_dcache_flush_all();
#if 1
#if 0
				/* reset RA mode */
				asm volatile ("MRC p15, 0, %0, c1, c0, 1\n" "ORR %0, %0, #1 << 11\n"	/* disable */
					      "ORR %0, %0, #1 << 12\n"	/* disable */
					      "MCR p15, 0, %0, c1, c0, 1\n" "MRC p15, 0, %0, c1, c0, 1\n" "BIC %0, %0, #1 << 11\n"	/* enable */
					      "BIC %0, %0, #1 << 12\n"	/* enable */
					      "MCR p15, 0, %0, c1, c0, 1\n":"+r" (temp)
 :  : "cc");
#endif
				/* enable ARM CPU PMU */
				asm volatile ("MRC p15, 0, %0, c9, c12, 0\n" "BIC %0, %0, #1 << 0\n"	/* disable */
					      "ORR %0, %0, #1 << 2\n"	/* reset cycle count */
					      "BIC %0, %0, #1 << 3\n"	/* count every clock cycle */
					      "MCR p15, 0, %0, c9, c12, 0\n":"+r" (temp)
 :  : "cc");
				asm volatile ("MRC p15, 0, %0, c9, c12, 0\n" "ORR %0, %0, #1 << 0\n"	/* enable */
					      "MCR p15, 0, %0, c9, c12, 0\n"
					      "MRC p15, 0, %0, c9, c12, 1\n"
					      "ORR %0, %0, #1 << 31\n"
					      "MCR p15, 0, %0, c9, c12, 1\n":"+r" (temp)
 :  : "cc");
#endif
				asm volatile ("push {r0,r1,r2,r9}\n"
					      "mov r0, %0\n"
					      "mov r1, %1\n"
					      "mov r2, %2\n"
					      "mov r9, %3\n"
					      "stmfd        sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
					      "1:\n" "pld         [r1, r9]\n"
					      /* "pldw        [r0, r9]\n" */
					      ".word 0xf790f009\n"
					      "ldmia       r1!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
					      "subs        r2, r2, #32 \n"
					      "stmia       r0!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
					      "blt 2f\n"
					      "ldmia       r1!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
					      "subs        r2, r2, #32 \n"
					      "stmia       r0!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
					      "bge 1b\n" "2:\n"
					      "ldmfd       sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
					      "pop {r0,r1,r2,r9}\n" :  : "r" (&buffer_dst),
					      "r"(&buffer_src), "r"(copy_size), "r"(pld_dst)
 : );


				/* #if defined(DEBUG_DRAMC_CALIB) */
#if 1
				/* get CPU cycle count from the ARM CPU PMU */
				asm volatile ("MRC p15, 0, %0, c9, c12, 0\n" "BIC %0, %0, #1 << 0\n"	/* disable */
					      "MCR p15, 0, %0, c9, c12, 0\n"
					      "MRC p15, 0, %0, c9, c13, 0\n":"+r" (temp)
 :  : "cc");
#endif
				result[j] = temp;
				/* pr_debug(" %d ", temp); */
			}
			avg = 0;
			for (j = 0; j < 8; j++) {
				avg += result[j];
			}
			avg = avg >> 3;
			if (printlog == 1)
				pr_debug(" %d ", avg);
		}
		local_irq_restore(flags);
		preempt_enable();
	}
	if (printlog == 1)
		pr_err("\n\r ==== test done pld+pldw==== flush_cache:%d\n", flush_cache);
}
