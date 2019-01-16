
static void test_instr_NEON(int printlog)
{
	unsigned long long copy_size;
	unsigned long flags;
	long i, j, avg, pld_dst;
    unsigned long long t1, t2, t_avg;
	unsigned long temp;
	unsigned long result[10];
	unsigned long long t_result[10];
    int best_pld[512];
    char output_line[380];
    char buf[32];

    char *src, *dst;
	copy_size = 256;
	/* copy_size = 1024*8; */
	/* copy_size = 1024*64; */
	/* copy_size = 1024*1024; */

#if 1
    src = (char*)((unsigned long)buffer_src+ 0x10 & ~0xf);
    dst = (char*)((unsigned long)buffer_dst+ 0x10 & ~0xf);
#else
    src = (char*)((unsigned long)buffer_src+ 0x8 & ~0x7);
    dst = (char*)((unsigned long)buffer_dst+ 0x8 & ~0x7);
#endif
	if (printlog == 1)
		printk(KERN_EMERG"\n\n\r == ARM64 memcpy === \n 0x%lx-->0x%lx\n 0x%lx-->0x%lx\n\r", (unsigned long)buffer_src, (unsigned long)buffer_dst, (unsigned long)src, (unsigned long)dst);
	/* for(i = 0; i< 256 + 16 + 4; i++, copy_size += 256){ */
	i = 0;
	while (i < 256 + 16 + 4) {
        int line_offset = 0;
		if (i < 256) {
			copy_size = 256 + i * 256;	/* inc 256 byte from 0~64 KBytes */
		} else if (i < 256 + 16) {
			copy_size = 1024 * 64 + (i - 256) * 1024 * 64;	/* inc 64Kbyte form 64KB~1MB */
		} else if (i < 256 + 16 + 4) {
			copy_size = 1024 * 1024 + (i - 256 - 16) * 1024 * 1024;	/* inc 1MB from 1MB~4MB */
		}
		i++;
		//mdelay(5);
		preempt_disable();
		local_irq_save(flags);
		/* for(i = 0; i< 8; i++, copy_size += 1024*8){ */
		/* for(i = 0; i< 16; i++, copy_size += 1024*64){ */
		/* for(i = 0; i< 4; i++, copy_size += 1024*1024){ */
		inner_dcache_flush_all();
        if(printlog == 1){
            memset(buf, 0, 32);
            sprintf(buf," \n%llu :",copy_size);
            snprintf(output_line+line_offset, 320 - line_offset, "%s",buf);
            line_offset += strlen(buf);
        }

        /* cached */
        for(j= 0; j< copy_size; j++)
            dst[j]=src[j];

		avg = 0;
		/* no pld */
		for (j = 0; j < 8; j++) {
			mdelay(3);
			if(flush_cache == 1)
			    inner_dcache_flush_all();

            t1 = sched_clock();
#if 0
            asm volatile(
                    "mov x0, %0\n"
                    "mov x1, %1\n"
                    "mov x2, %2\n"
                    "mov x3, %3\n"

                    "subs    x2, x2, #128\n"
                    /* There are at least 128 bytes to copy.  */
                    "ldp x7, x8, [x1, #0]\n"
                    "sub x0, x0, #16 \n"      /* Pre-bias.  */
                    "ldp x9, x10, [x1, #16]\n"
                    "ldp x11, x12, [x1, #32]\n"
                    "ldp x13, x14, [x1, #48]!\n"   /* src += 64 - Pre-bias.  */

                    "1:\n"
                    "stp x7, x8, [x0, #16]\n"
                    "ldp x7, x8, [x1, #16]\n"
                    "stp x9, x10, [x0, #32]\n"
                    "ldp x9, x10, [x1, #32]\n"
                    "stp x11, x12, [x0, #48]\n"
                    "ldp x11, x12, [x1, #48]\n"
                    "stp x13, x14, [x0, #64]!\n"
                    "ldp x13, x14, [x1, #64]!\n"
                    "subs x2, x2, #64\n"
                    "b.ge    1b\n"
                    "stp x7, x8, [x0, #16]\n"
                    "stp x9, x10, [x0, #32]\n"
                    "stp x11, x12, [x0, #48]\n"
                    "stp x13, x14, [x0, #64]!\n"

                    :  : "r" (dst), "r"(src), "r"(copy_size),"r"(pld_dst)
                    : "x0", "x1", "x2", "x3", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14");
#else
            asm volatile(
                    "mov x0, %0\n"
                    "mov x1, %1\n"
                    "mov x2, %2\n"
                    "mov x3, %3\n"

                    "subs    x2, x2, #128\n"
                    /* There are at least 128 bytes to copy.  */
                    "ldur q1, [x1, #0]\n"
                    "sub x0, x0, #16 \n"      /* Pre-bias.  */
                    "ldur q2, [x1, #16]\n"
                    "ldur q3, [x1, #32]\n"
                    "ldur q4, [x1, #48]\n"
                    "add x1, x1, #48\n"

                    "1:\n"
                    "stur q1, [x0, #16]\n"
                    "ldur q1, [x1, #16]\n"
                    "stur q2, [x0, #32]\n"
                    "ldur q2, [x1, #32]\n"
                    "stur q3, [x0, #48]\n"
                    "ldur q3, [x1, #48]\n"
                    "stur q4, [x0, #64]\n"
                    "add x0, x0, #64\n"
                    "ldur q4, [x1, #64]\n"
                    "add x1, x1, #64\n"
                    "subs x2, x2, #64\n"
                    "b.ge    1b\n"
                    "stur q1, [x0, #16]\n"
                    "stur q2, [x0, #32]\n"
                    "stur q3, [x0, #48]\n"
                    "stur q4, [x0, #64]\n"

                    :  : "r" (dst), "r"(src), "r"(copy_size),"r"(pld_dst)
                    : "x0", "x1", "x2", "x3", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14");
#endif

            t2 = sched_clock();
            t_result[j] = t2 - t1;
		}		/* 10 times loop */
		t_avg = 0;
		for (j = 0; j < 8; j++) {
			t_avg += t_result[j];
		}
		t_avg = t_avg >> 3;
        if(printlog == 1){
            //printk(KERN_CONT" %d ", avg );
            memset(buf, 0, 32);
            snprintf(buf,32, " %llu ",t_avg);
            snprintf(output_line+line_offset, 320 - line_offset, "%s %d",buf, flush_cache);
            line_offset += strlen(buf);
        }
//		for (pld_dst = 0; pld_dst < 64 * 16 * 2; pld_dst += 64*2) {
		for (pld_dst = 64 * 16 * 2 -128  ; pld_dst >=0 ; pld_dst -= 64*2) {
			avg = 0;
            for (j = 0; j < 8; j++) {
                mdelay(3);
                if (flush_cache == 1)
                    inner_dcache_flush_all();
                t1 = sched_clock();

#if 0
                asm volatile(
                        "mov x0, %0\n"
                        "mov x1, %1\n"
                        "mov x2, %2\n"
                        "mov x3, %3\n"

                        "subs    x2, x2, #128\n"
                        /* There are at least 128 bytes to copy.  */
                        "ldp x7, x8, [x1, #0]\n"
                        "sub x0, x0, #16 \n"      /* Pre-bias.  */
                        "ldp x9, x10, [x1, #16]\n"
                        "ldp x11, x12, [x1, #32]\n"
                        "ldp x13, x14, [x1, #48]!\n"   /* src += 64 - Pre-bias.  */

                        "1:\n"
                        "prfm pldl2keep, [x1,x3]\n"
                        "stp x7, x8, [x0, #16]\n"
                        "ldp x7, x8, [x1, #16]\n"
                        "stp x9, x10, [x0, #32]\n"
                        "ldp x9, x10, [x1, #32]\n"
                        "stp x11, x12, [x0, #48]\n"
                        "ldp x11, x12, [x1, #48]\n"
                        "stp x13, x14, [x0, #64]!\n"
                        "ldp x13, x14, [x1, #64]!\n"
                        "subs x2, x2, #64\n"
                        "b.ge    1b\n"
                        "stp x7, x8, [x0, #16]\n"
                        "stp x9, x10, [x0, #32]\n"
                        "stp x11, x12, [x0, #48]\n"
                        "stp x13, x14, [x0, #64]!\n"

                        :  : "r" (dst), "r"(src), "r"(copy_size),"r"(pld_dst)
                        : "x0", "x1", "x2", "x3", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14");
#else
            asm volatile(
                    "mov x0, %0\n"
                    "mov x1, %1\n"
                    "mov x2, %2\n"
                    "mov x3, %3\n"

                    "subs    x2, x2, #128\n"
                    /* There are at least 128 bytes to copy.  */
                    "ldur q1, [x1, #0]\n"
                    "sub x0, x0, #16 \n"      /* Pre-bias.  */
                    "ldur q2, [x1, #16]\n"
                    "ldur q3, [x1, #32]\n"
                    "ldur q4, [x1, #48]\n"
                    "add x1, x1, #48\n"
                    "1:\n"
                    "prfm pldl1keep, [x1,x3]\n"
                    "stur q1, [x0, #16]\n"
                    "ldur q1, [x1, #16]\n"
                    "stur q2, [x0, #32]\n"
                    "ldur q2, [x1, #32]\n"
                    "stur q3, [x0, #48]\n"
                    "ldur q3, [x1, #48]\n"
                    "stur q4, [x0, #64]\n"
                    "add x0, x0, #64\n"
                    "ldur q4, [x1, #64]\n"
                    "add x1, x1, #64\n"
                    "subs x2, x2, #64\n"
                    "b.ge    1b\n"
                    "stur q1, [x0, #16]\n"
                    "stur q2, [x0, #32]\n"
                    "stur q3, [x0, #48]\n"
                    "stur q4, [x0, #64]\n"

                    :  : "r" (dst), "r"(src), "r"(copy_size),"r"(pld_dst)
                    : "x0", "x1", "x2", "x3", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14");
#endif
                t2 = sched_clock();
                t_result[j] = t2 - t1;

			}	/* 10 times loop */
            t_avg = 0;
            for (j = 0; j < 8; j++) {
                t_avg += t_result[j];
            }
            t_avg = t_avg >> 3;
            if(printlog == 1){
                //printk(KERN_CONT" %d ", avg );
                memset(buf, 0, 32);
                snprintf(buf,32, " %llu ",t_avg);
                snprintf(output_line+line_offset, 320 - line_offset, "%s",buf);
                line_offset += strlen(buf);
            }
		}		/* pld dist loop */
		local_irq_restore(flags);
		preempt_enable();
        printk(KERN_EMERG "%s %d", output_line, flush_cache);
	}
	if (printlog == 1)
		pr_err("\n\r ====NEON instruction test done ==== flush_cache:%d\n", flush_cache);
}

