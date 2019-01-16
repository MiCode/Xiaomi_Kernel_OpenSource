
#ifndef __MT_I2C_H__
#define __MT_I2C_H__
#include <mach/mt_typedefs.h>

#define I2CTAG                "[mt-i2c]"
#define I2CLOG(fmt, arg...)   printk(KERN_ERR I2CTAG fmt, ##arg)
#define I2CMSG(fmt, arg...)   printk(fmt, ##arg)
#define I2CERR(fmt, arg...)   printk(KERN_ERR I2CTAG "ERROR,%d: "fmt, __LINE__, ##arg)
#define I2CFUC(fmt, arg...)   printk(I2CTAG "%s\n", __FUNCTION__)

#define I2C_DRIVER_IN_KERNEL
#ifdef I2C_DRIVER_IN_KERNEL
  #define I2C_MB() mb()
  //#define I2C_DEBUG
  #ifdef I2C_DEBUG
    #define I2C_BUG_ON(a) BUG_ON(a)
  #else
    #define I2C_BUG_ON(a)
  #endif

  #ifdef CONFIG_MTK_FPGA
    #define CONFIG_MT_I2C_FPGA_ENABLE
  #endif

  #if (defined(CONFIG_MT_I2C_FPGA_ENABLE))
    #define I2C_CLK_RATE        12000     /* kHz for FPGA I2C work frequency */
    //#define I2C_CLK_RATE        20400     /* kHz for FPGA I2C work frequency */
  #else
    #include <mach/mt_clkmgr.h>
    #include <mach/upmu_common.h>
    extern kal_uint32 mt_get_bus_freq(void);
  #endif
#else
  #define I2C_MB()
  #define I2C_BUG_ON(a)
  #define I2C_M_RD       0x0001
#endif


#define I2C_OK          0x0000
#define EAGAIN_I2C    11  /* Try again */
#define EINVAL_I2C    22  /* Invalid argument */
#define EOPNOTSUPP_I2C  95  /* Operation not supported on transport endpoint */
#define ETIMEDOUT_I2C                       110 /* Connection timed out */
#define EREMOTEIO_I2C 121 /* Remote I/O error */
#define ENOTSUPP_I2C 524 /* Remote I/O error */
#define I2C_WRITE_FAIL_HS_NACKERR           0xA013
#define I2C_WRITE_FAIL_ACKERR               0xA014
#define I2C_WRITE_FAIL_TIMEOUT              0xA015

//#define I2C_CLK_WRAPPER_RATE    36000   /* kHz for wrapper I2C work frequency */

/* refer to AP_DMA register address */
#define DMA_I2C_BASE_CH(id) (AP_DMA_BASE + 0x100 + (0x80 * (id)))
#define DMA_I2C_BASE(id, base) ((base) + 0x100 + (0x80 * (id)))


/******************************************register operation***********************************/
enum I2C_REGS_OFFSET {
  OFFSET_DATA_PORT      = 0x0,    //0x0
  OFFSET_SLAVE_ADDR     = 0x04,   //0x04
  OFFSET_INTR_MASK      = 0x08,   //0x08
  OFFSET_INTR_STAT      = 0x0C,   //0x0C
  OFFSET_CONTROL        = 0x10,   //0X10
  OFFSET_TRANSFER_LEN   = 0x14,   //0X14
  OFFSET_TRANSAC_LEN    = 0x18,   //0X18
  OFFSET_DELAY_LEN      = 0x1C,   //0X1C
  OFFSET_TIMING         = 0x20,   //0X20
  OFFSET_START          = 0x24,   //0X24
  OFFSET_EXT_CONF       = 0x28,
  OFFSET_FIFO_STAT      = 0x30,   //0X30
  OFFSET_FIFO_THRESH    = 0x34,   //0X34
  OFFSET_FIFO_ADDR_CLR  = 0x38,   //0X38
  OFFSET_IO_CONFIG      = 0x40,   //0X40
  OFFSET_RSV_DEBUG      = 0x44,   //0X44
  OFFSET_HS             = 0x48,   //0X48
  OFFSET_SOFTRESET      = 0x50,   //0X50
  OFFSET_DCM_EN         = 0x54,   //0X54
  OFFSET_DEBUGSTAT      = 0x64,   //0X64
  OFFSET_DEBUGCTRL      = 0x68,   //0x68
  OFFSET_TRANSFER_LEN_AUX      = 0x6C,   //0x6C
};

#define I2C_HS_NACKERR            (1 << 2)
#define I2C_ACKERR                (1 << 1)
#define I2C_TRANSAC_COMP          (1 << 0)

#define I2C_FIFO_SIZE             8

#define MAX_ST_MODE_SPEED         100  /* khz */
#define MAX_FS_MODE_SPEED         400  /* khz */
#define MAX_HS_MODE_SPEED         3400 /* khz */

#define MAX_DMA_TRANS_SIZE        65532 /* Max(65535) aligned to 4 bytes = 65532 */
#define MAX_DMA_TRANS_NUM         256

#define MAX_SAMPLE_CNT_DIV        8
#define MAX_STEP_CNT_DIV          64
#define MAX_HS_STEP_CNT_DIV       8

#define DMA_ADDRESS_HIGH          (0xC0000000)

