#include <xhci.h>
#include <linux/xhci/xhci-mtk.h>
#include <linux/xhci/xhci-mtk-power.h>
#include <linux/xhci/xhci-mtk-scheduler.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#ifdef CONFIG_USB_MTK_DUALMODE
#include <mach/eint.h>
#include <linux/irq.h>
#include <linux/switch.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <mach/mt_boot.h>
#undef DRV_Reg32
#undef DRV_WriteReg32
#include <mach/battery_meter.h>
#ifdef CONFIG_USBIF_COMPLIANCE
#define CONFIG_MTK_OTG_PMIC_BOOST_5V
#endif

#undef DRV_Reg32
#undef DRV_WriteReg32
#include <mach/upmu_common.h>
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#ifdef CONFIG_PROJECT_PHY
#include <linux/mu3phy/mtk-phy-asic.h>
#endif
#endif

#include <hub.h>
#include <linux/jiffies.h>
#include <linux/list.h>
#include <asm/atomic.h>
#include <linux/usb/hcd.h>
#include <mach/mt_chip.h>

#ifdef CONFIG_MTK_FPGA
#include <linux/mu3phy/mtk-phy.h>
#endif

#ifdef CONFIG_USB_MTK_DUALMODE
#ifdef CONFIG_MTK_OTG_PMIC_BOOST_5V
#ifdef CONFIG_MTK_OTG_OC_DETECTOR
#include <linux/kobject.h>
#include <linux/miscdevice.h>
#endif
#endif
#endif

