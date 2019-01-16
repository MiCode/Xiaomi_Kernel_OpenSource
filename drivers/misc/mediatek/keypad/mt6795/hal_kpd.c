#include <mach/hal_pub_kpd.h>
#include <mach/hal_priv_kpd.h>
#include <mach/mt_clkmgr.h>
#include <linux/kpd.h>
#ifdef CONFIG_MTK_AEE_POWERKEY_HANG_DETECT
#include <linux/aee.h>
#endif
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif


#define KPD_DEBUG	KPD_YES

#define KPD_SAY		"kpd: "
#if KPD_DEBUG
#define kpd_print(fmt, arg...)	printk(KPD_SAY fmt, ##arg)
#else
#define kpd_print(fmt, arg...)	do {} while (0)
#endif

extern struct input_dev *kpd_input_dev;

#if KPD_PWRKEY_USE_EINT
static u8 kpd_pwrkey_state = !KPD_PWRKEY_POLARITY;
#endif

static int kpd_show_hw_keycode = 1;
static int kpd_enable_lprst = 1;
static void mtk_kpd_get_gpio_col(unsigned int COL_REG[]);
/* for Volumeup key using EINT */
#ifdef HTC_KPD_USE_EINT
static unsigned int gpiopin;
static unsigned int kpddebounce;
static int kpd_key_irq;
#define KPD_PRESSED        (1)
#define KPD_RELEASED       (0)
static int kpd_eint_state = KPD_RELEASED;
static void kpd_key_handler(unsigned long data);
static DECLARE_TASKLET(kpd_key_tasklet, kpd_key_handler, 0);
#endif

static u16 kpd_keymap[KPD_NUM_KEYS] = KPD_INIT_KEYMAP();
static u16 kpd_keymap_state[KPD_NUM_MEMS] = {
	0xffff, 0xffff, 0xffff, 0xffff, 0x00ff
};

static bool kpd_sb_enable = false;

#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
static void sb_kpd_release_keys(struct input_dev *dev)
{
	int code;
	for (code = 0; code <= KEY_MAX; code++) {
		if (test_bit(code, dev->keybit)) {
			kpd_print("report release event for sb plug in! keycode:%d \n",code);
			input_report_key(dev, code, 0);
			input_sync(dev);
		}
	}
}

void sb_kpd_enable(void)
{
	kpd_sb_enable = true;
	kpd_print("sb_kpd_enable performed! \n");	
	mt_reg_sync_writew(0x0, KP_EN);
	sb_kpd_release_keys(kpd_input_dev);
}

void sb_kpd_disable(void)
{
	kpd_sb_enable = false;
	kpd_print("sb_kpd_disable performed! \n");
	mt_reg_sync_writew(0x1, KP_EN);
}
#else
void sb_kpd_enable(void)
{
	kpd_print("sb_kpd_enable empty function for HAL!\n");	
}

void sb_kpd_disable(void)
{
	kpd_print("sb_kpd_disable empty function for HAL!\n");
}
#endif

#ifndef EVB_PLATFORM
static void enable_kpd(int enable)
{
	if(enable ==1)
		{
			mt_reg_sync_writew((u16)(enable), KP_EN);
			kpd_print("KEYPAD is enabled\n");
		}
	else if(enable == 0)
		{
			mt_reg_sync_writew((u16)(enable), KP_EN);
			kpd_print("KEYPAD is disabled\n");
		}
}
#endif

