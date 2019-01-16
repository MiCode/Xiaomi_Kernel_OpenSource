#if defined(MT6620)
#include "mt6620.h"
#include "mt6620_reg.h"
#elif defined(MT6628)
#include "mt6628.h"
#include "mt6628_reg_copy.h"
#endif
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>

#ifndef ALIGN_4
#define ALIGN_4(_value)             (((_value) + 3) & ~3u)
#endif				/* ALIGN_4 */

#define BUILD_SIGN(ch0, ch1, ch2, ch3) \
		((uint32_t)(uint8_t)(ch0) | ((uint32_t)(uint8_t)(ch1) << 8) |   \
		 ((uint32_t)(uint8_t)(ch2) << 16) | ((uint32_t)(uint8_t)(ch3) << 24))

#define MTK_WIFI_SIGNATURE BUILD_SIGN('M', 'T', 'K', 'W')
#define CMD_PKT_SIZE_FOR_IMAGE              2048	/* !< 2048 Bytes CMD payload buffer */

#define RETRY_ITERATION     (512)

typedef enum _ENUM_INIT_CMD_ID {
	INIT_CMD_ID_DOWNLOAD_BUF = 1,
	INIT_CMD_ID_WIFI_START,
	INIT_CMD_ID_ACCESS_REG,
	INIT_CMD_ID_QUERY_PENDING_ERROR
} ENUM_INIT_CMD_ID, *P_ENUM_INIT_CMD_ID;


typedef struct _INIT_WIFI_CMD_T {
	uint8_t ucCID;
	uint8_t ucSeqNum;
	uint16_t u2Reserved;
	uint8_t aucBuffer[0];
} INIT_WIFI_CMD_T, *P_INIT_WIFI_CMD_T;

typedef struct _INIT_HIF_TX_HEADER_T {
	uint16_t u2TxByteCount;
	uint8_t ucEtherTypeOffset;
	uint8_t ucCSflags;
	INIT_WIFI_CMD_T rInitWifiCmd;
} INIT_HIF_TX_HEADER_T, *P_INIT_HIF_TX_HEADER_T;

typedef struct _INIT_CMD_DOWNLOAD_BUF {
	uint32_t u4Address;
	uint32_t u4Length;
	uint32_t u4CRC32;
	uint32_t u4DataMode;
	uint8_t aucBuffer[0];
} INIT_CMD_DOWNLOAD_BUF, *P_INIT_CMD_DOWNLOAD_BUF;

typedef struct _INIT_CMD_WIFI_START {
	uint32_t u4Override;
	uint32_t u4Address;
} INIT_CMD_WIFI_START, *P_INIT_CMD_WIFI_START;

/* for divided firmware loading */
typedef struct _FWDL_SECTION_INFO_T {
	uint32_t u4Offset;
	uint32_t u4Reserved;
	uint32_t u4Length;
	uint32_t u4DestAddr;
} FWDL_SECTION_INFO_T, *P_FWDL_SECTION_INFO_T;

typedef struct _FIRMWARE_DIVIDED_DOWNLOAD_T {
	uint32_t u4Signature;
	uint32_t u4CRC;		/* CRC calculated without first 8 bytes included */
	uint32_t u4NumOfEntries;
	uint32_t u4Reserved;
	FWDL_SECTION_INFO_T arSection[];
} FIRMWARE_DIVIDED_DOWNLOAD_T, *P_FIRMWARE_DIVIDED_DOWNLOAD_T;


/* data structures for normal mode operation */
typedef struct _WIFI_CMD_T {
	uint16_t u2TxByteCount_UserPriority;
	uint8_t ucEtherTypeOffset;
	uint8_t ucResource_PktType_CSflags;
	uint8_t ucCID;
	uint8_t ucSetQuery;
	uint8_t ucSeqNum;
	uint8_t aucReserved2;

	uint8_t aucBuffer[0];
} WIFI_CMD_T, *P_WIFI_CMD_T;

typedef struct _CMD_NIC_POWER_CTRL {
	uint8_t ucPowerMode;
	uint8_t aucReserved[3];
} CMD_NIC_POWER_CTRL, *P_CMD_NIC_POWER_CTRL;

