#ifndef MT_DBG_V71_H
#define MT_DBG_V71_H

#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/proc-fns.h>
#include <asm/system.h>
#if !defined (__KERNEL__) //|| defined (__CTP__)
#include "reg_base.H"
#else  //#if !defined (__KERNEL__) //|| defined(__CTP__)
#include <mach/mt_reg_base.h>
#endif  //#if !defined (__KERNEL__) //|| defined(__CTP__)
#include "sync_write.h"

#define DIDR_VERSION_SHIFT 16
#define DIDR_VERSION_MASK  0xF
#define DIDR_VERSION_7_1   5
#define DIDR_BP_SHIFT      24
#define DIDR_BP_MASK       0xF
#define DIDR_WP_SHIFT      28
#define DIDR_WP_MASK       0xF
#define CLAIMCLR_CLEAR_ALL 0xff

#define DRAR_VALID_MASK   0x00000003
#define DSAR_VALID_MASK   0x00000003
#define DRAR_ADDRESS_MASK 0xFFFFF000
#define DSAR_ADDRESS_MASK 0xFFFFF000
#define OSLSR_OSLM_MASK   0x00000009
#define OSLAR_UNLOCKED    0x00000000
#define OSLAR_LOCKED      0xC5ACCE55
#define LAR_UNLOCKED      0xC5ACCE55
#define LAR_LOCKED        0x00000000
#define OSDLR_UNLOCKED    0x00000000
#define OSDLR_LOCKED      0x00000001

#define DBGDSCR_RXFULL 	  (1<<30)
#define DBGDSCR_TXFULL 	  (1<<29)


#define DBGREG_BP_VAL     0x0
#define DBGREG_WP_VAL     0x1
#define DBGREG_BP_CTRL    0x2
#define DBGREG_WP_CTRL    0x3
#define DBGREG_BP_XVAL    0x4

#define DSCR_int        0x4
#define DIDR            0x0
#define WFAR            0x018
#define VCR             0x01C
#define DSCR_ext        0x088
#define BVR0            0x100
#define BCR0            0x140
#define WVR0            0x180
#define WCR0            0x1C0
#define BXVR4           0x250 
#define LAR             0xfb0
#define CLAIMSET        0xfa0
#define CLAIMCLR        0xfa4



#define dbg_mem_read(addr)          (*(volatile unsigned long *)(addr))
#define dbg_mem_write(addr, val)    mt65xx_reg_sync_writel(val, addr)

inline unsigned read_dbgdidr(void)
{	
	register unsigned ret; 
	__asm__ __volatile ("mrc p14, 0, %0, c0, c0, 0 \n\t"
						:"=r"(ret)); 
	
	return ret;
}


inline unsigned read_dbgosdlr(void)
{	
	register unsigned ret; 
	__asm__ __volatile ("mrc p14, 0, %0, c1, c3, 4 \n\t"
						:"=r"(ret)); 
	
	return ret;
}

inline unsigned write_dbgosdlr(unsigned data)
{	
	__asm__ __volatile ("mcr p14, 0, %0, c1, c3, 4 \n\t"
			    ::"r"(data)); 
	
	return data;
}

inline void write_dbgoslar(unsigned key)
{	
	__asm__ __volatile ("mcr p14, 0, %0, c1, c0, 4 \n\t"
						::"r"(key)); 
}



inline unsigned read_dbgdscr(void)
{	
	register unsigned ret; 
	__asm__ __volatile ("mrc p14, 0, %0, c0, c1, 0 \n\t"
						:"=r"(ret)); 
	
	return ret;
}


inline void write_dbgdscr(unsigned key)
{	
	__asm__ __volatile ("mrc p14, 0, %0, c0, c1, 0 \n\t"
						::"r"(key)); 
}