void kpd_slide_qwerty_init(void){
#if KPD_HAS_SLIDE_QWERTY
	bool evdev_flag=false;
	bool power_op=false;
	struct input_handler *handler;
	struct input_handle *handle;
	handle = rcu_dereference(dev->grab);
	if (handle)
	{
		handler = handle->handler;
		if(strcmp(handler->name, "evdev")==0) 
		{
			return -1;
		}	
	}
	else 
	{
		list_for_each_entry_rcu(handle, &dev->h_list, d_node) {
			handler = handle->handler;
			if(strcmp(handler->name, "evdev")==0) 
			{
				evdev_flag=true;
				break;
			}
		}
		if(evdev_flag==false)
		{
			return -1;	
		}	
	}

	power_op = powerOn_slidePin_interface();
	if(!power_op) {
		printk(KPD_SAY "Qwerty slide pin interface power on fail\n");
	} else {
		kpd_print("Qwerty slide pin interface power on success\n");
	}
		
	mt_eint_set_sens(KPD_SLIDE_EINT, KPD_SLIDE_SENSITIVE);
	mt_eint_set_hw_debounce(KPD_SLIDE_EINT, KPD_SLIDE_DEBOUNCE);
	mt_eint_registration(KPD_SLIDE_EINT, true, KPD_SLIDE_POLARITY,
	                         kpd_slide_eint_handler, false);
	                         
	power_op = powerOff_slidePin_interface();
	if(!power_op) {
		printk(KPD_SAY "Qwerty slide pin interface power off fail\n");
	} else {
		kpd_print("Qwerty slide pin interface power off success\n");
	}
#endif
return;
}
/************************************************************/
/**************************************/
#ifdef CONFIG_MTK_LDVT	
void mtk_kpd_gpios_get(unsigned int ROW_REG[], unsigned int COL_REG[])
{
	int i;
	for(i = 0; i< 8; i++)
	{
		ROW_REG[i] = 0;
		COL_REG[i] = 0;
	}
	#ifdef GPIO_KPD_KROW0_PIN
		ROW_REG[0] = GPIO_KPD_KROW0_PIN;
	#endif

	#ifdef GPIO_KPD_KROW1_PIN
		ROW_REG[1] = GPIO_KPD_KROW1_PIN;
	#endif

	#ifdef GPIO_KPD_KROW2_PIN
		ROW_REG[2] = GPIO_KPD_KROW2_PIN;
	#endif

	#ifdef GPIO_KPD_KROW3_PIN
		ROW_REG[3] = GPIO_KPD_KROW3_PIN;
	#endif

	#ifdef GPIO_KPD_KROW4_PIN
		ROW_REG[4] = GPIO_KPD_KROW4_PIN;
	#endif

	#ifdef GPIO_KPD_KROW5_PIN
		ROW_REG[5] = GPIO_KPD_KROW5_PIN;
	#endif

	#ifdef GPIO_KPD_KROW6_PIN
		ROW_REG[6] = GPIO_KPD_KROW6_PIN;
	#endif

	#ifdef GPIO_KPD_KROW7_PIN
		ROW_REG[7] = GPIO_KPD_KROW7_PIN;
	#endif


	#ifdef GPIO_KPD_KCOL0_PIN
		COL_REG[0] = GPIO_KPD_KCOL0_PIN;
	#endif

	#ifdef GPIO_KPD_KCOL1_PIN
		COL_REG[1] = GPIO_KPD_KCOL1_PIN;
	#endif

	#ifdef GPIO_KPD_KCOL2_PIN
		COL_REG[2] = GPIO_KPD_KCOL2_PIN;
	#endif

	#ifdef GPIO_KPD_KCOL3_PIN
		COL_REG[3] = GPIO_KPD_KCOL3_PIN;
	#endif

	#ifdef GPIO_KPD_KCOL4_PIN
		COL_REG[4] = GPIO_KPD_KCOL4_PIN;
	#endif

	#ifdef GPIO_KPD_KCOL5_PIN
		COL_REG[5] = GPIO_KPD_KCOL5_PIN;
	#endif

	#ifdef GPIO_KPD_KCOL6_PIN
		COL_REG[6] = GPIO_KPD_KCOL6_PIN;
	#endif

	#ifdef GPIO_KPD_KCOL7_PIN
		COL_REG[7] = GPIO_KPD_KCOL7_PIN;
	#endif
}

