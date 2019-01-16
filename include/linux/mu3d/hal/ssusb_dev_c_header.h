/* SSUSB_DEV REGISTER DEFINITION */

#define U3D_LV1ISR                                (SSUSB_DEV_BASE+0x0000)
#define U3D_LV1IER                                (SSUSB_DEV_BASE+0x0004)
#define U3D_LV1IESR                               (SSUSB_DEV_BASE+0x0008)
#define U3D_LV1IECR                               (SSUSB_DEV_BASE+0x000C)
#define U3D_AXI_WR_DMA_CFG                        (SSUSB_DEV_BASE+0x0020)
#define U3D_AXI_RD_DMA_CFG                        (SSUSB_DEV_BASE+0x0024)
#define U3D_MAC_U1_EN_CTRL                        (SSUSB_DEV_BASE+0x0030)
#define U3D_MAC_U2_EN_CTRL                        (SSUSB_DEV_BASE+0x0034)
#define U3D_SRAM_DBG_CTRL                         (SSUSB_DEV_BASE+0x0040)
#define U3D_SRAM_DBG_CTRL_1                       (SSUSB_DEV_BASE+0x0044)
#define U3D_RISC_SIZE                             (SSUSB_DEV_BASE+0x0050)
#define U3D_WRBUF_ERR_STS                         (SSUSB_DEV_BASE+0x0070)
#define U3D_BUF_ERR_EN                            (SSUSB_DEV_BASE+0x0074)
#define U3D_EPISR                                 (SSUSB_DEV_BASE+0x0080)
#define U3D_EPIER                                 (SSUSB_DEV_BASE+0x0084)
#define U3D_EPIESR                                (SSUSB_DEV_BASE+0x0088)
#define U3D_EPIECR                                (SSUSB_DEV_BASE+0x008C)
#define U3D_DMAISR                                (SSUSB_DEV_BASE+0x0090)
#define U3D_DMAIER                                (SSUSB_DEV_BASE+0x0094)
#define U3D_DMAIESR                               (SSUSB_DEV_BASE+0x0098)
#define U3D_DMAIECR                               (SSUSB_DEV_BASE+0x009C)
#define U3D_EP0DMACTRL                            (SSUSB_DEV_BASE+0x00C0)
#define U3D_EP0DMASTRADDR                         (SSUSB_DEV_BASE+0x00C4)
#define U3D_EP0DMATFRCOUNT                        (SSUSB_DEV_BASE+0x00C8)
#define U3D_EP0DMARLCOUNT                         (SSUSB_DEV_BASE+0x00CC)
#define U3D_TXDMACTRL                             (SSUSB_DEV_BASE+0x00D0)
#define U3D_TXDMASTRADDR                          (SSUSB_DEV_BASE+0x00D4)
#define U3D_TXDMATRDCNT                           (SSUSB_DEV_BASE+0x00D8)
#define U3D_TXDMARLCOUNT                          (SSUSB_DEV_BASE+0x00DC)
#define U3D_RXDMACTRL                             (SSUSB_DEV_BASE+0x00E0)
#define U3D_RXDMASTRADDR                          (SSUSB_DEV_BASE+0x00E4)
#define U3D_RXDMATRDCNT                           (SSUSB_DEV_BASE+0x00E8)
#define U3D_RXDMARLCOUNT                          (SSUSB_DEV_BASE+0x00EC)
#define U3D_EP0CSR                                (SSUSB_DEV_BASE+0x0100)
#define U3D_RXCOUNT0                              (SSUSB_DEV_BASE+0x0108)
#define U3D_RESERVED                              (SSUSB_DEV_BASE+0x010C)
#define U3D_TX1CSR0                               (SSUSB_DEV_BASE+0x0110)
#define U3D_TX1CSR1                               (SSUSB_DEV_BASE+0x0114)
#define U3D_TX1CSR2                               (SSUSB_DEV_BASE+0x0118)
#define U3D_TX2CSR0                               (SSUSB_DEV_BASE+0x0120)
#define U3D_TX2CSR1                               (SSUSB_DEV_BASE+0x0124)
#define U3D_TX2CSR2                               (SSUSB_DEV_BASE+0x0128)
#define U3D_TX3CSR0                               (SSUSB_DEV_BASE+0x0130)
#define U3D_TX3CSR1                               (SSUSB_DEV_BASE+0x0134)
#define U3D_TX3CSR2                               (SSUSB_DEV_BASE+0x0138)
#define U3D_TX4CSR0                               (SSUSB_DEV_BASE+0x0140)
#define U3D_TX4CSR1                               (SSUSB_DEV_BASE+0x0144)
#define U3D_TX4CSR2                               (SSUSB_DEV_BASE+0x0148)
#define U3D_TX5CSR0                               (SSUSB_DEV_BASE+0x0150)
#define U3D_TX5CSR1                               (SSUSB_DEV_BASE+0x0154)
#define U3D_TX5CSR2                               (SSUSB_DEV_BASE+0x0158)
#define U3D_TX6CSR0                               (SSUSB_DEV_BASE+0x0160)
#define U3D_TX6CSR1                               (SSUSB_DEV_BASE+0x0164)
#define U3D_TX6CSR2                               (SSUSB_DEV_BASE+0x0168)
#define U3D_TX7CSR0                               (SSUSB_DEV_BASE+0x0170)
#define U3D_TX7CSR1                               (SSUSB_DEV_BASE+0x0174)
#define U3D_TX7CSR2                               (SSUSB_DEV_BASE+0x0178)
#define U3D_TX8CSR0                               (SSUSB_DEV_BASE+0x0180)
#define U3D_TX8CSR1                               (SSUSB_DEV_BASE+0x0184)
#define U3D_TX8CSR2                               (SSUSB_DEV_BASE+0x0188)
#define U3D_TX9CSR0                               (SSUSB_DEV_BASE+0x0190)
#define U3D_TX9CSR1                               (SSUSB_DEV_BASE+0x0194)
#define U3D_TX9CSR2                               (SSUSB_DEV_BASE+0x0198)
#define U3D_TX10CSR0                              (SSUSB_DEV_BASE+0x01A0)
#define U3D_TX10CSR1                              (SSUSB_DEV_BASE+0x01A4)
#define U3D_TX10CSR2                              (SSUSB_DEV_BASE+0x01A8)
#define U3D_TX11CSR0                              (SSUSB_DEV_BASE+0x01B0)
#define U3D_TX11CSR1                              (SSUSB_DEV_BASE+0x01B4)
#define U3D_TX11CSR2                              (SSUSB_DEV_BASE+0x01B8)
#define U3D_TX12CSR0                              (SSUSB_DEV_BASE+0x01C0)
#define U3D_TX12CSR1                              (SSUSB_DEV_BASE+0x01C4)
#define U3D_TX12CSR2                              (SSUSB_DEV_BASE+0x01C8)
#define U3D_TX13CSR0                              (SSUSB_DEV_BASE+0x01D0)
#define U3D_TX13CSR1                              (SSUSB_DEV_BASE+0x01D4)
#define U3D_TX13CSR2                              (SSUSB_DEV_BASE+0x01D8)
#define U3D_TX14CSR0                              (SSUSB_DEV_BASE+0x01E0)
#define U3D_TX14CSR1                              (SSUSB_DEV_BASE+0x01E4)
#define U3D_TX14CSR2                              (SSUSB_DEV_BASE+0x01E8)
#define U3D_TX15CSR0                              (SSUSB_DEV_BASE+0x01F0)
#define U3D_TX15CSR1                              (SSUSB_DEV_BASE+0x01F4)
#define U3D_TX15CSR2                              (SSUSB_DEV_BASE+0x01F8)
#define U3D_RX1CSR0                               (SSUSB_DEV_BASE+0x0210)
#define U3D_RX1CSR1                               (SSUSB_DEV_BASE+0x0214)
#define U3D_RX1CSR2                               (SSUSB_DEV_BASE+0x0218)
#define U3D_RX1CSR3                               (SSUSB_DEV_BASE+0x021C)
#define U3D_RX2CSR0                               (SSUSB_DEV_BASE+0x0220)
#define U3D_RX2CSR1                               (SSUSB_DEV_BASE+0x0224)
#define U3D_RX2CSR2                               (SSUSB_DEV_BASE+0x0228)
#define U3D_RX2CSR3                               (SSUSB_DEV_BASE+0x022C)
#define U3D_RX3CSR0                               (SSUSB_DEV_BASE+0x0230)
#define U3D_RX3CSR1                               (SSUSB_DEV_BASE+0x0234)
#define U3D_RX3CSR2                               (SSUSB_DEV_BASE+0x0238)
#define U3D_RX3CSR3                               (SSUSB_DEV_BASE+0x023C)
#define U3D_RX4CSR0                               (SSUSB_DEV_BASE+0x0240)
#define U3D_RX4CSR1                               (SSUSB_DEV_BASE+0x0244)
#define U3D_RX4CSR2                               (SSUSB_DEV_BASE+0x0248)
#define U3D_RX4CSR3                               (SSUSB_DEV_BASE+0x024C)
#define U3D_RX5CSR0                               (SSUSB_DEV_BASE+0x0250)
#define U3D_RX5CSR1                               (SSUSB_DEV_BASE+0x0254)
#define U3D_RX5CSR2                               (SSUSB_DEV_BASE+0x0258)
#define U3D_RX5CSR3                               (SSUSB_DEV_BASE+0x025C)
#define U3D_RX6CSR0                               (SSUSB_DEV_BASE+0x0260)
#define U3D_RX6CSR1                               (SSUSB_DEV_BASE+0x0264)
#define U3D_RX6CSR2                               (SSUSB_DEV_BASE+0x0268)
#define U3D_RX6CSR3                               (SSUSB_DEV_BASE+0x026C)
#define U3D_RX7CSR0                               (SSUSB_DEV_BASE+0x0270)
#define U3D_RX7CSR1                               (SSUSB_DEV_BASE+0x0274)
#define U3D_RX7CSR2                               (SSUSB_DEV_BASE+0x0278)
#define U3D_RX7CSR3                               (SSUSB_DEV_BASE+0x027C)
#define U3D_RX8CSR0                               (SSUSB_DEV_BASE+0x0280)
#define U3D_RX8CSR1                               (SSUSB_DEV_BASE+0x0284)
#define U3D_RX8CSR2                               (SSUSB_DEV_BASE+0x0288)
#define U3D_RX8CSR3                               (SSUSB_DEV_BASE+0x028C)
#define U3D_RX9CSR0                               (SSUSB_DEV_BASE+0x0290)
#define U3D_RX9CSR1                               (SSUSB_DEV_BASE+0x0294)
#define U3D_RX9CSR2                               (SSUSB_DEV_BASE+0x0298)
#define U3D_RX9CSR3                               (SSUSB_DEV_BASE+0x029C)
#define U3D_RX10CSR0                              (SSUSB_DEV_BASE+0x02A0)
#define U3D_RX10CSR1                              (SSUSB_DEV_BASE+0x02A4)
#define U3D_RX10CSR2                              (SSUSB_DEV_BASE+0x02A8)
#define U3D_RX10CSR3                              (SSUSB_DEV_BASE+0x02AC)
#define U3D_RX11CSR0                              (SSUSB_DEV_BASE+0x02B0)
#define U3D_RX11CSR1                              (SSUSB_DEV_BASE+0x02B4)
#define U3D_RX11CSR2                              (SSUSB_DEV_BASE+0x02B8)
#define U3D_RX11CSR3                              (SSUSB_DEV_BASE+0x02BC)
#define U3D_RX12CSR0                              (SSUSB_DEV_BASE+0x02C0)
#define U3D_RX12CSR1                              (SSUSB_DEV_BASE+0x02C4)
#define U3D_RX12CSR2                              (SSUSB_DEV_BASE+0x02C8)
#define U3D_RX12CSR3                              (SSUSB_DEV_BASE+0x02CC)
#define U3D_RX13CSR0                              (SSUSB_DEV_BASE+0x02D0)
#define U3D_RX13CSR1                              (SSUSB_DEV_BASE+0x02D4)
#define U3D_RX13CSR2                              (SSUSB_DEV_BASE+0x02D8)
#define U3D_RX13CSR3                              (SSUSB_DEV_BASE+0x02DC)
#define U3D_RX14CSR0                              (SSUSB_DEV_BASE+0x02E0)
#define U3D_RX14CSR1                              (SSUSB_DEV_BASE+0x02E4)
#define U3D_RX14CSR2                              (SSUSB_DEV_BASE+0x02E8)
#define U3D_RX14CSR3                              (SSUSB_DEV_BASE+0x02EC)
#define U3D_RX15CSR0                              (SSUSB_DEV_BASE+0x02F0)
#define U3D_RX15CSR1                              (SSUSB_DEV_BASE+0x02F4)
#define U3D_RX15CSR2                              (SSUSB_DEV_BASE+0x02F8)
#define U3D_RX15CSR3                              (SSUSB_DEV_BASE+0x02FC)
#define U3D_FIFO0                                 (SSUSB_DEV_BASE+0x0300)
#define U3D_FIFO1                                 (SSUSB_DEV_BASE+0x0310)
#define U3D_FIFO2                                 (SSUSB_DEV_BASE+0x0320)
#define U3D_FIFO3                                 (SSUSB_DEV_BASE+0x0330)
#define U3D_FIFO4                                 (SSUSB_DEV_BASE+0x0340)
#define U3D_FIFO5                                 (SSUSB_DEV_BASE+0x0350)
#define U3D_FIFO6                                 (SSUSB_DEV_BASE+0x0360)
#define U3D_FIFO7                                 (SSUSB_DEV_BASE+0x0370)
#define U3D_FIFO8                                 (SSUSB_DEV_BASE+0x0380)
#define U3D_FIFO9                                 (SSUSB_DEV_BASE+0x0390)
#define U3D_FIFO10                                (SSUSB_DEV_BASE+0x03A0)
#define U3D_FIFO11                                (SSUSB_DEV_BASE+0x03B0)
#define U3D_FIFO12                                (SSUSB_DEV_BASE+0x03C0)
#define U3D_FIFO13                                (SSUSB_DEV_BASE+0x03D0)
#define U3D_FIFO14                                (SSUSB_DEV_BASE+0x03E0)
#define U3D_FIFO15                                (SSUSB_DEV_BASE+0x03F0)
#define U3D_QCR0                                  (SSUSB_DEV_BASE+0x0400)
#define U3D_QCR1                                  (SSUSB_DEV_BASE+0x0404)
#define U3D_QCR2                                  (SSUSB_DEV_BASE+0x0408)
#define U3D_QCR3                                  (SSUSB_DEV_BASE+0x040C)
#define U3D_QGCSR                                 (SSUSB_DEV_BASE+0x0410)
#define U3D_TXQCSR1                               (SSUSB_DEV_BASE+0x0510)
#define U3D_TXQSAR1                               (SSUSB_DEV_BASE+0x0514)
#define U3D_TXQCPR1                               (SSUSB_DEV_BASE+0x0518)
#define U3D_TXQCSR2                               (SSUSB_DEV_BASE+0x0520)
#define U3D_TXQSAR2                               (SSUSB_DEV_BASE+0x0524)
#define U3D_TXQCPR2                               (SSUSB_DEV_BASE+0x0528)
#define U3D_TXQCSR3                               (SSUSB_DEV_BASE+0x0530)
#define U3D_TXQSAR3                               (SSUSB_DEV_BASE+0x0534)
#define U3D_TXQCPR3                               (SSUSB_DEV_BASE+0x0538)
#define U3D_TXQCSR4                               (SSUSB_DEV_BASE+0x0540)
#define U3D_TXQSAR4                               (SSUSB_DEV_BASE+0x0544)
#define U3D_TXQCPR4                               (SSUSB_DEV_BASE+0x0548)
#define U3D_TXQCSR5                               (SSUSB_DEV_BASE+0x0550)
#define U3D_TXQSAR5                               (SSUSB_DEV_BASE+0x0554)
#define U3D_TXQCPR5                               (SSUSB_DEV_BASE+0x0558)
#define U3D_TXQCSR6                               (SSUSB_DEV_BASE+0x0560)
#define U3D_TXQSAR6                               (SSUSB_DEV_BASE+0x0564)
#define U3D_TXQCPR6                               (SSUSB_DEV_BASE+0x0568)
#define U3D_TXQCSR7                               (SSUSB_DEV_BASE+0x0570)
#define U3D_TXQSAR7                               (SSUSB_DEV_BASE+0x0574)
#define U3D_TXQCPR7                               (SSUSB_DEV_BASE+0x0578)
#define U3D_TXQCSR8                               (SSUSB_DEV_BASE+0x0580)
#define U3D_TXQSAR8                               (SSUSB_DEV_BASE+0x0584)
#define U3D_TXQCPR8                               (SSUSB_DEV_BASE+0x0588)
#define U3D_TXQCSR9                               (SSUSB_DEV_BASE+0x0590)
#define U3D_TXQSAR9                               (SSUSB_DEV_BASE+0x0594)
#define U3D_TXQCPR9                               (SSUSB_DEV_BASE+0x0598)
#define U3D_TXQCSR10                              (SSUSB_DEV_BASE+0x05A0)
#define U3D_TXQSAR10                              (SSUSB_DEV_BASE+0x05A4)
#define U3D_TXQCPR10                              (SSUSB_DEV_BASE+0x05A8)
#define U3D_TXQCSR11                              (SSUSB_DEV_BASE+0x05B0)
#define U3D_TXQSAR11                              (SSUSB_DEV_BASE+0x05B4)
#define U3D_TXQCPR11                              (SSUSB_DEV_BASE+0x05B8)
#define U3D_TXQCSR12                              (SSUSB_DEV_BASE+0x05C0)
#define U3D_TXQSAR12                              (SSUSB_DEV_BASE+0x05C4)
#define U3D_TXQCPR12                              (SSUSB_DEV_BASE+0x05C8)
#define U3D_TXQCSR13                              (SSUSB_DEV_BASE+0x05D0)
#define U3D_TXQSAR13                              (SSUSB_DEV_BASE+0x05D4)
#define U3D_TXQCPR13                              (SSUSB_DEV_BASE+0x05D8)
#define U3D_TXQCSR14                              (SSUSB_DEV_BASE+0x05E0)
#define U3D_TXQSAR14                              (SSUSB_DEV_BASE+0x05E4)
#define U3D_TXQCPR14                              (SSUSB_DEV_BASE+0x05E8)
#define U3D_TXQCSR15                              (SSUSB_DEV_BASE+0x05F0)
#define U3D_TXQSAR15                              (SSUSB_DEV_BASE+0x05F4)
#define U3D_TXQCPR15                              (SSUSB_DEV_BASE+0x05F8)
#define U3D_RXQCSR1                               (SSUSB_DEV_BASE+0x0610)
#define U3D_RXQSAR1                               (SSUSB_DEV_BASE+0x0614)
#define U3D_RXQCPR1                               (SSUSB_DEV_BASE+0x0618)
#define U3D_RXQLDPR1                              (SSUSB_DEV_BASE+0x061C)
#define U3D_RXQCSR2                               (SSUSB_DEV_BASE+0x0620)
#define U3D_RXQSAR2                               (SSUSB_DEV_BASE+0x0624)
#define U3D_RXQCPR2                               (SSUSB_DEV_BASE+0x0628)
#define U3D_RXQLDPR2                              (SSUSB_DEV_BASE+0x062C)
#define U3D_RXQCSR3                               (SSUSB_DEV_BASE+0x0630)
#define U3D_RXQSAR3                               (SSUSB_DEV_BASE+0x0634)
#define U3D_RXQCPR3                               (SSUSB_DEV_BASE+0x0638)
#define U3D_RXQLDPR3                              (SSUSB_DEV_BASE+0x063C)
#define U3D_RXQCSR4                               (SSUSB_DEV_BASE+0x0640)
#define U3D_RXQSAR4                               (SSUSB_DEV_BASE+0x0644)
#define U3D_RXQCPR4                               (SSUSB_DEV_BASE+0x0648)
#define U3D_RXQLDPR4                              (SSUSB_DEV_BASE+0x064C)
#define U3D_RXQCSR5                               (SSUSB_DEV_BASE+0x0650)
#define U3D_RXQSAR5                               (SSUSB_DEV_BASE+0x0654)
#define U3D_RXQCPR5                               (SSUSB_DEV_BASE+0x0658)
#define U3D_RXQLDPR5                              (SSUSB_DEV_BASE+0x065C)
#define U3D_RXQCSR6                               (SSUSB_DEV_BASE+0x0660)
#define U3D_RXQSAR6                               (SSUSB_DEV_BASE+0x0664)
#define U3D_RXQCPR6                               (SSUSB_DEV_BASE+0x0668)
#define U3D_RXQLDPR6                              (SSUSB_DEV_BASE+0x066C)
#define U3D_RXQCSR7                               (SSUSB_DEV_BASE+0x0670)
#define U3D_RXQSAR7                               (SSUSB_DEV_BASE+0x0674)
#define U3D_RXQCPR7                               (SSUSB_DEV_BASE+0x0678)
#define U3D_RXQLDPR7                              (SSUSB_DEV_BASE+0x067C)
#define U3D_RXQCSR8                               (SSUSB_DEV_BASE+0x0680)
#define U3D_RXQSAR8                               (SSUSB_DEV_BASE+0x0684)
#define U3D_RXQCPR8                               (SSUSB_DEV_BASE+0x0688)
#define U3D_RXQLDPR8                              (SSUSB_DEV_BASE+0x068C)
#define U3D_RXQCSR9                               (SSUSB_DEV_BASE+0x0690)
#define U3D_RXQSAR9                               (SSUSB_DEV_BASE+0x0694)
#define U3D_RXQCPR9                               (SSUSB_DEV_BASE+0x0698)
#define U3D_RXQLDPR9                              (SSUSB_DEV_BASE+0x069C)
#define U3D_RXQCSR10                              (SSUSB_DEV_BASE+0x06A0)
#define U3D_RXQSAR10                              (SSUSB_DEV_BASE+0x06A4)
#define U3D_RXQCPR10                              (SSUSB_DEV_BASE+0x06A8)
#define U3D_RXQLDPR10                             (SSUSB_DEV_BASE+0x06AC)
#define U3D_RXQCSR11                              (SSUSB_DEV_BASE+0x06B0)
#define U3D_RXQSAR11                              (SSUSB_DEV_BASE+0x06B4)
#define U3D_RXQCPR11                              (SSUSB_DEV_BASE+0x06B8)
#define U3D_RXQLDPR11                             (SSUSB_DEV_BASE+0x06BC)
#define U3D_RXQCSR12                              (SSUSB_DEV_BASE+0x06C0)
#define U3D_RXQSAR12                              (SSUSB_DEV_BASE+0x06C4)
#define U3D_RXQCPR12                              (SSUSB_DEV_BASE+0x06C8)
#define U3D_RXQLDPR12                             (SSUSB_DEV_BASE+0x06CC)
#define U3D_RXQCSR13                              (SSUSB_DEV_BASE+0x06D0)
#define U3D_RXQSAR13                              (SSUSB_DEV_BASE+0x06D4)
#define U3D_RXQCPR13                              (SSUSB_DEV_BASE+0x06D8)
#define U3D_RXQLDPR13                             (SSUSB_DEV_BASE+0x06DC)
#define U3D_RXQCSR14                              (SSUSB_DEV_BASE+0x06E0)
#define U3D_RXQSAR14                              (SSUSB_DEV_BASE+0x06E4)
#define U3D_RXQCPR14                              (SSUSB_DEV_BASE+0x06E8)
#define U3D_RXQLDPR14                             (SSUSB_DEV_BASE+0x06EC)
#define U3D_RXQCSR15                              (SSUSB_DEV_BASE+0x06F0)
#define U3D_RXQSAR15                              (SSUSB_DEV_BASE+0x06F4)
#define U3D_RXQCPR15                              (SSUSB_DEV_BASE+0x06F8)
#define U3D_RXQLDPR15                             (SSUSB_DEV_BASE+0x06FC)
#define U3D_QISAR0                                (SSUSB_DEV_BASE+0x0700)
#define U3D_QIER0                                 (SSUSB_DEV_BASE+0x0704)
#define U3D_QIESR0                                (SSUSB_DEV_BASE+0x0708)
#define U3D_QIECR0                                (SSUSB_DEV_BASE+0x070C)
#define U3D_QISAR1                                (SSUSB_DEV_BASE+0x0710)
#define U3D_QIER1                                 (SSUSB_DEV_BASE+0x0714)
#define U3D_QIESR1                                (SSUSB_DEV_BASE+0x0718)
#define U3D_QIECR1                                (SSUSB_DEV_BASE+0x071C)
#define U3D_QEMIR                                 (SSUSB_DEV_BASE+0x0740)
#define U3D_QEMIER                                (SSUSB_DEV_BASE+0x0744)
#define U3D_QEMIESR                               (SSUSB_DEV_BASE+0x0748)
#define U3D_QEMIECR                               (SSUSB_DEV_BASE+0x074C)
#define U3D_TQERRIR0                              (SSUSB_DEV_BASE+0x0780)
#define U3D_TQERRIER0                             (SSUSB_DEV_BASE+0x0784)
#define U3D_TQERRIESR0                            (SSUSB_DEV_BASE+0x0788)
#define U3D_TQERRIECR0                            (SSUSB_DEV_BASE+0x078C)
#define U3D_RQERRIR0                              (SSUSB_DEV_BASE+0x07C0)
#define U3D_RQERRIER0                             (SSUSB_DEV_BASE+0x07C4)
#define U3D_RQERRIESR0                            (SSUSB_DEV_BASE+0x07C8)
#define U3D_RQERRIECR0                            (SSUSB_DEV_BASE+0x07CC)
#define U3D_RQERRIR1                              (SSUSB_DEV_BASE+0x07D0)
#define U3D_RQERRIER1                             (SSUSB_DEV_BASE+0x07D4)
#define U3D_RQERRIESR1                            (SSUSB_DEV_BASE+0x07D8)
#define U3D_RQERRIECR1                            (SSUSB_DEV_BASE+0x07DC)
#define U3D_CAP_EP0FFSZ                           (SSUSB_DEV_BASE+0x0C04)
#define U3D_CAP_EPNTXFFSZ                         (SSUSB_DEV_BASE+0x0C08)
#define U3D_CAP_EPNRXFFSZ                         (SSUSB_DEV_BASE+0x0C0C)
#define U3D_CAP_EPINFO                            (SSUSB_DEV_BASE+0x0C10)
#define U3D_CAP_TX_SLOT1                          (SSUSB_DEV_BASE+0x0C20)
#define U3D_CAP_TX_SLOT2                          (SSUSB_DEV_BASE+0x0C24)
#define U3D_CAP_TX_SLOT3                          (SSUSB_DEV_BASE+0x0C28)
#define U3D_CAP_TX_SLOT4                          (SSUSB_DEV_BASE+0x0C2C)
#define U3D_CAP_RX_SLOT1                          (SSUSB_DEV_BASE+0x0C30)
#define U3D_CAP_RX_SLOT2                          (SSUSB_DEV_BASE+0x0C34)
#define U3D_CAP_RX_SLOT3                          (SSUSB_DEV_BASE+0x0C38)
#define U3D_CAP_RX_SLOT4                          (SSUSB_DEV_BASE+0x0C3C)
#define U3D_MISC_CTRL                             (SSUSB_DEV_BASE+0x0C84)

