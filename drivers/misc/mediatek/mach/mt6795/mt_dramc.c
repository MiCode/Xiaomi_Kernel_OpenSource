#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/vmalloc.h>
#include <linux/memblock.h>
#include <linux/sched.h> 
#include <asm/cacheflush.h>
#include <asm/outercache.h>
#include <asm/system.h>
#include <asm/delay.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_clkmgr.h>
#include <mach/mt_freqhopping.h>
#include <mach/emi_bwl.h>
#include <mach/mt_typedefs.h>
#include <mach/memory.h>
#include <mach/mt_sleep.h>
#include <mach/mt_dramc.h>
#include <mach/dma.h>
#include <mach/sync_write.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include "md32_ipi.h"
#include "md32_helper.h"


static void __iomem *APMIXED_BASE_ADDR;
static void __iomem *CQDMA_BASE_ADDR;
static void __iomem *DRAMCAO_BASE_ADDR;
static void __iomem *DDRPHY_BASE_ADDR;
static void __iomem *DRAMCNAO_BASE_ADDR;

volatile unsigned char *dst_array_v;
volatile unsigned char *src_array_v;
volatile unsigned int dst_array_p;
volatile unsigned int src_array_p;
int init_done = 0;
int org_dram_data_rate = 0;

void enter_pasr_dpd_config(unsigned char segment_rank0, unsigned char segment_rank1)
{
    if(segment_rank1 == 0xFF) //all segments of rank1 are not reserved -> rank1 enter DPD
    {
        slp_dpd_en(1);
	segment_rank1 = 0x0;
    }
    
    slp_pasr_en(1, segment_rank0 | (segment_rank1 << 8) | (((unsigned short)org_dram_data_rate) << 16)); 
}

void exit_pasr_dpd_config(void)
{
    slp_dpd_en(0);
    slp_pasr_en(0, 0);
}

#define FREQREG_SIZE 51 
#define PLLGRPREG_SIZE	20
vcore_dvfs_info_t __nosavedata g_vcore_dvfs_info;
unsigned int __nosavedata vcore_dvfs_data[((FREQREG_SIZE*4) + (PLLGRPREG_SIZE*2) + 2)];

void store_vcore_dvfs_setting(void)
{
    unsigned int *atag_ptr, *buf;
    unsigned int pll_setting_num, freq_setting_num;
    
#ifdef CONFIG_OF
    if (of_chosen) 
    {
        atag_ptr = (unsigned int*)of_get_property(of_chosen, "atag,vcore_dvfs", NULL);      
        if(atag_ptr)
        {
            atag_ptr += 2; //skip tag header
            
            memcpy((void*)vcore_dvfs_data, (void*)atag_ptr, (FREQREG_SIZE*4 + PLLGRPREG_SIZE*2 + 2)*sizeof(unsigned int));
            buf = (unsigned int *)&vcore_dvfs_data[0];
            pll_setting_num = g_vcore_dvfs_info.pll_setting_num = *buf++;
            freq_setting_num = g_vcore_dvfs_info.freq_setting_num = *buf++;
            
            ASSERT((pll_setting_num == PLLGRPREG_SIZE) && (freq_setting_num == FREQREG_SIZE));

            g_vcore_dvfs_info.low_freq_pll_setting_addr = (unsigned int)buf;
            buf += pll_setting_num;
            g_vcore_dvfs_info.low_freq_cha_setting_addr = (unsigned int)buf;
            buf += freq_setting_num;
            g_vcore_dvfs_info.low_freq_chb_setting_addr = (unsigned int)buf;
            buf += freq_setting_num;
            g_vcore_dvfs_info.high_freq_pll_setting_addr = (unsigned int)buf;
            buf += pll_setting_num;
            g_vcore_dvfs_info.high_freq_cha_setting_addr = (unsigned int)buf;
            buf += freq_setting_num;
            g_vcore_dvfs_info.high_freq_chb_setting_addr = (unsigned int)buf;    
            printk("[vcore dvfs][kernel]low_freq_pll_setting_addr = 0x%lx, value[0] = 0x%x\n", g_vcore_dvfs_info.low_freq_pll_setting_addr, *(unsigned int*)(g_vcore_dvfs_info.low_freq_pll_setting_addr));
            printk("[vcore dvfs][kernel]low_freq_cha_setting_addr = 0x%lx, value[0] = 0x%x\n", g_vcore_dvfs_info.low_freq_cha_setting_addr, *(unsigned int*)(g_vcore_dvfs_info.low_freq_cha_setting_addr));
            printk("[vcore dvfs][kernel]low_freq_chb_setting_addr = 0x%lx, value[0] = 0x%x\n", g_vcore_dvfs_info.low_freq_chb_setting_addr, *(unsigned int*)(g_vcore_dvfs_info.low_freq_chb_setting_addr));
            printk("[vcore dvfs][kernel]high_freq_pll_setting_addr = 0x%lx, value[0] = 0x%x\n", g_vcore_dvfs_info.high_freq_pll_setting_addr, *(unsigned int*)(g_vcore_dvfs_info.high_freq_pll_setting_addr));
            printk("[vcore dvfs][kernel]high_freq_cha_setting_addr = 0x%lx, value[0] = 0x%x\n", g_vcore_dvfs_info.high_freq_cha_setting_addr, *(unsigned int*)(g_vcore_dvfs_info.high_freq_cha_setting_addr));
            printk("[vcore dvfs][kernel]high_freq_chb_setting_addr = 0x%lx, value[0] = 0x%x\n", g_vcore_dvfs_info.high_freq_chb_setting_addr, *(unsigned int*)(g_vcore_dvfs_info.high_freq_chb_setting_addr));
            printk("[vcore dvfs][kernel]pll_setting_num = %d\n", g_vcore_dvfs_info.pll_setting_num);
            printk("[vcore dvfs][kernel]freq_setting_num = %d\n", g_vcore_dvfs_info.freq_setting_num);              
        }
        else
            printk("[%s] No atag,vcore_dvfs!\n", __func__);
    }
    else
        printk("[%s] of_chosen is NULL!\n", __func__);
#endif   
    {
        unsigned int high, low, num;
        get_mempll_table_info(&high, &low, &num);
        printk("[vcore dvfs]pll_high_addr = 0x%x, pll_low_addr = 0x%x, num = %d\n", high, low, num);
    }     
}