void mtk_kpd_gpio_set(void)
{
	unsigned int ROW_REG[8];
	unsigned int COL_REG[8];
	int i;

	kpd_print("Enter mtk_kpd_gpio_set! \n");
	mtk_kpd_gpios_get(ROW_REG, COL_REG);
	
	for(i = 0; i < 8; i++)
	{
		if (COL_REG[i] != 0)
		{
			/* KCOL: GPIO INPUT + PULL ENABLE + PULL UP */
			mt_set_gpio_mode(COL_REG[i], 1);
			mt_set_gpio_dir(COL_REG[i], 0);
			mt_set_gpio_pull_enable(COL_REG[i], 1);
			mt_set_gpio_pull_select(COL_REG[i], 1);
		}
		
		if(ROW_REG[i] != 0)
		{
			/* KROW: GPIO output + pull disable + pull down */
			mt_set_gpio_mode(ROW_REG[i], 1);
			mt_set_gpio_dir(ROW_REG[i], 1);
			mt_set_gpio_pull_enable(ROW_REG[i], 0);	
			mt_set_gpio_pull_select(ROW_REG[i], 0);
		}
	}
}
#endif

void kpd_ldvt_test_init(void){
#ifdef CONFIG_MTK_LDVT	
    	//set kpd enable and sel register
		mt_reg_sync_writew(0x0, KP_SEL);
		mt_reg_sync_writew(0x1, KP_EN);
		//set kpd GPIO to kpd mode
		mtk_kpd_gpio_set();
#endif
return;
}

/*******************************kpd factory mode auto test *************************************/
static void mtk_kpd_get_gpio_col(unsigned int COL_REG[])
{
	int i;
	for(i = 0; i< 8; i++)
	{
		COL_REG[i] = 0;
	}
	kpd_print("Enter mtk_kpd_get_gpio_col! \n");
	
	#ifdef GPIO_KPD_KCOL0_PIN
		kpd_print("checking GPIO_KPD_KCOL0_PIN! \n");
		COL_REG[0] = GPIO_KPD_KCOL0_PIN;
	#endif

	#ifdef GPIO_KPD_KCOL1_PIN
		kpd_print("checking GPIO_KPD_KCOL1_PIN! \n");
		COL_REG[1] = GPIO_KPD_KCOL1_PIN;
	#endif

	#ifdef GPIO_KPD_KCOL2_PIN
		kpd_print("checking GPIO_KPD_KCOL2_PIN! \n");
		COL_REG[2] = GPIO_KPD_KCOL2_PIN;
	#endif

	#ifdef GPIO_KPD_KCOL3_PIN
		kpd_print("checking GPIO_KPD_KCOL3_PIN! \n");
		COL_REG[3] = GPIO_KPD_KCOL3_PIN;
	#endif

	#ifdef GPIO_KPD_KCOL4_PIN
		kpd_print("checking GPIO_KPD_KCOL4_PIN! \n");
		COL_REG[4] = GPIO_KPD_KCOL4_PIN;
	#endif

	#ifdef GPIO_KPD_KCOL5_PIN
		kpd_print("checking GPIO_KPD_KCOL5_PIN! \n");
		COL_REG[5] = GPIO_KPD_KCOL5_PIN;
	#endif

	#ifdef GPIO_KPD_KCOL6_PIN
		kpd_print("checking GPIO_KPD_KCOL6_PIN! \n");
		COL_REG[6] = GPIO_KPD_KCOL6_PIN;
	#endif

	#ifdef GPIO_KPD_KCOL7_PIN
		kpd_print("checking GPIO_KPD_KCOL7_PIN! \n");
		COL_REG[7] = GPIO_KPD_KCOL7_PIN;
	#endif
}

void kpd_get_keymap_state(u16 state[])
{
	state[0] = *(volatile u16 *)KP_MEM1;
	state[1] = *(volatile u16 *)KP_MEM2;
	state[2] = *(volatile u16 *)KP_MEM3;
	state[3] = *(volatile u16 *)KP_MEM4;
	state[4] = *(volatile u16 *)KP_MEM5;
	printk(KPD_SAY "register = %x %x %x %x %x\n",state[0], state[1], state[2], state[3], state[4]);
	
}

