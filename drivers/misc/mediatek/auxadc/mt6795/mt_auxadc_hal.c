/*****************************************************************************
 *
 * Filename:
 * ---------
 *    mt_auxadc_hal.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 *   This Module defines functions of AUXADC
 *
 * Author:
 * -------
 * Zhong Wang
 *
 ****************************************************************************/

#include <linux/init.h>        /* For init/exit macros */
#include <linux/module.h>      /* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>


#include <mach/mt_gpt.h>
#include <mach/mt_clkmgr.h>
#include <mach/sync_write.h>
#include <cust_adc.h> // generate by DCT Tool
#include "mt_auxadc_sw.h"
#include "mt_auxadc_hw.h"

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#endif

#ifdef CONFIG_OF
void __iomem *auxadc_base = NULL;
void __iomem *auxadc_apmix_base = NULL;
#endif
#define DRV_ClearBits(addr,data)     {\
   kal_uint16 temp;\
   temp = DRV_Reg16(addr);\
   temp &=~(data);\
   mt_reg_sync_writew(temp, addr);\
}

#define DRV_SetBits(addr,data)     {\
   kal_uint16 temp;\
   temp = DRV_Reg16(addr);\
   temp |= (data);\
   mt_reg_sync_writew(temp, addr);\
}

#define DRV_SetData(addr, bitmask, value)     {\
   kal_uint16 temp;\
   temp = (~(bitmask)) & DRV_Reg16(addr);\
   temp |= (value);\
   mt_reg_sync_writew(temp, addr);\
}

#define AUXADC_DRV_ClearBits16(addr, data)           DRV_ClearBits(addr,data)
#define AUXADC_DRV_SetBits16(addr, data)             DRV_SetBits(addr,data)
#define AUXADC_DRV_WriteReg16(addr, data)            mt_reg_sync_writew(data, addr)
#define AUXADC_DRV_ReadReg16(addr)                   DRV_Reg(addr)
#define AUXADC_DRV_SetData16(addr, bitmask, value)   DRV_SetData(addr, bitmask, value)

#define AUXADC_DVT_DELAYMACRO(u4Num)                                     \
{                                                                        \
    unsigned int u4Count = 0 ;                                           \
    for (u4Count = 0; u4Count < u4Num; u4Count++ );                      \
}

#define AUXADC_CLR_BITS(BS,REG)     {\
   kal_uint32 temp;\
   temp = DRV_Reg32(REG);\
   temp &=~(BS);\
   mt_reg_sync_writel(temp, REG);\
}

#define AUXADC_SET_BITS(BS,REG)     {\
   kal_uint32 temp;\
   temp = DRV_Reg32(REG);\
   temp |= (BS);\
   mt_reg_sync_writel(temp, REG);\
}

#define VOLTAGE_FULL_RANGE  1500 // VA voltage
#define AUXADC_PRECISE      4096 // 12 bits

/*****************************************************************************
 * Integrate with NVRAM
****************************************************************************/
//use efuse cali
#if 0
static kal_uint32 g_adc_ge = 0;
static kal_uint32 g_adc_oe = 0;
//static kal_uint32 g_o_vts = 0;
static kal_uint32 g_o_vbg = 0;
//static kal_uint32 g_degc_cali = 0;
static kal_uint32 g_adc_cali_en = 0;
//static kal_uint32 g_o_vts_abb = 0;
//static kal_int32 g_o_slope = 0;
//static kal_uint32 g_o_slope_sign = 0;
//static kal_uint32 g_id = 0;
static kal_uint32 g_y_vbg = 0;//defaul 1967 if cali_en=0
#endif
static DEFINE_MUTEX(mutex_get_cali_value);
static int adc_auto_set =0;

static u16 mt_tpd_read_adc(u16 pos) {
   AUXADC_DRV_SetBits16((volatile u16 *)AUXADC_TP_ADDR, pos);
   AUXADC_DRV_SetBits16((volatile u16 *)AUXADC_TP_CON0, 0x01);
   while(0x01 & AUXADC_DRV_ReadReg16((volatile u16 *)AUXADC_TP_CON0)) { ; } //wait for write finish
   return AUXADC_DRV_ReadReg16((volatile u16 *)AUXADC_TP_DATA0);
}

