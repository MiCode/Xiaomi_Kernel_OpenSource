/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __BGF_GPS_DMA_REGS_H__
#define __BGF_GPS_DMA_REGS_H__

#define BGF_GPS_DMA_BASE                                       0x80010000

#define BGF_GPS_DMA_DMA1_WPPT_ADDR                             (BGF_GPS_DMA_BASE + 0x0108)
#define BGF_GPS_DMA_DMA1_WPTO_ADDR                             (BGF_GPS_DMA_BASE + 0x010C)
#define BGF_GPS_DMA_DMA1_COUNT_ADDR                            (BGF_GPS_DMA_BASE + 0x0110)
#define BGF_GPS_DMA_DMA1_CON_ADDR                              (BGF_GPS_DMA_BASE + 0x0114)
#define BGF_GPS_DMA_DMA1_START_ADDR                            (BGF_GPS_DMA_BASE + 0x0118)
#define BGF_GPS_DMA_DMA1_INTSTA_ADDR                           (BGF_GPS_DMA_BASE + 0x011C)
#define BGF_GPS_DMA_DMA1_ACKINT_ADDR                           (BGF_GPS_DMA_BASE + 0x0120)
#define BGF_GPS_DMA_DMA1_RLCT_ADDR                             (BGF_GPS_DMA_BASE + 0x0124)
#define BGF_GPS_DMA_DMA1_PGMADDR_ADDR                          (BGF_GPS_DMA_BASE + 0x012C)
#define BGF_GPS_DMA_DMA1_STATE_ADDR                            (BGF_GPS_DMA_BASE + 0x0148)
#define BGF_GPS_DMA_DMA2_WPPT_ADDR                             (BGF_GPS_DMA_BASE + 0x0208)
#define BGF_GPS_DMA_DMA2_WPTO_ADDR                             (BGF_GPS_DMA_BASE + 0x020C)
#define BGF_GPS_DMA_DMA2_COUNT_ADDR                            (BGF_GPS_DMA_BASE + 0x0210)
#define BGF_GPS_DMA_DMA2_CON_ADDR                              (BGF_GPS_DMA_BASE + 0x0214)
#define BGF_GPS_DMA_DMA2_START_ADDR                            (BGF_GPS_DMA_BASE + 0x0218)
#define BGF_GPS_DMA_DMA2_INTSTA_ADDR                           (BGF_GPS_DMA_BASE + 0x021C)
#define BGF_GPS_DMA_DMA2_ACKINT_ADDR                           (BGF_GPS_DMA_BASE + 0x0220)
#define BGF_GPS_DMA_DMA2_RLCT_ADDR                             (BGF_GPS_DMA_BASE + 0x0224)
#define BGF_GPS_DMA_DMA2_PGMADDR_ADDR                          (BGF_GPS_DMA_BASE + 0x022C)
#define BGF_GPS_DMA_DMA3_WPPT_ADDR                             (BGF_GPS_DMA_BASE + 0x0308)
#define BGF_GPS_DMA_DMA3_WPTO_ADDR                             (BGF_GPS_DMA_BASE + 0x030C)
#define BGF_GPS_DMA_DMA3_COUNT_ADDR                            (BGF_GPS_DMA_BASE + 0x0310)
#define BGF_GPS_DMA_DMA3_CON_ADDR                              (BGF_GPS_DMA_BASE + 0x0314)
#define BGF_GPS_DMA_DMA3_START_ADDR                            (BGF_GPS_DMA_BASE + 0x0318)
#define BGF_GPS_DMA_DMA3_INTSTA_ADDR                           (BGF_GPS_DMA_BASE + 0x031C)
#define BGF_GPS_DMA_DMA3_ACKINT_ADDR                           (BGF_GPS_DMA_BASE + 0x0320)
#define BGF_GPS_DMA_DMA3_RLCT_ADDR                             (BGF_GPS_DMA_BASE + 0x0324)
#define BGF_GPS_DMA_DMA3_PGMADDR_ADDR                          (BGF_GPS_DMA_BASE + 0x032C)
#define BGF_GPS_DMA_DMA4_WPPT_ADDR                             (BGF_GPS_DMA_BASE + 0x0408)
#define BGF_GPS_DMA_DMA4_WPTO_ADDR                             (BGF_GPS_DMA_BASE + 0x040C)
#define BGF_GPS_DMA_DMA4_COUNT_ADDR                            (BGF_GPS_DMA_BASE + 0x0410)
#define BGF_GPS_DMA_DMA4_CON_ADDR                              (BGF_GPS_DMA_BASE + 0x0414)
#define BGF_GPS_DMA_DMA4_START_ADDR                            (BGF_GPS_DMA_BASE + 0x0418)
#define BGF_GPS_DMA_DMA4_INTSTA_ADDR                           (BGF_GPS_DMA_BASE + 0x041C)
#define BGF_GPS_DMA_DMA4_ACKINT_ADDR                           (BGF_GPS_DMA_BASE + 0x0420)
#define BGF_GPS_DMA_DMA4_RLCT_ADDR                             (BGF_GPS_DMA_BASE + 0x0424)
#define BGF_GPS_DMA_DMA4_PGMADDR_ADDR                          (BGF_GPS_DMA_BASE + 0x042C)