static void kpd_factory_mode_handler(void)
{
	int i, j;
	bool pressed;
	u16 new_state[KPD_NUM_MEMS], change, mask;
	u16 hw_keycode, linux_keycode;

	kpd_keymap_state[0] = 0xffff;
	kpd_keymap_state[1] = 0xffff;
	kpd_keymap_state[2] = 0xffff;
	kpd_keymap_state[3] = 0xffff;
	kpd_keymap_state[4] = 0x00ff;

	kpd_get_keymap_state(new_state);

	for (i = 0; i < KPD_NUM_MEMS; i++) {
		change = new_state[i] ^ kpd_keymap_state[i];
		if (!change)
			continue;

		for (j = 0; j < 16; j++) {
			mask = 1U << j;
			if (!(change & mask))
				continue;

			hw_keycode = (i << 4) + j;
			/* bit is 1: not pressed, 0: pressed */
			pressed = !(new_state[i] & mask);
			if (kpd_show_hw_keycode) {
				printk(KPD_SAY "(%s) factory_mode HW keycode = %u\n",
				       pressed ? "pressed" : "released",
				       hw_keycode);
			}
			BUG_ON(hw_keycode >= KPD_NUM_KEYS);
			linux_keycode = kpd_keymap[hw_keycode];			
			if (unlikely(linux_keycode == 0)) {
				kpd_print("Linux keycode = 0\n");
				continue;
			}		
			input_report_key(kpd_input_dev, linux_keycode, pressed);
			input_sync(kpd_input_dev);
			kpd_print("factory_mode report Linux keycode = %u\n", linux_keycode);
		}
	}
	
	memcpy(kpd_keymap_state, new_state, sizeof(new_state));
	kpd_print("save new keymap state\n");
}

/********************************************************************/
void kpd_auto_test_for_factorymode(void)
{
	unsigned int COL_REG[8];
	int i;
	int time = 500;
	kpd_print("Enter kpd_auto_test_for_factorymode! \n");

	mdelay(1000);

	kpd_factory_mode_handler();
	kpd_print("begin kpd_auto_test_for_factorymode! \n");
	if(mt6331_upmu_get_pwrkey_deb()==1){
	kpd_print("power key release\n");
	//kpd_pwrkey_pmic_handler(1);
	//mdelay(time);
	//kpd_pwrkey_pmic_handler(0);}
	}else{
	kpd_print("power key press\n");
	kpd_pwrkey_pmic_handler(1);
	//mdelay(time);
	//kpd_pwrkey_pmic_handler(0);
	}
	
#ifdef KPD_PMIC_RSTKEY_MAP
	if(mt6331_upmu_get_homekey_deb()==1){
	//kpd_print("home key release\n");
	//kpd_pmic_rstkey_handler(1);
	//mdelay(time);
	//kpd_pmic_rstkey_handler(0);
	}else{
	kpd_print("home key press\n");
	kpd_pmic_rstkey_handler(1);
	//mdelay(time);
	//kpd_pmic_rstkey_handler(0);
	}
#endif

	return;
}
/********************************************************************/
void long_press_reboot_function_setting(void)
{
#ifndef EVB_PLATFORM
	if(kpd_enable_lprst&& get_boot_mode() == NORMAL_BOOT) {
		kpd_print("Normal Boot long press reboot selection\n");
#ifdef KPD_PMIC_LPRST_TD
		kpd_print("Enable normal mode LPRST\n");
	#ifdef ONEKEY_REBOOT_NORMAL_MODE
		mt6331_upmu_set_rg_pwrkey_rst_en(0x01);
		mt6331_upmu_set_rg_homekey_rst_en(0x00);
		mt6331_upmu_set_rg_pwrkey_rst_td(KPD_PMIC_LPRST_TD);
	#endif

	#ifdef TWOKEY_REBOOT_NORMAL_MODE
		mt6331_upmu_set_rg_pwrkey_rst_en(0x01);
		mt6331_upmu_set_rg_homekey_rst_en(0x01);
		mt6331_upmu_set_rg_pwrkey_rst_td(KPD_PMIC_LPRST_TD);
	#endif
#else
		kpd_print("disable normal mode LPRST\n");
		mt6331_upmu_set_rg_pwrkey_rst_en(0x00);
		mt6331_upmu_set_rg_homekey_rst_en(0x00);
#endif
	} 
	else {
		kpd_print("Other Boot Mode long press reboot selection\n");
	#ifdef KPD_PMIC_LPRST_TD
		kpd_print("Enable other mode LPRST\n");
	#ifdef ONEKEY_REBOOT_OTHER_MODE
		mt6331_upmu_set_rg_pwrkey_rst_en(0x01);
		mt6331_upmu_set_rg_homekey_rst_en(0x00);
		mt6331_upmu_set_rg_pwrkey_rst_td(KPD_PMIC_LPRST_TD);
	#endif

	#ifdef TWOKEY_REBOOT_OTHER_MODE
		mt6331_upmu_set_rg_pwrkey_rst_en(0x01);
		mt6331_upmu_set_rg_homekey_rst_en(0x01);
		mt6331_upmu_set_rg_pwrkey_rst_td(KPD_PMIC_LPRST_TD);
	#endif
#else
		kpd_print("disable other mode LPRST\n");
		mt6331_upmu_set_rg_pwrkey_rst_en(0x00);
		mt6331_upmu_set_rg_homekey_rst_en(0x00);
#endif
	}
#else
		mt6331_upmu_set_rg_pwrkey_rst_en(0x00);
		mt6331_upmu_set_rg_homekey_rst_en(0x00);
#endif
}
/********************************************************************/
void kpd_wakeup_src_setting(int enable)
{
#ifndef EVB_PLATFORM
	if(enable == 1){
		kpd_print("enable kpd work!\n");
		enable_kpd(1);
	}else{
		kpd_print("disable kpd work!\n");
		enable_kpd(0);
	}
#endif
}

