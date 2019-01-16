/* SSUSB_USB2_CSR REGISTER DEFINITION */

#define U3D_XHCI_PORT_CTRL                        (SSUSB_USB2_CSR_BASE+0x0000)
#define U3D_POWER_MANAGEMENT                      (SSUSB_USB2_CSR_BASE+0x0004)
#define U3D_TIMING_TEST_MODE                      (SSUSB_USB2_CSR_BASE+0x0008)
#define U3D_DEVICE_CONTROL                        (SSUSB_USB2_CSR_BASE+0x000C)
#define U3D_POWER_UP_COUNTER                      (SSUSB_USB2_CSR_BASE+0x0010)
#define U3D_USB2_TEST_MODE                        (SSUSB_USB2_CSR_BASE+0x0014)
#define U3D_COMMON_USB_INTR_ENABLE                (SSUSB_USB2_CSR_BASE+0x0018)
#define U3D_COMMON_USB_INTR                       (SSUSB_USB2_CSR_BASE+0x001C)
#define U3D_USB_BUS_PERFORMANCE                   (SSUSB_USB2_CSR_BASE+0x0020)
#define U3D_LINK_RESET_INFO                       (SSUSB_USB2_CSR_BASE+0x0024)
#define U3D_RESET_RESUME_TIME_VALUE               (SSUSB_USB2_CSR_BASE+0x0034)
#define U3D_UTMI_SIGNAL_SEL                       (SSUSB_USB2_CSR_BASE+0x0038)
#define U3D_USB20_FRAME_NUM                       (SSUSB_USB2_CSR_BASE+0x003C)
#define U3D_USB20_TIMING_PARAMETER                (SSUSB_USB2_CSR_BASE+0x0040)
#define U3D_USB20_LPM_PARAMETER                   (SSUSB_USB2_CSR_BASE+0x0044)
#define U3D_USB20_LPM_ENTRY_COUNT                 (SSUSB_USB2_CSR_BASE+0x0048)
#define U3D_USB20_MISC_CONTROL                    (SSUSB_USB2_CSR_BASE+0x004C)
#define U3D_USB20_LPM_TIMING_PARAM                (SSUSB_USB2_CSR_BASE+0x0050)
#define U3D_USB20_OPSTATE                         (SSUSB_USB2_CSR_BASE+0x0060)

/* SSUSB_USB2_CSR FIELD DEFINITION */

//U3D_XHCI_PORT_CTRL
#define GO_POLLING                                (0x1<<7) //7:7

//U3D_POWER_MANAGEMENT
#define LPM_BESL_STALL                            (0x1<<14) //14:14
#define LPM_BESLD_STALL                           (0x1<<13) //13:13
#define BC12_EN                                   (0x1<<12) //12:12
#define LPM_RWP                                   (0x1<<11) //11:11
#define LPM_HRWE                                  (0x1<<10) //10:10
#define LPM_MODE                                  (0x3<<8) //9:8
#define ISO_UPDATE                                (0x1<<7) //7:7
#define SOFT_CONN                                 (0x1<<6) //6:6
#define HS_ENABLE                                 (0x1<<5) //5:5
#define HS_MODE                                   (0x1<<4) //4:4
#define BUS_RESET                                 (0x1<<3) //3:3
#define RESUME                                    (0x1<<2) //2:2
#define SUSPEND                                   (0x1<<1) //1:1
#define SUSPENDM_ENABLE                           (0x1<<0) //0:0

//U3D_TIMING_TEST_MODE
#define FS_DIS_SEL                                (0xf<<4) //7:4
#define PHY_CLK_VALID                             (0x1<<3) //3:3
#define FS_DIS_NE                                 (0x1<<2) //2:2
#define TM1                                       (0x1<<0) //0:0

//U3D_DEVICE_CONTROL
#define HW_AUTO_SENDRST_EN                        (0x1<<8) //8:8
#define B_DEV                                     (0x1<<7) //7:7
#define FS_DEV                                    (0x1<<6) //6:6
#define LS_DEV                                    (0x1<<5) //5:5
#define VBUS                                      (0x3<<3) //4:3
#define HOSTMODE                                  (0x1<<2) //2:2
#define HOSTREQ                                   (0x1<<1) //1:1
#define SESSION                                   (0x1<<0) //0:0

