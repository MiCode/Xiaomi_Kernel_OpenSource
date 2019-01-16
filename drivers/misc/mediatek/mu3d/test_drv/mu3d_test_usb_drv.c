#include <linux/mu3d/hal/mu3d_hal_osal.h>
#define _DEV_USB_DRV_EXT_
#include <linux/mu3d/test_drv/mu3d_test_usb_drv.h>
#undef _DEV_USB_DRV_EXT_
#include <linux/mu3d/hal/mu3d_hal_hw.h>
#include <linux/mu3d/hal/mu3d_hal_qmu_drv.h>
#include <linux/mu3d/hal/mu3d_hal_usb_drv.h>
#include <linux/mu3d/test_drv/mu3d_test_qmu_drv.h>

static u64 mu3d_if_dmamask = DMA_BIT_MASK(32);
static struct device mu3d_if_dev = { 
		.dma_mask               = &mu3d_if_dmamask,
		.coherent_dma_mask      = DMA_BIT_MASK(32),
};


// USBIF
#include <mach/battery_common.h>


extern void BATTERY_SetUSBState(int usb_state);
extern void wake_up_bat(void);

EP0_STATE g_ep0_state = EP0_IDLE;
DEV_UINT8* g_dma_buffer[2 * MAX_EP_NUM + 1];
dma_addr_t g_mu3d_dma_debug;
static DEV_UINT8 bAddress_Offset = 0;
DEV_UINT8* loopback_buffer;
extern DEV_UINT16 g_nirq;

void u3d_sync_with_bat(int usb_state)
{
	printk("[U3D_T] u3d_sync_with_bat, usb_state is %d\n", usb_state);
#ifndef CONFIG_MTK_FPGA
	BATTERY_SetUSBState(usb_state);
	wake_up_bat();
#endif
}


// 12 bytes
// USBIF, should change to ROME phone MTK vendor ID, product ID
DEV_UINT8 device_descriptor[] =
{
	0x12,
	0x01,
	0x00, //0x0200
	0x02,
	0x00,
	0x00,
	0x00,
	0x40,
	0x8D, //0x0E8D , Mediatek
	0x0E,
	0x03, //0x0C03, STORAGE + ADB
	0x0C,
	0x00, //0x0200
	0x02, 
	0x01,
	0x02,
	0x03,
	0x01
};

// 9 bytes
DEV_UINT8 configuration_descriptor[] = 
{
	0x09,
	0x02,
	0x25, //0x0025
	0x00,
	0x01,
	0x01,
	0x00,
	0x80, //set to bus power and not remote wakeup , orig : 0xc0,
	0xFA, // 0x32 : 100 mA , 0x60 : 192 mA , 0xfa : 500 mA
};

// 9 bytes
DEV_UINT8 interface_descriptor[] =
{
	0x09,
	0x04,
	0x00,
	0x00,
	0x02,
	0x08,
	0x06,
	0x50,
	0x00
};

// 7 bytes
DEV_UINT8 endpoint_descriptor_in[] =
{
	0x07,
	0x05,
	0x81,
	0x02,
	0x00, //0x0200
	0x02,
	0x00
};

// 7 bytes
DEV_UINT8 endpoint_descriptor_out[] =
{
	0x07,
	0x05,
	0x02,
	0x02,
	0x00, //0x0200
	0x02,
	0x00
};

// 5 bytes
DEV_UINT8 otg_descriptor[] = 
{
	0x05,
	0x09,
	0x03,
	0x00,
	0x02  // bcdOTG version
};

// 4 bytes
DEV_UINT8 string_descriptor_0[] = 
{
	0x04,
	0x03,
	0x09,
	0x04	
};

// 18 bytes
DEV_UINT8 string_descriptor_1[] = 
{
	0x12,
	0x03,
	0x4d,
	0x00,
	0x65,
	0x00,
	0x64,
	0x00,
	0x69,
	0x00,
	0x61,
	0x00,
	0x54,
	0x00,
	0x65,
	0x00,
	0x6b,
	0x00
};

// 44 bytes
DEV_UINT8 string_descriptor_2[] = 
{
	0x2a,
	0x03,
	0x4d,
	0x00,
	0x54,
	0x00,
	0x36,
	0x00,
	0x35,
	0x00,
	0x78,
	0x00,
	0x78,
	0x00,
	0x20,
	0x00,
	0x41,
	0x00,
	0x6e,
	0x00,
	0x64,
	0x00,
	0x72,
	0x00,
	0x6f,
	0x00,
	0x69,
	0x00,
	0x64,
	0x00,
	0x20,
	0x00,
	0x50,
	0x00,
	0x68,
	0x00,
	0x6f,
	0x00,
	0x6e,
	0x00,
	0x65,
	0x00
};

// 34 bytes
DEV_UINT8 string_descriptor_3[] = 
{
	0x22,
	0x03,
	0x30,
	0x00,
	0x31,
	0x00,
	0x32,
	0x00,
	0x33,
	0x00,
	0x34,
	0x00,
	0x35,
	0x00,
	0x36,
	0x00,
	0x37,
	0x00,
	0x38,
	0x00,
	0x39,
	0x00,
	0x41,
	0x00,
	0x42,
	0x00,
	0x43,
	0x00,
	0x44,
	0x00,
	0x45,
	0x00,
	0x46,
	0x00
};

DEV_UINT8 string_one[] = 
{
	0x01
};

DEV_UINT8 string_zero[] = 
{
	0x00
};


/**
 * u3d_init_ctrl - initialize ep0 ctrl req
 *
 */
void u3d_init_ctrl(void)
{
	struct USB_REQ *req;

	os_printk(K_ERR, "%s\n", __func__);

	req = mu3d_hal_get_req(0, USB_TX);
	req->count = USB_BUF_SIZE;
	req->complete = 0;
	req->actual = 0;
	req->needZLP = 0;
}


/**
 * u3d_init - initialize mac & qmu/bmu
 *
 */
	DEV_UINT8 *isrbuffer;
 DEV_UINT8 *isrbuffer1;
 DEV_UINT8 *isrbuffer2;

void u3d_init_mem(void){ 
	isrbuffer = (DEV_UINT8 *)os_mem_alloc(10);
	u3d_allocate_ep0_buffer();
	u3d_alloc_req();	
	//u3d_rst_request();	

#if (BUS_MODE==QMU_MODE)
	printk("mu3d_hal_alloc_qmu_mem\n");
	/* Initialize QMU GPD/BD memory. */
	mu3d_hal_alloc_qmu_mem();
	
#endif	

	isrbuffer1 = (DEV_UINT8 *)os_mem_alloc(10);
	os_printk(K_ERR, "isrbuffer1: %p\n", isrbuffer1);

	isrbuffer2 = (DEV_UINT8 *)os_mem_alloc(10);	
	os_printk(K_ERR, "isrbuffer2: %p\n", isrbuffer2);

}
void u3d_init(void){	

	/* disable ip power down, disable U2/U3 ip power down */
	// USBIF , in ROME real chip , we should disable VBUS_FRC_EN
	//os_writel(U3D_MISC_CTRL, 0x0); 
	
	mu3d_hal_ssusb_en();
	/* reset U3D all dev module. */
	mu3d_hal_rst_dev();
	/* apply default register values */
	mu3d_hal_dft_reg();

	/* register U3D ISR. */
	mu3d_hal_initr_dis();
	mu3d_hal_clear_intr() ;

	/* register SSUSB_DEV_INT */
	//USBIF isrbuffer = (DEV_UINT8 *)os_mem_alloc(10);
	if(OS_R_OK != os_reg_isr((DEV_UINT16)g_nirq, u3d_inter_handler, isrbuffer)){
		os_printk(K_ERR,"Roll: Can't register IRQ %d\n", g_nirq);
		return;
  	}
  	else{
		os_printk(K_DEBUG,"Register IRQ %d\n", g_nirq);
  	}
  	os_printk(K_DEBUG,"USB Disable IRQ: %d\n", g_nirq);
  	os_disableIrq(g_nirq);
  	g_usb_irq = 0;

	//USBIF u3d_allocate_ep0_buffer();
	//USBIF u3d_alloc_req();	
	u3d_rst_request();

//USBIF# if (BUS_MODE==QMU_MODE)
	//USBIF printk("mu3d_hal_alloc_qmu_mem\n");
	/* Initialize QMU GPD/BD memory. */
	//USBIF mu3d_hal_alloc_qmu_mem();
//USBIF #endif
	
	/* Initialize usb speed. */
	mu3d_hal_set_speed(U3D_DFT_SPEED);
	/* Detect usb speed. */
	//speed depends on host/cable/device; so speed chk is bypassed
	//mu3d_hal_det_speed(U3D_DFT_SPEED, 0);
	os_ms_delay(1000);
	
    /* initialize usb ep0 & system */
	u3d_irq_en();
	u3d_initialize_drv();    	
#if (BUS_MODE==QMU_MODE)	
	/* Initialize QMU module. */
	mu3d_hal_init_qmu();
#endif
#ifdef POWER_SAVING_MODE
	mu3d_hal_pdn_cg_en();
#endif

	// USBIF , in ROME real chip , we should disable VBUS_FRC_EN
	os_writel(U3D_MISC_CTRL, 0); 

	#ifdef EXT_VBUS_DET
	// USBIF, in ROME real chip , this should be change to use polling vbus method or .... call back from PMIC
	os_writel(FPGA_REG, (os_readl(FPGA_REG) &~ VBUS_MSK ) | VBUS_FALL_BIT);

	/* register SSUSB_VBUS_RISE_INT */
	//USBIF isrbuffer1 = (DEV_UINT8 *)os_mem_alloc(10);
	//USBIF os_printk(K_ERR, "isrbuffer1: %x\n", isrbuffer1);

	if(ret = os_reg_isr((DEV_UINT16)VBUS_RISE_IRQ, u3d_vbus_rise_handler, isrbuffer1)){
		os_printk(K_ERR,"Roll: Can't register IRQ %d, error code: %d\n", VBUS_RISE_IRQ, ret);
		return;
  	}
  	else{
		os_printk(K_DEBUG,"Register IRQ %d\n", VBUS_RISE_IRQ);
  	}
  	os_printk(K_DEBUG,"USB Disable IRQ: %d\n", VBUS_RISE_IRQ);
  	os_disableIrq(VBUS_RISE_IRQ);
  	os_enableIrq(VBUS_RISE_IRQ);

	/* register SSUSB_VBUS_FALL_INT */
	//USBIF isrbuffer2 = (DEV_UINT8 *)os_mem_alloc(10);	
	//USBIF os_printk(K_ERR, "isrbuffer2: %x\n", isrbuffer2);

	if(ret = os_reg_isr((DEV_UINT16)VBUS_FALL_IRQ, u3d_vbus_fall_handler, isrbuffer2)){
		os_printk(K_ERR,"Roll: Can't register IRQ %d, error code: %d\n", VBUS_FALL_IRQ, ret);
		return;
  	}
  	else{
		os_printk(K_DEBUG,"Register IRQ %d\n", VBUS_FALL_IRQ);
  	}
  	os_printk(K_DEBUG,"USB Disable IRQ: %d\n", VBUS_FALL_IRQ);
  	os_disableIrq(VBUS_FALL_IRQ);
  	os_enableIrq(VBUS_FALL_IRQ);  	
  	#endif
}