void vcore_dvfs_ipi_handler(int id, void *data, unsigned int len)
{
    int i;
    unsigned int src_ptr, dst_ptr;
    int pll_setting_num = g_vcore_dvfs_info.pll_setting_num;
    int freq_setting_num = g_vcore_dvfs_info.freq_setting_num;
    
    vcore_dvfs_info_t *md32_dvfs_info = (vcore_dvfs_info_t*)data;
    *(unsigned int*)(MD32_DTCM + md32_dvfs_info->pll_setting_num) = pll_setting_num;
    *(unsigned int*)(MD32_DTCM + md32_dvfs_info->freq_setting_num) = freq_setting_num;
    
    printk("[vcore dvfs]md32 ipi handler is called\n");
   
    src_ptr = g_vcore_dvfs_info.low_freq_pll_setting_addr;
    dst_ptr = MD32_DTCM + md32_dvfs_info->low_freq_pll_setting_addr;
    for(i=0; i<pll_setting_num; i++)
    {
        *(u32*)dst_ptr = *(u32*)src_ptr;
        //printk("[vcore dvfs][md32]pll_low: dst_ptr = 0x%x, src_ptr=0x%x, value=0x%x\n", dst_ptr, src_ptr, *(u32*)dst_ptr);
        dst_ptr+=4;
        src_ptr+=4;
    }      
    
    src_ptr = g_vcore_dvfs_info.low_freq_cha_setting_addr;
    dst_ptr = MD32_DTCM + md32_dvfs_info->low_freq_cha_setting_addr;
    for(i=0; i<freq_setting_num; i++)
    {
        *(u32*)dst_ptr = *(u32*)src_ptr;
        //printk("[vcore dvfs][md32]cha_low: dst_ptr = 0x%x, src_ptr=0x%x, value=0x%x\n", dst_ptr, src_ptr, *(u32*)dst_ptr);
        dst_ptr+=4;
        src_ptr+=4;
    } 
        
    src_ptr = g_vcore_dvfs_info.low_freq_chb_setting_addr;
    dst_ptr = MD32_DTCM + md32_dvfs_info->low_freq_chb_setting_addr;
    for(i=0; i<freq_setting_num; i++)
    {
        *(u32*)dst_ptr = *(u32*)src_ptr;
        //printk("[vcore dvfs][md32]chb_low: dst_ptr = 0x%x, src_ptr=0x%x, value=0x%x\n", dst_ptr, src_ptr, *(u32*)dst_ptr);
        dst_ptr+=4;
        src_ptr+=4;
    } 
        
    src_ptr = g_vcore_dvfs_info.high_freq_pll_setting_addr;
    dst_ptr = MD32_DTCM + md32_dvfs_info->high_freq_pll_setting_addr;
    for(i=0; i<pll_setting_num; i++)
    {
        *(u32*)dst_ptr = *(u32*)src_ptr;
        //printk("[vcore dvfs][md32]pll_high: dst_ptr = 0x%x, src_ptr=0x%x, value=0x%x\n", dst_ptr, src_ptr, *(u32*)dst_ptr);
        dst_ptr+=4;
        src_ptr+=4;
    }  
    
    src_ptr = g_vcore_dvfs_info.high_freq_cha_setting_addr;
    dst_ptr = MD32_DTCM + md32_dvfs_info->high_freq_cha_setting_addr;
    for(i=0; i<freq_setting_num; i++)
    {
        *(u32*)dst_ptr = *(u32*)src_ptr;
        //printk("[vcore dvfs][md32]cha_high: dst_ptr = 0x%x, src_ptr=0x%x, value=0x%x\n", dst_ptr, src_ptr, *(u32*)dst_ptr);
        dst_ptr+=4;
        src_ptr+=4;        
    } 
        
    src_ptr = g_vcore_dvfs_info.high_freq_chb_setting_addr;
    dst_ptr = MD32_DTCM + md32_dvfs_info->high_freq_chb_setting_addr;
    for(i=0; i<freq_setting_num; i++)
    {
        *(u32*)dst_ptr = *(u32*)src_ptr;
        //printk("[vcore dvfs][md32]chb_high: dst_ptr = 0x%x, src_ptr=0x%x, value=0x%x\n", dst_ptr, src_ptr, *(u32*)dst_ptr);
        dst_ptr+=4;
        src_ptr+=4;        
    }   
}

void get_mempll_table_info(u32 *high_addr, u32 *low_addr, u32 *num)
{
    unsigned int pll_setting_num;
    pll_setting_num = g_vcore_dvfs_info.pll_setting_num;
    
    if(pll_setting_num != 0)
    {    
        *high_addr = g_vcore_dvfs_info.high_freq_pll_setting_addr;     
        *low_addr  = g_vcore_dvfs_info.low_freq_pll_setting_addr;
        *num = pll_setting_num;
    }
    else
    {
        *high_addr = *low_addr = *num = 0; 
    }
}

