/* SSUSB_SIFSLV_IPPC REGISTER DEFINITION */

#define U3D_SSUSB_IP_PW_CTRL0                     (SSUSB_SIFSLV_IPPC_BASE+0x0000)
#define U3D_SSUSB_IP_PW_CTRL1                     (SSUSB_SIFSLV_IPPC_BASE+0x0004)
#define U3D_SSUSB_IP_PW_CTRL2                     (SSUSB_SIFSLV_IPPC_BASE+0x0008)
#define U3D_SSUSB_IP_PW_CTRL3                     (SSUSB_SIFSLV_IPPC_BASE+0x000C)
#define U3D_SSUSB_IP_PW_STS1                      (SSUSB_SIFSLV_IPPC_BASE+0x0010)
#define U3D_SSUSB_IP_PW_STS2                      (SSUSB_SIFSLV_IPPC_BASE+0x0014)
#define U3D_SSUSB_OTG_STS                         (SSUSB_SIFSLV_IPPC_BASE+0x0018)
#define U3D_SSUSB_OTG_STS_CLR                     (SSUSB_SIFSLV_IPPC_BASE+0x001C)
#define U3D_SSUSB_IP_MAC_CAP                      (SSUSB_SIFSLV_IPPC_BASE+0x0020)
#define U3D_SSUSB_IP_XHCI_CAP                     (SSUSB_SIFSLV_IPPC_BASE+0x0024)
#define U3D_SSUSB_IP_DEV_CAP                      (SSUSB_SIFSLV_IPPC_BASE+0x0028)
#define U3D_SSUSB_OTG_INT_EN                      (SSUSB_SIFSLV_IPPC_BASE+0x002C)

#if (defined(SUPPORT_U3) || defined(CONFIG_MTK_FPGA))
#define U3D_SSUSB_U3_CTRL_0P                      (SSUSB_SIFSLV_IPPC_BASE+0x0030)
#define U3D_SSUSB_U3_CTRL_1P                      (SSUSB_SIFSLV_IPPC_BASE+0x0038)
#define U3D_SSUSB_U3_CTRL_2P                      (SSUSB_SIFSLV_IPPC_BASE+0x0040)
#define U3D_SSUSB_U3_CTRL_3P                      (SSUSB_SIFSLV_IPPC_BASE+0x0048)
#endif
#define U3D_SSUSB_U2_CTRL_0P                      (SSUSB_SIFSLV_IPPC_BASE+0x0050)
#ifdef SUPPORT_U3
#define U3D_SSUSB_U2_CTRL_1P                      (SSUSB_SIFSLV_IPPC_BASE+0x0058)
#define U3D_SSUSB_U2_CTRL_2P                      (SSUSB_SIFSLV_IPPC_BASE+0x0060)
#define U3D_SSUSB_U2_CTRL_3P                      (SSUSB_SIFSLV_IPPC_BASE+0x0068)
#define U3D_SSUSB_U2_CTRL_4P                      (SSUSB_SIFSLV_IPPC_BASE+0x0070)
#define U3D_SSUSB_U2_CTRL_5P                      (SSUSB_SIFSLV_IPPC_BASE+0x0078)
#endif
#define U3D_SSUSB_U2_PHY_PLL                      (SSUSB_SIFSLV_IPPC_BASE+0x007C)
#define U3D_SSUSB_DMA_CTRL                        (SSUSB_SIFSLV_IPPC_BASE+0x0080)
#define U3D_SSUSB_MAC_CK_CTRL                     (SSUSB_SIFSLV_IPPC_BASE+0x0084)
#define U3D_SSUSB_CSR_CK_CTRL                     (SSUSB_SIFSLV_IPPC_BASE+0x0088)
#define U3D_SSUSB_REF_CK_CTRL                     (SSUSB_SIFSLV_IPPC_BASE+0x008C)
#define U3D_SSUSB_XHCI_CK_CTRL                    (SSUSB_SIFSLV_IPPC_BASE+0x0090)
#define U3D_SSUSB_XHCI_RST_CTRL                   (SSUSB_SIFSLV_IPPC_BASE+0x0094)
#define U3D_SSUSB_DEV_RST_CTRL                    (SSUSB_SIFSLV_IPPC_BASE+0x0098)
#define U3D_SSUSB_SYS_CK_CTRL                     (SSUSB_SIFSLV_IPPC_BASE+0x009C)
#define U3D_SSUSB_HW_ID                           (SSUSB_SIFSLV_IPPC_BASE+0x00A0)
#define U3D_SSUSB_HW_SUB_ID                       (SSUSB_SIFSLV_IPPC_BASE+0x00A4)
#define U3D_SSUSB_PRB_CTRL0                       (SSUSB_SIFSLV_IPPC_BASE+0x00B0)
#define U3D_SSUSB_PRB_CTRL1                       (SSUSB_SIFSLV_IPPC_BASE+0x00B4)
#define U3D_SSUSB_PRB_CTRL2                       (SSUSB_SIFSLV_IPPC_BASE+0x00B8)
#define U3D_SSUSB_PRB_CTRL3                       (SSUSB_SIFSLV_IPPC_BASE+0x00BC)
#define U3D_SSUSB_PRB_CTRL4                       (SSUSB_SIFSLV_IPPC_BASE+0x00C0)
#define U3D_SSUSB_PRB_CTRL5                       (SSUSB_SIFSLV_IPPC_BASE+0x00C4)
#define U3D_SSUSB_IP_SPARE0                       (SSUSB_SIFSLV_IPPC_BASE+0x00C8)
#define U3D_SSUSB_IP_SPARE1                       (SSUSB_SIFSLV_IPPC_BASE+0x00CC)
#define U3D_SSUSB_FPGA_I2C_OUT_0P                 (SSUSB_SIFSLV_IPPC_BASE+0x00D0)
#define U3D_SSUSB_FPGA_I2C_IN_0P                  (SSUSB_SIFSLV_IPPC_BASE+0x00D4)
#define U3D_SSUSB_FPGA_I2C_OUT_1P                 (SSUSB_SIFSLV_IPPC_BASE+0x00D8)
#define U3D_SSUSB_FPGA_I2C_IN_1P                  (SSUSB_SIFSLV_IPPC_BASE+0x00DC)
#define U3D_SSUSB_FPGA_I2C_OUT_2P                 (SSUSB_SIFSLV_IPPC_BASE+0x00E0)
#define U3D_SSUSB_FPGA_I2C_IN_2P                  (SSUSB_SIFSLV_IPPC_BASE+0x00E4)
#define U3D_SSUSB_FPGA_I2C_OUT_3P                 (SSUSB_SIFSLV_IPPC_BASE+0x00E8)
#define U3D_SSUSB_FPGA_I2C_IN_3P                  (SSUSB_SIFSLV_IPPC_BASE+0x00EC)
#define U3D_SSUSB_FPGA_I2C_OUT_4P                 (SSUSB_SIFSLV_IPPC_BASE+0x00F0)
#define U3D_SSUSB_FPGA_I2C_IN_4P                  (SSUSB_SIFSLV_IPPC_BASE+0x00F4)
#define U3D_SSUSB_IP_SLV_TMOUT                    (SSUSB_SIFSLV_IPPC_BASE+0x00F8)

