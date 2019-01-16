
#include <linux/mu3d/hal/mu3d_hal_osal.h>
#include <linux/mu3d/test_drv/mu3d_test_usb_drv.h>
#include <linux/mu3d/hal/mu3d_hal_qmu_drv.h>
#include <linux/mu3d/hal/mu3d_hal_hw.h>
#include <linux/mu3d/hal/mu3d_hal_usb_drv.h>
#define _MTK_PHY_EXT_
#include <linux/mu3d/hal/mu3d_hal_phy.h>
#undef _MTK_PHY_EXT_
#include <linux/mu3d/test_drv/mu3d_test_qmu_drv.h>
#define _USB_UNIFIED_H_
#include <linux/mu3d/test_drv/mu3d_test_unified.h>
#undef _USB_UNIFIED_H_
#include <linux/mu3d/hal/ssusb_sifslv_ippc_c_header.h>
#include <linux/kthread.h>

#include <mach/pmic_mt6331_6332_sw.h>
#include <cust_pmic.h>

#ifdef CONFIG_PROJECT_PHY
#include <linux/mu3phy/mtk-phy-asic.h>
#endif

#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
// USBIF
#include <mach/battery_common.h>

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

DEV_UINT16 g_nirq;
#define USB_DEV_COMPATIBLE_NAME "mediatek,USB3"
void get_resource_setting(void){
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, USB_DEV_COMPATIBLE_NAME);

	if(node == NULL){
		os_printk(K_ERR, "USB get node failed\n");
		return -1;
	}

	g_nirq = irq_of_parse_and_map(node, 0);
}


int main_thread(void *data);
int otg_hnp_req_role_b_thread(void *data);
int otg_hnp_back_role_b_thread(void *data);
int otg_hnp_req_role_a_thread(void *data);
int otg_hnp_back_role_a_thread(void *data);
int otg_srp_thread(void *data);
int otg_a_uut_thread(void *data);
int otg_b_uut_thread(void *data);
int otg_opt20_a_uut_thread(void *data);
int otg_opt20_b_uut_thread(void *data);
int set_srp_session(int is_on);



DEV_INT32 otg_top(int argc, char** argv);


extern int usbif_u3d_test_send_event(char *event) ;
/**
 * u3w - i2c write function
 *@args - arg1: addr, arg2: value
 */
DEV_INT32 u3w(DEV_INT32 argc, DEV_INT8**argv){
	#ifdef CONFIG_U3_PHY_GPIO_SUPPORT
	DEV_UINT32 u4TimingValue;
	DEV_UINT8 u1TimingValue;
	DEV_UINT32 u4TimingAddress;

	if (argc<3)
    {
        os_printk(K_EMERG,"Arg: address value\n");
        return RET_FAIL;
    }

	u4TimingAddress = (DEV_UINT32)simple_strtol(argv[1], &argv[1], 16);
	u4TimingValue = (DEV_UINT32)simple_strtol(argv[2], &argv[2], 16);
	u1TimingValue = u4TimingValue & 0xff;

	_U3Write_Reg(u4TimingAddress,u1TimingValue);
	#endif

	return RET_SUCCESS;
}

/**
 * u3r - i2c read function
 *@args - arg1: addr
 */
DEV_INT32 u3r(DEV_INT32 argc, DEV_INT8**argv){
	#ifdef CONFIG_U3_PHY_GPIO_SUPPORT
	DEV_UINT8 u1ReadTimingValue;
	DEV_UINT32 u4TimingAddress;

	if (argc<2)
    {
        os_printk(K_EMERG, "Arg: address\n");
        return 0;
    }
	u4TimingAddress = (DEV_UINT32)simple_strtol(argv[1], &argv[1], 16);
	u1ReadTimingValue = _U3Read_Reg(u4TimingAddress);
	printk("Value = 0x%x\n", u1ReadTimingValue);
	#endif

	return 0;
}

/**
 * u3init - u3 phy pipe initial settings
 *
 */
DEV_INT32 u3init(DEV_INT32 argc, DEV_INT8**argv){
	if (!u3phy_ops)
		u3phy_init();

	u3phy_ops->init(u3phy);

	return 0;
}

/**
 * u3d_linkup - u3d link up
 *
 */
DEV_INT32 u3d_linkup(DEV_INT32 argc, DEV_INT8 **argv){
	DEV_INT32 latch,count,status;

	latch = 0;
	if(argc > 1){
		latch = (DEV_UINT32)simple_strtol(argv[1], &argv[1], 16);
	}

	mu3d_hal_link_up(latch);
	os_ms_delay(500);
	count = 10;
	status = RET_SUCCESS;
	do{
		if((os_readl(U3D_LINK_STATE_MACHINE)&LTSSM)!=STATE_U0_STATE){
			status = RET_FAIL;
			break;
		}
		os_ms_delay(50);
	}while(count--);

	if(status != RET_SUCCESS){
		printk("&&&&&& LINK UP FAIL !!&&&&&&\n");
	}else{
		printk("&&&&&& LINK UP PASS !!&&&&&&\n");
	}
	return 0;
}

/**
 * U3D_Phy_Cfg_Cmd - u3 phy clock phase scan
 *
 */
DEV_INT32 U3D_Phy_Cfg_Cmd(DEV_INT32 argc, DEV_INT8 **argv){

	DEV_INT32 latch_val;

	latch_val = 0;
	if(argc > 1){
		latch_val = (DEV_UINT32)simple_strtol(argv[1], &argv[1], 16);
	}
	
	mu3d_hal_phy_scan(latch_val, 2);

	return 0;
}


DEV_INT32 dbg_phy_eyeinit(int argc, char** argv){
	u3phy_ops->eyescan_init(u3phy);

	return RET_SUCCESS;
}

DEV_INT32 dbg_phy_eyescan(int argc, char** argv){
	if (argc > 10)
	{
		u3phy_ops->eyescan(
			u3phy,
			simple_strtol(argv[1], &argv[1], 10), //x_t1
			simple_strtol(argv[2], &argv[2], 10), //y_t1
			simple_strtol(argv[3], &argv[3], 10), //x_br
			simple_strtol(argv[4], &argv[4], 10), //y_br
			simple_strtol(argv[5], &argv[5], 10), //delta_x
			simple_strtol(argv[6], &argv[6], 10), //delta_y
			simple_strtol(argv[7], &argv[7], 10), //eye_cnt
			simple_strtol(argv[8], &argv[8], 10), //num_cnt
			simple_strtol(argv[9], &argv[9], 10), //PI_cal_en
			simple_strtol(argv[10], &argv[10], 10) //num_ignore_cnt
			);

		return RET_SUCCESS;
	}
	else
	{
		return RET_FAIL;
	}
};


#define EP0_SRAM_SIZE 512
#define EPN_TX_SRAM_SIZE 6144
#define EPN_RX_SRAM_SIZE 6144

void sram_write(DEV_UINT32 mode, DEV_UINT32 addr, DEV_UINT32 data)
{
	DEV_UINT8 index = 0;
	#ifdef SUPPORT_U3
	DEV_UINT32 port[] = {EP0_SRAM_DEBUG_MODE, EPNTX_SRAM_DEBUG_MODE, EPNRX_SRAM_DEBUG_MODE};
	#else
	DEV_UINT32 port[] = {EP0_SRAM_DEBUG_MODE, EP0_SRAM_DEBUG_MODE, EP0_SRAM_DEBUG_MODE};
	#endif
	DEV_UINT32 fifo[] = {U3D_FIFO0, U3D_FIFO1, U3D_FIFO1};

	//set debug mode
	os_writel(U3D_SRAM_DBG_CTRL, port[index]);

	if (mode == EP0_SRAM_DEBUG_MODE)
		index = 0;
	else if (mode == EPNTX_SRAM_DEBUG_MODE)
		index = 1;
	else if (mode == EPNRX_SRAM_DEBUG_MODE)		index = 2;
	else
		BUG_ON(1);

	//set increment to 1024
	os_writelmsk(U3D_SRAM_DBG_CTRL_1, 10<<SRAM_DEBUG_FIFOSEGSIZE_OFST, SRAM_DEBUG_FIFOSEGSIZE);
	os_writelmsk(U3D_SRAM_DBG_CTRL_1, (addr/1024)<<SRAM_DEBUG_SLOT_OFST, SRAM_DEBUG_SLOT);
	os_writelmsk(U3D_SRAM_DBG_CTRL_1, (addr%1024)<<SRAM_DEBUG_DP_COUNT_OFST, SRAM_DEBUG_DP_COUNT);

	os_writel(fifo[index], data);

	//disable debug mode
	os_writel(U3D_SRAM_DBG_CTRL, 0);
}

DEV_UINT32 sram_read(DEV_UINT32 mode, DEV_UINT32 addr)
{
	DEV_UINT8 index;
	DEV_UINT32 temp;
	#ifdef SUPPORT_U3
	DEV_UINT32 port[] = {EP0_SRAM_DEBUG_MODE, EPNTX_SRAM_DEBUG_MODE, EPNRX_SRAM_DEBUG_MODE};
	#else
	DEV_UINT32 port[] = {EP0_SRAM_DEBUG_MODE, EP0_SRAM_DEBUG_MODE, EP0_SRAM_DEBUG_MODE};
	#endif
	DEV_UINT32 fifo[] = {U3D_FIFO0, U3D_FIFO1, U3D_FIFO1};

	if (mode == EP0_SRAM_DEBUG_MODE)
		index = 0;
	else if (mode == EPNTX_SRAM_DEBUG_MODE)
		index = 1;
	else if (mode == EPNRX_SRAM_DEBUG_MODE)
		index = 2;
	else
		BUG_ON(1);

	//set debug mode
	os_writel(U3D_SRAM_DBG_CTRL, port[index]);

	//set increment to 1024
	os_writelmsk(U3D_SRAM_DBG_CTRL_1, 10<<SRAM_DEBUG_FIFOSEGSIZE_OFST, SRAM_DEBUG_FIFOSEGSIZE);
	os_writelmsk(U3D_SRAM_DBG_CTRL_1, (addr/1024)<<SRAM_DEBUG_SLOT_OFST, SRAM_DEBUG_SLOT);
	os_writelmsk(U3D_SRAM_DBG_CTRL_1, (addr%1024)<<SRAM_DEBUG_DP_COUNT_OFST, SRAM_DEBUG_DP_COUNT);
	os_printk(K_ERR, "U3D_SRAM_DBG_CTRL: %x\n", os_readl(U3D_SRAM_DBG_CTRL));
	os_printk(K_ERR, "U3D_SRAM_DBG_CTRL_1: %x\n", os_readl(U3D_SRAM_DBG_CTRL_1));

	temp = os_readl(fifo[index]);

	//disable debug mode
	os_writel(U3D_SRAM_DBG_CTRL, 0);

	return temp;
}

