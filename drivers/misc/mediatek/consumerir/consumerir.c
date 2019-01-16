#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/time.h>

#include <linux/kobject.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>

#include <mach/mt_pm_ldo.h>
#include <mach/mt_gpio.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <mach/mt_pwm.h>
#include <mach/mt_pwm_hal.h>
#include <linux/delay.h>
#include "cust_gpio_usage.h"

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#define CONSUMERIR_TAG		"[consumerir] "
#define CONSUMERIR_DEBUG
#if defined(CONSUMERIR_DEBUG)
#define CONSUMERIR_FUN(f)				printk(KERN_ERR CONSUMERIR_TAG"%s\n", __FUNCTION__)
#define CONSUMERIR_ERR(fmt, args...)		printk(KERN_ERR CONSUMERIR_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#else
#define CONSUMERIR_FUN(f)
#define CONSUMERIR_ERR(fmt, args...)
#endif

struct consumerir 
{
	struct platform_device *plat_dev;
	unsigned int carrier_freq;
};

struct consumerir consumerir_dev;

#define CONSUMERIR_IOC_SET_CARRIER_FREQ _IOW('R', 0, unsigned int)
#define CONSUMERIR_IOC_SET_IRTX_LED_EN    _IOW('R', 10, unsigned int)

static int consumerir_power_flag = 0;
static int g_Consumerir_Opened = 0;

static spinlock_t Consumerir_SpinLock;
static spinlock_t g_Consumerir_Open_SpinLock;

#define consumerir_driver_name "consumerir"

extern void mt_pwm_26M_clk_enable_hal(U32 enable);
extern S32 mt_set_intr_enable(U32 pwm_intr_enable_bit);
extern S32 mt_set_intr_ack ( U32 pwm_intr_ack_bit );
extern void mt_pwm_dump_regs_hal(void);
//add by zhoulingyun start for cx861 ir
int g_vibrator_station_for_ir=0;  
void vibrator_ir_lock_for_ir(void)
{
 
         int temp=0;
        return ;
        while(g_vibrator_station_for_ir==1)
          {
            
            mdelay(10);
            temp++;
            if(temp>1000)
              break;
             
          }
          g_vibrator_station_for_ir=5;     
}
void vibrator_ir_lock_for_vib(void)
{
 
         int temp=0;
         return;
        while(g_vibrator_station_for_ir==5)
          {
            
            mdelay(10);
            temp++;
            if(temp>1000)
              break;
             
          }
          g_vibrator_station_for_ir=1;     
}
//add by zhoulingyun end for cx861 ir
#define  pwm_clock_freq    1625000       //26m/16=162500
struct pwm_spec_config consumerir_pwm_config = 
{
#if 1	
	.pwm_no = 0,
	.mode = PWM_MODE_MEMORY,
	.clk_div = CLK_DIV16,	//div1:500K ; CLK_DIV64: 7.81k; CLK_DIV16: 33k
	.clk_src = PWM_CLK_NEW_MODE_BLOCK,
	.pmic_pad = 0,
	.PWM_MODE_MEMORY_REGS.IDLE_VALUE = IDLE_FALSE,
	.PWM_MODE_MEMORY_REGS.GUARD_VALUE = GUARD_FALSE,
	.PWM_MODE_MEMORY_REGS.STOP_BITPOS_VALUE = 31,
	.PWM_MODE_MEMORY_REGS.HDURATION = 20, //25  1 microseconds, assume clock source is 26M
	.PWM_MODE_MEMORY_REGS.LDURATION = 20,
	.PWM_MODE_MEMORY_REGS.GDURATION = 0,
	.PWM_MODE_MEMORY_REGS.WAVE_NUM = 1,
#else
	.pwm_no = 0,	//PWM1: 0
	.mode = PWM_MODE_FIFO,
	.clk_div = CLK_DIV32,
	.clk_src = PWM_CLK_NEW_MODE_BLOCK,
	.pmic_pad = false,
	.PWM_MODE_FIFO_REGS.IDLE_VALUE = 0,
	.PWM_MODE_FIFO_REGS.GUARD_VALUE = 0,
	.PWM_MODE_FIFO_REGS.GDURATION = 0,
	.PWM_MODE_FIFO_REGS.STOP_BITPOS_VALUE = 7,
	.PWM_MODE_FIFO_REGS.HDURATION = 10,
	.PWM_MODE_FIFO_REGS.LDURATION = 9,
	.PWM_MODE_FIFO_REGS.WAVE_NUM = 0,	//num
	.PWM_MODE_FIFO_REGS.SEND_DATA0 = 0xAA,
//	.PWM_MODE_FIFO_REGS.SEND_DATA1 = 0xAAAAAAAA,
#endif
};