/* SSUSB_SIFSLV_IPPC FIELD DEFINITION */

//U3D_SSUSB_IP_PW_CTRL0
#define SSUSB_AHB_SLV_AUTO_RSP                    (0x1<<17) //17:17
#define SSUSB_IP_SW_RST_CK_GATE_EN                (0x1<<16) //16:16
#define SSUSB_IP_U2_ENTER_SLEEP_CNT               (0xff<<8) //15:8
#define SSUSB_IP_SW_RST                           (0x1<<0) //0:0

//U3D_SSUSB_IP_PW_CTRL1
#define SSUSB_IP_HOST_PDN                         (0x1<<0) //0:0

//U3D_SSUSB_IP_PW_CTRL2
#define SSUSB_IP_DEV_PDN                          (0x1<<0) //0:0

//U3D_SSUSB_IP_PW_CTRL3
#define SSUSB_IP_PCIE_PDN                         (0x1<<0) //0:0

//U3D_SSUSB_IP_PW_STS1
#define SSUSB_IP_REF_CK_DIS_STS                   (0x1<<31) //31:31
#define SSUSB_IP_SLEEP_STS                        (0x1<<30) //30:30
#define SSUSB_U2_MAC_RST_B_STS_5P                 (0x1<<29) //29:29
#define SSUSB_U2_MAC_RST_B_STS_4P                 (0x1<<28) //28:28
#define SSUSB_U2_MAC_RST_B_STS_3P                 (0x1<<27) //27:27
#define SSUSB_U2_MAC_RST_B_STS_2P                 (0x1<<26) //26:26
#define SSUSB_U2_MAC_RST_B_STS_1P                 (0x1<<25) //25:25
#define SSUSB_U2_MAC_RST_B_STS                    (0x1<<24) //24:24
#define SSUSB_U3_MAC_RST_B_STS_3P                 (0x1<<19) //19:19
#define SSUSB_U3_MAC_RST_B_STS_2P                 (0x1<<18) //18:18
#define SSUSB_U3_MAC_RST_B_STS_1P                 (0x1<<17) //17:17
#define SSUSB_U3_MAC_RST_B_STS                    (0x1<<16) //16:16
#define SSUSB_DEV_DRAM_RST_B_STS                  (0x1<<13) //13:13
#define SSUSB_XHCI_DRAM_RST_B_STS                 (0x1<<12) //12:12
#define SSUSB_XHCI_RST_B_STS                      (0x1<<11) //11:11
#define SSUSB_SYS125_RST_B_STS                    (0x1<<10) //10:10
#define SSUSB_SYS60_RST_B_STS                     (0x1<<9) //9:9
#define SSUSB_REF_RST_B_STS                       (0x1<<8) //8:8
#define SSUSB_DEV_RST_B_STS                       (0x1<<3) //3:3
#define SSUSB_DEV_BMU_RST_B_STS                   (0x1<<2) //2:2
#define SSUSB_DEV_QMU_RST_B_STS                   (0x1<<1) //1:1
#define SSUSB_SYSPLL_STABLE                       (0x1<<0) //0:0

//U3D_SSUSB_IP_PW_STS2
#define SSUSB_U2_MAC_SYS_RST_B_STS_5P             (0x1<<5) //5:5
#define SSUSB_U2_MAC_SYS_RST_B_STS_4P             (0x1<<4) //4:4
#define SSUSB_U2_MAC_SYS_RST_B_STS_3P             (0x1<<3) //3:3
#define SSUSB_U2_MAC_SYS_RST_B_STS_2P             (0x1<<2) //2:2
#define SSUSB_U2_MAC_SYS_RST_B_STS_1P             (0x1<<1) //1:1
#define SSUSB_U2_MAC_SYS_RST_B_STS                (0x1<<0) //0:0

//U3D_SSUSB_OTG_STS
#define SSUSB_XHCI_MAS_DMA_REQ                    (0x1<<14) //14:14
#define SSUSB_DEV_DMA_REQ                         (0x1<<13) //13:13
#define SSUSB_AVALID_STS                          (0x1<<12) //12:12
#define SSUSB_SRP_REQ_INTR                        (0x1<<11) //11:11
#define SSUSB_IDDIG                               (0x1<<10) //10:10
#define SSUSB_VBUS_VALID                          (0x1<<9) //9:9
#define SSUSB_HOST_DEV_MODE                       (0x1<<8) //8:8
#define SSUSB_DEV_USBRST_INTR                     (0x1<<7) //7:7
#define VBUS_CHG_INTR                             (0x1<<6) //6:6
#define SSUSB_CHG_B_ROLE_B                        (0x1<<5) //5:5
#define SSUSB_CHG_A_ROLE_B                        (0x1<<4) //4:4
#define SSUSB_ATTACH_B_ROLE                       (0x1<<3) //3:3
#define SSUSB_CHG_B_ROLE_A                        (0x1<<2) //2:2
#define SSUSB_CHG_A_ROLE_A                        (0x1<<1) //1:1
#define SSUSB_ATTACH_A_ROLE                       (0x1<<0) //0:0

//U3D_SSUSB_OTG_STS_CLR
#define SSUSB_SRP_REQ_INTR_CLR                    (0x1<<11) //11:11
#define SSUSB_DEV_USBRST_INTR_CLR                 (0x1<<7) //7:7
#define SSUSB_VBUS_INTR_CLR                       (0x1<<6) //6:6
#define SSUSB_CHG_B_ROLE_B_CLR                    (0x1<<5) //5:5
#define SSUSB_CHG_A_ROLE_B_CLR                    (0x1<<4) //4:4
#define SSUSB_ATTACH_B_ROLE_CLR                   (0x1<<3) //3:3
#define SSUSB_CHG_B_ROLE_A_CLR                    (0x1<<2) //2:2
#define SSUSB_CHG_A_ROLE_A_CLR                    (0x1<<1) //1:1
#define SSUSB_ATTACH_A_ROLE_CLR                   (0x1<<0) //0:0

//U3D_SSUSB_IP_MAC_CAP
#define SSUSB_IP_MAC_U2_PORT_NO                   (0xff<<8) //15:8
#define SSUSB_IP_MAC_U3_PORT_NO                   (0xff<<0) //7:0