/* SSUSB_DEV FIELD DEFINITION */

//U3D_LV1ISR
#define EP_CTRL_INTR                              (0x1<<5) //5:5
#define MAC2_INTR                                 (0x1<<4) //4:4
#define DMA_INTR                                  (0x1<<3) //3:3
#define MAC3_INTR                                 (0x1<<2) //2:2
#define QMU_INTR                                  (0x1<<1) //1:1
#define BMU_INTR                                  (0x1<<0) //0:0

//U3D_LV1IER
#define LV1IER                                    (0xffffffff<<0) //31:0

//U3D_LV1IESR
#define LV1IESR                                   (0xffffffff<<0) //31:0

//U3D_LV1IECR
#define LV1IECR                                   (0xffffffff<<0) //31:0

//U3D_AXI_WR_DMA_CFG
#define AXI_WR_ULTRA_NUM                          (0xff<<24) //31:24
#define AXI_WR_PRE_ULTRA_NUM                      (0xff<<16) //23:16
#define AXI_WR_ULTRA_EN                           (0x1<<0) //0:0

//U3D_AXI_RD_DMA_CFG
#define AXI_RD_ULTRA_NUM                          (0xff<<24) //31:24
#define AXI_RD_PRE_ULTRA_NUM                      (0xff<<16) //23:16
#define AXI_RD_ULTRA_EN                           (0x1<<0) //0:0

//U3D_MAC_U1_EN_CTRL
#define EXIT_BY_ERDY_DIS                          (0x1<<31) //31:31
#define ACCEPT_BMU_RX_EMPTY_CHK                   (0x1<<20) //20:20
#define ACCEPT_BMU_TX_EMPTY_CHK                   (0x1<<19) //19:19
#define ACCEPT_RXQ_INACTIVE_CHK                   (0x1<<18) //18:18
#define ACCEPT_TXQ_INACTIVE_CHK                   (0x1<<17) //17:17
#define ACCEPT_EP0_INACTIVE_CHK                   (0x1<<16) //16:16
#define REQUEST_BMU_RX_EMPTY_CHK                  (0x1<<4) //4:4
#define REQUEST_BMU_TX_EMPTY_CHK                  (0x1<<3) //3:3
#define REQUEST_RXQ_INACTIVE_CHK                  (0x1<<2) //2:2
#define REQUEST_TXQ_INACTIVE_CHK                  (0x1<<1) //1:1
#define REQUEST_EP0_INACTIVE_CHK                  (0x1<<0) //0:0

//U3D_MAC_U2_EN_CTRL
#define EXIT_BY_ERDY_DIS                          (0x1<<31) //31:31
#define ACCEPT_BMU_RX_EMPTY_CHK                   (0x1<<20) //20:20
#define ACCEPT_BMU_TX_EMPTY_CHK                   (0x1<<19) //19:19
#define ACCEPT_RXQ_INACTIVE_CHK                   (0x1<<18) //18:18
#define ACCEPT_TXQ_INACTIVE_CHK                   (0x1<<17) //17:17
#define ACCEPT_EP0_INACTIVE_CHK                   (0x1<<16) //16:16
#define REQUEST_BMU_RX_EMPTY_CHK                  (0x1<<4) //4:4
#define REQUEST_BMU_TX_EMPTY_CHK                  (0x1<<3) //3:3
#define REQUEST_RXQ_INACTIVE_CHK                  (0x1<<2) //2:2
#define REQUEST_TXQ_INACTIVE_CHK                  (0x1<<1) //1:1
#define REQUEST_EP0_INACTIVE_CHK                  (0x1<<0) //0:0

//U3D_SRAM_DBG_CTRL
#define EPNRX_SRAM_DEBUG_MODE                     (0x1<<2) //2:2
#define EPNTX_SRAM_DEBUG_MODE                     (0x1<<1) //1:1
#define EP0_SRAM_DEBUG_MODE                       (0x1<<0) //0:0

//U3D_SRAM_DBG_CTRL_1
#define SRAM_DEBUG_FIFOSEGSIZE                    (0xf<<24) //27:24
#define SRAM_DEBUG_SLOT                           (0x3f<<16) //21:16
#define SRAM_DEBUG_DP_COUNT                       (0x7ff<<0) //10:0

//U3D_RISC_SIZE
#define RISC_SIZE                                 (0x3<<0) //1:0

//U3D_WRBUF_ERR_STS
#define RX_RDBUF_ERR_STS                          (0x7fff<<17) //31:17
#define TX_WRBUF_ERR_STS                          (0x7fff<<1) //15:1

//U3D_BUF_ERR_EN
#define RX_RDBUF_ERR_EN                           (0x7fff<<17) //31:17
#define TX_WRBUF_ERR_EN                           (0x7fff<<1) //15:1

//U3D_EPISR
#define EPRISR                                    (0x7fff<<17) //31:17
#define SETUPENDISR                               (0x1<<16) //16:16
#define EPTISR                                    (0x7fff<<1) //15:1
#define EP0ISR                                    (0x1<<0) //0:0

//U3D_EPIER
#define EPRIER                                    (0x7fff<<17) //31:17
#define SETUPENDIER                               (0x1<<16) //16:16
#define EPTIER                                    (0x7fff<<1) //15:1
#define EP0IER                                    (0x1<<0) //0:0

//U3D_EPIESR
#define EPRIESR                                   (0x7fff<<17) //31:17
#define SETUPENDIESR                              (0x1<<16) //16:16
#define EPTIESR                                   (0x7fff<<1) //15:1
#define EP0IESR                                   (0x1<<0) //0:0

//U3D_EPIECR
#define EPRISR                                    (0x7fff<<17) //31:17
#define SETUPENDIECR                              (0x1<<16) //16:16
#define EPTIECR                                   (0x7fff<<1) //15:1
#define EP0IECR                                   (0x1<<0) //0:0

//U3D_DMAISR
#define RXDMAISR                                  (0x1<<2) //2:2
#define TXDMAISR                                  (0x1<<1) //1:1
#define EP0DMAISR                                 (0x1<<0) //0:0

//U3D_DMAIER
#define RXDMAIER                                  (0x1<<2) //2:2
#define TXDMAIER                                  (0x1<<1) //1:1
#define EP0DMAER                                  (0x1<<0) //0:0

//U3D_DMAIESR
#define RXDMAIESR                                 (0x1<<2) //2:2
#define TXDMAIESR                                 (0x1<<1) //1:1
#define EP0DMAIESR                                (0x1<<0) //0:0

//U3D_DMAIECR
#define RXDMAIECR                                 (0x1<<2) //2:2
#define TXDMAIECR                                 (0x1<<1) //1:1
#define EP0DMAIECR                                (0x1<<0) //0:0

//U3D_EP0DMACTRL
#define FFSTRADDR0                                (0xffff<<16) //31:16
#define ENDPNT                                    (0xf<<4) //7:4
#define INTEN                                     (0x1<<3) //3:3
#define DMA_DIR                                   (0x1<<1) //1:1
#define DMA_EN                                    (0x1<<0) //0:0

//U3D_EP0DMASTRADDR
#define DMASTRADDR0                               (0xffffffff<<0) //31:0

//U3D_EP0DMATFRCOUNT
#define DMATFRCNT0                                (0x7ff<<0) //10:0

//U3D_EP0DMARLCOUNT
#define EP0_DMALIMITER                            (0x7<<28) //30:28
#define DMA_FAKE                                  (0x1<<27) //27:27
#define DMA_BURST                                 (0x3<<24) //25:24
#define AXI_DMA_OUTSTAND_NUM                      (0xf<<20) //23:20
#define AXI_DMA_COHERENCE                         (0x1<<19) //19:19
#define AXI_DMA_IOMMU                             (0x1<<18) //18:18
#define AXI_DMA_CACHEABLE                         (0x1<<17) //17:17
#define AXI_DMA_ULTRA_EN                          (0x1<<16) //16:16
#define AXI_DMA_ULTRA_NUM                         (0xff<<8) //15:8
#define AXI_DMA_PRE_ULTRA_NUM                     (0xff<<0) //7:0

//U3D_TXDMACTRL
#define FFSTRADDR                                 (0xffff<<16) //31:16
#define ENDPNT                                    (0xf<<4) //7:4
#define INTEN                                     (0x1<<3) //3:3
#define DMA_DIR                                   (0x1<<1) //1:1
#define DMA_EN                                    (0x1<<0) //0:0

//U3D_TXDMASTRADDR
#define DMASTRADDR                                (0xffffffff<<0) //31:0

//U3D_TXDMATRDCNT
#define DMATFRCNT                                 (0x7ff<<0) //10:0

//U3D_TXDMARLCOUNT
#define DMALIMITER                                (0x7<<28) //30:28
#define DMA_FAKE                                  (0x1<<27) //27:27
#define DMA_BURST                                 (0x3<<24) //25:24
#define AXI_DMA_OUTSTAND_NUM                      (0xf<<20) //23:20
#define AXI_DMA_COHERENCE                         (0x1<<19) //19:19
#define AXI_DMA_IOMMU                             (0x1<<18) //18:18
#define AXI_DMA_CACHEABLE                         (0x1<<17) //17:17
#define AXI_DMA_ULTRA_EN                          (0x1<<16) //16:16
#define AXI_DMA_ULTRA_NUM                         (0xff<<8) //15:8
#define AXI_DMA_PRE_ULTRA_NUM                     (0xff<<0) //7:0

//U3D_RXDMACTRL
#define FFSTRADDR                                 (0xffff<<16) //31:16
#define ENDPNT                                    (0xf<<4) //7:4
#define INTEN                                     (0x1<<3) //3:3
#define DMA_DIR                                   (0x1<<1) //1:1
#define DMA_EN                                    (0x1<<0) //0:0

//U3D_RXDMASTRADDR
#define DMASTRADDR                                (0xffffffff<<0) //31:0

//U3D_RXDMATRDCNT
#define DMATFRCNT                                 (0x7ff<<0) //10:0

//U3D_RXDMARLCOUNT
#define DMA_NON_BUF                               (0x1<<31) //31:31
#define DMALIMITER                                (0x7<<28) //30:28
#define DMA_FAKE                                  (0x1<<27) //27:27
#define DMA_BURST                                 (0x3<<24) //25:24
#define AXI_DMA_OUTSTAND_NUM                      (0xf<<20) //23:20
#define AXI_DMA_COHERENCE                         (0x1<<19) //19:19
#define AXI_DMA_IOMMU                             (0x1<<18) //18:18
#define AXI_DMA_CACHEABLE                         (0x1<<17) //17:17
#define AXI_DMA_ULTRA_EN                          (0x1<<16) //16:16
#define AXI_DMA_ULTRA_NUM                         (0xff<<8) //15:8
#define AXI_DMA_PRE_ULTRA_NUM                     (0xff<<0) //7:0

//U3D_EP0CSR
#define EP0_EP_RESET                              (0x1<<31) //31:31
#define EP0_AUTOCLEAR                             (0x1<<30) //30:30
#define EP0_AUTOSET                               (0x1<<29) //29:29
#define EP0_DMAREQEN                              (0x1<<28) //28:28
#define EP0_SENDSTALL                             (0x1<<25) //25:25
#define EP0_FIFOFULL                              (0x1<<23) //23:23
#define EP0_SENTSTALL                             (0x1<<22) //22:22
#define EP0_DPHTX                                 (0x1<<20) //20:20
#define EP0_DATAEND                               (0x1<<19) //19:19
#define EP0_TXPKTRDY                              (0x1<<18) //18:18
#define EP0_SETUPPKTRDY                           (0x1<<17) //17:17
#define EP0_RXPKTRDY                              (0x1<<16) //16:16
#define EP0_MAXPKTSZ0                             (0x3ff<<0) //9:0

//U3D_RXCOUNT0
#define EP0_RX_COUNT                              (0x3ff<<0) //9:0

//U3D_RESERVED

//U3D_TX1CSR0
#define TX_EP_RESET                               (0x1<<31) //31:31
#define TX_AUTOSET                                (0x1<<30) //30:30
#define TX_DMAREQEN                               (0x1<<29) //29:29
#define TX_FIFOFULL                               (0x1<<25) //25:25
#define TX_FIFOEMPTY                              (0x1<<24) //24:24
#define TX_SENTSTALL                              (0x1<<22) //22:22
#define TX_SENDSTALL                              (0x1<<21) //21:21
#define TX_TXPKTRDY                               (0x1<<16) //16:16
#define TX_TXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_TX1CSR1
#define TX_MULT                                   (0x3<<22) //23:22
#define TX_MAX_PKT                                (0x3f<<16) //21:16
#define TX_SLOT                                   (0x3f<<8) //13:8
#define TXTYPE                                    (0x3<<4) //5:4
#define SS_TX_BURST                               (0xf<<0) //3:0

//U3D_TX1CSR2
#define TXBINTERVAL                               (0xff<<24) //31:24
#define TXFIFOSEGSIZE                             (0xf<<16) //19:16
#define TXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_TX2CSR0
#define TX_EP_RESET                               (0x1<<31) //31:31
#define TX_AUTOSET                                (0x1<<30) //30:30
#define TX_DMAREQEN                               (0x1<<29) //29:29
#define TX_FIFOFULL                               (0x1<<25) //25:25
#define TX_FIFOEMPTY                              (0x1<<24) //24:24
#define TX_SENTSTALL                              (0x1<<22) //22:22
#define TX_SENDSTALL                              (0x1<<21) //21:21
#define TX_TXPKTRDY                               (0x1<<16) //16:16
#define TX_TXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_TX2CSR1
#define TX_MULT                                   (0x3<<22) //23:22
#define TX_MAX_PKT                                (0x3f<<16) //21:16
#define TX_SLOT                                   (0x3f<<8) //13:8
#define TXTYPE                                    (0x3<<4) //5:4
#define SS_TX_BURST                               (0xf<<0) //3:0

//U3D_TX2CSR2
#define TXBINTERVAL                               (0xff<<24) //31:24
#define TXFIFOSEGSIZE                             (0xf<<16) //19:16
#define TXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_TX3CSR0
#define TX_EP_RESET                               (0x1<<31) //31:31
#define TX_AUTOSET                                (0x1<<30) //30:30
#define TX_DMAREQEN                               (0x1<<29) //29:29
#define TX_FIFOFULL                               (0x1<<25) //25:25
#define TX_FIFOEMPTY                              (0x1<<24) //24:24
#define TX_SENTSTALL                              (0x1<<22) //22:22
#define TX_SENDSTALL                              (0x1<<21) //21:21
#define TX_TXPKTRDY                               (0x1<<16) //16:16
#define TX_TXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_TX3CSR1
#define TX_MULT                                   (0x3<<22) //23:22
#define TX_MAX_PKT                                (0x3f<<16) //21:16
#define TX_SLOT                                   (0x3f<<8) //13:8
#define TXTYPE                                    (0x3<<4) //5:4
#define SS_TX_BURST                               (0xf<<0) //3:0

//U3D_TX3CSR2
#define TXBINTERVAL                               (0xff<<24) //31:24
#define TXFIFOSEGSIZE                             (0xf<<16) //19:16
#define TXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_TX4CSR0
#define TX_EP_RESET                               (0x1<<31) //31:31
#define TX_AUTOSET                                (0x1<<30) //30:30
#define TX_DMAREQEN                               (0x1<<29) //29:29
#define TX_FIFOFULL                               (0x1<<25) //25:25
#define TX_FIFOEMPTY                              (0x1<<24) //24:24
#define TX_SENTSTALL                              (0x1<<22) //22:22
#define TX_SENDSTALL                              (0x1<<21) //21:21
#define TX_TXPKTRDY                               (0x1<<16) //16:16
#define TX_TXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_TX4CSR1
#define TX_MULT                                   (0x3<<22) //23:22
#define TX_MAX_PKT                                (0x3f<<16) //21:16
#define TX_SLOT                                   (0x3f<<8) //13:8
#define TXTYPE                                    (0x3<<4) //5:4
#define SS_TX_BURST                               (0xf<<0) //3:0