#define BGF_GPS_DMA_DMA1_WPPT_WPPT_ADDR                        BGF_GPS_DMA_DMA1_WPPT_ADDR
#define BGF_GPS_DMA_DMA1_WPPT_WPPT_MASK                        0x0000FFFF
#define BGF_GPS_DMA_DMA1_WPPT_WPPT_SHFT                        0

#define BGF_GPS_DMA_DMA1_WPTO_WPTO_ADDR                        BGF_GPS_DMA_DMA1_WPTO_ADDR
#define BGF_GPS_DMA_DMA1_WPTO_WPTO_MASK                        0xFFFFFFFF
#define BGF_GPS_DMA_DMA1_WPTO_WPTO_SHFT                        0

#define BGF_GPS_DMA_DMA1_COUNT_LEN_ADDR                        BGF_GPS_DMA_DMA1_COUNT_ADDR
#define BGF_GPS_DMA_DMA1_COUNT_LEN_MASK                        0x0000FFFF
#define BGF_GPS_DMA_DMA1_COUNT_LEN_SHFT                        0

#define BGF_GPS_DMA_DMA1_CON_MAS_ADDR                          BGF_GPS_DMA_DMA1_CON_ADDR
#define BGF_GPS_DMA_DMA1_CON_MAS_MASK                          0x00300000
#define BGF_GPS_DMA_DMA1_CON_MAS_SHFT                          20
#define BGF_GPS_DMA_DMA1_CON_W2B_ADDR                          BGF_GPS_DMA_DMA1_CON_ADDR
#define BGF_GPS_DMA_DMA1_CON_W2B_MASK                          0x00000040
#define BGF_GPS_DMA_DMA1_CON_W2B_SHFT                          6
#define BGF_GPS_DMA_DMA1_CON_B2W_ADDR                          BGF_GPS_DMA_DMA1_CON_ADDR
#define BGF_GPS_DMA_DMA1_CON_B2W_MASK                          0x00000020
#define BGF_GPS_DMA_DMA1_CON_B2W_SHFT                          5
#define BGF_GPS_DMA_DMA1_CON_SIZE_ADDR                         BGF_GPS_DMA_DMA1_CON_ADDR
#define BGF_GPS_DMA_DMA1_CON_SIZE_MASK                         0x00000003
#define BGF_GPS_DMA_DMA1_CON_SIZE_SHFT                         0

#define BGF_GPS_DMA_DMA1_START_STR_ADDR                        BGF_GPS_DMA_DMA1_START_ADDR
#define BGF_GPS_DMA_DMA1_START_STR_MASK                        0x00008000
#define BGF_GPS_DMA_DMA1_START_STR_SHFT                        15

#define BGF_GPS_DMA_DMA1_INTSTA_INT_ADDR                       BGF_GPS_DMA_DMA1_INTSTA_ADDR
#define BGF_GPS_DMA_DMA1_INTSTA_INT_MASK                       0x00008000
#define BGF_GPS_DMA_DMA1_INTSTA_INT_SHFT                       15

#define BGF_GPS_DMA_DMA1_ACKINT_ACK_ADDR                       BGF_GPS_DMA_DMA1_ACKINT_ADDR
#define BGF_GPS_DMA_DMA1_ACKINT_ACK_MASK                       0x00008000
#define BGF_GPS_DMA_DMA1_ACKINT_ACK_SHFT                       15

#define BGF_GPS_DMA_DMA1_RLCT_RLCT_ADDR                        BGF_GPS_DMA_DMA1_RLCT_ADDR
#define BGF_GPS_DMA_DMA1_RLCT_RLCT_MASK                        0x0000FFFF
#define BGF_GPS_DMA_DMA1_RLCT_RLCT_SHFT                        0