//U3D_SSUSB_IP_XHCI_CAP
#define SSUSB_IP_XHCI_U2_PORT_NO                  (0xff<<8) //15:8
#define SSUSB_IP_XHCI_U3_PORT_NO                  (0xff<<0) //7:0

//U3D_SSUSB_IP_DEV_CAP
#define SSUSB_IP_DEV_U2_PORT_NO                   (0xff<<8) //15:8
#define SSUSB_IP_DEV_U3_PORT_NO                   (0xff<<0) //7:0

//U3D_SSUSB_OTG_INT_EN
#define SSUSB_DEV_USBRST_INT_EN                   (0x1<<8) //8:8
#define SSUSB_VBUS_CHG_INT_A_EN                   (0x1<<7) //7:7
#define SSUSB_VBUS_CHG_INT_B_EN                   (0x1<<6) //6:6
#define SSUSB_CHG_B_ROLE_B_INT_EN                 (0x1<<5) //5:5
#define SSUSB_CHG_A_ROLE_B_INT_EN                 (0x1<<4) //4:4
#define SSUSB_ATTACH_B_ROLE_INT_EN                (0x1<<3) //3:3
#define SSUSB_CHG_B_ROLE_A_INT_EN                 (0x1<<2) //2:2
#define SSUSB_CHG_A_ROLE_A_INT_EN                 (0x1<<1) //1:1
#define SSUSB_ATTACH_A_ROLE_INT_EN                (0x1<<0) //0:0

//U3D_SSUSB_U3_CTRL_0P
#define SSUSB_U3_PORT_PHYD_RST                    (0x1<<5) //5:5
#define SSUSB_U3_PORT_MAC_RST                     (0x1<<4) //4:4
#define SSUSB_U3_PORT_U2_CG_EN                    (0x1<<3) //3:3
#define SSUSB_U3_PORT_HOST_SEL                    (0x1<<2) //2:2
#define SSUSB_U3_PORT_PDN                         (0x1<<1) //1:1
#define SSUSB_U3_PORT_DIS                         (0x1<<0) //0:0

//U3D_SSUSB_U3_CTRL_1P
#define SSUSB_U3_PORT_PHYD_RST_1P                 (0x1<<5) //5:5
#define SSUSB_U3_PORT_MAC_RST_1P                  (0x1<<4) //4:4
#define SSUSB_U3_PORT_U2_CG_EN_1P                 (0x1<<3) //3:3
#define SSUSB_U3_PORT_HOST_SEL_1P                 (0x1<<2) //2:2
#define SSUSB_U3_PORT_PDN_1P                      (0x1<<1) //1:1
#define SSUSB_U3_PORT_DIS_1P                      (0x1<<0) //0:0

//U3D_SSUSB_U3_CTRL_2P
#define SSUSB_U3_PORT_PHYD_RST_2P                 (0x1<<5) //5:5
#define SSUSB_U3_PORT_MAC_RST_2P                  (0x1<<4) //4:4
#define SSUSB_U3_PORT_U2_CG_EN_2P                 (0x1<<3) //3:3
#define SSUSB_U3_PORT_HOST_SEL_2P                 (0x1<<2) //2:2
#define SSUSB_U3_PORT_PDN_2P                      (0x1<<1) //1:1
#define SSUSB_U3_PORT_DIS_2P                      (0x1<<0) //0:0

//U3D_SSUSB_U3_CTRL_3P
#define SSUSB_U3_PORT_PHYD_RST_3P                 (0x1<<5) //5:5
#define SSUSB_U3_PORT_MAC_RST_3P                  (0x1<<4) //4:4
#define SSUSB_U3_PORT_U2_CG_EN_3P                 (0x1<<3) //3:3
#define SSUSB_U3_PORT_HOST_SEL_3P                 (0x1<<2) //2:2
#define SSUSB_U3_PORT_PDN_3P                      (0x1<<1) //1:1
#define SSUSB_U3_PORT_DIS_3P                      (0x1<<0) //0:0

//U3D_SSUSB_U2_CTRL_0P
#define SSUSB_U2_PORT_OTG_HOST_VBUSVALID_SEL      (0x1<<9) //9:9
#define SSUSB_U2_PORT_OTG_MAC_AUTO_SEL            (0x1<<8) //8:8
#define SSUSB_U2_PORT_OTG_SEL                     (0x1<<7) //7:7
#define SSUSB_U2_PORT_PLL_STABLE_SEL              (0x1<<6) //6:6
#define SSUSB_U2_PORT_PHYD_RST                    (0x1<<5) //5:5
#define SSUSB_U2_PORT_MAC_RST                     (0x1<<4) //4:4
#define SSUSB_U2_PORT_U2_CG_EN                    (0x1<<3) //3:3
#define SSUSB_U2_PORT_HOST_SEL                    (0x1<<2) //2:2
#define SSUSB_U2_PORT_PDN                         (0x1<<1) //1:1
#define SSUSB_U2_PORT_DIS                         (0x1<<0) //0:0

//U3D_SSUSB_U2_CTRL_1P
#define SSUSB_U2_PORT_PLL_STABLE_SEL_1P           (0x1<<6) //6:6
#define SSUSB_U2_PORT_PHYD_RST_1P                 (0x1<<5) //5:5
#define SSUSB_U2_PORT_MAC_RST_1P                  (0x1<<4) //4:4
#define SSUSB_U2_PORT_U2_CG_EN_1P                 (0x1<<3) //3:3
#define SSUSB_U2_PORT_HOST_SEL_1P                 (0x1<<2) //2:2
#define SSUSB_U2_PORT_PDN_1P                      (0x1<<1) //1:1
#define SSUSB_U2_PORT_DIS_1P                      (0x1<<0) //0:0

//U3D_SSUSB_U2_CTRL_2P
#define SSUSB_U2_PORT_PLL_STABLE_SEL_2P           (0x1<<6) //6:6
#define SSUSB_U2_PORT_PHYD_RST_2P                 (0x1<<5) //5:5
#define SSUSB_U2_PORT_MAC_RST_2P                  (0x1<<4) //4:4
#define SSUSB_U2_PORT_U2_CG_EN_2P                 (0x1<<3) //3:3
#define SSUSB_U2_PORT_HOST_SEL_2P                 (0x1<<2) //2:2
#define SSUSB_U2_PORT_PDN_2P                      (0x1<<1) //1:1
#define SSUSB_U2_PORT_DIS_2P                      (0x1<<0) //0:0