void sram_dbg(void)
{
	DEV_UINT8 *ptr;
	DEV_UINT16 i, j, addr, data;
	DEV_UINT32 value[] = {0xaaaaaaaa, 0x55555555, 0x5a5a5a5a, 0xa5a5a5a5, 0x0, 0xffffffff, 0x12345678, 0x87654321};
	DEV_UINT32 size[] = {EP0_SRAM_SIZE, EPN_TX_SRAM_SIZE, EPN_RX_SRAM_SIZE};
	DEV_UINT32 en[] = {EP0_SRAM_DEBUG_MODE, EPNTX_SRAM_DEBUG_MODE, EPNRX_SRAM_DEBUG_MODE};
	DEV_UINT32 fifo[] = {U3D_FIFO0, U3D_FIFO1, U3D_FIFO1};


	//set increment to 1024
	os_writelmsk(U3D_SRAM_DBG_CTRL_1, 10<<SRAM_DEBUG_FIFOSEGSIZE_OFST, SRAM_DEBUG_FIFOSEGSIZE);

	for (j = 0; j < sizeof(en)/sizeof(en[0]); j++)
	{
		os_writel(U3D_SRAM_DBG_CTRL, en[j]);
		os_printk(K_ERR, "test mode %d\n", j);

		for (addr = 0; addr < size[j]; addr = addr + 4)
		//addr = 0x100;
		{
			//addr = dp_count + slot * 2^10
			os_writel(U3D_RISC_SIZE, RISC_SIZE_4B);
			os_writelmsk(U3D_SRAM_DBG_CTRL_1, (addr/1024)<<SRAM_DEBUG_SLOT_OFST, SRAM_DEBUG_SLOT);
			os_writelmsk(U3D_SRAM_DBG_CTRL_1, (addr%1024)<<SRAM_DEBUG_DP_COUNT_OFST, SRAM_DEBUG_DP_COUNT);

			for (i = 0; i < sizeof(value)/sizeof(value[0]); i++)
			{
				//test 4-byte access
				os_writel(U3D_RISC_SIZE, RISC_SIZE_4B);
				os_writel(fifo[j], value[i]);
				if (os_readl(fifo[j]) != value[i])
				{
					os_printk(K_ERR, "[4-byte access]Write addr %x with value %x fail, got %x\n", addr, value[i], os_readl(fifo[j]));
				}


				//test 2-byte write access
				ptr = (DEV_UINT8 *)&value[i];
				//WORD only access for the following code
				os_writel(U3D_RISC_SIZE, RISC_SIZE_2B);
				os_writel(fifo[j], (*ptr | *(ptr+1)<<8));
				os_writelmsk(U3D_SRAM_DBG_CTRL_1, ((addr+2)%1024)<<SRAM_DEBUG_DP_COUNT_OFST, SRAM_DEBUG_DP_COUNT);
				os_writel(fifo[j], (*(ptr+2) | *(ptr+3)<<8));
				//restore to DWORD access
				os_writel(U3D_RISC_SIZE, RISC_SIZE_4B);
				os_writelmsk(U3D_SRAM_DBG_CTRL_1, (addr%1024)<<SRAM_DEBUG_DP_COUNT_OFST, SRAM_DEBUG_DP_COUNT);
				if (value[i] != os_readl(fifo[j]))
				{
					os_printk(K_ERR, "[2-byte access]Write addr %x with value %x fail, got %x\n", addr, value[i], os_readl(fifo[j]));
				}

				//test 1-byte access
				ptr = (DEV_UINT8 *)&value[i];
				//BYTE only access for the following code
				os_writel(U3D_RISC_SIZE, RISC_SIZE_1B);
				os_writel(fifo[j], *ptr);
				os_writelmsk(U3D_SRAM_DBG_CTRL_1, ((addr+1)%1024)<<SRAM_DEBUG_DP_COUNT_OFST, SRAM_DEBUG_DP_COUNT);
				os_writel(fifo[j], *(ptr+1));
				os_writelmsk(U3D_SRAM_DBG_CTRL_1, ((addr+2)%1024)<<SRAM_DEBUG_DP_COUNT_OFST, SRAM_DEBUG_DP_COUNT);
				os_writel(fifo[j], *(ptr+2));
				os_writelmsk(U3D_SRAM_DBG_CTRL_1, ((addr+3)%1024)<<SRAM_DEBUG_DP_COUNT_OFST, SRAM_DEBUG_DP_COUNT);
				os_writel(fifo[j], *(ptr+3));
				//restore to DWORD access
				os_writel(U3D_RISC_SIZE, RISC_SIZE_4B);
				os_writelmsk(U3D_SRAM_DBG_CTRL_1, (addr%1024)<<SRAM_DEBUG_DP_COUNT_OFST, SRAM_DEBUG_DP_COUNT);
				if (value[i] != os_readl(fifo[j]))
				{
					os_printk(K_ERR, "[1-byte access]Write addr %x with value %x fail, got %x\n", addr, value[i], os_readl(fifo[j]));
					while(1);
				}
			}
		}


		//write serial to every 4-byte, and then read back for comparison
		os_writel(U3D_RISC_SIZE, RISC_SIZE_4B);
		for (addr = 0; addr < size[j]; addr = addr + 4)
		{
			data = addr+0x100;

			//addr = dp_count + slot * 2^10
			os_writelmsk(U3D_SRAM_DBG_CTRL_1, (addr/1024)<<SRAM_DEBUG_SLOT_OFST, SRAM_DEBUG_SLOT);
			os_writelmsk(U3D_SRAM_DBG_CTRL_1, (addr%1024)<<SRAM_DEBUG_DP_COUNT_OFST, SRAM_DEBUG_DP_COUNT);

			os_writel(fifo[j], data);
		}

		for (addr = 0; addr < size[j]; addr = addr + 4)
		{
			data = addr+0x100;

			//addr = dp_count + slot * 2^10
			os_writelmsk(U3D_SRAM_DBG_CTRL_1, (addr/1024)<<SRAM_DEBUG_SLOT_OFST, SRAM_DEBUG_SLOT);
			os_writelmsk(U3D_SRAM_DBG_CTRL_1, (addr%1024)<<SRAM_DEBUG_DP_COUNT_OFST, SRAM_DEBUG_DP_COUNT);

			if (os_readl(fifo[j]) != data)
			{
				os_printk(K_ERR, "addr %x data has been modified, should be %x, got %x\n", addr, data, os_readl(fifo[j]));
			}
		}
	}
}
extern void mu3d_mtk_test_wakelock_lock(void) ;

extern void mu3d_mtk_test_wakelock_unlock(void) ;


DEV_UINT8 g_first_init = 1;
#ifdef SUPPORT_OTG
DEV_INT32 TS_AUTO_TEST(DEV_INT32 argc, DEV_INT8** argv){
	if (g_run)
		os_printk(K_ERR, "main_thread is already running\n");
	
	kthread_run(main_thread, NULL, "main_thread\n");

	return RET_SUCCESS;
}

