#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <mach/mt_pm_ldo.h>
#include <asm/memory.h>
#include <mt_i2c.h>

static char data_buffer[256 * 4];

static ssize_t show_config(struct device *dev, struct device_attribute *attr, char *buff)
{
  int len = strlen(data_buffer);
  memcpy(buff,data_buffer,len);
  printk("Return Value:%s\n",data_buffer);
  return len;
}

static int pows(int x, int y)
{
  int result = 1;
  while (y--) result *=x;
  return result;
}

int string2hex(const char * buffer, int cnt){
  int  c = 0;
  char t = 0;
  int count = cnt;
  while (count--){
    t = *(buffer + cnt - count - 1);
    if ( t >= 'A' && t <= 'F') {
      c += ((t - 'A') + 10 ) * pows(16,count);
    } else if (t >= '0' && t <= '9'){
      c += (t - '0') * pows(16,count);
    } else {
      c = -1;
    }
  }
  return c;
}

char * get_hexbuffer(char *data_buffer, char *hex_buffer)
{
  char * ptr = data_buffer;
  int index = 0;
  while (*ptr && *++ptr) {
    *(hex_buffer + index++) = string2hex(ptr-1, 2);
    ptr++;
  }
  *(hex_buffer + index) = 0;
  return hex_buffer;
}

int i2c_trans_data(int bus_id, int address, char *buf, int count, unsigned int ext_flag,int timing,int op)
{
  int ret;

  struct i2c_msg msg;
  struct i2c_adapter *adap;
  adap = i2c_get_adapter(bus_id);
  if (!adap) return -1;

  msg.addr = address;
  if(0 ==op){
  	msg.flags = 0;
  }else if(1==op){
    msg.flags =I2C_M_RD;
  }else if(2==op){
    msg.flags =0;
  }
  //msg.flags = ((ext_flag & 0x80000000)?I2C_M_RD:0);
  msg.timing = timing;
  msg.len = count;
  msg.buf = (char *)buf;
  msg.ext_flag = ext_flag;
//  msg.ext_flag = (ext_flag & 0x7FFF00FF);
//  msg.addr |= ext_flag & 0x0000FF00;
  ret = i2c_transfer(adap, &msg, 1);

  /* If everything went ok (i.e. 1 msg transmitted), return #bytes
     transmitted, else error code. */
  i2c_put_adapter(adap);
  return (ret == 1) ? count : ret;
}
int mt_i2c_test(int id, int addr)
{
  int ret = 0;
  unsigned long flag;
//  unsigned char buffer[]={0x55};
  if(id >3)
    flag = I2C_DIRECTION_FLAG;

 // flag |= 0x80000000;
  //ret = i2c_trans_data(id, addr,buffer,1,flag,200);
  return ret;
}
EXPORT_SYMBOL(mt_i2c_test);