void u3d_deinit(void){		
	/* register U3D ISR. */
	mu3d_hal_initr_dis();
	mu3d_hal_clear_intr();

	/* release SSUSB_DEV_INT */
  	os_disableIrq(g_nirq);	
	os_free_isr((DEV_UINT16)g_nirq, isrbuffer) ;
  	
	#ifdef EXT_VBUS_DET
	// KKKKK, in ROME real chip , this should be change to use polling vbus method or .... call back from PMIC
	/* release EXT VBUS RISE INT */
	os_disableIrq(VBUS_RISE_IRQ);	
	os_free_isr((DEV_UINT16)VBUS_RISE_IRQ, isrbuffer1) ;
	/* release EXT VBUS FALL INT */  	
  	os_disableIrq(VBUS_FALL_IRQ);
  	os_free_isr((DEV_UINT16)VBUS_FALL_IRQ, isrbuffer2) ;  	

  	#endif

	/* reset U3D all dev module. */
	//mu3d_hal_rst_dev();
  	
}


void u3d_irq_en(void){
    os_printk(K_ERR, "%s\n", __func__);
    os_writel(U3D_LV1IESR, 0xFFFFFFFF);
    os_enableIrq(g_nirq);
    g_usb_irq = 1;
}

static void u3d_free_dma0(void){

    DEV_INT32 ep_index, ep_num;
    struct USB_EP_SETTING *ep_setting;

	os_writel(U3D_EP0DMACTRL, 0);
	os_writel(U3D_EP0DMASTRADDR, 0);
 	os_writel(U3D_EP0DMATFRCOUNT, 0);
	ep_index = ep_num =0;
    ep_setting = &g_u3d_setting.ep_setting[ep_index];

    return;
}



void u3d_power_mode(DEV_INT32 mode, DEV_INT8 u1_value, DEV_INT8 u2_value, DEV_INT8 en_u1, DEV_INT8 en_u2){

	DEV_INT32 temp;

	printk("mode : 0x%08X\n",mode);
	printk("u1_value : 0x%08X\n",u1_value);
	printk("u2_value : 0x%08X\n",u2_value);
	printk("en_u1 : 0x%08X\n",en_u1);
	printk("en_u2 : 0x%08X\n",en_u2);

	if((mode == 0)||(mode == 4)){
		os_writel(U3D_LINK_POWER_CONTROL, 0);
	}
	if(mode == 1){
		//os_writel(U3D_LINK_POWER_CONTROL, os_readl(U3D_LINK_POWER_CONTROL) | LGO_U1);
		os_writel(U3D_LINK_POWER_CONTROL, os_readl(U3D_LINK_POWER_CONTROL) & ~(SW_U1_REQUEST_ENABLE | SW_U2_REQUEST_ENABLE));
		os_writel(U3D_LINK_POWER_CONTROL, os_readl(U3D_LINK_POWER_CONTROL) | SW_U1_REQUEST_ENABLE);
		temp = os_readl(U3D_LINK_UX_INACT_TIMER);
		temp &= ~U1_INACT_TIMEOUT_VALUE;
		temp |=  u1_value;
		os_writel(U3D_LINK_UX_INACT_TIMER, temp);
	}
	if(mode == 2){
		//os_writel(U3D_LINK_POWER_CONTROL, os_readl(U3D_LINK_POWER_CONTROL) | LGO_U2);
		os_writel(U3D_LINK_POWER_CONTROL, os_readl(U3D_LINK_POWER_CONTROL) & ~(SW_U1_REQUEST_ENABLE | SW_U2_REQUEST_ENABLE));
		os_writel(U3D_LINK_POWER_CONTROL, os_readl(U3D_LINK_POWER_CONTROL) | SW_U2_REQUEST_ENABLE);
		temp = os_readl(U3D_LINK_UX_INACT_TIMER);
		temp &= ~DEV_U2_INACT_TIMEOUT_VALUE;
		temp |=  (u2_value<<16);
		os_writel(U3D_LINK_UX_INACT_TIMER, temp);
	}
	if(mode == 3){

		if(en_u1){

			os_writel(U3D_LINK_POWER_CONTROL, os_readl(U3D_LINK_POWER_CONTROL) & ~(SW_U1_REQUEST_ENABLE | SW_U2_REQUEST_ENABLE));
			os_writel(U3D_LINK_POWER_CONTROL, os_readl(U3D_LINK_POWER_CONTROL) | SW_U1_REQUEST_ENABLE);
			temp = os_readl(U3D_LINK_UX_INACT_TIMER);
			temp &= ~U1_INACT_TIMEOUT_VALUE;
			temp |=  u1_value;
			os_writel(U3D_LINK_UX_INACT_TIMER, temp);
			while(!((os_readl(U3D_LINK_STATE_MACHINE)&LTSSM)==STATE_U1_STATE));
			os_ms_delay(500);
			os_writel(U3D_LINK_UX_INACT_TIMER, 0);
			os_writel(U3D_LINK_POWER_CONTROL, os_readl(U3D_LINK_POWER_CONTROL) | UX_EXIT);
			while((os_readl(U3D_LINK_POWER_CONTROL) & UX_EXIT));

		}
		if(en_u2){

			os_writel(U3D_LINK_POWER_CONTROL, os_readl(U3D_LINK_POWER_CONTROL) & ~(SW_U1_REQUEST_ENABLE | SW_U2_REQUEST_ENABLE));
			os_writel(U3D_LINK_POWER_CONTROL, os_readl(U3D_LINK_POWER_CONTROL) | SW_U2_REQUEST_ENABLE);
			temp = os_readl(U3D_LINK_UX_INACT_TIMER);
			temp &= ~DEV_U2_INACT_TIMEOUT_VALUE;
			temp |=  (u2_value<<16);
			os_writel(U3D_LINK_UX_INACT_TIMER, temp);
			while(!((os_readl(U3D_LINK_STATE_MACHINE)&LTSSM)==STATE_U2_STATE));
			os_ms_delay(500);
			os_writel(U3D_LINK_UX_INACT_TIMER, 0);
			os_writel(U3D_LINK_POWER_CONTROL, os_readl(U3D_LINK_POWER_CONTROL) | UX_EXIT);
			while((os_readl(U3D_LINK_POWER_CONTROL) & UX_EXIT));
		}
	}
	if(en_u1){
		os_writel(U3D_LINK_POWER_CONTROL, os_readl(U3D_LINK_POWER_CONTROL) | SW_U1_ACCEPT_ENABLE | SW_U1_REQUEST_ENABLE);
	}
	if(en_u2){
		os_writel(U3D_LINK_POWER_CONTROL, os_readl(U3D_LINK_POWER_CONTROL) | SW_U2_ACCEPT_ENABLE | SW_U2_REQUEST_ENABLE);
	}
	if(mode == 4){
		os_ms_delay(200);
		os_writel(U3D_LINK_POWER_CONTROL, os_readl(U3D_LINK_POWER_CONTROL) | UX_EXIT);
	}
}

DEV_UINT8 u3d_transfer_complete(DEV_INT32 ep_num, USB_DIR dir){
    DEV_INT32 ep_index=0;

    if(dir == USB_TX){
        ep_index = ep_num;
    }
    else if(dir == USB_RX){
        ep_index = ep_num + MAX_EP_NUM;
    }
    else{
        BUG_ON(1);
    }

    return g_u3d_req[ep_index].complete;
}

DEV_UINT8 req_complete(DEV_INT32 ep_num, USB_DIR dir){
    struct USB_REQ *req = mu3d_hal_get_req(ep_num, dir);
	os_ms_delay(1);
    if(req->complete){
        return true;
    }
    else{
        return false;
    }
}


/**
 * u3d_config_dma0 - config ep0 dma
 *
 */
static void u3d_config_dma0(DEV_INT32 burst_mode, DEV_INT32 dir, DEV_INT32 addr, DEV_INT32 count){

    DEV_UINT32 usb_dma_cntl = 0;
	#if defined(USB_RISC_CACHE_ENABLED)
    os_flushinvalidateDcache();
	#endif
	os_printk(K_DEBUG, "u3d_config_dma0\n");
	os_printk(K_DEBUG, "DMA CTRL0 :%d\n", usb_dma_cntl);
	os_printk(K_DEBUG, "addr: %x\n",addr);
	os_printk(K_DEBUG, "count: %x\n",count);

	usb_dma_cntl = ((dir&0x1)<<1) | INTEN;
 	os_writel(U3D_EP0DMACTRL, usb_dma_cntl);
 	os_writel(U3D_EP0DMASTRADDR, addr);
 	os_writel(U3D_EP0DMATFRCOUNT, count);
// USBIF 
 	/*<Caution> DMA_OUTSTAND_NUM at U3D_EP0DMARLCOUNT should NOT clear to 0. If so, DMA does NOT work anymore.*/
 	os_writel(U3D_EP0DMARLCOUNT, os_readl(U3D_EP0DMARLCOUNT) | ((burst_mode&0x3) << DMA_BURST_OFST));
 	os_writel(U3D_EP0DMACTRL, os_readl(U3D_EP0DMACTRL)|DMA_EN);
}


/**
 * u3d_ep0en - enable ep0 function
 *
 */
void u3d_ep0en(void)
{
	DEV_UINT32 temp;
	struct USB_EP_SETTING *ep_setting;

	os_printk(K_ERR, "%s\n", __func__);

	ep_setting = &g_u3d_setting.ep_setting[0];
	ep_setting->transfer_type = USB_CTRL;
	ep_setting->dir = USB_TX;
	ep_setting->fifoaddr = 0;
	ep_setting->enabled = 1;
	if((os_readl(U3D_DEVICE_CONF) & SSUSB_DEV_SPEED) == SSUSB_SPEED_SUPER)
	{
		ep_setting->fifosz = 512;
		ep_setting->maxp = 512;
	}
	else
	{
		ep_setting->fifosz = 64;
		ep_setting->maxp = 64;
	}

	//EP0CSR
	temp = ep_setting->maxp;
	#ifdef AUTOSET
	temp |= EP0_AUTOSET;
	#endif
	#ifdef AUTOCLEAR
	temp |= EP0_AUTOCLEAR;
	#endif
	//leave this bit on so that EP0 flow can switch between PIO & DMA easily
	//there is no EP0 DMA interrupt event under PIO mode
	//temp |= ((g_ep0_mode == PIO_MODE) ? EP0_DMAREQEN : 0);
	#if (EP0_BUS_MODE==DMA_MODE)
	temp |= EP0_DMAREQEN;
	#endif
	os_writel(U3D_EP0CSR, temp);

	//enable EP0 interrupts
	os_setmsk(U3D_EPIESR, (EP0ISR | SETUPENDISR));

	return;
}


void u3d_allocate_ep0_buffer(void)
{
	g_dma_buffer[0] = (DEV_UINT8 *)os_mem_alloc(USB_BUF_SIZE);
}