inline unsigned int *mt_save_dbg_regs(unsigned int *p, unsigned long dbg_base)
{      
	unsigned int dscr, didr;

#if 0
	/***************************************************/
	/* Test DBGDSCRext for halt mode:		   */
        /* if in debug mode, do not go through, 	   */
	/* otherwise, die at oslock.			   */
        /***************************************************/
	if (dbg_mem_read(dbg_base + DSCR_int) & 1) {
                // invlaidate the dbg container 
                *p = ~0x0;
		return p;  
        }
#endif

	// oslock
	write_dbgoslar(OSLAR_LOCKED);
	isb();

        // GDBLAR lock clear, to allow cp14 interface
        dbg_mem_write(dbg_base + LAR, LAR_UNLOCKED);

	// save register
	__asm__ __volatile__ (
                "mrc p14, 0, %1, c0, c2, 2 	@DBGDSCR_ext \n\t" 
		"movw	r5, #:lower16:0x6c30fc3c \n\t"
		"movt	r5, #:upper16:0x6c30fc3c \n\t"
		"and r4, %1, r5 \n\t"
		"mrc p14, 0, r5, c0, c6, 0 	@DBGWFAR \n\t"
		"stm %0!, {r4-r5} \n\t"

		"mrc p14, 0, r4, c0, c0, 5 	@DBGBCR \n\t"
		"mrc p14, 0, r5, c0, c1, 5 	@DBGBCR \n\t"
		"mrc p14, 0, r6, c0, c2, 5 	@DBGBCR \n\t"
		"mrc p14, 0, r7, c0, c3, 5 	@DBGBCR \n\t"
		"mrc p14, 0, r8, c0, c4, 5 	@DBGBCR \n\t"
		"mrc p14, 0, r9, c0, c5, 5 	@DBGBCR \n\t"
		"stm %0!, {r4-r9} \n\t"

		"mrc p14, 0, r4, c0, c0, 4 	@DBGBVR \n\t"
		"mrc p14, 0, r5, c0, c1, 4 	@DBGBVR \n\t"
		"mrc p14, 0, r6, c0, c2, 4 	@DBGBVR \n\t"
		"mrc p14, 0, r7, c0, c3, 4 	@DBGBVR \n\t"
		"mrc p14, 0, r8, c0, c4, 4 	@DBGBVR \n\t"
		"mrc p14, 0, r9, c0, c5, 4 	@DBGBVR \n\t"
		"stm %0!, {r4-r9} \n\t"

		"mrc p14, 0, r4, c1, c4, 1 	@DBGBXVR \n\t"
		"mrc p14, 0, r5, c1, c5, 1 	@DBGBXVR \n\t"
		"stm %0!, {r4-r5} \n\t"

		"mrc p14, 0, r4, c0, c0, 6 	@DBGWVR \n\t"	
		"mrc p14, 0, r5, c0, c1, 6 	@DBGWVR \n\t"	
		"mrc p14, 0, r6, c0, c2, 6 	@DBGWVR \n\t"	
		"mrc p14, 0, r7, c0, c3, 6 	@DBGWVR \n\t"	
		"stm %0!, {r4-r7} \n\t"

		"mrc p14, 0, r4, c0, c0, 7 	@DBGWCR \n\t"	
		"mrc p14, 0, r5, c0, c1, 7 	@DBGWCR \n\t"	
		"mrc p14, 0, r6, c0, c2, 7 	@DBGWCR \n\t"	
		"mrc p14, 0, r7, c0, c3, 7 	@DBGWCR \n\t"	
		"stm %0!, {r4-r7} \n\t"

		"mrc p14, 0, r4, c0, c7, 0 	@DBGVCR \n\t"	
		"mrc p14, 0, r5, c7, c9, 6 	@DBGCLAIMCLR \n\t"	
		"stm %0!, {r4-r5} \n\t"

		:"+r"(p), "=r"(dscr), "=r"(didr)
		:
                :"r4", "r5", "r6", "r7", "r8", "r9"
		);

	/* if (dscr & DBGDSCR_TXFULL) { */
	/* 	*p = *(volatile unsigned long *)(dbg_base+0x8c); //DTRTXext, TXFULL cleared */
		
	/* 	//write back in internal view to restore TXFULL bit */
	/* 	__asm__ __volatile__ ( */
	/* 		"mcr p14, 0, %0, c0, c7, 0 	@DBGVCR \n\t"  */
	/* 		::"r"(p[0])); */
	/* } */
	/* p++; */
	
	/* if (dscr & DBGDSCR_RXFULL) { */
	/* 	*p = *(volatile unsigned long *)(dbg_base+0x14); //DTRRXext */
	/* } */
	/* p++; */
	


        // GDBLAR lock on
        dbg_mem_write(dbg_base + LAR, LAR_LOCKED);

	// os unlock
	write_dbgoslar(OSLAR_UNLOCKED);
        isb();

	return p;
}