//U3D_POWER_UP_COUNTER
#define LPM_HUBDRVRMP_TIME                        (0xffff<<8) //23:8
#define PWR_UP_CNT_LMT                            (0xf<<0) //3:0

//U3D_USB2_TEST_MODE
#define U2U3_AUTO_SWITCH                          (0x1<<10) //10:10
#define HOST_FORCE_EN                             (0x1<<9) //9:9
#define LPM_FORCE_STALL                           (0x1<<8) //8:8
#define FORCE_HOST                                (0x1<<7) //7:7
#define FIFO_ACCESS                               (0x1<<6) //6:6
#define FORCE_FS                                  (0x1<<5) //5:5
#define FORCE_HS                                  (0x1<<4) //4:4
#define TEST_PACKET_MODE                          (0x1<<3) //3:3
#define TEST_K_MODE                               (0x1<<2) //2:2
#define TEST_J_MODE                               (0x1<<1) //1:1
#define TEST_SE0_NAK_MODE                         (0x1<<0) //0:0

//U3D_COMMON_USB_INTR_ENABLE
#define LPM_RESUME_INTR_EN                        (0x1<<9) //9:9
#define LPM_INTR_EN                               (0x1<<8) //8:8
#define VBUSERR_INTR_EN                           (0x1<<7) //7:7
#define SESSION_REQ_INTR_EN                       (0x1<<6) //6:6
#define DISCONN_INTR_EN                           (0x1<<5) //5:5
#define CONN_INTR_EN                              (0x1<<4) //4:4
#define SOF_INTR_EN                               (0x1<<3) //3:3
#define RESET_INTR_EN                             (0x1<<2) //2:2
#define RESUME_INTR_EN                            (0x1<<1) //1:1
#define SUSPEND_INTR_EN                           (0x1<<0) //0:0

//U3D_COMMON_USB_INTR
#define LPM_RESUME_INTR                           (0x1<<9) //9:9
#define LPM_INTR                                  (0x1<<8) //8:8
#define VBUSERR_INTR                              (0x1<<7) //7:7
#define SESSION_REQ_INTR                          (0x1<<6) //6:6
#define DISCONN_INTR                              (0x1<<5) //5:5
#define CONN_INTR                                 (0x1<<4) //4:4
#define SOF_INTR                                  (0x1<<3) //3:3
#define RESET_INTR                                (0x1<<2) //2:2
#define RESUME_INTR                               (0x1<<1) //1:1
#define SUSPEND_INTR                              (0x1<<0) //0:0

//U3D_USB_BUS_PERFORMANCE
#define XFER_START_FROM_SOF                       (0x1<<24) //24:24
#define VBUSERR_MODE                              (0x1<<23) //23:23
#define TX_FLUSH_EN                               (0x1<<22) //22:22
#define NOISE_STILL_SOF                           (0x1<<21) //21:21
#define UNDO_SRP_FIX                              (0x1<<19) //19:19
#define OTG_DEGLITCH_DISABLE                      (0x1<<18) //18:18
#define SWRST                                     (0x1<<17) //17:17
#define DIS_USB_RESET                             (0x1<<16) //16:16
#define SOFT_DEBOUCE                              (0x1<<0) //0:0

//U3D_LINK_RESET_INFO
#define WTWRSM                                    (0xf<<28) //31:28
#define WTRSMK                                    (0xf<<24) //27:24
#define WRFSSE0                                   (0xf<<20) //23:20
#define WTCHRP                                    (0xf<<16) //19:16
#define VPLEN                                     (0xff<<8) //15:8
#define WTCON                                     (0xf<<4) //7:4
#define WTID                                      (0xf<<0) //3:0

//U3D_RESET_RESUME_TIME_VALUE
#define USB20_RESET_TIME_VALUE                    (0xffff<<0) //15:0

