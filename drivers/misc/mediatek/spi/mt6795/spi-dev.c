#include <linux/spi/spi.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <mach/mt_spi.h>

#include <linux/dma-mapping.h>
#include <linux/sched.h>
#include "mt_spi_hal.h"
#include <linux/kthread.h>

#ifdef CONFIG_TRUSTONIC_TEE_SUPPORT
#define SPI_TRUSTONIC_TEE_SUPPORT
#endif

#ifdef SPI_TRUSTONIC_TEE_SUPPORT
#include <mobicore_driver_api.h>
#include <tlspi_Api.h>
#endif

#define SPIDEV_LOG(fmt, args...) printk("[SPI-UT]: [%s]:[%d]" fmt, __func__, __LINE__, ##args) 
#define SPIDEV_MSG(fmt, args...) printk(KERN_ERR  fmt, ##args )
//#define SPI_STRESS_MAX 512
#define SPI_STRESS_MAX 1 
DECLARE_COMPLETION(mt_spi_done);
static u32 stress_err = 0;
static struct task_struct *spi_concur1;
static struct task_struct *spi_concur2;
static struct task_struct *spi_concur3;
static struct task_struct *spi_concur4;

static struct spi_transfer stress_xfer[SPI_STRESS_MAX];
static struct spi_transfer stress_xfer_con[SPI_STRESS_MAX];

static struct spi_message stress_msg[SPI_STRESS_MAX];
static struct spi_device *spi_test;

static int spi_setup_xfer(struct device *dev, struct spi_transfer *xfer, u32 len, u32 flag)
{
	u32 tx_buffer = 0x12345678;
	u32 cnt, i;
#if 0
	u8 *p;
#endif
#define SPI_CROSS_ALIGN_OFFSET 1008	
	xfer->len=len;

	xfer->tx_buf = ( u32 * ) kzalloc ( len, GFP_KERNEL);
	xfer->rx_buf = ( u32 * ) kzalloc ( len, GFP_KERNEL);
	
	if((xfer->tx_buf == NULL) || (xfer->rx_buf == NULL)  )
			return -1;
	
	cnt = (len%4)?(len/4 + 1):(len/4);

	if(flag == 0){
		for ( i = 0; i < cnt; i++ )
			*(( u32 * )xfer->tx_buf + i) = tx_buffer;
	}else if(flag == 1){
		for ( i = 0; i < cnt; i++ )
			*(( u32 * )xfer->tx_buf + i) = tx_buffer + i;
	}else if(flag == 2){//cross 1 K boundary
		if(len < 2048)
			return -EINVAL;
		for ( i = 0; i < cnt; i++ )
			*(( u32 * )xfer->tx_buf + i) = tx_buffer + i;

		xfer->tx_dma = dma_map_single(dev, (void *)xfer->tx_buf, 	
				xfer->len,  DMA_TO_DEVICE);				
		if (dma_mapping_error(dev, xfer->tx_dma)) {
			SPIDEV_LOG("dma mapping tx_buf error.\n");
			return -ENOMEM;	
		}
		xfer->rx_dma = dma_map_single(dev, (void *)xfer->rx_buf, 	
				xfer->len,  DMA_TO_DEVICE);				
		if (dma_mapping_error(dev, xfer->rx_dma)) {
			SPIDEV_LOG("dma mapping rx_buf error.\n");
			return -ENOMEM;	
		}
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
		SPIDEV_LOG("Transfer addr:Tx:0x%llx, Rx:0x%llx, before\n", xfer->tx_dma,xfer->rx_dma);
#else
		SPIDEV_LOG("Transfer addr:Tx:0x%x, Rx:0x%x, before\n", xfer->tx_dma,xfer->rx_dma);
#endif
		xfer->len=32;
#if 0
		p= xfer->tx_dma;
		xfer->tx_dma = (u32)(p+ SPI_CROSS_ALIGN_OFFSET);
		p= xfer->rx_dma;
		xfer->rx_dma = (u32)(p+ SPI_CROSS_ALIGN_OFFSET);

		t= (u8*)xfer->tx_buf;
		xfer->tx_buf =(u32*)(t+ SPI_CROSS_ALIGN_OFFSET);
		t= (u8*)xfer->rx_buf;
		xfer->rx_buf =(u32*)(t+ SPI_CROSS_ALIGN_OFFSET);
#else
		xfer->tx_dma += SPI_CROSS_ALIGN_OFFSET;
		xfer->rx_dma += SPI_CROSS_ALIGN_OFFSET;

		xfer->tx_buf += SPI_CROSS_ALIGN_OFFSET;
		xfer->rx_buf += SPI_CROSS_ALIGN_OFFSET;
#endif
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
		SPIDEV_LOG("Transfer addr:Tx:0x%llx, Rx:0x%llx\n", xfer->tx_dma,xfer->rx_dma);
#else
		SPIDEV_LOG("Transfer addr:Tx:0x%x, Rx:0x%x\n", xfer->tx_dma,xfer->rx_dma);
#endif
		SPIDEV_LOG("Transfer addr:Tx:0x%x, Rx:0x%x\n", *((u32*)xfer->tx_buf),*((u32*)xfer->rx_buf));
		//for ( i = 0; i < cnt; i++ )
		//	*(( u32 * )xfer->tx_buf + i) = tx_buffer + i;
	}else{
	return -EINVAL;
	}
		
	return 0;
	
}
static inline int 
is_last_xfer(struct spi_message *msg, struct spi_transfer *xfer)
{
	return msg->transfers.prev == &xfer->transfer_list;
}

