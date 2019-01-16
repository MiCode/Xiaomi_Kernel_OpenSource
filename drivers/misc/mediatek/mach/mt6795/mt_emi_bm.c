#include <linux/kernel.h>
#include <asm/io.h>

#include <mach/mt_reg_base.h>
#include <mach/mt_emi_bm.h>
#include <mach/sync_write.h>
#include <mach/mt_typedefs.h>
#include <linux/of.h>
#include <linux/of_address.h>

static unsigned char g_cBWL;
static void __iomem *EMI_BASE_ADDR = NULL;
static void __iomem *DRAMCNAO_BASE_ADDR;

void BM_Init(void)
{

	  struct device_node *node;
	
	  /* DTS version */
	  node = of_find_compatible_node(NULL, NULL, "mediatek,EMI");
	  if (node) {
		  EMI_BASE_ADDR = of_iomap(node, 0);
		  pr_notice("get EMI_BASE_ADDR @ %p\n", EMI_BASE_ADDR);
	  } else {
		  pr_err("can't find compatible node\n");
		  return -1;
	  }
	  
	  node = of_find_compatible_node(NULL, NULL, "mediatek,DRAMC_NAO");
    if(node) {
      DRAMCNAO_BASE_ADDR = of_iomap(node, 0);
      printk("get DRAMCNAO_BASE_ADDR @ %p\n", DRAMCNAO_BASE_ADDR);
    }
    else {
      printk("can't find DRAMCNAO compatible node\n");
		  return -1;
	  }
	
    g_cBWL = 0;

    /*
    * make sure BW limiter counts consumed Soft-mode bandwidth of each master
    */
    if (readl(IOMEM(EMI_ARBA)) & 0x00008000) {
        g_cBWL |= 1 << 0;
        mt65xx_reg_sync_writel(readl(IOMEM(EMI_ARBA)) & ~0x00008000, EMI_ARBA);
    }

    if (readl(IOMEM(EMI_ARBB)) & 0x00008000) {
        g_cBWL |= 1 << 1;
        mt65xx_reg_sync_writel(readl(IOMEM(EMI_ARBB)) & ~0x00008000, EMI_ARBB);
    }

    if (readl(IOMEM(EMI_ARBC)) & 0x00008000) {
        g_cBWL |= 1 << 2;
        mt65xx_reg_sync_writel(readl(IOMEM(EMI_ARBC)) & ~0x00008000, EMI_ARBC);
    }

    if (readl(IOMEM(EMI_ARBD)) & 0x00008000) {
        g_cBWL |= 1 << 3;
        mt65xx_reg_sync_writel(readl(IOMEM(EMI_ARBD)) & ~0x00008000, EMI_ARBD);
    }

    if (readl(IOMEM(EMI_ARBE)) & 0x00008000) {
        g_cBWL |= 1 << 4;
        mt65xx_reg_sync_writel(readl(IOMEM(EMI_ARBE)) & ~0x00008000, EMI_ARBE);
    }
    if (readl(IOMEM(EMI_ARBF)) & 0x00008000) {
        g_cBWL |= 1 << 5;
        mt65xx_reg_sync_writel(readl(IOMEM(EMI_ARBF)) & ~0x00008000, EMI_ARBF);
    }
    if (readl(IOMEM(EMI_ARBG)) & 0x00008000) {
        g_cBWL |= 1 << 6;
        mt65xx_reg_sync_writel(readl(IOMEM(EMI_ARBG)) & ~0x00008000, EMI_ARBG);
    }
    if (readl(IOMEM(EMI_ARBH)) & 0x00008000) {
        g_cBWL |= 1 << 7;
        mt65xx_reg_sync_writel(readl(IOMEM(EMI_ARBH)) & ~0x00008000, EMI_ARBH);
    }

}

void BM_DeInit(void)
{
    if (g_cBWL & (1 << 0)) {
        g_cBWL &= ~(1 << 0);
        mt65xx_reg_sync_writel(readl(IOMEM(EMI_ARBA)) | 0x00008000, EMI_ARBA);
    }

    if (g_cBWL & (1 << 1)) {
        g_cBWL &= ~(1 << 1);
        mt65xx_reg_sync_writel(readl(IOMEM(EMI_ARBB)) | 0x00008000, EMI_ARBB);
    }

    if (g_cBWL & (1 << 2)) {
        g_cBWL &= ~(1 << 2);
        mt65xx_reg_sync_writel(readl(IOMEM(EMI_ARBC)) | 0x00008000, EMI_ARBC);
    }

    if (g_cBWL & (1 << 3)) {
        g_cBWL &= ~(1 << 3);
        mt65xx_reg_sync_writel(readl(IOMEM(EMI_ARBD)) | 0x00008000, EMI_ARBD);
    }

    if (g_cBWL & (1 << 4)) {
        g_cBWL &= ~(1 << 4);
        mt65xx_reg_sync_writel(readl(IOMEM(EMI_ARBE)) | 0x00008000, EMI_ARBE);
    }

    if (g_cBWL & (1 << 5)) {
	 g_cBWL &= ~(1 << 5);
	mt65xx_reg_sync_writel(readl(IOMEM(EMI_ARBF)) | 0x00008000, EMI_ARBF);
    }

    if (g_cBWL & (1 << 6)) {
         g_cBWL &= ~(1 << 6);
        mt65xx_reg_sync_writel(readl(IOMEM(EMI_ARBG)) | 0x00008000, EMI_ARBG);
    }

    if (g_cBWL & (1 << 7)) {
	 g_cBWL &= ~(1 << 7);
	mt65xx_reg_sync_writel(readl(IOMEM(EMI_ARBH)) | 0x00008000, EMI_ARBH);
    }

}