#define mtk_xhci_mtk_log(fmt, args...) \
    printk("%s(%d): " fmt, __func__, __LINE__, ##args)

#define RET_SUCCESS 0
#define RET_FAIL 1

struct xhci_hcd *mtk_xhci;
static spinlock_t *mtk_hub_event_lock;
static struct list_head* mtk_hub_event_list;
static int mtk_ep_count;
static int vbus_on = 0;

static struct wake_lock mtk_xhci_wakelock;

#ifdef CONFIG_MTK_FPGA
void __iomem *u3_base;
void __iomem *u3_sif_base;
void __iomem *u3_sif2_base;
void __iomem *i2c1_base;
#endif

//struct u3phy_info *u3phy;
//struct u3phy_operator *u3phy_ops;

#ifdef CONFIG_USB_MTK_DUALMODE
enum idpin_state {
	IDPIN_OUT,
	IDPIN_IN_HOST,
	IDPIN_IN_DEVICE,
};

static int mtk_idpin_irqnum;
static enum idpin_state mtk_idpin_cur_stat = IDPIN_OUT;
static struct switch_dev mtk_otg_state;

static struct delayed_work mtk_xhci_delaywork;

int mtk_iddig_debounce = 10;
module_param(mtk_iddig_debounce, int, 0644);
#if defined(CONFIG_MTK_BQ24296_SUPPORT)
extern void bq24296_set_otg_config(kal_uint32 val);
extern void bq24296_set_boostv(kal_uint32 val);
extern void bq24296_set_boost_lim(kal_uint32 val);
extern void bq24296_set_en_hiz(kal_uint32 val);
#endif

static void mtk_set_iddig_out_detect(void)
{
	irq_set_irq_type(mtk_idpin_irqnum, IRQF_TRIGGER_HIGH);
	enable_irq(mtk_idpin_irqnum);
}

static void mtk_set_iddig_in_detect(void)
{
	irq_set_irq_type(mtk_idpin_irqnum, IRQF_TRIGGER_LOW);
	enable_irq(mtk_idpin_irqnum);
}

static bool mtk_is_charger_4_vol(void)
{
	int vol = battery_meter_get_charger_voltage();
	mtk_xhci_mtk_log("voltage(%d)\n", vol);

#if defined(CONFIG_USBIF_COMPLIANCE) || defined(CONFIG_MTK_USB_EVB_BOARD)
	return false ;
#else
	return (vol > 4000) ? true : false;
#endif
}

bool mtk_is_usb_id_pin_short_gnd(void)
{
	return (mtk_idpin_cur_stat != IDPIN_OUT) ? true : false;
}

#ifdef CONFIG_MTK_OTG_PMIC_BOOST_5V
#define PMIC_REG_BAK_NUM (10)

#ifdef CONFIG_MTK_OTG_OC_DETECTOR
#define OC_DETECTOR_TIMER (2000)
static struct delayed_work mtk_xhci_oc_delaywork;

static struct miscdevice xhci_misc_uevent = {
         .minor = MISC_DYNAMIC_MINOR,
         .name = "xhci_misc_uevent",
         .fops = NULL,
};
#endif

extern U32 pmic_read_interface(U32 RegNum, U32 * val, U32 MASK, U32 SHIFT);
extern U32 pmic_config_interface(U32 RegNum, U32 val, U32 MASK, U32 SHIFT);

U32 pmic_bak_regs[PMIC_REG_BAK_NUM][2] = {
		{0x8D22, 0}, {0x8D14, 0}, {0x803C, 0}, {0x8036, 0}, {0x8D24, 0},
		{0x8D16, 0}, {0x803A, 0}, {0x8046, 0}, {0x803E, 0}, {0x8044, 0}
	};

static void pmic_save_regs(void){
	int i;

	for(i = 0; i < PMIC_REG_BAK_NUM; i++){
		pmic_read_interface(pmic_bak_regs[i][0], &pmic_bak_regs[i][1], 0xffffffff, 0);
	}
}

static void pmic_restore_regs(void){
	int i;

	for(i = 0; i < PMIC_REG_BAK_NUM; i++){
		pmic_config_interface(pmic_bak_regs[i][0], pmic_bak_regs[i][1], 0xffffffff, 0);
	}
}

void mtk_enable_pmic_otg_mode(void)
{
	int val = 0;
	int cnt = 0;
	
    vbus_on++;
    /// vbus_on =1;
    mtk_xhci_mtk_log("set pmic power on, %d\n", vbus_on);
    #if 1
	if(vbus_on > 1)
	{
	    return;
    }
    #endif
#if defined(CONFIG_MTK_BQ24296_SUPPORT)
        bq24296_set_otg_config(0x1); //OTG
       // bq24296_set_boostv(0x7); //boost voltage 4.998V
      //  bq24296_set_boost_lim(0x1); //1.5A on VBUS
      //  bq24296_set_en_hiz(0x0);
#else
	mt_set_gpio_mode(GPIO_OTG_DRVVBUS_PIN, GPIO_MODE_GPIO);
	mt_set_gpio_pull_select(GPIO_OTG_DRVVBUS_PIN, GPIO_PULL_DOWN);
	mt_set_gpio_pull_enable(GPIO_OTG_DRVVBUS_PIN, GPIO_PULL_ENABLE);
#endif
	/* save PMIC related registers */
	pmic_save_regs();

	pmic_config_interface(0x8D22, 0x1, 0x1, 12);
	pmic_config_interface(0x8D14, 0x1, 0x1, 12);
	pmic_config_interface(0x803C, 0x3, 0x3, 0);
	pmic_config_interface(0x803C, 0x2, 0x3, 2);
	pmic_config_interface(0x803C, 0x1, 0x1, 14);
	pmic_config_interface(0x8036, 0x0, 0x0, 0);
	pmic_config_interface(0x8D24, 0xf, 0xf, 12);
	pmic_config_interface(0x8D16, 0x1, 0x1, 15);
	pmic_config_interface(0x803A, 0x1, 0x1, 6);
	pmic_config_interface(0x8046, 0x00A0, 0xffff, 0);
	pmic_config_interface(0x803E, 0x1, 0x1, 2);
	pmic_config_interface(0x803E, 0x1, 0x1, 3);
	pmic_config_interface(0x803E, 0x3, 0x3, 8);

	#ifdef CONFIG_MTK_OTG_OC_DETECTOR
	/* Current limit is on */
	pmic_config_interface(0x803E, 0x1, 0x1, 10);
	#else
	pmic_config_interface(0x803E, 0x0, 0x1, 10);
	#endif
	
	pmic_config_interface(0x8044, 0x3, 0x3, 0);
	pmic_config_interface(0x8044, 0x3, 0x7, 8);
	pmic_config_interface(0x8044, 0x1, 0x1, 11);

	pmic_config_interface(0x809C, 0x8000, 0xFFFF, 0);

	val = 0;
	while (val == 0) {
		pmic_read_interface(0x809A, &val, 0x1, 15);
	}

	pmic_config_interface(0x8084, 0x1, 0x1, 0);
	mdelay(50);

	val = 0;
	while (val == 0 && cnt < 20) {
		pmic_read_interface(0x8060, &val, 0x1, 14);
		cnt++;
		mdelay(2);
	}

	#ifdef CONFIG_MTK_OTG_OC_DETECTOR
	schedule_delayed_work_on(0, &mtk_xhci_oc_delaywork, msecs_to_jiffies(OC_DETECTOR_TIMER)); 
	#endif
	mtk_xhci_mtk_log("set pmic power on(cnt:%d), done\n", cnt);
}

void mtk_disable_pmic_otg_mode(void)
{
	int val =0;
	int cnt = 0;
	
	///vbus_on = 0;
	
    vbus_on--;
    mtk_xhci_mtk_log("set pmic power off %d\n", vbus_on);
    
    if(vbus_on < 0 || vbus_on > 0)
    {
        if(vbus_on < 0)
            vbus_on = 0;
        return;
    }
  #if defined(CONFIG_MTK_BQ24296_SUPPORT)
        bq24296_set_otg_config(0); 
#endif  
	  
	
	pmic_config_interface(0x8068, 0x0, 0x1, 0);
	pmic_config_interface(0x8084, 0x0, 0x1, 0);
	mdelay(50);
	pmic_config_interface(0x8068, 0x0, 0x1, 1);

	val = 1;
	while (val == 1 && cnt <20) {
		pmic_read_interface(0x805E, &val, 0x1, 4);
		cnt++;
		mdelay(2);
	}

	#if 0
	pmic_config_interface(0x809E, 0x8000, 0xFFFF, 0);

	val = 1;
	while (val == 1) {
		pmic_read_interface(0x809A, &val, 0x1, 15);
	}
	#endif

	/* restore PMIC registers */
	pmic_restore_regs();
	#ifdef CONFIG_MTK_OTG_OC_DETECTOR
	cancel_delayed_work(&mtk_xhci_oc_delaywork);
	#endif
	mtk_xhci_mtk_log("set pmic power off(cnt:%d), done\n", cnt);
}

#ifdef CONFIG_MTK_OTG_OC_DETECTOR
void xhci_send_event(char * event)
{
	char udev_event[128];
	char *envp[] = {udev_event, NULL };
	int ret ;

	snprintf(udev_event, 128, "XHCI_MISC_UEVENT=%s",event);
	mtk_xhci_mtk_log("send %s in %s\n", udev_event, kobject_get_path(&xhci_misc_uevent.this_device->kobj, GFP_KERNEL));
	ret = kobject_uevent_env(&xhci_misc_uevent.this_device->kobj, KOBJ_CHANGE, envp);
	if (ret < 0)
		mtk_xhci_mtk_log("fail, ret(%d)\n", ret);
}

static bool mtk_is_over_current(void)
{
	int vol = battery_meter_get_charger_voltage();
	
	if(vol < 4200){
		mtk_xhci_mtk_log("over current occurs, voltage(%d)\n", vol);
		return true;
	}

	return false;
}

static void mtk_xhci_oc_detector(struct work_struct *work)
{
	int ret;
	
	if(mtk_is_over_current()){
		xhci_send_event("OVER_CURRENT");
		mtk_disable_pmic_otg_mode();
	}
	else{
		ret = schedule_delayed_work_on(0, &mtk_xhci_oc_delaywork, msecs_to_jiffies(OC_DETECTOR_TIMER));
	}
}
#endif

#endif

void mtk_hub_event_steal(spinlock_t *lock, struct list_head* list){
	mtk_hub_event_lock = lock;
	mtk_hub_event_list = list;
}

void mtk_ep_count_inc(void){
	mtk_ep_count++;
}

void mtk_ep_count_dec(void){
	mtk_ep_count--;
}

int mtk_is_hub_active(void){
	struct usb_hcd *hcd = xhci_to_hcd(mtk_xhci);
	struct usb_device *rhdev = hcd->self.root_hub;
	struct usb_hub *hub = usb_hub_to_struct_hub(rhdev);

	bool ret = true;

	spin_lock_irq(mtk_hub_event_lock);
	if((mtk_ep_count == 0) && (list_empty(&hub->event_list) == 1) && (atomic_read(&(hub->kref.refcount)) == 1)){
		ret = false;
	}
	spin_unlock_irq(mtk_hub_event_lock);

	return ret;
}


static int mtk_xhci_hcd_init(void)
{
	int retval;

	retval = xhci_register_plat();
	if (retval < 0) {
		printk(KERN_ERR "Problem registering platform driver.\n");
		return retval;
	}
	retval = xhci_attrs_init();
	if (retval < 0) {
		printk(KERN_ERR "Problem creating xhci attributes.\n");
		goto unreg_plat;
	}
	
	#ifdef CONFIG_MTK_OTG_PMIC_BOOST_5V
	#ifdef CONFIG_MTK_OTG_OC_DETECTOR
	retval = misc_register(&xhci_misc_uevent);
	if (retval){
		printk(KERN_ERR "create the xhci_uevent_device fail, ret(%d)\n", retval) ;
		goto unreg_attrs;
	}	
	#endif
	#endif
	
	/*
	 * Check the compiler generated sizes of structures that must be laid
	 * out in specific ways for hardware access.
	 */
	BUILD_BUG_ON(sizeof(struct xhci_doorbell_array) != 256 * 32 / 8);
	BUILD_BUG_ON(sizeof(struct xhci_slot_ctx) != 8 * 32 / 8);
	BUILD_BUG_ON(sizeof(struct xhci_ep_ctx) != 8 * 32 / 8);
	/* xhci_device_control has eight fields, and also
	 * embeds one xhci_slot_ctx and 31 xhci_ep_ctx
	 */
	BUILD_BUG_ON(sizeof(struct xhci_stream_ctx) != 4 * 32 / 8);
	BUILD_BUG_ON(sizeof(union xhci_trb) != 4 * 32 / 8);
	BUILD_BUG_ON(sizeof(struct xhci_erst_entry) != 4 * 32 / 8);
	BUILD_BUG_ON(sizeof(struct xhci_cap_regs) != 7 * 32 / 8);
	BUILD_BUG_ON(sizeof(struct xhci_intr_reg) != 8 * 32 / 8);
	/* xhci_run_regs has eight fields and embeds 128 xhci_intr_regs */
	BUILD_BUG_ON(sizeof(struct xhci_run_regs) != (8 + 8 * 128) * 32 / 8);
	return 0;
#ifdef CONFIG_MTK_OTG_PMIC_BOOST_5V
#ifdef CONFIG_MTK_OTG_OC_DETECTOR
unreg_attrs:
	xhci_attrs_exit();
#endif
#endif
unreg_plat:
	xhci_unregister_plat();
	return retval;
}

static void mtk_xhci_hcd_cleanup(void)
{
	#ifdef CONFIG_MTK_OTG_PMIC_BOOST_5V
	#ifdef CONFIG_MTK_OTG_OC_DETECTOR
	misc_deregister(&xhci_misc_uevent);
	#endif
	#endif
	
	xhci_attrs_exit();
	xhci_unregister_plat();
}

static void mtk_xhci_imod_set(u32 imod)
{
	u32 temp;

	temp = xhci_readl(mtk_xhci, &mtk_xhci->ir_set->irq_control);
	temp &= ~0xFFFF;
	temp |= imod;
	xhci_writel(mtk_xhci, temp, &mtk_xhci->ir_set->irq_control);
}

extern void usb20_pll_settings(bool host, bool forceOn);

static int mtk_xhci_driver_load(void)
{
	int ret = 0;

	/* recover clock/power setting and deassert reset bit of mac */
	usb_phy_recover(0);
	ret = mtk_xhci_hcd_init();
	if (ret || !mtk_xhci) {
		ret = -ENXIO;
		goto _err;
	}

	/* for performance, fixed the interrupt moderation from 0xA0(default) to 0x30 */
	mtk_xhci_imod_set(0x30);

#ifdef CONFIG_USBIF_COMPLIANCE
	mtk_enable_pmic_otg_mode();
	enableXhciAllPortPower(mtk_xhci);
#else
#ifdef CONFIG_MTK_OTG_PMIC_BOOST_5V
	mtk_enable_pmic_otg_mode();

	/* USB PLL Force settings */
	usb20_pll_settings(true, true);
#else
	enableXhciAllPortPower(mtk_xhci);
#endif
#endif

	return 0;

_err:
	mtk_xhci_mtk_log("ret(%d), mtk_xhci(0x%p)\n", ret, mtk_xhci);
	usb_phy_savecurrent(1);
	return ret;
}

static void mtk_xhci_disPortPower(void)
{
#ifdef CONFIG_USBIF_COMPLIANCE
	mtk_disable_pmic_otg_mode();
	disableXhciAllPortPower(mtk_xhci);
#else
	#ifdef CONFIG_MTK_OTG_PMIC_BOOST_5V
	mtk_disable_pmic_otg_mode();
	#else
	disableXhciAllPortPower(mtk_xhci);
	#endif
#endif
}

static void mtk_xhci_driver_unload(void)
{
	mtk_xhci_hcd_cleanup();
	/* close clock/power setting and assert reset bit of mac */
	usb_phy_savecurrent(1);
}

void mtk_xhci_switch_init(void)
{
	mtk_otg_state.name = "otg_state";
	mtk_otg_state.index = 0;
	mtk_otg_state.state = 0;

#ifndef CONFIG_USBIF_COMPLIANCE
	if (switch_dev_register(&mtk_otg_state))
		mtk_xhci_mtk_log("switch_dev_register fail\n");
	else
		mtk_xhci_mtk_log("switch_dev register success\n");
#endif
}

#if defined(MHL_SII8348)
extern void switch_mhl_to_d3(void);
#endif
void mtk_xhci_mode_switch(struct work_struct *work)
{
	static bool is_load = false;
	static bool is_pwoff = false;
	int ret = 0;

	if (mtk_idpin_cur_stat == IDPIN_OUT) {
		is_load = false;

		/* expect next isr is for id-pin out action */
		mtk_idpin_cur_stat = (mtk_is_charger_4_vol())? IDPIN_IN_DEVICE : IDPIN_IN_HOST;
		/* make id pin to detect the plug-out */
		mtk_set_iddig_out_detect();

		if (mtk_idpin_cur_stat == IDPIN_IN_DEVICE)
			goto done;

		ret = mtk_xhci_driver_load();
		if (!ret) {
			is_load = true;
			mtk_xhci_wakelock_lock();
#ifndef CONFIG_USBIF_COMPLIANCE
			switch_set_state(&mtk_otg_state, 1);
#endif
		}

	} else {		/* IDPIN_OUT */
		if (is_load) {
			if(!is_pwoff)
				mtk_xhci_disPortPower();

			/* prevent hang here */
			/* if(mtk_is_hub_active()){
				is_pwoff = true;
				schedule_delayed_work_on(0, &mtk_xhci_delaywork, msecs_to_jiffies(mtk_iddig_debounce));
				mtk_xhci_mtk_log("wait, hub is still active, ep cnt %d !!!\n", mtk_ep_count);
				return;
			}*/

			/* USB PLL Force settings */
			usb20_pll_settings(true, false);

			mtk_xhci_driver_unload();
			is_pwoff = false;
			is_load = false;
#ifndef CONFIG_USBIF_COMPLIANCE
			switch_set_state(&mtk_otg_state, 0);
#endif
			mtk_xhci_wakelock_unlock();

#if defined(MHL_SII8348)
			switch_mhl_to_d3();
#endif			
		}

		/* expect next isr is for id-pin in action */
		mtk_idpin_cur_stat = IDPIN_OUT;
		/* make id pin to detect the plug-in */
		mtk_set_iddig_in_detect();
	}

done:
	mtk_xhci_mtk_log("current mode is %s, ret(%d), switch(%d)\n",
			 (mtk_idpin_cur_stat == IDPIN_IN_HOST) ? "host" :
			 (mtk_idpin_cur_stat == IDPIN_IN_DEVICE) ? "id_device" : "device",
			 ret, mtk_otg_state.state);
}

static irqreturn_t xhci_eint_iddig_isr(int irqnum, void *data)
{
	int ret;
	//schedule_delayed_work(&mtk_xhci_delay_work, mtk_iddig_debounce*HZ/1000);
	/* microseconds */
	ret = schedule_delayed_work_on(0, &mtk_xhci_delaywork, msecs_to_jiffies(mtk_iddig_debounce));
	mtk_xhci_mtk_log("schedule to delayed work, ret(%d), gpio_mode(%d), gpio_pull_enable(%d), gpio_pull_select(%d)\n",
		ret, mt_get_gpio_mode(GPIO_OTG_IDDIG_EINT_PIN), mt_get_gpio_pull_enable(GPIO_OTG_IDDIG_EINT_PIN), mt_get_gpio_pull_select(GPIO_OTG_IDDIG_EINT_PIN));

	disable_irq_nosync(irqnum);
	return IRQ_HANDLED;
}

int mtk_xhci_eint_iddig_init(void)
{
	int retval;
	struct device_node *node;
	int iddig_gpio;

    mt_set_gpio_mode(GPIO_OTG_IDDIG_EINT_PIN,GPIO_OTG_IDDIG_EINT_PIN_M_IDDIG);
    mt_set_gpio_dir(GPIO_OTG_IDDIG_EINT_PIN,GPIO_DIR_IN);
    mt_set_gpio_pull_enable(GPIO_OTG_IDDIG_EINT_PIN,GPIO_PULL_ENABLE);
    mt_set_gpio_pull_select(GPIO_OTG_IDDIG_EINT_PIN,GPIO_PULL_UP);

	node = of_find_compatible_node(NULL, NULL, "mediatek,USB3_XHCI");
	if(node){
		retval = of_property_read_u32(node, "iddig-gpio-num", &iddig_gpio);
		if(retval){
			mtk_xhci_mtk_log("get iddig-gpio-num failed, retval(%d)!!!\n", retval);
			return retval;
		}

		mtk_xhci_mtk_log("iddig gpio num, %d\n", iddig_gpio);
	}
	else{
		mtk_xhci_mtk_log("cannot get the node\n");
		return -ENODEV;
	}

	INIT_DELAYED_WORK(&mtk_xhci_delaywork, mtk_xhci_mode_switch);
	#ifdef CONFIG_MTK_OTG_PMIC_BOOST_5V
	#ifdef CONFIG_MTK_OTG_OC_DETECTOR
	INIT_DELAYED_WORK(&mtk_xhci_oc_delaywork, mtk_xhci_oc_detector);
	#endif
	#endif

	mtk_idpin_irqnum = mt_gpio_to_irq(iddig_gpio);

	/* microseconds */
	mt_gpio_set_debounce(iddig_gpio, 150000); 
 
	retval =
	    request_irq(mtk_idpin_irqnum, xhci_eint_iddig_isr, IRQF_TRIGGER_LOW, "iddig_eint",
			NULL);
	if (retval != 0) {
		mtk_xhci_mtk_log("request_irq fail, ret %d, irqnum %d!!!\n", retval, mtk_idpin_irqnum);
		return retval;
	}
	mtk_xhci_mtk_log("external iddig register done, irqnum = %d, gpio_mode(%d), gpio_pull_enable(%d), gpio_pull_select(%d)\n", 
		mtk_idpin_irqnum, mt_get_gpio_mode(GPIO_OTG_IDDIG_EINT_PIN), mt_get_gpio_pull_enable(GPIO_OTG_IDDIG_EINT_PIN), mt_get_gpio_pull_select(GPIO_OTG_IDDIG_EINT_PIN));

	/* set in-detect and umask the iddig interrupt */
	//enable_irq(mtk_idpin_irqnum);
	return retval;
}

void mtk_xhci_eint_iddig_deinit(void)
{
	//mt_eint_registration(IDDIG_EINT_PIN, EINTF_TRIGGER_LOW, NULL, false);
	disable_irq_nosync(mtk_idpin_irqnum);

	free_irq(mtk_idpin_irqnum, NULL);

	cancel_delayed_work(&mtk_xhci_delaywork);
	#ifdef CONFIG_MTK_OTG_PMIC_BOOST_5V
	#ifdef CONFIG_MTK_OTG_OC_DETECTOR
	cancel_delayed_work(&mtk_xhci_oc_delaywork);
	#endif
	#endif

	mtk_idpin_cur_stat = IDPIN_OUT;

	mtk_xhci_mtk_log("external iddig unregister done.\n");
}

void mtk_set_host_mode_in_host(void)
{
	mtk_idpin_cur_stat = IDPIN_IN_HOST;
}

void mtk_set_host_mode_out(void)
{
	mtk_idpin_cur_stat = IDPIN_OUT;
}

bool mtk_is_host_mode(void)
{
	return (vbus_on > 0 || mtk_idpin_cur_stat == IDPIN_IN_HOST) ? true : false;
}

#endif

void mtk_xhci_wakelock_init(void)
{
	wake_lock_init(&mtk_xhci_wakelock, WAKE_LOCK_SUSPEND, "xhci.wakelock");
}

void mtk_xhci_wakelock_lock(void)
{
	if (!wake_lock_active(&mtk_xhci_wakelock))
		wake_lock(&mtk_xhci_wakelock);
	mtk_xhci_mtk_log("done\n");
}

void mtk_xhci_wakelock_unlock(void)
{
	if (wake_lock_active(&mtk_xhci_wakelock))
		wake_unlock(&mtk_xhci_wakelock);
	mtk_xhci_mtk_log("done\n");
}

void mtk_xhci_set(struct usb_hcd *hcd, struct xhci_hcd *xhci)
{
	struct platform_device *pdev = to_platform_device(hcd->self.controller);
	struct resource *sif_res, *sif2_res;

	xhci->base_regs = (unsigned long)hcd->regs;

	sif_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, XHCI_SIF_REGS_ADDR_RES_NAME);
	if (!sif_res){
		printk("%s(%d): cannot get sif resources\n", __func__, __LINE__);
	}

	xhci->sif_regs = (unsigned long)ioremap(sif_res->start, resource_size(sif_res));

	printk("%s(%d): sif_base, logic 0x%p, phys 0x%p\n", __func__, __LINE__,
		(void *)(unsigned long)sif_res->start, (void *)xhci->sif_regs);

	sif2_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, XHCI_SIF2_REGS_ADDR_RES_NAME);
	if (!sif2_res){
		printk("%s(%d): cannot get sif2 resources\n", __func__, __LINE__);
	}

	xhci->sif2_regs = (unsigned long)ioremap(sif2_res->start, resource_size(sif2_res));

	printk("%s(%d): sif2_base, logic 0x%p, phys 0x%p\n", __func__, __LINE__,
		(void *)(unsigned long)sif2_res->start, (void *)xhci->sif2_regs);

	mtk_xhci_mtk_log("mtk_xhci = 0x%p\n", xhci);
	mtk_xhci = xhci;
}