inline void mt_restore_dbg_regs(unsigned int *p, unsigned long dbg_base)
{      
	unsigned int dscr;

        // the dbg container is invalid
        if (*p == ~0x0)
                return;

#if 0
	/***************************************************/
	/* Test DBGDSCRext for halt mode:		   */
        /* if in debug mode, do not go through, 	   */
	/* otherwise, die at oslock.			   */
        /***************************************************/
	if (dbg_mem_read(dbg_base + DSCR_int) & 1) {
		//BUG();  
                return;
        }
#endif
	// oslock
	write_dbgoslar(OSLAR_LOCKED);
	isb();

        // GDBLAR lock clear
        dbg_mem_write(dbg_base + LAR, LAR_UNLOCKED);

	// restore register
	__asm__ __volatile__ (
		"ldm %0!, {r4-r5} \n\t"
		"mov %1, r4 \n\t"
		"mcr p14, 0, r4, c0, c2, 2 	@DBGDSCR \n\t" 
		"mcr p14, 0, r5, c0, c6, 0 	@DBGWFAR \n\t"

		"ldm %0!, {r4-r9} \n\t"
		"mcr p14, 0, r4, c0, c0, 5 	@DBGBCR \n\t"
		"mcr p14, 0, r5, c0, c1, 5 	@DBGBCR \n\t"
		"mcr p14, 0, r6, c0, c2, 5 	@DBGBCR \n\t"
		"mcr p14, 0, r7, c0, c3, 5 	@DBGBCR \n\t"
		"mcr p14, 0, r8, c0, c4, 5 	@DBGBCR \n\t"
		"mcr p14, 0, r9, c0, c5, 5 	@DBGBCR \n\t"

		"ldm %0!, {r4-r9} \n\t"
		"mcr p14, 0, r4, c0, c0, 4 	@DBGBVR \n\t"
		"mcr p14, 0, r5, c0, c1, 4 	@DBGBVR \n\t"
		"mcr p14, 0, r6, c0, c2, 4 	@DBGBVR \n\t"
		"mcr p14, 0, r7, c0, c3, 4 	@DBGBVR \n\t"
		"mcr p14, 0, r8, c0, c4, 4 	@DBGBVR \n\t"
		"mcr p14, 0, r9, c0, c5, 4 	@DBGBVR \n\t"

		"ldm %0!, {r4-r5} \n\t"
		"mcr p14, 0, r4, c1, c4, 1 	@DBGBXVR \n\t"
		"mcr p14, 0, r5, c1, c5, 1 	@DBGBXVR \n\t"

		"ldm %0!, {r4-r7} \n\t"
		"mcr p14, 0, r4, c0, c0, 6 	@DBGWVR \n\t"	
		"mcr p14, 0, r5, c0, c1, 6 	@DBGWVR \n\t"	
		"mcr p14, 0, r6, c0, c2, 6 	@DBGWVR \n\t"	
		"mcr p14, 0, r7, c0, c3, 6 	@DBGWVR \n\t"	

		"ldm %0!, {r4-r7} \n\t"
		"mcr p14, 0, r4, c0, c0, 7 	@DBGWCR \n\t"	
		"mcr p14, 0, r5, c0, c1, 7 	@DBGWCR \n\t"	
		"mcr p14, 0, r6, c0, c2, 7 	@DBGWCR \n\t"	
		"mcr p14, 0, r7, c0, c3, 7 	@DBGWCR \n\t"	
                
		"ldm %0!, {r4-r5} \n\t"
		"mcr p14, 0, r4, c0, c7, 0 	@DBGVCR \n\t"	
		"mcr p14, 0, r5, c7, c8, 6 	@DBGCLAIMSET \n\t"	

		:"+r"(p), "=r"(dscr)
		:
		:"r4", "r5", "r6", "r7", "r8", "r9"
                );

	/* if (dscr & DBGDSCR_TXFULL) { */
	/* 	//write back in internal view to restore TXFULL bit */
	/* 	*(volatile unsigned long *)(dbg_base+0x8c) = *p; //DTRTXext, TXFULL cleared */

	/* 	__asm__ __volatile__ ( */
	/* 		"mcr p14, 0, %0, c0, c7, 0 	@DBGVCR \n\t"  */
	/* 		::"r"(p[0])); */
	/* } */
	/* p++; */
	
	/* if (dscr & DBGDSCR_RXFULL) { */
	/* 	*(volatile unsigned long *)(dbg_base+0x14) = *p; //DTRRXext */
	/* } */
	/* p++; */
	

        // GDBLAR lock on
        dbg_mem_write(dbg_base + LAR, LAR_LOCKED);

	// os unlock
	write_dbgoslar(OSLAR_UNLOCKED);
        isb();
	      
}