#define MEM_TEST_SIZE 0x2000
#define PATTERN1 0x5A5A5A5A
#define PATTERN2 0xA5A5A5A5
int Binning_DRAM_complex_mem_test (void)
{
    unsigned char *MEM8_BASE;
    unsigned short *MEM16_BASE;
    unsigned int *MEM32_BASE;
    unsigned int *MEM_BASE;
    unsigned char pattern8;
    unsigned short pattern16;
    unsigned int i, j, size, pattern32;
    unsigned int value;
    unsigned int len=MEM_TEST_SIZE;
    void *ptr;   
    ptr = vmalloc(PAGE_SIZE*2);
    MEM8_BASE=(unsigned char *)ptr;
    MEM16_BASE=(unsigned short *)ptr;
    MEM32_BASE=(unsigned int *)ptr;
    MEM_BASE=(unsigned int *)ptr;
    printk("Test DRAM start address 0x%x\n",(unsigned int)ptr);
    printk("Test DRAM SIZE 0x%x\n",MEM_TEST_SIZE);
    size = len >> 2;

    /* === Verify the tied bits (tied high) === */
    for (i = 0; i < size; i++)
    {
        MEM32_BASE[i] = 0;
    }

    for (i = 0; i < size; i++)
    {
        if (MEM32_BASE[i] != 0)
        {
            vfree(ptr);
            return -1;
        }
        else
        {
            MEM32_BASE[i] = 0xffffffff;
        }
    }

    /* === Verify the tied bits (tied low) === */
    for (i = 0; i < size; i++)
    {
        if (MEM32_BASE[i] != 0xffffffff)
        {
            vfree(ptr);
            return -2;
        }
        else
            MEM32_BASE[i] = 0x00;
    }

    /* === Verify pattern 1 (0x00~0xff) === */
    pattern8 = 0x00;
    for (i = 0; i < len; i++)
        MEM8_BASE[i] = pattern8++;
    pattern8 = 0x00;
    for (i = 0; i < len; i++)
    {
        if (MEM8_BASE[i] != pattern8++)
        { 
            vfree(ptr);
            return -3;
        }
    }

    /* === Verify pattern 2 (0x00~0xff) === */
    pattern8 = 0x00;
    for (i = j = 0; i < len; i += 2, j++)
    {
        if (MEM8_BASE[i] == pattern8)
            MEM16_BASE[j] = pattern8;
        if (MEM16_BASE[j] != pattern8)
        {
            vfree(ptr);
            return -4;
        }
        pattern8 += 2;
    }

    /* === Verify pattern 3 (0x00~0xffff) === */
    pattern16 = 0x00;
    for (i = 0; i < (len >> 1); i++)
        MEM16_BASE[i] = pattern16++;
    pattern16 = 0x00;
    for (i = 0; i < (len >> 1); i++)
    {
        if (MEM16_BASE[i] != pattern16++)                                                                                                    
        {
            vfree(ptr);
            return -5;
        }
    }

    /* === Verify pattern 4 (0x00~0xffffffff) === */
    pattern32 = 0x00;
    for (i = 0; i < (len >> 2); i++)
        MEM32_BASE[i] = pattern32++;
    pattern32 = 0x00;
    for (i = 0; i < (len >> 2); i++)
    {
        if (MEM32_BASE[i] != pattern32++)
        { 
            vfree(ptr);
            return -6;
        }
    }

    /* === Pattern 5: Filling memory range with 0x44332211 === */
    for (i = 0; i < size; i++)
        MEM32_BASE[i] = 0x44332211;

    /* === Read Check then Fill Memory with a5a5a5a5 Pattern === */
    for (i = 0; i < size; i++)
    {
        if (MEM32_BASE[i] != 0x44332211)
        {
            vfree(ptr);
            return -7;
        }
        else
        {
            MEM32_BASE[i] = 0xa5a5a5a5;
        }
    }

    /* === Read Check then Fill Memory with 00 Byte Pattern at offset 0h === */
    for (i = 0; i < size; i++)
    {
        if (MEM32_BASE[i] != 0xa5a5a5a5)
        { 
            vfree(ptr);
            return -8;  
        }
        else                                                                                                                              
        {
            MEM8_BASE[i * 4] = 0x00;
        }
    }

    /* === Read Check then Fill Memory with 00 Byte Pattern at offset 2h === */
    for (i = 0; i < size; i++)
    {
        if (MEM32_BASE[i] != 0xa5a5a500)
        {
            vfree(ptr);
            return -9;
        }
        else
        {
            MEM8_BASE[i * 4 + 2] = 0x00;
        }
    }

    /* === Read Check then Fill Memory with 00 Byte Pattern at offset 1h === */
    for (i = 0; i < size; i++)
    {
        if (MEM32_BASE[i] != 0xa500a500)
        {
            vfree(ptr);
            return -10;
        }
        else
        {
            MEM8_BASE[i * 4 + 1] = 0x00;
        }
    }

    /* === Read Check then Fill Memory with 00 Byte Pattern at offset 3h === */
    for (i = 0; i < size; i++)
    {
        if (MEM32_BASE[i] != 0xa5000000)
        {
            vfree(ptr);
            return -11;
        }
        else
        {
            MEM8_BASE[i * 4 + 3] = 0x00;
        }
    }

    /* === Read Check then Fill Memory with ffff Word Pattern at offset 1h == */
    for (i = 0; i < size; i++)
    {
        if (MEM32_BASE[i] != 0x00000000)
        {
            vfree(ptr);
            return -12;
        }
        else
        {
            MEM16_BASE[i * 2 + 1] = 0xffff;
        }
    }


    /* === Read Check then Fill Memory with ffff Word Pattern at offset 0h == */
    for (i = 0; i < size; i++)
    {
        if (MEM32_BASE[i] != 0xffff0000)
        {
            vfree(ptr);
            return -13;
        }
        else
        {
            MEM16_BASE[i * 2] = 0xffff;
        }
    }
    /*===  Read Check === */
    for (i = 0; i < size; i++)
    {
        if (MEM32_BASE[i] != 0xffffffff)
        {
            vfree(ptr);
            return -14;
        }
    }


    /************************************************
    * Additional verification
    ************************************************/
    /* === stage 1 => write 0 === */

    for (i = 0; i < size; i++)
    {
        MEM_BASE[i] = PATTERN1;
    }


    /* === stage 2 => read 0, write 0xF === */
    for (i = 0; i < size; i++)
    {
        value = MEM_BASE[i];

        if (value != PATTERN1)
        {
            vfree(ptr);
            return -15;
        }
        MEM_BASE[i] = PATTERN2;
    }


    /* === stage 3 => read 0xF, write 0 === */
    for (i = 0; i < size; i++)
    {
        value = MEM_BASE[i];
        if (value != PATTERN2)
        {
            vfree(ptr);
            return -16;
        }
        MEM_BASE[i] = PATTERN1;
    }


    /* === stage 4 => read 0, write 0xF === */
    for (i = 0; i < size; i++)
    {
        value = MEM_BASE[i];
        if (value != PATTERN1)
        {
            vfree(ptr);
            return -17;
        }
        MEM_BASE[i] = PATTERN2;
    }


    /* === stage 5 => read 0xF, write 0 === */
    for (i = 0; i < size; i++)
    {
        value = MEM_BASE[i];
        if (value != PATTERN2)
        { 
            vfree(ptr);
            return -18;
        }
        MEM_BASE[i] = PATTERN1;
    }


    /* === stage 6 => read 0 === */
    for (i = 0; i < size; i++)
    {
        value = MEM_BASE[i];
        if (value != PATTERN1)
        {
            vfree(ptr);
            return -19;
        }
    }


    /* === 1/2/4-byte combination test === */
    i = (unsigned int) MEM_BASE;
    while (i < (unsigned int) MEM_BASE + (size << 2))
    {
        *((unsigned char *) i) = 0x78;
        i += 1;
        *((unsigned char *) i) = 0x56;
        i += 1;
        *((unsigned short *) i) = 0x1234;
        i += 2;
        *((unsigned int *) i) = 0x12345678;
        i += 4;
        *((unsigned short *) i) = 0x5678;
        i += 2;
        *((unsigned char *) i) = 0x34;
        i += 1;
        *((unsigned char *) i) = 0x12;
        i += 1;
        *((unsigned int *) i) = 0x12345678;
        i += 4;
        *((unsigned char *) i) = 0x78;
        i += 1;
        *((unsigned char *) i) = 0x56;
        i += 1;
        *((unsigned short *) i) = 0x1234;
        i += 2;
        *((unsigned int *) i) = 0x12345678;
        i += 4;
        *((unsigned short *) i) = 0x5678;
        i += 2;
        *((unsigned char *) i) = 0x34;
        i += 1;
        *((unsigned char *) i) = 0x12;
        i += 1;
        *((unsigned int *) i) = 0x12345678;
        i += 4;
    }
    for (i = 0; i < size; i++)
    {
        value = MEM_BASE[i];
        if (value != 0x12345678)
        {
            vfree(ptr);
            return -20;
        }
    }


    /* === Verify pattern 1 (0x00~0xff) === */
    pattern8 = 0x00;
    MEM8_BASE[0] = pattern8;
    for (i = 0; i < size * 4; i++)
    {
        unsigned char waddr8, raddr8;
        waddr8 = i + 1;
        raddr8 = i;
        if (i < size * 4 - 1)
            MEM8_BASE[waddr8] = pattern8 + 1;
        if (MEM8_BASE[raddr8] != pattern8)
        {
            vfree(ptr);
            return -21;
        }
        pattern8++;
    }


    /* === Verify pattern 2 (0x00~0xffff) === */
    pattern16 = 0x00;
    MEM16_BASE[0] = pattern16;
    for (i = 0; i < size * 2; i++)
    {
        if (i < size * 2 - 1)
            MEM16_BASE[i + 1] = pattern16 + 1;
        if (MEM16_BASE[i] != pattern16)
        {
            vfree(ptr);
            return -22;
        }
        pattern16++;
    }
    /* === Verify pattern 3 (0x00~0xffffffff) === */
    pattern32 = 0x00;
    MEM32_BASE[0] = pattern32;
    for (i = 0; i < size; i++)
    {
        if (i < size - 1)
            MEM32_BASE[i + 1] = pattern32 + 1;
        if (MEM32_BASE[i] != pattern32)
        {
            vfree(ptr);
            return -23;
        }
        pattern32++;
    }
    printk("complex R/W mem test pass\n");
    vfree(ptr);
    return 1;
}