static int one_clock_time=250;
void set_consumerir_pwm(unsigned int carrier_freq)
{
   int temp0=0;
   int high=0;
    int low=0;
    int  freq=0;
    printk("%s:zlya carrier_freq =%d\n",__func__,carrier_freq);
  //pwm freq= clock source/div/((High_duration+1)+(low_duration+1))
  //excepl   38700=(26*1000*1000)/16/((High_duration+1)+(low_duration+1))
  if(carrier_freq<=10000)
	  temp0=pwm_clock_freq/38000;
  else if(carrier_freq>=100000)
      temp0=pwm_clock_freq/38000;
  else 
     temp0=(pwm_clock_freq/carrier_freq);
   if(temp0 % 2 == 0)
    {
     high=temp0/2;
     low=high;
    }
    else
     {
     high=temp0/2;
     low=high+1;
    }
//((High_duration+1)+(low_duration+1))
if(high>2)
  high=high-1;

if(low>2)
  low=low-1;

freq=pwm_clock_freq/(high+low+2);
printk("zlya high =%d,low = %d,freq = %d\n",high,low,freq);
one_clock_time=1000000 * 10/freq;
printk("%s first one_clock_time = %d\n",__func__,one_clock_time);
//if (carrier_freq == 38000)
  //  one_clock_time = 1000000/carrier_freq;
//printk("%s second one_clock_time = %d\n",__func__,one_clock_time);
consumerir_pwm_config.PWM_MODE_MEMORY_REGS.HDURATION=high;
consumerir_pwm_config.PWM_MODE_MEMORY_REGS.LDURATION=low;
CONSUMERIR_ERR("zlya high=%d,low=%d,freq=%d,one_clock_time=%d\n",high,low,freq,one_clock_time);
//consumerir_dev.carrier_freq
}


static int dev_char_open(struct inode *inode, struct file *file)
{
	CONSUMERIR_ERR("start\n");
	if(g_Consumerir_Opened)
	{
		CONSUMERIR_ERR("The device is opened \n");
		return -EBUSY;
	}

	spin_lock(&g_Consumerir_Open_SpinLock);
	g_Consumerir_Opened = 1;
	spin_unlock(&g_Consumerir_Open_SpinLock);

	CONSUMERIR_ERR("end\n");
	
	return 0;
}

static int dev_char_release(struct inode *inode, struct file *file)
{
	CONSUMERIR_ERR("Start \n");

	if (g_Consumerir_Opened)
	{
		CONSUMERIR_ERR("Free \n");

		spin_lock(&g_Consumerir_Open_SpinLock);
		g_Consumerir_Opened = 0;
		spin_unlock(&g_Consumerir_Open_SpinLock);
	}

	CONSUMERIR_ERR("End \n");

	return 0;
}

static ssize_t dev_char_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	CONSUMERIR_ERR("dev_char_read\n");
	return 0;
}