void BM_Enable(const unsigned int enable)
{
    const unsigned int value = readl(IOMEM(EMI_BMEN));

    mt65xx_reg_sync_writel((value & ~(BUS_MON_PAUSE | BUS_MON_EN)) | (enable ? BUS_MON_EN : 0), EMI_BMEN);
}

/*
void BM_Disable(void)
{
    const unsigned int value = readl(EMI_BMEN);

    mt65xx_reg_sync_writel(value & (~BUS_MON_EN), EMI_BMEN);
}
*/

void BM_Pause(void)
{
    const unsigned int value = readl(IOMEM(EMI_BMEN));

    mt65xx_reg_sync_writel(value | BUS_MON_PAUSE, EMI_BMEN);
}

void BM_Continue(void)
{
    const unsigned int value = readl(IOMEM(EMI_BMEN));

    mt65xx_reg_sync_writel(value & (~BUS_MON_PAUSE), EMI_BMEN);
}

unsigned int BM_IsOverrun(void)
{
    /*
    * return 0 if EMI_BCNT(bus cycle counts) or EMI_WACT(total word counts) is overrun,
    * otherwise return an !0 value
    */
    const unsigned int value = readl(IOMEM(EMI_BMEN));

    return (value & BC_OVERRUN);
}

void BM_SetReadWriteType(const unsigned int ReadWriteType)
{
    const unsigned int value = readl(IOMEM(EMI_BMEN));

    /*
    * ReadWriteType: 00/11 --> both R/W
    *                   01 --> only R
    *                   10 --> only W
    */
    mt65xx_reg_sync_writel((value & 0xFFFFFFCF) | (ReadWriteType << 4), EMI_BMEN);
}

int BM_GetBusCycCount(void)
{
    return BM_IsOverrun() ? BM_ERR_OVERRUN : readl(IOMEM(EMI_BCNT));
}

unsigned int BM_GetTransAllCount(void)
{    
    return readl(IOMEM(EMI_TACT));
}

int BM_GetTransCount(const unsigned int counter_num)
{
    unsigned int iCount;

    switch (counter_num) {
    case 1:
        iCount = readl(IOMEM(EMI_TSCT));
        break;

    case 2:
        iCount = readl(IOMEM(EMI_TSCT2));
        break;

    case 3:
        iCount = readl(IOMEM(EMI_TSCT3));
        break;

    default:
        return BM_ERR_WRONG_REQ;
    }

    return iCount;
}

int BM_GetWordAllCount(void)
{
    return BM_IsOverrun() ? BM_ERR_OVERRUN : readl(IOMEM(EMI_WACT));
}

int BM_GetWordCount(const unsigned int counter_num)
{
    unsigned int iCount;

    switch (counter_num) {
    case 1:
        iCount = readl(IOMEM(EMI_WSCT));
        break;

    case 2:
        iCount = readl(IOMEM(EMI_WSCT2));
        break;

    case 3:
        iCount = readl(IOMEM(EMI_WSCT3));
        break;

    case 4:
        iCount = readl(IOMEM(EMI_WSCT4));
        break;

    default:
        return BM_ERR_WRONG_REQ;
    }

    return iCount;
}

unsigned int BM_GetBandwidthWordCount(void)
{
    return readl(IOMEM(EMI_BACT));
}

unsigned int BM_GetOverheadWordCount(void)
{
    return readl(IOMEM(EMI_BSCT));
}

int BM_GetTransTypeCount(const unsigned int counter_num)
{
    return (counter_num < 1 || counter_num > BM_COUNTER_MAX) ? BM_ERR_WRONG_REQ : readl(IOMEM(EMI_TTYPE1 + (counter_num - 1) * 8));
}