static ssize_t complex_mem_test_show(struct device_driver *driver, char *buf)
{
    int ret;
    ret=Binning_DRAM_complex_mem_test();
    if(ret>0)
    {
      return snprintf(buf, PAGE_SIZE, "MEM Test all pass\n");
    }
    else
    {
      return snprintf(buf, PAGE_SIZE, "MEM TEST failed %d \n", ret);
    }
}

static ssize_t complex_mem_test_store(struct device_driver *driver, const char *buf, size_t count)
{
    return count;
}

#ifdef APDMA_TEST
static ssize_t DFS_APDMA_TEST_show(struct device_driver *driver, char *buf)
{   
    dma_dummy_read_for_vcorefs(7);
    return snprintf(buf, PAGE_SIZE, "DFS APDMA Dummy Read Address 0x%x\n",(unsigned int)src_array_p);
}
static ssize_t DFS_APDMA_TEST_store(struct device_driver *driver, const char *buf, size_t count)
{
    return count;
}
#endif

U32 ucDram_Register_Read(unsigned long u4reg_addr)
{
    U32 pu4reg_value;

   	pu4reg_value = (*(volatile unsigned int *)(DRAMCAO_BASE_ADDR + (u4reg_addr))) |
				   (*(volatile unsigned int *)(DDRPHY_BASE_ADDR + (u4reg_addr))) |
				   (*(volatile unsigned int *)(DRAMCNAO_BASE_ADDR + (u4reg_addr)));
 
    return pu4reg_value;
}