//U3D_UTMI_SIGNAL_SEL
#define TX_SIGNAL_SEL                             (0x3<<2) //3:2
#define RX_SIGNAL_SEL                             (0x3<<0) //1:0

//U3D_USB20_FRAME_NUM
#define FRAME_NUMBER                              (0x7ff<<0) //10:0

//U3D_USB20_TIMING_PARAMETER
#define CHOPPER_DELAY_TIME                        (0xff<<16) //23:16
#define SOFTCON_DELAY_TIME                        (0xff<<8) //15:8
#define TIME_VALUE_1US                            (0xff<<0) //7:0

//U3D_USB20_LPM_PARAMETER
#define BESLCK_U3                                 (0xf<<12) //15:12
#define BESLCK                                    (0xf<<8) //11:8
#define BESLDCK                                   (0xf<<4) //7:4
#define BESL                                      (0xf<<0) //3:0

//U3D_USB20_LPM_ENTRY_COUNT
#define LPM_EXIT_COUNT                            (0xff<<16) //23:16
#define LPM_EXIT_COUNT_RESET                      (0x1<<9) //9:9
#define LPM_ENTRY_COUNT_RESET                     (0x1<<8) //8:8
#define LPM_ENTRY_COUNT                           (0xff<<0) //7:0

//U3D_USB20_MISC_CONTROL
#define LPM_U3_ACK_EN                             (0x1<<0) //0:0

//U3D_USB20_LPM_TIMING_PARAM
#define LPM_L1_TOKENRETRY                         (0x1ff<<16) //24:16
#define LPM_L1_RESIDENCY                          (0xfff<<0) //11:0

//U3D_USB20_OPSTATE
#define OPSTATE_SYS                               (0x3f<<0) //5:0


/* SSUSB_USB2_CSR FIELD OFFSET DEFINITION */

//U3D_XHCI_PORT_CTRL
#define GO_POLLING_OFST                           (7)

//U3D_POWER_MANAGEMENT
#define LPM_BESL_STALL_OFST                       (14)
#define LPM_BESLD_STALL_OFST                      (13)
#define BC12_EN_OFST                              (12)
#define LPM_RWP_OFST                              (11)
#define LPM_HRWE_OFST                             (10)
#define LPM_MODE_OFST                             (8)
#define ISO_UPDATE_OFST                           (7)
#define SOFT_CONN_OFST                            (6)
#define HS_ENABLE_OFST                            (5)
#define HS_MODE_OFST                              (4)
#define BUS_RESET_OFST                            (3)
#define RESUME_OFST                               (2)
#define SUSPEND_OFST                              (1)
#define SUSPENDM_ENABLE_OFST                      (0)

//U3D_TIMING_TEST_MODE
#define FS_DIS_SEL_OFST                           (4)
#define PHY_CLK_VALID_OFST                        (3)
#define FS_DIS_NE_OFST                            (2)
#define TM1_OFST                                  (0)

//U3D_DEVICE_CONTROL
#define HW_AUTO_SENDRST_EN_OFST                   (8)
#define B_DEV_OFST                                (7)
#define FS_DEV_OFST                               (6)
#define LS_DEV_OFST                               (5)
#define VBUS_OFST                                 (3)
#define HOSTMODE_OFST                             (2)
#define HOSTREQ_OFST                              (1)
#define SESSION_OFST                              (0)

//U3D_POWER_UP_COUNTER
#define LPM_HUBDRVRMP_TIME_OFST                   (8)
#define PWR_UP_CNT_LMT_OFST                       (0)

//U3D_USB2_TEST_MODE
#define U2U3_AUTO_SWITCH_OFST                     (10)
#define HOST_FORCE_EN_OFST                        (9)
#define LPM_FORCE_STALL_OFST                      (8)
#define FORCE_HOST_OFST                           (7)
#define FIFO_ACCESS_OFST                          (6)
#define FORCE_FS_OFST                             (5)
#define FORCE_HS_OFST                             (4)
#define TEST_PACKET_MODE_OFST                     (3)
#define TEST_K_MODE_OFST                          (2)
#define TEST_J_MODE_OFST                          (1)
#define TEST_SE0_NAK_MODE_OFST                    (0)

