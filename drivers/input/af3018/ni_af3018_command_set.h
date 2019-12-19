/*
 * Reference Driver for NextInput Sensor
 *
 * The GPL Deliverables are provided to Licensee under the terms
 * of the GNU General Public License version 2 (the "GPL") and
 * any use of such GPL Deliverables shall comply with the terms
 * and conditions of the GPL. A copy of the GPL is available
 * in the license txt file accompanying the Deliverables and
 * at http://www.gnu.org/licenses/gpl.txt
 *
 * Copyright (C) NextInput, Inc.
 * Copyright (C) 2019 XiaoMi, Inc.
 * All rights reserved
 *
 * 1. Redistribution in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 * 2. Neither the name of NextInput nor the names of the contributors
 *    may be used to endorse or promote products derived from
 *    the software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES INCLUDING BUT
 * NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

/* Registers */

#define TOEN_REG                    0
#define TOEN_POS                    7
#define TOEN_MSK                    0b10000000

#define WAIT_REG                    0
#define WAIT_POS                    4
#define WAIT_MSK                    0b01110000

#define ADCRAW_REG                  0
#define ADCRAW_POS                  3
#define ADCRAW_MSK                  0b00001000

#define TEMPWAIT_REG                0
#define TEMPWAIT_POS                1
#define TEMPWAIT_MSK                0b00000110
#define OEM_NUM_SENSORS 1

#define EN_REG                      (OEM_NUM_SENSORS == 1 ? 0 : 1)
#define EN_POS                      0
#define EN_MSK                      0b00000001

#define INAGAIN_REG                 1
#define INAGAIN_POS                 4
#define INAGAIN_MSK                 0b01110000

#define PRECHARGE_REG               1
#define PRECHARGE_POS               (OEM_NUM_SENSORS == 1 ? 0 : 1)
#define PRECHARGE_MSK               (OEM_NUM_SENSORS == 1 ? 0b00000111 : 0b00001110)

#define OTPBUSY_REG                 (1 + OEM_NUM_SENSORS)
#define OTPBUSY_POS                 7
#define OTPBUSY_MSK                 0b10000000

#define OVRINTRTH_REG               (OEM_NUM_SENSORS == 1 ? 2 : 2 + OEM_NUM_SENSORS)
#define OVRINTRTH_POS               3
#define OVRINTRTH_MSK               0b00001000

#define OVRACALTH_REG               (OEM_NUM_SENSORS == 1 ? 2 : 2 + OEM_NUM_SENSORS)
#define OVRACALTH_POS               2
#define OVRACALTH_MSK               0b00000100

#define SNSERR_REG                  (OEM_NUM_SENSORS == 1 ? 2 : 2 + OEM_NUM_SENSORS)
#define SNSERR_POS                  1
#define SNSERR_MSK                  0b00000010

#define INTR_REG                    (OEM_NUM_SENSORS == 1 ? 2 : 2 + OEM_NUM_SENSORS)
#define INTR_POS                    0
#define INTR_MSK                    0b00000001

#define ADCOUT_REG                  (OEM_NUM_SENSORS == 1 ? 3 : 2 + 2 * OEM_NUM_SENSORS)
#define ADCOUT_SHIFT                4

#define SCOUNT_REG                  (OEM_NUM_SENSORS == 1 ? 4 : 3 + 2 * OEM_NUM_SENSORS)
#define SCOUNT_POS                  0
#define SCOUNT_MSK                  0b00001111

#define TEMP_REG                    (OEM_NUM_SENSORS == 1 ? 5 : 2 + 4 * OEM_NUM_SENSORS)
#define TEMP_SHIFT                  4

#define TCOUNT_REG                  (OEM_NUM_SENSORS == 1 ? 6 : 3 + 4 * OEM_NUM_SENSORS)
#define TCOUNT_POS                  0
#define TCOUNT_MSK                  0b00001111

#define AUTOCAL_REG                 (OEM_NUM_SENSORS == 1 ? 7 : 4 + 4 * OEM_NUM_SENSORS)
#define AUTOCAL_SHIFT               4

#if OEM_NUM_SENSORS != 1
#define CALMODE_REG                 (4 + 6 * OEM_NUM_SENSORS)
#define CALMODE_POS                 7
#define CALMODE_MSK                 0b10000000
#endif

#define CALRESET_REG                (OEM_NUM_SENSORS == 1 ? 9 : 4 + 6 * OEM_NUM_SENSORS)
#define CALRESET_POS                4
#define CALRESET_MSK                0b01110000