//U3D_SSUSB_U2_CTRL_3P
#define SSUSB_U2_PORT_PLL_STABLE_SEL_3P           (0x1<<6) //6:6
#define SSUSB_U2_PORT_PHYD_RST_3P                 (0x1<<5) //5:5
#define SSUSB_U2_PORT_MAC_RST_3P                  (0x1<<4) //4:4
#define SSUSB_U2_PORT_U2_CG_EN_3P                 (0x1<<3) //3:3
#define SSUSB_U2_PORT_HOST_SEL_3P                 (0x1<<2) //2:2
#define SSUSB_U2_PORT_PDN_3P                      (0x1<<1) //1:1
#define SSUSB_U2_PORT_DIS_3P                      (0x1<<0) //0:0

//U3D_SSUSB_U2_CTRL_4P
#define SSUSB_U2_PORT_PLL_STABLE_SEL_4P           (0x1<<6) //6:6
#define SSUSB_U2_PORT_PHYD_RST_4P                 (0x1<<5) //5:5
#define SSUSB_U2_PORT_MAC_RST_4P                  (0x1<<4) //4:4
#define SSUSB_U2_PORT_U2_CG_EN_4P                 (0x1<<3) //3:3
#define SSUSB_U2_PORT_HOST_SEL_4P                 (0x1<<2) //2:2
#define SSUSB_U2_PORT_PDN_4P                      (0x1<<1) //1:1
#define SSUSB_U2_PORT_DIS_4P                      (0x1<<0) //0:0

//U3D_SSUSB_U2_CTRL_5P
#define SSUSB_U2_PORT_PLL_STABLE_SEL_5P           (0x1<<6) //6:6
#define SSUSB_U2_PORT_PHYD_RST_5P                 (0x1<<5) //5:5
#define SSUSB_U2_PORT_MAC_RST_5P                  (0x1<<4) //4:4
#define SSUSB_U2_PORT_U2_CG_EN_5P                 (0x1<<3) //3:3
#define SSUSB_U2_PORT_HOST_SEL_5P                 (0x1<<2) //2:2
#define SSUSB_U2_PORT_PDN_5P                      (0x1<<1) //1:1
#define SSUSB_U2_PORT_DIS_5P                      (0x1<<0) //0:0

//U3D_SSUSB_U2_PHY_PLL
#define SSUSB_SYSPLL_USE                          (0x1<<30) //30:30
#define RG_SSUSB_U2_PLL_STB                       (0x1<<29) //29:29
#define SSUSB_U2_FORCE_PLL_STB                    (0x1<<28) //28:28
#define SSUSB_U2_PORT_PHY_CK_DEB_TIMER            (0xf<<24) //27:24
#define SSUSB_U2_PORT_LPM_PLL_STABLE_TIMER        (0xff<<16) //23:16
#define SSUSB_U2_PORT_PLL_STABLE_TIMER            (0xff<<8) //15:8
#define SSUSB_U2_PORT_1US_TIMER                   (0xff<<0) //7:0

//U3D_SSUSB_DMA_CTRL
#define SSUSB_IP_DMA_BUS_CK_GATE_DIS              (0x1<<0) //0:0

//U3D_SSUSB_MAC_CK_CTRL
#define SSUSB_MAC3_SYS_CK_GATE_MASK_TIME          (0xff<<16) //23:16
#define SSUSB_MAC2_SYS_CK_GATE_MASK_TIME          (0xff<<8) //15:8
#define SSUSB_PHY_REF_CK_DIV2                     (0x1<<4) //4:4
#define SSUSB_MAC3_SYS_CK_GATE_MODE               (0x3<<2) //3:2
#define SSUSB_MAC2_SYS_CK_GATE_MODE               (0x3<<0) //1:0

//U3D_SSUSB_CSR_CK_CTRL
#define SSUSB_SIFSLV_MCU_BUS_CK_GATE_EN           (0x1<<1) //1:1
#define SSUSB_CSR_MCU_BUS_CK_GATE_EN              (0x1<<0) //0:0

//U3D_SSUSB_REF_CK_CTRL
#define SSUSB_REF_MAC2_CK_GATE_EN                 (0x1<<4) //4:4
#define SSUSB_REF_MAC3_CK_GATE_EN                 (0x1<<3) //3:3
#define SSUSB_REF_CK_GATE_EN                      (0x1<<2) //2:2
#define SSUSB_REF_PHY_CK_GATE_EN                  (0x1<<1) //1:1
#define SSUSB_REF_MAC_CK_GATE_EN                  (0x1<<0) //0:0

//U3D_SSUSB_XHCI_CK_CTRL
#define SSUSB_XACT3_XHCI_CK_GATE_MASK_TIME        (0xff<<8) //15:8
#define SSUSB_XACT3_XHCI_CK_GATE_MODE             (0x3<<4) //5:4
#define SSUSB_XHCI_CK_DIV2_EN                     (0x1<<0) //0:0

//U3D_SSUSB_XHCI_RST_CTRL
#define SSUSB_XHCI_SW_DRAM_RST                    (0x1<<4) //4:4
#define SSUSB_XHCI_SW_SYS60_RST                   (0x1<<3) //3:3
#define SSUSB_XHCI_SW_SYS125_RST                  (0x1<<2) //2:2
#define SSUSB_XHCI_SW_XHCI_RST                    (0x1<<1) //1:1
#define SSUSB_XHCI_SW_RST                         (0x1<<0) //0:0

//U3D_SSUSB_DEV_RST_CTRL
#define SSUSB_DEV_SW_DRAM_RST                     (0x1<<3) //3:3
#define SSUSB_DEV_SW_QMU_RST                      (0x1<<2) //2:2
#define SSUSB_DEV_SW_BMU_RST                      (0x1<<1) //1:1
#define SSUSB_DEV_SW_RST                          (0x1<<0) //0:0

//U3D_SSUSB_SYS_CK_CTRL
#define SSUSB_SYS60_CK_EXT_SEL                    (0x1<<2) //2:2
#define SSUSB_SYS_CK_EXT_SEL                      (0x1<<1) //1:1
#define SSUSB_SYS_CK_DIV2_EN                      (0x1<<0) //0:0

//U3D_SSUSB_HW_ID
#define SSUSB_HW_ID                               (0xffffffff<<0) //31:0

//U3D_SSUSB_HW_SUB_ID
#define SSUSB_HW_SUB_ID                           (0xffffffff<<0) //31:0

//U3D_SSUSB_PRB_CTRL0
#define PRB_BYTE3_EN                              (0x1<<3) //3:3
#define PRB_BYTE2_EN                              (0x1<<2) //2:2
#define PRB_BYTE1_EN                              (0x1<<1) //1:1
#define PRB_BYTE0_EN                              (0x1<<0) //0:0