static int spi_recv_check(struct spi_message *msg)
{

	struct spi_transfer *xfer;
	u32 cnt, i, err=0;
	
	list_for_each_entry(xfer, &msg->transfers, transfer_list) {

		if (!xfer) {
			SPIDEV_MSG ( "rv msg is NULL.\n" );
			return -1;
		}
//			SPIDEV_LOG("xfer:0x%p, length is:%d\n", xfer,xfer->len);
		cnt = (xfer->len%4)?(xfer->len/4 + 1):(xfer->len/4);
		for(i=0; i<cnt; i++){
			if(*( ( u32 * ) xfer->rx_buf + i ) != *( ( u32 * ) xfer->tx_buf + i )){ 					
				SPIDEV_LOG("tx xfer %d is:%.8x\n",i,  *( ( u32 * ) xfer->tx_buf + i ) );
				SPIDEV_LOG("rx xfer %d is:%.8x\n",i,  *( ( u32 * ) xfer->rx_buf + i ) ); 
				//SPIDEV_LOG("tx xfer %d dma is:%.8x\n",i,  ( ( u32 * )xfer->tx_dma + i ) );
				//SPIDEV_LOG("rx xfer %d dma is:%.8x\n",i,  ( ( u32 * )xfer->rx_dma + i ) );
				SPIDEV_LOG("\n");
				err++;
			}
		}
		//memset(xfer->tx_buf,0,xfer->len);
		//memset(xfer->rx_buf,0,xfer->len);
		kfree(xfer->tx_buf);
		kfree(xfer->rx_buf);
	}
	
		SPIDEV_LOG("Message:0x%p,error %d,actual xfer length is:%d\n", msg,err, msg->actual_length);

	return err;
	
}
static int spi_recv_check_all(struct spi_device *spi, struct spi_message *msg)
{

	struct spi_transfer *xfer;
	u32 i, err=0;
	int j;
	u8 rec_cac = 0;
	
	struct mt_chip_conf *chip_config;
	chip_config = (struct mt_chip_conf *) spi->controller_data;
	
	list_for_each_entry(xfer, &msg->transfers, transfer_list) {

		if (!xfer) {
			SPIDEV_MSG ( "rv msg is NULL.\n" );
			return -1;
		}
//			SPIDEV_LOG("xfer:0x%p, length is:%d\n", xfer,xfer->len);
//		cnt = (xfer->len%4)?(xfer->len/4 + 1):(xfer->len/4);
		for(i=0; i<xfer->len; i++){
			if(chip_config->tx_mlsb ^ chip_config->rx_mlsb){
				rec_cac = 0;
				for(j=7;j>=0;j--){
					rec_cac |= ((*((u8 *)xfer->tx_buf + i) & (1<<j)) >> j)<< (7-j);					
				}				
				//SPIDEV_LOG("rec_cac %x :%x\n",rec_cac,	*( ( u8 * ) xfer->tx_buf + i ) );
				
			}else{
				rec_cac = *( ( u8 * ) xfer->tx_buf + i );
			}
			if(*( ( u8 * ) xfer->rx_buf + i ) != rec_cac ){ 					
				SPIDEV_LOG("tx xfer %d is:%x\n",i,  *( ( u8 * ) xfer->tx_buf + i ) );
				SPIDEV_LOG("rx xfer %d is:%x\n",i,  *( ( u8 * ) xfer->rx_buf + i ) ); 
				err++;
			}
		}
		kfree(xfer->tx_buf);
		kfree(xfer->rx_buf);
	}
	
		SPIDEV_LOG("Message:0x%p,error %d,actual xfer length is:%d\n", msg,err, msg->actual_length);

	return err;
	
}