//extern mt_i2c ;
static int i2c_test_reg(int bus_id,int val)
{
  int ret=0;
  struct i2c_adapter *adap;
  mt_i2c *i2c;
  adap = i2c_get_adapter(bus_id);
  if (!adap) return -1;
  i2c = container_of(adap,mt_i2c,adap);
  printk("I2C%d base address 0x%p\n",bus_id,i2c->base);
  //write i2c writable register with 0
  i2c_writel(i2c,OFFSET_SLAVE_ADDR,val);
  i2c_writel(i2c,OFFSET_INTR_MASK,val);
  i2c_writel(i2c,OFFSET_INTR_STAT,val);
  i2c_writel(i2c,OFFSET_CONTROL,val);
  i2c_writel(i2c,OFFSET_TRANSFER_LEN,val);
  i2c_writel(i2c,OFFSET_TRANSAC_LEN,val);
  i2c_writel(i2c,OFFSET_DELAY_LEN,val);
  i2c_writel(i2c,OFFSET_TIMING,val);
  i2c_writel(i2c,OFFSET_EXT_CONF,val);
  i2c_writel(i2c,OFFSET_IO_CONFIG,val);
  i2c_writel(i2c,OFFSET_HS,val);
  /* If everything went ok (i.e. 1 msg transmitted), return #bytes
     transmitted, else error code. */
  i2c_put_adapter(adap);
  return ret;
}
static int i2c_soft_reset(int bus_id)
{
  int ret=0;
  struct i2c_adapter *adap;
  mt_i2c *i2c;
  adap = i2c_get_adapter(bus_id);
  if (!adap) return -1;
  i2c = container_of(adap,mt_i2c,adap);
  printk("I2C%d base address 0x%p\n",bus_id, i2c->base);
  //write i2c writable register with 0
  i2c_writel(i2c,OFFSET_SOFTRESET,1);
  /* If everything went ok (i.e. 1 msg transmitted), return #bytes
     transmitted, else error code. */
  i2c_put_adapter(adap);
  return ret;
}
static int i2c_ext_conf_test(int bus_id,int val)
{
  int ret=0;
  struct i2c_adapter *adap;
  mt_i2c *i2c;
  adap = i2c_get_adapter(bus_id);
  if (!adap) return -1;
  i2c = container_of(adap,mt_i2c,adap);
  printk("I2C%d base address 0x%p\n",bus_id, i2c->base);
  //write i2c writable register with 0
  i2c_writel(i2c,OFFSET_EXT_CONF,val);
  printk("EXT_CONF 0x%x",i2c_readl(i2c,OFFSET_EXT_CONF));
  /* If everything went ok (i.e. 1 msg transmitted), return #bytes
     transmitted, else error code. */
  i2c_put_adapter(adap);
  return ret;
}
static void hex2string(unsigned char * in, unsigned char * out, int length)
{
  unsigned char * ptr = in;
  unsigned char * ptrout = out;
  unsigned char t;

  while ( length-- ) {
    t = (*ptr & 0xF0) >> 4 ;
    if ( t < 10 ) *ptrout = t + '0';
    else *ptrout = t + 'A' - 10;

    ptrout++;

    t = (*ptr & 0x0F);
    if ( t < 10 ) *ptrout = t + '0';
    else *ptrout = t + 'A' - 10;

    ptr++;
    ptrout++;
  }
  *ptrout = 0;
}

/*
add this function for cast pointer from PA to VA
1 32bit, but open 4G RAM
2 64bit
   -32-bit/64-bit is ok
   -32-bit with open 4G DRAM config will lose high 32bit ([63:32])
Note: this function will cast 64 bit address to 32 bit, so, must need to confirm [63:32] of address
is 0. need flag GFP_DMA32 for dma_alloc_coherent()
*/
char *mt_i2c_bus_to_virt(unsigned long address)
{
   return (char *)address;
}