enum DMA_REGS_OFFSET {
  OFFSET_INT_FLAG       = 0x0,
  OFFSET_INT_EN         = 0x04,
  OFFSET_EN             = 0x08,
  OFFSET_RST            = 0x0C,
  OFFSET_STOP           = 0x10,
  OFFSET_FLUSH          = 0x14,
  OFFSET_CON            = 0x18,
  OFFSET_TX_MEM_ADDR    = 0x1c,
  OFFSET_RX_MEM_ADDR    = 0x20,
  OFFSET_TX_LEN         = 0x24,
  OFFSET_RX_LEN         = 0x28,
  OFFSET_INT_BUF_SIZE	= 0x38,
  OFFSET_DEBUG_STA      = 0x50,
  OFFSET_TX_4G_SUPPORT  = 0x54,
  OFFSET_RX_4G_SUPPORT  = 0x58,
};

struct i2c_dma_info {
	unsigned long base;
    unsigned int int_flag;
	unsigned int int_en;
	unsigned int en;
	unsigned int rst;
	unsigned int stop;
	unsigned int flush;
	unsigned int con;
	unsigned int tx_mem_addr;
	unsigned int rx_mem_addr;
	unsigned int tx_len;
	unsigned int rx_len;
	unsigned int int_buf_size;
	unsigned int debug_sta;
	unsigned int tx_4g_support;
	unsigned int rx_4g_support;
    
};

enum i2c_trans_st_rs {
    I2C_TRANS_STOP = 0,
    I2C_TRANS_REPEATED_START,
};

typedef enum {
     ST_MODE,
     FS_MODE,
     HS_MODE,
 }I2C_SPEED_MODE;

enum mt_trans_op {
  I2C_MASTER_NONE = 0,
  I2C_MASTER_WR = 1,
  I2C_MASTER_RD,
  I2C_MASTER_WRRD,
};

//CONTROL
#define I2C_CONTROL_RS                    (0x1 << 1)
#define I2C_CONTROL_DMA_EN                (0x1 << 2)
#define I2C_CONTROL_CLK_EXT_EN            (0x1 << 3)
#define I2C_CONTROL_DIR_CHANGE            (0x1 << 4)
#define I2C_CONTROL_ACKERR_DET_EN         (0x1 << 5)
#define I2C_CONTROL_TRANSFER_LEN_CHANGE   (0x1 << 6)
//#define I2C_CONTROL_WRAPPER               (0x1 << 0)
/***********************************end of register operation****************************************/
/***********************************I2C Param********************************************************/
struct mt_trans_data {
  U16 trans_num;
  U16 data_size;
  U16 trans_len;
  U16 trans_auxlen;
};
struct i2c_dma_buf {
	u8 *vaddr;
	dma_addr_t paddr;
};

typedef struct mt_i2c_t {
  #ifdef I2C_DRIVER_IN_KERNEL
    //==========only used in kernel================//
    struct i2c_adapter  adap;   /* i2c host adapter */
    struct device   *dev;   /* the device object of i2c host adapter */
    atomic_t          trans_err;  /* i2c transfer error */
    atomic_t          trans_comp; /* i2c transfer completion */
    atomic_t          trans_stop; /* i2c transfer stop */
    spinlock_t        lock;   /* for mt_i2c struct protection */
    wait_queue_head_t wait;   /* i2c transfer wait queue */
  #endif
  //==========set in i2c probe============//
	void     __iomem *base;    /* i2c base addr */
  U16      id;
  U16      irqnr;    /* i2c interrupt number */
  U16      irq_stat; /* i2c interrupt status */
  U32      clk;     /* host clock speed in khz */
  U32      pdn;     /*clock number*/
  //==========common data define============//
  enum     i2c_trans_st_rs st_rs;
  enum     mt_trans_op op;
	void     __iomem *pdmabase;
  U32      speed;   //The speed (khz)
  U16      delay_len;    //number of half pulse between transfers in a trasaction
  U32      msg_len;    //number of half pulse between transfers in a trasaction
  U8       *msg_buf;    /* pointer to msg data      */
  U8       addr;      //The address of the slave device, 7bit,the value include read/write bit.
  U8       master_code;/* master code in HS mode */
  U8       mode;    /* ST/FS/HS mode */
  //==========reserved funtion============//
  U8       is_push_pull_enable; //IO push-pull or open-drain
  U8       is_clk_ext_disable;   //clk entend default enable
  U8       is_dma_enabled;   //Transaction via DMA instead of 8-byte FIFO
  U8       read_flag;//read,write,read_write
  BOOL     dma_en;
  BOOL     poll_en;
  BOOL     pushpull;//open drain
  BOOL     filter_msg;//filter msg error log
  BOOL     i2c_3dcamera_flag;//flag for 3dcamera

  //==========define reg============//
  U16      timing_reg;
  U16      high_speed_reg;
  U16      control_reg;
  U32      last_speed;
  U8       last_mode;
  U32      defaul_speed;
  struct   mt_trans_data trans_data;
  struct i2c_dma_buf dma_buf;
}mt_i2c;
//external API
void _i2c_dump_info(mt_i2c *i2c);
void i2c_writel(mt_i2c * i2c, U8 offset, U16 value);
U32 i2c_readl(mt_i2c * i2c, U8 offset);


#endif /* __MT_I2C_H__ */