//U3D_TX4CSR2
#define TXBINTERVAL                               (0xff<<24) //31:24
#define TXFIFOSEGSIZE                             (0xf<<16) //19:16
#define TXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_TX5CSR0
#define TX_EP_RESET                               (0x1<<31) //31:31
#define TX_AUTOSET                                (0x1<<30) //30:30
#define TX_DMAREQEN                               (0x1<<29) //29:29
#define TX_FIFOFULL                               (0x1<<25) //25:25
#define TX_FIFOEMPTY                              (0x1<<24) //24:24
#define TX_SENTSTALL                              (0x1<<22) //22:22
#define TX_SENDSTALL                              (0x1<<21) //21:21
#define TX_TXPKTRDY                               (0x1<<16) //16:16
#define TX_TXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_TX5CSR1
#define TX_MULT                                   (0x3<<22) //23:22
#define TX_MAX_PKT                                (0x3f<<16) //21:16
#define TX_SLOT                                   (0x3f<<8) //13:8
#define TXTYPE                                    (0x3<<4) //5:4
#define SS_TX_BURST                               (0xf<<0) //3:0

//U3D_TX5CSR2
#define TXBINTERVAL                               (0xff<<24) //31:24
#define TXFIFOSEGSIZE                             (0xf<<16) //19:16
#define TXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_TX6CSR0
#define TX_EP_RESET                               (0x1<<31) //31:31
#define TX_AUTOSET                                (0x1<<30) //30:30
#define TX_DMAREQEN                               (0x1<<29) //29:29
#define TX_FIFOFULL                               (0x1<<25) //25:25
#define TX_FIFOEMPTY                              (0x1<<24) //24:24
#define TX_SENTSTALL                              (0x1<<22) //22:22
#define TX_SENDSTALL                              (0x1<<21) //21:21
#define TX_TXPKTRDY                               (0x1<<16) //16:16
#define TX_TXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_TX6CSR1
#define TX_MULT                                   (0x3<<22) //23:22
#define TX_MAX_PKT                                (0x3f<<16) //21:16
#define TX_SLOT                                   (0x3f<<8) //13:8
#define TXTYPE                                    (0x3<<4) //5:4
#define SS_TX_BURST                               (0xf<<0) //3:0

//U3D_TX6CSR2
#define TXBINTERVAL                               (0xff<<24) //31:24
#define TXFIFOSEGSIZE                             (0xf<<16) //19:16
#define TXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_TX7CSR0
#define TX_EP_RESET                               (0x1<<31) //31:31
#define TX_AUTOSET                                (0x1<<30) //30:30
#define TX_DMAREQEN                               (0x1<<29) //29:29
#define TX_FIFOFULL                               (0x1<<25) //25:25
#define TX_FIFOEMPTY                              (0x1<<24) //24:24
#define TX_SENTSTALL                              (0x1<<22) //22:22
#define TX_SENDSTALL                              (0x1<<21) //21:21
#define TX_TXPKTRDY                               (0x1<<16) //16:16
#define TX_TXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_TX7CSR1
#define TX_MULT                                   (0x3<<22) //23:22
#define TX_MAX_PKT                                (0x3f<<16) //21:16
#define TX_SLOT                                   (0x3f<<8) //13:8
#define TXTYPE                                    (0x3<<4) //5:4
#define SS_TX_BURST                               (0xf<<0) //3:0

//U3D_TX7CSR2
#define TXBINTERVAL                               (0xff<<24) //31:24
#define TXFIFOSEGSIZE                             (0xf<<16) //19:16
#define TXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_TX8CSR0
#define TX_EP_RESET                               (0x1<<31) //31:31
#define TX_AUTOSET                                (0x1<<30) //30:30
#define TX_DMAREQEN                               (0x1<<29) //29:29
#define TX_FIFOFULL                               (0x1<<25) //25:25
#define TX_FIFOEMPTY                              (0x1<<24) //24:24
#define TX_SENTSTALL                              (0x1<<22) //22:22
#define TX_SENDSTALL                              (0x1<<21) //21:21
#define TX_TXPKTRDY                               (0x1<<16) //16:16
#define TX_TXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_TX8CSR1
#define TX_MULT                                   (0x3<<22) //23:22
#define TX_MAX_PKT                                (0x3f<<16) //21:16
#define TX_SLOT                                   (0x3f<<8) //13:8
#define TXTYPE                                    (0x3<<4) //5:4
#define SS_TX_BURST                               (0xf<<0) //3:0

//U3D_TX8CSR2
#define TXBINTERVAL                               (0xff<<24) //31:24
#define TXFIFOSEGSIZE                             (0xf<<16) //19:16
#define TXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_TX9CSR0
#define TX_EP_RESET                               (0x1<<31) //31:31
#define TX_AUTOSET                                (0x1<<30) //30:30
#define TX_DMAREQEN                               (0x1<<29) //29:29
#define TX_FIFOFULL                               (0x1<<25) //25:25
#define TX_FIFOEMPTY                              (0x1<<24) //24:24
#define TX_SENTSTALL                              (0x1<<22) //22:22
#define TX_SENDSTALL                              (0x1<<21) //21:21
#define TX_TXPKTRDY                               (0x1<<16) //16:16
#define TX_TXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_TX9CSR1
#define TX_MULT                                   (0x3<<22) //23:22
#define TX_MAX_PKT                                (0x3f<<16) //21:16
#define TX_SLOT                                   (0x3f<<8) //13:8
#define TXTYPE                                    (0x3<<4) //5:4
#define SS_TX_BURST                               (0xf<<0) //3:0

//U3D_TX9CSR2
#define TXBINTERVAL                               (0xff<<24) //31:24
#define TXFIFOSEGSIZE                             (0xf<<16) //19:16
#define TXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_TX10CSR0
#define TX_EP_RESET                               (0x1<<31) //31:31
#define TX_AUTOSET                                (0x1<<30) //30:30
#define TX_DMAREQEN                               (0x1<<29) //29:29
#define TX_FIFOFULL                               (0x1<<25) //25:25
#define TX_FIFOEMPTY                              (0x1<<24) //24:24
#define TX_SENTSTALL                              (0x1<<22) //22:22
#define TX_SENDSTALL                              (0x1<<21) //21:21
#define TX_TXPKTRDY                               (0x1<<16) //16:16
#define TX_TXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_TX10CSR1
#define TX_MULT                                   (0x3<<22) //23:22
#define TX_MAX_PKT                                (0x3f<<16) //21:16
#define TX_SLOT                                   (0x3f<<8) //13:8
#define TXTYPE                                    (0x3<<4) //5:4
#define SS_TX_BURST                               (0xf<<0) //3:0

//U3D_TX10CSR2
#define TXBINTERVAL                               (0xff<<24) //31:24
#define TXFIFOSEGSIZE                             (0xf<<16) //19:16
#define TXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_TX11CSR0
#define TX_EP_RESET                               (0x1<<31) //31:31
#define TX_AUTOSET                                (0x1<<30) //30:30
#define TX_DMAREQEN                               (0x1<<29) //29:29
#define TX_FIFOFULL                               (0x1<<25) //25:25
#define TX_FIFOEMPTY                              (0x1<<24) //24:24
#define TX_SENTSTALL                              (0x1<<22) //22:22
#define TX_SENDSTALL                              (0x1<<21) //21:21
#define TX_TXPKTRDY                               (0x1<<16) //16:16
#define TX_TXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_TX11CSR1
#define TX_MULT                                   (0x3<<22) //23:22
#define TX_MAX_PKT                                (0x3f<<16) //21:16
#define TX_SLOT                                   (0x3f<<8) //13:8
#define TXTYPE                                    (0x3<<4) //5:4
#define SS_TX_BURST                               (0xf<<0) //3:0

//U3D_TX11CSR2
#define TXBINTERVAL                               (0xff<<24) //31:24
#define TXFIFOSEGSIZE                             (0xf<<16) //19:16
#define TXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_TX12CSR0
#define TX_EP_RESET                               (0x1<<31) //31:31
#define TX_AUTOSET                                (0x1<<30) //30:30
#define TX_DMAREQEN                               (0x1<<29) //29:29
#define TX_FIFOFULL                               (0x1<<25) //25:25
#define TX_FIFOEMPTY                              (0x1<<24) //24:24
#define TX_SENTSTALL                              (0x1<<22) //22:22
#define TX_SENDSTALL                              (0x1<<21) //21:21
#define TX_TXPKTRDY                               (0x1<<16) //16:16
#define TX_TXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_TX12CSR1
#define TX_MULT                                   (0x3<<22) //23:22
#define TX_MAX_PKT                                (0x3f<<16) //21:16
#define TX_SLOT                                   (0x3f<<8) //13:8
#define TXTYPE                                    (0x3<<4) //5:4
#define SS_TX_BURST                               (0xf<<0) //3:0

//U3D_TX12CSR2
#define TXBINTERVAL                               (0xff<<24) //31:24
#define TXFIFOSEGSIZE                             (0xf<<16) //19:16
#define TXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_TX13CSR0
#define TX_EP_RESET                               (0x1<<31) //31:31
#define TX_AUTOSET                                (0x1<<30) //30:30
#define TX_DMAREQEN                               (0x1<<29) //29:29
#define TX_FIFOFULL                               (0x1<<25) //25:25
#define TX_FIFOEMPTY                              (0x1<<24) //24:24
#define TX_SENTSTALL                              (0x1<<22) //22:22
#define TX_SENDSTALL                              (0x1<<21) //21:21
#define TX_TXPKTRDY                               (0x1<<16) //16:16
#define TX_TXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_TX13CSR1
#define TX_MULT                                   (0x3<<22) //23:22
#define TX_MAX_PKT                                (0x3f<<16) //21:16
#define TX_SLOT                                   (0x3f<<8) //13:8
#define TXTYPE                                    (0x3<<4) //5:4
#define SS_TX_BURST                               (0xf<<0) //3:0

//U3D_TX13CSR2
#define TXBINTERVAL                               (0xff<<24) //31:24
#define TXFIFOSEGSIZE                             (0xf<<16) //19:16
#define TXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_TX14CSR0
#define TX_EP_RESET                               (0x1<<31) //31:31
#define TX_AUTOSET                                (0x1<<30) //30:30
#define TX_DMAREQEN                               (0x1<<29) //29:29
#define TX_FIFOFULL                               (0x1<<25) //25:25
#define TX_FIFOEMPTY                              (0x1<<24) //24:24
#define TX_SENTSTALL                              (0x1<<22) //22:22
#define TX_SENDSTALL                              (0x1<<21) //21:21
#define TX_TXPKTRDY                               (0x1<<16) //16:16
#define TX_TXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_TX14CSR1
#define TX_MULT                                   (0x3<<22) //23:22
#define TX_MAX_PKT                                (0x3f<<16) //21:16
#define TX_SLOT                                   (0x3f<<8) //13:8
#define TXTYPE                                    (0x3<<4) //5:4
#define SS_TX_BURST                               (0xf<<0) //3:0

//U3D_TX14CSR2
#define TXBINTERVAL                               (0xff<<24) //31:24
#define TXFIFOSEGSIZE                             (0xf<<16) //19:16
#define TXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_TX15CSR0
#define TX_EP_RESET                               (0x1<<31) //31:31
#define TX_AUTOSET                                (0x1<<30) //30:30
#define TX_DMAREQEN                               (0x1<<29) //29:29
#define TX_FIFOFULL                               (0x1<<25) //25:25
#define TX_FIFOEMPTY                              (0x1<<24) //24:24
#define TX_SENTSTALL                              (0x1<<22) //22:22
#define TX_SENDSTALL                              (0x1<<21) //21:21
#define TX_TXPKTRDY                               (0x1<<16) //16:16
#define TX_TXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_TX15CSR1
#define TX_MULT                                   (0x3<<22) //23:22
#define TX_MAX_PKT                                (0x3f<<16) //21:16
#define TX_SLOT                                   (0x3f<<8) //13:8
#define TXTYPE                                    (0x3<<4) //5:4
#define SS_TX_BURST                               (0xf<<0) //3:0

//U3D_TX15CSR2
#define TXBINTERVAL                               (0xff<<24) //31:24
#define TXFIFOSEGSIZE                             (0xf<<16) //19:16
#define TXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_RX1CSR0
#define RX_EP_RESET                               (0x1<<31) //31:31
#define RX_AUTOCLEAR                              (0x1<<30) //30:30
#define RX_DMAREQEN                               (0x1<<29) //29:29
#define RX_SENTSTALL                              (0x1<<22) //22:22
#define RX_SENDSTALL                              (0x1<<21) //21:21
#define RX_FIFOFULL                               (0x1<<18) //18:18
#define RX_FIFOEMPTY                              (0x1<<17) //17:17
#define RX_RXPKTRDY                               (0x1<<16) //16:16
#define RX_RXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_RX1CSR1
#define RX_MULT                                   (0x3<<22) //23:22
#define RX_MAX_PKT                                (0x3f<<16) //21:16
#define RX_SLOT                                   (0x3f<<8) //13:8
#define RX_TYPE                                   (0x3<<4) //5:4
#define SS_RX_BURST                               (0xf<<0) //3:0

//U3D_RX1CSR2
#define RXBINTERVAL                               (0xff<<24) //31:24
#define RXFIFOSEGSIZE                             (0xf<<16) //19:16
#define RXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_RX1CSR3
#define EP_RX_COUNT                               (0x7ff<<16) //26:16

//U3D_RX2CSR0
#define RX_EP_RESET                               (0x1<<31) //31:31
#define RX_AUTOCLEAR                              (0x1<<30) //30:30
#define RX_DMAREQEN                               (0x1<<29) //29:29
#define RX_SENTSTALL                              (0x1<<22) //22:22
#define RX_SENDSTALL                              (0x1<<21) //21:21
#define RX_FIFOFULL                               (0x1<<18) //18:18
#define RX_FIFOEMPTY                              (0x1<<17) //17:17
#define RX_RXPKTRDY                               (0x1<<16) //16:16
#define RX_RXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_RX2CSR1
#define RX_MULT                                   (0x3<<22) //23:22
#define RX_MAX_PKT                                (0x3f<<16) //21:16
#define RX_SLOT                                   (0x3f<<8) //13:8
#define RX_TYPE                                   (0x3<<4) //5:4
#define SS_RX_BURST                               (0xf<<0) //3:0

//U3D_RX2CSR2
#define RXBINTERVAL                               (0xff<<24) //31:24
#define RXFIFOSEGSIZE                             (0xf<<16) //19:16
#define RXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_RX2CSR3
#define EP_RX_COUNT                               (0x7ff<<16) //26:16

//U3D_RX3CSR0
#define RX_EP_RESET                               (0x1<<31) //31:31
#define RX_AUTOCLEAR                              (0x1<<30) //30:30
#define RX_DMAREQEN                               (0x1<<29) //29:29
#define RX_SENTSTALL                              (0x1<<22) //22:22
#define RX_SENDSTALL                              (0x1<<21) //21:21
#define RX_FIFOFULL                               (0x1<<18) //18:18
#define RX_FIFOEMPTY                              (0x1<<17) //17:17
#define RX_RXPKTRDY                               (0x1<<16) //16:16
#define RX_RXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_RX3CSR1
#define RX_MULT                                   (0x3<<22) //23:22
#define RX_MAX_PKT                                (0x3f<<16) //21:16
#define RX_SLOT                                   (0x3f<<8) //13:8
#define RX_TYPE                                   (0x3<<4) //5:4
#define SS_RX_BURST                               (0xf<<0) //3:0

//U3D_RX3CSR2
#define RXBINTERVAL                               (0xff<<24) //31:24
#define RXFIFOSEGSIZE                             (0xf<<16) //19:16
#define RXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_RX3CSR3
#define EP_RX_COUNT                               (0x7ff<<16) //26:16

//U3D_RX4CSR0
#define RX_EP_RESET                               (0x1<<31) //31:31
#define RX_AUTOCLEAR                              (0x1<<30) //30:30
#define RX_DMAREQEN                               (0x1<<29) //29:29
#define RX_SENTSTALL                              (0x1<<22) //22:22
#define RX_SENDSTALL                              (0x1<<21) //21:21
#define RX_FIFOFULL                               (0x1<<18) //18:18
#define RX_FIFOEMPTY                              (0x1<<17) //17:17
#define RX_RXPKTRDY                               (0x1<<16) //16:16
#define RX_RXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_RX4CSR1
#define RX_MULT                                   (0x3<<22) //23:22
#define RX_MAX_PKT                                (0x3f<<16) //21:16
#define RX_SLOT                                   (0x3f<<8) //13:8
#define RX_TYPE                                   (0x3<<4) //5:4
#define SS_RX_BURST                               (0xf<<0) //3:0

//U3D_RX4CSR2
#define RXBINTERVAL                               (0xff<<24) //31:24
#define RXFIFOSEGSIZE                             (0xf<<16) //19:16
#define RXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_RX4CSR3
#define EP_RX_COUNT                               (0x7ff<<16) //26:16

//U3D_RX5CSR0
#define RX_EP_RESET                               (0x1<<31) //31:31
#define RX_AUTOCLEAR                              (0x1<<30) //30:30
#define RX_DMAREQEN                               (0x1<<29) //29:29
#define RX_SENTSTALL                              (0x1<<22) //22:22
#define RX_SENDSTALL                              (0x1<<21) //21:21
#define RX_FIFOFULL                               (0x1<<18) //18:18
#define RX_FIFOEMPTY                              (0x1<<17) //17:17
#define RX_RXPKTRDY                               (0x1<<16) //16:16
#define RX_RXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_RX5CSR1
#define RX_MULT                                   (0x3<<22) //23:22
#define RX_MAX_PKT                                (0x3f<<16) //21:16
#define RX_SLOT                                   (0x3f<<8) //13:8
#define RX_TYPE                                   (0x3<<4) //5:4
#define SS_RX_BURST                               (0xf<<0) //3:0

//U3D_RX5CSR2
#define RXBINTERVAL                               (0xff<<24) //31:24
#define RXFIFOSEGSIZE                             (0xf<<16) //19:16
#define RXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_RX5CSR3
#define EP_RX_COUNT                               (0x7ff<<16) //26:16

//U3D_RX6CSR0
#define RX_EP_RESET                               (0x1<<31) //31:31
#define RX_AUTOCLEAR                              (0x1<<30) //30:30
#define RX_DMAREQEN                               (0x1<<29) //29:29
#define RX_SENTSTALL                              (0x1<<22) //22:22
#define RX_SENDSTALL                              (0x1<<21) //21:21
#define RX_FIFOFULL                               (0x1<<18) //18:18
#define RX_FIFOEMPTY                              (0x1<<17) //17:17
#define RX_RXPKTRDY                               (0x1<<16) //16:16
#define RX_RXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_RX6CSR1
#define RX_MULT                                   (0x3<<22) //23:22
#define RX_MAX_PKT                                (0x3f<<16) //21:16
#define RX_SLOT                                   (0x3f<<8) //13:8
#define RX_TYPE                                   (0x3<<4) //5:4
#define SS_RX_BURST                               (0xf<<0) //3:0

//U3D_RX6CSR2
#define RXBINTERVAL                               (0xff<<24) //31:24
#define RXFIFOSEGSIZE                             (0xf<<16) //19:16
#define RXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_RX6CSR3
#define EP_RX_COUNT                               (0x7ff<<16) //26:16

//U3D_RX7CSR0
#define RX_EP_RESET                               (0x1<<31) //31:31
#define RX_AUTOCLEAR                              (0x1<<30) //30:30
#define RX_DMAREQEN                               (0x1<<29) //29:29
#define RX_SENTSTALL                              (0x1<<22) //22:22
#define RX_SENDSTALL                              (0x1<<21) //21:21
#define RX_FIFOFULL                               (0x1<<18) //18:18
#define RX_FIFOEMPTY                              (0x1<<17) //17:17
#define RX_RXPKTRDY                               (0x1<<16) //16:16
#define RX_RXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_RX7CSR1
#define RX_MULT                                   (0x3<<22) //23:22
#define RX_MAX_PKT                                (0x3f<<16) //21:16
#define RX_SLOT                                   (0x3f<<8) //13:8
#define RX_TYPE                                   (0x3<<4) //5:4
#define SS_RX_BURST                               (0xf<<0) //3:0

//U3D_RX7CSR2
#define RXBINTERVAL                               (0xff<<24) //31:24
#define RXFIFOSEGSIZE                             (0xf<<16) //19:16
#define RXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_RX7CSR3
#define EP_RX_COUNT                               (0x7ff<<16) //26:16