static ssize_t set_config(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
  int bus_id;
  int address;
  int operation;
  int trans_mode;
  int trans_stop;
  int speed_mode;
  int pushpull_mode;
  int query_mode;
  int timing;
  int trans_num;
  int trans_auxlen;
  int dir=0;

  int number = 0;
  int length = 0;
  unsigned int ext_flag = 0;
  dma_addr_t dma_addr = 0;
  void * vir_addr = NULL;
  //int status;
  int ret = 0;

  unsigned char tmpbuffer[128];
  printk("%s\n", buf);
  //if ( sscanf(buf, "%d %d %d %d %d %d %d %d %d %d %d %d %s", &bus_id, &address, &operation, &trans_mode, &trans_stop, &speed_mode, &pushpull_mode, &query_mode, &timing, &trans_num, &trans_auxlen,&dir, data_buffer) ) {
  if ( sscanf(buf, "%d %x %d %d %d %d %d %d %d %d %d %s", &bus_id, &address, &operation, &trans_mode, &trans_stop, &speed_mode, &pushpull_mode, &query_mode, &timing, &trans_num, &trans_auxlen,data_buffer) ) {
      if((address != 0)&&(operation<=2)){
        length = strlen(data_buffer);
      if (operation == 0){
        ext_flag |= I2C_WR_FLAG;
        number = (trans_auxlen << 8 ) | (length >> 1);
      } else if (operation == 1) {
        //ext_flag |= 0x80000000;
        number = (trans_num << 16 ) | (length >> 1);
      } else if (operation == 2) {
        //ext_flag &= 0x7FFFFFFF;
        number = (trans_num << 16 ) | (length >> 1);
      } else {

        printk("invalid operation\n");
        goto err;
      }
      if(dir > 0)
        ext_flag |= I2C_DIRECTION_FLAG;

      if (trans_mode == 0){
        //default is fifo
      } else if (trans_mode == 1) {
        ext_flag |= I2C_DMA_FLAG;
      } else {

        printk("invalid trans_mod fifo/dma\n");
        goto err;
      }

      if (trans_stop == 0) {
        //default
      } else if (trans_stop == 1) {
        ext_flag |= I2C_RS_FLAG;
      } else {

        printk("invalid trans_stop\n");
        goto err;
      }

      if (speed_mode == 0) {
        timing = 0;//ST mode
      } else if (speed_mode == 1) {
        //ext_flag |= I2C_FS_FLAG;//FS MODE
      } else if (speed_mode == 2) {
        ext_flag |= I2C_HS_FLAG;//HS mode
      } else {
        printk("invalid speed_mode\n");
        goto err;
      }

      if (pushpull_mode == 0){
        //default
      } else if (pushpull_mode == 1) {
        ext_flag |= I2C_PUSHPULL_FLAG;
      } else {

        printk("invalid pushpull mode\n");
        goto err;
      }

      if ( query_mode == 0 ){
        //
      } else if (query_mode == 1) {
        ext_flag |= I2C_POLLING_FLAG;
      } else {

        printk("invalid query mode interrupt/polling\n");
        goto err;
      }

      if (trans_mode == 1) {/*DMA MODE*/
	  	/*need GFP_DMA32 flag to confirm DMA alloc PA is 32bit range*/
        vir_addr = dma_alloc_coherent(dev, length >> 1,  &dma_addr, GFP_KERNEL|GFP_DMA32);
        if ( vir_addr == NULL ){

          printk("alloc dma memory failed\n");
          goto err;
        }
      } else {
        vir_addr = kzalloc(length >> 1, GFP_KERNEL);
        if ( vir_addr == NULL){

          printk("alloc virtual memory failed\n");
          goto err;
        }
      }

      get_hexbuffer(data_buffer, vir_addr);
      printk(KERN_ALERT"bus_id:%d,address:%x,count:%x,ext_flag:0x%x,timing:%d\n", bus_id,address,number,ext_flag,timing);
      printk(KERN_ALERT"data_buffer:%s\n", data_buffer);

      if ( trans_mode == 1 ){
        /*DMA*/
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
        pr_info("phys_addr: 0x%llx\n", dma_addr);
#else
        pr_info("phys_addr: 0x%x\n", dma_addr);
#endif
        ret = i2c_trans_data(bus_id, address, mt_i2c_bus_to_virt(dma_addr), number, ext_flag, timing,operation);
      } else {
        ret = i2c_trans_data(bus_id, address, vir_addr, number, ext_flag, timing,operation);
      }

      //dealing

      if ( ret >= 0) {

        if ( operation == 1 ) {
          hex2string(vir_addr, tmpbuffer, length >> 1);
          sprintf(data_buffer, "1 %s", tmpbuffer);
          printk("received data: %s\n", tmpbuffer);
        } else if ( operation == 0 ){
          hex2string(vir_addr, tmpbuffer, trans_auxlen);
          sprintf(data_buffer, "1 %s", tmpbuffer);
          printk("received data: %s\n", tmpbuffer);
        } else {
          sprintf(data_buffer, "1 %s", "00");
        }
        printk("Actual return Value:%d 0x%p\n",ret, vir_addr);
      } else if ( ret < 0 ) {

        if ( ret == -EINVAL) {
          sprintf(data_buffer, "0 %s", "Invalid Parameter");
        } else if ( ret == -ETIMEDOUT ) {
          sprintf(data_buffer, "0 %s", "Transfer Timeout");
        } else if ( ret == -EREMOTEIO ) {
          sprintf(data_buffer, "0 %s", "Ack Error");
        } else {
          sprintf(data_buffer, "0 %s", "unknow error");
        }
        printk("Actual return Value:%d 0x%p\n",ret, vir_addr);
      }

      if (trans_mode == 1 && vir_addr != NULL) {/*DMA MODE*/
        dma_free_coherent(dev, length >> 1, vir_addr, dma_addr);
      } else {
          if (vir_addr)
            kfree(vir_addr);
      }
      //log for UT test.
      {
        struct i2c_adapter *adap= i2c_get_adapter(bus_id);
        mt_i2c *i2c = i2c_get_adapdata(adap);
        _i2c_dump_info(i2c);
      }
    }else{
    struct i2c_adapter *adap= i2c_get_adapter(bus_id);
    mt_i2c *i2c = i2c_get_adapdata(adap);
      if(operation == 3){
        _i2c_dump_info(i2c);
      }else if(operation == 4){
        i2c_test_reg(bus_id,0);
        _i2c_dump_info(i2c);
        i2c_test_reg(bus_id,0xFFFFFFFF);
        _i2c_dump_info(i2c);
      }else if(operation == 5){
        i2c_ext_conf_test(bus_id,address);
      }else if(operation == 9){
        i2c_soft_reset(bus_id);
        _i2c_dump_info(i2c);
      }else if(operation == 6){
        if(bus_id == 0){
          //I2C0 PINMUX2 power on
          //hwPowerOn(MT65XX_POWER_LDO_VMC1,VOL_DEFAULT,"i2c_pinmux");
          //hwPowerOn(MT65XX_POWER_LDO_VMCH1,VOL_DEFAULT,"i2c_pinmux");
      }

      }else if(operation == 7){
        mt_i2c_test(1,0x50);
      }else{
        dev_err(dev, "i2c debug system: Parameter invalid!\n");
      }
      _i2c_dump_info(i2c);

    }
  } else {
    /*parameter invalid*/
    dev_err(dev, "i2c debug system: Parameter invalid!\n");
  }

  return count;
err:
  printk("analyze failed\n");
  return -1;
}