uint32_t wlanCRC32(uint8_t *buf, uint32_t len)
{
	uint32_t i, crc32 = 0xFFFFFFFF;
	const uint32_t crc32_ccitt_table[256] = {
		0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419,
		0x706af48f, 0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4,
		0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07,
		0x90bf1d91, 0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
		0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856,
		0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
		0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4,
		0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
		0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3,
		0x45df5c75, 0xdcd60dcf, 0xabd13d59, 0x26d930ac, 0x51de003a,
		0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599,
		0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
		0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190,
		0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f,
		0x9fbfe4a5, 0xe8b8d433, 0x7807c9a2, 0x0f00f934, 0x9609a88e,
		0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
		0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed,
		0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
		0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3,
		0xfbd44c65, 0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
		0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a,
		0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5,
		0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa, 0xbe0b1010,
		0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
		0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17,
		0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6,
		0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615,
		0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
		0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1, 0xf00f9344,
		0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
		0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a,
		0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
		0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1,
		0xa6bc5767, 0x3fb506dd, 0x48b2364b, 0xd80d2bda, 0xaf0a1b4c,
		0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef,
		0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
		0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe,
		0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31,
		0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226, 0x756aa39c,
		0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
		0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b,
		0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
		0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1,
		0x18b74777, 0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
		0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45, 0xa00ae278,
		0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7,
		0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 0x40df0b66,
		0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
		0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605,
		0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8,
		0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b,
		0x2d02ef8d
	};

	for (i = 0; i < len; i++)
		crc32 = crc32_ccitt_table[(crc32 ^ buf[i]) & 0xff] ^ (crc32 >> 8);

	return (~crc32);
}

/* for low-level file access */
static struct file *filp;
static uid_t orgfsuid;
static gid_t orgfsgid;
static mm_segment_t orgfs;