//U3D_RX8CSR0
#define RX_EP_RESET                               (0x1<<31) //31:31
#define RX_AUTOCLEAR                              (0x1<<30) //30:30
#define RX_DMAREQEN                               (0x1<<29) //29:29
#define RX_SENTSTALL                              (0x1<<22) //22:22
#define RX_SENDSTALL                              (0x1<<21) //21:21
#define RX_FIFOFULL                               (0x1<<18) //18:18
#define RX_FIFOEMPTY                              (0x1<<17) //17:17
#define RX_RXPKTRDY                               (0x1<<16) //16:16
#define RX_RXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_RX8CSR1
#define RX_MULT                                   (0x3<<22) //23:22
#define RX_MAX_PKT                                (0x3f<<16) //21:16
#define RX_SLOT                                   (0x3f<<8) //13:8
#define RX_TYPE                                   (0x3<<4) //5:4
#define SS_RX_BURST                               (0xf<<0) //3:0

//U3D_RX8CSR2
#define RXBINTERVAL                               (0xff<<24) //31:24
#define RXFIFOSEGSIZE                             (0xf<<16) //19:16
#define RXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_RX8CSR3
#define EP_RX_COUNT                               (0x7ff<<16) //26:16

//U3D_RX9CSR0
#define RX_EP_RESET                               (0x1<<31) //31:31
#define RX_AUTOCLEAR                              (0x1<<30) //30:30
#define RX_DMAREQEN                               (0x1<<29) //29:29
#define RX_SENTSTALL                              (0x1<<22) //22:22
#define RX_SENDSTALL                              (0x1<<21) //21:21
#define RX_FIFOFULL                               (0x1<<18) //18:18
#define RX_FIFOEMPTY                              (0x1<<17) //17:17
#define RX_RXPKTRDY                               (0x1<<16) //16:16
#define RX_RXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_RX9CSR1
#define RX_MULT                                   (0x3<<22) //23:22
#define RX_MAX_PKT                                (0x3f<<16) //21:16
#define RX_SLOT                                   (0x3f<<8) //13:8
#define RX_TYPE                                   (0x3<<4) //5:4
#define SS_RX_BURST                               (0xf<<0) //3:0

//U3D_RX9CSR2
#define RXBINTERVAL                               (0xff<<24) //31:24
#define RXFIFOSEGSIZE                             (0xf<<16) //19:16
#define RXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_RX9CSR3
#define EP_RX_COUNT                               (0x7ff<<16) //26:16

//U3D_RX10CSR0
#define RX_EP_RESET                               (0x1<<31) //31:31
#define RX_AUTOCLEAR                              (0x1<<30) //30:30
#define RX_DMAREQEN                               (0x1<<29) //29:29
#define RX_SENTSTALL                              (0x1<<22) //22:22
#define RX_SENDSTALL                              (0x1<<21) //21:21
#define RX_FIFOFULL                               (0x1<<18) //18:18
#define RX_FIFOEMPTY                              (0x1<<17) //17:17
#define RX_RXPKTRDY                               (0x1<<16) //16:16
#define RX_RXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_RX10CSR1
#define RX_MULT                                   (0x3<<22) //23:22
#define RX_MAX_PKT                                (0x3f<<16) //21:16
#define RX_SLOT                                   (0x3f<<8) //13:8
#define RX_TYPE                                   (0x3<<4) //5:4
#define SS_RX_BURST                               (0xf<<0) //3:0

//U3D_RX10CSR2
#define RXBINTERVAL                               (0xff<<24) //31:24
#define RXFIFOSEGSIZE                             (0xf<<16) //19:16
#define RXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_RX10CSR3
#define EP_RX_COUNT                               (0x7ff<<16) //26:16

//U3D_RX11CSR0
#define RX_EP_RESET                               (0x1<<31) //31:31
#define RX_AUTOCLEAR                              (0x1<<30) //30:30
#define RX_DMAREQEN                               (0x1<<29) //29:29
#define RX_SENTSTALL                              (0x1<<22) //22:22
#define RX_SENDSTALL                              (0x1<<21) //21:21
#define RX_FIFOFULL                               (0x1<<18) //18:18
#define RX_FIFOEMPTY                              (0x1<<17) //17:17
#define RX_RXPKTRDY                               (0x1<<16) //16:16
#define RX_RXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_RX11CSR1
#define RX_MULT                                   (0x3<<22) //23:22
#define RX_MAX_PKT                                (0x3f<<16) //21:16
#define RX_SLOT                                   (0x3f<<8) //13:8
#define RX_TYPE                                   (0x3<<4) //5:4
#define SS_RX_BURST                               (0xf<<0) //3:0

//U3D_RX11CSR2
#define RXBINTERVAL                               (0xff<<24) //31:24
#define RXFIFOSEGSIZE                             (0xf<<16) //19:16
#define RXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_RX11CSR3
#define EP_RX_COUNT                               (0x7ff<<16) //26:16

//U3D_RX12CSR0
#define RX_EP_RESET                               (0x1<<31) //31:31
#define RX_AUTOCLEAR                              (0x1<<30) //30:30
#define RX_DMAREQEN                               (0x1<<29) //29:29
#define RX_SENTSTALL                              (0x1<<22) //22:22
#define RX_SENDSTALL                              (0x1<<21) //21:21
#define RX_FIFOFULL                               (0x1<<18) //18:18
#define RX_FIFOEMPTY                              (0x1<<17) //17:17
#define RX_RXPKTRDY                               (0x1<<16) //16:16
#define RX_RXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_RX12CSR1
#define RX_MULT                                   (0x3<<22) //23:22
#define RX_MAX_PKT                                (0x3f<<16) //21:16
#define RX_SLOT                                   (0x3f<<8) //13:8
#define RX_TYPE                                   (0x3<<4) //5:4
#define SS_RX_BURST                               (0xf<<0) //3:0

//U3D_RX12CSR2
#define RXBINTERVAL                               (0xff<<24) //31:24
#define RXFIFOSEGSIZE                             (0xf<<16) //19:16
#define RXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_RX12CSR3
#define EP_RX_COUNT                               (0x7ff<<16) //26:16

//U3D_RX13CSR0
#define RX_EP_RESET                               (0x1<<31) //31:31
#define RX_AUTOCLEAR                              (0x1<<30) //30:30
#define RX_DMAREQEN                               (0x1<<29) //29:29
#define RX_SENTSTALL                              (0x1<<22) //22:22
#define RX_SENDSTALL                              (0x1<<21) //21:21
#define RX_FIFOFULL                               (0x1<<18) //18:18
#define RX_FIFOEMPTY                              (0x1<<17) //17:17
#define RX_RXPKTRDY                               (0x1<<16) //16:16
#define RX_RXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_RX13CSR1
#define RX_MULT                                   (0x3<<22) //23:22
#define RX_MAX_PKT                                (0x3f<<16) //21:16
#define RX_SLOT                                   (0x3f<<8) //13:8
#define RX_TYPE                                   (0x3<<4) //5:4
#define SS_RX_BURST                               (0xf<<0) //3:0

//U3D_RX13CSR2
#define RXBINTERVAL                               (0xff<<24) //31:24
#define RXFIFOSEGSIZE                             (0xf<<16) //19:16
#define RXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_RX13CSR3
#define EP_RX_COUNT                               (0x7ff<<16) //26:16

//U3D_RX14CSR0
#define RX_EP_RESET                               (0x1<<31) //31:31
#define RX_AUTOCLEAR                              (0x1<<30) //30:30
#define RX_DMAREQEN                               (0x1<<29) //29:29
#define RX_SENTSTALL                              (0x1<<22) //22:22
#define RX_SENDSTALL                              (0x1<<21) //21:21
#define RX_FIFOFULL                               (0x1<<18) //18:18
#define RX_FIFOEMPTY                              (0x1<<17) //17:17
#define RX_RXPKTRDY                               (0x1<<16) //16:16
#define RX_RXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_RX14CSR1
#define RX_MULT                                   (0x3<<22) //23:22
#define RX_MAX_PKT                                (0x3f<<16) //21:16
#define RX_SLOT                                   (0x3f<<8) //13:8
#define RX_TYPE                                   (0x3<<4) //5:4
#define SS_RX_BURST                               (0xf<<0) //3:0

//U3D_RX14CSR2
#define RXBINTERVAL                               (0xff<<24) //31:24
#define RXFIFOSEGSIZE                             (0xf<<16) //19:16
#define RXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_RX14CSR3
#define EP_RX_COUNT                               (0x7ff<<16) //26:16

//U3D_RX15CSR0
#define RX_EP_RESET                               (0x1<<31) //31:31
#define RX_AUTOCLEAR                              (0x1<<30) //30:30
#define RX_DMAREQEN                               (0x1<<29) //29:29
#define RX_SENTSTALL                              (0x1<<22) //22:22
#define RX_SENDSTALL                              (0x1<<21) //21:21
#define RX_FIFOFULL                               (0x1<<18) //18:18
#define RX_FIFOEMPTY                              (0x1<<17) //17:17
#define RX_RXPKTRDY                               (0x1<<16) //16:16
#define RX_RXMAXPKTSZ                             (0x7ff<<0) //10:0

//U3D_RX15CSR1
#define RX_MULT                                   (0x3<<22) //23:22
#define RX_MAX_PKT                                (0x3f<<16) //21:16
#define RX_SLOT                                   (0x3f<<8) //13:8
#define RX_TYPE                                   (0x3<<4) //5:4
#define SS_RX_BURST                               (0xf<<0) //3:0

//U3D_RX15CSR2
#define RXBINTERVAL                               (0xff<<24) //31:24
#define RXFIFOSEGSIZE                             (0xf<<16) //19:16
#define RXFIFOADDR                                (0x1fff<<0) //12:0

//U3D_RX15CSR3
#define EP_RX_COUNT                               (0x7ff<<16) //26:16

//U3D_FIFO0
#define BYTE3                                     (0xff<<24) //31:24
#define BYTE2                                     (0xff<<16) //23:16
#define BYTE1                                     (0xff<<8) //15:8
#define BYTE0                                     (0xff<<0) //7:0

//U3D_FIFO1
#define BYTE3                                     (0xff<<24) //31:24
#define BYTE2                                     (0xff<<16) //23:16
#define BYTE1                                     (0xff<<8) //15:8
#define BYTE0                                     (0xff<<0) //7:0

//U3D_FIFO2
#define BYTE3                                     (0xff<<24) //31:24
#define BYTE2                                     (0xff<<16) //23:16
#define BYTE1                                     (0xff<<8) //15:8
#define BYTE0                                     (0xff<<0) //7:0

//U3D_FIFO3
#define BYTE3                                     (0xff<<24) //31:24
#define BYTE2                                     (0xff<<16) //23:16
#define BYTE1                                     (0xff<<8) //15:8
#define BYTE0                                     (0xff<<0) //7:0

//U3D_FIFO4
#define BYTE3                                     (0xff<<24) //31:24
#define BYTE2                                     (0xff<<16) //23:16
#define BYTE1                                     (0xff<<8) //15:8
#define BYTE0                                     (0xff<<0) //7:0

//U3D_FIFO5
#define BYTE3                                     (0xff<<24) //31:24
#define BYTE2                                     (0xff<<16) //23:16
#define BYTE1                                     (0xff<<8) //15:8
#define BYTE0                                     (0xff<<0) //7:0

//U3D_FIFO6
#define BYTE3                                     (0xff<<24) //31:24
#define BYTE2                                     (0xff<<16) //23:16
#define BYTE1                                     (0xff<<8) //15:8
#define BYTE0                                     (0xff<<0) //7:0

//U3D_FIFO7
#define BYTE3                                     (0xff<<24) //31:24
#define BYTE2                                     (0xff<<16) //23:16
#define BYTE1                                     (0xff<<8) //15:8
#define BYTE0                                     (0xff<<0) //7:0

//U3D_FIFO8
#define BYTE3                                     (0xff<<24) //31:24
#define BYTE2                                     (0xff<<16) //23:16
#define BYTE1                                     (0xff<<8) //15:8
#define BYTE0                                     (0xff<<0) //7:0

//U3D_FIFO9
#define BYTE3                                     (0xff<<24) //31:24
#define BYTE2                                     (0xff<<16) //23:16
#define BYTE1                                     (0xff<<8) //15:8
#define BYTE0                                     (0xff<<0) //7:0

//U3D_FIFO10
#define BYTE3                                     (0xff<<24) //31:24
#define BYTE2                                     (0xff<<16) //23:16
#define BYTE1                                     (0xff<<8) //15:8
#define BYTE0                                     (0xff<<0) //7:0

//U3D_FIFO11
#define BYTE3                                     (0xff<<24) //31:24
#define BYTE2                                     (0xff<<16) //23:16
#define BYTE1                                     (0xff<<8) //15:8
#define BYTE0                                     (0xff<<0) //7:0

//U3D_FIFO12
#define BYTE3                                     (0xff<<24) //31:24
#define BYTE2                                     (0xff<<16) //23:16
#define BYTE1                                     (0xff<<8) //15:8
#define BYTE0                                     (0xff<<0) //7:0

//U3D_FIFO13
#define BYTE3                                     (0xff<<24) //31:24
#define BYTE2                                     (0xff<<16) //23:16
#define BYTE1                                     (0xff<<8) //15:8
#define BYTE0                                     (0xff<<0) //7:0

//U3D_FIFO14
#define BYTE3                                     (0xff<<24) //31:24
#define BYTE2                                     (0xff<<16) //23:16
#define BYTE1                                     (0xff<<8) //15:8
#define BYTE0                                     (0xff<<0) //7:0

//U3D_FIFO15
#define BYTE3                                     (0xff<<24) //31:24
#define BYTE2                                     (0xff<<16) //23:16
#define BYTE1                                     (0xff<<8) //15:8
#define BYTE0                                     (0xff<<0) //7:0

//U3D_QCR0
#define RXQ_CS_EN                                 (0x7fff<<17) //31:17
#define TXQ_CS_EN                                 (0x7fff<<1) //15:1
#define CS16B_EN                                  (0x1<<0) //0:0

//U3D_QCR1
#define CFG_TX_ZLP_GPD                            (0x7fff<<1) //15:1

//U3D_QCR2
#define CFG_TX_ZLP                                (0x7fff<<1) //15:1

//U3D_QCR3
#define CFG_RX_COZ                                (0x7fff<<17) //31:17
#define CFG_RX_ZLP                                (0x7fff<<1) //15:1

//U3D_QGCSR
#define RXQ_EN                                    (0x7fff<<17) //31:17
#define TXQ_EN                                    (0x7fff<<1) //15:1

//U3D_TXQCSR1
#define TXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define TXQ_ACTIVE                                (0x1<<15) //15:15
#define TXQ_EPQ_STATE                             (0xf<<8) //11:8
#define TXQ_STOP                                  (0x1<<2) //2:2
#define TXQ_RESUME                                (0x1<<1) //1:1
#define TXQ_START                                 (0x1<<0) //0:0

//U3D_TXQSAR1
#define TXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_TXQCPR1
#define TXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_TXQCSR2
#define TXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define TXQ_ACTIVE                                (0x1<<15) //15:15
#define TXQ_EPQ_STATE                             (0xf<<8) //11:8
#define TXQ_STOP                                  (0x1<<2) //2:2
#define TXQ_RESUME                                (0x1<<1) //1:1
#define TXQ_START                                 (0x1<<0) //0:0

//U3D_TXQSAR2
#define TXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_TXQCPR2
#define TXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_TXQCSR3
#define TXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define TXQ_ACTIVE                                (0x1<<15) //15:15
#define TXQ_EPQ_STATE                             (0xf<<8) //11:8
#define TXQ_STOP                                  (0x1<<2) //2:2
#define TXQ_RESUME                                (0x1<<1) //1:1
#define TXQ_START                                 (0x1<<0) //0:0

//U3D_TXQSAR3
#define TXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_TXQCPR3
#define TXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_TXQCSR4
#define TXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define TXQ_ACTIVE                                (0x1<<15) //15:15
#define TXQ_EPQ_STATE                             (0xf<<8) //11:8
#define TXQ_STOP                                  (0x1<<2) //2:2
#define TXQ_RESUME                                (0x1<<1) //1:1
#define TXQ_START                                 (0x1<<0) //0:0

//U3D_TXQSAR4
#define TXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_TXQCPR4
#define TXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_TXQCSR5
#define TXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define TXQ_ACTIVE                                (0x1<<15) //15:15
#define TXQ_EPQ_STATE                             (0xf<<8) //11:8
#define TXQ_STOP                                  (0x1<<2) //2:2
#define TXQ_RESUME                                (0x1<<1) //1:1
#define TXQ_START                                 (0x1<<0) //0:0

//U3D_TXQSAR5
#define TXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_TXQCPR5
#define TXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_TXQCSR6
#define TXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define TXQ_ACTIVE                                (0x1<<15) //15:15
#define TXQ_EPQ_STATE                             (0xf<<8) //11:8
#define TXQ_STOP                                  (0x1<<2) //2:2
#define TXQ_RESUME                                (0x1<<1) //1:1
#define TXQ_START                                 (0x1<<0) //0:0

//U3D_TXQSAR6
#define TXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_TXQCPR6
#define TXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_TXQCSR7
#define TXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define TXQ_ACTIVE                                (0x1<<15) //15:15
#define TXQ_EPQ_STATE                             (0xf<<8) //11:8
#define TXQ_STOP                                  (0x1<<2) //2:2
#define TXQ_RESUME                                (0x1<<1) //1:1
#define TXQ_START                                 (0x1<<0) //0:0

//U3D_TXQSAR7
#define TXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_TXQCPR7
#define TXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_TXQCSR8
#define TXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define TXQ_ACTIVE                                (0x1<<15) //15:15
#define TXQ_EPQ_STATE                             (0xf<<8) //11:8
#define TXQ_STOP                                  (0x1<<2) //2:2
#define TXQ_RESUME                                (0x1<<1) //1:1
#define TXQ_START                                 (0x1<<0) //0:0

//U3D_TXQSAR8
#define TXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_TXQCPR8
#define TXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_TXQCSR9
#define TXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define TXQ_ACTIVE                                (0x1<<15) //15:15
#define TXQ_EPQ_STATE                             (0xf<<8) //11:8
#define TXQ_STOP                                  (0x1<<2) //2:2
#define TXQ_RESUME                                (0x1<<1) //1:1
#define TXQ_START                                 (0x1<<0) //0:0

//U3D_TXQSAR9
#define TXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_TXQCPR9
#define TXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_TXQCSR10
#define TXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define TXQ_ACTIVE                                (0x1<<15) //15:15
#define TXQ_EPQ_STATE                             (0xf<<8) //11:8
#define TXQ_STOP                                  (0x1<<2) //2:2
#define TXQ_RESUME                                (0x1<<1) //1:1
#define TXQ_START                                 (0x1<<0) //0:0

//U3D_TXQSAR10
#define TXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_TXQCPR10
#define TXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_TXQCSR11
#define TXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define TXQ_ACTIVE                                (0x1<<15) //15:15
#define TXQ_EPQ_STATE                             (0xf<<8) //11:8
#define TXQ_STOP                                  (0x1<<2) //2:2
#define TXQ_RESUME                                (0x1<<1) //1:1
#define TXQ_START                                 (0x1<<0) //0:0

//U3D_TXQSAR11
#define TXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_TXQCPR11
#define TXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_TXQCSR12
#define TXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define TXQ_ACTIVE                                (0x1<<15) //15:15
#define TXQ_EPQ_STATE                             (0xf<<8) //11:8
#define TXQ_STOP                                  (0x1<<2) //2:2
#define TXQ_RESUME                                (0x1<<1) //1:1
#define TXQ_START                                 (0x1<<0) //0:0

//U3D_TXQSAR12
#define TXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_TXQCPR12
#define TXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_TXQCSR13
#define TXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define TXQ_ACTIVE                                (0x1<<15) //15:15
#define TXQ_EPQ_STATE                             (0xf<<8) //11:8
#define TXQ_STOP                                  (0x1<<2) //2:2
#define TXQ_RESUME                                (0x1<<1) //1:1
#define TXQ_START                                 (0x1<<0) //0:0

//U3D_TXQSAR13
#define TXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_TXQCPR13
#define TXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_TXQCSR14
#define TXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define TXQ_ACTIVE                                (0x1<<15) //15:15
#define TXQ_EPQ_STATE                             (0xf<<8) //11:8
#define TXQ_STOP                                  (0x1<<2) //2:2
#define TXQ_RESUME                                (0x1<<1) //1:1
#define TXQ_START                                 (0x1<<0) //0:0

//U3D_TXQSAR14
#define TXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_TXQCPR14
#define TXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_TXQCSR15
#define TXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define TXQ_ACTIVE                                (0x1<<15) //15:15
#define TXQ_EPQ_STATE                             (0xf<<8) //11:8
#define TXQ_STOP                                  (0x1<<2) //2:2
#define TXQ_RESUME                                (0x1<<1) //1:1
#define TXQ_START                                 (0x1<<0) //0:0

//U3D_TXQSAR15
#define TXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_TXQCPR15
#define TXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_RXQCSR1
#define RXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define RXQ_ACTIVE                                (0x1<<15) //15:15
#define RXQ_EPQ_STATE                             (0x1f<<8) //12:8
#define RXQ_STOP                                  (0x1<<2) //2:2
#define RXQ_RESUME                                (0x1<<1) //1:1
#define RXQ_START                                 (0x1<<0) //0:0

//U3D_RXQSAR1
#define RXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_RXQCPR1
#define RXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_RXQLDPR1
#define RXQ_LAST_DONE_PTR                         (0x3fffffff<<2) //31:2

//U3D_RXQCSR2
#define RXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define RXQ_ACTIVE                                (0x1<<15) //15:15
#define RXQ_EPQ_STATE                             (0x1f<<8) //12:8
#define RXQ_STOP                                  (0x1<<2) //2:2
#define RXQ_RESUME                                (0x1<<1) //1:1
#define RXQ_START                                 (0x1<<0) //0:0