//U3D_SSUSB_PRB_CTRL1
#define PRB_BYTE1_SEL                             (0xffff<<16) //31:16
#define PRB_BYTE0_SEL                             (0xffff<<0) //15:0

//U3D_SSUSB_PRB_CTRL2
#define PRB_BYTE3_SEL                             (0xffff<<16) //31:16
#define PRB_BYTE2_SEL                             (0xffff<<0) //15:0

//U3D_SSUSB_PRB_CTRL3
#define PRB_BYTE3_MODULE_SEL                      (0xff<<24) //31:24
#define PRB_BYTE2_MODULE_SEL                      (0xff<<16) //23:16
#define PRB_BYTE1_MODULE_SEL                      (0xff<<8) //15:8
#define PRB_BYTE0_MODULE_SEL                      (0xff<<0) //7:0

//U3D_SSUSB_PRB_CTRL4
#define SW_PRB_OUT                                (0xffffffff<<0) //31:0

//U3D_SSUSB_PRB_CTRL5
#define PRB_RD_DATA                               (0xffffffff<<0) //31:0

//U3D_SSUSB_IP_SPARE0
#define SSUSB_IP_SPARE0                           (0xffffffff<<0) //31:0

//U3D_SSUSB_IP_SPARE1
#define SSUSB_IP_SPARE1                           (0xffffffff<<0) //31:0

//U3D_SSUSB_FPGA_I2C_OUT_0P
#define SSUSB_FPGA_I2C_SCL_OEN_0P                 (0x1<<3) //3:3
#define SSUSB_FPGA_I2C_SCL_OUT_0P                 (0x1<<2) //2:2
#define SSUSB_FPGA_I2C_SDA_OEN_0P                 (0x1<<1) //1:1
#define SSUSB_FPGA_I2C_SDA_OUT_0P                 (0x1<<0) //0:0

//U3D_SSUSB_FPGA_I2C_IN_0P
#define SSUSB_FPGA_I2C_SCL_IN_0P                  (0x1<<1) //1:1
#define SSUSB_FPGA_I2C_SDA_IN_0P                  (0x1<<0) //0:0

//U3D_SSUSB_FPGA_I2C_OUT_1P
#define SSUSB_FPGA_I2C_SCL_OEN_1P                 (0x1<<3) //3:3
#define SSUSB_FPGA_I2C_SCL_OUT_1P                 (0x1<<2) //2:2
#define SSUSB_FPGA_I2C_SDA_OEN_1P                 (0x1<<1) //1:1
#define SSUSB_FPGA_I2C_SDA_OUT_1P                 (0x1<<0) //0:0

//U3D_SSUSB_FPGA_I2C_IN_1P
#define SSUSB_FPGA_I2C_SCL_IN_1P                  (0x1<<1) //1:1
#define SSUSB_FPGA_I2C_SDA_IN_1P                  (0x1<<0) //0:0

//U3D_SSUSB_FPGA_I2C_OUT_2P
#define SSUSB_FPGA_I2C_SCL_OEN_2P                 (0x1<<3) //3:3
#define SSUSB_FPGA_I2C_SCL_OUT_2P                 (0x1<<2) //2:2
#define SSUSB_FPGA_I2C_SDA_OEN_2P                 (0x1<<1) //1:1
#define SSUSB_FPGA_I2C_SDA_OUT_2P                 (0x1<<0) //0:0

//U3D_SSUSB_FPGA_I2C_IN_2P
#define SSUSB_FPGA_I2C_SCL_IN_2P                  (0x1<<1) //1:1
#define SSUSB_FPGA_I2C_SDA_IN_2P                  (0x1<<0) //0:0

//U3D_SSUSB_FPGA_I2C_OUT_3P
#define SSUSB_FPGA_I2C_SCL_OEN_3P                 (0x1<<3) //3:3
#define SSUSB_FPGA_I2C_SCL_OUT_3P                 (0x1<<2) //2:2
#define SSUSB_FPGA_I2C_SDA_OEN_3P                 (0x1<<1) //1:1
#define SSUSB_FPGA_I2C_SDA_OUT_3P                 (0x1<<0) //0:0

//U3D_SSUSB_FPGA_I2C_IN_3P
#define SSUSB_FPGA_I2C_SCL_IN_3P                  (0x1<<1) //1:1
#define SSUSB_FPGA_I2C_SDA_IN_3P                  (0x1<<0) //0:0

//U3D_SSUSB_FPGA_I2C_OUT_4P
#define SSUSB_FPGA_I2C_SCL_OEN_4P                 (0x1<<3) //3:3
#define SSUSB_FPGA_I2C_SCL_OUT_4P                 (0x1<<2) //2:2
#define SSUSB_FPGA_I2C_SDA_OEN_4P                 (0x1<<1) //1:1
#define SSUSB_FPGA_I2C_SDA_OUT_4P                 (0x1<<0) //0:0

//U3D_SSUSB_FPGA_I2C_IN_4P
#define SSUSB_FPGA_I2C_SCL_IN_4P                  (0x1<<1) //1:1
#define SSUSB_FPGA_I2C_SDA_IN_4P                  (0x1<<0) //0:0

//U3D_SSUSB_IP_SLV_TMOUT
#define SSUSB_IP_SLV_TMOUT                        (0xffffffff<<0) //31:0


/* SSUSB_SIFSLV_IPPC FIELD OFFSET DEFINITION */

//U3D_SSUSB_IP_PW_CTRL0
#define SSUSB_AHB_SLV_AUTO_RSP_OFST               (17)
#define SSUSB_IP_SW_RST_CK_GATE_EN_OFST           (16)
#define SSUSB_IP_U2_ENTER_SLEEP_CNT_OFST          (8)
#define SSUSB_IP_SW_RST_OFST                      (0)

//U3D_SSUSB_IP_PW_CTRL1
#define SSUSB_IP_HOST_PDN_OFST                    (0)

//U3D_SSUSB_IP_PW_CTRL2
#define SSUSB_IP_DEV_PDN_OFST                     (0)

//U3D_SSUSB_IP_PW_CTRL3
#define SSUSB_IP_PCIE_PDN_OFST                    (0)