void ucDram_Register_Write(unsigned long u4reg_addr, unsigned int u4reg_value)
{
	(*(volatile unsigned int *)(DRAMCAO_BASE_ADDR + (u4reg_addr))) = u4reg_value;
	(*(volatile unsigned int *)(DDRPHY_BASE_ADDR + (u4reg_addr))) = u4reg_value;
	(*(volatile unsigned int *)(DRAMCNAO_BASE_ADDR + (u4reg_addr))) = u4reg_value;
    dsb();
}

bool pasr_is_valid(void)
{
	unsigned int ddr_type=0;

		ddr_type=get_ddr_type();
		/* Following DDR types can support PASR */
      		if(ddr_type == LPDDR3_1866 || 
      		   ddr_type == DUAL_LPDDR3_1600 || 
      		   ddr_type == LPDDR2) 
        {
			return true;
		}

	return false;
}

//-------------------------------------------------------------------------
/** Round_Operation
 *  Round operation of A/B
 *  @param  A   
 *  @param  B   
 *  @retval round(A/B) 
 */
//-------------------------------------------------------------------------
U32 Round_Operation(U32 A, U32 B)
{
    U32 temp;

    if (B == 0)
    {
        return 0xffffffff;
    }
    
    temp = A/B;
        
    if ((A-temp*B) >= ((temp+1)*B-A))
    {
        return (temp+1);
    }
    else
    {
        return temp;
    }    
}

U32 get_dram_data_rate()
{
    U32 u4value1, u4value2, MPLL_POSDIV, MPLL_PCW, MPLL_FOUT;
    U32 MEMPLL_FBKDIV, MEMPLL_FOUT;
    
    u4value1 = (*(volatile unsigned int *)(APMIXED_BASE_ADDR + 0x280));
    u4value2 = (u4value1 & 0x00000070) >> 4;
    if (u4value2 == 0)
    {
        MPLL_POSDIV = 1;
    }
    else if (u4value2 == 1)
    {
        MPLL_POSDIV = 2;
    }
    else if (u4value2 == 2)
    {
        MPLL_POSDIV = 4;
    }
    else if (u4value2 == 3)
    {
        MPLL_POSDIV = 8;
    }
    else
    {
        MPLL_POSDIV = 16;
    }

    u4value1 = *(volatile unsigned int *)(APMIXED_BASE_ADDR + 0x284);
    MPLL_PCW = (u4value1 & 0x001fffff);

    MPLL_FOUT = 26/1*MPLL_PCW;
    MPLL_FOUT = Round_Operation(MPLL_FOUT, MPLL_POSDIV*28); // freq*16384

    u4value1 = *(volatile unsigned int *)(DDRPHY_BASE_ADDR + 0x614);
    MEMPLL_FBKDIV = (u4value1 & 0x007f0000) >> 16;

    MEMPLL_FOUT = MPLL_FOUT*1*4*(MEMPLL_FBKDIV+1);
    MEMPLL_FOUT = Round_Operation(MEMPLL_FOUT, 16384);

    //printk("MPLL_POSDIV=%d, MPLL_PCW=0x%x, MPLL_FOUT=%d, MEMPLL_FBKDIV=%d, MEMPLL_FOUT=%d\n", MPLL_POSDIV, MPLL_PCW, MPLL_FOUT, MEMPLL_FBKDIV, MEMPLL_FOUT);

    return MEMPLL_FOUT;
}

unsigned int DRAM_MRR(int MRR_num)
{
    unsigned int MRR_value = 0x0;
    unsigned int u4value; 
          
    // set DQ bit 0, 1, 2, 3, 4, 5, 6, 7 pinmux for LPDDR3             
    ucDram_Register_Write(DRAMC_REG_RRRATE_CTL, 0x13121110);
    ucDram_Register_Write(DRAMC_REG_MRR_CTL, 0x17161514);
    
    ucDram_Register_Write(DRAMC_REG_MRS, MRR_num);
    ucDram_Register_Write(DRAMC_REG_SPCMD, ucDram_Register_Read(DRAMC_REG_SPCMD) | 0x00000002);
    //udelay(1);
    while ((ucDram_Register_Read(DRAMC_REG_SPCMDRESP) & 0x02) == 0);
    ucDram_Register_Write(DRAMC_REG_SPCMD, ucDram_Register_Read(DRAMC_REG_SPCMD) & 0xFFFFFFFD);

    
    u4value = ucDram_Register_Read(DRAMC_REG_SPCMDRESP);
    MRR_value = (u4value >> 20) & 0xFF;

    return MRR_value;
}