#define CALPERIOD_REG               (OEM_NUM_SENSORS == 1 ? 9 : 4 + 6 * OEM_NUM_SENSORS)
#define CALPERIOD_POS               0
#define CALPERIOD_MSK               0b00000111

#define RISEBLWGT_REG               (OEM_NUM_SENSORS == 1 ? 10 : 5 + 6 * OEM_NUM_SENSORS)
#define RISEBLWGT_POS               4
#define RISEBLWGT_MSK               0b01110000

#define LIFTDELAY_REG               (OEM_NUM_SENSORS == 1 ? 10 : 5 + 6 * OEM_NUM_SENSORS)
#define LIFTDELAY_POS               0
#define LIFTDELAY_MSK               0b00000111

#define PRELDADJ_REG                (OEM_NUM_SENSORS == 1 ? 11 : 5 + 4 * OEM_NUM_SENSORS)
#define PRELDADJ_POS                (OEM_NUM_SENSORS == 1 ? 3 : 0)
#define PRELDADJ_MSK                (OEM_NUM_SENSORS == 1 ? 0b11111000 : 0b00001111)

#define FALLBLWGT_REG               (OEM_NUM_SENSORS == 1 ? 11 : 6 + 6 * OEM_NUM_SENSORS)
#define FALLBLWGT_POS               0
#define FALLBLWGT_MSK               0b00000111

#define INTRTHRSLD_REG              (OEM_NUM_SENSORS == 1 ? 12 : 7 + 6 * OEM_NUM_SENSORS)
#define INTRTHRSLD_SHIFT            4

#define INTREN_REG                  (OEM_NUM_SENSORS == 1 ? 14 : 8 + 6 * OEM_NUM_SENSORS)
#define INTREN_POS                  (OEM_NUM_SENSORS == 1 ? 7 : 0)
#define INTREN_MSK                  (OEM_NUM_SENSORS == 1 ? 0b10000000 : 0b00000001)

#define INTRMODE_REG                (OEM_NUM_SENSORS == 1 ? 14 : 7 + 8 * OEM_NUM_SENSORS)
#define INTRMODE_POS                6
#define INTRMODE_MSK                0b01000000

#define INTRPERSIST_REG             (OEM_NUM_SENSORS == 1 ? 14 : 7 + 8 * OEM_NUM_SENSORS)
#define INTRPERSIST_POS             5
#define INTRPERSIST_MSK             0b00100000

#define INTRSAMPLES_REG             (OEM_NUM_SENSORS == 1 ? 14 : 7 + 8 * OEM_NUM_SENSORS)
#define INTRSAMPLES_POS             0
#define INTRSAMPLES_MSK             0b00000111

#define FORCEBL_REG                 (OEM_NUM_SENSORS == 1 ? 15 : 8 + 8 * OEM_NUM_SENSORS)
#define FORCEBL_POS                 0
#define FORCEBL_MSK                 0b11111111

#define ADCMAX_REG                  (OEM_NUM_SENSORS == 1 ? 16 : 9 + 8 * OEM_NUM_SENSORS)
#define ADCMAX_SHIFT                4

#define MCOUNT_REG                  (OEM_NUM_SENSORS == 1 ? 17 : 10 + 8 * OEM_NUM_SENSORS)
#define MCOUNT_POS                  0
#define MCOUNT_MSK                  0b00001111

#define BASELINE_REG                (OEM_NUM_SENSORS == 1 ? 18 : 9 + 10 * OEM_NUM_SENSORS)
#define BASELINE_SHIFT              4

#define ADCLIFO_REG                 (OEM_NUM_SENSORS == 1 ? 20 : 9 + 12 * OEM_NUM_SENSORS)
#define ADCLIFO_SHIFT               4

#define LCOUNT_REG                  (OEM_NUM_SENSORS == 1 ? 21 : 10 + 12 * OEM_NUM_SENSORS)
#define LCOUNT_POS                  0
#define LCOUNT_MSK                  0b00001111

#define DEVID_REG                   0x80
#define DEVID_POS                   3
#define DEVID_MSK                   0b11111000

#define REV_REG                     0x80
#define REV_POS                     0
#define REV_MSK                     0b00000111

#define FIRST_REG                   TOEN_REG
#define LAST_REG                    (LCOUNT_REG + 2 * (OEM_NUM_SENSORS - 1))