static long dev_char_ioctl( struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	unsigned int para = 0, en = 0;
	unsigned long mode = 0, dir = 0, outp = 0;

	CONSUMERIR_ERR("cmd=%d\n", cmd);
	switch(cmd) 
	{
		case CONSUMERIR_IOC_SET_CARRIER_FREQ:
			if(copy_from_user(&consumerir_dev.carrier_freq, (void __user *)arg, sizeof(unsigned int))) 
			{
				CONSUMERIR_ERR("IRTX_IOC_SET_CARRIER_FREQ: copy_from_user fail!\n");
				ret = -EFAULT;
			} 
			else 
			{
				CONSUMERIR_ERR(" IRTX_IOC_SET_CARRIER_FREQ: %d\n", consumerir_dev.carrier_freq);
				if(!consumerir_dev.carrier_freq) 
				{
					ret = -EINVAL;
					consumerir_dev.carrier_freq = 38000;
				}
			}
			break;

		case CONSUMERIR_IOC_SET_IRTX_LED_EN:
			if(copy_from_user(&para, (void __user *)arg, sizeof(unsigned int))) 
			{
				CONSUMERIR_ERR(" IRTX_IOC_SET_IRTX_LED_EN: copy_from_user fail!\n");
				ret = -EFAULT;
			} 
			else 
			{
				en = (para & 0xF);
				CONSUMERIR_ERR(" IRTX_IOC_SET_IRTX_LED_EN: 0x%x, en:%ul\n", para, en);
				if (en) 
				{
					mode = GPIO_MODE_03;
					dir = GPIO_DIR_OUT;
					outp = GPIO_OUT_ZERO;    // Low means enable LED
				} 
				else
				{
					mode = GPIO_MODE_00;
					dir = GPIO_DIR_OUT;
					outp = GPIO_OUT_ONE;  // High means disable LED
				}

				mt_set_gpio_mode(GPIO_CONSUMERIR_LED_PIN, mode);
				mt_set_gpio_dir(GPIO_CONSUMERIR_LED_PIN, dir);
				mt_set_gpio_out(GPIO_CONSUMERIR_LED_PIN, outp);
			}
			break;

		default:
			CONSUMERIR_ERR(" unknown ioctl cmd 0x%x\n", cmd);
			ret = -ENOTTY;
			break;
	}
	
	return ret;
}

#define USE_INT_ARRAY_FORM_HAL 1

//#define carrier_freq  38600
//#define one_clock_time  25   //us    1000000/38600=25
static ssize_t dev_char_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	dma_addr_t wave_phy_time;
        dma_addr_t wave_phy_out;
	
#if 1 
	unsigned int *wave_vir_time,*wave_vir_time_backup,*wave_vir_out;
#else
	void *wave_vir;
#endif

	int ret, num,num_clock;
	int buf_size = 0;
	int i,j;
	int total_time = 0;
        unsigned int *wave_out,*wave_out_backup;
        int rel_buffer_num=0;
        int bit_ptr=0;
        int flag=1; //must be 1


	CONSUMERIR_ERR("size=%ld\n", sizeof(long int));
	buf_size = (count + (sizeof( int) - 1))  / (sizeof( int)); 
	CONSUMERIR_ERR("zly2 consumerir write count=%d, buf_size=%d\n", (unsigned int)count, buf_size);
	wave_vir_time = dma_alloc_coherent(&consumerir_dev.plat_dev->dev, count, &wave_phy_time, GFP_KERNEL);
        wave_vir_time_backup=wave_vir_time;
	if(!wave_vir_time)
	{
		CONSUMERIR_ERR(" alloc memory fail\n");
		return -ENOMEM;
	}		

	ret = copy_from_user(wave_vir_time, buf, count);
	if(ret) 
	{
		CONSUMERIR_ERR(" write, copy from user fail %d\n", ret);
		goto exit;
	}

#if 0//USE_INT_ARRAY_FORM_HAL
	for(i = 0; i < buf_size; i++)
	{
		CONSUMERIR_ERR("zly3 i=%d: 0x%04x\n", i, wave_vir_time[i]);
	}
#endif

       for (i = 0; i < buf_size; i++)
	{
		total_time += wave_vir_time[i];
	}
  
        set_consumerir_pwm(consumerir_dev.carrier_freq);

        num_clock=total_time * 10/one_clock_time;
        num=(num_clock/16)+1;
        CONSUMERIR_ERR("zly4 total_time=%d: num_clock=%d,num=%d\n", total_time, num_clock,num);
        num=num+(buf_size/16)+10;
#if 0
	wave_out = kmalloc(num * sizeof(int), GFP_KERNEL);
        *wave_out_backup=*wave_out;
if (!wave_out) {
		CONSUMERIR_ERR("list_sort_test: error: cannot allocate memory\n");
		return ret;
	}