int BM_SetMonitorCounter(const unsigned int counter_num, const unsigned int master, const unsigned int trans_type)
{
    unsigned int value;
    unsigned long addr;
    const unsigned int iMask = 0xFFFF;

    if (counter_num < 1 || counter_num > BM_COUNTER_MAX) {
        return BM_ERR_WRONG_REQ;
    }

    if (counter_num == 1) {
        addr = EMI_BMEN;
        value = (readl(IOMEM(addr)) & ~(iMask << 16)) | ((trans_type & 0xFF) << 24) | ((master & 0xFF) << 16);
    }else {
        addr = (counter_num <= 3) ? EMI_MSEL : (EMI_MSEL2 + (counter_num / 2 - 2) * 8);

        // clear master and transaction type fields
        value = readl(IOMEM(addr)) & ~(iMask << ((counter_num % 2) * 16));

        // set master and transaction type fields
        value |= (((trans_type & 0xFF) << 8) | (master & 0xFF)) << ((counter_num % 2) * 16);
    }

    mt65xx_reg_sync_writel(value, addr);

    return BM_REQ_OK;
}

int BM_SetMaster(const unsigned int counter_num, const unsigned int master)
{
    unsigned int value;
    unsigned long addr;
    const unsigned int iMask = 0xFF;

    if (counter_num < 1 || counter_num > BM_COUNTER_MAX) {
        return BM_ERR_WRONG_REQ;
    }

    if (counter_num == 1) {
        addr = EMI_BMEN;
        value = (readl(IOMEM(addr)) & ~(iMask << 16)) | ((master & iMask) << 16);
    }else {
        addr = (counter_num <= 3) ? EMI_MSEL : (EMI_MSEL2 + (counter_num / 2 - 2) * 8);

        // clear master and transaction type fields
        value = readl(IOMEM(addr)) & ~(iMask << ((counter_num % 2) * 16));

        // set master and transaction type fields
        value |= ((master & iMask) << ((counter_num % 2) * 16));
    }

    mt65xx_reg_sync_writel(value, addr);

    return BM_REQ_OK;
}

int BM_SetIDSelect(const unsigned int counter_num, const unsigned int id, const unsigned int enable)
{
    unsigned int value, shift_num;
    unsigned long addr;

    if ((counter_num < 1 || counter_num > BM_COUNTER_MAX)
        || (id > 0x1FFF)
        || (enable > 1)) {
        return BM_ERR_WRONG_REQ;
    }

    addr = EMI_BMID0 + ((counter_num - 1) / 2) * 4;

    // field's offset in the target EMI_BMIDx register
    shift_num = ((counter_num - 1) % 2) * 16;

    // clear SELx_ID field
    value = readl(IOMEM(addr)) & ~(0x1FFF << shift_num);

    // set SELx_ID field
    value |= id << shift_num;

    mt65xx_reg_sync_writel(value, addr);

    value = (readl(IOMEM(EMI_BMEN2)) & ~(1 << (counter_num - 1))) | (enable << (counter_num - 1));

    mt65xx_reg_sync_writel(value, EMI_BMEN2);

    return BM_REQ_OK;
}

int BM_SetUltraHighFilter(const unsigned int counter_num, const unsigned int enable)
{
    unsigned int value;

    if ((counter_num < 1 || counter_num > BM_COUNTER_MAX)
        || (enable > 1)) {
        return BM_ERR_WRONG_REQ;
    }

    value = (readl(IOMEM(EMI_BMEN1)) & ~(1 << (counter_num - 1))) | (enable << (counter_num - 1));

    mt65xx_reg_sync_writel(value, EMI_BMEN1);

    return BM_REQ_OK;
}

int BM_SetLatencyCounter(void)
{
  unsigned int value;
  value = readl(IOMEM(EMI_BMEN2)) & ~(0b11 << 24);
  //emi_ttype1 -- emi_ttype7 change as total latencies for m0 -- m6, and emi_ttype9 -- emi_ttype15 change as total transaction counts for m0 -- m6
  value |= (0b10 << 24);
  mt65xx_reg_sync_writel(value, EMI_BMEN2);
  return BM_REQ_OK;
}