#define WAIT_0MS                    0
#define WAIT_1MS                    1
#define WAIT_4MS                    2
#define WAIT_8MS                    3
#define WAIT_16MS                   4
#define WAIT_32MS                   5
#define WAIT_64MS                   6
#define WAIT_256MS                  7

#define ADCRAW_BL                   0
#define ADCRAW_RAW                  1

#define TEMPWAIT_DISABLED           0
#define TEMPWAIT_32                 1
#define TEMPWAIT_64                 2
#define TEMPWAIT_128                3

#define TEMP_READ_TIME_MS           1

#define INAGAIN_1X                  0
#define INAGAIN_2X                  1
#define INAGAIN_4X                  2
#define INAGAIN_8X                  3
#define INAGAIN_16X                 4
#define INAGAIN_32X                 5
#define INAGAIN_64X                 6
#define INAGAIN_128X                7

#define PRECHARGE_50US              0
#define PRECHARGE_100US             1
#define PRECHARGE_200US             2
#define PRECHARGE_400US             3
#define PRECHARGE_800US             4
#define PRECHARGE_1600US            5
#define PRECHARGE_3200US            6
#define PRECHARGE_6400US            7

#define CALRESET_500MS              0
#define CALRESET_1000MS             1
#define CALRESET_2000MS             2
#define CALRESET_4000MS             3
#define CALRESET_8000MS             4
#define CALRESET_16000MS            5
#define CALRESET_32000MS            6
#define CALRESET_DISABLED           7

#define CALRESET_TIMER_STOPPED      0
#define CALRESET_TIMER_STARTED      1
#define CALRESET_TIMER_FINISHED     2

#define CALPERIOD_100MS             0
#define CALPERIOD_200MS             1
#define CALPERIOD_400MS             2
#define CALPERIOD_800MS             3
#define CALPERIOD_1600MS            4
#define CALPERIOD_3200MS            5
#define CALPERIOD_6400MS            6
#define CALPERIOD_DISABLED          7

#define CALPERIOD_TIMER_STOPPED     0
#define CALPERIOD_TIMER_STARTED     1
#define CALPERIOD_TIMER_FINISHED    2

#define RISEBLWGT_0X                0
#define RISEBLWGT_1X                1
#define RISEBLWGT_3X                2
#define RISEBLWGT_7X                3
#define RISEBLWGT_15X               4
#define RISEBLWGT_31X               5
#define RISEBLWGT_63X               6
#define RISEBLWGT_127X              7

#define LIFTDELAY_DISABLED          0
#define LIFTDELAY_20MS              1
#define LIFTDELAY_40MS              2
#define LIFTDELAY_80MS              3
#define LIFTDELAY_160MS             4
#define LIFTDELAY_320MS             5
#define LIFTDELAY_640MS             6
#define LIFTDELAY_1280MS            7

#define LIFTDELAY_TIMER_DISABLED    0
#define LIFTDELAY_TIMER_ENABLED     1
#define LIFTDELAY_TIMER_RUNNING     2

#define FALLBLWGT_0X                0
#define FALLBLWGT_1X                1
#define FALLBLWGT_3X                2
#define FALLBLWGT_7X                3
#define FALLBLWGT_15X               4
#define FALLBLWGT_31X               5
#define FALLBLWGT_63X               6
#define FALLBLWGT_127X              7

#define CALMODE_COUPLED             0
#define CALMODE_DECOUPLED           1

#define INTRMODE_THRESH             0
#define INTRMODE_DRDY               1

#define INTRPERSIST_PULSE           0
#define INTRPERSIST_INF             1

#define INT_DURATION                100

#define INTRSAMPLES_1               0
#define INTRSAMPLES_2               1
#define INTRSAMPLES_3               2
#define INTRSAMPLES_4               3
#define INTRSAMPLES_5               4
#define INTRSAMPLES_6               5
#define INTRSAMPLES_7               6
#define INTRSAMPLES_8               7

#ifdef NI_MCU
#ifdef HOST_DEBUGGER
#define NI_CMD_DEBUG_DUMP_TRACE         192
#define NI_CMD_DEBUG_SLEEP              193
#define NI_CMD_DEBUG_WAKE               194
#endif

#define NI_CMD_FW_VERSION               252
#define NI_CMD_DEBUG                    253
#define NI_CMD_RESET                    254
#define NI_CMD_ENTER_BOOTLOADER         255
#endif

