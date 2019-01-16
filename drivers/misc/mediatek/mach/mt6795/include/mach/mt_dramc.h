#ifndef __DRAMC_H__
#define __DRAMC_H__

//#define DUMP_VCORE_TABLE

#define DMA_SRC           IOMEM((CQDMA_BASE_ADDR + 0x001C))
#define DMA_DST           IOMEM((CQDMA_BASE_ADDR + 0x0020))
#define DMA_LEN1          IOMEM((CQDMA_BASE_ADDR + 0x0024))
#define DMA_GSEC_EN       IOMEM((CQDMA_BASE_ADDR + 0x0058))
#define DMA_INT_EN        IOMEM((CQDMA_BASE_ADDR + 0x0004))
#define DMA_CON           IOMEM((CQDMA_BASE_ADDR + 0x0018))
#define DMA_START         IOMEM((CQDMA_BASE_ADDR + 0x0008))
#define DMA_INT_FLAG      IOMEM((CQDMA_BASE_ADDR + 0x0000))

#define DMA_GDMA_LEN_MAX_MASK   (0x000FFFFF)
#define DMA_GSEC_EN_BIT         (0x00000001)
#define DMA_INT_EN_BIT          (0x00000001)
#define DMA_INT_FLAG_CLR_BIT (0x00000000)

#if 0
#define CHA_DRAMCAO_BASE	0xF0004000
#define CHA_DDRPHY_BASE	0xF000F000
#define CHA_DRAMCNAO_BASE	0xF020E000
#endif

#define DRAMC_REG_MRS 0x088
#define DRAMC_REG_PADCTL4 0x0e4
#define DRAMC_REG_LPDDR2_3 0x1e0
#define DRAMC_REG_SPCMD 0x1e4
#define DRAMC_REG_ACTIM1 0x1e8
#define DRAMC_REG_RRRATE_CTL 0x1f4
#define DRAMC_REG_MRR_CTL 0x1fc
#define DRAMC_REG_SPCMDRESP 0x3b8
#define READ_DRAM_TEMP_TEST

#define PATTERN1 0x5A5A5A5A
#define PATTERN2 0xA5A5A5A5
/*Config*/
//#define APDMA_TEST
#define FREQ_HOPPING_TEST
//#define APDMAREG_DUMP
#define PHASE_NUMBER 3
#define DRAM_BASE             (0x40000000ULL)
#define BUFF_LEN   0x400
#define IOREMAP_ALIGMENT 0x1000
#define Delay_magic_num 0x295;   //We use GPT to measurement how many clk pass in 100us

typedef struct {
    unsigned int pll_setting_num;
    unsigned int freq_setting_num;    
    unsigned long low_freq_pll_setting_addr;
    unsigned long low_freq_cha_setting_addr;
    unsigned long low_freq_chb_setting_addr;
    unsigned long high_freq_pll_setting_addr;
    unsigned long high_freq_cha_setting_addr;
    unsigned long high_freq_chb_setting_addr;
} vcore_dvfs_info_t;

extern unsigned int DMA_TIMES_RECORDER;
extern phys_addr_t get_max_DRAM_size(void);

int DFS_APDMA_Enable(void);
int DFS_APDMA_Init(void);
int DFS_APDMA_END(void);
int DFS_APDMA_early_init(void);
void DFS_APDMA_dummy_read_preinit(void);
void DFS_APDMA_dummy_read_deinit(void);  
void dma_dummy_read_for_vcorefs(int loops);
void get_mempll_table_info(u32 *high_addr, u32 *low_addr, u32 *num);
void get_freq_table_info(unsigned long *high_cha_addr, unsigned long *high_chb_addr, unsigned long *low_cha_addr, unsigned long *low_chb_addr, unsigned int *num);
void updae_gating_value(unsigned int high_table, unsigned int cha_r0_fine, unsigned int cha_r1_fine, unsigned int chb_r0_fine, unsigned int chb_r1_fine);
unsigned int get_dram_data_rate(void);
unsigned int read_dram_temperature(void);
void sync_hw_gating_value(void);
void disable_MR4_enable_manual_ref_rate(void);
void enable_MR4_disable_manual_ref_rate(void);
int dram_can_support_fh(void);
int dram_do_dfs_by_fh(unsigned int freq);

#endif   /*__WDT_HW_H__*/