int BM_GetLatencyCycle(const unsigned int counter_num)
{
  unsigned int cycle_count;
  
  switch(counter_num)
  {
    case 1:
      cycle_count = readl(IOMEM(EMI_TTYPE1));
      break;
    case 2:
      cycle_count = readl(IOMEM(EMI_TTYPE2));
      break;
    case 3:
      cycle_count = readl(IOMEM(EMI_TTYPE3));
      break;  
    case 4:
      cycle_count = readl(IOMEM(EMI_TTYPE4));
      break;
    case 5:
      cycle_count = readl(IOMEM(EMI_TTYPE5));
      break;
    case 6:
      cycle_count = readl(IOMEM(EMI_TTYPE6));
      break;
    case 7:
      cycle_count = readl(IOMEM(EMI_TTYPE7));
      break;	  
    case 9:
      cycle_count = readl(IOMEM(EMI_TTYPE9));
      break;
    case 10:
      cycle_count = readl(IOMEM(EMI_TTYPE10));
      break;
    case 11:
      cycle_count = readl(IOMEM(EMI_TTYPE11));
      break;
    case 12:
      cycle_count = readl(IOMEM(EMI_TTYPE12));
      break;
    case 13:
      cycle_count = readl(IOMEM(EMI_TTYPE13));
      break;          
    case 14:
	cycle_count = readl(IOMEM(EMI_TTYPE14));
	break;	
    case 15:
	cycle_count = readl(IOMEM(EMI_TTYPE15));
	break;	
    case 16:
	cycle_count = readl(IOMEM(EMI_TTYPE16));
	break;		
    default:
      return BM_ERR_WRONG_REQ;  
  }
  return cycle_count;
}

int BM_GetEmiDcm(void)
{
    return ((readl(IOMEM(EMI_CONM)) >> 24) ? 1 : 0);
}

int BM_SetEmiDcm(const unsigned int setting)
{
    unsigned int value;
        
    value = readl(IOMEM(EMI_CONM));
    mt65xx_reg_sync_writel( (value & 0x00FFFFFF) | (setting << 24), EMI_CONM);

    return BM_REQ_OK;
}

unsigned int DRAMC_GetPageHitCount(DRAMC_Cnt_Type CountType)
{
    unsigned int iCount;

    switch (CountType) {
    case DRAMC_R2R:
        iCount = readl(IOMEM(DRAMC_R2R_PAGE_HIT));
        break;

    case DRAMC_R2W:
        iCount = readl(IOMEM(DRAMC_R2W_PAGE_HIT));
        break;

    case DRAMC_W2R:
        iCount = readl(IOMEM(DRAMC_W2R_PAGE_HIT));
        break;

    case DRAMC_W2W:
        iCount = readl(IOMEM(DRAMC_W2W_PAGE_HIT));
        break;
    case DRAMC_ALL:
        iCount = readl(IOMEM(DRAMC_R2R_PAGE_HIT)) + readl(IOMEM(DRAMC_R2W_PAGE_HIT)) +
                 readl(IOMEM(DRAMC_W2R_PAGE_HIT)) + readl(IOMEM(DRAMC_W2W_PAGE_HIT));
        break;
    default:
        return BM_ERR_WRONG_REQ;
    }

    return iCount;
}

unsigned int DRAMC_GetPageMissCount(DRAMC_Cnt_Type CountType)
{
    unsigned int iCount;

    switch (CountType) {
    case DRAMC_R2R:
        iCount = readl(IOMEM(DRAMC_R2R_PAGE_MISS));
        break;

    case DRAMC_R2W:
        iCount = readl(IOMEM(DRAMC_R2W_PAGE_MISS));
        break;

    case DRAMC_W2R:
        iCount = readl(IOMEM(DRAMC_W2R_PAGE_MISS));
        break;

    case DRAMC_W2W:
        iCount = readl(IOMEM(DRAMC_W2W_PAGE_MISS));
        break;
    case DRAMC_ALL:
        iCount = readl(IOMEM(DRAMC_R2R_PAGE_MISS)) + readl(IOMEM(DRAMC_R2W_PAGE_MISS)) +
                 readl(IOMEM(DRAMC_W2R_PAGE_MISS)) + readl(IOMEM(DRAMC_W2W_PAGE_MISS));
        break;
    default:
        return BM_ERR_WRONG_REQ;
    }

    return iCount;
}

unsigned int DRAMC_GetInterbankCount(DRAMC_Cnt_Type CountType)
{
    unsigned int iCount;

    switch (CountType) {
    case DRAMC_R2R:
        iCount = readl(IOMEM(DRAMC_R2R_INTERBANK));
        break;

    case DRAMC_R2W:
        iCount = readl(IOMEM(DRAMC_R2W_INTERBANK));
        break;

    case DRAMC_W2R:
        iCount = readl(IOMEM(DRAMC_W2R_INTERBANK));
        break;

    case DRAMC_W2W:
        iCount = readl(IOMEM(DRAMC_W2W_INTERBANK));
        break;
    case DRAMC_ALL:
        iCount = readl(IOMEM(DRAMC_R2R_INTERBANK)) + readl(IOMEM(DRAMC_R2W_INTERBANK)) +
                 readl(IOMEM(DRAMC_W2R_INTERBANK)) + readl(IOMEM(DRAMC_W2W_INTERBANK));
        break;
    default:
        return BM_ERR_WRONG_REQ;
    }

    return iCount;
}

unsigned int DRAMC_GetIdleCount(void)
{
    return readl(IOMEM(DRAMC_IDLE_COUNT));
}