void mtk_xhci_reset(struct xhci_hcd *xhci){
	iounmap((volatile void __iomem *)xhci->sif_regs);
	mtk_xhci_mtk_log("iounmap, sif_reg, 0x%p\n", (void *)xhci->sif_regs);
	iounmap((volatile void __iomem *)xhci->sif2_regs);
	mtk_xhci_mtk_log("iounmap, sif2_reg, 0x%p\n", (void *)xhci->sif2_regs);
	mtk_xhci = NULL;
}

void mtk_xhci_ck_timer_init(struct xhci_hcd *xhci)
{
	void __iomem *addr;
	u32 temp = 0;
	int num_u3_port;
	unsigned int hw_code = mt_get_chip_hw_code();
	CHIP_SW_VER sw_code = mt_get_chip_sw_ver();

	mtk_xhci_mtk_log("hw code(0x%x), sw_code(0x%x)\n", hw_code, sw_code);

	if (0x6595 == hw_code) {
		/* The sys125_ck = 1/2 sys_ck = 62.5MHz */
		addr = (void __iomem *)_SSUSB_SYS_CK_CTRL(xhci->sif_regs);
		temp = readl(addr);
		temp |= SSUSB_SYS_CK_DIV2_EN;
		writel(temp, addr);
		mtk_xhci_mtk_log("mu3d sys_clk, addr 0x%p, value 0x%x\n",
				(void *)_SSUSB_SYS_CK_CTRL(xhci->sif_regs), readl((__u32 __iomem *)_SSUSB_SYS_CK_CTRL(xhci->sif_regs)));
		
		num_u3_port = SSUSB_U3_PORT_NUM(readl((void __iomem *)_SSUSB_IP_CAP(xhci->sif_regs)));
		if (num_u3_port) {
			#if 0
			//set MAC reference clock speed
			addr = (void __iomem *) (_SSUSB_U3_MAC_BASE(xhci->base_regs) + U3_UX_EXIT_LFPS_TIMING_PAR);
			temp = readl(addr);
			temp &= ~(0xff << U3_RX_UX_EXIT_LFPS_REF_OFFSET);
			temp |= (U3_RX_UX_EXIT_LFPS_REF << U3_RX_UX_EXIT_LFPS_REF_OFFSET);
			writel(temp, addr);
			addr = (void __iomem *)(_SSUSB_U3_MAC_BASE(xhci->base_regs) + U3_REF_CK_PAR);
			temp = readl(addr);
			temp &= ~(0xff);
			temp |= U3_REF_CK_VAL;
			writel(temp, addr);
			#endif

			/* set U3 MAC SYS_CK */
			addr = (void __iomem *)(_SSUSB_U3_SYS_BASE(xhci->base_regs) + U3_TIMING_PULSE_CTRL);
			temp = readl(addr);
			temp &= ~(0xff);
			temp |= MTK_CNT_1US_VALUE;
			writel(temp, addr);
		}
		
		/* set U2 MAC SYS_CK */
		addr = (void __iomem *)(_SSUSB_U2_SYS_BASE(xhci->base_regs) + USB20_TIMING_PARAMETER);
		temp &= ~(0xff);
		temp |= MTK_TIME_VALUE_1US;
		writel(temp, addr);

		mtk_xhci_mtk_log("mu3d u2 mac sys_clk, addr 0x%p, value 0x%x\n",
							(void *)(_SSUSB_U2_SYS_BASE(xhci->base_regs) + USB20_TIMING_PARAMETER),
							readl((void __iomem *)(_SSUSB_U2_SYS_BASE(xhci->base_regs) + (unsigned long)USB20_TIMING_PARAMETER)));

		#if 0
		if (num_u3_port) {
			//set LINK_PM_TIMER=3
			addr = (void __iomem *)(_SSUSB_U3_SYS_BASE(xhci->base_regs) + LINK_PM_TIMER);
			temp = readl(addr);
			temp &= ~(0xf);
			temp |= MTK_PM_LC_TIMEOUT_VALUE;
			writel(temp, addr);
		}
		#endif
	}
}