//U3D_COMMON_USB_INTR_ENABLE
#define LPM_RESUME_INTR_EN_OFST                   (9)
#define LPM_INTR_EN_OFST                          (8)
#define VBUSERR_INTR_EN_OFST                      (7)
#define SESSION_REQ_INTR_EN_OFST                  (6)
#define DISCONN_INTR_EN_OFST                      (5)
#define CONN_INTR_EN_OFST                         (4)
#define SOF_INTR_EN_OFST                          (3)
#define RESET_INTR_EN_OFST                        (2)
#define RESUME_INTR_EN_OFST                       (1)
#define SUSPEND_INTR_EN_OFST                      (0)

//U3D_COMMON_USB_INTR
#define LPM_RESUME_INTR_OFST                      (9)
#define LPM_INTR_OFST                             (8)
#define VBUSERR_INTR_OFST                         (7)
#define SESSION_REQ_INTR_OFST                     (6)
#define DISCONN_INTR_OFST                         (5)
#define CONN_INTR_OFST                            (4)
#define SOF_INTR_OFST                             (3)
#define RESET_INTR_OFST                           (2)
#define RESUME_INTR_OFST                          (1)
#define SUSPEND_INTR_OFST                         (0)

//U3D_USB_BUS_PERFORMANCE
#define XFER_START_FROM_SOF_OFST                  (24)
#define VBUSERR_MODE_OFST                         (23)
#define TX_FLUSH_EN_OFST                          (22)
#define NOISE_STILL_SOF_OFST                      (21)
#define UNDO_SRP_FIX_OFST                         (19)
#define OTG_DEGLITCH_DISABLE_OFST                 (18)
#define SWRST_OFST                                (17)
#define DIS_USB_RESET_OFST                        (16)
#define SOFT_DEBOUCE_OFST                         (0)

//U3D_LINK_RESET_INFO
#define WTWRSM_OFST                               (28)
#define WTRSMK_OFST                               (24)
#define WRFSSE0_OFST                              (20)
#define WTCHRP_OFST                               (16)
#define VPLEN_OFST                                (8)
#define WTCON_OFST                                (4)
#define WTID_OFST                                 (0)

//U3D_RESET_RESUME_TIME_VALUE
#define USB20_RESET_TIME_VALUE_OFST               (0)

//U3D_UTMI_SIGNAL_SEL
#define TX_SIGNAL_SEL_OFST                        (2)
#define RX_SIGNAL_SEL_OFST                        (0)

//U3D_USB20_FRAME_NUM
#define FRAME_NUMBER_OFST                         (0)

//U3D_USB20_TIMING_PARAMETER
#define CHOPPER_DELAY_TIME_OFST                   (16)
#define SOFTCON_DELAY_TIME_OFST                   (8)
#define TIME_VALUE_1US_OFST                       (0)

//U3D_USB20_LPM_PARAMETER
#define BESLCK_U3_OFST                            (12)
#define BESLCK_OFST                               (8)
#define BESLDCK_OFST                              (4)
#define BESL_OFST                                 (0)

//U3D_USB20_LPM_ENTRY_COUNT
#define LPM_EXIT_COUNT_OFST                       (16)
#define LPM_EXIT_COUNT_RESET_OFST                 (9)
#define LPM_ENTRY_COUNT_RESET_OFST                (8)
#define LPM_ENTRY_COUNT_OFST                      (0)

//U3D_USB20_MISC_CONTROL
#define LPM_U3_ACK_EN_OFST                        (0)

//U3D_USB20_LPM_TIMING_PARAM
#define LPM_L1_TOKENRETRY_OFST                    (16)
#define LPM_L1_RESIDENCY_OFST                     (0)

//U3D_USB20_OPSTATE
#define OPSTATE_SYS_OFST                          (0)

//////////////////////////////////////////////////////////////////////