#define BGF_GPS_DMA_DMA1_PGMADDR_PGMADDR_ADDR                  BGF_GPS_DMA_DMA1_PGMADDR_ADDR
#define BGF_GPS_DMA_DMA1_PGMADDR_PGMADDR_MASK                  0xFFFFFFFF
#define BGF_GPS_DMA_DMA1_PGMADDR_PGMADDR_SHFT                  0

#define BGF_GPS_DMA_DMA1_STATE_STATE_ADDR                      BGF_GPS_DMA_DMA1_STATE_ADDR
#define BGF_GPS_DMA_DMA1_STATE_STATE_MASK                      0x0000007F
#define BGF_GPS_DMA_DMA1_STATE_STATE_SHFT                      0

#define BGF_GPS_DMA_DMA2_WPPT_WPPT_ADDR                        BGF_GPS_DMA_DMA2_WPPT_ADDR
#define BGF_GPS_DMA_DMA2_WPPT_WPPT_MASK                        0x0000FFFF
#define BGF_GPS_DMA_DMA2_WPPT_WPPT_SHFT                        0

#define BGF_GPS_DMA_DMA2_WPTO_WPTO_ADDR                        BGF_GPS_DMA_DMA2_WPTO_ADDR
#define BGF_GPS_DMA_DMA2_WPTO_WPTO_MASK                        0xFFFFFFFF
#define BGF_GPS_DMA_DMA2_WPTO_WPTO_SHFT                        0

#define BGF_GPS_DMA_DMA2_COUNT_LEN_ADDR                        BGF_GPS_DMA_DMA2_COUNT_ADDR
#define BGF_GPS_DMA_DMA2_COUNT_LEN_MASK                        0x0000FFFF
#define BGF_GPS_DMA_DMA2_COUNT_LEN_SHFT                        0

#define BGF_GPS_DMA_DMA2_START_STR_ADDR                        BGF_GPS_DMA_DMA2_START_ADDR
#define BGF_GPS_DMA_DMA2_START_STR_MASK                        0x00008000
#define BGF_GPS_DMA_DMA2_START_STR_SHFT                        15

#define BGF_GPS_DMA_DMA2_INTSTA_INT_ADDR                       BGF_GPS_DMA_DMA2_INTSTA_ADDR
#define BGF_GPS_DMA_DMA2_INTSTA_INT_MASK                       0x00008000
#define BGF_GPS_DMA_DMA2_INTSTA_INT_SHFT                       15

#define BGF_GPS_DMA_DMA2_ACKINT_ACK_ADDR                       BGF_GPS_DMA_DMA2_ACKINT_ADDR
#define BGF_GPS_DMA_DMA2_ACKINT_ACK_MASK                       0x00008000
#define BGF_GPS_DMA_DMA2_ACKINT_ACK_SHFT                       15

#define BGF_GPS_DMA_DMA2_RLCT_RLCT_ADDR                        BGF_GPS_DMA_DMA2_RLCT_ADDR
#define BGF_GPS_DMA_DMA2_RLCT_RLCT_MASK                        0x0000FFFF
#define BGF_GPS_DMA_DMA2_RLCT_RLCT_SHFT                        0

#define BGF_GPS_DMA_DMA2_PGMADDR_PGMADDR_ADDR                  BGF_GPS_DMA_DMA2_PGMADDR_ADDR
#define BGF_GPS_DMA_DMA2_PGMADDR_PGMADDR_MASK                  0xFFFFFFFF
#define BGF_GPS_DMA_DMA2_PGMADDR_PGMADDR_SHFT                  0

#define BGF_GPS_DMA_DMA3_WPPT_WPPT_ADDR                        BGF_GPS_DMA_DMA3_WPPT_ADDR
#define BGF_GPS_DMA_DMA3_WPPT_WPPT_MASK                        0x0000FFFF
#define BGF_GPS_DMA_DMA3_WPPT_WPPT_SHFT                        0

#define BGF_GPS_DMA_DMA3_WPTO_WPTO_ADDR                        BGF_GPS_DMA_DMA3_WPTO_ADDR
#define BGF_GPS_DMA_DMA3_WPTO_WPTO_MASK                        0xFFFFFFFF
#define BGF_GPS_DMA_DMA3_WPTO_WPTO_SHFT                        0

#define BGF_GPS_DMA_DMA3_COUNT_LEN_ADDR                        BGF_GPS_DMA_DMA3_COUNT_ADDR
#define BGF_GPS_DMA_DMA3_COUNT_LEN_MASK                        0x0000FFFF
#define BGF_GPS_DMA_DMA3_COUNT_LEN_SHFT                        0