void u3d_initialize_drv(void){
    DEV_INT32 i;
    USB_SPEED speed = g_u3d_setting.speed;

	os_printk(K_ERR, "%s\n", __func__);

    /* initialize ep fifo address*/
    g_TxFIFOadd = USB_TX_FIFO_START_ADDRESS;
    g_RxFIFOadd = USB_RX_FIFO_START_ADDRESS;
    /* initialize ep0 state*/
    g_ep0_state = EP0_IDLE;
    /* initialize test setting and test status structures */
	os_memset((DEV_UINT32 *)(&g_u3d_setting), 0 , sizeof(struct USB_TEST_SETTING));
    g_u3d_setting.speed = speed; /* reserve speed setting */
	os_memset((DEV_UINT32 *)(&g_usb_status), 0 , sizeof(struct USB_TEST_STATUS));

    for(i = 0; i < 2 * MAX_EP_NUM + 1; i++){
		os_memset((DEV_UINT32 *)(&g_u3d_req[i]), 0 , sizeof(struct USB_REQ));
    }
	g_u3d_req[0].buf = g_dma_buffer[0];
	g_u3d_status = READY;

	//enable system global interrupt
	mu3d_hal_system_intr_en();

	//initialize EP0
	u3d_init_ctrl();
	u3d_ep0en();

	g_run_stress = false;
	g_insert_hwo = false;

#if !defined(USB_RISC_CACHE_ENABLED)
	os_disableDcache();
#endif
}

void u3d_set_address(DEV_INT32 addr){

	os_writel(U3D_DEVICE_CONF, (addr<<DEV_ADDR_OFST));
    return;
}

void u3d_rxep_dis(DEV_INT32 ep_num){
	os_writel(U3D_EPIECR, os_readl(U3D_EPIECR)|(BIT16 << ep_num));
}

/**
 * u3d_ep_start_transfer - epn start to transfer data, do not be used in qmu mode
 *@args -arg1:ep number, arg2: dir
 */
void u3d_ep_start_transfer(DEV_INT32 ep_num, USB_DIR dir){

    DEV_INT32 ep_index=0, length, maxp;
    struct USB_EP_SETTING *ep_setting;
    struct USB_REQ *req;
	DEV_UINT8 *bp;
	DEV_UINT8 need_zlp;

    if(dir == USB_TX){
        ep_index = ep_num;
    }
    else if(dir == USB_RX){
        ep_index = ep_num + MAX_EP_NUM;
    }
    else{
        BUG_ON(1);
    }

    ep_setting = &g_u3d_setting.ep_setting[ep_index];
    req = &g_u3d_req[ep_index];

    if(ep_setting->enabled){

        if((dir == USB_TX) && (ep_num != 0)){

			os_writel(U3D_EPIESR, os_readl(U3D_EPIESR)|(BIT0 << ep_num));
            req->actual = 0;
            req->complete = 0;

            #if (BUS_MODE==PIO_MODE)
			bp = req->buf + req->actual;

			maxp = ep_setting->maxp;
			need_zlp = 0;
			if(req->count - req->actual > maxp){
				length = ep_setting->maxp;
			}
			else{
				length = req->count - req->actual;
			}
			req->actual += length;

			need_zlp = ((req->actual == req->count) && (!(req->count % maxp)) && ep_num!=0);
			mu3d_hal_write_fifo(ep_num, length, bp, maxp);

			/* Here is really tricky, need to print this log to pass EPn PIO loopback
			 * No time to figure out why. Sorry~
			 */
			printk("zlp=%d\n",need_zlp);
			if(need_zlp) {
			 	while(!(USB_ReadCsr32(U3D_TX1CSR0, ep_num) & TX_FIFOFULL)) {
					os_printk(K_ALET, "send zlp\n");
					mu3d_hal_write_fifo(ep_num, 0, bp, maxp);
					break;
			 	}
			}
            #endif
        }
        else if(dir == USB_RX){
            req->actual = 0;
            req->complete = 0;
            req->count = USB_BUF_SIZE;
            os_writel(U3D_EPIESR, os_readl(U3D_EPIESR)|(BIT16 << ep_num));
        }
        else{
            BUG_ON(1);
        }
    } else {
		os_printk(K_ALET,"EP%d is not enabled\n", ep_num);
        BUG_ON(1);
    }

    return;
}


DEV_UINT8 u3d_command(void){
	return Request->bCommand;
}

void *u3d_req_buffer(void){
	return AT_CMD->buffer;
}

void u3d_alloc_req(void){
	Request=(DEV_REQ *)os_mem_alloc(sizeof(DEV_REQ));
	Request->buffer=(DEV_UINT8 *)os_mem_alloc(2048);
	AT_CMD=(DEV_AT_CMD *)os_mem_alloc(sizeof(DEV_AT_CMD));
	AT_CMD->buffer=(DEV_UINT8 *)os_mem_alloc(2048);
}


DEV_UINT8 u3d_req_valid(void){
	return Request->bValid;
}

void u3d_rst_request(void)
{
	os_printk(K_ERR, "%s\n", __func__);

	Request->bmRequestType=0;
	Request->bRequest=0;
	Request->wValue=0;
	Request->wIndex=0;
	Request->wLength=0;
	Request->bValid=0;
}

void dev_power_mode(DEV_INT32 mode, DEV_INT8 u1_value, DEV_INT8 u2_value, DEV_INT8 en_u1, DEV_INT8 en_u2){
  	u3d_power_mode(mode,u1_value,u2_value,en_u1, en_u2);
}

void dev_send_one_packet(DEV_INT32 ep_tx){

	struct USB_REQ *req;
	dma_addr_t mapping;
    DEV_UINT8* dma_buf;

	req = mu3d_hal_get_req(ep_tx, USB_TX);
	dma_buf = g_loopback_buffer[0];
	req->buf = g_loopback_buffer[0];
	os_memset(req->buf, 0 , 1000000);
	mapping = dma_map_single(NULL, dma_buf,g_dma_buffer_size, DMA_BIDIRECTIONAL);
	dma_sync_single_for_device(NULL, mapping, g_dma_buffer_size, DMA_BIDIRECTIONAL);
	req->dma_adr = mapping;
	req->count = 1024;
	mu3d_hal_insert_transfer_gpd(ep_tx,USB_TX, req->dma_adr, req->count , true, true, false, false, 1024);
	mu3d_hal_resume_qmu(ep_tx, USB_TX);
	mapping=req->dma_adr;
	dma_sync_single_for_cpu(NULL, mapping, g_dma_buffer_size, DMA_BIDIRECTIONAL);
	dma_unmap_single(NULL, mapping, g_dma_buffer_size,DMA_BIDIRECTIONAL);
}

void dev_send_erdy(DEV_INT8 opt,DEV_INT32 ep_rx , DEV_INT32 ep_tx){
	if(opt==6){
		//send ERDY until LTSSM goes to U1/U2
		while ((os_readl(U3D_LINK_STATE_MACHINE)&LTSSM) == STATE_U0_STATE);
		os_writel(U3D_USB3_SW_ERDY, (ep_tx<<2)|SW_SEND_ERDY);
	}
}

void dev_receive_ep0_test_packet(DEV_INT8 opt){
	unsigned long flags;

	if(opt==1){

		spin_lock_irqsave(&_lock, flags);
		os_writel(U3D_EP0CSR, os_readl(U3D_EP0CSR) & (~EP0_DMAREQEN));
		while(!(os_readl(U3D_EP0CSR)&EP0_SETUPPKTRDY));
		mu3d_hal_read_fifo(0,g_u3d_req[0].buf);
		os_ms_delay(3000);
		os_writel(U3D_EP0CSR,os_readl(U3D_EP0CSR) | EP0_SETUPPKTRDY | EP0_DATAEND);
		os_writel(U3D_EP0CSR, os_readl(U3D_EP0CSR) | EP0_DMAREQEN); // protect for PIO mode
		spin_unlock_irqrestore(&_lock, flags);
	}
}


void dev_u1u2_en_cond(DEV_INT8 opt,DEV_INT8 cond,DEV_INT32 ep_rx , DEV_INT32 ep_tx){

	struct USB_REQ *treq, *rreq;
	dma_addr_t mapping;
	DEV_UINT32 temp, maxp;
    DEV_UINT8* dma_buf,zlp;

	rreq = mu3d_hal_get_req(ep_rx, USB_RX);
	treq = mu3d_hal_get_req(ep_tx, USB_TX);
	dma_buf = g_loopback_buffer[0];
	treq->buf =rreq->buf =g_loopback_buffer[0];
	os_memset(rreq->buf, 0 , 1000000);
	mapping = dma_map_single(NULL, dma_buf,g_dma_buffer_size, DMA_BIDIRECTIONAL);
	dma_sync_single_for_device(NULL, mapping, g_dma_buffer_size, DMA_BIDIRECTIONAL);
	treq->dma_adr=rreq->dma_adr=mapping;
	treq->count=rreq->count=0x1000;
	zlp = (USB_ReadCsr32(U3D_TX1CSR1, ep_tx) & TYPE_ISO) ? false : true;
	maxp = USB_ReadCsr32(U3D_RX1CSR0, ep_rx) & RX_RXMAXPKTSZ;

	if(cond){
		if(opt==2){
			mu3d_hal_insert_transfer_gpd(ep_tx,USB_TX, treq->dma_adr,treq->count , true, true, false, zlp, maxp);
			mu3d_hal_resume_qmu(ep_tx, USB_TX);
		}
		if(opt==3){
			mu3d_hal_insert_transfer_gpd(ep_rx,USB_RX, rreq->dma_adr, rreq->count, true, true, false, zlp, maxp);
			mu3d_hal_resume_qmu(ep_rx, USB_RX);
		}
		if(opt==4){
			temp = USB_ReadCsr32(U3D_TX1CSR0, ep_tx) & 0xFFFEFFFF;
			USB_WriteCsr32(U3D_TX1CSR0, ep_tx, (temp &~ TX_DMAREQEN) | TX_AUTOSET);
			os_writel(U3D_QGCSR, 0);
			os_memset(treq->buf, 0xff , 1000000);
			treq->count=2048;
			mu3d_hal_write_fifo_burst(ep_tx,treq->count,treq->buf, maxp);
		}
	} else {
			mu3d_hal_stop_qmu(ep_tx, USB_TX);
			while((os_readl(USB_QMU_TQCSR(ep_tx)) & (QMU_Q_ACTIVE)));
			mu3d_hal_stop_qmu(ep_rx, USB_RX);
			while((os_readl(USB_QMU_RQCSR(ep_rx)) & (QMU_Q_ACTIVE)));
	}
	mapping=rreq->dma_adr;
	dma_sync_single_for_cpu(NULL, mapping, g_dma_buffer_size, DMA_BIDIRECTIONAL);
	dma_unmap_single(NULL, mapping, g_dma_buffer_size,DMA_BIDIRECTIONAL);
}