//U3D_RXQSAR2
#define RXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_RXQCPR2
#define RXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_RXQLDPR2
#define RXQ_LAST_DONE_PTR                         (0x3fffffff<<2) //31:2

//U3D_RXQCSR3
#define RXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define RXQ_ACTIVE                                (0x1<<15) //15:15
#define RXQ_EPQ_STATE                             (0x1f<<8) //12:8
#define RXQ_STOP                                  (0x1<<2) //2:2
#define RXQ_RESUME                                (0x1<<1) //1:1
#define RXQ_START                                 (0x1<<0) //0:0

//U3D_RXQSAR3
#define RXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_RXQCPR3
#define RXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_RXQLDPR3
#define RXQ_LAST_DONE_PTR                         (0x3fffffff<<2) //31:2

//U3D_RXQCSR4
#define RXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define RXQ_ACTIVE                                (0x1<<15) //15:15
#define RXQ_EPQ_STATE                             (0x1f<<8) //12:8
#define RXQ_STOP                                  (0x1<<2) //2:2
#define RXQ_RESUME                                (0x1<<1) //1:1
#define RXQ_START                                 (0x1<<0) //0:0

//U3D_RXQSAR4
#define RXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_RXQCPR4
#define RXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_RXQLDPR4
#define RXQ_LAST_DONE_PTR                         (0x3fffffff<<2) //31:2

//U3D_RXQCSR5
#define RXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define RXQ_ACTIVE                                (0x1<<15) //15:15
#define RXQ_EPQ_STATE                             (0x1f<<8) //12:8
#define RXQ_STOP                                  (0x1<<2) //2:2
#define RXQ_RESUME                                (0x1<<1) //1:1
#define RXQ_START                                 (0x1<<0) //0:0

//U3D_RXQSAR5
#define RXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_RXQCPR5
#define RXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_RXQLDPR5
#define RXQ_LAST_DONE_PTR                         (0x3fffffff<<2) //31:2

//U3D_RXQCSR6
#define RXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define RXQ_ACTIVE                                (0x1<<15) //15:15
#define RXQ_EPQ_STATE                             (0x1f<<8) //12:8
#define RXQ_STOP                                  (0x1<<2) //2:2
#define RXQ_RESUME                                (0x1<<1) //1:1
#define RXQ_START                                 (0x1<<0) //0:0

//U3D_RXQSAR6
#define RXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_RXQCPR6
#define RXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_RXQLDPR6
#define RXQ_LAST_DONE_PTR                         (0x3fffffff<<2) //31:2

//U3D_RXQCSR7
#define RXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define RXQ_ACTIVE                                (0x1<<15) //15:15
#define RXQ_EPQ_STATE                             (0x1f<<8) //12:8
#define RXQ_STOP                                  (0x1<<2) //2:2
#define RXQ_RESUME                                (0x1<<1) //1:1
#define RXQ_START                                 (0x1<<0) //0:0

//U3D_RXQSAR7
#define RXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_RXQCPR7
#define RXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_RXQLDPR7
#define RXQ_LAST_DONE_PTR                         (0x3fffffff<<2) //31:2

//U3D_RXQCSR8
#define RXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define RXQ_ACTIVE                                (0x1<<15) //15:15
#define RXQ_EPQ_STATE                             (0x1f<<8) //12:8
#define RXQ_STOP                                  (0x1<<2) //2:2
#define RXQ_RESUME                                (0x1<<1) //1:1
#define RXQ_START                                 (0x1<<0) //0:0

//U3D_RXQSAR8
#define RXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_RXQCPR8
#define RXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_RXQLDPR8
#define RXQ_LAST_DONE_PTR                         (0x3fffffff<<2) //31:2

//U3D_RXQCSR9
#define RXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define RXQ_ACTIVE                                (0x1<<15) //15:15
#define RXQ_EPQ_STATE                             (0x1f<<8) //12:8
#define RXQ_STOP                                  (0x1<<2) //2:2
#define RXQ_RESUME                                (0x1<<1) //1:1
#define RXQ_START                                 (0x1<<0) //0:0

//U3D_RXQSAR9
#define RXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_RXQCPR9
#define RXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_RXQLDPR9
#define RXQ_LAST_DONE_PTR                         (0x3fffffff<<2) //31:2

//U3D_RXQCSR10
#define RXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define RXQ_ACTIVE                                (0x1<<15) //15:15
#define RXQ_EPQ_STATE                             (0x1f<<8) //12:8
#define RXQ_STOP                                  (0x1<<2) //2:2
#define RXQ_RESUME                                (0x1<<1) //1:1
#define RXQ_START                                 (0x1<<0) //0:0

//U3D_RXQSAR10
#define RXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_RXQCPR10
#define RXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_RXQLDPR10
#define RXQ_LAST_DONE_PTR                         (0x3fffffff<<2) //31:2

//U3D_RXQCSR11
#define RXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define RXQ_ACTIVE                                (0x1<<15) //15:15
#define RXQ_EPQ_STATE                             (0x1f<<8) //12:8
#define RXQ_STOP                                  (0x1<<2) //2:2
#define RXQ_RESUME                                (0x1<<1) //1:1
#define RXQ_START                                 (0x1<<0) //0:0

//U3D_RXQSAR11
#define RXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_RXQCPR11
#define RXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_RXQLDPR11
#define RXQ_LAST_DONE_PTR                         (0x3fffffff<<2) //31:2

//U3D_RXQCSR12
#define RXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define RXQ_ACTIVE                                (0x1<<15) //15:15
#define RXQ_EPQ_STATE                             (0x1f<<8) //12:8
#define RXQ_STOP                                  (0x1<<2) //2:2
#define RXQ_RESUME                                (0x1<<1) //1:1
#define RXQ_START                                 (0x1<<0) //0:0

//U3D_RXQSAR12
#define RXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_RXQCPR12
#define RXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_RXQLDPR12
#define RXQ_LAST_DONE_PTR                         (0x3fffffff<<2) //31:2

//U3D_RXQCSR13
#define RXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define RXQ_ACTIVE                                (0x1<<15) //15:15
#define RXQ_EPQ_STATE                             (0x1f<<8) //12:8
#define RXQ_STOP                                  (0x1<<2) //2:2
#define RXQ_RESUME                                (0x1<<1) //1:1
#define RXQ_START                                 (0x1<<0) //0:0

//U3D_RXQSAR13
#define RXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_RXQCPR13
#define RXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_RXQLDPR13
#define RXQ_LAST_DONE_PTR                         (0x3fffffff<<2) //31:2

//U3D_RXQCSR14
#define RXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define RXQ_ACTIVE                                (0x1<<15) //15:15
#define RXQ_EPQ_STATE                             (0x1f<<8) //12:8
#define RXQ_STOP                                  (0x1<<2) //2:2
#define RXQ_RESUME                                (0x1<<1) //1:1
#define RXQ_START                                 (0x1<<0) //0:0

//U3D_RXQSAR14
#define RXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_RXQCPR14
#define RXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_RXQLDPR14
#define RXQ_LAST_DONE_PTR                         (0x3fffffff<<2) //31:2

//U3D_RXQCSR15
#define RXQ_DMGR_DMSM_CS                          (0xf<<16) //19:16
#define RXQ_ACTIVE                                (0x1<<15) //15:15
#define RXQ_EPQ_STATE                             (0x1f<<8) //12:8
#define RXQ_STOP                                  (0x1<<2) //2:2
#define RXQ_RESUME                                (0x1<<1) //1:1
#define RXQ_START                                 (0x1<<0) //0:0

//U3D_RXQSAR15
#define RXQ_START_ADDR                            (0x3fffffff<<2) //31:2

//U3D_RXQCPR15
#define RXQ_CUR_GPD_ADDR                          (0x3fffffff<<2) //31:2

//U3D_RXQLDPR15
#define RXQ_LAST_DONE_PTR                         (0x3fffffff<<2) //31:2

//U3D_QISAR0
#define RXQ_DONE_INT                              (0x7fff<<17) //31:17
#define TXQ_DONE_INT                              (0x7fff<<1) //15:1

//U3D_QIER0
#define RXQ_DONE_IER                              (0x7fff<<17) //31:17
#define TXQ_DONE_IER                              (0x7fff<<1) //15:1

//U3D_QIESR0
#define RXQ_DONE_IESR                             (0x7fff<<17) //31:17
#define TXQ_DONE_IESR                             (0x7fff<<1) //15:1

//U3D_QIECR0
#define RXQ_DONE_IECR                             (0x7fff<<17) //31:17
#define TXQ_DONE_IECR                             (0x7fff<<1) //15:1

//U3D_QISAR1
#define RXQ_ZLPERR_INT                            (0x1<<20) //20:20
#define RXQ_LENERR_INT                            (0x1<<18) //18:18
#define RXQ_CSERR_INT                             (0x1<<17) //17:17
#define RXQ_EMPTY_INT                             (0x1<<16) //16:16
#define TXQ_LENERR_INT                            (0x1<<2) //2:2
#define TXQ_CSERR_INT                             (0x1<<1) //1:1
#define TXQ_EMPTY_INT                             (0x1<<0) //0:0

//U3D_QIER1
#define RXQ_ZLPERR_IER                            (0x1<<20) //20:20
#define RXQ_LENERR_IER                            (0x1<<18) //18:18
#define RXQ_CSERR_IER                             (0x1<<17) //17:17
#define RXQ_EMPTY_IER                             (0x1<<16) //16:16
#define TXQ_LENERR_IER                            (0x1<<2) //2:2
#define TXQ_CSERR_IER                             (0x1<<1) //1:1
#define TXQ_EMPTY_IER                             (0x1<<0) //0:0

//U3D_QIESR1
#define RXQ_ZLPERR_IESR                           (0x1<<20) //20:20
#define RXQ_LENERR_IESR                           (0x1<<18) //18:18
#define RXQ_CSERR_IESR                            (0x1<<17) //17:17
#define RXQ_EMPTY_IESR                            (0x1<<16) //16:16
#define TXQ_LENERR_IESR                           (0x1<<2) //2:2
#define TXQ_CSERR_IESR                            (0x1<<1) //1:1
#define TXQ_EMPTY_IESR                            (0x1<<0) //0:0

//U3D_QIECR1
#define RXQ_ZLPERR_IECR                           (0x1<<20) //20:20
#define RXQ_LENERR_IECR                           (0x1<<18) //18:18
#define RXQ_CSERR_IECR                            (0x1<<17) //17:17
#define RXQ_EMPTY_IECR                            (0x1<<16) //16:16
#define TXQ_LENERR_IECR                           (0x1<<2) //2:2
#define TXQ_CSERR_IECR                            (0x1<<1) //1:1
#define TXQ_EMPTY_IECR                            (0x1<<0) //0:0

//U3D_QEMIR
#define RXQ_EMPTY_MASK                            (0x7fff<<17) //31:17
#define TXQ_EMPTY_MASK                            (0x7fff<<1) //15:1

//U3D_QEMIER
#define RXQ_EMPTY_IER_MASK                        (0x7fff<<17) //31:17
#define TXQ_EMPTY_IER_MASK                        (0x7fff<<1) //15:1

//U3D_QEMIESR
#define RXQ_EMPTY_IESR_MASK                       (0x7fff<<17) //31:17
#define TXQ_EMPTY_IESR_MASK                       (0x7fff<<1) //15:1

//U3D_QEMIECR
#define RXQ_EMPTY_IECR_MASK                       (0x7fff<<17) //31:17
#define TXQ_EMPTY_IECR_MASK                       (0x7fff<<1) //15:1

//U3D_TQERRIR0
#define TXQ_LENERR_MASK                           (0x7fff<<17) //31:17
#define TXQ_CSERR_MASK                            (0x7fff<<1) //15:1

//U3D_TQERRIER0
#define TXQ_LENERR_IER_MASK                       (0x7fff<<17) //31:17
#define TXQ_CSERR_IER_MASK                        (0x7fff<<1) //15:1

//U3D_TQERRIESR0
#define TXQ_LENERR_IESR_MASK                      (0x7fff<<17) //31:17
#define TXQ_CSERR_IESR_MASK                       (0x7fff<<1) //15:1

//U3D_TQERRIECR0
#define TXQ_LENERR_IECR_MASK                      (0x7fff<<17) //31:17
#define TXQ_CSERR_IECR_MASK                       (0x7fff<<1) //15:1

//U3D_RQERRIR0
#define RXQ_LENERR_MASK                           (0x7fff<<17) //31:17
#define RXQ_CSERR_MASK                            (0x7fff<<1) //15:1

//U3D_RQERRIER0
#define RXQ_LENERR_IER_MASK                       (0x7fff<<17) //31:17
#define RXQ_CSERR_IER_MASK                        (0x7fff<<1) //15:1

//U3D_RQERRIESR0
#define RXQ_LENERR_IESR_MASK                      (0x7fff<<17) //31:17
#define RXQ_CSERR_IESR_MASK                       (0x7fff<<1) //15:1

//U3D_RQERRIECR0
#define RXQ_LENERR_IECR_MASK                      (0x7fff<<17) //31:17
#define RXQ_CSERR_IECR_MASK                       (0x7fff<<1) //15:1

//U3D_RQERRIR1
#define RXQ_ZLPERR_MASK                           (0x7fff<<17) //31:17

//U3D_RQERRIER1
#define RXQ_ZLPERR_IER_MASK                       (0x7fff<<17) //31:17

//U3D_RQERRIESR1
#define RXQ_ZLPERR_IESR_MASK                      (0x7fff<<17) //31:17

//U3D_RQERRIECR1
#define RXQ_ZLPERR_IECR_MASK                      (0x7fff<<17) //31:17

//U3D_CAP_EP0FFSZ
#define CAP_EP0FFSZ                               (0xffffffff<<0) //31:0

//U3D_CAP_EPNTXFFSZ
#define CAP_EPNTXFFSZ                             (0xffffffff<<0) //31:0

//U3D_CAP_EPNRXFFSZ
#define CAP_EPNRXFFSZ                             (0xffffffff<<0) //31:0

//U3D_CAP_EPINFO
#define CAP_RX_EP_NUM                             (0x1f<<8) //12:8
#define CAP_TX_EP_NUM                             (0x1f<<0) //4:0

//U3D_CAP_TX_SLOT1
#define CAP_TX_SLOT3                              (0x3f<<24) //29:24
#define CAP_TX_SLOT2                              (0x3f<<16) //21:16
#define CAP_TX_SLOT1                              (0x3f<<8) //13:8
#define RSV                                       (0x3f<<0) //5:0

//U3D_CAP_TX_SLOT2
#define CAP_TX_SLOT7                              (0x3f<<24) //29:24
#define CAP_TX_SLOT6                              (0x3f<<16) //21:16
#define CAP_TX_SLOT5                              (0x3f<<8) //13:8
#define CAP_TX_SLOT4                              (0x3f<<0) //5:0

//U3D_CAP_TX_SLOT3
#define CAP_TX_SLOT11                             (0x3f<<24) //29:24
#define CAP_TX_SLOT10                             (0x3f<<16) //21:16
#define CAP_TX_SLOT9                              (0x3f<<8) //13:8
#define CAP_TX_SLOT8                              (0x3f<<0) //5:0

//U3D_CAP_TX_SLOT4
#define CAP_TX_SLOT15                             (0x3f<<24) //29:24
#define CAP_TX_SLOT14                             (0x3f<<16) //21:16
#define CAP_TX_SLOT13                             (0x3f<<8) //13:8
#define CAP_TX_SLOT12                             (0x3f<<0) //5:0

//U3D_CAP_RX_SLOT1
#define CAP_RX_SLOT3                              (0x3f<<24) //29:24
#define CAP_RX_SLOT2                              (0x3f<<16) //21:16
#define CAP_RX_SLOT1                              (0x3f<<8) //13:8
#define RSV                                       (0x3f<<0) //5:0

//U3D_CAP_RX_SLOT2
#define CAP_RX_SLOT7                              (0x3f<<24) //29:24
#define CAP_RX_SLOT6                              (0x3f<<16) //21:16
#define CAP_RX_SLOT5                              (0x3f<<8) //13:8
#define CAP_RX_SLOT4                              (0x3f<<0) //5:0

//U3D_CAP_RX_SLOT3
#define CAP_RX_SLOT11                             (0x3f<<24) //29:24
#define CAP_RX_SLOT10                             (0x3f<<16) //21:16
#define CAP_RX_SLOT9                              (0x3f<<8) //13:8
#define CAP_RX_SLOT8                              (0x3f<<0) //5:0

//U3D_CAP_RX_SLOT4
#define CAP_RX_SLOT15                             (0x3f<<24) //29:24
#define CAP_RX_SLOT14                             (0x3f<<16) //21:16
#define CAP_RX_SLOT13                             (0x3f<<8) //13:8
#define CAP_RX_SLOT12                             (0x3f<<0) //5:0

//U3D_MISC_CTRL
#define DMA_BUS_CK_GATE_DIS                       (0x1<<2) //2:2
#define VBUS_ON                                   (0x1<<1) //1:1
#define VBUS_FRC_EN                               (0x1<<0) //0:0


/* SSUSB_DEV FIELD OFFSET DEFINITION */

//U3D_LV1ISR
#define EP_CTRL_INTR_OFST                         (5)
#define MAC2_INTR_OFST                            (4)
#define DMA_INTR_OFST                             (3)
#define MAC3_INTR_OFST                            (2)
#define QMU_INTR_OFST                             (1)
#define BMU_INTR_OFST                             (0)

//U3D_LV1IER
#define LV1IER_OFST                               (0)

//U3D_LV1IESR
#define LV1IESR_OFST                              (0)

//U3D_LV1IECR
#define LV1IECR_OFST                              (0)

//U3D_AXI_WR_DMA_CFG
#define AXI_WR_ULTRA_NUM_OFST                     (24)
#define AXI_WR_PRE_ULTRA_NUM_OFST                 (16)
#define AXI_WR_ULTRA_EN_OFST                      (0)

//U3D_AXI_RD_DMA_CFG
#define AXI_RD_ULTRA_NUM_OFST                     (24)
#define AXI_RD_PRE_ULTRA_NUM_OFST                 (16)
#define AXI_RD_ULTRA_EN_OFST                      (0)

//U3D_MAC_U1_EN_CTRL
#define EXIT_BY_ERDY_DIS_OFST                     (31)
#define ACCEPT_BMU_RX_EMPTY_CHK_OFST              (20)
#define ACCEPT_BMU_TX_EMPTY_CHK_OFST              (19)
#define ACCEPT_RXQ_INACTIVE_CHK_OFST              (18)
#define ACCEPT_TXQ_INACTIVE_CHK_OFST              (17)
#define ACCEPT_EP0_INACTIVE_CHK_OFST              (16)
#define REQUEST_BMU_RX_EMPTY_CHK_OFST             (4)
#define REQUEST_BMU_TX_EMPTY_CHK_OFST             (3)
#define REQUEST_RXQ_INACTIVE_CHK_OFST             (2)
#define REQUEST_TXQ_INACTIVE_CHK_OFST             (1)
#define REQUEST_EP0_INACTIVE_CHK_OFST             (0)

//U3D_MAC_U2_EN_CTRL
#define EXIT_BY_ERDY_DIS_OFST                     (31)
#define ACCEPT_BMU_RX_EMPTY_CHK_OFST              (20)
#define ACCEPT_BMU_TX_EMPTY_CHK_OFST              (19)
#define ACCEPT_RXQ_INACTIVE_CHK_OFST              (18)
#define ACCEPT_TXQ_INACTIVE_CHK_OFST              (17)
#define ACCEPT_EP0_INACTIVE_CHK_OFST              (16)
#define REQUEST_BMU_RX_EMPTY_CHK_OFST             (4)
#define REQUEST_BMU_TX_EMPTY_CHK_OFST             (3)
#define REQUEST_RXQ_INACTIVE_CHK_OFST             (2)
#define REQUEST_TXQ_INACTIVE_CHK_OFST             (1)
#define REQUEST_EP0_INACTIVE_CHK_OFST             (0)

//U3D_SRAM_DBG_CTRL
#define EPNRX_SRAM_DEBUG_MODE_OFST                (2)
#define EPNTX_SRAM_DEBUG_MODE_OFST                (1)
#define EP0_SRAM_DEBUG_MODE_OFST                  (0)

//U3D_SRAM_DBG_CTRL_1
#define SRAM_DEBUG_FIFOSEGSIZE_OFST               (24)
#define SRAM_DEBUG_SLOT_OFST                      (16)
#define SRAM_DEBUG_DP_COUNT_OFST                  (0)

//U3D_RISC_SIZE
#define RISC_SIZE_OFST                            (0)

//U3D_WRBUF_ERR_STS
#define RX_RDBUF_ERR_STS_OFST                     (17)
#define TX_WRBUF_ERR_STS_OFST                     (1)

//U3D_BUF_ERR_EN
#define RX_RDBUF_ERR_EN_OFST                      (17)
#define TX_WRBUF_ERR_EN_OFST                      (1)

//U3D_EPISR
#define EPRISR_OFST                               (17)
#define SETUPENDISR_OFST                          (16)
#define EPTISR_OFST                               (1)
#define EP0ISR_OFST                               (0)

//U3D_EPIER
#define EPRIER_OFST                               (17)
#define SETUPENDIER_OFST                          (16)
#define EPTIER_OFST                               (1)
#define EP0IER_OFST                               (0)