#endif
        wave_vir_out = dma_alloc_coherent(&consumerir_dev.plat_dev->dev, num*sizeof(unsigned int), &wave_phy_out, GFP_KERNEL);
if(!wave_vir_out)
	{
		CONSUMERIR_ERR(" alloc memory fail\n");
		return -ENOMEM;
	}
       for(i = 0; i < num; i++)
	{
                *(wave_vir_out+i)=0x00000000;
	}

for (i = 0; i < buf_size; i++) 
	{
         int one_num_clock=0; 
	// one_num_clock = wave_vir_time[i]/25;
	// one_num_clock = (wave_vir_time[i]*1625)/(1000*(consumerir_pwm_config.PWM_MODE_MEMORY_REGS.HDURATION+consumerir_pwm_config.PWM_MODE_MEMORY_REGS.LDURATION));
        #if 0
	 if(consumerir_dev.carrier_freq<=10000)
	   one_num_clock = (wave_vir_time[i]*38000)/1000000;
     else if(consumerir_dev.carrier_freq>=100000)
	   one_num_clock = (wave_vir_time[i]*38000)/1000000;
     else
	 one_num_clock = (wave_vir_time[i]*consumerir_dev.carrier_freq)/1000000;
       #endif
	 one_num_clock = wave_vir_time[i] * 10/one_clock_time;
         if((wave_vir_time[i]* 10-(one_num_clock*one_clock_time))>(one_clock_time/2))
         {
            one_num_clock = one_num_clock+1;
           // CONSUMERIR_ERR("zly51 i=%d: wave_vir_time=%d,one_num_clock=%d\n", i,wave_vir_time[i],one_num_clock);
          }
	 //CONSUMERIR_ERR("zly5 i=%d: wave_vir_time=%d,one_num_clock=%d\n", i,wave_vir_time[i],one_num_clock);
		for(j = 0; j < one_num_clock; j++)
		{
			if(flag==1)
			{
			   *(wave_vir_out+rel_buffer_num) = *(wave_vir_out+rel_buffer_num) | (0x01 << (2 * bit_ptr));
			}
			else
			{
			   //*(wave_vir_out+rel_buffer_num) = *(wave_vir_out+rel_buffer_num)  & (0x00 << (2 * bit_ptr)) ;   
			}
							
			bit_ptr++;
			if(bit_ptr == 16) 
			{
				bit_ptr = 0;
				rel_buffer_num++;
                                if(rel_buffer_num>=num)
                                 {
                                   CONSUMERIR_ERR("zly5_error rel_buffer_num>=num is error rel_buffer_num=%d,num=%d \n",rel_buffer_num,num);
                                   return ret;
                                  }
			}
		}
             
               if(flag==1)
                   flag=0;
                else
                   flag=1;
          

	}
if (bit_ptr > 0)
    rel_buffer_num++;

#if 0
CONSUMERIR_ERR("zly6 total_time=%d: rel_buffer_num=%d,num=%d\n", total_time,rel_buffer_num,num);
for(i = 0; i <= rel_buffer_num; i++)
	{
		CONSUMERIR_ERR("zly7 i=%d: 0x%04x\n", i, *(wave_vir_out+i));
	}