unsigned int read_dram_temperature(void)
{
    unsigned int value;
        
    value = DRAM_MRR(4) & 0x7;
    return value;
}

#ifdef READ_DRAM_TEMP_TEST
static ssize_t read_dram_temp_show(struct device_driver *driver, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "DRAM MR4 = 0x%x \n", read_dram_temperature());
}
static ssize_t read_dram_temp_store(struct device_driver *driver, const char *buf, size_t count)
{
    return count;
}
#endif

static ssize_t read_dram_data_rate_show(struct device_driver *driver, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "DRAM data rate = %d \n", get_dram_data_rate());
}
static ssize_t read_dram_data_rate_store(struct device_driver *driver, const char *buf, size_t count)
{
    return count;
}

DRIVER_ATTR(emi_clk_mem_test, 0664, complex_mem_test_show, complex_mem_test_store);

#ifdef APDMA_TEST
DRIVER_ATTR(dram_dummy_read_test, 0664, DFS_APDMA_TEST_show, DFS_APDMA_TEST_store);
#endif

#ifdef READ_DRAM_TEMP_TEST
DRIVER_ATTR(read_dram_temp_test, 0664, read_dram_temp_show, read_dram_temp_store);
#endif

DRIVER_ATTR(read_dram_data_rate, 0664, read_dram_data_rate_show, read_dram_data_rate_store);

static struct device_driver dram_test_drv =
{
    .name = "emi_clk_test",
    .bus = &platform_bus_type,
    .owner = THIS_MODULE,
};

extern char __ssram_text, _sram_start, __esram_text;
int __init dram_test_init(void)
{
    int ret;
    struct device_node *node;

    /* DTS version */  
    node = of_find_compatible_node(NULL, NULL, "mediatek,APMIXED");
    if(node) {
        APMIXED_BASE_ADDR = of_iomap(node, 0);
        printk("get APMIXED_BASE_ADDR @ %p\n", APMIXED_BASE_ADDR);
    } else {
        printk("can't find compatible node\n");
        return -1;
    }

    node = of_find_compatible_node(NULL, NULL, "mediatek,CQDMA");
    if(node) {
        CQDMA_BASE_ADDR = of_iomap(node, 0);
        printk("[DRAMC]get CQDMA_BASE_ADDR @ %p\n", CQDMA_BASE_ADDR);
    } else {
        printk("[DRAMC]can't find compatible node\n");
        return -1;
    }
        
	  node = of_find_compatible_node(NULL, NULL, "mediatek,DRAMC0");
	  if (node) {
        DRAMCAO_BASE_ADDR = of_iomap(node, 0);
        printk("[DRAMC]get DRAMCAO_BASE_ADDR @ %p\n", DRAMCAO_BASE_ADDR);
    }
    else {
        printk("[DRAMC]can't find DRAMC0 compatible node\n");
        return -1;
    }
    
    node = of_find_compatible_node(NULL, NULL, "mediatek,DDRPHY");
    if(node) {
        DDRPHY_BASE_ADDR = of_iomap(node, 0);
        printk("[DRAMC]get DDRPHY_BASE_ADDR @ %p\n", DDRPHY_BASE_ADDR);
    }
    else {
        printk("[DRAMC]can't find DDRPHY compatible node\n");
        return -1;
    }
    
    node = of_find_compatible_node(NULL, NULL, "mediatek,DRAMC_NAO");
    if(node) {
        DRAMCNAO_BASE_ADDR = of_iomap(node, 0);
        printk("[DRAMC]get DRAMCNAO_BASE_ADDR @ %p\n", DRAMCNAO_BASE_ADDR);
    }
    else {
        printk("[DRAMC]can't find DRAMCNAO compatible node\n");
	  	return -1;
	  }

    ret = driver_register(&dram_test_drv);
    if (ret) {
        printk(KERN_ERR "fail to create the dram_test driver\n");
        return ret;
    }

    ret = driver_create_file(&dram_test_drv, &driver_attr_emi_clk_mem_test);
    if (ret) {
        printk(KERN_ERR "fail to create the emi_clk_mem_test sysfs files\n");
        return ret;
    }

#ifdef APDMA_TEST
    ret = driver_create_file(&dram_test_drv, &driver_attr_dram_dummy_read_test);
    if (ret) {
        printk(KERN_ERR "fail to create the DFS sysfs files\n");
        return ret;
    }
#endif

#ifdef READ_DRAM_TEMP_TEST
    ret = driver_create_file(&dram_test_drv, &driver_attr_read_dram_temp_test);
    if (ret) {
        printk(KERN_ERR "fail to create the read dram temp sysfs files\n");
        return ret;
}
#endif

    ret = driver_create_file(&dram_test_drv, &driver_attr_read_dram_data_rate);
    if (ret) {
        printk(KERN_ERR "fail to create the read dram data rate sysfs files\n");
        return ret;
    }
    
    printk(KERN_INFO "[DRAMC Driver] Store Vcore DVFS settings...\n");
    store_vcore_dvfs_setting();
    printk(KERN_INFO "[DRAMC Driver] Register MD32 Vcore DVFS Handler...\n");
    md32_ipi_registration(IPI_VCORE_DVFS, vcore_dvfs_ipi_handler, "vcore_dvfs");
    org_dram_data_rate = get_dram_data_rate();
    printk(KERN_INFO "[DRAMC Driver] Dram Data Rate = %d\n", org_dram_data_rate);
    
    return 0;
}