static void mt_auxadc_disable_penirq(void)
{
	//Turn off PENIRQ detection circuit
	AUXADC_DRV_SetBits16((volatile u16 *)AUXADC_TP_CMD, 1);
	//run once touch function
	mt_tpd_read_adc(TP_CMD_ADDR_X);
}


//step1 check con2 if auxadc is busy
//step2 clear bit
//step3  read channle and make sure old ready bit ==0
//step4 set bit  to trigger sample
//step5  read channle and make sure  ready bit ==1
//step6 read data

int IMM_auxadc_GetOneChannelValue(int dwChannel, int data[4], int* rawdata)
{
   unsigned int channel[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
   int idle_count =0;
   int data_ready_count=0;

   mutex_lock(&mutex_get_cali_value);

#ifndef CONFIG_MTK_FPGA
   if(enable_clock(MT_PDN_PERI_AUXADC,"AUXADC"))
   {
	printk("hwEnableClock AUXADC failed.");
	mutex_unlock(&mutex_get_cali_value);
	return -1;
   }
#endif
   if(dwChannel == PAD_AUX_XP)mt_auxadc_disable_penirq();
   //step1 check con2 if auxadc is busy
   while (AUXADC_DRV_ReadReg16((volatile u16 *)AUXADC_CON2) & 0x01)
   {
       printk("[adc_api]: wait for module idle\n");
       msleep(100);
	   idle_count++;
	   if(idle_count>30)
	   {
	      //wait for idle time out
	      printk("[adc_api]: wait for auxadc idle time out\n");
          mutex_unlock(&mutex_get_cali_value);
	      return -1;
	   }
   }
   // step2 clear bit
   if(0 == adc_auto_set)
   {
	   //clear bit
	   AUXADC_DRV_ClearBits16((volatile u16 *)AUXADC_CON1, (1 << dwChannel));
   }


   //step3  read channle and make sure old ready bit ==0
   while (AUXADC_DRV_ReadReg16(AUXADC_DAT0 + dwChannel * 0x04) & (1<<12))
   {
       printk("[adc_api]: wait for channel[%d] ready bit clear\n",dwChannel);
       msleep(10);
	   data_ready_count++;
	   if(data_ready_count>30)
	   {
	      //wait for idle time out
	      printk("[adc_api]: wait for channel[%d] ready bit clear time out\n",dwChannel);
		mutex_unlock(&mutex_get_cali_value);
	      return -2;
	   }
   }

   //step4 set bit  to trigger sample
   if(0==adc_auto_set)
   {
   	  AUXADC_DRV_SetBits16((volatile u16 *)AUXADC_CON1, (1 << dwChannel));
   }
   //step5  read channle and make sure  ready bit ==1
   udelay(25);//we must dealay here for hw sample cahnnel data
   while (0==(AUXADC_DRV_ReadReg16(AUXADC_DAT0 + dwChannel * 0x04) & (1<<12)))
   {
       printk("[adc_api]: wait for channel[%d] ready bit ==1\n",dwChannel);
       msleep(10);
	 data_ready_count++;

	 if(data_ready_count>30)
	 {
	      //wait for idle time out
	      printk("[adc_api]: wait for channel[%d] data ready time out\n",dwChannel);
		mutex_unlock(&mutex_get_cali_value);
	      return -3;
	 }
   }
   //step6 read data

   channel[dwChannel] = AUXADC_DRV_ReadReg16(AUXADC_DAT0 + dwChannel * 0x04) & 0x0FFF;
   if(NULL != rawdata)
   {
      *rawdata = channel[dwChannel];
   }
   //printk("[adc_api: imm mode raw data => channel[%d] = %d\n",dwChannel, channel[dwChannel]);
   //printk("[adc_api]: imm mode => channel[%d] = %d.%02d\n", dwChannel, (channel[dwChannel] * 150 / AUXADC_PRECISE / 100), ((channel[dwChannel] * 150 / AUXADC_PRECISE) % 100));
   data[0] = (channel[dwChannel] * 150 / AUXADC_PRECISE / 100);
   data[1] = ((channel[dwChannel] * 150 / AUXADC_PRECISE) % 100);

#ifndef CONFIG_MTK_FPGA
   if(disable_clock(MT_PDN_PERI_AUXADC,"AUXADC"))
   {
        printk("hwEnableClock AUXADC failed.");
		mutex_unlock(&mutex_get_cali_value);
		return -1;
   }
#endif
    mutex_unlock(&mutex_get_cali_value);

   return 0;

}

// 1v == 1000000 uv
// this function voltage Unit is uv
int IMM_auxadc_GetOneChannelValue_Cali(int Channel, int*voltage)
{
     int ret = 0, data[4], rawvalue;

     ret = IMM_auxadc_GetOneChannelValue( Channel,  data, &rawvalue);
     if(ret)
     {
	        printk("[adc_api]:IMM_auxadc_GetOneChannelValue_Cali  get raw value error %d \n",ret);
		return -1;
     }
     *voltage = rawvalue*1500000 / AUXADC_PRECISE;
      //printk("[adc_api]:IMM_auxadc_GetOneChannelValue_Cali  voltage= %d uv \n",*voltage);
      return 0;

}

#if 0
static int IMM_auxadc_get_evrage_data(int times, int Channel)
{
	int ret = 0, data[4], i, ret_value = 0, ret_temp = 0;

	i = times;
	while (i--)
	{
		ret_value = IMM_auxadc_GetOneChannelValue(Channel, data, &ret_temp);
		ret += ret_temp;
		printk("[auxadc_get_data(channel%d)]: ret_temp=%d\n",Channel,ret_temp);
	}

	ret = ret / times;
	return ret;
}
#endif

static void mt_auxadc_cal_prepare(void)
{
	//no voltage calibration
}

void mt_auxadc_hal_init(void)
{
#ifdef CONFIG_OF
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "mediatek,AUXADC");
	if (!node) {
    	printk("[AUXADC] find node failed\n");
    }
	auxadc_base = of_iomap(node, 0);
    if (!auxadc_base)
        printk("[AUXADC] base failed\n");
    printk("[AUXADC]: auxadc:0x%p\n", auxadc_base);


    node = of_find_compatible_node(NULL, NULL, "mediatek,APMIXED");
    if(node){
		/* Setup IO addresses */
		auxadc_apmix_base = of_iomap(node, 0);
		printk("[AUXADC] auxadc auxadc_apmix_base=0x%p\n",auxadc_apmix_base);
    }
    else{
        printk("[AUXADC] auxadc_apmix_base error\n");
    }

#endif

	mt_auxadc_cal_prepare();

	AUXADC_DRV_SetBits16((volatile u16 *)AUXADC_CON_RTP, 1);		//disable RTP
}