//U3D_EPIESR
#define EPRIESR_OFST                              (17)
#define SETUPENDIESR_OFST                         (16)
#define EPTIESR_OFST                              (1)
#define EP0IESR_OFST                              (0)

//U3D_EPIECR
#define EPRISR_OFST                               (17)
#define SETUPENDIECR_OFST                         (16)
#define EPTIECR_OFST                              (1)
#define EP0IECR_OFST                              (0)

//U3D_DMAISR
#define RXDMAISR_OFST                             (2)
#define TXDMAISR_OFST                             (1)
#define EP0DMAISR_OFST                            (0)

//U3D_DMAIER
#define RXDMAIER_OFST                             (2)
#define TXDMAIER_OFST                             (1)
#define EP0DMAER_OFST                             (0)

//U3D_DMAIESR
#define RXDMAIESR_OFST                            (2)
#define TXDMAIESR_OFST                            (1)
#define EP0DMAIESR_OFST                           (0)

//U3D_DMAIECR
#define RXDMAIECR_OFST                            (2)
#define TXDMAIECR_OFST                            (1)
#define EP0DMAIECR_OFST                           (0)

//U3D_EP0DMACTRL
#define FFSTRADDR0_OFST                           (16)
#define ENDPNT_OFST                               (4)
#define INTEN_OFST                                (3)
#define DMA_DIR_OFST                              (1)
#define DMA_EN_OFST                               (0)

//U3D_EP0DMASTRADDR
#define DMASTRADDR0_OFST                          (0)

//U3D_EP0DMATFRCOUNT
#define DMATFRCNT0_OFST                           (0)

//U3D_EP0DMARLCOUNT
#define EP0_DMALIMITER_OFST                       (28)
#define DMA_FAKE_OFST                             (27)
#define DMA_BURST_OFST                            (24)
#define AXI_DMA_OUTSTAND_NUM_OFST                 (20)
#define AXI_DMA_COHERENCE_OFST                    (19)
#define AXI_DMA_IOMMU_OFST                        (18)
#define AXI_DMA_CACHEABLE_OFST                    (17)
#define AXI_DMA_ULTRA_EN_OFST                     (16)
#define AXI_DMA_ULTRA_NUM_OFST                    (8)
#define AXI_DMA_PRE_ULTRA_NUM_OFST                (0)

//U3D_TXDMACTRL
#define FFSTRADDR_OFST                            (16)
#define ENDPNT_OFST                               (4)
#define INTEN_OFST                                (3)
#define DMA_DIR_OFST                              (1)
#define DMA_EN_OFST                               (0)

//U3D_TXDMASTRADDR
#define DMASTRADDR_OFST                           (0)

//U3D_TXDMATRDCNT
#define DMATFRCNT_OFST                            (0)

//U3D_TXDMARLCOUNT
#define DMALIMITER_OFST                           (28)
#define DMA_FAKE_OFST                             (27)
#define DMA_BURST_OFST                            (24)
#define AXI_DMA_OUTSTAND_NUM_OFST                 (20)
#define AXI_DMA_COHERENCE_OFST                    (19)
#define AXI_DMA_IOMMU_OFST                        (18)
#define AXI_DMA_CACHEABLE_OFST                    (17)
#define AXI_DMA_ULTRA_EN_OFST                     (16)
#define AXI_DMA_ULTRA_NUM_OFST                    (8)
#define AXI_DMA_PRE_ULTRA_NUM_OFST                (0)

//U3D_RXDMACTRL
#define FFSTRADDR_OFST                            (16)
#define ENDPNT_OFST                               (4)
#define INTEN_OFST                                (3)
#define DMA_DIR_OFST                              (1)
#define DMA_EN_OFST                               (0)

//U3D_RXDMASTRADDR
#define DMASTRADDR_OFST                           (0)

//U3D_RXDMATRDCNT
#define DMATFRCNT_OFST                            (0)

//U3D_RXDMARLCOUNT
#define DMA_NON_BUF_OFST                          (31)
#define DMALIMITER_OFST                           (28)
#define DMA_FAKE_OFST                             (27)
#define DMA_BURST_OFST                            (24)
#define AXI_DMA_OUTSTAND_NUM_OFST                 (20)
#define AXI_DMA_COHERENCE_OFST                    (19)
#define AXI_DMA_IOMMU_OFST                        (18)
#define AXI_DMA_CACHEABLE_OFST                    (17)
#define AXI_DMA_ULTRA_EN_OFST                     (16)
#define AXI_DMA_ULTRA_NUM_OFST                    (8)
#define AXI_DMA_PRE_ULTRA_NUM_OFST                (0)

//U3D_EP0CSR
#define EP0_EP_RESET_OFST                         (31)
#define EP0_AUTOCLEAR_OFST                        (30)
#define EP0_AUTOSET_OFST                          (29)
#define EP0_DMAREQEN_OFST                         (28)
#define EP0_SENDSTALL_OFST                        (25)
#define EP0_FIFOFULL_OFST                         (23)
#define EP0_SENTSTALL_OFST                        (22)
#define EP0_DPHTX_OFST                            (20)
#define EP0_DATAEND_OFST                          (19)
#define EP0_TXPKTRDY_OFST                         (18)
#define EP0_SETUPPKTRDY_OFST                      (17)
#define EP0_RXPKTRDY_OFST                         (16)
#define EP0_MAXPKTSZ0_OFST                        (0)

//U3D_RXCOUNT0
#define EP0_RX_COUNT_OFST                         (0)

//U3D_RESERVED

//U3D_TX1CSR0
#define TX_EP_RESET_OFST                          (31)
#define TX_AUTOSET_OFST                           (30)
#define TX_DMAREQEN_OFST                          (29)
#define TX_FIFOFULL_OFST                          (25)
#define TX_FIFOEMPTY_OFST                         (24)
#define TX_SENTSTALL_OFST                         (22)
#define TX_SENDSTALL_OFST                         (21)
#define TX_TXPKTRDY_OFST                          (16)
#define TX_TXMAXPKTSZ_OFST                        (0)

//U3D_TX1CSR1
#define TX_MULT_OFST                              (22)
#define TX_MAX_PKT_OFST                           (16)
#define TX_SLOT_OFST                              (8)
#define TXTYPE_OFST                               (4)
#define SS_TX_BURST_OFST                          (0)

//U3D_TX1CSR2
#define TXBINTERVAL_OFST                          (24)
#define TXFIFOSEGSIZE_OFST                        (16)
#define TXFIFOADDR_OFST                           (0)

//U3D_TX2CSR0
#define TX_EP_RESET_OFST                          (31)
#define TX_AUTOSET_OFST                           (30)
#define TX_DMAREQEN_OFST                          (29)
#define TX_FIFOFULL_OFST                          (25)
#define TX_FIFOEMPTY_OFST                         (24)
#define TX_SENTSTALL_OFST                         (22)
#define TX_SENDSTALL_OFST                         (21)
#define TX_TXPKTRDY_OFST                          (16)
#define TX_TXMAXPKTSZ_OFST                        (0)

//U3D_TX2CSR1
#define TX_MULT_OFST                              (22)
#define TX_MAX_PKT_OFST                           (16)
#define TX_SLOT_OFST                              (8)
#define TXTYPE_OFST                               (4)
#define SS_TX_BURST_OFST                          (0)

//U3D_TX2CSR2
#define TXBINTERVAL_OFST                          (24)
#define TXFIFOSEGSIZE_OFST                        (16)
#define TXFIFOADDR_OFST                           (0)

//U3D_TX3CSR0
#define TX_EP_RESET_OFST                          (31)
#define TX_AUTOSET_OFST                           (30)
#define TX_DMAREQEN_OFST                          (29)
#define TX_FIFOFULL_OFST                          (25)
#define TX_FIFOEMPTY_OFST                         (24)
#define TX_SENTSTALL_OFST                         (22)
#define TX_SENDSTALL_OFST                         (21)
#define TX_TXPKTRDY_OFST                          (16)
#define TX_TXMAXPKTSZ_OFST                        (0)

//U3D_TX3CSR1
#define TX_MULT_OFST                              (22)
#define TX_MAX_PKT_OFST                           (16)
#define TX_SLOT_OFST                              (8)
#define TXTYPE_OFST                               (4)
#define SS_TX_BURST_OFST                          (0)

//U3D_TX3CSR2
#define TXBINTERVAL_OFST                          (24)
#define TXFIFOSEGSIZE_OFST                        (16)
#define TXFIFOADDR_OFST                           (0)

//U3D_TX4CSR0
#define TX_EP_RESET_OFST                          (31)
#define TX_AUTOSET_OFST                           (30)
#define TX_DMAREQEN_OFST                          (29)
#define TX_FIFOFULL_OFST                          (25)
#define TX_FIFOEMPTY_OFST                         (24)
#define TX_SENTSTALL_OFST                         (22)
#define TX_SENDSTALL_OFST                         (21)
#define TX_TXPKTRDY_OFST                          (16)
#define TX_TXMAXPKTSZ_OFST                        (0)

//U3D_TX4CSR1
#define TX_MULT_OFST                              (22)
#define TX_MAX_PKT_OFST                           (16)
#define TX_SLOT_OFST                              (8)
#define TXTYPE_OFST                               (4)
#define SS_TX_BURST_OFST                          (0)

//U3D_TX4CSR2
#define TXBINTERVAL_OFST                          (24)
#define TXFIFOSEGSIZE_OFST                        (16)
#define TXFIFOADDR_OFST                           (0)

//U3D_TX5CSR0
#define TX_EP_RESET_OFST                          (31)
#define TX_AUTOSET_OFST                           (30)
#define TX_DMAREQEN_OFST                          (29)
#define TX_FIFOFULL_OFST                          (25)
#define TX_FIFOEMPTY_OFST                         (24)
#define TX_SENTSTALL_OFST                         (22)
#define TX_SENDSTALL_OFST                         (21)
#define TX_TXPKTRDY_OFST                          (16)
#define TX_TXMAXPKTSZ_OFST                        (0)

//U3D_TX5CSR1
#define TX_MULT_OFST                              (22)
#define TX_MAX_PKT_OFST                           (16)
#define TX_SLOT_OFST                              (8)
#define TXTYPE_OFST                               (4)
#define SS_TX_BURST_OFST                          (0)

//U3D_TX5CSR2
#define TXBINTERVAL_OFST                          (24)
#define TXFIFOSEGSIZE_OFST                        (16)
#define TXFIFOADDR_OFST                           (0)

//U3D_TX6CSR0
#define TX_EP_RESET_OFST                          (31)
#define TX_AUTOSET_OFST                           (30)
#define TX_DMAREQEN_OFST                          (29)
#define TX_FIFOFULL_OFST                          (25)
#define TX_FIFOEMPTY_OFST                         (24)
#define TX_SENTSTALL_OFST                         (22)
#define TX_SENDSTALL_OFST                         (21)
#define TX_TXPKTRDY_OFST                          (16)
#define TX_TXMAXPKTSZ_OFST                        (0)

//U3D_TX6CSR1
#define TX_MULT_OFST                              (22)
#define TX_MAX_PKT_OFST                           (16)
#define TX_SLOT_OFST                              (8)
#define TXTYPE_OFST                               (4)
#define SS_TX_BURST_OFST                          (0)

//U3D_TX6CSR2
#define TXBINTERVAL_OFST                          (24)
#define TXFIFOSEGSIZE_OFST                        (16)
#define TXFIFOADDR_OFST                           (0)

//U3D_TX7CSR0
#define TX_EP_RESET_OFST                          (31)
#define TX_AUTOSET_OFST                           (30)
#define TX_DMAREQEN_OFST                          (29)
#define TX_FIFOFULL_OFST                          (25)
#define TX_FIFOEMPTY_OFST                         (24)
#define TX_SENTSTALL_OFST                         (22)
#define TX_SENDSTALL_OFST                         (21)
#define TX_TXPKTRDY_OFST                          (16)
#define TX_TXMAXPKTSZ_OFST                        (0)

//U3D_TX7CSR1
#define TX_MULT_OFST                              (22)
#define TX_MAX_PKT_OFST                           (16)
#define TX_SLOT_OFST                              (8)
#define TXTYPE_OFST                               (4)
#define SS_TX_BURST_OFST                          (0)

//U3D_TX7CSR2
#define TXBINTERVAL_OFST                          (24)
#define TXFIFOSEGSIZE_OFST                        (16)
#define TXFIFOADDR_OFST                           (0)

//U3D_TX8CSR0
#define TX_EP_RESET_OFST                          (31)
#define TX_AUTOSET_OFST                           (30)
#define TX_DMAREQEN_OFST                          (29)
#define TX_FIFOFULL_OFST                          (25)
#define TX_FIFOEMPTY_OFST                         (24)
#define TX_SENTSTALL_OFST                         (22)
#define TX_SENDSTALL_OFST                         (21)
#define TX_TXPKTRDY_OFST                          (16)
#define TX_TXMAXPKTSZ_OFST                        (0)

//U3D_TX8CSR1
#define TX_MULT_OFST                              (22)
#define TX_MAX_PKT_OFST                           (16)
#define TX_SLOT_OFST                              (8)
#define TXTYPE_OFST                               (4)
#define SS_TX_BURST_OFST                          (0)

//U3D_TX8CSR2
#define TXBINTERVAL_OFST                          (24)
#define TXFIFOSEGSIZE_OFST                        (16)
#define TXFIFOADDR_OFST                           (0)

//U3D_TX9CSR0
#define TX_EP_RESET_OFST                          (31)
#define TX_AUTOSET_OFST                           (30)
#define TX_DMAREQEN_OFST                          (29)
#define TX_FIFOFULL_OFST                          (25)
#define TX_FIFOEMPTY_OFST                         (24)
#define TX_SENTSTALL_OFST                         (22)
#define TX_SENDSTALL_OFST                         (21)
#define TX_TXPKTRDY_OFST                          (16)
#define TX_TXMAXPKTSZ_OFST                        (0)

//U3D_TX9CSR1
#define TX_MULT_OFST                              (22)
#define TX_MAX_PKT_OFST                           (16)
#define TX_SLOT_OFST                              (8)
#define TXTYPE_OFST                               (4)
#define SS_TX_BURST_OFST                          (0)

//U3D_TX9CSR2
#define TXBINTERVAL_OFST                          (24)
#define TXFIFOSEGSIZE_OFST                        (16)
#define TXFIFOADDR_OFST                           (0)

//U3D_TX10CSR0
#define TX_EP_RESET_OFST                          (31)
#define TX_AUTOSET_OFST                           (30)
#define TX_DMAREQEN_OFST                          (29)
#define TX_FIFOFULL_OFST                          (25)
#define TX_FIFOEMPTY_OFST                         (24)
#define TX_SENTSTALL_OFST                         (22)
#define TX_SENDSTALL_OFST                         (21)
#define TX_TXPKTRDY_OFST                          (16)
#define TX_TXMAXPKTSZ_OFST                        (0)

//U3D_TX10CSR1
#define TX_MULT_OFST                              (22)
#define TX_MAX_PKT_OFST                           (16)
#define TX_SLOT_OFST                              (8)
#define TXTYPE_OFST                               (4)
#define SS_TX_BURST_OFST                          (0)

//U3D_TX10CSR2
#define TXBINTERVAL_OFST                          (24)
#define TXFIFOSEGSIZE_OFST                        (16)
#define TXFIFOADDR_OFST                           (0)

//U3D_TX11CSR0
#define TX_EP_RESET_OFST                          (31)
#define TX_AUTOSET_OFST                           (30)
#define TX_DMAREQEN_OFST                          (29)
#define TX_FIFOFULL_OFST                          (25)
#define TX_FIFOEMPTY_OFST                         (24)
#define TX_SENTSTALL_OFST                         (22)
#define TX_SENDSTALL_OFST                         (21)
#define TX_TXPKTRDY_OFST                          (16)
#define TX_TXMAXPKTSZ_OFST                        (0)

//U3D_TX11CSR1
#define TX_MULT_OFST                              (22)
#define TX_MAX_PKT_OFST                           (16)
#define TX_SLOT_OFST                              (8)
#define TXTYPE_OFST                               (4)
#define SS_TX_BURST_OFST                          (0)

//U3D_TX11CSR2
#define TXBINTERVAL_OFST                          (24)
#define TXFIFOSEGSIZE_OFST                        (16)
#define TXFIFOADDR_OFST                           (0)

//U3D_TX12CSR0
#define TX_EP_RESET_OFST                          (31)
#define TX_AUTOSET_OFST                           (30)
#define TX_DMAREQEN_OFST                          (29)
#define TX_FIFOFULL_OFST                          (25)
#define TX_FIFOEMPTY_OFST                         (24)
#define TX_SENTSTALL_OFST                         (22)
#define TX_SENDSTALL_OFST                         (21)
#define TX_TXPKTRDY_OFST                          (16)
#define TX_TXMAXPKTSZ_OFST                        (0)

//U3D_TX12CSR1
#define TX_MULT_OFST                              (22)
#define TX_MAX_PKT_OFST                           (16)
#define TX_SLOT_OFST                              (8)
#define TXTYPE_OFST                               (4)
#define SS_TX_BURST_OFST                          (0)

//U3D_TX12CSR2
#define TXBINTERVAL_OFST                          (24)
#define TXFIFOSEGSIZE_OFST                        (16)
#define TXFIFOADDR_OFST                           (0)

//U3D_TX13CSR0
#define TX_EP_RESET_OFST                          (31)
#define TX_AUTOSET_OFST                           (30)
#define TX_DMAREQEN_OFST                          (29)
#define TX_FIFOFULL_OFST                          (25)
#define TX_FIFOEMPTY_OFST                         (24)
#define TX_SENTSTALL_OFST                         (22)
#define TX_SENDSTALL_OFST                         (21)
#define TX_TXPKTRDY_OFST                          (16)
#define TX_TXMAXPKTSZ_OFST                        (0)

//U3D_TX13CSR1
#define TX_MULT_OFST                              (22)
#define TX_MAX_PKT_OFST                           (16)
#define TX_SLOT_OFST                              (8)
#define TXTYPE_OFST                               (4)
#define SS_TX_BURST_OFST                          (0)

//U3D_TX13CSR2
#define TXBINTERVAL_OFST                          (24)
#define TXFIFOSEGSIZE_OFST                        (16)
#define TXFIFOADDR_OFST                           (0)

//U3D_TX14CSR0
#define TX_EP_RESET_OFST                          (31)
#define TX_AUTOSET_OFST                           (30)
#define TX_DMAREQEN_OFST                          (29)
#define TX_FIFOFULL_OFST                          (25)
#define TX_FIFOEMPTY_OFST                         (24)
#define TX_SENTSTALL_OFST                         (22)
#define TX_SENDSTALL_OFST                         (21)
#define TX_TXPKTRDY_OFST                          (16)
#define TX_TXMAXPKTSZ_OFST                        (0)

//U3D_TX14CSR1
#define TX_MULT_OFST                              (22)
#define TX_MAX_PKT_OFST                           (16)
#define TX_SLOT_OFST                              (8)
#define TXTYPE_OFST                               (4)
#define SS_TX_BURST_OFST                          (0)

//U3D_TX14CSR2
#define TXBINTERVAL_OFST                          (24)
#define TXFIFOSEGSIZE_OFST                        (16)
#define TXFIFOADDR_OFST                           (0)

//U3D_TX15CSR0
#define TX_EP_RESET_OFST                          (31)
#define TX_AUTOSET_OFST                           (30)
#define TX_DMAREQEN_OFST                          (29)
#define TX_FIFOFULL_OFST                          (25)
#define TX_FIFOEMPTY_OFST                         (24)
#define TX_SENTSTALL_OFST                         (22)
#define TX_SENDSTALL_OFST                         (21)
#define TX_TXPKTRDY_OFST                          (16)
#define TX_TXMAXPKTSZ_OFST                        (0)

//U3D_TX15CSR1
#define TX_MULT_OFST                              (22)
#define TX_MAX_PKT_OFST                           (16)
#define TX_SLOT_OFST                              (8)
#define TXTYPE_OFST                               (4)
#define SS_TX_BURST_OFST                          (0)

//U3D_TX15CSR2
#define TXBINTERVAL_OFST                          (24)
#define TXFIFOSEGSIZE_OFST                        (16)
#define TXFIFOADDR_OFST                           (0)

//U3D_RX1CSR0
#define RX_EP_RESET_OFST                          (31)
#define RX_AUTOCLEAR_OFST                         (30)
#define RX_DMAREQEN_OFST                          (29)
#define RX_SENTSTALL_OFST                         (22)
#define RX_SENDSTALL_OFST                         (21)
#define RX_FIFOFULL_OFST                          (18)
#define RX_FIFOEMPTY_OFST                         (17)
#define RX_RXPKTRDY_OFST                          (16)
#define RX_RXMAXPKTSZ_OFST                        (0)

//U3D_RX1CSR1
#define RX_MULT_OFST                              (22)
#define RX_MAX_PKT_OFST                           (16)
#define RX_SLOT_OFST                              (8)
#define RX_TYPE_OFST                              (4)
#define SS_RX_BURST_OFST                          (0)

//U3D_RX1CSR2
#define RXBINTERVAL_OFST                          (24)
#define RXFIFOSEGSIZE_OFST                        (16)
#define RXFIFOADDR_OFST                           (0)

//U3D_RX1CSR3
#define EP_RX_COUNT_OFST                          (16)