void dev_u1u2_en_ctrl(DEV_INT8 type,DEV_INT8 u_num,DEV_INT8 opt,DEV_INT8 cond,DEV_INT8 u1_value, DEV_INT8 u2_value){

	DEV_INT32 temp,ux_en_ctrl,ux_base;

os_printk(K_ALET,"type :%d\n",type);
os_printk(K_ALET,"u_num :%d\n",u_num);
os_printk(K_ALET,"opt :%d\n",opt);
os_printk(K_ALET,"cond :%d\n",cond);
os_printk(K_ALET,"value1 :%d\n",u1_value);
os_printk(K_ALET,"value2 :%d\n",u2_value);


	ux_en_ctrl = (u_num == 1) ? U3D_MAC_U1_EN_CTRL : U3D_MAC_U2_EN_CTRL;
	ux_base = (type == 2) ? 16 : 0;

	if(opt==6){
		temp = (!cond) ? cond : EXIT_BY_ERDY_DIS;
		os_writel(ux_en_ctrl, temp);
	}
	else if(opt==1){
		temp = (!cond) ? cond : 1<<(opt+ux_base-1);
		os_writel(ux_en_ctrl, temp);
	}
	else{
		if(opt){
			os_writel(ux_en_ctrl, 1<<(opt+ux_base-1));
		}
	}

	if(type == 1){// request

		if(u_num == 1){
			os_writel(U3D_LINK_POWER_CONTROL, SW_U1_REQUEST_ENABLE);
		}
		if(u_num == 2){
			os_writel(U3D_LINK_POWER_CONTROL, SW_U2_REQUEST_ENABLE);
		}
		temp = os_readl(U3D_LINK_UX_INACT_TIMER);
		temp &= ~(DEV_U2_INACT_TIMEOUT_VALUE | U1_INACT_TIMEOUT_VALUE);
		temp |= ((u1_value)|(u2_value<<16));
		os_writel(U3D_LINK_UX_INACT_TIMER, temp);
	}
	if(type == 2){// accept
		if(u_num == 1){
			os_writel(U3D_LINK_POWER_CONTROL, SW_U1_ACCEPT_ENABLE);
		}
		if(u_num == 2){
			os_writel(U3D_LINK_POWER_CONTROL, SW_U2_ACCEPT_ENABLE);
		}
	}
	if(type == 3){// end
		os_writel(U3D_LINK_UX_INACT_TIMER, 0);
		os_writel(U3D_LINK_POWER_CONTROL, 0);
		os_writel(U3D_MAC_U1_EN_CTRL, 0);
		os_writel(U3D_MAC_U2_EN_CTRL, 0);
	}
}

#define STS_CHK_CLEAR 0
#define STS_CHK_U1 1
#define STS_CHK_U2 2
#define STS_CHK_U1_REJECT 3
#define STS_CHK_U2_REJECT 4
#define STS_CHK_HOT_RST 5
#define STS_CHK_WARM_RST 6
#define STS_CHK_FORCE_LINK_PM_ACPT 7
#define STS_CHK_RX_LEN_ERR 8
DEV_INT8 dev_stschk(DEV_INT8 type, DEV_INT8 change){
	DEV_UINT32 cnt = 0;

	switch (type)
	{
		case STS_CHK_CLEAR: //clear mode
			//reset counter
			#ifdef SUPPORT_U3
			if (!(os_readl(U3D_SSUSB_U3_CTRL_0P) & SSUSB_U3_PORT_PDN))
			{
				os_writel(U3D_USB3_U1_STATE_INFO, CLR_USB3_U1_CNT);
				os_writel(U3D_USB3_U2_STATE_INFO, CLR_USB3_U2_CNT);
				os_writel(U3D_USB3_U1_REJECT, CLR_USB3_U1_REJECT_CNT);
				os_writel(U3D_USB3_U2_REJECT, CLR_USB3_U2_REJECT_CNT);
			}

			g_hot_rst_cnt = 0;
			g_warm_rst_cnt = 0;
			#endif
			g_rx_len_err_cnt = 0;
			break;
		#ifdef SUPPORT_U3
		case STS_CHK_U1: //U1
			cnt = os_readl(U3D_USB3_U1_STATE_INFO)&USB3_U1_CNT;
			break;
		case STS_CHK_U2: //U2
			cnt = os_readl(U3D_USB3_U2_STATE_INFO)&USB3_U2_CNT;
			break;
		case STS_CHK_U1_REJECT: //U1_reject
			cnt = os_readl(U3D_USB3_U1_REJECT)&USB3_U1_REJECT_CNT;
			break;
		case STS_CHK_U2_REJECT: //U2_reject
			cnt = os_readl(U3D_USB3_U2_REJECT)&USB3_U2_REJECT_CNT;
			break;
		case STS_CHK_HOT_RST: //hot reset
			cnt = g_hot_rst_cnt;
			break;
		case STS_CHK_WARM_RST: //warm reset
			cnt = g_warm_rst_cnt;
			break;
		case STS_CHK_FORCE_LINK_PM_ACPT: //force link pm
			cnt = (os_readl(U3D_HOST_SET_PORT_CTRL) & FORCE_LINK_PM_ACPT) ? 1 : 0;
			break;
		#endif
		case STS_CHK_RX_LEN_ERR: //RX length error
			cnt = g_rx_len_err_cnt;
			break;
	}


	//cnt should be greater than 0 if change is expected
	//cnt should be 0 if change is not expected
	os_printk(K_NOTICE, "type: %d, change: %d, cnt: %x\n", type, change, cnt);
	if ((change && cnt) || (!change && !cnt) || (type == 0))
		return RET_SUCCESS; //RET_SUCCESS
	else
		return RET_FAIL; //RET_FAIL
}

void dev_lpm_config_dev(LPM_INFO *lpm_info){
	#define LPM_MODE_NORMAL 0
	#define LPM_MODE_FRC_REJECT 1
	#define LPM_MODE_FRC_ACCEPT 2
	#define LPM_MODE_FRC_TIMEOUT 3
	#define LPM_MODE_FRC_STALL 4
	#define LPM_MODE_HW_LPM 5
	#define LPM_RESUME_HOST 0
	#define LPM_RESUME_DEVICE_SW 1
	#define LPM_RESUME_DEVICE_HW 2
	#define LPM_RESUME_DEVICE_SW_2 3
	#define LPM_INACT_EP0 0
	#define LPM_INACT_TXQ 1
	#define LPM_INACT_RXQ 2
	#define LPM_INACT_BMU_TX 3
	#define LPM_INACT_BMU_RX 4

	DEV_UINT32 dwTemp;

	os_printk(K_ALET,"dev_lpm_config_dev\n");
	os_printk(K_ALET,"lpm_mode: %d\n",lpm_info->lpm_mode);
	os_printk(K_ALET,"wakeup: %d\n",lpm_info->wakeup);
	os_printk(K_ALET,"beslck: %d\n",lpm_info->beslck);
	os_printk(K_ALET,"beslck_u3: %d\n",lpm_info->beslck_u3);
	os_printk(K_ALET,"besldck: %d\n",lpm_info->besldck);
	os_printk(K_ALET,"cond: %d\n",lpm_info->cond);
	os_printk(K_ALET,"cond_en: %d\n",lpm_info->cond_en);
	os_printk(K_ALET,"\n");


	//LPM_MODE
	os_writel(U3D_POWER_MANAGEMENT,
		(os_readl(U3D_POWER_MANAGEMENT) &~ LPM_MODE)
		| (((lpm_info->lpm_mode == LPM_MODE_HW_LPM) ? LPM_MODE_NORMAL : lpm_info->lpm_mode)<<8));

	//LPM_FORCE_STALL
	os_writel(U3D_USB2_TEST_MODE,
		(os_readl(U3D_USB2_TEST_MODE) & ~(FIFO_ACCESS | LPM_FORCE_STALL))
		| ((lpm_info->lpm_mode == LPM_MODE_FRC_STALL) ? LPM_FORCE_STALL : 0));


	//RESUME method
	//HRWE
	#if 1
	os_writel(U3D_POWER_MANAGEMENT,
		(os_readl(U3D_POWER_MANAGEMENT) &~ LPM_HRWE)
		| ((lpm_info->wakeup == LPM_RESUME_DEVICE_HW) ? LPM_HRWE : 0));
	#else
	//enable HRWE by default; let RWE bit in LPM token decides if remote wakeup is enabled
	os_writel(U3D_POWER_MANAGEMENT, os_readl(U3D_POWER_MANAGEMENT) | LPM_HRWE);
	#endif
	//EXIT CHK
	os_writel(U3D_USB2_EPCTL_LPM,
		((lpm_info->wakeup == LPM_RESUME_DEVICE_HW) ?
		(L1_EXIT_EP0_CHK | L1_EXIT_EP_IN_CHK | L1_EXIT_EP_OUT_CHK) : 0));

	//SW REMOTE WAKEUP
	g_sw_rw = (lpm_info->wakeup == LPM_RESUME_DEVICE_SW) ? 1 : 0;
	//SW REMOTE WAKEUP TEST MODE, drive resume before entering suspend
	if (lpm_info->wakeup == LPM_RESUME_DEVICE_SW_2)
	{
		os_writel(U3D_POWER_MANAGEMENT, os_readl(U3D_POWER_MANAGEMENT) | RESUME);
	}
	//HW REMOTE WAKEUP
	g_hw_rw = (lpm_info->wakeup == LPM_RESUME_DEVICE_HW) ? 1 : 0;


	//BESLCK <= BESLCK_U3 <= BESLDCK
	dwTemp = (((DEV_UINT32)lpm_info->beslck_u3<<BESLCK_U3_OFST) & BESLCK_U3)
		| (((DEV_UINT32)lpm_info->beslck<<BESLCK_OFST) & BESLCK)
		| (((DEV_UINT32)lpm_info->besldck<<BESLDCK_OFST) & BESLDCK);
	os_writel(U3D_USB20_LPM_PARAMETER, dwTemp);

	//STALL or NYET
	os_writelmsk(U3D_POWER_MANAGEMENT,
		(lpm_info->beslck&0x10) ? LPM_BESL_STALL : 0, LPM_BESL_STALL);
	os_writelmsk(U3D_POWER_MANAGEMENT,
		(lpm_info->besldck&0x10) ? LPM_BESLD_STALL : 0, LPM_BESLD_STALL);


	//LPM INACTIVITY checker
	#if LPM_STRESS
	os_writel(U3D_MAC_U2_EN_CTRL, ACCEPT_EP0_INACTIVE_CHK);
	#else
	os_writel(U3D_MAC_U2_EN_CTRL,
		(os_readl(U3D_MAC_U2_EN_CTRL) & ~(0x1f<<16))
		| (lpm_info->cond_en ? 1<<(lpm_info->cond+16) : 0));
	#endif
}

/**
 * reset_dev - device reset flow
 *
 */
void reset_dev(USB_SPEED speed, DEV_UINT8 det_speed, DEV_UINT8 sw_rst){
	/* Reset usb ip. */
	if (g_usb_irq)
	{
		os_disableIrq(g_nirq);
		g_usb_irq = 0;
	}


	//reset or just disconnect IP
	if (sw_rst)
	{
		//reset
		mu3d_hal_rst_dev();
	}
	else
	{
		mu3d_hal_ssusb_en();

		#ifdef SUPPORT_U3
		os_writel(U3D_USB3_CONFIG, 0);
		#endif
		mu3d_hal_u2dev_disconn();

		//make sure speed_chg_intr is cleared before enabling U2 or U3 port again
		os_writel(U3D_DEV_LINK_INTR_ENABLE, 0);
		os_writel(U3D_DEV_LINK_INTR, SSUSB_DEV_SPEED_CHG_INTR);
	}
	os_ms_delay(50);


	//disable IP/U2 MAC/U3 MAC power down
	mu3d_hal_ssusb_en();

	//apply default register values
	mu3d_hal_dft_reg();


	//set device speed
	mu3d_hal_set_speed(speed);
	//detect connect speed
	mu3d_hal_det_speed(speed, det_speed);


	//initialize device
	/*
	 * Don't enable interrupt at this moment.
	 * Before enable interrput, We should finish all the setting
	 */
	/*u3d_irq_en();*/
	u3d_initialize_drv();
	#if (BUS_MODE==QMU_MODE)
	//initialize QMU
	mu3d_hal_init_qmu();
	#endif


	#ifdef POWER_SAVING_MODE
	//power down unused port
	mu3d_hal_pdn_cg_en();
	#endif

	// USBIF
	u3d_irq_en();	
}