void mt_auxadc_hal_suspend(void)
{
    printk("******** MT auxadc driver suspend!! ********\n" );
#ifndef CONFIG_MTK_FPGA
    if(disable_clock(MT_PDN_PERI_AUXADC,"AUXADC"))
    	printk("hwEnableClock AUXADC failed.");
#endif
}

void mt_auxadc_hal_resume(void)
{
    printk("******** MT auxadc driver resume!! ********\n" );
#ifndef CONFIG_MTK_FPGA
	if(enable_clock(MT_PDN_PERI_AUXADC,"AUXADC"))
	    printk("hwEnableClock AUXADC failed!!!.");
#endif
	AUXADC_DRV_SetBits16((volatile u16 *)AUXADC_CON_RTP, 1);		//disable RTP
}

int mt_auxadc_dump_register(char *buf)
{
	printk("[auxadc]: AUXADC_CON0=%x\n",AUXADC_DRV_ReadReg16((volatile u16 *)AUXADC_CON0));
	printk("[auxadc]: AUXADC_CON1=%x\n",AUXADC_DRV_ReadReg16((volatile u16 *)AUXADC_CON1));
	printk("[auxadc]: AUXADC_CON2=%x\n",AUXADC_DRV_ReadReg16((volatile u16 *)AUXADC_CON2));

	return sprintf(buf, "AUXADC_CON0:%x\n AUXADC_CON1:%x\n AUXADC_CON2:%x\n",
		AUXADC_DRV_ReadReg16((volatile u16 *)AUXADC_CON0),
		AUXADC_DRV_ReadReg16((volatile u16 *)AUXADC_CON1),
		AUXADC_DRV_ReadReg16((volatile u16 *)AUXADC_CON2));
}