/********************************************************************/
void kpd_init_keymap(u16 keymap[])
{
	int i = 0;
	for(i = 0;i < KPD_NUM_KEYS;i++){
	keymap[i] = kpd_keymap[i];
	}
}

void kpd_init_keymap_state(u16 keymap_state[])
{
	int i = 0;
	for(i = 0;i < KPD_NUM_MEMS;i++){
	keymap_state[i] = kpd_keymap_state[i];
	}
	printk(KPD_SAY "init_keymap_state done: %x %x %x %x %x!\n",keymap_state[0], keymap_state[1], keymap_state[2], keymap_state[3], keymap_state[4]);
}
/********************************************************************/

void kpd_set_debounce(u16 val)
{
	mt_reg_sync_writew((u16)(val & KPD_DEBOUNCE_MASK), KP_DEBOUNCE);
}

/********************************************************************/
void kpd_pmic_rstkey_hal(unsigned long pressed){
#ifdef KPD_PMIC_RSTKEY_MAP
	if(!kpd_sb_enable){
		input_report_key(kpd_input_dev, KPD_PMIC_RSTKEY_MAP, pressed);
		input_sync(kpd_input_dev);
		if (kpd_show_hw_keycode) {
			printk(KPD_SAY "(%s) HW keycode =%d using PMIC\n",
			       pressed ? "pressed" : "released", KPD_PMIC_RSTKEY_MAP);
		}
	}
#endif
}

void kpd_pmic_pwrkey_hal(unsigned long pressed){
#if KPD_PWRKEY_USE_PMIC
	if(!kpd_sb_enable){
		input_report_key(kpd_input_dev, KPD_PWRKEY_MAP, pressed);
		input_sync(kpd_input_dev);
		if (kpd_show_hw_keycode) {
			printk(KPD_SAY "(%s) HW keycode =%d using PMIC\n",
			       pressed ? "pressed" : "released", KPD_PWRKEY_MAP);
		}
		#ifdef CONFIG_MTK_AEE_POWERKEY_HANG_DETECT
		aee_powerkey_notify_press(pressed);
		#endif
		}
#endif
}
/***********************************************************************/
void kpd_pwrkey_handler_hal(unsigned long data){
#if KPD_PWRKEY_USE_EINT
	bool pressed;
	u8 old_state = kpd_pwrkey_state;

	kpd_pwrkey_state = !kpd_pwrkey_state;
	pressed = (kpd_pwrkey_state == !!KPD_PWRKEY_POLARITY);
	if (kpd_show_hw_keycode) {
		printk(KPD_SAY "(%s) HW keycode = using EINT\n",
		       pressed ? "pressed" : "released");
	}
	kpd_backlight_handler(pressed, KPD_PWRKEY_MAP);
	input_report_key(kpd_input_dev, KPD_PWRKEY_MAP, pressed);
	input_sync(kpd_input_dev);
	kpd_print("report Linux keycode = %u\n", KPD_PWRKEY_MAP);

	/* for detecting the return to old_state */
	mt_eint_set_polarity(KPD_PWRKEY_EINT, old_state);
	mt_eint_unmask(KPD_PWRKEY_EINT);
#endif
}
/************************************************************************/
#ifdef HTC_KPD_USE_EINT