#if defined(MT6620) || defined(MT6628)
int firmware_download(char *filename, MTK_WCN_HIF_SDIO_CLTCTX cltCtx)
{
	int i, j, k, ret, err, retValue = -1;
	char *data = NULL, *data_head = NULL;
	unsigned long len;
	uint8_t ucSeqNum = 0;
#if defined(MT6620)
	uint32_t u4DestAddr = 0x10008000;
#elif defined(MT6628)
	uint32_t u4DestAddr = 0x00060000;
#endif
	uint32_t transSize;
	P_INIT_HIF_TX_HEADER_T prInitHifTxHeader = NULL;
	P_INIT_CMD_DOWNLOAD_BUF prInitCmdDownloadBuf;
	P_INIT_CMD_WIFI_START prInitCmdWifiStart;
	uint8_t ucMaxNumOfBuffer = 8;
	uint8_t ucFreeBufferCount = 8;
	P_FIRMWARE_DIVIDED_DOWNLOAD_T prFwHead;
	int ValidHead;
	const uint32_t u4CRCOffset = offsetof(FIRMWARE_DIVIDED_DOWNLOAD_T, u4NumOfEntries);

	struct cred *cred = (struct cred *)get_current_cred();

#if 1
	printk("%s(%s)\n", __func__, filename);
#endif

	orgfsuid = cred->fsuid;
	orgfsgid = cred->fsgid;
	/* change to superuser */
	cred->fsuid = cred->fsgid = 0;

	orgfs = get_fs();
	set_fs(get_ds());

	/* ACQUIRE FW-OWN */
	for (i = 0; i < RETRY_ITERATION; i++) {
		err = mtk_wcn_hif_sdio_readl(cltCtx, MCR_WHLPCR, &ret);
		if ((err == 0) && ((ret & 0x100) == 0x100)) {
			printk(KERN_WARNING "firmware own acquired\n");
			break;
		} else {
			err = mtk_wcn_hif_sdio_writel(cltCtx, MCR_WHLPCR, 0x200);
			mdelay(10);
		}
	}

	if (i == RETRY_ITERATION) {
		printk(KERN_WARNING "firmware own NOT acquired\n");
		goto fail_firmware;
	}

	/* open firmware image for download */
	filp = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		printk(KERN_WARNING "Failed to open %s!\n", filename);
		goto fail_firmware;
	}

	/* load firmware image */
	len = filp->f_path.dentry->d_inode->i_size;
	data = data_head = vmalloc(ALIGN_4(len));

	if (data == NULL) {
		printk("[MT662x] memory allocation error! (%s:%d)\n", __FILE__, __LINE__);
	}

	filp->f_op->read(filp, data, len, &filp->f_pos);

	prInitHifTxHeader = kmalloc(4096, GFP_ATOMIC);
	if (prInitHifTxHeader == NULL) {
		goto fail_memory;
	}

	/* read for header */
	prFwHead = (P_FIRMWARE_DIVIDED_DOWNLOAD_T) data;
	if (prFwHead->u4Signature == MTK_WIFI_SIGNATURE &&
	    prFwHead->u4CRC == wlanCRC32((uint8_t *) data + u4CRCOffset, len - u4CRCOffset)) {
		ValidHead = 1;
		printk("[MT6620] == DIVIDED DOWNLOAD ==\n");
	} else {
		ValidHead = 0;
		printk("[loopback][MT6620] == SINGLE DOWNLOAD ==\n");
	}

	if (ValidHead == 1) {
		for (i = 0; i < prFwHead->u4NumOfEntries; i++) {
			for (j = 0; j < prFwHead->arSection[i].u4Length;
			     j += CMD_PKT_SIZE_FOR_IMAGE) {
				int translen;

				if (j + CMD_PKT_SIZE_FOR_IMAGE < prFwHead->arSection[i].u4Length) {
					translen = CMD_PKT_SIZE_FOR_IMAGE;
				} else {
					translen = prFwHead->arSection[i].u4Length - j;
				}

				/* memory zero */
				memset(prInitHifTxHeader, 0, 4096);

				/* prepare init command .. */
				prInitHifTxHeader->u2TxByteCount =
				    ALIGN_4(sizeof(INIT_HIF_TX_HEADER_T) +
					    sizeof(INIT_CMD_DOWNLOAD_BUF) + translen);
				prInitHifTxHeader->ucEtherTypeOffset = 0;
				prInitHifTxHeader->ucCSflags = 0;
				prInitHifTxHeader->rInitWifiCmd.ucCID = INIT_CMD_ID_DOWNLOAD_BUF;
				prInitHifTxHeader->rInitWifiCmd.ucSeqNum = ucSeqNum;

				prInitCmdDownloadBuf =
				    (P_INIT_CMD_DOWNLOAD_BUF) (prInitHifTxHeader->rInitWifiCmd.
							       aucBuffer);
				prInitCmdDownloadBuf->u4Address =
				    prFwHead->arSection[i].u4DestAddr + j;
				prInitCmdDownloadBuf->u4Length = translen;
				prInitCmdDownloadBuf->u4CRC32 =
				    wlanCRC32((uint8_t *) (data + prFwHead->arSection[i].u4Offset +
							   j), translen);
				prInitCmdDownloadBuf->u4DataMode = 0 | 0x1;	/* enable encryption */

				memcpy(prInitCmdDownloadBuf->aucBuffer,
				       (uint8_t *) (data + prFwHead->arSection[i].u4Offset + j),
				       translen);

				/* acquire TX resource */
				if (ucFreeBufferCount == 0) {
					/* for(i = 0 ; i < 2000 ; i++) { */
					for (k = 0; k < 256; k++) {
						err =
						    mtk_wcn_hif_sdio_readl(cltCtx, MCR_WTSR0, &ret);
						if (err == 0) {
							ucFreeBufferCount += ret & 0xff;
							if (ucFreeBufferCount > 0) {
								break;
							}
						}
						/* udelay(10); */
						msleep(50);
					}

					if (ucFreeBufferCount > ucMaxNumOfBuffer) {
						printk(KERN_WARNING
						       "[FWDL] TX resource = %d > %d..\n",
						       ucFreeBufferCount, ucMaxNumOfBuffer);
						goto exit;
					} else if (ucFreeBufferCount == 0) {
						printk(KERN_WARNING "[FWDL] no TX resource ..\n");
						goto exit;
					}
				}
				/* send */
				ucFreeBufferCount--;
				transSize = prInitHifTxHeader->u2TxByteCount;
				if (transSize % 512) {
					transSize += (512 - (transSize % 512));
				}

				mtk_wcn_hif_sdio_write_buf(cltCtx, MCR_WTDR0,
							   (PUINT32) (prInitHifTxHeader),
							   transSize);

				ucSeqNum++;
			}
		}
	} else {
		/* upload firmware */
		while (len > 0) {
			int translen =
			    (len > CMD_PKT_SIZE_FOR_IMAGE) ? CMD_PKT_SIZE_FOR_IMAGE : len;

			/* memory zero */
			memset(prInitHifTxHeader, 0, 4096);

			/* prepare init command .. */
			prInitHifTxHeader->u2TxByteCount =
			    ALIGN_4(sizeof(INIT_HIF_TX_HEADER_T) + sizeof(INIT_CMD_DOWNLOAD_BUF) +
				    translen);
			prInitHifTxHeader->ucEtherTypeOffset = 0;
			prInitHifTxHeader->ucCSflags = 0;
			prInitHifTxHeader->rInitWifiCmd.ucCID = INIT_CMD_ID_DOWNLOAD_BUF;
			prInitHifTxHeader->rInitWifiCmd.ucSeqNum = ucSeqNum;

			prInitCmdDownloadBuf =
			    (P_INIT_CMD_DOWNLOAD_BUF) (prInitHifTxHeader->rInitWifiCmd.aucBuffer);
			prInitCmdDownloadBuf->u4Address = u4DestAddr;
			prInitCmdDownloadBuf->u4Length = translen;
			prInitCmdDownloadBuf->u4CRC32 = wlanCRC32((uint8_t *) data, translen);
			prInitCmdDownloadBuf->u4DataMode = 0 | 0x1;	/* enable encryption */

			memcpy(prInitCmdDownloadBuf->aucBuffer, data, translen);

			/* acquire TX resource */
			if (ucFreeBufferCount == 0) {
				/* for(i = 0 ; i < 2000 ; i++) { */
				for (i = 0; i < 256; i++) {
					err = mtk_wcn_hif_sdio_readl(cltCtx, MCR_WTSR0, &ret);
					if (err == 0) {
						ucFreeBufferCount += ret & 0xff;
						if (ucFreeBufferCount > 0) {
							break;
						}
					}
					/* udelay(10); */
					msleep(50);
				}

				if (ucFreeBufferCount > ucMaxNumOfBuffer) {
					printk(KERN_WARNING "[FWDL] TX resource = %d > %d..\n",
					       ucFreeBufferCount, ucMaxNumOfBuffer);
					goto exit;
				} else if (ucFreeBufferCount == 0) {
					printk(KERN_WARNING "[FWDL] no TX resource ..\n");
					goto exit;
				}
			}
			/* send */
			ucFreeBufferCount--;
			transSize = prInitHifTxHeader->u2TxByteCount;
			if (transSize % 512) {
				transSize += (512 - (transSize % 512));
			}

			mtk_wcn_hif_sdio_write_buf(cltCtx, MCR_WTDR0, (PUINT32) (prInitHifTxHeader),
						   transSize);

			/* increasing */
			u4DestAddr += translen;
			ucSeqNum++;

			len -= translen;
			data += translen;
		}
	}

	/* send WLAN_START command */
	/* memory zero */
	memset(prInitHifTxHeader, 0, 4096);

	/* prepare WLAN_START command */
	prInitHifTxHeader->u2TxByteCount =
	    ALIGN_4(sizeof(INIT_HIF_TX_HEADER_T) + sizeof(INIT_CMD_WIFI_START));
	prInitHifTxHeader->ucEtherTypeOffset = 0;
	prInitHifTxHeader->ucCSflags = 0;
	prInitHifTxHeader->rInitWifiCmd.ucCID = INIT_CMD_ID_WIFI_START;
	prInitHifTxHeader->rInitWifiCmd.ucSeqNum = ucSeqNum;

	prInitCmdWifiStart = (P_INIT_CMD_WIFI_START) (prInitHifTxHeader->rInitWifiCmd.aucBuffer);
	prInitCmdWifiStart->u4Override = 0;
	prInitCmdWifiStart->u4Address = 0;

	/* acquire TX resource */
	if (ucFreeBufferCount == 0) {
		for (i = 0; i < 2000; i++) {
			err = mtk_wcn_hif_sdio_readl(cltCtx, MCR_WTSR0, &ret);
			if (err == 0) {
				ucFreeBufferCount += ret & 0xff;
				if (ucFreeBufferCount > 0) {
					break;
				}
			}

			udelay(10);
		}

		if (ucFreeBufferCount > ucMaxNumOfBuffer) {
			printk(KERN_WARNING "[START] TX resource = %d > %d..\n", ucFreeBufferCount,
			       ucMaxNumOfBuffer);
			goto exit;
		} else if (ucFreeBufferCount == 0) {
			printk(KERN_WARNING "[START] no TX resource ..\n");
			goto exit;
		}
	}
	/* send it out */
	transSize = prInitHifTxHeader->u2TxByteCount;
	mtk_wcn_hif_sdio_write_buf(cltCtx, MCR_WTDR0, (PUINT32) (prInitHifTxHeader), transSize);

	/* wait for ready bit */
	for (i = 0; i < RETRY_ITERATION; i++) {
		err = mtk_wcn_hif_sdio_readl(cltCtx, MCR_WCIR, &ret);
		if ((err == 0) && ((ret & (0x1 << 21)) == (0x1 << 21))) {
			printk("[FWDL] Ready bit asserted (0x%08X)\n", ret);
			retValue = 0;
			break;
		}
		mdelay(10);
	}

 exit:
	/* free buffer */
	if (data_head) {
		vfree(data_head);
	}
	if (prInitHifTxHeader) {
		kfree(prInitHifTxHeader);
	}

 fail_memory:
	/* release firmware */
	if ((filp != NULL) && !IS_ERR(filp)) {
		/* close firmware file */
		filp_close(filp, NULL);
	}

 fail_firmware:
	/* restore */
	set_fs(orgfs);
	cred->fsuid = orgfsuid;
	cred->fsgid = orgfsgid;

	return retValue;
}