int DFS_APDMA_early_init(void)
{
    phys_addr_t max_dram_size = get_max_DRAM_size();
    phys_addr_t dummy_read_center_address;

    if(init_done == 0)
    {
        if(max_dram_size == 0x100000000ULL )  //dram size = 4GB
        {
          dummy_read_center_address = 0x80000000ULL; 
        }
        else if (max_dram_size <= 0xC0000000) //dram size <= 3GB   
        {
          dummy_read_center_address = DRAM_BASE+(max_dram_size >> 1); 
        }
        else
        {
          ASSERT(0);
        }   
        
        src_array_p = (volatile unsigned int)(dummy_read_center_address - (BUFF_LEN >> 1));
        dst_array_p = (volatile unsigned int)(dummy_read_center_address + (BUFF_LEN >> 1)); 
        
#ifdef APDMAREG_DUMP        
        src_array_v = ioremap(rounddown(src_array_p,IOREMAP_ALIGMENT),IOREMAP_ALIGMENT << 1)+IOREMAP_ALIGMENT-(BUFF_LEN >> 1);
        dst_array_v = src_array_v+BUFF_LEN;
#endif
        
        init_done = 1;
    }    

   return 1;
}
int DFS_APDMA_Init(void)
{
    writel(((~DMA_GSEC_EN_BIT)&readl(DMA_GSEC_EN)), DMA_GSEC_EN);
    return 1;
}

int DFS_APDMA_Enable(void)
{
#ifdef APDMAREG_DUMP    
    int i;
#endif
    
    while(readl(DMA_START)& 0x1);
    writel(src_array_p, DMA_SRC);
    writel(dst_array_p, DMA_DST);
    writel(BUFF_LEN , DMA_LEN1);
    writel(DMA_CON_BURST_8BEAT, DMA_CON);    

#ifdef APDMAREG_DUMP
   printk("src_p=0x%x, dst_p=0x%x, src_v=0x%x, dst_v=0x%x, len=%d\n", src_array_p, dst_array_p, (unsigned int)src_array_v, (unsigned int)dst_array_v, BUFF_LEN);
   for (i=0;i<0x60;i+=4)
   {
     printk("[Before]addr:0x%x, value:%x\n",(unsigned int)(DMA_BASE+i),*((volatile int *)(DMA_BASE+i)));
   }                          
     
#ifdef APDMA_TEST
   for(i = 0; i < BUFF_LEN/sizeof(unsigned int); i++) {
                dst_array_v[i] = 0;
                src_array_v[i] = i;
        }
#endif
#endif

  mt_reg_sync_writel(0x1,DMA_START);
   
#ifdef APDMAREG_DUMP
   for (i=0;i<0x60;i+=4)
   {
     printk("[AFTER]addr:0x%x, value:%x\n",(unsigned int)(DMA_BASE+i),*((volatile int *)(DMA_BASE+i)));
   }

#ifdef APDMA_TEST
        for(i = 0; i < BUFF_LEN/sizeof(unsigned int); i++){
                if(dst_array_v[i] != src_array_v[i]){
                        printk("DMA ERROR at Address %x\n (i=%d, value=0x%x(should be 0x%x))", (unsigned int)&dst_array_v[i], i, dst_array_v[i], src_array_v[i]);
                        ASSERT(0);
                }
        }
        printk("Channe0 DFS DMA TEST PASS\n");
#endif
#endif
        return 1;     
}

int DFS_APDMA_END(void)
{
    while(readl(DMA_START));
    return 1 ;
}

void dma_dummy_read_for_vcorefs(int loops)
{
    int i, count;
    unsigned long long start_time, end_time, duration;
    
    DFS_APDMA_early_init();    
    enable_clock(MT_CG_INFRA_GCE, "CQDMA");
    for(i=0; i<loops; i++)
    {
        count = 0;        
        start_time = sched_clock();
        do{
            DFS_APDMA_Enable();
            DFS_APDMA_END();
            end_time = sched_clock();
            duration = end_time - start_time;
            count++;
        }while(duration < 16000L);
        //printk("[DMA_dummy_read[%d], duration=%lld, count = %d\n", duration, count);   
    }        
    disable_clock(MT_CG_INFRA_GCE, "CQDMA");
}

/*
 * XXX: Reserved memory in low memory must be 1MB aligned.
 *     This is because the Linux kernel always use 1MB section to map low memory.
 *
 *    We Reserved the memory regien which could cross rank for APDMA to do dummy read.
 *    
 */