static void kpd_key_handler_hal(unsigned long data)
{
	kpd_print("Enter kpd_pwrkey_handler_hal\n");
	kpd_backlight_handler(kpd_eint_state, KPD_KEY_MAP);
	input_report_key(kpd_input_dev, KPD_KEY_MAP, kpd_eint_state);
	input_sync(kpd_input_dev);
	kpd_print("(%s) HW keycode = using EINT,report Linux keycode = %u\n", kpd_eint_state ? "pressed" : "released",KPD_KEY_MAP);
	enable_irq(kpd_key_irq);

}

static void kpd_key_handler(unsigned long data)
{
	kpd_key_handler_hal(data);
}

static irqreturn_t kpd_key_eint_handler(int irq,void *data)
{
	printk(KPD_SAY"Enter kpd_pwrkey_eint_handler\n");

	disable_irq_nosync(kpd_key_irq);
	
	if(kpd_eint_state == KPD_RELEASED){ 
		if (CUST_EINT_KPD_PWRKEY_TYPE == CUST_EINTF_TRIGGER_HIGH){
			irq_set_irq_type(kpd_key_irq,IRQ_TYPE_LEVEL_LOW);
		}else{
			irq_set_irq_type(kpd_key_irq,IRQ_TYPE_LEVEL_HIGH);
		}
		kpd_eint_state = KPD_PRESSED;
	}else{
		if (CUST_EINT_KPD_PWRKEY_TYPE == CUST_EINTF_TRIGGER_HIGH){
			irq_set_irq_type(kpd_key_irq,IRQ_TYPE_LEVEL_HIGH);
		}else{
			irq_set_irq_type(kpd_key_irq,IRQ_TYPE_LEVEL_LOW);
		}
		kpd_eint_state = KPD_RELEASED;
	}
	tasklet_schedule(&kpd_key_tasklet);
	return IRQ_HANDLED;
}
#endif

/***********************************************************************/
void mt_eint_register(void){
#if KPD_PWRKEY_USE_EINT
	mt_eint_set_sens(KPD_PWRKEY_EINT, KPD_PWRKEY_SENSITIVE);
	mt_eint_set_hw_debounce(KPD_PWRKEY_EINT, KPD_PWRKEY_DEBOUNCE);
	mt_eint_registration(KPD_PWRKEY_EINT, true, KPD_PWRKEY_POLARITY,
	                         kpd_pwrkey_eint_handler, false);
#endif
#ifdef HTC_KPD_USE_EINT
	int ret;
	u32 ints[2]={0,0};
	struct device_node *node;

	node = of_find_compatible_node(NULL,NULL,"mediatek, KPD_PWRKEY-eint");
	if(node) {
		of_property_read_u32_array(node,"debounce",ints,ARRAY_SIZE(ints));
		gpiopin = ints[0];
		kpddebounce = ints[1];
		mt_gpio_set_debounce(gpiopin, kpddebounce);
		kpd_key_irq = irq_of_parse_and_map(node,0);
		ret = request_irq(kpd_key_irq,kpd_key_eint_handler,IRQF_TRIGGER_NONE,"KPD_PWRKEY-eint",NULL);
		if(ret>0){
			printk(KPD_SAY" EINT IRQ LINE£NNOT AVAILABLE\n");
		}else{
			printk(KPD_SAY" kpd_pwrkey_eint_handler request success\n");
		}
	}
	else {
		printk(KPD_SAY" can't find compatible node\n");
	}
#endif
}
/************************************************************************/