static void setLatchSel(struct xhci_hcd *xhci)
{
	void __iomem *latch_sel_addr;
	u32 latch_sel_value;
	int num_u3_port;

	num_u3_port = SSUSB_U3_PORT_NUM(readl((void __iomem *)_SSUSB_IP_CAP(xhci->sif_regs)));
	if (num_u3_port <= 0)
		return;

	latch_sel_addr = (void __iomem *) _U3_PIPE_LATCH_SEL_ADD(xhci->base_regs);
	latch_sel_value = ((U3_PIPE_LATCH_TX) << 2) | (U3_PIPE_LATCH_RX);
	writel(latch_sel_value, latch_sel_addr);
}

#ifndef CONFIG_USB_MTK_DUALMODE
static int mtk_xhci_phy_init(int argc, char **argv)
{
	/* initialize PHY related data structure */
	if (!u3phy_ops)
		u3phy_init();

	/* USB 2.0 slew rate calibration */
	if (u3phy_ops->u2_slew_rate_calibration)
		u3phy_ops->u2_slew_rate_calibration(u3phy);
	else
		printk(KERN_ERR "WARN: PHY doesn't implement u2 slew rate calibration function\n");

	/* phy initialization */
	if (u3phy_ops->init(u3phy) != PHY_TRUE)
		return RET_FAIL;

	printk(KERN_ERR "phy registers and operations initial done\n");
	return RET_SUCCESS;
}
#endif