//U3D_SSUSB_IP_PW_STS1
#define SSUSB_IP_REF_CK_DIS_STS_OFST              (31)
#define SSUSB_IP_SLEEP_STS_OFST                   (30)
#define SSUSB_U2_MAC_RST_B_STS_5P_OFST            (29)
#define SSUSB_U2_MAC_RST_B_STS_4P_OFST            (28)
#define SSUSB_U2_MAC_RST_B_STS_3P_OFST            (27)
#define SSUSB_U2_MAC_RST_B_STS_2P_OFST            (26)
#define SSUSB_U2_MAC_RST_B_STS_1P_OFST            (25)
#define SSUSB_U2_MAC_RST_B_STS_OFST               (24)
#define SSUSB_U3_MAC_RST_B_STS_3P_OFST            (19)
#define SSUSB_U3_MAC_RST_B_STS_2P_OFST            (18)
#define SSUSB_U3_MAC_RST_B_STS_1P_OFST            (17)
#define SSUSB_U3_MAC_RST_B_STS_OFST               (16)
#define SSUSB_DEV_DRAM_RST_B_STS_OFST             (13)
#define SSUSB_XHCI_DRAM_RST_B_STS_OFST            (12)
#define SSUSB_XHCI_RST_B_STS_OFST                 (11)
#define SSUSB_SYS125_RST_B_STS_OFST               (10)
#define SSUSB_SYS60_RST_B_STS_OFST                (9)
#define SSUSB_REF_RST_B_STS_OFST                  (8)
#define SSUSB_DEV_RST_B_STS_OFST                  (3)
#define SSUSB_DEV_BMU_RST_B_STS_OFST              (2)
#define SSUSB_DEV_QMU_RST_B_STS_OFST              (1)
#define SSUSB_SYSPLL_STABLE_OFST                  (0)

//U3D_SSUSB_IP_PW_STS2
#define SSUSB_U2_MAC_SYS_RST_B_STS_5P_OFST        (5)
#define SSUSB_U2_MAC_SYS_RST_B_STS_4P_OFST        (4)
#define SSUSB_U2_MAC_SYS_RST_B_STS_3P_OFST        (3)
#define SSUSB_U2_MAC_SYS_RST_B_STS_2P_OFST        (2)
#define SSUSB_U2_MAC_SYS_RST_B_STS_1P_OFST        (1)
#define SSUSB_U2_MAC_SYS_RST_B_STS_OFST           (0)

//U3D_SSUSB_OTG_STS
#define SSUSB_XHCI_MAS_DMA_REQ_OFST               (14)
#define SSUSB_DEV_DMA_REQ_OFST                    (13)
#define SSUSB_AVALID_STS_OFST                     (12)
#define SSUSB_SRP_REQ_INTR_OFST                   (11)
#define SSUSB_IDDIG_OFST                          (10)
#define SSUSB_VBUS_VALID_OFST                     (9)
#define SSUSB_HOST_DEV_MODE_OFST                  (8)
#define SSUSB_DEV_USBRST_INTR_OFST                (7)
#define VBUS_CHG_INTR_OFST                        (6)
#define SSUSB_CHG_B_ROLE_B_OFST                   (5)
#define SSUSB_CHG_A_ROLE_B_OFST                   (4)
#define SSUSB_ATTACH_B_ROLE_OFST                  (3)
#define SSUSB_CHG_B_ROLE_A_OFST                   (2)
#define SSUSB_CHG_A_ROLE_A_OFST                   (1)
#define SSUSB_ATTACH_A_ROLE_OFST                  (0)

//U3D_SSUSB_OTG_STS_CLR
#define SSUSB_SRP_REQ_INTR_CLR_OFST               (11)
#define SSUSB_DEV_USBRST_INTR_CLR_OFST            (7)
#define SSUSB_VBUS_INTR_CLR_OFST                  (6)
#define SSUSB_CHG_B_ROLE_B_CLR_OFST               (5)
#define SSUSB_CHG_A_ROLE_B_CLR_OFST               (4)
#define SSUSB_ATTACH_B_ROLE_CLR_OFST              (3)
#define SSUSB_CHG_B_ROLE_A_CLR_OFST               (2)
#define SSUSB_CHG_A_ROLE_A_CLR_OFST               (1)
#define SSUSB_ATTACH_A_ROLE_CLR_OFST              (0)

//U3D_SSUSB_IP_MAC_CAP
#define SSUSB_IP_MAC_U2_PORT_NO_OFST              (8)
#define SSUSB_IP_MAC_U3_PORT_NO_OFST              (0)

//U3D_SSUSB_IP_XHCI_CAP
#define SSUSB_IP_XHCI_U2_PORT_NO_OFST             (8)
#define SSUSB_IP_XHCI_U3_PORT_NO_OFST             (0)

//U3D_SSUSB_IP_DEV_CAP
#define SSUSB_IP_DEV_U2_PORT_NO_OFST              (8)
#define SSUSB_IP_DEV_U3_PORT_NO_OFST              (0)

//U3D_SSUSB_OTG_INT_EN
#define SSUSB_DEV_USBRST_INT_EN_OFST              (8)
#define SSUSB_VBUS_CHG_INT_A_EN_OFST              (7)
#define SSUSB_VBUS_CHG_INT_B_EN_OFST              (6)
#define SSUSB_CHG_B_ROLE_B_INT_EN_OFST            (5)
#define SSUSB_CHG_A_ROLE_B_INT_EN_OFST            (4)
#define SSUSB_ATTACH_B_ROLE_INT_EN_OFST           (3)
#define SSUSB_CHG_B_ROLE_A_INT_EN_OFST            (2)
#define SSUSB_CHG_A_ROLE_A_INT_EN_OFST            (1)
#define SSUSB_ATTACH_A_ROLE_INT_EN_OFST           (0)

//U3D_SSUSB_U3_CTRL_0P
#define SSUSB_U3_PORT_PHYD_RST_OFST               (5)
#define SSUSB_U3_PORT_MAC_RST_OFST                (4)
#define SSUSB_U3_PORT_U2_CG_EN_OFST               (3)
#define SSUSB_U3_PORT_HOST_SEL_OFST               (2)
#define SSUSB_U3_PORT_PDN_OFST                    (1)
#define SSUSB_U3_PORT_DIS_OFST                    (0)

//U3D_SSUSB_U3_CTRL_1P
#define SSUSB_U3_PORT_PHYD_RST_1P_OFST            (5)
#define SSUSB_U3_PORT_MAC_RST_1P_OFST             (4)
#define SSUSB_U3_PORT_U2_CG_EN_1P_OFST            (3)
#define SSUSB_U3_PORT_HOST_SEL_1P_OFST            (2)
#define SSUSB_U3_PORT_PDN_1P_OFST                 (1)
#define SSUSB_U3_PORT_DIS_1P_OFST                 (0)

//U3D_SSUSB_U3_CTRL_2P
#define SSUSB_U3_PORT_PHYD_RST_2P_OFST            (5)
#define SSUSB_U3_PORT_MAC_RST_2P_OFST             (4)
#define SSUSB_U3_PORT_U2_CG_EN_2P_OFST            (3)
#define SSUSB_U3_PORT_HOST_SEL_2P_OFST            (2)
#define SSUSB_U3_PORT_PDN_2P_OFST                 (1)
#define SSUSB_U3_PORT_DIS_2P_OFST                 (0)