static void spi_complete(void *arg)
{
	static u32 i = 0;
	stress_err += spi_recv_check((struct spi_message *)arg);
	if(stress_err > 0){
		
		SPIDEV_LOG("Message transfer err:%d\n", stress_err);
//		BUG();
	}
		
//		SPIDEV_LOG("Message:%d\n", i);
	if(++i == SPI_STRESS_MAX){
		i = 0;
		complete(&mt_spi_done);
	}
}


static int threadfunc1(void *data)
{
	struct spi_transfer transfer;
	struct spi_message msg;
	struct spi_device *spi=(struct spi_device *) data;
	u32 len= 8;
	int ret;	

	while(1){
		spi_message_init(&msg);
		
		set_current_state(TASK_INTERRUPTIBLE);
		if(kthread_should_stop()) break;
		
		spi_setup_xfer(&spi->dev,&transfer,len,0);
		spi_message_add_tail(&transfer, &msg);
		ret = spi_sync(spi, &msg);
		if(ret < 0){
			stress_err ++;
			SPIDEV_LOG("Message transfer err:%d\n", ret);
		}else{
			ret = spi_recv_check(&msg);
			if(ret != 0){
				stress_err += ret;					
				SPIDEV_LOG("thread Message transfer err:%d\n", ret);
			}
		}
		schedule_timeout(HZ);
	}
	return 0;
}
static int threadfunc2(void *data)
{
	struct spi_transfer transfer;
	struct spi_message msg;	
	struct spi_device *spi=(struct spi_device *) data;
	
	u32 len= 128;
	int ret;

	while(1){
		spi_message_init(&msg);
		
		set_current_state(TASK_INTERRUPTIBLE);
		if(kthread_should_stop()) break;
		
		spi_setup_xfer(&spi->dev,&transfer,len,0);
		spi_message_add_tail(&transfer, &msg);
		ret = spi_sync(spi, &msg);
		if(ret < 0){
			stress_err ++;
			SPIDEV_LOG("Message transfer err:%d\n", ret);
		}else{
			ret = spi_recv_check(&msg);
			if(ret != 0){
				stress_err += ret;					
				SPIDEV_LOG("thread Message transfer err:%d\n", ret);
			}
		}
		schedule_timeout(HZ);
	}
	return 0;
}
static int threadfunc3(void *data)
{
	//struct spi_transfer transfer;
	struct spi_message msg;	
	struct spi_device *spi=(struct spi_device *) data;
	static struct spi_message *p;
	u32 len= 64;
	int ret;
	u16 i;
	
	while(1){
		spi_message_init(&msg);
		
		set_current_state(TASK_INTERRUPTIBLE);
		if(kthread_should_stop()) break;
		
		p = stress_msg;
		for(i=0;i<SPI_STRESS_MAX;i++){
			spi_message_init(p);
			ret = spi_setup_xfer(&spi->dev,&stress_xfer[i],len,0);
			if(ret != 0){									
				SPIDEV_LOG("xfer set up err:%d\n", ret);
			}
			spi_message_add_tail(&stress_xfer[i], p);
		
			p->complete = spi_complete;
			p->context = p;
			ret = spi_async(spi, p);
			if(ret < 0){
				SPIDEV_LOG("Message %d transfer err:%d\n",i, ret);
			}
			p ++;
		}
		
		wait_for_completion(&mt_spi_done);

		schedule_timeout(5*HZ);
	}
	return 0;
}
static int threadfunc4(void *data)
{
	//struct spi_transfer transfer;
	struct spi_message msg;	
	struct spi_device *spi=(struct spi_device *) data;

	u32 len= 32;
	int ret;
	u16 i;
	
	while(1){
		spi_message_init(&msg);
		
		set_current_state(TASK_INTERRUPTIBLE);
		if(kthread_should_stop()) break;
		
	
		for(i=0;i<SPI_STRESS_MAX;i++){			
			ret = spi_setup_xfer(&spi->dev,&stress_xfer_con[i],len,1);
			if(ret != 0){									
				SPIDEV_LOG("Message set up err:%d\n", ret);
			}
			spi_message_add_tail(&stress_xfer_con[i], &msg);
			
		}
		ret = spi_sync(spi, &msg);
		if(ret < 0){
			SPIDEV_LOG("Message transfer err:%d\n", ret);
		}else{
			ret = spi_recv_check(&msg);
			if(ret != 0){
				ret -= ret;
				stress_err += ret;
				SPIDEV_LOG("Message transfer err:%d\n", ret);
			}
//				SPIDEV_LOG("Message multi xfer stress pass\n");
		}

		schedule_timeout(2*HZ);
	}
	return 0;
}