//U3D_RX2CSR0
#define RX_EP_RESET_OFST                          (31)
#define RX_AUTOCLEAR_OFST                         (30)
#define RX_DMAREQEN_OFST                          (29)
#define RX_SENTSTALL_OFST                         (22)
#define RX_SENDSTALL_OFST                         (21)
#define RX_FIFOFULL_OFST                          (18)
#define RX_FIFOEMPTY_OFST                         (17)
#define RX_RXPKTRDY_OFST                          (16)
#define RX_RXMAXPKTSZ_OFST                        (0)

//U3D_RX2CSR1
#define RX_MULT_OFST                              (22)
#define RX_MAX_PKT_OFST                           (16)
#define RX_SLOT_OFST                              (8)
#define RX_TYPE_OFST                              (4)
#define SS_RX_BURST_OFST                          (0)

//U3D_RX2CSR2
#define RXBINTERVAL_OFST                          (24)
#define RXFIFOSEGSIZE_OFST                        (16)
#define RXFIFOADDR_OFST                           (0)

//U3D_RX2CSR3
#define EP_RX_COUNT_OFST                          (16)

//U3D_RX3CSR0
#define RX_EP_RESET_OFST                          (31)
#define RX_AUTOCLEAR_OFST                         (30)
#define RX_DMAREQEN_OFST                          (29)
#define RX_SENTSTALL_OFST                         (22)
#define RX_SENDSTALL_OFST                         (21)
#define RX_FIFOFULL_OFST                          (18)
#define RX_FIFOEMPTY_OFST                         (17)
#define RX_RXPKTRDY_OFST                          (16)
#define RX_RXMAXPKTSZ_OFST                        (0)

//U3D_RX3CSR1
#define RX_MULT_OFST                              (22)
#define RX_MAX_PKT_OFST                           (16)
#define RX_SLOT_OFST                              (8)
#define RX_TYPE_OFST                              (4)
#define SS_RX_BURST_OFST                          (0)

//U3D_RX3CSR2
#define RXBINTERVAL_OFST                          (24)
#define RXFIFOSEGSIZE_OFST                        (16)
#define RXFIFOADDR_OFST                           (0)

//U3D_RX3CSR3
#define EP_RX_COUNT_OFST                          (16)

//U3D_RX4CSR0
#define RX_EP_RESET_OFST                          (31)
#define RX_AUTOCLEAR_OFST                         (30)
#define RX_DMAREQEN_OFST                          (29)
#define RX_SENTSTALL_OFST                         (22)
#define RX_SENDSTALL_OFST                         (21)
#define RX_FIFOFULL_OFST                          (18)
#define RX_FIFOEMPTY_OFST                         (17)
#define RX_RXPKTRDY_OFST                          (16)
#define RX_RXMAXPKTSZ_OFST                        (0)

//U3D_RX4CSR1
#define RX_MULT_OFST                              (22)
#define RX_MAX_PKT_OFST                           (16)
#define RX_SLOT_OFST                              (8)
#define RX_TYPE_OFST                              (4)
#define SS_RX_BURST_OFST                          (0)

//U3D_RX4CSR2
#define RXBINTERVAL_OFST                          (24)
#define RXFIFOSEGSIZE_OFST                        (16)
#define RXFIFOADDR_OFST                           (0)

//U3D_RX4CSR3
#define EP_RX_COUNT_OFST                          (16)

//U3D_RX5CSR0
#define RX_EP_RESET_OFST                          (31)
#define RX_AUTOCLEAR_OFST                         (30)
#define RX_DMAREQEN_OFST                          (29)
#define RX_SENTSTALL_OFST                         (22)
#define RX_SENDSTALL_OFST                         (21)
#define RX_FIFOFULL_OFST                          (18)
#define RX_FIFOEMPTY_OFST                         (17)
#define RX_RXPKTRDY_OFST                          (16)
#define RX_RXMAXPKTSZ_OFST                        (0)

//U3D_RX5CSR1
#define RX_MULT_OFST                              (22)
#define RX_MAX_PKT_OFST                           (16)
#define RX_SLOT_OFST                              (8)
#define RX_TYPE_OFST                              (4)
#define SS_RX_BURST_OFST                          (0)

//U3D_RX5CSR2
#define RXBINTERVAL_OFST                          (24)
#define RXFIFOSEGSIZE_OFST                        (16)
#define RXFIFOADDR_OFST                           (0)

//U3D_RX5CSR3
#define EP_RX_COUNT_OFST                          (16)

//U3D_RX6CSR0
#define RX_EP_RESET_OFST                          (31)
#define RX_AUTOCLEAR_OFST                         (30)
#define RX_DMAREQEN_OFST                          (29)
#define RX_SENTSTALL_OFST                         (22)
#define RX_SENDSTALL_OFST                         (21)
#define RX_FIFOFULL_OFST                          (18)
#define RX_FIFOEMPTY_OFST                         (17)
#define RX_RXPKTRDY_OFST                          (16)
#define RX_RXMAXPKTSZ_OFST                        (0)

//U3D_RX6CSR1
#define RX_MULT_OFST                              (22)
#define RX_MAX_PKT_OFST                           (16)
#define RX_SLOT_OFST                              (8)
#define RX_TYPE_OFST                              (4)
#define SS_RX_BURST_OFST                          (0)

//U3D_RX6CSR2
#define RXBINTERVAL_OFST                          (24)
#define RXFIFOSEGSIZE_OFST                        (16)
#define RXFIFOADDR_OFST                           (0)

//U3D_RX6CSR3
#define EP_RX_COUNT_OFST                          (16)

//U3D_RX7CSR0
#define RX_EP_RESET_OFST                          (31)
#define RX_AUTOCLEAR_OFST                         (30)
#define RX_DMAREQEN_OFST                          (29)
#define RX_SENTSTALL_OFST                         (22)
#define RX_SENDSTALL_OFST                         (21)
#define RX_FIFOFULL_OFST                          (18)
#define RX_FIFOEMPTY_OFST                         (17)
#define RX_RXPKTRDY_OFST                          (16)
#define RX_RXMAXPKTSZ_OFST                        (0)

//U3D_RX7CSR1
#define RX_MULT_OFST                              (22)
#define RX_MAX_PKT_OFST                           (16)
#define RX_SLOT_OFST                              (8)
#define RX_TYPE_OFST                              (4)
#define SS_RX_BURST_OFST                          (0)

//U3D_RX7CSR2
#define RXBINTERVAL_OFST                          (24)
#define RXFIFOSEGSIZE_OFST                        (16)
#define RXFIFOADDR_OFST                           (0)

//U3D_RX7CSR3
#define EP_RX_COUNT_OFST                          (16)

//U3D_RX8CSR0
#define RX_EP_RESET_OFST                          (31)
#define RX_AUTOCLEAR_OFST                         (30)
#define RX_DMAREQEN_OFST                          (29)
#define RX_SENTSTALL_OFST                         (22)
#define RX_SENDSTALL_OFST                         (21)
#define RX_FIFOFULL_OFST                          (18)
#define RX_FIFOEMPTY_OFST                         (17)
#define RX_RXPKTRDY_OFST                          (16)
#define RX_RXMAXPKTSZ_OFST                        (0)

//U3D_RX8CSR1
#define RX_MULT_OFST                              (22)
#define RX_MAX_PKT_OFST                           (16)
#define RX_SLOT_OFST                              (8)
#define RX_TYPE_OFST                              (4)
#define SS_RX_BURST_OFST                          (0)

//U3D_RX8CSR2
#define RXBINTERVAL_OFST                          (24)
#define RXFIFOSEGSIZE_OFST                        (16)
#define RXFIFOADDR_OFST                           (0)

//U3D_RX8CSR3
#define EP_RX_COUNT_OFST                          (16)

//U3D_RX9CSR0
#define RX_EP_RESET_OFST                          (31)
#define RX_AUTOCLEAR_OFST                         (30)
#define RX_DMAREQEN_OFST                          (29)
#define RX_SENTSTALL_OFST                         (22)
#define RX_SENDSTALL_OFST                         (21)
#define RX_FIFOFULL_OFST                          (18)
#define RX_FIFOEMPTY_OFST                         (17)
#define RX_RXPKTRDY_OFST                          (16)
#define RX_RXMAXPKTSZ_OFST                        (0)

//U3D_RX9CSR1
#define RX_MULT_OFST                              (22)
#define RX_MAX_PKT_OFST                           (16)
#define RX_SLOT_OFST                              (8)
#define RX_TYPE_OFST                              (4)
#define SS_RX_BURST_OFST                          (0)

//U3D_RX9CSR2
#define RXBINTERVAL_OFST                          (24)
#define RXFIFOSEGSIZE_OFST                        (16)
#define RXFIFOADDR_OFST                           (0)

//U3D_RX9CSR3
#define EP_RX_COUNT_OFST                          (16)

//U3D_RX10CSR0
#define RX_EP_RESET_OFST                          (31)
#define RX_AUTOCLEAR_OFST                         (30)
#define RX_DMAREQEN_OFST                          (29)
#define RX_SENTSTALL_OFST                         (22)
#define RX_SENDSTALL_OFST                         (21)
#define RX_FIFOFULL_OFST                          (18)
#define RX_FIFOEMPTY_OFST                         (17)
#define RX_RXPKTRDY_OFST                          (16)
#define RX_RXMAXPKTSZ_OFST                        (0)

//U3D_RX10CSR1
#define RX_MULT_OFST                              (22)
#define RX_MAX_PKT_OFST                           (16)
#define RX_SLOT_OFST                              (8)
#define RX_TYPE_OFST                              (4)
#define SS_RX_BURST_OFST                          (0)

//U3D_RX10CSR2
#define RXBINTERVAL_OFST                          (24)
#define RXFIFOSEGSIZE_OFST                        (16)
#define RXFIFOADDR_OFST                           (0)

//U3D_RX10CSR3
#define EP_RX_COUNT_OFST                          (16)

//U3D_RX11CSR0
#define RX_EP_RESET_OFST                          (31)
#define RX_AUTOCLEAR_OFST                         (30)
#define RX_DMAREQEN_OFST                          (29)
#define RX_SENTSTALL_OFST                         (22)
#define RX_SENDSTALL_OFST                         (21)
#define RX_FIFOFULL_OFST                          (18)
#define RX_FIFOEMPTY_OFST                         (17)
#define RX_RXPKTRDY_OFST                          (16)
#define RX_RXMAXPKTSZ_OFST                        (0)

//U3D_RX11CSR1
#define RX_MULT_OFST                              (22)
#define RX_MAX_PKT_OFST                           (16)
#define RX_SLOT_OFST                              (8)
#define RX_TYPE_OFST                              (4)
#define SS_RX_BURST_OFST                          (0)

//U3D_RX11CSR2
#define RXBINTERVAL_OFST                          (24)
#define RXFIFOSEGSIZE_OFST                        (16)
#define RXFIFOADDR_OFST                           (0)

//U3D_RX11CSR3
#define EP_RX_COUNT_OFST                          (16)

//U3D_RX12CSR0
#define RX_EP_RESET_OFST                          (31)
#define RX_AUTOCLEAR_OFST                         (30)
#define RX_DMAREQEN_OFST                          (29)
#define RX_SENTSTALL_OFST                         (22)
#define RX_SENDSTALL_OFST                         (21)
#define RX_FIFOFULL_OFST                          (18)
#define RX_FIFOEMPTY_OFST                         (17)
#define RX_RXPKTRDY_OFST                          (16)
#define RX_RXMAXPKTSZ_OFST                        (0)

//U3D_RX12CSR1
#define RX_MULT_OFST                              (22)
#define RX_MAX_PKT_OFST                           (16)
#define RX_SLOT_OFST                              (8)
#define RX_TYPE_OFST                              (4)
#define SS_RX_BURST_OFST                          (0)

//U3D_RX12CSR2
#define RXBINTERVAL_OFST                          (24)
#define RXFIFOSEGSIZE_OFST                        (16)
#define RXFIFOADDR_OFST                           (0)

//U3D_RX12CSR3
#define EP_RX_COUNT_OFST                          (16)

//U3D_RX13CSR0
#define RX_EP_RESET_OFST                          (31)
#define RX_AUTOCLEAR_OFST                         (30)
#define RX_DMAREQEN_OFST                          (29)
#define RX_SENTSTALL_OFST                         (22)
#define RX_SENDSTALL_OFST                         (21)
#define RX_FIFOFULL_OFST                          (18)
#define RX_FIFOEMPTY_OFST                         (17)
#define RX_RXPKTRDY_OFST                          (16)
#define RX_RXMAXPKTSZ_OFST                        (0)

//U3D_RX13CSR1
#define RX_MULT_OFST                              (22)
#define RX_MAX_PKT_OFST                           (16)
#define RX_SLOT_OFST                              (8)
#define RX_TYPE_OFST                              (4)
#define SS_RX_BURST_OFST                          (0)

//U3D_RX13CSR2
#define RXBINTERVAL_OFST                          (24)
#define RXFIFOSEGSIZE_OFST                        (16)
#define RXFIFOADDR_OFST                           (0)

//U3D_RX13CSR3
#define EP_RX_COUNT_OFST                          (16)

//U3D_RX14CSR0
#define RX_EP_RESET_OFST                          (31)
#define RX_AUTOCLEAR_OFST                         (30)
#define RX_DMAREQEN_OFST                          (29)
#define RX_SENTSTALL_OFST                         (22)
#define RX_SENDSTALL_OFST                         (21)
#define RX_FIFOFULL_OFST                          (18)
#define RX_FIFOEMPTY_OFST                         (17)
#define RX_RXPKTRDY_OFST                          (16)
#define RX_RXMAXPKTSZ_OFST                        (0)

//U3D_RX14CSR1
#define RX_MULT_OFST                              (22)
#define RX_MAX_PKT_OFST                           (16)
#define RX_SLOT_OFST                              (8)
#define RX_TYPE_OFST                              (4)
#define SS_RX_BURST_OFST                          (0)

//U3D_RX14CSR2
#define RXBINTERVAL_OFST                          (24)
#define RXFIFOSEGSIZE_OFST                        (16)
#define RXFIFOADDR_OFST                           (0)

//U3D_RX14CSR3
#define EP_RX_COUNT_OFST                          (16)

//U3D_RX15CSR0
#define RX_EP_RESET_OFST                          (31)
#define RX_AUTOCLEAR_OFST                         (30)
#define RX_DMAREQEN_OFST                          (29)
#define RX_SENTSTALL_OFST                         (22)
#define RX_SENDSTALL_OFST                         (21)
#define RX_FIFOFULL_OFST                          (18)
#define RX_FIFOEMPTY_OFST                         (17)
#define RX_RXPKTRDY_OFST                          (16)
#define RX_RXMAXPKTSZ_OFST                        (0)

//U3D_RX15CSR1
#define RX_MULT_OFST                              (22)
#define RX_MAX_PKT_OFST                           (16)
#define RX_SLOT_OFST                              (8)
#define RX_TYPE_OFST                              (4)
#define SS_RX_BURST_OFST                          (0)

//U3D_RX15CSR2
#define RXBINTERVAL_OFST                          (24)
#define RXFIFOSEGSIZE_OFST                        (16)
#define RXFIFOADDR_OFST                           (0)

//U3D_RX15CSR3
#define EP_RX_COUNT_OFST                          (16)

//U3D_FIFO0
#define BYTE3_OFST                                (24)
#define BYTE2_OFST                                (16)
#define BYTE1_OFST                                (8)
#define BYTE0_OFST                                (0)

//U3D_FIFO1
#define BYTE3_OFST                                (24)
#define BYTE2_OFST                                (16)
#define BYTE1_OFST                                (8)
#define BYTE0_OFST                                (0)

//U3D_FIFO2
#define BYTE3_OFST                                (24)
#define BYTE2_OFST                                (16)
#define BYTE1_OFST                                (8)
#define BYTE0_OFST                                (0)

//U3D_FIFO3
#define BYTE3_OFST                                (24)
#define BYTE2_OFST                                (16)
#define BYTE1_OFST                                (8)
#define BYTE0_OFST                                (0)

//U3D_FIFO4
#define BYTE3_OFST                                (24)
#define BYTE2_OFST                                (16)
#define BYTE1_OFST                                (8)
#define BYTE0_OFST                                (0)

//U3D_FIFO5
#define BYTE3_OFST                                (24)
#define BYTE2_OFST                                (16)
#define BYTE1_OFST                                (8)
#define BYTE0_OFST                                (0)

//U3D_FIFO6
#define BYTE3_OFST                                (24)
#define BYTE2_OFST                                (16)
#define BYTE1_OFST                                (8)
#define BYTE0_OFST                                (0)

//U3D_FIFO7
#define BYTE3_OFST                                (24)
#define BYTE2_OFST                                (16)
#define BYTE1_OFST                                (8)
#define BYTE0_OFST                                (0)

//U3D_FIFO8
#define BYTE3_OFST                                (24)
#define BYTE2_OFST                                (16)
#define BYTE1_OFST                                (8)
#define BYTE0_OFST                                (0)

//U3D_FIFO9
#define BYTE3_OFST                                (24)
#define BYTE2_OFST                                (16)
#define BYTE1_OFST                                (8)
#define BYTE0_OFST                                (0)

//U3D_FIFO10
#define BYTE3_OFST                                (24)
#define BYTE2_OFST                                (16)
#define BYTE1_OFST                                (8)
#define BYTE0_OFST                                (0)

//U3D_FIFO11
#define BYTE3_OFST                                (24)
#define BYTE2_OFST                                (16)
#define BYTE1_OFST                                (8)
#define BYTE0_OFST                                (0)

//U3D_FIFO12
#define BYTE3_OFST                                (24)
#define BYTE2_OFST                                (16)
#define BYTE1_OFST                                (8)
#define BYTE0_OFST                                (0)

//U3D_FIFO13
#define BYTE3_OFST                                (24)
#define BYTE2_OFST                                (16)
#define BYTE1_OFST                                (8)
#define BYTE0_OFST                                (0)

//U3D_FIFO14
#define BYTE3_OFST                                (24)
#define BYTE2_OFST                                (16)
#define BYTE1_OFST                                (8)
#define BYTE0_OFST                                (0)

//U3D_FIFO15
#define BYTE3_OFST                                (24)
#define BYTE2_OFST                                (16)
#define BYTE1_OFST                                (8)
#define BYTE0_OFST                                (0)

//U3D_QCR0
#define RXQ_CS_EN_OFST                            (17)
#define TXQ_CS_EN_OFST                            (1)
#define CS16B_EN_OFST                             (0)

//U3D_QCR1
#define CFG_TX_ZLP_GPD_OFST                       (1)

//U3D_QCR2
#define CFG_TX_ZLP_OFST                           (1)

//U3D_QCR3
#define CFG_RX_COZ_OFST                           (17)
#define CFG_RX_ZLP_OFST                           (1)

//U3D_QGCSR
#define RXQ_EN_OFST                               (17)
#define TXQ_EN_OFST                               (1)

//U3D_TXQCSR1
#define TXQ_DMGR_DMSM_CS_OFST                     (16)
#define TXQ_ACTIVE_OFST                           (15)
#define TXQ_EPQ_STATE_OFST                        (8)
#define TXQ_STOP_OFST                             (2)
#define TXQ_RESUME_OFST                           (1)
#define TXQ_START_OFST                            (0)

//U3D_TXQSAR1
#define TXQ_START_ADDR_OFST                       (2)

//U3D_TXQCPR1
#define TXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_TXQCSR2
#define TXQ_DMGR_DMSM_CS_OFST                     (16)
#define TXQ_ACTIVE_OFST                           (15)
#define TXQ_EPQ_STATE_OFST                        (8)
#define TXQ_STOP_OFST                             (2)
#define TXQ_RESUME_OFST                           (1)
#define TXQ_START_OFST                            (0)

//U3D_TXQSAR2
#define TXQ_START_ADDR_OFST                       (2)

//U3D_TXQCPR2
#define TXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_TXQCSR3
#define TXQ_DMGR_DMSM_CS_OFST                     (16)
#define TXQ_ACTIVE_OFST                           (15)
#define TXQ_EPQ_STATE_OFST                        (8)
#define TXQ_STOP_OFST                             (2)
#define TXQ_RESUME_OFST                           (1)
#define TXQ_START_OFST                            (0)

//U3D_TXQSAR3
#define TXQ_START_ADDR_OFST                       (2)

//U3D_TXQCPR3
#define TXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_TXQCSR4
#define TXQ_DMGR_DMSM_CS_OFST                     (16)
#define TXQ_ACTIVE_OFST                           (15)
#define TXQ_EPQ_STATE_OFST                        (8)
#define TXQ_STOP_OFST                             (2)
#define TXQ_RESUME_OFST                           (1)
#define TXQ_START_OFST                            (0)

//U3D_TXQSAR4
#define TXQ_START_ADDR_OFST                       (2)

//U3D_TXQCPR4
#define TXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_TXQCSR5
#define TXQ_DMGR_DMSM_CS_OFST                     (16)
#define TXQ_ACTIVE_OFST                           (15)
#define TXQ_EPQ_STATE_OFST                        (8)
#define TXQ_STOP_OFST                             (2)
#define TXQ_RESUME_OFST                           (1)
#define TXQ_START_OFST                            (0)

//U3D_TXQSAR5
#define TXQ_START_ADDR_OFST                       (2)