#elif defined(MT5931)
#else
#error
#endif

int firmware_power_off(MTK_WCN_HIF_SDIO_CLTCTX cltCtx)
{
	int i, err, ret;
	P_WIFI_CMD_T prWifiCmd = NULL;

	/* 1. command buffer allocation */
	prWifiCmd = kmalloc(sizeof(WIFI_CMD_T) + sizeof(CMD_NIC_POWER_CTRL), GFP_ATOMIC);

	/* 2. memory zero */
	memset(prWifiCmd, 0, sizeof(WIFI_CMD_T) + sizeof(CMD_NIC_POWER_CTRL));

	/* 3. prepare POWER CTRL command */
	prWifiCmd->u2TxByteCount_UserPriority = sizeof(WIFI_CMD_T) + sizeof(CMD_NIC_POWER_CTRL);
	prWifiCmd->ucCID = 0x05;	/* CMD_ID_NIC_POWER_CTRL */
	prWifiCmd->ucSetQuery = 1;	/* TRUE */
	prWifiCmd->ucSeqNum = 0;
	((P_CMD_NIC_POWER_CTRL) (prWifiCmd->aucBuffer))->ucPowerMode = 1;	/* for turning off */

	/* 4. send CMD_ID_NIC_POWER_CTRL */
	mtk_wcn_hif_sdio_write_buf(cltCtx, MCR_WTDR1, (PUINT32) (prWifiCmd),
				   sizeof(WIFI_CMD_T) + sizeof(CMD_NIC_POWER_CTRL));

	/* 5. wait for ready bit de-assertion */
	for (i = 0; i < RETRY_ITERATION; i++) {
		err = mtk_wcn_hif_sdio_readl(cltCtx, MCR_WCIR, &ret);
		if ((err == 0) && ((ret & (0x1 << 21)) == 0)) {
			printk("[FWDL] Ready bit de-asserted (0x%08X)\n", ret);
			break;
		}
		mdelay(10);
	}

	/* 6. Set Onwership to F/W */
	mtk_wcn_hif_sdio_writel(cltCtx, MCR_WHLPCR, 0x100);

	return 0;
}