void DFS_Reserved_Memory(void)
{
  phys_addr_t high_memory_phys;
  phys_addr_t DFS_dummy_read_center_address;
  phys_addr_t max_dram_size = get_max_DRAM_size();

  high_memory_phys=virt_to_phys(high_memory);

  if(max_dram_size == 0x100000000ULL )  //dram size = 4GB
  {
    DFS_dummy_read_center_address = 0x80000000ULL; 
  }
  else if (max_dram_size <= 0xC0000000) //dram size <= 3GB   
  {
    DFS_dummy_read_center_address = DRAM_BASE+(max_dram_size >> 1);
  }
  else
  {
    ASSERT(0);
  }
  
  /*For DFS Purpose, we remove this memory block for Dummy read/write by APDMA.*/
  printk("[DFS Check]DRAM SIZE:0x%llx\n",(unsigned long long)max_dram_size);
  printk("[DFS Check]DRAM Dummy read from:0x%llx to 0x%llx\n",(unsigned long long)(DFS_dummy_read_center_address-(BUFF_LEN >> 1)),(unsigned long long)(DFS_dummy_read_center_address+(BUFF_LEN >> 1)));
  printk("[DFS Check]DRAM Dummy read center address:0x%llx\n",(unsigned long long)DFS_dummy_read_center_address);
  printk("[DFS Check]High Memory start address 0x%llx\n",(unsigned long long)high_memory_phys);
  
  if((DFS_dummy_read_center_address - SZ_4K) >= high_memory_phys){
    printk("[DFS Check]DFS Dummy read reserved 0x%llx to 0x%llx\n",(unsigned long long)(DFS_dummy_read_center_address-SZ_4K),(unsigned long long)(DFS_dummy_read_center_address+SZ_4K));
    memblock_reserve(DFS_dummy_read_center_address-SZ_4K, (SZ_4K << 1));
    memblock_free(DFS_dummy_read_center_address-SZ_4K, (SZ_4K << 1));
    memblock_remove(DFS_dummy_read_center_address-SZ_4K, (SZ_4K << 1));
  }
  else{
#ifndef CONFIG_ARM_LPAE    
    printk("[DFS Check]DFS Dummy read reserved 0x%llx to 0x%llx\n",(unsigned long long)(DFS_dummy_read_center_address-SZ_1M),(unsigned long long)(DFS_dummy_read_center_address+SZ_1M));
    memblock_reserve(DFS_dummy_read_center_address-SZ_1M, (SZ_1M << 1));
    memblock_free(DFS_dummy_read_center_address-SZ_1M, (SZ_1M << 1));
    memblock_remove(DFS_dummy_read_center_address-SZ_1M, (SZ_1M << 1));
#else
    printk("[DFS Check]DFS Dummy read reserved 0x%llx to 0x%llx\n",(unsigned long long)(DFS_dummy_read_center_address-SZ_2M),(unsigned long long)(DFS_dummy_read_center_address+SZ_2M));
    memblock_reserve(DFS_dummy_read_center_address-SZ_2M, (SZ_2M << 1));
    memblock_free(DFS_dummy_read_center_address-SZ_2M, (SZ_2M << 1));
    memblock_remove(DFS_dummy_read_center_address-SZ_2M, (SZ_2M << 1));
#endif    
  }
 
  return;
}

void sync_hw_gating_value(void)
{
    unsigned int reg_val;
    
    reg_val = (*(volatile unsigned int *)(0xF0004028)) & (~(0x01<<30));         // cha DLLFRZ=0
    mt_reg_sync_writel(reg_val, 0xF0004028);
    reg_val = (*(volatile unsigned int *)(0xF0011028)) & (~(0x01<<30));         // chb DLLFRZ=0
    mt_reg_sync_writel(reg_val, 0xF0011028);
    
    mt_reg_sync_writel((*(volatile unsigned int *)(0xF020e374)), 0xF0004094);   // cha r0 
    mt_reg_sync_writel((*(volatile unsigned int *)(0xF020e378)), 0xF0004098);   // cha r1
    mt_reg_sync_writel((*(volatile unsigned int *)(0xF0213374)), 0xF0011094);   // chb r0  
    mt_reg_sync_writel((*(volatile unsigned int *)(0xF0213378)), 0xF0011098);   // chb r1 

    reg_val = (*(volatile unsigned int *)(0xF0004028)) | (0x01<<30);            // cha DLLFRZ=1
    mt_reg_sync_writel(reg_val, 0xF0004028);
    reg_val = (*(volatile unsigned int *)(0xF0011028)) | (0x01<<30);            // chb DLLFRZ=0
    mt_reg_sync_writel(reg_val, 0xF0011028);        
}

void disable_MR4_enable_manual_ref_rate(void)
{
    mt_reg_sync_writel((*(volatile unsigned int *)(0xF00041e8))|(1<<26), 0xF00041e8);//disable changelA MR4
    mt_reg_sync_writel((*(volatile unsigned int *)(0xF00111e8))|(1<<26), 0xF00111e8);//disable changelB MR4
  
    udelay(10);
    
    //before deepidle
    mt_reg_sync_writel((*(volatile unsigned int *)(0xF0004114))|0x80000000, 0xF0004114);  //set R_DMREFRATE_MANUAL_TRIG=1 to change refresh_rate=h3
    mt_reg_sync_writel((*(volatile unsigned int *)(0xF0011114))|0x80000000, 0xF0011114);    
   
    udelay(10);
}

void enable_MR4_disable_manual_ref_rate(void)
{
    mt_reg_sync_writel((*(volatile unsigned int *)(0xF0004114))&0x7FFFFFFF, 0xF0004114);  //After leave self refresh, set R_DMREFRATE_MANUAL_TRIG=0 
    mt_reg_sync_writel((*(volatile unsigned int *)(0xF0011114))&0x7FFFFFFF, 0xF0011114);

    udelay(10);
    
    mt_reg_sync_writel((*(volatile unsigned int *)(0xF00041e8))&~(1<<26), 0xF00041e8);//enable changelA MR4
    mt_reg_sync_writel((*(volatile unsigned int *)(0xF00111e8))&~(1<<26), 0xF00111e8);//enable changelB MR4 
}

arch_initcall(dram_test_init);