/**
 * u3d_stall_status - return stall status
 *
 */

DEV_UINT8 u3d_stall_status(void){
    DEV_UINT32 i, tx_ep_num, rx_ep_num;
	DEV_UINT8 ret;

#ifdef HARDCODE_EP
    tx_ep_num = MAX_QMU_EP;
    rx_ep_num = MAX_QMU_EP;
#else
    tx_ep_num = os_readl(U3D_CAP_EPINFO) & CAP_TX_EP_NUM;
    rx_ep_num = (os_readl(U3D_CAP_EPINFO) & CAP_RX_EP_NUM) >> 8;
#endif

	ret = 0;
	for(i=1; i<=tx_ep_num; i++){
		if(USB_ReadCsr32(U3D_TX1CSR0, i) & TX_SENDSTALL){
			ret = 1;
		}
		if(USB_ReadCsr32(U3D_TX1CSR0, i) & TX_SENTSTALL){
			ret = 1;
		}
	}
	for(i=1; i<=rx_ep_num; i++){
		if(USB_ReadCsr32(U3D_RX1CSR0, i) & RX_SENDSTALL){
			ret = 1;
		}
		if(USB_ReadCsr32(U3D_TX1CSR0, i) & RX_SENTSTALL){
			ret = 1;
		}
	}
	return ret;
}


/**
 *
 * u3d_clear_stall_all - clear all stall
 */

void u3d_clear_stall_all(void){
    DEV_UINT32 i, tx_ep_num, rx_ep_num, tx_q_num, rx_q_num;

#ifdef HARDCODE_EP
    tx_q_num = tx_ep_num = MAX_QMU_EP;
    rx_q_num = rx_ep_num = MAX_QMU_EP;
#else
    tx_q_num = tx_ep_num = os_readl(U3D_CAP_EPINFO) & CAP_TX_EP_NUM;
    rx_q_num = rx_ep_num = (os_readl(U3D_CAP_EPINFO) & CAP_RX_EP_NUM) >> 8;
#endif

	for(i=1; i<=tx_ep_num; i++){
		USB_WriteCsr32(U3D_TX1CSR0, i, USB_ReadCsr32(U3D_TX1CSR0, i) & ~TX_SENDSTALL);
		USB_WriteCsr32(U3D_TX1CSR0, i, USB_ReadCsr32(U3D_TX1CSR0, i) | TX_SENTSTALL);
	}
	for(i=1; i<=rx_ep_num; i++){
		USB_WriteCsr32(U3D_RX1CSR0, i, USB_ReadCsr32(U3D_RX1CSR0, i) & ~RX_SENDSTALL);
		USB_WriteCsr32(U3D_RX1CSR0, i, USB_ReadCsr32(U3D_RX1CSR0, i) | RX_SENTSTALL);
	}

	for(i=1; i<=tx_q_num; i++){

		mu3d_hal_flush_qmu(i,USB_TX);
		mu3d_hal_restart_qmu(i,USB_TX);
	}
	for(i=1; i<=rx_q_num; i++){

		mu3d_hal_flush_qmu(i,USB_RX);
		mu3d_hal_restart_qmu(i,USB_RX);
	}
}


/**
 * u3d_stall_all - stall all epn
 *
 */

void u3d_stall_all(void){
    DEV_UINT32 i, tx_ep_num, rx_ep_num;

#ifdef HARDCODE_EP
    tx_ep_num = MAX_QMU_EP;
    rx_ep_num = MAX_QMU_EP;
#else
    tx_ep_num = os_readl(U3D_CAP_EPINFO) & CAP_TX_EP_NUM;
    rx_ep_num = (os_readl(U3D_CAP_EPINFO) & CAP_RX_EP_NUM) >> 8;
#endif

	for(i=1; i<=tx_ep_num; i++){
		USB_WriteCsr32(U3D_TX1CSR0, i, USB_ReadCsr32(U3D_TX1CSR0, i) | TX_SENDSTALL);
	}

	for(i=1; i<=rx_ep_num; i++){
		USB_WriteCsr32(U3D_RX1CSR0, i, USB_ReadCsr32(U3D_RX1CSR0, i) | RX_SENDSTALL);
	}
}


/**
 * u3d_send_ep0_stall - send an ep0 stall
 *
 */

void u3d_send_ep0_stall(void){
	//toggle EP0_RST
	// USBIF 
	//os_setmsk(U3D_EP_RST, EP0_RST);
	//os_clrmsk(U3D_EP_RST, EP0_RST);

	os_writel(U3D_EP0CSR, os_readl(U3D_EP0CSR) | EP0_SENDSTALL);
	while(!(os_readl(U3D_EP0CSR) & EP0_SENTSTALL));
	os_writel(U3D_EP0CSR, os_readl(U3D_EP0CSR) | EP0_SENTSTALL);
}


void u3d_dev_loopback(DEV_INT32 ep_rx,DEV_INT32 ep_tx){
	struct USB_REQ *treq, *rreq;

	rreq = mu3d_hal_get_req(ep_rx, USB_RX);
	treq = mu3d_hal_get_req(ep_tx, USB_TX);
	treq->buf = g_loopback_buffer[0];
	rreq->buf = g_loopback_buffer[0];
	/* epn rx enable. */
	u3d_ep_start_transfer(ep_rx, USB_RX);
	os_printk(K_DEBUG,"RX start..\n");
	while(!req_complete(ep_rx, USB_RX));
	os_printk(K_DEBUG,"RX complete!!\n");

	do{
		if(TransferLength > gpd_buf_size){
			treq->count = gpd_buf_size;
			TransferLength -= gpd_buf_size;
		}else{
			treq->count = TransferLength;
			TransferLength = 0;
		}
		/* epn start to transmit data. */
		u3d_ep_start_transfer(ep_tx, USB_TX);
		os_printk(K_DEBUG,"TX start..\n");
		while(!req_complete(ep_tx, USB_TX));
		treq->buf +=gpd_buf_size;
	}while(TransferLength>0);

	os_printk(K_DEBUG,"TX complete!!\n");
	treq->count = rreq->actual =0;
	u3d_rxep_dis(ep_rx);
}

DEV_UINT8 u3d_device_halt(void){
	return g_device_halt;
}

void u3d_ep0_tx(void)
{
	DEV_INT32 count, length, maxp;
	struct USB_EP_SETTING *ep_setting;
	struct USB_REQ *req;
	DEV_UINT8 *bp;

	req = &g_u3d_req[0];
	ep_setting = &g_u3d_setting.ep_setting[0];

	if (g_ep0_mode == PIO_MODE)
	{
		bp = req->buf + req->actual;
		maxp = ep_setting->maxp;

		if(req->count - req->actual > maxp)
			length = ep_setting->maxp;
		else
			length = req->count - req->actual;

		req->actual += length;
		count = mu3d_hal_write_fifo(0, length, bp, maxp);

		if(!count)
		{
			g_ep0_state = EP0_IDLE;
			req->complete = 1;
			req->count = 0;
			req->actual = 0;
		}

		os_printk(K_DEBUG,"count :%d\n",count);
		os_printk(K_DEBUG,"ep_setting->maxp :%d\n",ep_setting->maxp);
		os_printk(K_DEBUG,"needZLP :%d\n",g_u3d_req[0].needZLP);
		os_printk(K_DEBUG,"U3D_EP0CSR :%x\n",  os_readl(U3D_EP0CSR));
		os_printk(K_DEBUG,"g_u3d_req[0].actual :%x\n",g_u3d_req[0].actual);
		os_printk(K_DEBUG,"g_u3d_req[0].count :%x\n",g_u3d_req[0].count);
		os_printk(K_DEBUG,"req->count :%d\n",req->count);
		os_printk(K_DEBUG,"req->actual :%d\n",req->count);
	}
	else
	{
		req = &g_u3d_req[0];

		if(req->complete == 1)
		{
			os_printk(K_ERR,"1 completed!\r\n");
			u3d_free_dma0();
		}
		else
		{
			os_printk(K_DEBUG,"req->actual : %d\n",req->actual);
			os_printk(K_DEBUG,"req->count : %d\n",req->count);
			req->currentCount = ((req->count - req->actual) > ep_setting->maxp) ? ep_setting->maxp : req->count - req->actual;

			if(req->actual >= req->count)
			{
				os_writel(U3D_EP0CSR, os_readl(U3D_EP0CSR) | EP0_DATAEND);
				os_printk(K_DEBUG,"USB_EP0_DATAEND\r\n");
				u3d_free_dma0();
				g_ep0_state = EP0_IDLE;
				req->complete = 1;
				os_printk(K_ERR, "2 completed\n");
				dma_sync_single_for_cpu(&mu3d_if_dev, (dma_addr_t)req->dma_adr, USB_BUF_SIZE, DMA_BIDIRECTIONAL);
				dma_unmap_single(&mu3d_if_dev, req->dma_adr, USB_BUF_SIZE, DMA_BIDIRECTIONAL);
				os_printk(K_DEBUG,"Dma han (02): EP[0] complete, send %d bytes\r\n", req->actual);
			}
			else
			{
				os_printk(K_DEBUG,"usb_config_dma 00\n");
				/* config ep0 tx dma channel */
				u3d_config_dma0(0, USB_TX, (DEV_UINT32)((u32)req->dma_adr + req->actual), req->currentCount);
			}
		}
	}

	return;
}

void u3d_ep0_rx(void)
{
	DEV_UINT8 *ptr1;
	DEV_UINT8 *ptr2=0, *bp;
	DEV_INT32 count,i;
	struct USB_EP_SETTING *ep_setting;
	struct USB_REQ *req;

	os_printk(K_DEBUG,"ep0 rx start !!\n");

	ep_setting = &g_u3d_setting.ep_setting[0];

	if (g_ep0_mode == PIO_MODE)
	{
		req = &g_u3d_req[0];
		bp = req->buf + req->actual;
		count = mu3d_hal_read_fifo(0,bp);
		req->actual += count;

		if(Request->bRequest==AT_CMD_SET)
		{
			os_printk(K_DEBUG,"AT_CMD_SET\n");
			ptr1=(DEV_UINT8 *)AT_CMD;
			ptr2=req->buf;

			for(i=0;i<AT_CMD_SET_BUFFER_OFFSET;i++)
			{
				*(ptr1+i)=*(ptr2+i);
			}

			ptr1= AT_CMD->buffer;
			ptr2=req->buf;

			for(i=0;i<(count-AT_CMD_SET_BUFFER_OFFSET);i++)
			{
				*(ptr1+i)=*(ptr2+i+AT_CMD_SET_BUFFER_OFFSET);
			}

			g_u3d_status = BUSY;
		}

		if((count < ep_setting->maxp)||(req->actual==req->count))
		{
			os_writel(U3D_EP0CSR, os_readl(U3D_EP0CSR) | EP0_DATAEND);
			g_ep0_state = EP0_IDLE;
			req->complete = 1;
			Request->bCommand=AT_CMD->tsfun;

			if(Request->bRequest!=AT_CTRL_TEST) {
				os_printk(K_DEBUG,"bValid=1\n");
				Request->bValid=1;
			}
		}
	}
	else
	{
		req = &g_u3d_req[0];
		req->currentCount = os_readl(U3D_RXCOUNT0);
		os_printk(K_DEBUG,"RxCount : %d\n",os_readl(U3D_RXCOUNT0));

		if(req->currentCount == 0)
		{
			req->complete = 1;
			u3d_free_dma0();
		}
		else
		{
			/* config ep0 rx dma channel */
			u3d_config_dma0(0, USB_RX, (DEV_UINT32)((u32)req->dma_adr + req->actual), req->currentCount);
		}
	}

return;
}

