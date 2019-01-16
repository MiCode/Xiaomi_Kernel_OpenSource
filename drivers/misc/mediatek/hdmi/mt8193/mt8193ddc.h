#ifndef __mt8193ddc_h__
#define __mt8193ddc_h__
#ifdef CONFIG_MTK_MT8193_HDMI_SUPPORT


#define EDID_BLOCK_LEN      128
#define EDID_SIZE 512

#define SIF1_CLOK 260		/* 27M/432 = 62.5Khz */


#define EDID_ID     0x50	/* 0xA0 */
#define EDID_ID1    0x51	/* 0xA2 */

#define EDID_ADDR_HEADER                      0x00
#define EDID_ADDR_VERSION                     0x12
#define EDID_ADDR_REVISION                    0x13
#define EDID_IMAGE_HORIZONTAL_SIZE            0x15
#define EDID_IMAGE_VERTICAL_SIZE              0x16
#define EDID_ADDR_FEATURE_SUPPORT             0x18
#define EDID_ADDR_TIMING_DSPR_1               0x36
#define EDID_ADDR_TIMING_DSPR_2               0x48
#define EDID_ADDR_MONITOR_DSPR_1              0x5A
#define EDID_ADDR_MONITOR_DSPR_2              0x6C
#define EDID_ADDR_EXT_BLOCK_FLAG              0x7E
#define EDID_ADDR_EXTEND_BYTE3                0x03	/* EDID address: 0x83 */
						   /* for ID receiver if RGB, YCbCr 4:2:2 or 4:4:4 */
/* Extension Block 1: */
#define EXTEDID_ADDR_TAG                      0x00
#define EXTEDID_ADDR_REVISION                 0x01
#define EXTEDID_ADDR_OFST_TIME_DSPR           0x02


/* ddcci master */
#define DDC_DDCMCTL0           ((UINT32)0x0)
#define DDCM_ODRAIN          ((UINT32)0x1<<31)
#define DDCM_CLK_DIV_OFFSET  ((UINT32)16)
#define DDCM_CLK_DIV_MASK    ((UINT32)0xFFF<<16)
#define DDCM_CS_STATUS       ((UINT32)0x1<<4)
#define DDCM_SCL_STATE       ((UINT32)0x1<<3)
#define DDCM_SDA_STATE       ((UINT32)0x1<<2)
#define DDCM_SM0EN           ((UINT32)0x1<<1)
#define DDCM_SCL_STRECH      ((UINT32)0x1<<0)

#define DDC_DDCMCTL1           ((UINT32)0x4)
#define DDCM_ACK_OFFSET      ((UINT32)16)
#define DDCM_ACK_MASK        ((UINT32)0xFF<<16)
#define DDCM_PGLEN_OFFSET    ((UINT32)8)
#define DDCM_PGLEN_MASK      ((UINT32)0x7<<8)
#define DDCM_SIF_MODE_OFFSET ((UINT32)4)
#define DDCM_SIF_MODE_MASK   ((UINT32)0x7<<4)
#define DDCM_START            ((UINT32)0x1)
#define DDCM_WRITE_DATA       ((UINT32)0x2)
#define DDCM_STOP             ((UINT32)0x3)
#define DDCM_READ_DATA_NO_ACK ((UINT32)0x4)
#define DDCM_READ_DATA_ACK    ((UINT32)0x5)
#define DDCM_TRI             ((UINT32)0x1<<0)

#define DDC_DDCMD0             ((UINT32)0x8)
#define DDCM_DATA3           ((UINT32)0xFF<<24)
#define DDCM_DATA2           ((UINT32)0xFF<<16)
#define DDCM_DATA1           ((UINT32)0xFF<<8)
#define DDCM_DATA0           ((UINT32)0xFF<<0)

#define DDC_DDCMD1             ((UINT32)0xC)
#define DDCM_DATA7           ((UINT32)0xFF<<24)
#define DDCM_DATA6           ((UINT32)0xFF<<16)
#define DDCM_DATA5           ((UINT32)0xFF<<8)
#define DDCM_DATA4           ((UINT32)0xFF<<0)


typedef enum {
	SIF_8_BIT,		/* /< [8 bits data address.] */
	SIF_16_BIT,		/* /< [16 bits data address.] */
} SIF_BIT_T;

typedef enum {
	SIF_NORMAL,		/* /< [Normal, always select this.] */
	SIF_OTHER,		/* /< [Other.] */
} SIF_TYPE_T;
  /* / [Sif control mode select.] */
typedef struct _SIF_MODE_T {
	SIF_BIT_T eBit;		/* /< [The data address type. ] */
	SIF_TYPE_T eType;	/* /< [The control mode.] */
} SIF_MODE_T;

extern u8 fgDDCDataRead(u8 bDevice, u8 bData_Addr, u8 bDataCount, u8 *prData);
extern u8 fgDDCDataWrite(u8 bDevice, u8 bData_Addr, u8 bDataCount, u8 *prData);

#endif
#endif