//U3D_SSUSB_U3_CTRL_3P
#define SSUSB_U3_PORT_PHYD_RST_3P_OFST            (5)
#define SSUSB_U3_PORT_MAC_RST_3P_OFST             (4)
#define SSUSB_U3_PORT_U2_CG_EN_3P_OFST            (3)
#define SSUSB_U3_PORT_HOST_SEL_3P_OFST            (2)
#define SSUSB_U3_PORT_PDN_3P_OFST                 (1)
#define SSUSB_U3_PORT_DIS_3P_OFST                 (0)

//U3D_SSUSB_U2_CTRL_0P
#define SSUSB_U2_PORT_OTG_HOST_VBUSVALID_SEL_OFST (9)
#define SSUSB_U2_PORT_OTG_MAC_AUTO_SEL_OFST       (8)
#define SSUSB_U2_PORT_OTG_SEL_OFST                (7)
#define SSUSB_U2_PORT_PLL_STABLE_SEL_OFST         (6)
#define SSUSB_U2_PORT_PHYD_RST_OFST               (5)
#define SSUSB_U2_PORT_MAC_RST_OFST                (4)
#define SSUSB_U2_PORT_U2_CG_EN_OFST               (3)
#define SSUSB_U2_PORT_HOST_SEL_OFST               (2)
#define SSUSB_U2_PORT_PDN_OFST                    (1)
#define SSUSB_U2_PORT_DIS_OFST                    (0)

//U3D_SSUSB_U2_CTRL_1P
#define SSUSB_U2_PORT_PLL_STABLE_SEL_1P_OFST      (6)
#define SSUSB_U2_PORT_PHYD_RST_1P_OFST            (5)
#define SSUSB_U2_PORT_MAC_RST_1P_OFST             (4)
#define SSUSB_U2_PORT_U2_CG_EN_1P_OFST            (3)
#define SSUSB_U2_PORT_HOST_SEL_1P_OFST            (2)
#define SSUSB_U2_PORT_PDN_1P_OFST                 (1)
#define SSUSB_U2_PORT_DIS_1P_OFST                 (0)

//U3D_SSUSB_U2_CTRL_2P
#define SSUSB_U2_PORT_PLL_STABLE_SEL_2P_OFST      (6)
#define SSUSB_U2_PORT_PHYD_RST_2P_OFST            (5)
#define SSUSB_U2_PORT_MAC_RST_2P_OFST             (4)
#define SSUSB_U2_PORT_U2_CG_EN_2P_OFST            (3)
#define SSUSB_U2_PORT_HOST_SEL_2P_OFST            (2)
#define SSUSB_U2_PORT_PDN_2P_OFST                 (1)
#define SSUSB_U2_PORT_DIS_2P_OFST                 (0)

//U3D_SSUSB_U2_CTRL_3P
#define SSUSB_U2_PORT_PLL_STABLE_SEL_3P_OFST      (6)
#define SSUSB_U2_PORT_PHYD_RST_3P_OFST            (5)
#define SSUSB_U2_PORT_MAC_RST_3P_OFST             (4)
#define SSUSB_U2_PORT_U2_CG_EN_3P_OFST            (3)
#define SSUSB_U2_PORT_HOST_SEL_3P_OFST            (2)
#define SSUSB_U2_PORT_PDN_3P_OFST                 (1)
#define SSUSB_U2_PORT_DIS_3P_OFST                 (0)

//U3D_SSUSB_U2_CTRL_4P
#define SSUSB_U2_PORT_PLL_STABLE_SEL_4P_OFST      (6)
#define SSUSB_U2_PORT_PHYD_RST_4P_OFST            (5)
#define SSUSB_U2_PORT_MAC_RST_4P_OFST             (4)
#define SSUSB_U2_PORT_U2_CG_EN_4P_OFST            (3)
#define SSUSB_U2_PORT_HOST_SEL_4P_OFST            (2)
#define SSUSB_U2_PORT_PDN_4P_OFST                 (1)
#define SSUSB_U2_PORT_DIS_4P_OFST                 (0)

//U3D_SSUSB_U2_CTRL_5P
#define SSUSB_U2_PORT_PLL_STABLE_SEL_5P_OFST      (6)
#define SSUSB_U2_PORT_PHYD_RST_5P_OFST            (5)
#define SSUSB_U2_PORT_MAC_RST_5P_OFST             (4)
#define SSUSB_U2_PORT_U2_CG_EN_5P_OFST            (3)
#define SSUSB_U2_PORT_HOST_SEL_5P_OFST            (2)
#define SSUSB_U2_PORT_PDN_5P_OFST                 (1)
#define SSUSB_U2_PORT_DIS_5P_OFST                 (0)

//U3D_SSUSB_U2_PHY_PLL
#define SSUSB_SYSPLL_USE_OFST                     (30)
#define RG_SSUSB_U2_PLL_STB_OFST                  (29)
#define SSUSB_U2_FORCE_PLL_STB_OFST               (28)
#define SSUSB_U2_PORT_PHY_CK_DEB_TIMER_OFST       (24)
#define SSUSB_U2_PORT_LPM_PLL_STABLE_TIMER_OFST   (16)
#define SSUSB_U2_PORT_PLL_STABLE_TIMER_OFST       (8)
#define SSUSB_U2_PORT_1US_TIMER_OFST              (0)

//U3D_SSUSB_DMA_CTRL
#define SSUSB_IP_DMA_BUS_CK_GATE_DIS_OFST         (0)

//U3D_SSUSB_MAC_CK_CTRL
#define SSUSB_MAC3_SYS_CK_GATE_MASK_TIME_OFST     (16)
#define SSUSB_MAC2_SYS_CK_GATE_MASK_TIME_OFST     (8)
#define SSUSB_PHY_REF_CK_DIV2_OFST                (4)
#define SSUSB_MAC3_SYS_CK_GATE_MODE_OFST          (2)
#define SSUSB_MAC2_SYS_CK_GATE_MODE_OFST          (0)

//U3D_SSUSB_CSR_CK_CTRL
#define SSUSB_SIFSLV_MCU_BUS_CK_GATE_EN_OFST      (1)
#define SSUSB_CSR_MCU_BUS_CK_GATE_EN_OFST         (0)

//U3D_SSUSB_REF_CK_CTRL
#define SSUSB_REF_MAC2_CK_GATE_EN_OFST            (4)
#define SSUSB_REF_MAC3_CK_GATE_EN_OFST            (3)
#define SSUSB_REF_CK_GATE_EN_OFST                 (2)
#define SSUSB_REF_PHY_CK_GATE_EN_OFST             (1)
#define SSUSB_REF_MAC_CK_GATE_EN_OFST             (0)