DEV_UINT8* u3d_fill_in_buffer(DEV_UINT8 *ptr, DEV_UINT8 size, DEV_UINT8 *array)
{
	DEV_UINT8 i;
	
	for (i = 0; i < size; i++)
		*(ptr+i) = *(array+i);

	return ptr+size;
}

void u3d_ep0_idle(void){
    DEV_INT32 i;
    DEV_UINT32 word[8];
	struct USB_REQ *req;
	dma_addr_t mapping;
	DEV_UINT8 *ptr1;

	req = mu3d_hal_get_req(0, USB_TX);

    if(os_readl(U3D_RXCOUNT0) == 0){
    	os_printk(K_ERR, "RXCOUNT == 0\n");
        return;
    }

	if (g_ep0_mode == PIO_MODE)
	{
		/* decode command */
		for(i = 0; i < 2; i++)
		{
			word[i] = os_readl(USB_FIFO(0));
		}
	}
	else
	{
		req->currentCount=os_readl(U3D_RXCOUNT0);
		req->buf = g_dma_buffer[0];

		mapping = dma_map_single(&mu3d_if_dev, req->buf, USB_BUF_SIZE, DMA_BIDIRECTIONAL);
		g_mu3d_dma_debug = mapping;
		dma_sync_single_for_device(&mu3d_if_dev, mapping, USB_BUF_SIZE, DMA_BIDIRECTIONAL);
		req->dma_adr = mapping;
		u3d_config_dma0(0, USB_RX, (u32)req->dma_adr, req->currentCount);

		os_printk(K_DEBUG, "dma move\n");
		
		return;
	}

	Request->bmRequestType=(word[0]&0x000000FF);
	Request->bRequest=(word[0]&0x0000FF00)>>8;
	Request->wValue=((word[0]&0x00FF0000)>>16)|((word[0]&0xFF000000)>>16);
	Request->wIndex=(word[1]&0x000000FF)|(word[1]&0x0000FF00);
	Request->wLength=((word[1]&0x00FF0000)>>16)|((word[1]&0xFF000000)>>16);

	os_printk(K_INFO,"Request->bmRequestType :  %x  \n",Request->bmRequestType);
	os_printk(K_INFO,"Request->bRequest :  %x  \n",Request->bRequest);
	os_printk(K_INFO,"Request->wValue :  %x  \n",Request->wValue);
	os_printk(K_INFO,"Request->wIndex :  %x  \n",Request->wIndex);
	os_printk(K_INFO,"Request->wLength :  %x  \n",Request->wLength);


	if (os_readl(U3D_EPISR) & SETUPENDISR) //SETUPEND
	{
		os_printk(K_ERR, "Abort this command because of SETUP\n");
		return;
	}


	if(((Request->bmRequestType&USB_TYPE_MASK)==USB_TYPE_STANDARD)&&
				(Request->bRequest==USB_REQ_SET_ADDRESS)){

 		u3d_set_address(Request->wValue);
		os_writel(U3D_EP0CSR,os_readl(U3D_EP0CSR) | EP0_SETUPPKTRDY | EP0_DATAEND);

		return;
	}

	if(((Request->bmRequestType&USB_TYPE_MASK)==USB_TYPE_STANDARD)&&
				((Request->bmRequestType&USB_RECIP_MASK)==USB_RECIP_ENDPOINT)){

		if(Request->bmRequestType & USB_DIR_IN){

			g_ep0_state = EP0_TX;
			os_writel(U3D_EP0CSR,os_readl(U3D_EP0CSR) | EP0_SETUPPKTRDY | EP0_DPHTX);

			if(Request->bRequest==USB_REQ_GET_STATUS){
				ptr1 = req->buf = g_dma_buffer[0];
				*ptr1 = u3d_stall_status();
				*(ptr1+1) = 0;
				req->count = USB_STATUS_SIZE;
				req->complete = 0;
				req->actual = 0;
				req->needZLP = 0;
			}
			if(Request->bRequest==USB_REQ_EP0_IN_STALL){
				g_ep0_state = EP0_IDLE;
				u3d_send_ep0_stall();
				return;
			}
		}
		else{
			if((Request->bRequest==USB_REQ_SET_FEATURE) &&
			(Request->wValue==ENDPOINT_HALT)){
				u3d_stall_all();
				g_device_halt = 1;
				os_writel(U3D_EP0CSR,os_readl(U3D_EP0CSR) | EP0_SETUPPKTRDY | EP0_DATAEND);
				return;
			}
			if((Request->bRequest==USB_REQ_CLEAR_FEATURE) &&
			(Request->wValue==ENDPOINT_HALT)){
				u3d_clear_stall_all();
				g_device_halt = 0;
				os_writel(U3D_EP0CSR,os_readl(U3D_EP0CSR) | EP0_SETUPPKTRDY | EP0_DATAEND);
				return;
			}
			if((Request->bRequest==USB_REQ_EP0_STALL) &&
			(Request->wValue==ENDPOINT_HALT)){
				u3d_send_ep0_stall();
				return;
			}
			if(Request->bRequest==USB_REQ_EP0_OUT_STALL){
				os_writel(U3D_EP0CSR,os_readl(U3D_EP0CSR) | EP0_SETUPPKTRDY);
				u3d_send_ep0_stall();
				return;
			}
		}

	}

	if(Request->bmRequestType==0x00C0){
 		g_ep0_state = EP0_TX;
		os_writel(U3D_EP0CSR,os_readl(U3D_EP0CSR) | EP0_SETUPPKTRDY | EP0_DPHTX);

		if(Request->bRequest==AT_CMD_ACK){
			os_printk(K_DEBUG, "AT_CMD_ACK valid=%x, %d\n", Request->bValid, g_u3d_status);
			ptr1 = req->buf = g_dma_buffer[0];
			*ptr1 = 0x55;
			*(ptr1+1) = 0xAA;
			*(ptr1+2) = AT_CMD_ACK_DATA_LENGTH&0xFF;
			*(ptr1+3) = AT_CMD_ACK_DATA_LENGTH>>8;
			*(ptr1+4) = (!Request->bValid) ? READY : BUSY;
			*(ptr1+5) = 0;
			*(ptr1+6) = g_u3d_status;
			*(ptr1+7) = 0;

			req->count = AT_CMD_ACK_DATA_LENGTH;
			req->complete = 0;
			req->actual = 0;
			req->needZLP = 0;
		}
		else if(Request->bRequest==AT_CTRL_TEST){
			os_printk(K_DEBUG,"AT_CTRL_TEST\n");

#ifdef BOUNDARY_4K
			req->buf =loopback_buffer;
#else
			req->buf =g_loopback_buffer[1];
#endif

			req->count = req->actual;
			req->complete = 0;
			req->actual = 0;
			req->needZLP = 0;

			os_printk(K_DEBUG,"req->buf=%p\n",req->buf);
		}
		else if(Request->bRequest==AT_PW_STS_CHK)
		{
			os_printk(K_DEBUG, "AT_CMD_GET\n");

			ptr1 = req->buf = g_dma_buffer[0];
			*ptr1 = 0x55;
			*(ptr1+1) = 0xAA;
			*(ptr1+2) = AT_PW_STS_CHK_DATA_LENGTH&0xFF;
			*(ptr1+3) = AT_PW_STS_CHK_DATA_LENGTH>>8;
			*(ptr1+4) = 0;
			*(ptr1+5) = 0;
			*(ptr1+6) = dev_stschk(Request->wIndex, Request->wValue);
			*(ptr1+7) = 0;

			req->count = AT_PW_STS_CHK_DATA_LENGTH;
			req->complete = 0;
			req->actual = 0;
			req->needZLP = 0;
		}

		os_printk(K_DEBUG,"g_u3d_req[0].count:  %x  \n",g_u3d_req[0].count);
	}
	else if(Request->bmRequestType==0x0040){
		g_ep0_state = EP0_RX;
		req->buf =g_loopback_buffer[1];
#ifdef BOUNDARY_4K
		loopback_buffer = g_loopback_buffer[1]+(0x1000-(DEV_INT32)(unsigned long)g_loopback_buffer[1]%0x1000)-0x08+bAddress_Offset;
		bAddress_Offset++;
	 	bAddress_Offset%=4;
		req->buf =loopback_buffer;
#else
		req->buf =g_loopback_buffer[1];
#endif
		req->count = Request->wLength;
		req->complete = 0;
		req->actual = 0;
		req->needZLP = 0;
		os_writel(U3D_EP0CSR,os_readl(U3D_EP0CSR) | EP0_SETUPPKTRDY);
		os_printk(K_DEBUG,"EP0 RX\n");
 	}

    if(g_ep0_state == EP0_IDLE){ //no data phase

		g_u3d_req[0].complete = 1;
    }
	else if(g_ep0_state == EP0_TX){//data phase in

	os_printk(K_DEBUG,"u3d_ep0_tx 0 \n");
		u3d_ep0_tx();
	}
    return;

}

void u3d_ep0_handler(void){
	//starts for EP0_IDLE for normal and abnormal case (SETUPEND)
	if (os_readl(U3D_EP0CSR) & EP0_SETUPPKTRDY){
		os_printk(K_DEBUG, "EP0_IDLE\n");
		u3d_ep0_idle();
	}
	else if(g_ep0_state == EP0_RX){
		os_printk(K_DEBUG, "EP0_IDLE RX\n");
		u3d_ep0_rx();
	}
	else if(g_ep0_state == EP0_TX){
		os_printk(K_DEBUG, "EP0_IDLE TX\n");
		u3d_ep0_tx();
	}

	return;
}