#endif
/////////////////////////////////////////////////////////////////////////


 //add by zhoulingyun start for cx861 ir
      vibrator_ir_lock_for_ir();
       //add by zhoulingyun end for cx861 ir
	if(0 == consumerir_power_flag)
	{
		hwPowerOn(MT6331_POWER_LDO_VMCH, VOL_3300, "consumerir");
		consumerir_power_flag = 1;
	}

	mt_set_intr_enable(0);
	mt_set_intr_enable(1);
	mt_pwm_26M_clk_enable_hal(1);

	consumerir_pwm_config.PWM_MODE_MEMORY_REGS.BUF0_BASE_ADDR = wave_phy_out;
	consumerir_pwm_config.PWM_MODE_MEMORY_REGS.BUF0_SIZE = rel_buffer_num;

	mt_set_gpio_mode(GPIO_CONSUMERIR_LED_PIN, GPIO_MODE_03);
	mt_set_gpio_dir(GPIO_CONSUMERIR_LED_PIN, GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_CONSUMERIR_LED_PIN, GPIO_OUT_ZERO);
	mt_set_intr_ack(0);
	mt_set_intr_ack(1);

	CONSUMERIR_ERR("before consumerir_pwm_config\n");
	ret = pwm_set_spec_config(&consumerir_pwm_config);
	
	CONSUMERIR_ERR(" pwm is triggered, %d\n", ret);

	CONSUMERIR_ERR(" consumerir_dev.carrier_freq=%d,h=%d,l=%d\n", consumerir_dev.carrier_freq,consumerir_pwm_config.PWM_MODE_MEMORY_REGS.HDURATION,consumerir_pwm_config.PWM_MODE_MEMORY_REGS.LDURATION);

	//total_time = 500; //count * 1000 * 1000 * 16/ consumerir_dev.carrier_freq;
	//CONSUMERIR_ERR("total_time=%d\n", total_time);
	//spin_lock(&Consumerir_SpinLock);
	mdelay((total_time*12)/10000); //total_time * 8 / 1000);
	//spin_unlock(&Consumerir_SpinLock);
	mt_pwm_disable(consumerir_pwm_config.pwm_no, consumerir_pwm_config.pmic_pad);
	ret = rel_buffer_num;
	
exit:
	CONSUMERIR_ERR(" done, clean up\n");
#if 1	
	dma_free_coherent(&consumerir_dev.plat_dev->dev, count, *wave_vir_time, wave_phy_time);
        dma_free_coherent(&consumerir_dev.plat_dev->dev, num*sizeof(unsigned int), *wave_vir_out, wave_phy_out);
	if(consumerir_power_flag == 1)
	{
		consumerir_power_flag = 0;
		hwPowerDown(MT6331_POWER_LDO_VMCH, "consumerir");
	}
	mt_set_gpio_mode(GPIO_CONSUMERIR_LED_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_CONSUMERIR_LED_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CONSUMERIR_LED_PIN, GPIO_OUT_ZERO);
#endif
        //add by zhoulingyun end for cx861 ir
       {
        g_vibrator_station_for_ir=0;
       }
       //add by zhoulingyun end for cx861 ir
	return ret;
}

#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))
static u64 consumerir_dma_mask = DMA_BIT_MASK((sizeof(unsigned long)<<3)); //  TODO: 3?

static struct file_operations char_dev_fops = {
    .owner = THIS_MODULE,
    .open = &dev_char_open,
    .release = &dev_char_release,
    .read = &dev_char_read,
    .write = &dev_char_write,
    .unlocked_ioctl = &dev_char_ioctl,
};