#ifdef CONFIG_TRUSTONIC_TEE_SUPPORT
extern int secspi_session_open(void);
extern int secspi_execute(u32 cmd, tciSpiMessage_t *param);
#endif

static ssize_t spi_store(struct device *dev, 
	struct device_attribute *attr, 
	const char *buf, size_t count)
{
	struct spi_device *spi;

	struct mt_chip_conf *chip_config;
	
	u32 setuptime, holdtime, high_time, low_time;
	u32 cs_idletime, ulthgh_thrsh;
	int cpol, cpha,tx_mlsb, rx_mlsb, tx_endian;
	int rx_endian, com_mod, pause, finish_intr;
	int deassert, tckdly, ulthigh;
	
	spi = container_of(dev, struct spi_device, dev);

	SPIDEV_LOG("SPIDEV name is:%s\n", spi->modalias);

	chip_config = (struct mt_chip_conf *) spi->controller_data;

	if (!chip_config) {
		SPIDEV_LOG ( "chip_config is NULL.\n");
		chip_config = kzalloc ( sizeof ( struct mt_chip_conf ), GFP_KERNEL );
		if ( !chip_config ) 
			return -ENOMEM;
	}	
#ifdef CONFIG_TRUSTONIC_TEE_SUPPORT
	if (!strncmp(buf, "-1", 2 ) ) { /*TRANSFER*/
		    SPIDEV_MSG("start to access TL SPI driver.\n");
			secspi_session_open();
			secspi_execute(1, NULL);
			SPIDEV_MSG("secspi_execute 1 finished!!!\n");
	}else if(!strncmp(buf, "-2", 2 ) ) { /*HW CONFIG*/
		    SPIDEV_MSG("start to access TL SPI driver.\n");
			secspi_session_open();
			secspi_execute(2, NULL);
			SPIDEV_MSG("secspi_execute 2 finished!!!\n");
	}else if(!strncmp(buf, "-3", 2 ) ) { /*DEBUG*/
		    SPIDEV_MSG("start to access TL SPI driver.\n");
			secspi_session_open();
			secspi_execute(3, NULL);
			SPIDEV_MSG("secspi_execute 3 finished!!!\n");
	}else if(!strncmp(buf, "-4", 2 ) ) { /*TEST*/
		    SPIDEV_MSG("start to access TL SPI driver.\n");
			secspi_session_open();
			secspi_execute(4, NULL);
			SPIDEV_MSG("secspi_execute 4 finished!!!\n");
#else
	if (!strncmp(buf, "-h", 2 ) ) {
		SPIDEV_MSG("Please input the parameters for this device.\n");
#endif
	} else if ( !strncmp(buf, "-w", 2 ) ) {
		buf += 3;
		if (!buf) {
			SPIDEV_LOG("buf is NULL.\n");
			goto out;
		}
		if (!strncmp(buf, "setuptime=",10) && (1 == sscanf(buf+10, "%d", &setuptime))) {
			SPIDEV_MSG("setuptime is:%d\n", setuptime);
			chip_config->setuptime=setuptime;
		}else if (!strncmp(buf, "holdtime=", 9)&&(1==sscanf(buf+9, "%d", &holdtime))) {
			SPIDEV_MSG("Set holdtime is:%d\n", holdtime);
			chip_config->holdtime=holdtime;	
		}else if (!strncmp(buf, "high_time=", 10)&&(1==sscanf(buf+10, "%d", &high_time))) {
			SPIDEV_MSG("Set high_time is:%d\n", high_time);
			chip_config->high_time=high_time;	
		}else if (!strncmp(buf, "low_time=", 9)&&(1==sscanf(buf+9, "%d", &low_time))) {
			SPIDEV_MSG("Set low_time is:%d\n", low_time);
			chip_config->low_time=low_time;
		}else if (!strncmp(buf, "cs_idletime=", 12)&&(1==sscanf(buf+12, "%d", &cs_idletime))) {
			SPIDEV_MSG("Set cs_idletime is:%d\n", cs_idletime);
			chip_config->cs_idletime=cs_idletime;	
		}else if (!strncmp(buf, "ulthgh_thrsh=", 13)&&(1==sscanf(buf+13, "%d", &ulthgh_thrsh))) {
			SPIDEV_MSG("Set slwdown_thrsh is:%d\n", ulthgh_thrsh);
			chip_config->ulthgh_thrsh=ulthgh_thrsh; 
		}else if (!strncmp(buf, "cpol=", 5) && (1 == sscanf(buf+5, "%d", &cpol))){
			SPIDEV_MSG("Set cpol is:%d\n", cpol);
			chip_config->cpol = cpol;
		}else if (!strncmp(buf, "cpha=", 5) && (1 == sscanf(buf+5, "%d", &cpha))) {
			SPIDEV_MSG("Set cpha is:%d\n", cpha);
			chip_config->cpha = cpha;
		}else if (!strncmp(buf, "tx_mlsb=", 8)&&(1==sscanf(buf+8, "%d", &tx_mlsb))) {
			SPIDEV_MSG("Set tx_mlsb is:%d\n", tx_mlsb);
			chip_config->tx_mlsb=tx_mlsb;	
		}else if (!strncmp(buf, "rx_mlsb=", 8)&&(1==sscanf(buf+8, "%d", &rx_mlsb))) {
			SPIDEV_MSG("Set rx_mlsb is:%d\n", rx_mlsb);
			chip_config->rx_mlsb=rx_mlsb;	
		}else if (!strncmp(buf, "tx_endian=", 10)&&(1==sscanf(buf+10, "%d", &tx_endian))) {
			SPIDEV_MSG("Set tx_endian is:%d\n", tx_endian);
			chip_config->tx_endian=tx_endian;	
		}else if (!strncmp(buf, "rx_endian=", 10)&&(1==sscanf(buf+10, "%d", &rx_endian))) {
			SPIDEV_MSG("Set rx_endian is:%d\n", rx_endian);
			chip_config->rx_endian=rx_endian;	
		}else if (!strncmp(buf, "com_mod=", 8)&&(1==sscanf(buf+8, "%d", &com_mod))) {
			chip_config->com_mod=com_mod;
			SPIDEV_MSG("Set com_mod is:%d\n", com_mod);
		}else if (!strncmp(buf, "pause=", 6)&&(1==sscanf(buf+6, "%d", &pause))) {
			SPIDEV_MSG("Set pause is:%d\n", pause);
			chip_config->pause=pause;
		}else if (!strncmp(buf, "finish_intr=", 12)&&(1==sscanf(buf+12, "%d", &finish_intr))) {
			SPIDEV_MSG("Set finish_intr is:%d\n", finish_intr);
			chip_config->finish_intr=finish_intr;
		}else if (!strncmp(buf, "deassert=", 9)&&(1==sscanf(buf+9, "%d", &deassert))) {
			SPIDEV_MSG("Set deassert is:%d\n", deassert);
			chip_config->deassert=deassert;	
		}else if (!strncmp(buf, "ulthigh=", 8 ) && ( 1 == sscanf(buf+8, "%d", &ulthigh))) {
			SPIDEV_MSG("Set ulthigh is:%d\n", ulthigh);	
			chip_config->ulthigh=ulthigh;
		}else if (!strncmp(buf, "tckdly=",7) && ( 1 == sscanf(buf+7, "%d", &tckdly))) {
			SPIDEV_MSG("Set tckdly is:%d\n", tckdly);
			chip_config->tckdly=tckdly;
		}else {
			SPIDEV_LOG("Wrong parameters.\n");
			goto out;
		}
		spi->controller_data = chip_config;
//			spi_setup(spi);
			
	}
out:
	return count;
}

static int tc_spi_cross_1k(struct spi_device *spi)
{
	int ret=0;
	//struct spi_transfer transfer;
	//struct spi_message msg;
	//spi_setup_xfer(&transfer,2048,2);
	//spi_message_add_tail ( &transfer, &msg );
	//msg.is_dma_mapped=1;
	//ret = spi_sync ( spi_test, &msg );
	//if(ret < 0){
	//	SPIDEV_LOG("Message transfer err:%d\n", ret);
	//}else{
	//	ret = spi_recv_check(&msg);
	//}
	return ret;
}
static ssize_t 
spi_msg_store(struct device *dev, struct device_attribute *attr, 
				const char *buf, size_t count)
{
	//struct spi_transfer *xfer;
	//struct spi_message *msg_str;
	struct spi_message *p;

	int ret = 0;
	struct spi_device *spi;

	struct spi_transfer transfer;
	struct spi_transfer transfer2;
	struct spi_transfer transfer3;
	struct spi_message msg;
	struct mt_chip_conf *chip_config;

	u32 i, len=4;
	u32 tx_buffer = 0x12345678;
	u32 rx_buffer = 0xaaaaaaaa;

	transfer.tx_buf = &tx_buffer;
	transfer.rx_buf = &rx_buffer;
	transfer.len = 4;	
	
	spi = container_of ( dev, struct spi_device, dev );

	if(unlikely(!spi)){
		SPIDEV_LOG ( "spi device is invalid\n" );
		goto out;
	}
	if(unlikely(!buf)){
		SPIDEV_LOG("buf is invalid\n" );
		goto out;
	}
	spi_message_init(&msg);
	
	if( !strncmp ( buf, "-h", 2 ) ) {
		SPIDEV_MSG("Please input the message \
			of this device to send and receive. \n" );
	}else if(!strncmp(buf, "-w", 2)) {
		buf += 3;
		if (!buf) {
			SPIDEV_LOG("Parameter is not enough.\n");
			goto out;
		}
		if (!strncmp ( buf, "len=", 4 ) && 1 == sscanf ( buf+4, "%d", &len ) ) {
			spi_setup_xfer(&spi->dev,&transfer,len,0);
			spi_message_add_tail ( &transfer, &msg );
			ret = spi_sync ( spi, &msg );
			if(ret < 0){
				SPIDEV_LOG("Message transfer err:%d\n", ret);
			}else{
				ret = spi_recv_check_all(spi,&msg);
				if(ret != 0){
					ret = -ret;					
					SPIDEV_LOG("Message transfer err:%d\n", ret);
					goto out;
				}
			}				
		}
	}else if(!strncmp(buf, "-func", 5)) {
		buf += 6;
		if (!buf) {
			SPIDEV_LOG("Parameter is not enough.\n");
			goto out;
		}
		if (!strncmp ( buf, "len=", 4 ) && 1 == sscanf ( buf+4, "%d", &len ) ) {
			spi_setup_xfer(&spi->dev,&transfer,len,1);
			spi_message_add_tail ( &transfer, &msg );
			ret = spi_sync ( spi, &msg );
			if(ret < 0){
				SPIDEV_LOG("Message transfer err:%d\n", ret);
			}else{
				ret = spi_recv_check(&msg);
				if(ret != 0){
					ret = -ret;					
					SPIDEV_LOG("Message transfer err:%d\n", ret);
					goto out;
				}
			}				
		}
		if (!strncmp ( buf, "cross", 5 )) {
			ret = tc_spi_cross_1k(spi);
			if(ret < 0){
				SPIDEV_LOG("Message transfer err:%d\n", ret);
			}else if(ret != 0){
				ret = -ret;					
				SPIDEV_LOG("Message transfer err:%d\n", ret);
				goto out;
			}
		}				
	}else if(!strncmp(buf, "-err", 4)) {
		buf += 5;
		chip_config = (struct mt_chip_conf *) spi->controller_data;
		
		if(!strncmp(buf, "buf", 3)) {	//case: tx_buf = NULL,rx_buf = NULL		
			transfer.len = 8;
			transfer.tx_buf = NULL;
			transfer.rx_buf = NULL;
		}else if(!strncmp(buf, "len", 3)){//case: tx_buf != NULL,rx_buf != NULL, len = 0
			transfer.len = 0;
			transfer.tx_buf = ( u32 * ) kzalloc ( 8, GFP_KERNEL);
			transfer.rx_buf = ( u32 * ) kzalloc ( 8, GFP_KERNEL);
		}else if(!strncmp(buf, "nomem", 5)){//case: DMA mapping error
			spi_setup_xfer(&spi->dev,&transfer,8,0);
		}else if(!strncmp(buf, "fifo_len", 8)){//case: len exceed FIFO size
			chip_config->com_mod = 0;
			spi_setup_xfer(&spi->dev,&transfer,33,0);
		}else if(!strncmp(buf, "dma_len", 7)){//case: len exceed DMA size
			chip_config->com_mod = 1;
			spi_setup_xfer(&spi->dev,&transfer,1025,0);
		}else if(!strncmp(buf, "xfer", 4)){//case: len exceed DMA size
			;
		}else if(!strncmp(buf, "msg", 3)){//case: len exceed DMA size
			;
		}else{
			SPIDEV_LOG("Error test Wrong parameters.\n");
			ret = -1;
			goto out;
		}
		
		if(strncmp(buf, "xfer", 4)){	//case: MSG empty
			spi_message_add_tail(&transfer, &msg);
		}
		if(!strncmp(buf, "msg", 3)) {	//case: message = NULL	
			ret = spi_sync(spi, NULL);
				
		}else{
			ret = spi_sync(spi, &msg);			
		}		
		if(ret != -EINVAL){
				return -100;
		}else{			
			SPIDEV_LOG("Message transfer error test passed ret:%d\n", ret);
		}
	}else if( !strncmp(buf, "-pause", 6)){
		spi_setup_xfer(&spi->dev,&transfer,32,0);
		spi_message_add_tail(&transfer, &msg);
		spi_setup_xfer(&spi->dev,&transfer2,1024,0);
		spi_message_add_tail(&transfer2, &msg);
		spi_setup_xfer(&spi->dev,&transfer3,32,0);
		spi_message_add_tail(&transfer3, &msg);
		
		ret = spi_sync(spi, &msg);
		if(ret < 0){
			SPIDEV_LOG("Message transfer err:%d\n", ret);
		}else{
			ret = spi_recv_check(&msg);
			if(ret != 0){
				ret = -ret;					
				printk(KERN_ERR"Message transfer err:%d\n", ret);
			}
		}		
	}else if(!strncmp(buf, "-stress", 7)){
		sscanf ( buf+8, "%d", &len );
		stress_err = 0;	
		if(len == 0){
	//			xfer = (struct spi_transfer *)kzalloc(SPI_STRESS_MAX*sizeof( struct spi_transfer) , GFP_KERNEL);
			//will release in spi_recv_check() function
			SPIDEV_LOG("Message multi xfer stress start\n");
			for(i=0;i<SPI_STRESS_MAX;i++){			
				ret = spi_setup_xfer(&spi->dev,&stress_xfer[i],32,0);
				if(ret != 0){									
					SPIDEV_LOG("Message set up err:%d\n", ret);
				}
				spi_message_add_tail(&stress_xfer[i], &msg);
				
			}
			ret = spi_sync(spi, &msg);
			if(ret < 0){
				SPIDEV_LOG("Message transfer err:%d\n", ret);
			}else{
				ret = spi_recv_check(&msg);
				if(ret != 0){
					ret = -ret;					
					SPIDEV_LOG("Message transfer err:%d\n", ret);
				}else{
					SPIDEV_LOG("Message multi xfer stress pass\n");
				}
			}
		}else if(len == 1){	
			int loop_count = 0;
			//loop_count  =250, take aboat half a hour.
			for(loop_count=0;loop_count <= 5000;loop_count ++){
		//			msg_str = (struct spi_message *)kzalloc(SPI_STRESS_MAX * sizeof(struct spi_message) , GFP_KERNEL);
		//			xfer = ( struct spi_transfer * )kzalloc(SPI_STRESS_MAX * sizeof(struct spi_transfer) , GFP_KERNEL);
				SPIDEV_LOG("Message multi msg stress start\n");
				p = stress_msg;
				for(i=0;i<SPI_STRESS_MAX;i++){
					spi_message_init(p);
					ret = spi_setup_xfer(&spi->dev,&stress_xfer[i],32,0);
					if(ret != 0){									
						SPIDEV_LOG("xfer set up err:%d\n", ret);
					}
					spi_message_add_tail(&stress_xfer[i], p);
				
					p->complete = spi_complete;
					p->context = p;
		//				ret = spi_async(spi, p);
		//				if(ret < 0){
		//					SPIDEV_LOG("Message %d transfer err:%d\n",i, ret);
		//				}
					p ++;
				}
				p = stress_msg;
				for(i=0;i<SPI_STRESS_MAX;i++){
					ret = spi_async(spi, p);
					if(ret < 0){
						SPIDEV_LOG("Message %d transfer err:%d\n",i, ret);
					}
					p++;
				}
				wait_for_completion(&mt_spi_done);
			}
	//			kfree(msg_str);
			if(stress_err != 0){
				ret = -stress_err;					
				stress_err = 0;
				SPIDEV_LOG("Message stress err:%d\n", ret);
			}else{
				SPIDEV_LOG("Message stress Pass\n");
				ret = 0;
			}
		}else{
		}	
	}else if(!strncmp(buf, "-concurrent", 11)){

		stress_err = 0;
		spi_concur1 = kthread_run(threadfunc1,(void *)spi,"spi_concrrent1");
		if(IS_ERR(spi_concur1)){
			SPIDEV_LOG("Unable to start kernelthread 1\n");
			ret = -5;
			goto out;
		}
		spi_concur2 = kthread_run(threadfunc2,(void *)spi,"spi_concrrent2");
		if(IS_ERR(spi_concur2)){
			SPIDEV_LOG("Unable to start kernelthread 2\n");
			ret = -5;
			goto out;
		}
		spi_concur3 = kthread_run(threadfunc3,(void *)spi,"spi_concrrent3");
		if(IS_ERR(spi_concur3)){
			SPIDEV_LOG("Unable to start kernelthread 3\n");
			ret = -5;
			goto out;
		}
		spi_concur4 = kthread_run(threadfunc4,(void *)spi,"spi_concrrent4");
		if(IS_ERR(spi_concur4)){
			SPIDEV_LOG("Unable to start kernelthread 4\n");
			ret = -5;
			goto out;
		}

		msleep(10000);
		SPIDEV_LOG("stop kernelthread \n");

		kthread_stop(spi_concur1);
		kthread_stop(spi_concur2);
		kthread_stop(spi_concur3);
		kthread_stop(spi_concur4);
		ret = -stress_err;
		stress_err = 0;
		SPIDEV_LOG("concurrent test done %d\n\n\n",ret);
		
	}else{
		SPIDEV_LOG("Wrong parameters.\n");
		ret = -1;
		goto out;
	}
	ret = count;

out:
	return ret;
		
}

static DEVICE_ATTR(spi, 0200, NULL, spi_store);
static DEVICE_ATTR(spi_msg, 0200, NULL, spi_msg_store);



static struct device_attribute *spi_attribute[]={
	&dev_attr_spi,
	&dev_attr_spi_msg,
};

static int spi_create_attribute(struct device *dev)
{
	int num,idx;
	int err =0;
	num = (int)(sizeof(spi_attribute)/sizeof(spi_attribute[0]));

	for (idx = 0; idx < num; idx ++) {
		if ((err = device_create_file(dev, spi_attribute[idx])))
			break;
	}
	return err;
	
}

static void spi_remove_attribute(struct device *dev)
{
	int num, idx;
	num = (int)(sizeof(spi_attribute)/sizeof(spi_attribute[0]));

	for (idx = 0; idx < num; idx ++) {
		device_remove_file(dev, spi_attribute[idx]);
	}

	return;
}

static int spi_test_remove(struct spi_device *spi)
{

	SPIDEV_LOG("spi_test_remove.\n");
	spi_remove_attribute(&spi->dev);
	return 0;
}

static int __init spi_test_probe(struct spi_device *spi)
{
	SPIDEV_LOG("spi test probe  enter\n");
	spi_test=spi;
	spi->mode = SPI_MODE_3;
	spi->bits_per_word = 32;
	return spi_create_attribute( &spi->dev );
	return 0;
}

//struct spi_device_id spi_id_table = {"test_spi", 0};
struct spi_device_id spi_id_table = {"spi-ut", 0};

static struct spi_driver spi_test_driver = {
	.driver = {
		.name = "test_spi",
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
	},
	.probe = spi_test_probe,
	.remove=spi_test_remove,
	.id_table = &spi_id_table,
};
static struct spi_board_info spi_board_devs[] __initdata = {
	[0] = {
        	.modalias="spi-ut",
		.bus_num = 0,
		.chip_select=1,
		.mode = SPI_MODE_3,
	},
};

static int __init spi_dev_init(void)
{
	SPIDEV_LOG("SPI_DEV_INIT.\n");
	spi_register_board_info(spi_board_devs, ARRAY_SIZE(spi_board_devs));
	return spi_register_driver(&spi_test_driver);
}

static void __exit spi_test_exit(void)
{
	SPIDEV_LOG("SPI_DEV_EXIT.\n");
	spi_unregister_driver(&spi_test_driver);
	
	return;
}

module_init(spi_dev_init);
module_exit(spi_test_exit);

MODULE_DESCRIPTION ( "mt SPI test device driver" );
MODULE_AUTHOR ( "Ranran Lu <ranran.lu@mediatek.com>" );
MODULE_LICENSE("GPL");