void u3d_epx_handler(DEV_INT32 ep_num, USB_DIR dir){

    DEV_INT32 ep_index, maxp, length;
#if (BUS_MODE!=QMU_MODE)
    DEV_INT32 count;
#endif
	DEV_UINT8 *bp;
    struct USB_EP_SETTING *ep_setting;
    struct USB_REQ *req;

 	os_printk(K_DEBUG,"ep_num :%x\n",ep_num);
	os_printk(K_DEBUG,"dir :%x\n",dir);

	if(dir == USB_TX){
		ep_index = ep_num;
       	ep_setting = &g_u3d_setting.ep_setting[ep_index];
       	req = &g_u3d_req[ep_index];
		maxp = ep_setting->maxp;
	   	os_printk(K_DEBUG,"g_u3d_req[%d].buf@0x%p\n", ep_index, g_u3d_req[ep_index].buf);
       #if (BUS_MODE==PIO_MODE)
	   	bp = req->buf + req->actual;
	   	length = req->count - req->actual;

		if(req->actual == req->count){
			req->count = 0;
			req->actual = 0;
			req->complete = 1;
		}
		else{
			count = mu3d_hal_write_fifo_burst(ep_num,length,bp, maxp);
			req->actual += count;

			if((req->actual == req->count)&&(!(req->count % maxp))){
				mu3d_hal_write_fifo_burst(ep_num,0,bp, maxp);
			}
		}

	   #endif
    }
    else if(dir == USB_RX){

        ep_index = ep_num + MAX_EP_NUM;
        ep_setting = &g_u3d_setting.ep_setting[ep_index];
        req = &g_u3d_req[ep_index];
        #if (BUS_MODE==PIO_MODE)
		bp = req->buf + req->actual;
        count = mu3d_hal_read_fifo_burst(ep_num,bp);
		req->actual += count;
		#endif
		if(req->actual==TransferLength){
			req->complete = 1;
		}
	} else {
        BUG_ON(1);
    }

    return;
}


/**
 * u3d_dma_handler - receive setup in idle state, data phase in(TX) in tx state, data phase out(RX) in rx state
 *
 */