int main_thread(void *data){
#else
DEV_INT32 TS_AUTO_TEST(DEV_INT32 argc, DEV_INT8** argv){
#endif
	EP_INFO *ep_info;
	REST_INFO *reset;
	LOOPBACK_INFO *lb_info;
	STRESS_INFO *st_info;
	RANDOM_STOP_INFO *rs_info;
	STOP_QMU_INFO *sq_info;
	RX_ZLP_INFO *cz_info;
	DEV_NOTIF_INFO *notif_info;
	SINGLE_INFO *sg_info;
	POWER_INFO *pw_info;
	U1U2_INFO *u1u2_info;
	LPM_INFO *lpm_info;
	STALL_INFO *stall_info;
	REMOTE_WAKE_INFO *remote_wake_info;
	CTRL_MODE_INFO *ctrl_mode_info;
	OTG_MODE_INFO *otg_mode_info;
	unsigned long flags;
    USB_SPEED speed = SSUSB_SPEED_HIGH;
	DEV_UINT32 i,stop_count_1,stop_count_2;
	DEV_UINT8 EP_TX[16],EP_RX[16],st_num,delay,run_stop,run_test,ran_num,method;
	static DEV_INT32 tx_num,rx_num;
	DEV_UINT8 dir_1=USB_RX,dir_2=USB_RX,ep_num,ep_num1,ep_num2,dir,ep_n[15];


	get_resource_setting();

	printk("[U3D_T] TS_AUTO_TEST ===> \n") ;

	mu3d_mtk_test_wakelock_lock() ;
	
	g_run = 1;
	g_usb_phy_clk_on = 0 ;

	//dump HW/SW version
	os_printk(K_EMERG, "HW VERSION: %x\n", os_readl(U3D_SSUSB_HW_ID));
	os_printk(K_EMERG, "HW SUB VERSION: %x\n", os_readl(U3D_SSUSB_HW_SUB_ID));
	os_printk(K_EMERG, "SW VERSION: %s\n", SW_VERSION);

	//dump HW capability
	os_printk(K_EMERG, "TX EP: %x, RX EP: %x\n",
		os_readl(U3D_CAP_EPINFO) & CAP_TX_EP_NUM,
		(os_readl(U3D_CAP_EPINFO) & CAP_RX_EP_NUM) >> CAP_RX_EP_NUM_OFST);
	os_printk(K_EMERG, "EP0 FIFO: 0x%x\n", os_readl(U3D_CAP_EP0FFSZ));
	os_printk(K_EMERG, "TX FIFO: 0x%x\n", os_readl(U3D_CAP_EPNTXFFSZ));
	os_printk(K_EMERG, "RX FIFO: 0x%x\n", os_readl(U3D_CAP_EPNRXFFSZ));


	if (g_first_init)
	{
		printk("[U3D_T] TS_AUTO_TEST ===> g_first_init\n") ;
		//apply probe setting to detect register R/W hang problem
		os_writel(U3D_SSUSB_PRB_CTRL1, 0x000c0000);
		os_writel(U3D_SSUSB_PRB_CTRL2, 0x00170016);
		//os_writel(U3D_SSUSB_PRB_CTRL3, 0x0f0f0f0f);
		os_writel(U3D_SSUSB_PRB_CTRL3, 0xb0b0b0b0);

		//IDDIG function does not work on FPGA. Need to set B role manually
		#ifdef SUPPORT_OTG
		// USBIF , if no FPGA ? 
		// os_setmsk(U3D_SSUSB_OTG_STS, SSUSB_CHG_B_ROLE_B);
		#endif


		g_dma_buffer_size=STRESS_DATA_LENGTH*MAX_GPD_NUM;

		for(i=0;i<(sizeof(g_loopback_buffer)/sizeof(g_loopback_buffer[0]));i++){
			g_loopback_buffer[i] = NULL;
		}
		
		g_loopback_buffer[0] = (DEV_UINT8 *)os_mem_alloc(g_dma_buffer_size);
		g_loopback_buffer[1] = (DEV_UINT8 *)os_mem_alloc(g_dma_buffer_size);
		g_loopback_buffer[2] = (DEV_UINT8 *)os_mem_alloc(g_dma_buffer_size);
		g_loopback_buffer[3] = (DEV_UINT8 *)os_mem_alloc(g_dma_buffer_size);

		os_printk(K_CRIT,"STRESS_GPD_TH : %d\n",STRESS_GPD_TH);
		os_printk(K_CRIT,"g_dma_buffer_size : %d\n",g_dma_buffer_size);
		os_printk(K_CRIT,"g_loopback_buffer[0] : %p\n", g_loopback_buffer[0]);
		os_printk(K_CRIT,"g_loopback_buffer[1] : %p\n", g_loopback_buffer[1]);
		os_printk(K_CRIT,"g_loopback_buffer[2] : %p\n", g_loopback_buffer[2]);
		os_printk(K_CRIT,"g_loopback_buffer[3] : %p\n", g_loopback_buffer[3]);

		ep_info=(EP_INFO *)os_mem_alloc(sizeof(EP_INFO));

		#if (EP0_BUS_MODE==DMA_MODE)
		g_ep0_mode = DMA_MODE;
		#else
		g_ep0_mode = PIO_MODE;
		#endif

		/* initialize PHY related data structure */
		// USBIF os_printk(K_ERR, "PHY_INIT here\n");
		u3phy_init();

		/* USB 2.0 slew rate calibration */
		u3phy_ops->u2_slew_rate_calibration(u3phy);

		// USBIF OTG
		//#ifdef SUPPORT_OTG
		//u3phy_ops->init(u3phy);
		//#endif		

		/* initialize U3D mem resources. */
		// USBIF,WCD u3d_init();
		u3d_init_mem();

		g_first_init = 0;

		spin_lock_init(&g_otg_lock) ;
	}

	printk("[U3D_T] TS_AUTO_TEST ===> u3d_init\n") ;

// USBIF OTG
	#ifdef SUPPORT_OTG
	mt_set_gpio_pull_enable(GPIO_OTG_DRVVBUS_PIN, GPIO_PULL_DISABLE);
	mt_set_gpio_mode(GPIO_OTG_DRVVBUS_PIN, GPIO_OTG_DRVVBUS_PIN_M_USB_DRVVBUS);
	
	u3phy_ops->init(u3phy);
	#endif		
	
	u3d_init();

	os_memset(EP_RX, 0 , 16);
	os_memset(EP_TX, 0 , 16);
	rx_num=0;
	tx_num=0;


	u3d_rst_request();
#if 0 	
	while(g_run){
		if(u3d_req_valid()){
			os_printk(K_INFO, "[START STATE %d]\n", u3d_command());
			switch(u3d_command()){

				case RESET_STATE:
					//TODO: make sure all DMA actions are stopped
					os_printk(K_INFO,"RESET_STATE\n");
					#if (BUS_MODE==QMU_MODE)
					for(i=0;i<MAX_EP_NUM;i++){
						if(EP_RX[i]!=0){
							mu3d_hal_flush_qmu(EP_RX[i],USB_RX);
						}
						if(EP_TX[i]!=0){
							mu3d_hal_flush_qmu(EP_TX[i],USB_TX);
						}
					}
					#endif
					reset=(REST_INFO *)u3d_req_buffer();
					os_printk(K_DEBUG,"reset->speed : 0x%08X\n",reset->speed);

					//device speed is specified after reset
					reset_dev(reset->speed & 0xf, 1,
						(reset->speed & 0x10) ? 0 : 1);
					tx_num=0;
					rx_num=0;
					speed = reset->speed;
					break;


				case CONFIG_EP_STATE:
					os_printk(K_INFO,"CONFIG_EP_STATE\n");

					ep_info=(EP_INFO *)u3d_req_buffer();
					os_printk(K_ERR,"ep_info->ep_num :%x\r\n",ep_info->ep_num);
					os_printk(K_ERR,"ep_info->dir :%x\r\n",ep_info->dir);
					os_printk(K_ERR,"ep_info->type :%x\r\n",ep_info->type);
					os_printk(K_ERR,"ep_info->ep_size :%d\r\n",ep_info->ep_size);
					os_printk(K_ERR,"ep_info->interval :%x\r\n",ep_info->interval);
					os_printk(K_ERR,"ep_info->slot :%d\r\n",ep_info->slot);
					os_printk(K_ERR,"ep_info->burst :%d\r\n",ep_info->burst);
					os_printk(K_ERR,"ep_info->mult :%d\r\n",ep_info->mult);

					mu3d_hal_ep_enable(ep_info->ep_num, ep_info->dir, ep_info->type, ep_info->ep_size, ep_info->interval,ep_info->slot,ep_info->burst,ep_info->mult);

					#if (BUS_MODE==QMU_MODE)
					mu3d_hal_start_qmu(ep_info->ep_num, ep_info->dir);
					#endif
					if(ep_info->dir==USB_RX){
						EP_RX[rx_num]=ep_info->ep_num;
						rx_num++;
					}else{
						EP_TX[tx_num]=ep_info->ep_num;
						tx_num++;
					}
					if(speed == SSUSB_SPEED_SUPER){
						dev_power_mode(0, 0, 0,1, 1);
					}
					os_printk(K_ERR,"EP_RX :%x\r\n",EP_RX[0]);
					os_printk(K_ERR,"EP_TX :%x\r\n",EP_TX[0]);
					break;


				case LOOPBACK_STATE:
					os_printk(K_INFO,"LOOPBACK_STATE\n");
					lb_info=(LOOPBACK_INFO *)u3d_req_buffer();
					TransferLength=lb_info->transfer_length;
					is_bdp=lb_info->bdp;
					gpd_buf_size=lb_info->gpd_buf_size;
					bd_buf_size=lb_info->bd_buf_size;
					bDramOffset=lb_info->dram_offset;
					bBD_Extension=lb_info->extension;
					bGPD_Extension=lb_info->extension;
					bdma_burst=lb_info->dma_burst;
					bdma_limiter=lb_info->dma_limiter;

					#if (BUS_MODE==QMU_MODE)
					os_printk(K_INFO,"TransferLength :%d\r\n",TransferLength);
					os_printk(K_INFO,"gpd_buf_size :%x\r\n",gpd_buf_size);
					os_printk(K_INFO,"bd_buf_size :%x\r\n",bd_buf_size);
					os_printk(K_INFO,"is_bdp :%x\r\n",is_bdp);
					os_printk(K_INFO,"bBD_Extension :%x\r\n",bBD_Extension);
					os_printk(K_INFO,"bGPD_Extension :%x\r\n",bGPD_Extension);
					os_printk(K_INFO,"bDramOffset :%x\r\n",bDramOffset);
					//dev_set_dma_busrt_limiter(bdma_burst,bdma_limiter);
					dev_qmu_loopback(EP_RX[0],EP_TX[0]);
					#else
					os_printk(K_INFO,"TransferLength :%d\r\n",TransferLength);
					u3d_dev_loopback(EP_RX[0],EP_TX[0]);
					#endif
					break;


				case LOOPBACK_EXT_STATE:
					os_printk(K_INFO,"LOOPBACK_EXT_STATE\n");

					lb_info=(LOOPBACK_INFO *)u3d_req_buffer();
					TransferLength=lb_info->transfer_length;
					is_bdp=lb_info->bdp;
					gpd_buf_size=lb_info->gpd_buf_size;
					bd_buf_size=lb_info->bd_buf_size;
					bDramOffset=lb_info->dram_offset;
					bBD_Extension=lb_info->extension;
					bGPD_Extension=lb_info->extension;

					os_printk(K_INFO,"TransferLength :%d\r\n",TransferLength);
					os_printk(K_INFO,"gpd_buf_size :%x\r\n",gpd_buf_size);
					os_printk(K_INFO,"bd_buf_size :%x\r\n",bd_buf_size);
					os_printk(K_INFO,"is_bdp :%x\r\n",is_bdp);
					os_printk(K_INFO,"bBD_Extension :%x\r\n",bBD_Extension);
					os_printk(K_INFO,"bGPD_Extension :%x\r\n",bGPD_Extension);
					os_printk(K_INFO,"bDramOffset :%x\r\n",bDramOffset);

					dev_qmu_loopback_ext(EP_RX[0],EP_TX[0]);
					break;


				case REMOTE_WAKEUP:
					remote_wake_info = (REMOTE_WAKE_INFO *)u3d_req_buffer();
					os_printk(K_INFO, "REMOTE_WAKEUP\n");
					printk("remote wakeup suspend=%d\n",g_usb_status.suspend);
					while(!u3d_dev_suspend());
					printk("remote wakeup suspend=%d\n",g_usb_status.suspend);
					os_ms_delay(10);
					os_us_delay(remote_wake_info->delay);
					mu3d_hal_resume();
					if(g_u3d_setting.speed == SSUSB_SPEED_SUPER)
						dev_notification(1, 0, 0); //send FUNCTION WAKE UP

					os_printk(K_INFO,"resume+\n");
					while(u3d_dev_suspend());
					os_printk(K_INFO,"resume-\n");
					break;


				case STRESS:
					os_printk(K_CRIT,"STRESS\n");
					st_info=(STRESS_INFO *)u3d_req_buffer();
					TransferLength=st_info->transfer_length;
					is_bdp=st_info->bdp;
					gpd_buf_size=st_info->gpd_buf_size;
					bd_buf_size=st_info->bd_buf_size;
					st_num=st_info->num;

					bDramOffset=0;
					bBD_Extension=0;
					bGPD_Extension=0;


					g_dma_buffer_size=TransferLength*MAX_GPD_NUM;

					for(i=0;i<st_num;i++){
						// USBIF 
						if (g_loopback_buffer[i] == NULL){
							g_loopback_buffer[i] = (DEV_UINT8 *)os_mem_alloc(g_dma_buffer_size);
						}
					}

					g_run_stress = true;
					g_insert_hwo = true;

					for(i=0;i<st_num;i++){
					 	dev_prepare_stress_gpd(MAX_GPD_NUM,USB_RX,EP_RX[i],g_loopback_buffer[i]);
					 	dev_prepare_stress_gpd(MAX_GPD_NUM,USB_TX,EP_TX[i],g_loopback_buffer[i]);
					}

					for(i=0;i<st_num;i++){
						dev_insert_stress_gpd_hwo(USB_RX, EP_RX[i]);
						dev_start_stress(USB_RX,EP_RX[i]);
					}
					break;


				case WARM_RESET: //fall down on purpose
					os_printk(K_INFO,"WARM_RESET\n");


				case STALL:
					os_printk(K_INFO,"STALL\n");
					stall_info=(STALL_INFO *)u3d_req_buffer();
					TransferLength=stall_info->transfer_length;
					gpd_buf_size=stall_info->gpd_buf_size;
					bd_buf_size=stall_info->bd_buf_size;
					is_bdp=stall_info->bdp;

				 	dev_tx_rx(EP_RX[0],EP_TX[0]);
				 	g_u3d_status = READY;
					break;


				case EP_RESET_STATE:
					os_printk(K_INFO,"EP_RESET_STATE\n");
				 	g_u3d_status = dev_ep_reset();
					break;


				case SINGLE:
					os_printk(K_INFO,"SINGLE\n");
					sg_info=(SINGLE_INFO *)u3d_req_buffer();


					os_printk(K_INFO,"sg_info->transfer_length = %d\n",sg_info->transfer_length);
					os_printk(K_INFO,"sg_info->gpd_buf_size = %d\n",sg_info->gpd_buf_size);
					os_printk(K_INFO,"sg_info->bd_buf_size = %d\n",sg_info->bd_buf_size);
					os_printk(K_INFO,"sg_info->dir = %d\n",sg_info->dir);


					TransferLength=sg_info->transfer_length;
					gpd_buf_size=sg_info->gpd_buf_size;
					bd_buf_size=sg_info->bd_buf_size;
					g_run_stress = true;
					dir=sg_info->dir;
					st_num=sg_info->num;
					if(bd_buf_size){
						is_bdp=1;
					}
					for(i=0;i<st_num;i++){
						if(dir==USB_RX){
							ep_n[i] = EP_RX[i];
						}
						else{
							ep_n[i] = EP_TX[i];
						}
					}
					os_printk(K_INFO,"SINGLE 01\n");

					for(i=0;i<st_num;i++){
						dev_prepare_gpd(STRESS_GPD_TH,dir,ep_n[i],g_loopback_buffer[0]);
					}

					for(i=0;i<st_num;i++){
						dev_start_stress(dir,ep_n[i]);
					}
					break;


				case POWER_STATE:
					pw_info=(POWER_INFO *)u3d_req_buffer();
					if(pw_info->mode == 5){
						dev_send_one_packet(EP_TX[0]);
					}
					dev_power_mode(pw_info->mode, pw_info->u1_value, pw_info->u2_value,pw_info->en_u1, pw_info->en_u2);
					break;


				case U1U2_STATE:
					u1u2_info=(U1U2_INFO *)u3d_req_buffer();
					dev_u1u2_en_cond(u1u2_info->opt,u1u2_info->cond,EP_RX[0] , EP_TX[0]);
					dev_u1u2_en_ctrl(u1u2_info->type,u1u2_info->u_num,u1u2_info->opt,u1u2_info->cond,u1u2_info->u1_value,u1u2_info->u2_value);
					dev_send_erdy(u1u2_info->opt ,EP_RX[0] , EP_TX[0]);
					dev_receive_ep0_test_packet(u1u2_info->opt);
					break;


				case LPM_STATE:
					lpm_info=(LPM_INFO *)u3d_req_buffer();
					#if !LPM_STRESS
					dev_u1u2_en_cond(lpm_info->cond+1, lpm_info->cond_en, EP_RX[0], EP_TX[0]);
					#endif
					dev_lpm_config_dev(lpm_info);

					g_u3d_status = READY;
					break;


				case RANDOM_STOP_STATE:
					os_printk(K_INFO,"RANDOM_STOP_STATE\n");
					rs_info=(RANDOM_STOP_INFO *)u3d_req_buffer();
					TransferLength=rs_info->transfer_length;
					gpd_buf_size=rs_info->gpd_buf_size;
					bd_buf_size=rs_info->bd_buf_size;
					dir_1=rs_info->dir_1;
					dir_2=rs_info->dir_2;
					stop_count_1=rs_info->stop_count_1;
					stop_count_2=rs_info->stop_count_2;
					is_bdp = (!bd_buf_size) ? false : true;
					bDramOffset=0;
					bBD_Extension=0;
					bGPD_Extension=0;

					os_printk(K_CRIT,"TransferLength :%d\r\n",TransferLength);
					os_printk(K_CRIT,"gpd_buf_size :%x\r\n",gpd_buf_size);
					os_printk(K_CRIT,"bd_buf_size :%x\r\n",bd_buf_size);
					os_printk(K_CRIT,"is_bdp :%x\r\n",is_bdp);
					os_printk(K_CRIT,"dir_1 :%x\r\n",dir_1);
					os_printk(K_CRIT,"dir_2 :%x\r\n",dir_2);
					os_printk(K_CRIT,"stop_count_1 :%x\r\n",stop_count_1);
					os_printk(K_CRIT,"stop_count_2 :%x\r\n",stop_count_2);
					os_printk(K_CRIT,"EP_RX :%x  EP_TX :%x\r\n",EP_RX[0],EP_TX[0]);

					g_run_stress = true;
					g_insert_hwo = false;

					if(dir_1==USB_RX){
						ep_num1 = EP_RX[0];
						dev_prepare_gpd(STRESS_GPD_TH,USB_RX,ep_num1,g_loopback_buffer[0]);
					}
					else{
						ep_num1 = EP_TX[0];
						dev_prepare_gpd(STRESS_GPD_TH,USB_TX,ep_num1,g_loopback_buffer[0]);
					}
					if(dir_2==USB_RX){
						ep_num2 = (dir_1==USB_RX) ? EP_RX[1] : EP_RX[0];
						dev_prepare_gpd(STRESS_GPD_TH,USB_RX,ep_num2,g_loopback_buffer[0]);
					}
					else{
						ep_num2 = (dir_1==USB_TX) ? EP_TX[1] : EP_TX[0];
						dev_prepare_gpd(STRESS_GPD_TH,USB_TX,ep_num2,g_loopback_buffer[0]);
					}


					dev_start_stress(dir_1,ep_num1);
					dev_start_stress(dir_2,ep_num2);
					run_test = true;
					while(run_test)
					{

						os_get_random_bytes(&ran_num,1);
						if((ran_num%3)==1){
							if(stop_count_1){
								ep_num = ep_num1;
								stop_count_1--;
								dir = dir_1;
								run_stop = true;
							}
						}
						if((ran_num%3)==2){

							if(stop_count_2){
								ep_num = ep_num2;
								stop_count_2--;
								dir = dir_2;
								run_stop = true;
							}
						}
						if(!(ran_num%3)){
							os_get_random_bytes(&delay,1);
							os_ms_delay(delay);
						}
						if(run_stop){
							g_u3d_status = BUSY;
							os_printk(K_CRIT,"ep_num :%x\r\n",ep_num);
							os_printk(K_CRIT,"dir :%x\r\n",dir);
							os_get_random_bytes(&delay,1);
							os_ms_delay(delay);
							mu3d_hal_send_stall(ep_num,dir);
							mu3d_hal_flush_qmu(ep_num,dir);

							//os_ms_delay(100);
							mu3d_hal_restart_qmu(ep_num,dir);
							dev_prepare_gpd(STRESS_GPD_TH,dir,ep_num,g_loopback_buffer[0]);
							dev_start_stress(dir,ep_num);
							g_u3d_status = READY;
							run_stop = false;
							os_printk(K_CRIT,"stop_count_1 :%x\r\n",stop_count_1);
							os_printk(K_CRIT,"stop_count_2 :%x\r\n",stop_count_2);

							for(i=0;i<RANDOM_STOP_DELAY;i++){
								os_ms_delay(100);
							}
						}
						if((stop_count_1==0)&&(stop_count_2==0)){
							run_test = false;
							os_printk(K_CRIT,"End !!\r\n");
						}

					}
					break;


				case STOP_QMU_STATE:
					os_printk(K_INFO,"STOP_QMU_STATE\n");
					sq_info=(STOP_QMU_INFO *)u3d_req_buffer();
					TransferLength=sq_info->transfer_length;
					gpd_buf_size=sq_info->gpd_buf_size;
					bd_buf_size=sq_info->bd_buf_size;
					method=sq_info->tx_method;

					is_bdp = (!bd_buf_size) ? false : true;
					bDramOffset=0;
					bBD_Extension=0;
					bGPD_Extension=0;
					is_bdp = 1;
					bd_buf_size = 1000;
					os_printk(K_CRIT,"TransferLength :%d\r\n",TransferLength);
					os_printk(K_CRIT,"gpd_buf_size :%x\r\n",gpd_buf_size);
					os_printk(K_CRIT,"bd_buf_size :%x\r\n",bd_buf_size);
					os_printk(K_CRIT,"is_bdp :%x\r\n",is_bdp);
					os_printk(K_CRIT,"dir_1 :%x\r\n",dir_1);
					os_printk(K_CRIT,"dir_2 :%x\r\n",dir_2);
					os_printk(K_CRIT,"EP_RX :%x  EP_TX :%x\r\n",EP_RX[0],EP_TX[0]);

					os_printk(K_CRIT,"method :%x\r\n",method);

					g_run_stress = true;

					dev_prepare_gpd(STRESS_GPD_TH,USB_RX,EP_RX[0],g_loopback_buffer[0]);
					dev_prepare_gpd(STRESS_GPD_TH,USB_TX,EP_TX[0],g_loopback_buffer[0]);
					dev_prepare_gpd(STRESS_GPD_TH,USB_TX,EP_TX[1],g_loopback_buffer[0]);
					dev_start_stress(USB_RX,EP_RX[0]);
					dev_start_stress(USB_TX,EP_TX[0]);
					dev_start_stress(USB_TX,EP_TX[1]);


					//for(stop_count_1=0;stop_count_1<1000;stop_count_1++){
					while(1){
						/*1.Random delay*/
						os_get_random_bytes(&delay,1);
						os_ms_delay((delay));

						/*2.Stop QMU*/
						mu3d_hal_restart_qmu_no_flush(EP_RX[0], USB_RX,0);

						/*3.Random delay*/
						os_get_random_bytes(&delay,1);
						os_ms_delay((delay));
						spin_lock_irqsave(&_lock, flags);

						/*4.Insert GPD*/
						dev_prepare_gpd(STRESS_GPD_TH,USB_RX,EP_RX[0],g_loopback_buffer[0]);

						/*5.Resume QMU*/
						dev_start_stress(USB_RX,EP_RX[0]);
						spin_unlock_irqrestore(&_lock, flags);
						os_ms_delay(20);

						/*1.Random delay*/
						os_get_random_bytes(&delay,1);
						os_ms_delay(delay);

						/*2.Stop QMU*/
						mu3d_hal_restart_qmu_no_flush(EP_TX[0], USB_TX,method);

						/*3.Random delay*/
						os_get_random_bytes(&delay,1);
						os_ms_delay(delay);

						spin_lock_irqsave(&_lock, flags);

						/*4.Insert GPD*/
						if(method==1){
							dev_prepare_gpd(STRESS_GPD_TH,USB_TX,EP_TX[0],g_loopback_buffer[0]);
						}
						if(method==2){
							//dev_prepare_gpd((STRESS_GPD_TH-1),USB_TX,EP_TX[0],g_loopback_buffer[0]);
							dev_prepare_gpd_short(STRESS_GPD_TH,USB_TX,EP_TX[0],g_loopback_buffer[0]);
							//dev_prepare_gpd(STRESS_GPD_TH,USB_TX,EP_TX[0],g_loopback_buffer[0]);


						}
						dev_start_stress(USB_TX,EP_TX[0]);
						spin_unlock_irqrestore(&_lock, flags);
						os_ms_delay(20);
					
					}
					break;


				case RX_ZLP_STATE:
					os_printk(K_ERR,"RX_ZLP_STATE\n");
					cz_info=(RX_ZLP_INFO *)u3d_req_buffer();
					TransferLength=cz_info->transfer_length;
					gpd_buf_size=cz_info->gpd_buf_size;
					bd_buf_size=cz_info->bd_buf_size;
					is_bdp = (!bd_buf_size) ? false : true;
					bDramOffset=0;
					bBD_Extension=0;
					bGPD_Extension=0;
				 	cfg_rx_zlp_en = cz_info->zlp_en;
 					cfg_rx_coz_en = cz_info->coz_en;
					rx_done_count = 0;
					rx_IOC_count = 0;
					if(!TransferLength){
						TransferLength++;
					}


					dev_qmu_rx(EP_RX[0]);
					os_printk(K_CRIT,"rx_zlp:[%d], rx_coz:[%d], rx_done_count:[%d], rx_IOC_count:[%d]\r\n",cfg_rx_zlp_en,cfg_rx_coz_en,rx_done_count,rx_IOC_count);
					os_printk(K_CRIT,"done\r\n");
					break;


				 case DEV_NOTIFICATION_STATE:
					os_printk(K_ERR,"DEV_NOTIFICATION_STATE\n");
					notif_info=(DEV_NOTIF_INFO *)u3d_req_buffer();
					dev_notification(notif_info->type,notif_info->valuel,notif_info->valueh);
					break;


				 case STOP_DEV_STATE:
					os_printk(K_ERR,"STOP_DEV_STATE\n");

					reset_dev(U3D_DFT_SPEED, 0, 1);

					//wait for the current control xfer to finish
					os_ms_delay(1000);

					return RET_SUCCESS;

				 case CTRL_MODE_STATE:
					os_printk(K_ERR,"CTRL_MODE_STATE\n");
					ctrl_mode_info=(CTRL_MODE_INFO *)u3d_req_buffer();

					g_ep0_mode = ctrl_mode_info->mode;					
				 	break;


				case OTG_MODE_STATE:
					os_printk(K_ERR,"OTG_MODE_STATE\n");
					otg_mode_info=(OTG_MODE_INFO *)u3d_req_buffer();
					dev_otg(otg_mode_info->mode);
					break;
			}
			os_printk(K_INFO, "[END STATE %d]\n", u3d_command());

				u3d_rst_request();
			}

		//os_ms_sleep(1);
	}
#endif
	os_printk(K_ERR, "%s Done\n", __func__);
	
	return RET_SUCCESS;
}

DEV_INT32 TS_AUTO_TEST_STOP(DEV_INT32 argc, DEV_INT8** argv){
	g_run = 0;

	// USBIF
	os_ms_sleep(1000);
	
	u3d_deinit();

	mu3d_mtk_test_wakelock_unlock() ;

	mt_set_gpio_mode(GPIO_OTG_DRVVBUS_PIN, GPIO_MODE_GPIO);
	mt_set_gpio_pull_select(GPIO_OTG_DRVVBUS_PIN, GPIO_PULL_DOWN);
	mt_set_gpio_pull_enable(GPIO_OTG_DRVVBUS_PIN, GPIO_PULL_ENABLE);
	
	os_printk(K_ERR, "stop\n");
	
	return RET_SUCCESS;
}

void otg_init(void)
{	
	// USBIF, WARN
	//g_otg_config = 0;
	//g_otg_srp_reqd = 0;
	//g_otg_hnp_reqd = 0;

	mb();

	g_otg_vbus_chg = 0;	
	g_otg_reset = 0;	
	g_otg_suspend = 0;	
	g_otg_resume = 0;
	g_otg_connect = 0;
	g_otg_disconnect = 0;	
	g_otg_chg_a_role_b = 0;
	g_otg_chg_b_role_b = 0;
	g_otg_attach_b_role = 0;
	g_otg_hnp_reqd = 0;
	g_otg_b_hnp_enable = 0;
	
	mb();

	os_printk(K_DEBUG, "clear OTG events done, g_otg_config %d\n", g_otg_config);
}

#ifdef SUPPORT_OTG
DEV_INT32 otg_top(int argc, char** argv)
{
	DEV_UINT8 speed;
	DEV_UINT8 otg_mode = 0;
	
	if (argc >= 2)
	{
		otg_mode = (DEV_UINT32)simple_strtol(argv[1], &argv[1], 10);
		os_printk(K_ERR, "OTG_MODE: %x\n", otg_mode);
	}
	if (argc >= 3)
	{
		speed = (DEV_UINT32)simple_strtol(argv[2], &argv[2], 10);
		os_printk(K_ERR, "SPEED: %x\n", speed);
	}

	otg_init();
	switch (otg_mode)
	{
		case 0:
			os_printk(K_ERR, "1: set iddig_b\n");
			os_printk(K_ERR, "2: change speed\n");
			os_printk(K_ERR, "3: otg_hnp_req_thread_role_b\n");
			os_printk(K_ERR, "4: otg_hnp_back_thread_role_b\n");
			os_printk(K_ERR, "5: otg_hnp_req_thread_role_a\n");
			os_printk(K_ERR, "6: otg_hnp_back_thread_role_a\n");
			os_printk(K_ERR, "7: otg_srp_thread\n");
			os_printk(K_ERR, "8: otg_a_uut_thread\n");
			os_printk(K_ERR, "9: otg_b_uut_thread\n");
			os_printk(K_ERR, "10: stop otg thread\n");
			os_printk(K_ERR, "11: td 5.9\n");
			os_printk(K_ERR, "12: SRP\n");
			os_printk(K_ERR, "13: detect vbus\n");			
			break;
		case 1:
			os_setmsk(U3D_SSUSB_OTG_STS, SSUSB_ATTACH_B_ROLE);
			break;
		case 2:
			reset_dev((speed == 0) ? SSUSB_SPEED_FULL : SSUSB_SPEED_HIGH, 0, 1);
			break;
		case 3: //HNP request role B
			kthread_run(otg_hnp_req_role_b_thread, NULL, "otg_hnp_req_thread_role_b\n");
			break;
		case 4: //HNP back role B
			kthread_run(otg_hnp_back_role_b_thread, NULL, "otg_hnp_back_thread_role_b\n");
			break;
		case 5: //HNP request role A
			kthread_run(otg_hnp_req_role_a_thread, NULL, "otg_hnp_req_thread_role_a\n");
			break;
		case 6: //HNP back role A
			kthread_run(otg_hnp_back_role_a_thread, NULL, "otg_hnp_back_thread_role_a\n");
			break;
		case 7: //SRP
			kthread_run(otg_srp_thread, NULL, "otg_srp_thread\n");
			break;	
		case 8: //A-UUT
			//kthread_run(otg_a_uut_thread, NULL, "dev_otg_a_uut_thread\n");
			kthread_run(otg_opt20_a_uut_thread, NULL, "dev_otg_opt20_a_uut_thread\n");				
			break;
		case 9: //B-UUT
			//kthread_run(otg_b_uut_thread, NULL, "dev_otg_b_uut_thread\n");		
			kthread_run(otg_opt20_b_uut_thread, NULL, "dev_otg_opt20_b_uut_thread\n");		
			break;
		case 10: //stop A-UUT/B-UUT thread
			g_otg_exec = 0;
			break;
		case 11: //TD 5.9
			g_otg_td_5_9 = 1;
			break;
		case 12:
			os_setmsk(U3D_POWER_MANAGEMENT, SOFT_CONN); 
			os_setmsk(U3D_DEVICE_CONTROL, SESSION);
			break;
		case 13:
			os_setmsk(U3D_POWER_MANAGEMENT, SOFT_CONN); 
			os_setmsk(U3D_DEVICE_CONTROL, SESSION);

			g_otg_exec = 1;
			while (g_otg_exec && ((os_readl(U3D_DEVICE_CONTROL) & VBUS) != 0x18))
				os_ms_sleep(100);
			if (!g_otg_exec)
				return RET_SUCCESS;

			mu3d_hal_set_speed(SSUSB_SPEED_HIGH);				
			break;
		case 14:
			set_srp_session(1) ;
			break ;
		case 15:
			set_srp_session(0) ;
			break ;

	}

    return RET_SUCCESS;
}

int otg_hnp_req_role_b_thread(void *data){
	os_printk(K_ERR, "%s\n", __func__);
	os_printk(K_ERR, "Enable host request\n");
	os_setmsk(U3D_DEVICE_CONTROL, HOSTREQ);

	os_printk(K_ERR, "Wait for suspend\n");
	while (!g_otg_suspend)
		os_ms_sleep(1);

	os_printk(K_ERR, "Request HNP\n");
	//os_clrmsk(U3D_SSUSB_U2_CTRL_0P, SSUSB_U2_PORT_PDN);

	os_printk(K_ERR, "Wait to become host (CHG_A_ROLE_B)\n");
	while (!(g_otg_chg_a_role_b && (os_readl(U3D_SSUSB_OTG_STS) & SSUSB_HOST_DEV_MODE)))
		os_ms_sleep(1);

	os_printk(K_ERR, "Set CHG_A_ROLE_A\n");
	os_setmsk(U3D_SSUSB_OTG_STS, SSUSB_CHG_A_ROLE_A);

	os_printk(K_ERR, "%s Done\n", __func__);
	return 0 ;
}

int otg_hnp_back_role_b_thread(void *data){
	os_printk(K_ERR, "%s\n", __func__);
	os_printk(K_ERR, "Wait to become device (CHG_B_ROLE_B)\n");
	while (!(g_otg_chg_b_role_b && !(os_readl(U3D_SSUSB_OTG_STS) & SSUSB_HOST_DEV_MODE)))
		os_ms_sleep(1); 

	os_printk(K_ERR, "Waiting for reset\n");
	while (!g_otg_reset)
		os_ms_sleep(1); 

	os_printk(K_ERR, "%s Done\n", __func__);
	return 0 ;
}

int otg_hnp_req_role_a_thread(void *data){
	os_printk(K_ERR, "%s\n", __func__);
	os_printk(K_ERR, "Wait CHG_B_ROLE_B\n");
	while (!(g_otg_chg_b_role_b && !(os_readl(U3D_SSUSB_OTG_STS) & SSUSB_HOST_DEV_MODE)))
		os_ms_sleep(1); 

	#if 0
	os_printk(K_ERR, "Clear CHG_B_ROLE_B\n");
	os_setmsk(U3D_SSUSB_OTG_STS_CLR, SSUSB_CHG_B_ROLE_B_CLR);
	#endif

	os_printk(K_ERR, "Waiting for reset\n");
	while (!g_otg_reset)
		os_ms_sleep(1); 

	os_printk(K_ERR, "%s Done\n", __func__);
	return 0;
}

int otg_hnp_back_role_a_thread(void *data){
	os_printk(K_ERR, "%s\n", __func__);
	os_printk(K_ERR, "Wait to become host (CHG_A_ROLE_B)\n");
	while (!(g_otg_chg_a_role_b && (os_readl(U3D_SSUSB_OTG_STS) & SSUSB_HOST_DEV_MODE)))
		os_ms_sleep(1); 

	os_printk(K_ERR, "Set CHG_A_ROLE_A\n");
	os_setmsk(U3D_SSUSB_OTG_STS, SSUSB_CHG_A_ROLE_A);

	os_printk(K_ERR, "%s Done\n", __func__);
	return 0;
}

int set_srp_session(int is_on){
	os_printk(K_ERR, "%s\n", __func__);

	os_printk(K_ERR, "Set session\n");
	if (is_on){
		os_setmsk(U3D_DEVICE_CONTROL, SESSION);
	}else{
		os_clrmsk(U3D_DEVICE_CONTROL, SESSION);
	}
	return 0;
}

//USBIF OTG 
int otg_srp_thread(void *data){
	os_printk(K_ERR, "%s\n", __func__);
	os_printk(K_ERR, "Wait for A-valid to drop\n");
	while (!(g_otg_vbus_chg && !(os_readl(U3D_SSUSB_OTG_STS) & SSUSB_AVALID_STS)))
		os_ms_sleep(1); 

	os_printk(K_ERR, "Delay 1s\n");
	os_ms_delay(1000);
	
	os_printk(K_ERR, "Wait for session to drop\n");	
	while(os_readl(U3D_DEVICE_CONTROL) & SESSION);

	os_printk(K_ERR, "Set session\n");
	os_setmsk(U3D_DEVICE_CONTROL, SESSION);

	os_printk(K_ERR, "Wait for reset\n");
	while (!g_otg_reset);

	os_printk(K_ERR, "%s Done\n", __func__);
	return 0;
}

////////////////////////////////////////////////////////////////////////////////

//interrupt polling mode
#define OTG_EVENT_CONNECT 0x1
#define OTG_EVENT_CONNECT_OR_RESUME 0x2
#define OTG_EVENT_RESET 0x3

//interrupt polling return code
#define OTG_OK 0x0
#define OTG_TEST_STOP    0xfff1
#define OTG_NOT_CONNECT 0xfff2
#define OTG_HNP_TMOUT 0xfff3
#define OTG_NOT_RESET   0xfff4
#define OTG_RESUME 0xfff5
#define OTG_DISCONNECT 0xfff6

//interrupt polling timeout
#define OTG_A_OPT_DRIVE_VBUS_TMOUT	5*1000
#define OTG_CONNECT_TMOUT 15*1000
#define OTG_CONNECT_OR_RESUME_TMOUT 2*1000
#define OTG_RESET_TMOUT 2*1000

//OTG message code
DEV_UINT8 err_msg;

#define OTG_MSG_DEV_NOT_SUPPORT     0x01
#define OTG_MSG_DEV_NOT_RESPONSE    0x02
#define OTG_MSG_HUB_NOT_SUPPORT     0x03

extern int PMIC_IMM_GetOneChannelValue(upmu_adc_chl_list_enum dwChannel, int deCount, int trimd);

extern void usb_phy_recover(unsigned int clk_on);
extern void usb_phy_savecurrent(unsigned int clk_on);

unsigned int u3d_get_vbus_mV(void)
{
	unsigned int vbus_value_raw;
	DEV_INT8 devctl ;
	
	//vbus_value_raw = PMIC_IMM_GetOneChannelValue(ADC_VCHRIN_AP, 1, 0);
	vbus_value_raw = PMIC_IMM_GetOneChannelValue(ADC_VUSB_AP , 1, 0);
	//vbus_value_raw = PMIC_IMM_GetOneChannelValue(ADC_ADCVIN0_AP , 1, 0);
	
	devctl = os_readl(U3D_DEVICE_CONTROL) ;
			

	//os_printk(K_ERR, "[U3D_T]u3d_get_vbus_mV : raw: %d , devctl : 0x%x\n", vbus_value_raw, devctl );

	return vbus_value_raw ;
}
DEV_INT32 otg_wait_event(DEV_UINT8 mode)
{
	DEV_INT32 cnt;

	if (mode == OTG_EVENT_CONNECT)
		cnt = OTG_CONNECT_TMOUT;
	else if (mode == OTG_EVENT_CONNECT_OR_RESUME)
		cnt = OTG_CONNECT_OR_RESUME_TMOUT;
	else if (mode == OTG_EVENT_RESET)
		cnt = OTG_RESET_TMOUT;
	else
		BUG_ON(1);
		//os_ASSERT(0);

	do
	{
		os_ms_sleep(10);
		cnt -= 10;
		mb();
		if (mode == OTG_EVENT_CONNECT && g_otg_connect)
		{
			os_printk(K_ERR, "otg_wait_event : g_otg_connect\n");			

			break;
		}
		else if (mode == OTG_EVENT_CONNECT_OR_RESUME && (g_otg_connect || g_otg_resume || g_otg_disconnect || (u3d_get_vbus_mV()<1000)))
		{
			if (g_otg_connect)
				os_printk(K_ERR, "otg_wait_event : g_otg_connect\n");
			if (g_otg_resume)
				os_printk(K_ERR, "otg_wait_event : g_otg_resume\n");
			if (g_otg_disconnect || (u3d_get_vbus_mV()<1000))
			{
				g_otg_disconnect = 1 ;
				os_printk(K_ERR, "otg_wait_event : g_otg_disconnect\n");

				return OTG_DISCONNECT;
			}
			break;			
		}
		else if (mode == OTG_EVENT_RESET && g_otg_reset)
		{
			os_printk(K_ERR, "otg_wait_event : g_otg_reset\n");

			break;
		}
		else
		{
			//os_ms_sleep(100);
			//cnt -= 100;
			
			if (cnt <= 0)
			{
				if (mode == OTG_EVENT_CONNECT)
				{
					os_printk(K_ERR, "otg_wait_event : == OTG_EVENT_CONNECT timeout ==\n");
					return OTG_NOT_CONNECT;
				}
				else if (mode == OTG_EVENT_CONNECT_OR_RESUME)
				{
					os_printk(K_ERR, "otg_wait_event : == OTG_EVENT_CONNECT_OR_RESUME timeout ==, vbus is %d\n", u3d_get_vbus_mV());
					return OTG_HNP_TMOUT;
				}
				else if (mode == OTG_EVENT_RESET)
				{
					os_printk(K_ERR, "otg_wait_event : == OTG_EVENT_RESET timeout ==\n");
					return OTG_NOT_RESET;				
				}

				return 0;
			}
		}
	} while (g_otg_exec);


	#if 0
	if (!g_otg_exec)
	{
		os_printk(K_ERR, "TEST STOP\n");
		return OTG_TEST_STOP;
	}
	#endif

	//TD 4.8
	if (g_otg_resume)
	{
		os_printk(K_ERR, "otg_wait_event : RESUME\n");
		return OTG_RESUME;
	}
	else
		return 0;
}

int otg_a_uut_thread(void *data){
	DEV_INT32 ret;
	
	

	os_printk(K_ERR, "%s\n", __func__);
	g_otg_exec = 1;

TD4_6:
	//clear all events
	otg_init();

	//enable interrupts. there may be cleared by host cleanup
	u3d_initialize_drv();
	
	//remove termination
	mu3d_hal_u2dev_disconn();

	//wait A-UUT to become device
	os_printk(K_ERR, "Wait to become device (CHG_B_ROLE_B)\n");
	while (!(g_otg_chg_b_role_b && !(os_readl(U3D_SSUSB_OTG_STS) & SSUSB_HOST_DEV_MODE)) && g_otg_exec)
		os_ms_sleep(1);
	if (!g_otg_exec)
		goto A_UUT_DONE;
	
	//A-UUT termination on
	os_printk(K_ERR, "11111\n");
	mu3d_hal_set_speed(SSUSB_SPEED_HIGH);
	
	//wait for bus reset from A-OPT
	os_printk(K_ERR, "2222\n");

	ret = otg_wait_event(OTG_EVENT_RESET);
	if (ret == OTG_TEST_STOP)
		return 0;
	if (ret == OTG_NOT_RESET)
		return 0;

	os_printk(K_ERR, "3333\n");

	os_printk(K_ERR, "wait for host to suspend\n");	
	while (!(g_otg_config && g_otg_suspend) && g_otg_exec)
	{
		if (g_otg_suspend && !g_otg_config)
		{
			os_printk(K_ERR, "Set CHG_A_ROLE_A\n");
			os_setmsk(U3D_SSUSB_OTG_STS, SSUSB_CHG_A_ROLE_A);		

			goto TD4_6;
		}
		
		os_ms_sleep(1);
	};
	if (!g_otg_exec)
		goto A_UUT_DONE;

	//wait A-UUT to become host
	os_printk(K_ERR, "Wait to become host (CHG_A_ROLE_B)\n");
	while (!(g_otg_chg_a_role_b && (os_readl(U3D_SSUSB_OTG_STS) & SSUSB_HOST_DEV_MODE)) && g_otg_exec)
		os_ms_sleep(1);
	if (!g_otg_exec)
		goto A_UUT_DONE;

	//handover to xHCI
	os_printk(K_ERR, "Set CHG_A_ROLE_A\n");
	os_setmsk(U3D_SSUSB_OTG_STS, SSUSB_CHG_A_ROLE_A);


A_UUT_DONE:
	os_printk(K_ERR, "%s Done\n", __func__);
	g_otg_exec = 0;
}

int otg_b_uut_thread(void *data){
	#if 1
		DEV_INT32 cnt = 0;
		DEV_INT32 ret;
		
		
		os_printk(K_ERR, "%s\n", __func__);
		g_otg_exec = 1;
	
		os_setmsk(U3D_SSUSB_OTG_STS, SSUSB_CHG_B_ROLE_B);
		while (!g_otg_chg_b_role_b)
			os_ms_sleep(1);
		
		//clear all events
		otg_init();
	
		//enable interrupts. there may be cleared by host cleanup
		u3d_initialize_drv();
	
	#if 0 //fail
		//B-UUT issues an SRP to start a session with A-OPT
		os_printk(K_ERR, "Make sure SESSION = 0\n");	
		while(os_readl(U3D_DEVICE_CONTROL) & SESSION);
	#endif
	
		//remove termination
		//mu3d_hal_u2dev_disconn();
	
		os_printk(K_ERR, "Set session\n");
		os_setmsk(U3D_POWER_MANAGEMENT, SOFT_CONN); 
		os_setmsk(U3D_DEVICE_CONTROL, SESSION);
	
		//100ms after Vbus begins to decay, A-OPT powers Vbus
		cnt = OTG_A_OPT_DRIVE_VBUS_TMOUT;
		
		while (!(g_otg_vbus_chg && ((os_readl(U3D_DEVICE_CONTROL) & VBUS) == 0x18)))
		{
			os_ms_sleep(10);
			cnt -= 10;
	
			if (cnt <= 0)
			{
				err_msg = OTG_MSG_DEV_NOT_RESPONSE;
				os_printk(K_ERR, "A-OPT driving Vbus timeout\n");
				
				return 0;
			}
		};
	
		//after detecting Vbus, B-UUT connects to A-OPT
		mu3d_hal_set_speed(SSUSB_SPEED_HIGH);
	
		//wait for bus reset from A-OPT
		ret = otg_wait_event(OTG_EVENT_RESET);
		if (ret == OTG_TEST_STOP)
			return 0;

	TD6_13:
		os_printk(K_ERR, "wait A-OPT suspend\n");
		while (!(g_otg_config && g_otg_suspend) && g_otg_exec)
		{
	//		os_printk(K_ERR, "U3D_COMMON_USB_INTR: %x\n", os_readl(U3D_COMMON_USB_INTR));
	//		os_printk(K_ERR, "U3D_COMMON_USB_INTR_ENABLE: %x\n", os_readl(U3D_COMMON_USB_INTR_ENABLE));
	
			os_ms_sleep(10);
		}
		if (!g_otg_exec)
			goto B_UUT_DONE;
	
		if (g_otg_resume)
	{
			g_otg_resume = 0;
			goto TD6_13;
		}
		
		os_printk(K_ERR, "HNP start\n");
		os_printk(K_ERR, "wait A-OPT connect\n");
		ret = otg_wait_event(OTG_EVENT_CONNECT_OR_RESUME);
	
		if (ret == OTG_TEST_STOP)
			return 0;
		else if (ret == OTG_RESUME)
		{
			//clear all events
			otg_init();
		
			goto TD6_13;
		}
		else if (ret == OTG_HNP_TMOUT)
		{
			os_printk(K_ERR, "HNP timeout\n");
	
			os_clrmsk(U3D_DEVICE_CONTROL, HOSTREQ);
	
			if (g_otg_td_5_9)
				err_msg = OTG_MSG_DEV_NOT_RESPONSE; 	
	}

		//wait B-UUT to become host
		os_printk(K_ERR, "Wait to become host (CHG_A_ROLE_B)\n");
		while (!(g_otg_chg_a_role_b && (os_readl(U3D_SSUSB_OTG_STS) & SSUSB_HOST_DEV_MODE)) && g_otg_exec)
			os_ms_sleep(1); 
		if (!g_otg_exec)
			goto B_UUT_DONE;
	
		//handover to xHCI
	os_printk(K_ERR, "Set CHG_A_ROLE_A\n");
	os_setmsk(U3D_SSUSB_OTG_STS, SSUSB_CHG_A_ROLE_A);

		//wait B-UUT to become device
		os_printk(K_ERR, "Wait to become device (CHG_B_ROLE_B)\n");
		while (!(g_otg_chg_b_role_b && !(os_readl(U3D_SSUSB_OTG_STS) & SSUSB_HOST_DEV_MODE)) && g_otg_exec)
			os_ms_sleep(1);
		if (!g_otg_exec)
			goto B_UUT_DONE;
	
		//wait for reset from A-OPT
		ret = otg_wait_event(OTG_EVENT_RESET);
		if (ret == OTG_TEST_STOP)
			return 0;
	
		if (g_otg_exec)
			goto TD6_13;
	
	
	B_UUT_DONE:
		os_printk(K_ERR, "%s Done\n", __func__);	
		g_otg_exec = 0;
	#endif
}

int otg_opt20_a_uut_thread(void *data){
	typedef enum
	{
		OTG_INIT_STATE,
		OTG_ATTACHED_STATE,
		OTG_POWERED_STATE,
		OTG_DEFAULT_STATE,
		OTG_ADDRESS_STATE,
		OTG_CONFIGURED_STATE,
		OTG_SUSPEND_STATE,
		OTG_A_IDLE_1_STATE,
		OTG_A_IDLE_2_STATE,		
	} STATE;

	char* state_string[] = {
		"INIT",
		"ATTACHED",
		"POWERED",
		"DEFAULT",
		"ADDRESS",
		"CONFIGURED",
		"SUSPEND",
		"A_IDLE_1",
		"A_IDLE_2"		
	};
	
	STATE cur_state, next_state;
	DEV_INT32 cnt = 0;
	DEV_INT32 ret;
	DEV_UINT32 vbus =0;
	
	

	//clear OTG events
	otg_init();

	//enable interrupts
	u3d_initialize_drv();

	#if 0
	//force device to work as B-UUT by a false IDDIG event
	os_setmsk(U3D_SSUSB_OTG_STS, SSUSB_ATTACH_B_ROLE);
	while (!g_otg_attach_b_role)
		os_ms_sleep(1);
	#endif


	//initialize state machine
	cur_state = OTG_INIT_STATE;
	next_state = OTG_INIT_STATE;

	usb_phy_recover(g_usb_phy_clk_on) ;
	if (g_usb_phy_clk_on == 0){	
		g_usb_phy_clk_on = 1 ;
	}
	
	while (1)
	{	
		switch (cur_state)
		{
			case OTG_INIT_STATE:
				//clear OTG events
				otg_init();
				mb();
				g_otg_exec = 1;
				mb();

				#ifdef SUPPORT_U3
				if (os_readl(U3D_USB3_CONFIG) & USB3_EN){
					mu3d_hal_u3dev_dis();	
				}
				#endif

				if (os_readl(U3D_POWER_MANAGEMENT) & SOFT_CONN){
					//mu3d_hal_u2dev_disconn();		// KKKKKK			
				}

				//make sure vbus is low
				//this fails, why?
				//while ((os_readl(U3D_DEVICE_CONTROL) & VBUS) != 0x0);

				next_state = OTG_A_IDLE_2_STATE;
				break;
				
			case OTG_ATTACHED_STATE:
				//A-OPT powers vbus. wait for valid vbus
				if (vbus != (os_readl(U3D_DEVICE_CONTROL) & VBUS))
				{
					vbus = (os_readl(U3D_DEVICE_CONTROL) & VBUS);
					os_printk(K_ERR, "g_otg_vbus_chr: %x, VBUS: %x\n", g_otg_vbus_chg, (os_readl(U3D_DEVICE_CONTROL) & VBUS));						
				}

	
				//if (g_otg_vbus_chg && ((os_readl(U3D_DEVICE_CONTROL) & VBUS) >= 0x10))
				vbus = u3d_get_vbus_mV();
				//if (((os_readl(U3D_DEVICE_CONTROL) & VBUS) >= 0x10)) //AVALID
				if (vbus > 3950)	
				{
					//g_otg_vbus_chg = 0;
					//usb_phy_recover(0);
					next_state = OTG_POWERED_STATE;					
					os_printk(K_ERR, "[U3D_T]u3d_get_vbus_mV : raw: %d, change to OTG_POWERED_STATE\n", vbus);
				}

				break;

			case OTG_POWERED_STATE:				
				//after detecting vbus, B-UUT connects to A-OPT
				mu3d_hal_set_speed(SSUSB_SPEED_HIGH);
	
				next_state = OTG_DEFAULT_STATE;
				break;

			case OTG_DEFAULT_STATE:
				//wait for bus reset from A-OPT				
				ret = otg_wait_event(OTG_EVENT_RESET);
				mb();
				if (ret == OTG_OK)
				{
					g_otg_reset = 0;
					mb();
					os_printk(K_ERR, "g_otg_reset = 0 (%s)\n", __func__);	

					next_state = OTG_ADDRESS_STATE;
				}
				else if (g_otg_config)
				{
					next_state = OTG_CONFIGURED_STATE;
				}
				//else?
				break;

			case OTG_ADDRESS_STATE:
				mb();
				if (g_otg_config)
					next_state = OTG_CONFIGURED_STATE;
				//else?
				break;

			case OTG_CONFIGURED_STATE:
				mb();
				 if (g_otg_b_hnp_enable)
				{
					next_state = OTG_A_IDLE_1_STATE;
				}else if (g_otg_suspend)
				{
					g_otg_suspend = 0;
					mb();
					next_state = OTG_SUSPEND_STATE;
				}

				#if 0
				else if (g_otg_chg_a_role_b  && (os_readl(U3D_SSUSB_OTG_STS) & SSUSB_HOST_DEV_MODE))
				{
					os_printk(K_ERR, "Set CHG_A_ROLE_A\n");
					os_setmsk(U3D_SSUSB_OTG_STS, SSUSB_CHG_A_ROLE_A);		

					os_printk(K_ERR, "Wait to become device (CHG_B_ROLE_B) again\n");
					
					next_state = OTG_A_IDLE_2_STATE;					
				} 	
				#endif
				break;

			case OTG_SUSPEND_STATE:
				mb();
				if (g_otg_resume)
				{
					g_otg_resume = 0;
					mb();
					next_state = OTG_CONFIGURED_STATE;
				}else if(g_otg_b_hnp_enable){
					next_state = OTG_A_IDLE_1_STATE;
				}
				//else?

				break;
			case OTG_A_IDLE_1_STATE:
				os_printk(K_ERR, "Enable host request\n");
				os_setmsk(U3D_DEVICE_CONTROL, HOSTREQ);
		
				os_printk(K_ERR, "Wait for suspend\n");
				mb();
				while (!g_otg_suspend){
					os_ms_sleep(1);
					mb();
				}
				g_otg_suspend = 0;
				mb();
				os_printk(K_ERR, "Request HNP\n");


				ret = otg_wait_event(OTG_EVENT_CONNECT_OR_RESUME);  // wait disconnect 
				
				if (ret == OTG_RESUME)
				{
					mb();
					g_otg_resume = 0;
					mb();
					next_state = OTG_CONFIGURED_STATE;
				}
				else if (ret == OTG_HNP_TMOUT)
				{
					os_printk(K_ERR, "== Device no response ==\n");
					os_printk(K_ERR, "HNP timeout\n");
				
					os_clrmsk(U3D_DEVICE_CONTROL, HOSTREQ);

					//next_state = ??		
					next_state = OTG_CONFIGURED_STATE;
				}
				#if 0
				else if (ret == OTG_DISCONNECT)
				{
					os_printk(K_ERR, "Device Disconnected\n");
				
					os_clrmsk(U3D_DEVICE_CONTROL, HOSTREQ);

					//next_state = ??		
					next_state = OTG_INIT_STATE;
				}
				#endif
				else
				{
					os_printk(K_ERR, "Wait to become host (CHG_A_ROLE_B)\n");
					mb();
					while (!(g_otg_chg_a_role_b && (os_readl(U3D_SSUSB_OTG_STS) & SSUSB_HOST_DEV_MODE))){
						os_ms_sleep(1);
						mb();
					}

					os_printk(K_ERR, "Set CHG_A_ROLE_A\n");
					os_setmsk(U3D_SSUSB_OTG_STS, SSUSB_CHG_A_ROLE_A);

					os_printk(K_ERR, "Wait to become device (CHG_B_ROLE_B) again\n");

					// next_state = OTG_A_IDLE_2_STATE;
					// USBIF , WARN
					next_state = OTG_INIT_STATE; //?? device can call disconnect
				}
				
				break;
			case OTG_A_IDLE_2_STATE:
				#if 0
				os_printk(K_ERR, "g_otg_chg_b_role_b: %x, SSUSB_HOST_DEV_MODE: %x\n",
					g_otg_chg_b_role_b,
					(os_readl(U3D_SSUSB_OTG_STS) & SSUSB_HOST_DEV_MODE));
				#endif
				mb();
				if (g_otg_chg_b_role_b && !(os_readl(U3D_SSUSB_OTG_STS) & SSUSB_HOST_DEV_MODE))
				{
					//if (!(os_readl(U3D_SSUSB_OTG_STS) & SSUSB_HOST_DEV_MODE))
					//{
						os_printk(K_ERR, "Become device\n");
						//clear OTG events
						// otg_init();

						next_state = OTG_ATTACHED_STATE;
					//}
					//else
					//{
						os_printk(K_ERR, "SSUSB_HOST_DEV_MODE: %x\n", (os_readl(U3D_SSUSB_OTG_STS) & SSUSB_HOST_DEV_MODE));
					//}
				}

				// test
				//vbus = u3d_get_vbus_mV();
				//msleep(500) ;
			
				break;
		}


		if (cur_state != OTG_A_IDLE_2_STATE) // In Device mode 
		{
			mb();
			//A-OPT ends each test case with disconnection
			if (g_otg_disconnect)
			{				
				g_otg_disconnect = 0;
				os_printk(K_ERR, "A-OPT disconnect\n");

				mb();
//				if (g_otg_config && g_otg_srp_reqd)
				if (g_otg_srp_reqd)
				{							
					os_printk(K_ERR, "Send SRP request\n");
					os_ms_delay(1500);
					//os_ms_sleep(1000);
					os_setmsk(U3D_DEVICE_CONTROL, SESSION);

					cnt = OTG_A_OPT_DRIVE_VBUS_TMOUT;				
					
					vbus = u3d_get_vbus_mV();
					//while (!(g_otg_vbus_chg && ((os_readl(U3D_DEVICE_CONTROL) & VBUS) == 0x18)))
					while (vbus < 3950)
					{
						os_ms_sleep(10);
						cnt -= 10;
					
						if (cnt <= 0)
						{
							err_msg = OTG_MSG_DEV_NOT_RESPONSE;
							os_printk(K_ERR, "== Device no response == \n");
							os_printk(K_ERR, "== A-OPT driving Vbus timeout == \n");
							
							break;
						}
						vbus = u3d_get_vbus_mV();
					};
				}
				else
				{
					next_state = OTG_INIT_STATE;
				}
			}
			//else if (g_otg_vbus_chg && ((os_readl(U3D_DEVICE_CONTROL) & VBUS) == 0x8))
			else if (u3d_get_vbus_mV() < 2000)
			{
				os_printk(K_DEBUG, "[U3D_T]u3d_get_vbus_mV : raw: %d, back to OTG_INIT_STATE\n", u3d_get_vbus_mV() );
				next_state = OTG_INIT_STATE;
				#ifdef SUPPORT_OTG
				mb();
				g_otg_disconnect = 1;
				mb();
				#endif
			}
			else if (g_otg_reset)
			{
				mb();
				g_otg_reset = 0;
				mb();
				os_printk(K_ERR, "g_otg_reset = 0 (%s)\n", __func__);					
				//g_otg_config = 0;
				//g_otg_srp_reqd = 0;
				//g_otg_hnp_reqd = 0;
				//os_printk(K_ERR, "g_otg_hnp_reqd = 0 (%s)\n", __func__);				
				//g_otg_b_hnp_enable = 0;
				os_clrmsk(U3D_DEVICE_CONTROL, HOSTREQ);			

				next_state = OTG_ADDRESS_STATE;
			}
			else if (g_otg_chg_a_role_b  && (os_readl(U3D_SSUSB_OTG_STS) & SSUSB_HOST_DEV_MODE))
			{
				os_printk(K_ERR, "Set CHG_A_ROLE_A\n");
				os_setmsk(U3D_SSUSB_OTG_STS, SSUSB_CHG_A_ROLE_A);
				
				os_printk(K_ERR, "Wait to become device (CHG_B_ROLE_B) again\n");
				
				// next_state = OTG_A_IDLE_2_STATE;					
				// USBIF , WARN
				next_state = OTG_INIT_STATE; //?? device can call disconnect
			} 				
		}
		else if (g_otg_hnp_reqd)
		{
			next_state = OTG_A_IDLE_2_STATE;
		}

		if (cur_state != next_state)
		{
			os_printk(K_ERR, "CUR_STATE: %s, NEXT_STATE: %s\n", 
				state_string[cur_state], state_string[next_state]);
		}
		os_ms_sleep(1);

		cur_state = next_state;

		if (!g_otg_exec)
			break;
			
	}

	os_printk(K_ERR, "%s Done\n", __func__);
	return 0 ;
}

int otg_opt20_b_uut_thread(void *data){
	typedef enum
	{
		OTG_INIT_STATE,
		OTG_ATTACHED_STATE,
		OTG_POWERED_STATE,
		OTG_DEFAULT_STATE,
		OTG_ADDRESS_STATE,
		OTG_CONFIGURED_STATE,
		OTG_SUSPEND_STATE,
		OTG_A_IDLE_1_STATE,
		OTG_A_IDLE_2_STATE,		
	} STATE;

	char* state_string[] = {
		"INIT",
		"ATTACHED",
		"POWERED",
		"DEFAULT",
		"ADDRESS",
		"CONFIGURED",
		"SUSPEND",
		"A_IDLE_1",
		"A_IDLE_2"		
	};
	
	STATE cur_state, next_state;
	DEV_INT32 ret = -1;
	DEV_UINT32 vbus;
	
	


	printk("[U3D_T] otg_opt20_b_uut_thread ===> otg_init\n") ;
	//clear OTG events
	otg_init();

	printk("[U3D_T] otg_opt20_b_uut_thread ===> u3d_initialize_drv\n") ;
	//enable interrupts
	u3d_initialize_drv();


	//force device to work as B-UUT by a false IDDIG event
	// USBIF, in Real chip , we have IDDIG ~ should we remove this 
	os_setmsk(U3D_SSUSB_OTG_STS, SSUSB_ATTACH_B_ROLE);
	printk("[U3D_T] otg_opt20_b_uut_thread ===> wait g_otg_attach_b_role\n") ;
	while (!g_otg_attach_b_role)
		os_ms_sleep(1);


	//initialize state machine
	cur_state = OTG_INIT_STATE;
	next_state = OTG_INIT_STATE;
	
	while (1)
	{	
		switch (cur_state)
		{
			case OTG_INIT_STATE:
				//clear OTG events
				//printk("[U3D_T] UUT-B : OTG_INIT_STATE\n");
				otg_init();
				u3d_initialize_drv();
				g_otg_exec = 1;

				os_clrmsk(U3D_DEVICE_CONTROL, HOSTREQ);	 // clean host request
				
				//remove D+
				os_printk(K_ERR, "Remove D+\n");
				mu3d_hal_u2dev_disconn();

				// USBIF
				//usb_phy_savecurrent(1) ;
				usb_phy_savecurrent(g_usb_phy_clk_on) ;
				if (g_usb_phy_clk_on == 1){
					g_usb_phy_clk_on = 0 ;
				}
			
				next_state = OTG_ATTACHED_STATE;
				break;
				
			case OTG_ATTACHED_STATE:
				//A-OPT powers vbus. wait for valid vbus
				//printk("[U3D_T] UUT-B : OTG_ATTACHED_STATE\n");

				// USBIF, check VBUS from PMIC value
				// check PMIC vbus from PMIC API 
				/*
				if (vbus != (os_readl(U3D_DEVICE_CONTROL) & VBUS))
				{
					vbus = (os_readl(U3D_DEVICE_CONTROL) & VBUS);
					os_printk(K_ERR, "g_otg_vbus_chr: %x, VBUS: %x\n", g_otg_vbus_chg, (os_readl(U3D_DEVICE_CONTROL) & VBUS));						
				}
				*/

				//os_printk(K_ERR, "g_otg_vbus_chr: %x, VBUS: %x\n", g_otg_vbus_chg, (os_readl(U3D_DEVICE_CONTROL) & VBUS));						

				//set the RG_VBUSVALID=1, RG_VALID=1, RG_SESSEND=0
				//U3PhyWriteField32(U3D_U2PHYDTM1, E60802_RG_VBUSVALID_OFST, E60802_RG_VBUSVALID, 1);
				//U3PhyWriteField32(U3D_U2PHYDTM1, E60802_RG_AVALID_OFST, E60802_RG_AVALID, 1);
				//U3PhyWriteField32(U3D_U2PHYDTM1, E60802_RG_SESSEND_OFST, E60802_RG_SESSEND, 0);

				//os_printk(K_ERR, "g_otg_vbus_chr: %x, VBUS: %x\n", g_otg_vbus_chg, (os_readl(U3D_DEVICE_CONTROL) & VBUS));										

				// USBIF, DEVICE_CONTROL can't work in real chip , change to check PMID value
				// if ((check PMIC vbus is greater than 4 V ))
				/*
				if (g_otg_vbus_chg && ((os_readl(U3D_DEVICE_CONTROL) & VBUS) >= 0x10))
				{
					g_otg_vbus_chg = 0;
					next_state = OTG_POWERED_STATE;					
					// USBIF
					//set the RG_VBUSVALID=1, RG_VALID=1, RG_SESSEND=0
					//U3PhyWriteField32(U3D_U2PHYDTM1, E60802_RG_VBUSVALID_OFST, E60802_RG_VBUSVALID, 1);
					//U3PhyWriteField32(U3D_U2PHYDTM1, E60802_RG_AVALID_OFST, E60802_RG_AVALID, 1);
					//U3PhyWriteField32(U3D_U2PHYDTM1, E60802_RG_SESSEND_OFST, E60802_RG_SESSEND, 0);
				}
				*/
				// USBIF
				vbus = u3d_get_vbus_mV() ;
				if (vbus > 3950){
					//usb_phy_recover(0);
					usb_phy_recover(g_usb_phy_clk_on);
					if (g_usb_phy_clk_on == 0){
						g_usb_phy_clk_on = 1 ;
					}
				}else{
					//usb_phy_savecurrent(1) ;
					usb_phy_savecurrent(g_usb_phy_clk_on) ;
					if (g_usb_phy_clk_on == 1){
						g_usb_phy_clk_on = 0 ;
					}				
				}

				//if (((os_readl(U3D_DEVICE_CONTROL) & VBUS) >= 0x10))
				//temp3 = readl(SSUSB_OTG_STS);
		
				if (vbus > 3950 && (os_readl(U3D_SSUSB_OTG_STS) & SSUSB_IDDIG))  // do we need to check IDDIG in DEVICE mode
				{
					//g_otg_vbus_chg = 0;					
					next_state = OTG_POWERED_STATE;		
					os_printk(K_ERR, "[U3D_T]u3d_get_vbus_mV : raw: %d, change to OTG_POWERED_STATE\n", vbus);
				}else{
					g_otg_reset = 0;
					os_ms_sleep(100);
				}

				break;

			case OTG_POWERED_STATE:				
				//after detecting vbus, B-UUT connects to A-OPT
				//printk("[U3D_T] UUT-B : OTG_POWERED_STATE\n");	
				mu3d_hal_set_speed(SSUSB_SPEED_HIGH);

				next_state = OTG_DEFAULT_STATE;
				break;
			
			case OTG_DEFAULT_STATE:
				//wait for bus reset from A-OPT		
				//printk("[U3D_T] UUT-B : OTG_DEFAULT_STATE\n");
				// USBIF , can pass 6.8.9.1.4 ??
				//ret = otg_wait_event(OTG_EVENT_RESET);
				mb();
				if (g_otg_reset){
					ret = OTG_OK ;
				}
				
				if (ret == OTG_OK)
				{
					g_otg_reset = 0;
					mb();
					os_printk(K_ERR, "g_otg_reset = 0 (%s)\n", __func__);	

					next_state = OTG_ADDRESS_STATE;
				}
				//else?
				break;

			case OTG_ADDRESS_STATE:
				mb() ;
				if (g_otg_config)
					next_state = OTG_CONFIGURED_STATE;

				//else?
				break;

			case OTG_CONFIGURED_STATE:
				//printk("[U3D_T] UUT-B : OTG_CONFIGURED_STATE\n");
				mb();
				if (g_otg_b_hnp_enable)
				{
					next_state = OTG_A_IDLE_1_STATE;
				}else if (g_otg_suspend)
				{
					g_otg_suspend = 0;
					mb();
					next_state = OTG_SUSPEND_STATE;
				}
				
				break;

			case OTG_SUSPEND_STATE:
				//printk("[U3D_T] UUT-B : OTG_SUSPEND_STATE\n");
				mb();
				if (g_otg_resume)
				{
					g_otg_resume = 0;
					mb();
					next_state = OTG_CONFIGURED_STATE;
				}else if(g_otg_b_hnp_enable){
					next_state = OTG_A_IDLE_1_STATE; 
				}
				//else?

				break;
			case OTG_A_IDLE_1_STATE:	
				//printk("[U3D_T] UUT-B : OTG_A_IDLE_1_STATE\n");
				os_printk(K_ERR, "Enable host request\n");
				os_setmsk(U3D_DEVICE_CONTROL, HOSTREQ);
				
				os_printk(K_ERR, "Wait for suspend\n");
				mb();
				while (!g_otg_suspend){
					os_ms_sleep(1);
					mb();
				}
				g_otg_suspend = 0;
				mb();
				os_printk(K_ERR, "Request HNP\n");


				ret = otg_wait_event(OTG_EVENT_CONNECT_OR_RESUME);// wait for A connect 
				
				if (ret == OTG_RESUME)
				{
					g_otg_resume = 0;
					mb();
					next_state = OTG_CONFIGURED_STATE;
				}
				else if (ret == OTG_HNP_TMOUT)
				{
					os_printk(K_ERR, "== HNP TIME OUT Device no response ==\n");
				
					os_clrmsk(U3D_DEVICE_CONTROL, HOSTREQ);

					//next_state = ??	OTG_CONFIGURED_STATE ??		
					g_otg_b_hnp_enable = 0 ;
					mb() ;
					next_state = OTG_INIT_STATE;
					usbif_u3d_test_send_event(USBIF_OTG_EVENT_HNP_FAILED) ;
				}
				#if 0
				else if (ret == OTG_DISCONNECT)
				{
					os_printk(K_ERR, "Device Disconnected\n");
				
					os_clrmsk(U3D_DEVICE_CONTROL, HOSTREQ);

					//next_state = ??		
					next_state = OTG_INIT_STATE;
				}
				#endif
				else
				{
					os_printk(K_ERR, "Wait to become host (CHG_A_ROLE_B)\n");
					mb();
					while (!(g_otg_chg_a_role_b && (os_readl(U3D_SSUSB_OTG_STS) & SSUSB_HOST_DEV_MODE))){
						os_ms_sleep(1);
						mb();
					}
					
					if (ret == OTG_DISCONNECT)
					{
						os_setmsk(U3D_SSUSB_U2_CTRL_0P, SSUSB_U2_PORT_MAC_RST);
						os_clrmsk(U3D_SSUSB_U2_CTRL_0P, SSUSB_U2_PORT_MAC_RST);

						mu3d_hal_system_intr_en();

						next_state = OTG_INIT_STATE;
					}
					else
					{
						#if 1
						os_printk(K_ERR, "Set CHG_A_ROLE_A\n");
						os_setmsk(U3D_SSUSB_OTG_STS, SSUSB_CHG_A_ROLE_A);
						#endif

						os_printk(K_ERR, "Wait to become device (CHG_B_ROLE_B) again\n");

						//os_printk(K_ERR, "Remove D+\n");
						//mu3d_hal_u2dev_disconn();
				
						next_state = OTG_A_IDLE_2_STATE;

						#if 0
						os_printk(K_ERR, "Set CHG_A_ROLE_A\n");
						os_setmsk(U3D_SSUSB_OTG_STS, SSUSB_CHG_A_ROLE_A);					
						#endif
					}
				}
				
				break;
			case OTG_A_IDLE_2_STATE:
				//printk("[U3D_T] UUT-B : OTG_A_IDLE_2_STATE\n");
				//if (g_otg_chg_b_role_b && !(os_readl(U3D_SSUSB_OTG_STS) & SSUSB_HOST_DEV_MODE))  // wait B back to B,  g_otg_chg_b_role_b
				mb();
				if (g_otg_chg_b_role_b)  // wait B back to B,  g_otg_chg_b_role_b
				{
					g_otg_chg_b_role_b = 0 ;
					os_printk(K_ERR, "Become device\n");

					os_clrmsk(U3D_DEVICE_CONTROL, HOSTREQ);	 // clean host request
					//g_otg_hnp_reqd = 0;	
					mb();
					//clear OTG events
					//otg_init();

					next_state = OTG_ATTACHED_STATE;
					//next_state = OTG_DEFAULT_STATE;
				}
			
				break;
		}

		
		mb();
		if (cur_state != OTG_A_IDLE_2_STATE)
		{
			//A-OPT ends each test case with disconnection
			mb();
			if (g_otg_disconnect)
			{
				g_otg_disconnect = 0;
				mb();
				os_printk(K_ERR, "A-OPT disconnect\n");
				// USBIF

//				if (g_otg_config && g_otg_srp_reqd)
				mb();
				if (g_otg_srp_reqd)
				{							
					os_printk(K_ERR, "Send SRP request\n");
					g_otg_srp_reqd = 0 ;
					mb();
					//os_ms_sleep(1800);  //USBIF spec , wait 1.5s minimum ? or HW help to do this delay ? 
// USBIF , the following code should be replaced by checking PMIC 
#if 1
					{
					unsigned int vbus_value;
				    bool timeout_flag = false;
				    unsigned long vbus_polling_timeout;
				    unsigned int vbus_above_valied_cnt ;

					timeout_flag = false;
					vbus_value = u3d_get_vbus_mV() ;														
					vbus_polling_timeout=jiffies + 5 * HZ;

					while (vbus_value > 700) {
						os_printk(K_ERR,"musb::not below Session-End! (%d)\n", vbus_value);
						if(time_after(jiffies, vbus_polling_timeout)){
							timeout_flag = true;
							break;
						}
						msleep(20);
						vbus_value = u3d_get_vbus_mV() ;
					}
					os_printk(K_ERR, "[U3D_T]u3d_get_vbus_mV : raw: %d, SRP vbus fall\n", vbus_value);
					// USBIF
					//mu3d_hal_u2dev_disconn() ;

					// set the RG_VBUSVALID=1, RG_VALID=1, RG_SESSEND=0
#ifdef CONFIG_PROJECT_PHY					
					U3PhyWriteField32(U3D_U2PHYDTM1, E60802_RG_VBUSVALID_OFST, E60802_RG_VBUSVALID, 0);
					U3PhyWriteField32(U3D_U2PHYDTM1, E60802_RG_AVALID_OFST, E60802_RG_AVALID, 0);
					U3PhyWriteField32(U3D_U2PHYDTM1, E60802_RG_SESSEND_OFST, E60802_RG_SESSEND, 1);
#endif					
					os_printk(K_ERR, "[U3D_T]1 u3d_get_vbus_mV : raw: %d, SRP vbus fall\n", u3d_get_vbus_mV());

					os_clrmsk(U3D_DEVICE_CONTROL, SESSION);

					os_printk(K_ERR, "[U3D_T]2 u3d_get_vbus_mV : raw: %d, SRP vbus fall\n", u3d_get_vbus_mV());
					
					os_ms_sleep(1600);	
					// Set Vbus Pulsing Length
#if 0
					os_printk(K_ERR, "[U3D_T] U3D_LINK_RESET_INFO : 0x%x\n", os_readl(U3D_LINK_RESET_INFO));

					os_writel(U3D_LINK_RESET_INFO, os_readl(U3D_LINK_RESET_INFO) | 0xff00); // enable vbus pulsing in U3
					
					os_setmsk(U3D_SSUSB_IP_SPARE0, (0x1<<19)); // enable vbus pulsing in U3
					
					os_printk(K_ERR, "[U3D_T] U3D_SSUSB_IP_SPARE0 : 0x%x\n", os_readl(U3D_SSUSB_IP_SPARE0));

					os_printk(K_ERR, "VBUS PULSE OUT 1\n");
					mt_set_gpio_mode(GPIO121,GPIO_MODE_GPIO);  
					mt_set_gpio_dir(GPIO121,GPIO_DIR_OUT);
					mt_set_gpio_out(GPIO121,GPIO_OUT_ONE);    
					os_ms_sleep(10);
					os_printk(K_ERR, "VBUS PULSE OUT 0\n");
					mt_set_gpio_out(GPIO121,GPIO_OUT_ZERO);    
#endif					
					os_setmsk(U3D_DEVICE_CONTROL, SESSION);
					os_printk(K_ERR, "[U3D_T]3 u3d_get_vbus_mV : raw: %d, SRP vbus fall\n", u3d_get_vbus_mV());
					//os_printk(K_ERR, "Remove D+\n");
					
					
					//mdelay(1800);  //USBIF spec ,wait 1.5s minimum ? or HW help to do this delay ? 

					//mu3d_hal_u2dev_connect() ;
										
					vbus_polling_timeout =jiffies + 6 * HZ;
					vbus_above_valied_cnt = 0 ;
					while (vbus_above_valied_cnt<2) { // to ignore the vbus glitch
						os_printk(K_ERR,"musb::not above Session-Valid! (%d)\n", vbus_value);
						if(time_after(jiffies, vbus_polling_timeout)){
							timeout_flag = true;
							break;
						}
						msleep(20);
						vbus_value = u3d_get_vbus_mV() ;			
						if (vbus_value > 3950){
							vbus_above_valied_cnt ++ ;
						}
					}
					os_printk(K_ERR, "[U3D_T]u3d_get_vbus_mV : raw: %d, SRP vbus UP\n", vbus_value);
					
					if (!timeout_flag)
					{					
						os_printk(K_ERR, "VBUS ready , Set mu3d_hal_set_speed and SOFT CONNECT\n");
						
						// set the RG_VBUSVALID=1, RG_VALID=1, RG_SESSEND=0
#ifdef CONFIG_PROJECT_PHY
						U3PhyWriteField32(U3D_U2PHYDTM1, E60802_RG_VBUSVALID_OFST, E60802_RG_VBUSVALID, 1);
						U3PhyWriteField32(U3D_U2PHYDTM1, E60802_RG_AVALID_OFST, E60802_RG_AVALID, 1);
						U3PhyWriteField32(U3D_U2PHYDTM1, E60802_RG_SESSEND_OFST, E60802_RG_SESSEND, 0);
#endif						

						os_clrmsk(U3D_DEVICE_CONTROL, SESSION);
						mu3d_hal_set_speed(SSUSB_SPEED_HIGH);
					} else {
						err_msg = OTG_MSG_DEV_NOT_RESPONSE;
						os_printk(K_ERR, "== Device no response == \n");
						os_printk(K_ERR, "== A-OPT driving Vbus timeout == \n");
						usbif_u3d_test_send_event(USBIF_OTG_EVENT_NO_RESP_FOR_SRP) ;

					}
					}
#else					
					os_setmsk(U3D_DEVICE_CONTROL, SESSION);

					cnt = OTG_A_OPT_DRIVE_VBUS_TMOUT;			
					// USBIF, DEVICE_CONTROL can't work in real chip , change to check PMID value
					// while (!(check PMIC vbus is smaller than 4 V ))
					while (!(g_otg_vbus_chg && ((os_readl(U3D_DEVICE_CONTROL) & VBUS) == 0x18)))
					{
						os_ms_sleep(10);
						cnt -= 10;
					
						if (cnt <= 0)
						{
							err_msg = OTG_MSG_DEV_NOT_RESPONSE;
							os_printk(K_ERR, "== Device no response == \n");
							os_printk(K_ERR, "== A-OPT driving Vbus timeout == \n");
							usbif_u3d_test_send_event(USBIF_OTG_EVENT_NO_RESP_FOR_SRP) ;
							break;
						}
					};
					os_clrmsk(U3D_DEVICE_CONTROL, SESSION);
#endif					
				}
				else
				{
					// double check the UUT-B is really disconnect
					if (u3d_get_vbus_mV()<700){ 
						if (cur_state != OTG_ATTACHED_STATE){
							next_state = OTG_INIT_STATE;
						}
					}
				}
			}
			// USBIF, DEVICE_CONTROL can't work in real chip , change to check PMID value
			//else if ((check PMIC vbus is smaller than 0.7 V ))
			//else if (g_otg_vbus_chg && ((os_readl(U3D_DEVICE_CONTROL) & VBUS) == 0x8))
			//else if (u3d_get_vbus_mV()<700 && ((os_readl(U3D_DEVICE_CONTROL) & VBUS) == 0x8))
			else if (u3d_get_vbus_mV()<700)
			{
				// USBIF
				//next_state = OTG_INIT_STATE;			
				os_printk(K_ERR, "[U3D_T]u3d_get_vbus_mV : raw: %d, change to OTG_INIT_STATE\n", u3d_get_vbus_mV());
				
				// USBIF , WARN
#ifdef SUPPORT_OTG
				g_otg_disconnect = 1;
				u3d_sync_with_bat(USB_SUSPEND) ;
				mb();
#endif
			}
			else if (g_otg_reset)
			{
				g_otg_reset = 0;
				os_printk(K_ERR, "g_otg_reset = 0 (%s)\n", __func__);					
				//g_otg_config = 0;
				mb() ;
				os_printk(K_ERR, "g_otg_config = 0 (%s)\n", __func__);					
				//g_otg_srp_reqd = 0;
				//g_otg_hnp_reqd = 0;			
				//os_printk(K_ERR, "g_otg_hnp_reqd = 0 (%s)\n", __func__);				
				//g_otg_b_hnp_enable = 0;
				//os_clrmsk(U3D_DEVICE_CONTROL, HOSTREQ);			

				if (next_state != OTG_A_IDLE_2_STATE){
					next_state = OTG_ADDRESS_STATE;
				}
			}
		}
		else if (g_otg_hnp_reqd)
		{			
			//next_state = OTG_A_IDLE_2_STATE;
		}

		if (cur_state != next_state)
		{
			os_printk(K_ERR, "CUR_STATE: %s, NEXT_STATE: %s, g_otg_disconnect is %d\n", 
				state_string[cur_state], state_string[next_state], g_otg_disconnect);
		}
		os_ms_sleep(1);

		cur_state = next_state;

		if (!g_otg_exec)
			break;
			
	}

	os_printk(K_ERR, "%s Done\n", __func__);
	
#if 0




TD6_13:
	os_printk(K_ERR, "wait A-OPT suspend\n");
	while (!(g_otg_config && g_otg_suspend) && g_otg_exec)
	{
//		os_printk(K_ERR, "U3D_COMMON_USB_INTR: %x\n", os_readl(U3D_COMMON_USB_INTR));
//		os_printk(K_ERR, "U3D_COMMON_USB_INTR_ENABLE: %x\n", os_readl(U3D_COMMON_USB_INTR_ENABLE));

		os_ms_sleep(10);
	}
	if (!g_otg_exec)
		goto B_UUT_DONE;

	if (g_otg_resume)
	{
		g_otg_resume = 0;
		goto TD6_13;
	}
	
	os_printk(K_ERR, "HNP start\n");
	os_printk(K_ERR, "wait A-OPT connect\n");
	ret = otg_wait_event(OTG_EVENT_CONNECT_OR_RESUME);

	if (ret == OTG_TEST_STOP)
		return 0;
	else if (ret == OTG_RESUME)
	{
		//clear all events
		otg_init();
	
		goto TD6_13;
	}
	else if (ret == OTG_HNP_TMOUT)
	{
		os_printk(K_ERR, "HNP timeout\n");

		os_clrmsk(U3D_DEVICE_CONTROL, HOSTREQ);

		if (g_otg_td_5_9)
			err_msg = OTG_MSG_DEV_NOT_RESPONSE; 	
	}

	//wait B-UUT to become host
	os_printk(K_ERR, "Wait to become host (CHG_A_ROLE_B)\n");
	while (!(g_otg_chg_a_role_b && (os_readl(U3D_SSUSB_OTG_STS) & SSUSB_HOST_DEV_MODE)) && g_otg_exec)
		os_ms_sleep(1); 
	if (!g_otg_exec)
		goto B_UUT_DONE;

	//handover to xHCI
	os_printk(K_ERR, "Set CHG_A_ROLE_A\n");
	os_setmsk(U3D_SSUSB_OTG_STS, SSUSB_CHG_A_ROLE_A);

	//wait B-UUT to become device
	os_printk(K_ERR, "Wait to become device (CHG_B_ROLE_B)\n");
	while (!(g_otg_chg_b_role_b && !(os_readl(U3D_SSUSB_OTG_STS) & SSUSB_HOST_DEV_MODE)) && g_otg_exec)
		os_ms_sleep(1);
	if (!g_otg_exec)
		goto B_UUT_DONE;

	//wait for reset from A-OPT
	ret = otg_wait_event(OTG_EVENT_RESET);
	if (ret == OTG_TEST_STOP)
		return 0;

	if (g_otg_exec)
		goto TD6_13;

B_UUT_DONE:
	g_otg_exec = 0; 
	os_printk(K_ERR, "%s Done\n", __func__);	

	
	#if 0
	DEV_UINT8 timeout = 0;
	DEV_INT32 cnt = 0;
	DEV_INT32 ret;
	
	os_printk(K_ERR, "%s\n", __func__);
	g_otg_exec = 1;

	os_setmsk(U3D_SSUSB_OTG_STS, SSUSB_CHG_B_ROLE_B);
	while (!g_otg_chg_b_role_b)
		os_ms_sleep(1);
	
	//clear all events
	otg_init();

	//enable interrupts. there may be cleared by host cleanup
	u3d_initialize_drv();

	#if 0 //fail
	//B-UUT issues an SRP to start a session with A-OPT
	os_printk(K_ERR, "Make sure SESSION = 0\n");	
	while(os_readl(U3D_DEVICE_CONTROL) & SESSION);
	#endif

	//remove termination
	//mu3d_hal_u2dev_disconn();

	os_printk(K_ERR, "Set session\n");
	os_setmsk(U3D_POWER_MANAGEMENT, SOFT_CONN);	
	os_setmsk(U3D_DEVICE_CONTROL, SESSION);

	//100ms after Vbus begins to decay, A-OPT powers Vbus
	cnt = OTG_A_OPT_DRIVE_VBUS_TMOUT;
	
	while (!(g_otg_vbus_chg && ((os_readl(U3D_DEVICE_CONTROL) & VBUS) == 0x18)))
	{
		os_ms_sleep(10);
		cnt -= 10;

		if (cnt <= 0)
		{
			err_msg = OTG_MSG_DEV_NOT_RESPONSE;
			os_printk(K_ERR, "A-OPT driving Vbus timeout\n");

			return 0;
		}
	};

	//after detecting Vbus, B-UUT connects to A-OPT
	mu3d_hal_set_speed(SSUSB_SPEED_HIGH);

	//wait for bus reset from A-OPT
	ret = otg_wait_event(OTG_EVENT_RESET);
	if (ret == OTG_TEST_STOP)
		return 0;

TD6_13:
	os_printk(K_ERR, "wait A-OPT suspend\n");
	while (!(g_otg_config && g_otg_suspend) && g_otg_exec)
	{
//		os_printk(K_ERR, "U3D_COMMON_USB_INTR: %x\n", os_readl(U3D_COMMON_USB_INTR));
//		os_printk(K_ERR, "U3D_COMMON_USB_INTR_ENABLE: %x\n", os_readl(U3D_COMMON_USB_INTR_ENABLE));

		os_ms_sleep(10);
	}
	if (!g_otg_exec)
		goto B_UUT_DONE;

	if (g_otg_resume)
	{
		g_otg_resume = 0;
		goto TD6_13;
	}
	
	os_printk(K_ERR, "HNP start\n");
	os_printk(K_ERR, "wait A-OPT connect\n");
	ret = otg_wait_event(OTG_EVENT_CONNECT_OR_RESUME);

	if (ret == OTG_TEST_STOP)
		return 0;
	else if (ret == OTG_RESUME)
	{
		//clear all events
		otg_init();
	
		goto TD6_13;
}
	else if (ret == OTG_HNP_TMOUT)
	{
		os_printk(K_ERR, "HNP timeout\n");

		os_clrmsk(U3D_DEVICE_CONTROL, HOSTREQ);

		if (g_otg_td_5_9)
			err_msg = OTG_MSG_DEV_NOT_RESPONSE;		
	}

	//wait B-UUT to become host
	os_printk(K_ERR, "Wait to become host (CHG_A_ROLE_B)\n");
	while (!(g_otg_chg_a_role_b && (os_readl(U3D_SSUSB_OTG_STS) & SSUSB_HOST_DEV_MODE)) && g_otg_exec)
		os_ms_sleep(1); 
	if (!g_otg_exec)
		goto B_UUT_DONE;

	//handover to xHCI
	os_printk(K_ERR, "Set CHG_A_ROLE_A\n");
	os_setmsk(U3D_SSUSB_OTG_STS, SSUSB_CHG_A_ROLE_A);

	//wait B-UUT to become device
	os_printk(K_ERR, "Wait to become device (CHG_B_ROLE_B)\n");
	while (!(g_otg_chg_b_role_b && !(os_readl(U3D_SSUSB_OTG_STS) & SSUSB_HOST_DEV_MODE)) && g_otg_exec)
		os_ms_sleep(1);
	if (!g_otg_exec)
		goto B_UUT_DONE;

	//wait for reset from A-OPT
	ret = otg_wait_event(OTG_EVENT_RESET);
	if (ret == OTG_TEST_STOP)
		return 0;

	if (g_otg_exec)
		goto TD6_13;


B_UUT_DONE:
	os_printk(K_ERR, "%s Done\n", __func__);	
	g_otg_exec = 0;
	#endif
	#endif
	return 0 ;
}

#endif