//U3D_SSUSB_XHCI_CK_CTRL
#define SSUSB_XACT3_XHCI_CK_GATE_MASK_TIME_OFST   (8)
#define SSUSB_XACT3_XHCI_CK_GATE_MODE_OFST        (4)
#define SSUSB_XHCI_CK_DIV2_EN_OFST                (0)

//U3D_SSUSB_XHCI_RST_CTRL
#define SSUSB_XHCI_SW_DRAM_RST_OFST               (4)
#define SSUSB_XHCI_SW_SYS60_RST_OFST              (3)
#define SSUSB_XHCI_SW_SYS125_RST_OFST             (2)
#define SSUSB_XHCI_SW_XHCI_RST_OFST               (1)
#define SSUSB_XHCI_SW_RST_OFST                    (0)

//U3D_SSUSB_DEV_RST_CTRL
#define SSUSB_DEV_SW_DRAM_RST_OFST                (3)
#define SSUSB_DEV_SW_QMU_RST_OFST                 (2)
#define SSUSB_DEV_SW_BMU_RST_OFST                 (1)
#define SSUSB_DEV_SW_RST_OFST                     (0)

//U3D_SSUSB_SYS_CK_CTRL
#define SSUSB_SYS60_CK_EXT_SEL_OFST               (2)
#define SSUSB_SYS_CK_EXT_SEL_OFST                 (1)
#define SSUSB_SYS_CK_DIV2_EN_OFST                 (0)

//U3D_SSUSB_HW_ID
#define SSUSB_HW_ID_OFST                          (0)

//U3D_SSUSB_HW_SUB_ID
#define SSUSB_HW_SUB_ID_OFST                      (0)

//U3D_SSUSB_PRB_CTRL0
#define PRB_BYTE3_EN_OFST                         (3)
#define PRB_BYTE2_EN_OFST                         (2)
#define PRB_BYTE1_EN_OFST                         (1)
#define PRB_BYTE0_EN_OFST                         (0)

//U3D_SSUSB_PRB_CTRL1
#define PRB_BYTE1_SEL_OFST                        (16)
#define PRB_BYTE0_SEL_OFST                        (0)

//U3D_SSUSB_PRB_CTRL2
#define PRB_BYTE3_SEL_OFST                        (16)
#define PRB_BYTE2_SEL_OFST                        (0)

//U3D_SSUSB_PRB_CTRL3
#define PRB_BYTE3_MODULE_SEL_OFST                 (24)
#define PRB_BYTE2_MODULE_SEL_OFST                 (16)
#define PRB_BYTE1_MODULE_SEL_OFST                 (8)
#define PRB_BYTE0_MODULE_SEL_OFST                 (0)

//U3D_SSUSB_PRB_CTRL4
#define SW_PRB_OUT_OFST                           (0)

//U3D_SSUSB_PRB_CTRL5
#define PRB_RD_DATA_OFST                          (0)

//U3D_SSUSB_IP_SPARE0
#define SSUSB_IP_SPARE0_OFST                      (0)

//U3D_SSUSB_IP_SPARE1
#define SSUSB_IP_SPARE1_OFST                      (0)

//U3D_SSUSB_FPGA_I2C_OUT_0P
#define SSUSB_FPGA_I2C_SCL_OEN_0P_OFST            (3)
#define SSUSB_FPGA_I2C_SCL_OUT_0P_OFST            (2)
#define SSUSB_FPGA_I2C_SDA_OEN_0P_OFST            (1)
#define SSUSB_FPGA_I2C_SDA_OUT_0P_OFST            (0)

//U3D_SSUSB_FPGA_I2C_IN_0P
#define SSUSB_FPGA_I2C_SCL_IN_0P_OFST             (1)
#define SSUSB_FPGA_I2C_SDA_IN_0P_OFST             (0)

//U3D_SSUSB_FPGA_I2C_OUT_1P
#define SSUSB_FPGA_I2C_SCL_OEN_1P_OFST            (3)
#define SSUSB_FPGA_I2C_SCL_OUT_1P_OFST            (2)
#define SSUSB_FPGA_I2C_SDA_OEN_1P_OFST            (1)
#define SSUSB_FPGA_I2C_SDA_OUT_1P_OFST            (0)

//U3D_SSUSB_FPGA_I2C_IN_1P
#define SSUSB_FPGA_I2C_SCL_IN_1P_OFST             (1)
#define SSUSB_FPGA_I2C_SDA_IN_1P_OFST             (0)

//U3D_SSUSB_FPGA_I2C_OUT_2P
#define SSUSB_FPGA_I2C_SCL_OEN_2P_OFST            (3)
#define SSUSB_FPGA_I2C_SCL_OUT_2P_OFST            (2)
#define SSUSB_FPGA_I2C_SDA_OEN_2P_OFST            (1)
#define SSUSB_FPGA_I2C_SDA_OUT_2P_OFST            (0)

//U3D_SSUSB_FPGA_I2C_IN_2P
#define SSUSB_FPGA_I2C_SCL_IN_2P_OFST             (1)
#define SSUSB_FPGA_I2C_SDA_IN_2P_OFST             (0)

//U3D_SSUSB_FPGA_I2C_OUT_3P
#define SSUSB_FPGA_I2C_SCL_OEN_3P_OFST            (3)
#define SSUSB_FPGA_I2C_SCL_OUT_3P_OFST            (2)
#define SSUSB_FPGA_I2C_SDA_OEN_3P_OFST            (1)
#define SSUSB_FPGA_I2C_SDA_OUT_3P_OFST            (0)

//U3D_SSUSB_FPGA_I2C_IN_3P
#define SSUSB_FPGA_I2C_SCL_IN_3P_OFST             (1)
#define SSUSB_FPGA_I2C_SDA_IN_3P_OFST             (0)

//U3D_SSUSB_FPGA_I2C_OUT_4P
#define SSUSB_FPGA_I2C_SCL_OEN_4P_OFST            (3)
#define SSUSB_FPGA_I2C_SCL_OUT_4P_OFST            (2)
#define SSUSB_FPGA_I2C_SDA_OEN_4P_OFST            (1)
#define SSUSB_FPGA_I2C_SDA_OUT_4P_OFST            (0)

//U3D_SSUSB_FPGA_I2C_IN_4P
#define SSUSB_FPGA_I2C_SCL_IN_4P_OFST             (1)
#define SSUSB_FPGA_I2C_SDA_IN_4P_OFST             (0)

//U3D_SSUSB_IP_SLV_TMOUT
#define SSUSB_IP_SLV_TMOUT_OFST                   (0)

//////////////////////////////////////////////////////////////////////