int time_to_wave_num(int time, int freq)
{

		int n = (time/1000/1000) * freq;
}
static ssize_t store_value(struct device_driver *ddri, const char *buf1, size_t count0)
{
	int buf[4] ; //=  {0xAAAA0000, 0x0000AAAA, 0xA0A0A0A0, 0xAAAAAAAA};
	size_t count = 4*4;
	dma_addr_t wave_phy;

	int *wave_vir;
	int *tmp ;

	int ret, num;
	int buf_size = 0;
	long i;
	int total_time = 0;
	int index = 0; 
	int index1 = 0; 
	int pattern[4];

	printk("%s: %s \n", __func__, buf1);

	sscanf(buf1, "%d", &index);
	sscanf(buf1, "%02x %02x %02x %02x",  &pattern[0], &pattern[1], &pattern[2], &pattern[3]);

	for( i = 0; i < 4; i++){
			printk("%s, pattern[%ld]=%08x \n", __func__, i, pattern[i]);
	}

	strict_strtoul(buf1, 10, &index1);
	printk("%s, index=%d, index1=%d, sizeof( unsigned long )=%ld \n", __func__,  index , index1, sizeof(unsigned long));

	if(index == 1 ) {
			buf[0] = 0xAA00000A; 
			buf[1] = 0x00000000; 
			buf[2] = 0xA0A0A0A0; 
			buf[3] = 0xAAAAAAAA;
			printk("%s, %d\n", __func__, __LINE__);
	}else if( index == 2 ) {
			buf[0] = 0x00000000; 
			buf[1] = 0x0000AAAA; 
			buf[2] = 0xA0A0A0A0; 
			buf[3] = 0xAAAAAAAA;
			printk("%s, %d\n", __func__, __LINE__);
	}else if ( index ==3 ) {
			buf[0] = 0xA0A0A0A0; 
			buf[1] = 0x0000AAAA; 
			buf[2] = 0xA0A0A0A0; 
			buf[3] = 0xAAAAAAAA;
			printk("%s, %d\n", __func__, __LINE__);
	} else {
			buf[0] = 0xAA00000A; 
			buf[1] = 0x00000000; 
			buf[2] = 0xA0A0A0A0; 
			buf[3] = 0xAAAAAAAA;
			printk("%s, %d\n", __func__, __LINE__);
	}
	
	CONSUMERIR_ERR("long int size=%ld\n", sizeof(long int));
	buf_size = (count * (sizeof(int) - 1))  / (sizeof(int)); 
	CONSUMERIR_ERR(" consumerir write count=%d, buf_size=%d\n", (unsigned int)count, buf_size);
	wave_vir = dma_alloc_coherent(&consumerir_dev.plat_dev->dev, count, &wave_phy, GFP_KERNEL);
	tmp = wave_vir;

	if(!wave_vir)
	{
		CONSUMERIR_ERR(" alloc memory fail\n");
		return -ENOMEM;
	}		

	for(i = 0; i < 4; i++)
	{
			*tmp = buf[i];
			tmp++;
	}

	for(i = 0; i < 4; i++)
	{
		CONSUMERIR_ERR("i=%ld: 0x%08x\n", i, wave_vir[i]);
	}

	if(1 ) {   // 0 == consumerir_power_flag) {
		printk("%s, %d \n", __func__, __LINE__);
		hwPowerOn(MT6331_POWER_LDO_VMCH, VOL_3300, "consumerir");
		consumerir_power_flag = 1;
	}

	mt_set_intr_enable(0);
	mt_set_intr_enable(1);
	mt_pwm_26M_clk_enable_hal(1);

	consumerir_pwm_config.PWM_MODE_MEMORY_REGS.BUF0_BASE_ADDR = wave_phy;
	consumerir_pwm_config.PWM_MODE_MEMORY_REGS.BUF0_SIZE = (count ? (count -1) : 0) ;

	mt_set_gpio_mode(GPIO_CONSUMERIR_LED_PIN, GPIO_MODE_03);
	mt_set_gpio_dir(GPIO_CONSUMERIR_LED_PIN, GPIO_DIR_OUT);

	mt_set_intr_ack(0);
	mt_set_intr_ack(1);

	CONSUMERIR_ERR("before consumerir_pwm_config\n");
	ret = pwm_set_spec_config(&consumerir_pwm_config);
	
	CONSUMERIR_ERR(" pwm is triggered, %d\n", ret);

	total_time = 2000; //count * 1000 * 1000 * 16/ consumerir_dev.carrier_freq;
	CONSUMERIR_ERR("total_time=%d\n", total_time);
	spin_lock(&Consumerir_SpinLock);
	mdelay(2000);  //total_time * 8 / 1000);
	mdelay(100);
	spin_unlock(&Consumerir_SpinLock);
	mt_pwm_disable(consumerir_pwm_config.pwm_no, consumerir_pwm_config.pmic_pad);
	ret = count;
	
exit:
	CONSUMERIR_ERR(" done, clean up\n");
#if 1	
	dma_free_coherent(&consumerir_dev.plat_dev->dev, count, wave_vir, wave_phy);
	if(consumerir_power_flag == 1)
	{
		consumerir_power_flag = 0;
		hwPowerDown(MT6331_POWER_LDO_VMCH, "consumerir");
	}
	mt_set_gpio_mode(GPIO_CONSUMERIR_LED_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_CONSUMERIR_LED_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CONSUMERIR_LED_PIN, GPIO_OUT_ZERO);
#endif
	return ret;

}
static ssize_t devinfo_show(struct device *dev1, struct device_attribute *attr, char *buf1)
{
	return 1;
}
static DEVICE_ATTR(devinfo, S_IRUGO  | S_IWUSR, devinfo_show, store_value);