#define dbg_reg_copy(offset, base_to, base_from)   \
        dbg_mem_write((base_to) + (offset), dbg_mem_read((base_from) + (offset)))

/** to copy dbg registers from FROM to TO within the same cluster.
 * DBG_BASE is the common base of 2 cores debug register space.
 **/
static inline void mt_copy_dbg_regs(unsigned long dbg_base, int to, int from)
{
        unsigned long base_to, base_from;
        
        base_to = dbg_base + to*0x2000;
        base_from = dbg_base + from*0x2000;

	// os unlock
	write_dbgoslar(OSLAR_LOCKED);
        isb();

        // GDBLAR lock clear
        dbg_mem_write(base_to + LAR, LAR_UNLOCKED);
        
        
        dbg_reg_copy(DSCR_ext, base_to, base_from);
        isb();
	

        dbg_reg_copy(WFAR, base_to, base_from);

        dbg_reg_copy(BCR0, base_to, base_from);
        dbg_reg_copy(BCR0+4, base_to, base_from);
        dbg_reg_copy(BCR0+8, base_to, base_from);
        dbg_reg_copy(BCR0+12, base_to, base_from);
        dbg_reg_copy(BCR0+16, base_to, base_from);
        dbg_reg_copy(BCR0+20, base_to, base_from);

        dbg_reg_copy(BVR0, base_to, base_from);
        dbg_reg_copy(BVR0+4, base_to, base_from);
        dbg_reg_copy(BVR0+8, base_to, base_from);
        dbg_reg_copy(BVR0+12, base_to, base_from);
        dbg_reg_copy(BVR0+16, base_to, base_from);
        dbg_reg_copy(BVR0+20, base_to, base_from);

        dbg_reg_copy(BXVR4+0, base_to, base_from);
        dbg_reg_copy(BXVR4+4, base_to, base_from);

        dbg_reg_copy(WVR0, base_to, base_from);
        dbg_reg_copy(WVR0+4, base_to, base_from);
        dbg_reg_copy(WVR0+8, base_to, base_from);
        dbg_reg_copy(WVR0+12, base_to, base_from);

        dbg_reg_copy(WCR0, base_to, base_from);
        dbg_reg_copy(WCR0+4, base_to, base_from);
        dbg_reg_copy(WCR0+8, base_to, base_from);
        dbg_reg_copy(WCR0+12, base_to, base_from);


        dbg_reg_copy(VCR, base_to, base_from);
        dbg_mem_write(base_to + CLAIMSET, dbg_mem_read(base_from + CLAIMCLR));

        isb();

        // GDBLAR lock on
        dbg_mem_write(base_to + LAR, LAR_LOCKED);

	// os unlock
	write_dbgoslar(OSLAR_UNLOCKED);
        isb();

}

#endif