int mtk_xhci_ip_init(struct usb_hcd *hcd, struct xhci_hcd *xhci)
{
	mtk_xhci_set(hcd, xhci);
	
#ifdef CONFIG_MTK_FPGA
	u3_base = xhci->base_regs;
	u3_sif_base = xhci->sif_regs;
	u3_sif2_base = xhci->sif2_regs;
	i2c1_base = ioremap(0x11008000, 0x1000);
	if (!(i2c1_base)) {
		pr_err("Can't remap I2C1 BASE\n");
	}
	printk("%s(%d): i2c1_base, logic x%x, phys 0x%p\n", __func__, __LINE__, 0x11008000, (void *)i2c1_base);
#endif

#ifdef CONFIG_MTK_LDVT
	mt_set_gpio_mode(121, 4);
#endif

	/* phy initialization is done by device, if target runs on dual mode */
#ifndef CONFIG_USB_MTK_DUALMODE
	mtk_xhci_phy_init(0, NULL);
	enableAllClockPower(xhci, 1);	/* host do reset ip */
#else
	enableAllClockPower(xhci, 1);	/* device do reset ip */
#endif

	setLatchSel(xhci);
	mtk_xhci_ck_timer_init(xhci);
	mtk_xhci_scheduler_init();

	return 0;
}

#if 0
int mtk_xhci_get_port_num(void)
{
	return SSUSB_U3_PORT_NUM(readl((void __iomem *)SSUSB_IP_CAP))
	    + SSUSB_U2_PORT_NUM(readl((void __iomem *)SSUSB_IP_CAP));
}
#endif