extern void dump_mmc();

static int consumerir_probe(struct platform_device *plat_dev)
{
	struct cdev *c_dev;
	dev_t dev_t_consumerir;
	struct device *dev = NULL;
	static void *dev_class;
	int ret = 0;
	//int error = 0;

	CONSUMERIR_ERR("!!!!!!\n");

	consumerir_power_flag = 0;
	spin_lock_init(&Consumerir_SpinLock);
	spin_lock_init(&g_Consumerir_Open_SpinLock);
	
//#ifdef CONFIG_OF
	mt_set_gpio_mode(GPIO_CONSUMERIR_LED_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_CONSUMERIR_LED_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CONSUMERIR_LED_PIN, GPIO_OUT_ZERO);
//#endif

	consumerir_dev.plat_dev = plat_dev;
	consumerir_dev.plat_dev->dev.dma_mask = &consumerir_dma_mask;
	consumerir_dev.plat_dev->dev.coherent_dma_mask = consumerir_dma_mask;
	consumerir_dev.carrier_freq = 38000; //  default

	 //Allocate char driver no.
	if(alloc_chrdev_region(&dev_t_consumerir, 0, 1, consumerir_driver_name))
	{
		CONSUMERIR_ERR("Allocate device no failed\n");
		return -EAGAIN;
	}

	//Allocate driver
	c_dev = cdev_alloc();
	if(NULL == c_dev)
	{
		unregister_chrdev_region(dev_t_consumerir, 1);
		CONSUMERIR_ERR("Allocate mem for kobject failed\n");
		return -ENOMEM;
	}

	cdev_init(c_dev, &char_dev_fops);
	c_dev->owner = THIS_MODULE;
	
	ret = cdev_add(c_dev, dev_t_consumerir, 1);
	if(ret) 
	{
		CONSUMERIR_ERR(" cdev_add fail ret=%d\n", ret);
		unregister_chrdev_region(dev_t_consumerir, 1);
		return -ENOMEM;
	}
	
	dev_class = class_create(THIS_MODULE, consumerir_driver_name);
	if (IS_ERR(dev_class)) 
	{
		ret = PTR_ERR(dev_class);
		CONSUMERIR_ERR("Unable to create class, err = %d\n", ret);
		return -1;
	}
	
	dev = device_create(dev_class, NULL, dev_t_consumerir, NULL, consumerir_driver_name);

	device_create_file(dev, &dev_attr_devinfo);
	if(IS_ERR(dev)) 
	{
		ret = PTR_ERR(dev);
		CONSUMERIR_ERR(" device_create fail ret=%d\n", ret);
		return -EIO;
	}

	CONSUMERIR_ERR("end\n");
	dump_mmc();
	return 0;
}

static struct platform_driver mt_consumerir_driver =
{
	.driver = 
	{
		.name = "consumerir",
	},
	.probe = consumerir_probe,
};

static struct platform_device mt_consumerir_dev =
{
	.name = "consumerir",
	.id = -1,
};

static int __init consumerir_init(void)
{
	int ret = 0;
	CONSUMERIR_ERR("consumerir_init !!!!!!\n");

	if(platform_device_register(&mt_consumerir_dev) != 0)
	{
		CONSUMERIR_ERR("consumerir platform dev register fail\n");
		return -ENODEV;
	}
	
	CONSUMERIR_ERR("consumerir platform driver\n");

	ret = platform_driver_register(&mt_consumerir_driver);
	if (ret) 
	{
		CONSUMERIR_ERR("consumerir platform driver register fail %d\n", ret);
		return -ENODEV;
	}

	return 0;
}

static void __exit  consumerir_exit(void)
{
	CONSUMERIR_FUN();	
	platform_driver_unregister(&mt_consumerir_driver);
}

module_init(consumerir_init);
module_exit(consumerir_exit);
MODULE_AUTHOR("lizhiye <zhiye.li@longcheer.net");
MODULE_DESCRIPTION("Consumer IR transmitter driver v1.0");
MODULE_LICENSE("GPL");