void u3d_dma_handler(DEV_INT32 DMAIntSts){

    struct USB_REQ *req;
    DEV_INT32 ep_index, ep_num, i, count;
    DEV_INT16 dir;
	DEV_UINT8 *ptr1;
	DEV_UINT8 *ptr2;
    struct USB_EP_SETTING *ep_setting;
	dma_addr_t mapping;

	if (DMAIntSts & EP0DMAISR) {//ep0

		ep_index = ep_num = 0;
        ep_setting = &g_u3d_setting.ep_setting[ep_index];
        req = &g_u3d_req[ep_index];
		count = req->currentCount;
		dir = (os_readl(U3D_EP0DMACTRL)&DMA_DIR)>>1;
		os_printk(K_DEBUG,"req->actual :%d\n",req->actual);
		os_printk(K_DEBUG,"req->currentCount :%d\n",req->currentCount);
		os_printk(K_DEBUG,"ep_setting->maxp :%d\n",ep_setting->maxp);

        u3d_free_dma0();

		if((os_readl(U3D_EP0CSR) & EP0_SETUPPKTRDY) //SETUPEND case; restarts from EP0_IDLE
			|| (g_ep0_state == EP0_IDLE)){
			os_printk(K_ERR, "DMA EP0_IDLE\n");
			/* decode and handle ep0 setup packet*/
			mapping=req->dma_adr;
			dma_sync_single_for_cpu(&mu3d_if_dev, mapping, USB_BUF_SIZE, DMA_BIDIRECTIONAL);
			dma_unmap_single(&mu3d_if_dev, mapping, USB_BUF_SIZE,DMA_BIDIRECTIONAL);
			ptr1=req->buf;
			Request->bmRequestType=*ptr1;
			Request->bRequest=*(ptr1+1);
			Request->wValue=*(ptr1+2)|(*(ptr1+3)<<8);
			Request->wIndex=*(ptr1+4)|(*(ptr1+5)<<8);
			Request->wLength=*(ptr1+6)|(*(ptr1+7)<<8);

			os_printk(K_DEBUG, "DMA move done\n");
			os_printk(K_DEBUG ,"Request->bmRequestType :  %x  \n",Request->bmRequestType);
			os_printk(K_DEBUG ,"Request->bRequest :	%x	\n",Request->bRequest);
			os_printk(K_DEBUG ,"Request->wValue :  %x  \n",Request->wValue);
			os_printk(K_DEBUG ,"Request->wIndex :  %x  \n",Request->wIndex);
			os_printk(K_DEBUG ,"Request->wLength :  %x  \n",Request->wLength);


			if (os_readl(U3D_EPISR) & SETUPENDISR) //SETUPEND
			{
				os_printk(K_ERR, "Abort this command because of SETUP\n");
				return;
			}


			if ((Request->bmRequestType & USB_TYPE_MASK) == USB_TYPE_STANDARD)
			{
				//DEVICE
				if ((Request->bmRequestType & USB_RECIP_MASK) == USB_RECIP_DEVICE)
				{
					//0x0
					if (Request->bRequest == USB_REQ_GET_STATUS)
					{
						printk("==DEV - USB_REQ_GET_STATUS==\n");
						ptr1 = req->buf = g_dma_buffer[0];

						//OTG status
						// USBIF OTG , HNP polling 
						if (Request->wIndex == 0xf000)
						{
							os_printk(K_ERR, "g_otg_hnp_reqd = %d %s)\n", g_otg_hnp_reqd, __func__);						
							mb();
							if (g_otg_hnp_reqd)
								ptr1 = u3d_fill_in_buffer(ptr1, sizeof(string_one), string_one);
							else
								ptr1 = u3d_fill_in_buffer(ptr1, sizeof(string_zero), string_zero);
						}
						
						req->count = ptr1 - req->buf;
	//					os_printk(K_ERR, "length: %d\n", req->count);
						req->complete = 0;
						req->actual = 0;
						req->needZLP = 0;

						mapping = dma_map_single(&mu3d_if_dev, req->buf,USB_BUF_SIZE, DMA_BIDIRECTIONAL);
						dma_sync_single_for_device(&mu3d_if_dev, mapping, USB_BUF_SIZE, DMA_BIDIRECTIONAL);
						req->dma_adr=mapping;

						g_ep0_state = EP0_TX;
						os_writel(U3D_EP0CSR,os_readl(U3D_EP0CSR) | EP0_SETUPPKTRDY | EP0_DPHTX);											
					}

					#ifdef SUPPORT_OTG
					//0x1
					if (Request->bRequest == USB_REQ_CLEAR_FEATURE)
					{
						printk("==DEV - USB_REQ_CLEAR_FEATURE==\n");
						//B_HNP_ENABLE
						if (Request->wValue == 0x0003)
						{
							g_ep0_state = EP0_IDLE;
							u3d_send_ep0_stall();
						}
					}
					
					//0x3
					if (Request->bRequest == USB_REQ_SET_FEATURE)
					{
						printk("==DEV - USB_REQ_SET_FEATURE==\n"); 
						printk("Request wValue = 0x%x, Request wIndex = 0x%x\n", Request->wValue, Request->wIndex);
						//TEST_MODE
						if (Request->wValue == 0x0002)
						{
							//otg_srp_reqd
							if (Request->wIndex == 0x600)
							{
								printk("== set g_otg_srp_reqd = 1 ==\n");
								g_otg_srp_reqd = 1;
								mb() ;
								os_printk(K_ERR, "g_otg_srp_reqd = 1\n");								
							}
							else if (Request->wIndex == 0x700)
							{
								printk("== set g_otg_hnp_reqd = 1 ==\n");
								g_otg_hnp_reqd = 1;

								mb() ;
								os_printk(K_ERR, "g_otg_hnp_reqd = 1 (%s)\n", __func__);
							}
						}
						//B_HNP_ENABLE
						else if (Request->wValue == 0x0003)
						{
							g_otg_b_hnp_enable = 1;						
							mb() ;
							os_printk(K_ERR, "set g_otg_b_hnp_enable = 1 (%s)\n", __func__);						
						}
						
						os_writel(U3D_EP0CSR,os_readl(U3D_EP0CSR) | EP0_SETUPPKTRDY | EP0_DATAEND);
									
					}
					#endif

					//0x5
					if (Request->bRequest == USB_REQ_SET_ADDRESS)
					{
						u3d_sync_with_bat(USB_UNCONFIGURED) ;
						printk("==DEV - USB_REQ_SET_ADDRESS==\n");
						#ifdef SUPPORT_OTG
						//g_otg_config = 0;
						//mb();
						#endif

				/* set device address*/
				u3d_set_address(Request->wValue);
						
				os_printk(K_DEBUG,"Device Address :%x\n",(os_readl(U3D_DEVICE_CONF)>>DEV_ADDR_OFST));
				os_writel(U3D_EP0CSR,os_readl(U3D_EP0CSR) | EP0_SETUPPKTRDY | EP0_DATAEND);
			}

					//0x6
					if (Request->bRequest == USB_REQ_GET_DESCRIPTOR)
					{
						printk("==DEV - USB_REQ_GET_DESCRIPTOR==\n");
						ptr1 = req->buf = g_dma_buffer[0];

						//device
						if (Request->wValue == 0x100)
						{
							ptr1 = u3d_fill_in_buffer(ptr1, sizeof(device_descriptor), device_descriptor);
							os_printk(K_ERR, "device_descriptor\n");
						}
						//configuration
						else if (Request->wValue == 0x200)
						{
							if (Request->wLength == 9)
							{
								ptr1 = u3d_fill_in_buffer(ptr1, sizeof(configuration_descriptor), configuration_descriptor);
								os_printk(K_ERR, "configuration_descriptor\n");
							}
							else
							{
								//9 bytes
								ptr1 = u3d_fill_in_buffer(ptr1, sizeof(configuration_descriptor), configuration_descriptor);
								//5 bytes
								ptr1 = u3d_fill_in_buffer(ptr1, sizeof(otg_descriptor), otg_descriptor);
								//9 bytes
								ptr1 = u3d_fill_in_buffer(ptr1, sizeof(interface_descriptor), interface_descriptor);
								//7 bytes
								ptr1 = u3d_fill_in_buffer(ptr1, sizeof(endpoint_descriptor_in), endpoint_descriptor_in);
								//7 bytes
								ptr1 = u3d_fill_in_buffer(ptr1, sizeof(endpoint_descriptor_out), endpoint_descriptor_out);
								os_printk(K_ERR, "5 descriptors\n");
							}
						}
						//string
						else if ((Request->wValue & 0xff00) == 0x300)
						{
							switch (Request->wValue & 0xff)
							{
								case 0:
									ptr1 = u3d_fill_in_buffer(ptr1, sizeof(string_descriptor_0), string_descriptor_0);
									break;
								case 1:
									ptr1 = u3d_fill_in_buffer(ptr1, sizeof(string_descriptor_1), string_descriptor_1);								
									break;
								case 2:
									ptr1 = u3d_fill_in_buffer(ptr1, sizeof(string_descriptor_2), string_descriptor_2);								
									break;
								case 3:
									ptr1 = u3d_fill_in_buffer(ptr1, sizeof(string_descriptor_3), string_descriptor_3);								
									break;
							}
						}
						//OTG
						else if (Request->wValue == 0x900)
						{
							ptr1 = u3d_fill_in_buffer(ptr1, sizeof(otg_descriptor), otg_descriptor);													
						}

						req->count = ptr1 - req->buf;
	//					os_printk(K_ERR, "length: %d\n", req->count);
						req->complete = 0;
						req->actual = 0;
						req->needZLP = 0;

						mapping = dma_map_single(&mu3d_if_dev, req->buf,USB_BUF_SIZE, DMA_BIDIRECTIONAL);
						dma_sync_single_for_device(&mu3d_if_dev, mapping, USB_BUF_SIZE, DMA_BIDIRECTIONAL);
						req->dma_adr=mapping;

						g_ep0_state = EP0_TX;
						os_writel(U3D_EP0CSR,os_readl(U3D_EP0CSR) | EP0_SETUPPKTRDY | EP0_DPHTX);
					}

					//0x9
					if (Request->bRequest == USB_REQ_SET_CONFIGURATION)
					{
						printk("==DEV - USB_REQ_SET_CONFIGURATION==\n");
						#ifdef SUPPORT_OTG
						g_otg_config = 1;
						mb() ;
						#endif
						printk("==DEV - USB_REQ_SET_CONFIGURATION== done, g_otg_config %d\n", g_otg_config);
						
						os_writel(U3D_EP0CSR,os_readl(U3D_EP0CSR) | EP0_SETUPPKTRDY | EP0_DATAEND);								

						// USBIF						
						u3d_sync_with_bat(USB_CONFIGURED) ;
					}
				}


				//ENDPOINT				
				if ((Request->bmRequestType & USB_RECIP_MASK) == USB_RECIP_ENDPOINT)
				{
					if (Request->bmRequestType & USB_DIR_IN)
					{
						g_ep0_state = EP0_TX;
						os_writel(U3D_EP0CSR,os_readl(U3D_EP0CSR) | EP0_SETUPPKTRDY | EP0_DPHTX);

						if (Request->bRequest == USB_REQ_GET_STATUS)
						{
							printk("==EP - USB_REQ_GET_STATUS==\n");
							ptr1 = req->buf = g_dma_buffer[0];
							*ptr1 = u3d_stall_status();
							*(ptr1+1) = 0x00;
							req->count = USB_STATUS_SIZE;
							req->complete = 0;
							req->actual = 0;
							req->needZLP = 0;
							mapping = dma_map_single(&mu3d_if_dev, req->buf,USB_BUF_SIZE, DMA_BIDIRECTIONAL);
							dma_sync_single_for_device(&mu3d_if_dev, mapping, USB_BUF_SIZE, DMA_BIDIRECTIONAL);
							req->dma_adr=mapping;
						}
						
						if (Request->bRequest == USB_REQ_EP0_IN_STALL)
						{
							printk("==EP - USB_REQ_EP0_IN_STALL==\n");
							g_ep0_state = EP0_IDLE;
							u3d_send_ep0_stall();
						}
					}
					else
					{
						if((Request->bRequest==USB_REQ_SET_FEATURE) &&
							(Request->wValue == ENDPOINT_HALT))
						{
							printk("==EP - USB_REQ_SET_FEATURE to EP HALT==\n");
							u3d_stall_all();
							g_device_halt = 1;
							os_writel(U3D_EP0CSR,os_readl(U3D_EP0CSR) | EP0_SETUPPKTRDY | EP0_DATAEND);
						}
						
						if((Request->bRequest==USB_REQ_CLEAR_FEATURE) &&
							(Request->wValue == ENDPOINT_HALT))
						{
							printk("==EP - USB_REQ_CLEAR_FEATURE to EP HALT==\n");
							u3d_clear_stall_all();
							g_device_halt = 0;
							os_writel(U3D_EP0CSR,os_readl(U3D_EP0CSR) | EP0_SETUPPKTRDY | EP0_DATAEND);
						}
						
						if((Request->bRequest==USB_REQ_EP0_STALL) &&
							(Request->wValue == ENDPOINT_HALT))
						{
							printk("==EP - USB_REQ_EP0_STALL to EP HALT==\n");
							u3d_send_ep0_stall();
						}
						
						if (Request->bRequest == USB_REQ_EP0_OUT_STALL)
						{
							printk("==EP - USB_REQ_EP0_STALL==\n");
							os_writel(U3D_EP0CSR,os_readl(U3D_EP0CSR) | EP0_SETUPPKTRDY);
							u3d_send_ep0_stall();
						}
					}
				}
			}


			if (Request->bmRequestType == USB_TYPE_VENDOR)
			{
				if (Request->bmRequestType & USB_DIR_IN)
				{
				os_printk(K_DEBUG,"EP0_TX \n");
				g_ep0_state = EP0_TX;
				os_writel(U3D_EP0CSR,os_readl(U3D_EP0CSR)| EP0_SETUPPKTRDY | EP0_DPHTX);

					if (Request->bRequest == AT_CMD_ACK)
					{
						os_printk(K_DEBUG,"AT_CMD_ACK  valid:  %x  \n",Request->bValid);

					/* handle AT_CMD_ACK status*/
					ptr1 = req->buf = g_dma_buffer[0];
					*ptr1 = 0x55;
					*(ptr1+1) = 0xAA;
					*(ptr1+2) = AT_CMD_ACK_DATA_LENGTH&0xFF;
					*(ptr1+3) = AT_CMD_ACK_DATA_LENGTH>>8;
					*(ptr1+4) = (!Request->bValid) ? READY : BUSY;
					*(ptr1+5) = 0;
					*(ptr1+6) = g_u3d_status;
					*(ptr1+7) = 0;

					req->count = AT_CMD_ACK_DATA_LENGTH;
					req->complete = 0;
					req->actual = 0;
					req->needZLP = 0;
				}
					else if (Request->bRequest == AT_CTRL_TEST)
					{				
					os_printk(K_DEBUG,"AT_CTRL_TEST\n");

					/* handle AT_CTRL_TEST for unit test ctrl loopback*/
#ifdef BOUNDARY_4K
					req->buf =loopback_buffer;
#else
					req->buf =g_loopback_buffer[1];
#endif

					req->count = req->actual;
					req->complete = 0;
					req->actual = 0;
					req->needZLP = 0;
				}
				else if(Request->bRequest==AT_PW_STS_CHK)
				{
					os_printk(K_DEBUG, "AT_CMD_GET\n");

					ptr1 = req->buf = g_dma_buffer[0];
					*ptr1 = 0x55;
					*(ptr1+1) = 0xAA;
					*(ptr1+2) = AT_PW_STS_CHK_DATA_LENGTH&0xFF;
					*(ptr1+3) = AT_PW_STS_CHK_DATA_LENGTH>>8;
					*(ptr1+4) = 0;
					*(ptr1+5) = 0;
					*(ptr1+6) = dev_stschk(Request->wIndex, Request->wValue);
					*(ptr1+7) = 0;

					req->count = AT_PW_STS_CHK_DATA_LENGTH;
					req->complete = 0;
					req->actual = 0;
					req->needZLP = 0;
				}

				mapping = dma_map_single(&mu3d_if_dev, req->buf,USB_BUF_SIZE, DMA_BIDIRECTIONAL);
				dma_sync_single_for_device(&mu3d_if_dev, mapping, USB_BUF_SIZE, DMA_BIDIRECTIONAL);
				os_printk(K_INFO,"mapping : 0x%p\n",(void *)(unsigned long)mapping);
				req->dma_adr=mapping;

				os_printk(K_DEBUG,"g_u3d_req[0].count:  %x  \n",g_u3d_req[0].count);
			}
				else
				{						
				os_printk(K_DEBUG,"EP0_RX \n");
				g_ep0_state = EP0_RX;

#ifdef BOUNDARY_4K
				loopback_buffer = g_loopback_buffer[1]+(0x1000-(DEV_INT32)(unsigned long)g_loopback_buffer[1]%0x1000)-0x08+bAddress_Offset;
				bAddress_Offset++;
				bAddress_Offset%=4;
				req->buf =loopback_buffer;
#else
				req->buf =g_loopback_buffer[1];
#endif

				req->count = Request->wLength;
				req->complete = 0;
				req->actual = 0;
				req->needZLP = 0;
				os_writel(U3D_EP0CSR,os_readl(U3D_EP0CSR) | EP0_SETUPPKTRDY);

				mapping = dma_map_single(&mu3d_if_dev, req->buf,USB_BUF_SIZE, DMA_BIDIRECTIONAL);
				dma_sync_single_for_device(&mu3d_if_dev, mapping, USB_BUF_SIZE, DMA_BIDIRECTIONAL);
					os_printk(K_INFO,"req->buf: %p, mapping : 0x%p\n", req->buf, (void *)(unsigned long)mapping);
				req->dma_adr=mapping;
			}
			}

		
			if(g_ep0_state == EP0_IDLE) //no data phase
				g_u3d_req[0].complete = 1;
			else if(g_ep0_state == EP0_TX) //data in			
				u3d_ep0_tx();

			return;
		}
		else if(g_ep0_state == EP0_RX){
			/*  handle data phase out(rx)*/
			os_printk(K_INFO, "DMA EP0_RX\n");
			req->actual += count;
			os_printk(K_DEBUG,"receive : %d\n",req->actual);

			if(Request->bRequest==AT_CMD_SET){

				ptr1=(DEV_UINT8 *)AT_CMD;
				ptr2=g_u3d_req[0].buf;
				for(i=0;i<AT_CMD_SET_BUFFER_OFFSET;i++){
					*(ptr1+i)=*(ptr2+i);
				}
				ptr1=AT_CMD->buffer;
				ptr2=g_u3d_req[0].buf;
				for(i=0;i<(count-AT_CMD_SET_BUFFER_OFFSET);i++){
					*(ptr1+i)=*(ptr2+i+AT_CMD_SET_BUFFER_OFFSET);
				}
				g_u3d_status = BUSY;
			}
#ifndef AUTOCLEAR
			os_writel(U3D_EP0CSR, os_readl(U3D_EP0CSR)|EP0_RXPKTRDY);
#endif
			if((count < ep_setting->maxp)||(req->actual==req->count)){

				os_writel(U3D_EP0CSR, os_readl(U3D_EP0CSR) | EP0_DATAEND);

				g_ep0_state = EP0_IDLE;
				req->complete = 1;

				if(Request->bRequest!=AT_CTRL_TEST){
					Request->bValid=1;
				}
				os_printk(K_DEBUG,"bValid !!\n");
				Request->bCommand=AT_CMD->tsfun;
				dma_sync_single_for_cpu(&mu3d_if_dev, req->dma_adr, USB_BUF_SIZE, DMA_BIDIRECTIONAL);
				dma_unmap_single(&mu3d_if_dev, req->dma_adr, USB_BUF_SIZE, DMA_BIDIRECTIONAL);
			}
		}
		else if(g_ep0_state == EP0_TX){
			/*  handle data phase in(tx)*/
				os_printk(K_INFO, "DMA EP0_TX\n");
				req->actual += req->currentCount;
				os_printk(K_DEBUG,"req->actual :%d\n",req->actual);
				os_printk(K_DEBUG,"req->currentCount :%d\n",req->currentCount);
#ifdef AUTOSET
				if(req->currentCount<ep_setting->maxp){
					os_writel(U3D_EP0CSR, os_readl(U3D_EP0CSR)| EP0_TXPKTRDY);
				}
#else
				os_writel(U3D_EP0CSR, os_readl(U3D_EP0CSR)| EP0_TXPKTRDY);
#endif
		}
    }

    return;
}

// USBIF
#if 0
void mt_usb_disconnect(void)
{
	os_printk(K_DEBUG,"[MUSB] USB is ready for disconnect\n");

	os_printk(K_DEBUG,"[MUSB] USB disconnect\n");
}
EXPORT_SYMBOL_GPL(mt_usb_disconnect);

void mt_usb_connect(void)
{
	os_printk(K_DEBUG,"[MUSB] USB is ready for connect\n");

	os_printk(K_DEBUG,"[MUSB] USB connect\n");
}
EXPORT_SYMBOL_GPL(mt_usb_connect);
#endif