#define BGF_GPS_DMA_DMA3_START_STR_ADDR                        BGF_GPS_DMA_DMA3_START_ADDR
#define BGF_GPS_DMA_DMA3_START_STR_MASK                        0x00008000
#define BGF_GPS_DMA_DMA3_START_STR_SHFT                        15

#define BGF_GPS_DMA_DMA3_INTSTA_INT_ADDR                       BGF_GPS_DMA_DMA3_INTSTA_ADDR
#define BGF_GPS_DMA_DMA3_INTSTA_INT_MASK                       0x00008000
#define BGF_GPS_DMA_DMA3_INTSTA_INT_SHFT                       15

#define BGF_GPS_DMA_DMA3_ACKINT_ACK_ADDR                       BGF_GPS_DMA_DMA3_ACKINT_ADDR
#define BGF_GPS_DMA_DMA3_ACKINT_ACK_MASK                       0x00008000
#define BGF_GPS_DMA_DMA3_ACKINT_ACK_SHFT                       15

#define BGF_GPS_DMA_DMA3_RLCT_RLCT_ADDR                        BGF_GPS_DMA_DMA3_RLCT_ADDR
#define BGF_GPS_DMA_DMA3_RLCT_RLCT_MASK                        0x0000FFFF
#define BGF_GPS_DMA_DMA3_RLCT_RLCT_SHFT                        0

#define BGF_GPS_DMA_DMA3_PGMADDR_PGMADDR_ADDR                  BGF_GPS_DMA_DMA3_PGMADDR_ADDR
#define BGF_GPS_DMA_DMA3_PGMADDR_PGMADDR_MASK                  0xFFFFFFFF
#define BGF_GPS_DMA_DMA3_PGMADDR_PGMADDR_SHFT                  0

#define BGF_GPS_DMA_DMA4_WPPT_WPPT_ADDR                        BGF_GPS_DMA_DMA4_WPPT_ADDR
#define BGF_GPS_DMA_DMA4_WPPT_WPPT_MASK                        0x0000FFFF
#define BGF_GPS_DMA_DMA4_WPPT_WPPT_SHFT                        0

#define BGF_GPS_DMA_DMA4_WPTO_WPTO_ADDR                        BGF_GPS_DMA_DMA4_WPTO_ADDR
#define BGF_GPS_DMA_DMA4_WPTO_WPTO_MASK                        0xFFFFFFFF
#define BGF_GPS_DMA_DMA4_WPTO_WPTO_SHFT                        0

#define BGF_GPS_DMA_DMA4_COUNT_LEN_ADDR                        BGF_GPS_DMA_DMA4_COUNT_ADDR
#define BGF_GPS_DMA_DMA4_COUNT_LEN_MASK                        0x0000FFFF
#define BGF_GPS_DMA_DMA4_COUNT_LEN_SHFT                        0

#define BGF_GPS_DMA_DMA4_START_STR_ADDR                        BGF_GPS_DMA_DMA4_START_ADDR
#define BGF_GPS_DMA_DMA4_START_STR_MASK                        0x00008000
#define BGF_GPS_DMA_DMA4_START_STR_SHFT                        15

#define BGF_GPS_DMA_DMA4_INTSTA_INT_ADDR                       BGF_GPS_DMA_DMA4_INTSTA_ADDR
#define BGF_GPS_DMA_DMA4_INTSTA_INT_MASK                       0x00008000
#define BGF_GPS_DMA_DMA4_INTSTA_INT_SHFT                       15

#define BGF_GPS_DMA_DMA4_ACKINT_ACK_ADDR                       BGF_GPS_DMA_DMA4_ACKINT_ADDR
#define BGF_GPS_DMA_DMA4_ACKINT_ACK_MASK                       0x00008000
#define BGF_GPS_DMA_DMA4_ACKINT_ACK_SHFT                       15

#define BGF_GPS_DMA_DMA4_RLCT_RLCT_ADDR                        BGF_GPS_DMA_DMA4_RLCT_ADDR
#define BGF_GPS_DMA_DMA4_RLCT_RLCT_MASK                        0x0000FFFF
#define BGF_GPS_DMA_DMA4_RLCT_RLCT_SHFT                        0

#define BGF_GPS_DMA_DMA4_PGMADDR_PGMADDR_ADDR                  BGF_GPS_DMA_DMA4_PGMADDR_ADDR
#define BGF_GPS_DMA_DMA4_PGMADDR_PGMADDR_MASK                  0xFFFFFFFF
#define BGF_GPS_DMA_DMA4_PGMADDR_PGMADDR_SHFT                  0

#endif /* __BGF_GPS_DMA_REGS_H__ */