//U3D_TXQCPR5
#define TXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_TXQCSR6
#define TXQ_DMGR_DMSM_CS_OFST                     (16)
#define TXQ_ACTIVE_OFST                           (15)
#define TXQ_EPQ_STATE_OFST                        (8)
#define TXQ_STOP_OFST                             (2)
#define TXQ_RESUME_OFST                           (1)
#define TXQ_START_OFST                            (0)

//U3D_TXQSAR6
#define TXQ_START_ADDR_OFST                       (2)

//U3D_TXQCPR6
#define TXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_TXQCSR7
#define TXQ_DMGR_DMSM_CS_OFST                     (16)
#define TXQ_ACTIVE_OFST                           (15)
#define TXQ_EPQ_STATE_OFST                        (8)
#define TXQ_STOP_OFST                             (2)
#define TXQ_RESUME_OFST                           (1)
#define TXQ_START_OFST                            (0)

//U3D_TXQSAR7
#define TXQ_START_ADDR_OFST                       (2)

//U3D_TXQCPR7
#define TXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_TXQCSR8
#define TXQ_DMGR_DMSM_CS_OFST                     (16)
#define TXQ_ACTIVE_OFST                           (15)
#define TXQ_EPQ_STATE_OFST                        (8)
#define TXQ_STOP_OFST                             (2)
#define TXQ_RESUME_OFST                           (1)
#define TXQ_START_OFST                            (0)

//U3D_TXQSAR8
#define TXQ_START_ADDR_OFST                       (2)

//U3D_TXQCPR8
#define TXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_TXQCSR9
#define TXQ_DMGR_DMSM_CS_OFST                     (16)
#define TXQ_ACTIVE_OFST                           (15)
#define TXQ_EPQ_STATE_OFST                        (8)
#define TXQ_STOP_OFST                             (2)
#define TXQ_RESUME_OFST                           (1)
#define TXQ_START_OFST                            (0)

//U3D_TXQSAR9
#define TXQ_START_ADDR_OFST                       (2)

//U3D_TXQCPR9
#define TXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_TXQCSR10
#define TXQ_DMGR_DMSM_CS_OFST                     (16)
#define TXQ_ACTIVE_OFST                           (15)
#define TXQ_EPQ_STATE_OFST                        (8)
#define TXQ_STOP_OFST                             (2)
#define TXQ_RESUME_OFST                           (1)
#define TXQ_START_OFST                            (0)

//U3D_TXQSAR10
#define TXQ_START_ADDR_OFST                       (2)

//U3D_TXQCPR10
#define TXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_TXQCSR11
#define TXQ_DMGR_DMSM_CS_OFST                     (16)
#define TXQ_ACTIVE_OFST                           (15)
#define TXQ_EPQ_STATE_OFST                        (8)
#define TXQ_STOP_OFST                             (2)
#define TXQ_RESUME_OFST                           (1)
#define TXQ_START_OFST                            (0)

//U3D_TXQSAR11
#define TXQ_START_ADDR_OFST                       (2)

//U3D_TXQCPR11
#define TXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_TXQCSR12
#define TXQ_DMGR_DMSM_CS_OFST                     (16)
#define TXQ_ACTIVE_OFST                           (15)
#define TXQ_EPQ_STATE_OFST                        (8)
#define TXQ_STOP_OFST                             (2)
#define TXQ_RESUME_OFST                           (1)
#define TXQ_START_OFST                            (0)

//U3D_TXQSAR12
#define TXQ_START_ADDR_OFST                       (2)

//U3D_TXQCPR12
#define TXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_TXQCSR13
#define TXQ_DMGR_DMSM_CS_OFST                     (16)
#define TXQ_ACTIVE_OFST                           (15)
#define TXQ_EPQ_STATE_OFST                        (8)
#define TXQ_STOP_OFST                             (2)
#define TXQ_RESUME_OFST                           (1)
#define TXQ_START_OFST                            (0)

//U3D_TXQSAR13
#define TXQ_START_ADDR_OFST                       (2)

//U3D_TXQCPR13
#define TXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_TXQCSR14
#define TXQ_DMGR_DMSM_CS_OFST                     (16)
#define TXQ_ACTIVE_OFST                           (15)
#define TXQ_EPQ_STATE_OFST                        (8)
#define TXQ_STOP_OFST                             (2)
#define TXQ_RESUME_OFST                           (1)
#define TXQ_START_OFST                            (0)

//U3D_TXQSAR14
#define TXQ_START_ADDR_OFST                       (2)

//U3D_TXQCPR14
#define TXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_TXQCSR15
#define TXQ_DMGR_DMSM_CS_OFST                     (16)
#define TXQ_ACTIVE_OFST                           (15)
#define TXQ_EPQ_STATE_OFST                        (8)
#define TXQ_STOP_OFST                             (2)
#define TXQ_RESUME_OFST                           (1)
#define TXQ_START_OFST                            (0)

//U3D_TXQSAR15
#define TXQ_START_ADDR_OFST                       (2)

//U3D_TXQCPR15
#define TXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_RXQCSR1
#define RXQ_DMGR_DMSM_CS_OFST                     (16)
#define RXQ_ACTIVE_OFST                           (15)
#define RXQ_EPQ_STATE_OFST                        (8)
#define RXQ_STOP_OFST                             (2)
#define RXQ_RESUME_OFST                           (1)
#define RXQ_START_OFST                            (0)

//U3D_RXQSAR1
#define RXQ_START_ADDR_OFST                       (2)

//U3D_RXQCPR1
#define RXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_RXQLDPR1
#define RXQ_LAST_DONE_PTR_OFST                    (2)

//U3D_RXQCSR2
#define RXQ_DMGR_DMSM_CS_OFST                     (16)
#define RXQ_ACTIVE_OFST                           (15)
#define RXQ_EPQ_STATE_OFST                        (8)
#define RXQ_STOP_OFST                             (2)
#define RXQ_RESUME_OFST                           (1)
#define RXQ_START_OFST                            (0)

//U3D_RXQSAR2
#define RXQ_START_ADDR_OFST                       (2)

//U3D_RXQCPR2
#define RXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_RXQLDPR2
#define RXQ_LAST_DONE_PTR_OFST                    (2)

//U3D_RXQCSR3
#define RXQ_DMGR_DMSM_CS_OFST                     (16)
#define RXQ_ACTIVE_OFST                           (15)
#define RXQ_EPQ_STATE_OFST                        (8)
#define RXQ_STOP_OFST                             (2)
#define RXQ_RESUME_OFST                           (1)
#define RXQ_START_OFST                            (0)

//U3D_RXQSAR3
#define RXQ_START_ADDR_OFST                       (2)

//U3D_RXQCPR3
#define RXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_RXQLDPR3
#define RXQ_LAST_DONE_PTR_OFST                    (2)

//U3D_RXQCSR4
#define RXQ_DMGR_DMSM_CS_OFST                     (16)
#define RXQ_ACTIVE_OFST                           (15)
#define RXQ_EPQ_STATE_OFST                        (8)
#define RXQ_STOP_OFST                             (2)
#define RXQ_RESUME_OFST                           (1)
#define RXQ_START_OFST                            (0)

//U3D_RXQSAR4
#define RXQ_START_ADDR_OFST                       (2)

//U3D_RXQCPR4
#define RXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_RXQLDPR4
#define RXQ_LAST_DONE_PTR_OFST                    (2)

//U3D_RXQCSR5
#define RXQ_DMGR_DMSM_CS_OFST                     (16)
#define RXQ_ACTIVE_OFST                           (15)
#define RXQ_EPQ_STATE_OFST                        (8)
#define RXQ_STOP_OFST                             (2)
#define RXQ_RESUME_OFST                           (1)
#define RXQ_START_OFST                            (0)

//U3D_RXQSAR5
#define RXQ_START_ADDR_OFST                       (2)

//U3D_RXQCPR5
#define RXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_RXQLDPR5
#define RXQ_LAST_DONE_PTR_OFST                    (2)

//U3D_RXQCSR6
#define RXQ_DMGR_DMSM_CS_OFST                     (16)
#define RXQ_ACTIVE_OFST                           (15)
#define RXQ_EPQ_STATE_OFST                        (8)
#define RXQ_STOP_OFST                             (2)
#define RXQ_RESUME_OFST                           (1)
#define RXQ_START_OFST                            (0)

//U3D_RXQSAR6
#define RXQ_START_ADDR_OFST                       (2)

//U3D_RXQCPR6
#define RXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_RXQLDPR6
#define RXQ_LAST_DONE_PTR_OFST                    (2)

//U3D_RXQCSR7
#define RXQ_DMGR_DMSM_CS_OFST                     (16)
#define RXQ_ACTIVE_OFST                           (15)
#define RXQ_EPQ_STATE_OFST                        (8)
#define RXQ_STOP_OFST                             (2)
#define RXQ_RESUME_OFST                           (1)
#define RXQ_START_OFST                            (0)

//U3D_RXQSAR7
#define RXQ_START_ADDR_OFST                       (2)

//U3D_RXQCPR7
#define RXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_RXQLDPR7
#define RXQ_LAST_DONE_PTR_OFST                    (2)

//U3D_RXQCSR8
#define RXQ_DMGR_DMSM_CS_OFST                     (16)
#define RXQ_ACTIVE_OFST                           (15)
#define RXQ_EPQ_STATE_OFST                        (8)
#define RXQ_STOP_OFST                             (2)
#define RXQ_RESUME_OFST                           (1)
#define RXQ_START_OFST                            (0)

//U3D_RXQSAR8
#define RXQ_START_ADDR_OFST                       (2)

//U3D_RXQCPR8
#define RXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_RXQLDPR8
#define RXQ_LAST_DONE_PTR_OFST                    (2)

//U3D_RXQCSR9
#define RXQ_DMGR_DMSM_CS_OFST                     (16)
#define RXQ_ACTIVE_OFST                           (15)
#define RXQ_EPQ_STATE_OFST                        (8)
#define RXQ_STOP_OFST                             (2)
#define RXQ_RESUME_OFST                           (1)
#define RXQ_START_OFST                            (0)

//U3D_RXQSAR9
#define RXQ_START_ADDR_OFST                       (2)

//U3D_RXQCPR9
#define RXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_RXQLDPR9
#define RXQ_LAST_DONE_PTR_OFST                    (2)

//U3D_RXQCSR10
#define RXQ_DMGR_DMSM_CS_OFST                     (16)
#define RXQ_ACTIVE_OFST                           (15)
#define RXQ_EPQ_STATE_OFST                        (8)
#define RXQ_STOP_OFST                             (2)
#define RXQ_RESUME_OFST                           (1)
#define RXQ_START_OFST                            (0)

//U3D_RXQSAR10
#define RXQ_START_ADDR_OFST                       (2)

//U3D_RXQCPR10
#define RXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_RXQLDPR10
#define RXQ_LAST_DONE_PTR_OFST                    (2)

//U3D_RXQCSR11
#define RXQ_DMGR_DMSM_CS_OFST                     (16)
#define RXQ_ACTIVE_OFST                           (15)
#define RXQ_EPQ_STATE_OFST                        (8)
#define RXQ_STOP_OFST                             (2)
#define RXQ_RESUME_OFST                           (1)
#define RXQ_START_OFST                            (0)

//U3D_RXQSAR11
#define RXQ_START_ADDR_OFST                       (2)

//U3D_RXQCPR11
#define RXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_RXQLDPR11
#define RXQ_LAST_DONE_PTR_OFST                    (2)

//U3D_RXQCSR12
#define RXQ_DMGR_DMSM_CS_OFST                     (16)
#define RXQ_ACTIVE_OFST                           (15)
#define RXQ_EPQ_STATE_OFST                        (8)
#define RXQ_STOP_OFST                             (2)
#define RXQ_RESUME_OFST                           (1)
#define RXQ_START_OFST                            (0)

//U3D_RXQSAR12
#define RXQ_START_ADDR_OFST                       (2)

//U3D_RXQCPR12
#define RXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_RXQLDPR12
#define RXQ_LAST_DONE_PTR_OFST                    (2)

//U3D_RXQCSR13
#define RXQ_DMGR_DMSM_CS_OFST                     (16)
#define RXQ_ACTIVE_OFST                           (15)
#define RXQ_EPQ_STATE_OFST                        (8)
#define RXQ_STOP_OFST                             (2)
#define RXQ_RESUME_OFST                           (1)
#define RXQ_START_OFST                            (0)

//U3D_RXQSAR13
#define RXQ_START_ADDR_OFST                       (2)

//U3D_RXQCPR13
#define RXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_RXQLDPR13
#define RXQ_LAST_DONE_PTR_OFST                    (2)

//U3D_RXQCSR14
#define RXQ_DMGR_DMSM_CS_OFST                     (16)
#define RXQ_ACTIVE_OFST                           (15)
#define RXQ_EPQ_STATE_OFST                        (8)
#define RXQ_STOP_OFST                             (2)
#define RXQ_RESUME_OFST                           (1)
#define RXQ_START_OFST                            (0)

//U3D_RXQSAR14
#define RXQ_START_ADDR_OFST                       (2)

//U3D_RXQCPR14
#define RXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_RXQLDPR14
#define RXQ_LAST_DONE_PTR_OFST                    (2)

//U3D_RXQCSR15
#define RXQ_DMGR_DMSM_CS_OFST                     (16)
#define RXQ_ACTIVE_OFST                           (15)
#define RXQ_EPQ_STATE_OFST                        (8)
#define RXQ_STOP_OFST                             (2)
#define RXQ_RESUME_OFST                           (1)
#define RXQ_START_OFST                            (0)

//U3D_RXQSAR15
#define RXQ_START_ADDR_OFST                       (2)

//U3D_RXQCPR15
#define RXQ_CUR_GPD_ADDR_OFST                     (2)

//U3D_RXQLDPR15
#define RXQ_LAST_DONE_PTR_OFST                    (2)

//U3D_QISAR0
#define RXQ_DONE_INT_OFST                         (17)
#define TXQ_DONE_INT_OFST                         (1)

//U3D_QIER0
#define RXQ_DONE_IER_OFST                         (17)
#define TXQ_DONE_IER_OFST                         (1)

//U3D_QIESR0
#define RXQ_DONE_IESR_OFST                        (17)
#define TXQ_DONE_IESR_OFST                        (1)

//U3D_QIECR0
#define RXQ_DONE_IECR_OFST                        (17)
#define TXQ_DONE_IECR_OFST                        (1)

//U3D_QISAR1
#define RXQ_ZLPERR_INT_OFST                       (20)
#define RXQ_LENERR_INT_OFST                       (18)
#define RXQ_CSERR_INT_OFST                        (17)
#define RXQ_EMPTY_INT_OFST                        (16)
#define TXQ_LENERR_INT_OFST                       (2)
#define TXQ_CSERR_INT_OFST                        (1)
#define TXQ_EMPTY_INT_OFST                        (0)

//U3D_QIER1
#define RXQ_ZLPERR_IER_OFST                       (20)
#define RXQ_LENERR_IER_OFST                       (18)
#define RXQ_CSERR_IER_OFST                        (17)
#define RXQ_EMPTY_IER_OFST                        (16)
#define TXQ_LENERR_IER_OFST                       (2)
#define TXQ_CSERR_IER_OFST                        (1)
#define TXQ_EMPTY_IER_OFST                        (0)

//U3D_QIESR1
#define RXQ_ZLPERR_IESR_OFST                      (20)
#define RXQ_LENERR_IESR_OFST                      (18)
#define RXQ_CSERR_IESR_OFST                       (17)
#define RXQ_EMPTY_IESR_OFST                       (16)
#define TXQ_LENERR_IESR_OFST                      (2)
#define TXQ_CSERR_IESR_OFST                       (1)
#define TXQ_EMPTY_IESR_OFST                       (0)

//U3D_QIECR1
#define RXQ_ZLPERR_IECR_OFST                      (20)
#define RXQ_LENERR_IECR_OFST                      (18)
#define RXQ_CSERR_IECR_OFST                       (17)
#define RXQ_EMPTY_IECR_OFST                       (16)
#define TXQ_LENERR_IECR_OFST                      (2)
#define TXQ_CSERR_IECR_OFST                       (1)
#define TXQ_EMPTY_IECR_OFST                       (0)

//U3D_QEMIR
#define RXQ_EMPTY_MASK_OFST                       (17)
#define TXQ_EMPTY_MASK_OFST                       (1)

//U3D_QEMIER
#define RXQ_EMPTY_IER_MASK_OFST                   (17)
#define TXQ_EMPTY_IER_MASK_OFST                   (1)

//U3D_QEMIESR
#define RXQ_EMPTY_IESR_MASK_OFST                  (17)
#define TXQ_EMPTY_IESR_MASK_OFST                  (1)

//U3D_QEMIECR
#define RXQ_EMPTY_IECR_MASK_OFST                  (17)
#define TXQ_EMPTY_IECR_MASK_OFST                  (1)

//U3D_TQERRIR0
#define TXQ_LENERR_MASK_OFST                      (17)
#define TXQ_CSERR_MASK_OFST                       (1)

//U3D_TQERRIER0
#define TXQ_LENERR_IER_MASK_OFST                  (17)
#define TXQ_CSERR_IER_MASK_OFST                   (1)

//U3D_TQERRIESR0
#define TXQ_LENERR_IESR_MASK_OFST                 (17)
#define TXQ_CSERR_IESR_MASK_OFST                  (1)

//U3D_TQERRIECR0
#define TXQ_LENERR_IECR_MASK_OFST                 (17)
#define TXQ_CSERR_IECR_MASK_OFST                  (1)

//U3D_RQERRIR0
#define RXQ_LENERR_MASK_OFST                      (17)
#define RXQ_CSERR_MASK_OFST                       (1)

//U3D_RQERRIER0
#define RXQ_LENERR_IER_MASK_OFST                  (17)
#define RXQ_CSERR_IER_MASK_OFST                   (1)

//U3D_RQERRIESR0
#define RXQ_LENERR_IESR_MASK_OFST                 (17)
#define RXQ_CSERR_IESR_MASK_OFST                  (1)

//U3D_RQERRIECR0
#define RXQ_LENERR_IECR_MASK_OFST                 (17)
#define RXQ_CSERR_IECR_MASK_OFST                  (1)

//U3D_RQERRIR1
#define RXQ_ZLPERR_MASK_OFST                      (17)

//U3D_RQERRIER1
#define RXQ_ZLPERR_IER_MASK_OFST                  (17)

//U3D_RQERRIESR1
#define RXQ_ZLPERR_IESR_MASK_OFST                 (17)

//U3D_RQERRIECR1
#define RXQ_ZLPERR_IECR_MASK_OFST                 (17)

//U3D_CAP_EP0FFSZ
#define CAP_EP0FFSZ_OFST                          (0)

//U3D_CAP_EPNTXFFSZ
#define CAP_EPNTXFFSZ_OFST                        (0)

//U3D_CAP_EPNRXFFSZ
#define CAP_EPNRXFFSZ_OFST                        (0)

//U3D_CAP_EPINFO
#define CAP_RX_EP_NUM_OFST                        (8)
#define CAP_TX_EP_NUM_OFST                        (0)

//U3D_CAP_TX_SLOT1
#define CAP_TX_SLOT3_OFST                         (24)
#define CAP_TX_SLOT2_OFST                         (16)
#define CAP_TX_SLOT1_OFST                         (8)
#define RSV_OFST                                  (0)

//U3D_CAP_TX_SLOT2
#define CAP_TX_SLOT7_OFST                         (24)
#define CAP_TX_SLOT6_OFST                         (16)
#define CAP_TX_SLOT5_OFST                         (8)
#define CAP_TX_SLOT4_OFST                         (0)

//U3D_CAP_TX_SLOT3
#define CAP_TX_SLOT11_OFST                        (24)
#define CAP_TX_SLOT10_OFST                        (16)
#define CAP_TX_SLOT9_OFST                         (8)
#define CAP_TX_SLOT8_OFST                         (0)

//U3D_CAP_TX_SLOT4
#define CAP_TX_SLOT15_OFST                        (24)
#define CAP_TX_SLOT14_OFST                        (16)
#define CAP_TX_SLOT13_OFST                        (8)
#define CAP_TX_SLOT12_OFST                        (0)

//U3D_CAP_RX_SLOT1
#define CAP_RX_SLOT3_OFST                         (24)
#define CAP_RX_SLOT2_OFST                         (16)
#define CAP_RX_SLOT1_OFST                         (8)
#define RSV_OFST                                  (0)

//U3D_CAP_RX_SLOT2
#define CAP_RX_SLOT7_OFST                         (24)
#define CAP_RX_SLOT6_OFST                         (16)
#define CAP_RX_SLOT5_OFST                         (8)
#define CAP_RX_SLOT4_OFST                         (0)

//U3D_CAP_RX_SLOT3
#define CAP_RX_SLOT11_OFST                        (24)
#define CAP_RX_SLOT10_OFST                        (16)
#define CAP_RX_SLOT9_OFST                         (8)
#define CAP_RX_SLOT8_OFST                         (0)

//U3D_CAP_RX_SLOT4
#define CAP_RX_SLOT15_OFST                        (24)
#define CAP_RX_SLOT14_OFST                        (16)
#define CAP_RX_SLOT13_OFST                        (8)
#define CAP_RX_SLOT12_OFST                        (0)

//U3D_MISC_CTRL
#define DMA_BUS_CK_GATE_DIS_OFST                  (2)
#define VBUS_ON_OFST                              (1)
#define VBUS_FRC_EN_OFST                          (0)

//////////////////////////////////////////////////////////////////////