static DEVICE_ATTR(ut, 660, show_config, set_config);

static int i2c_common_probe(struct platform_device *pdev)
{
  int ret = 0;
  //your code here£¬your should save client in your own way
  printk(KERN_ALERT"i2c_common device probe\n");
  ret = device_create_file(&pdev->dev, &dev_attr_ut);
  return ret;
}

static int i2c_common_remove(struct platform_device *pdev)
{
  int ret = 0;
  //your code here
  device_remove_file(&pdev->dev, &dev_attr_ut);
  return ret;
}

static struct platform_driver i2c_common_driver= {
  .driver = {
    .name   = "mt-i2cd",
    .owner  = THIS_MODULE,
  },

  .probe  = i2c_common_probe,
  .remove = i2c_common_remove,
};


//platfrom device
static struct platform_device i2c_common_device = {
  .name = "mt-i2cd",
  .dev = {
    .coherent_dma_mask = DMA_BIT_MASK(32),
  },
};

static int __init xxx_init( void )
{
  printk(KERN_ALERT"i2c_common device init\n");
  platform_device_register(&i2c_common_device);
  return platform_driver_register(&i2c_common_driver);
}

static void __exit xxx_exit(void)
{
  platform_driver_unregister(&i2c_common_driver);
  platform_device_unregister(&i2c_common_device);
}

module_init( xxx_init );
module_exit( xxx_exit);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek I2C Bus Driver Test Driver");
MODULE_AUTHOR("Ranran Lu");

