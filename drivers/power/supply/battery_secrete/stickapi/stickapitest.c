/**
  * attention
  *
  * <h2><center>&copy; COPYRIGHT 2020 STMicroelectronics</center></h2>
  *
  * STSAFE DRIVER SOFTWARE LICENSE AGREEMENT (SLA0088)
  *
  * BY INSTALLING, COPYING, DOWNLOADING, ACCESSING OR OTHERWISE USING THIS SOFTWARE
  * OR ANY PART THEREOF (AND THE RELATED DOCUMENTATION) FROM STMICROELECTRONICS
  * INTERNATIONAL N.V, SWISS BRANCH AND/OR ITS AFFILIATED COMPANIES (STMICROELECTRONICS),
  * THE RECIPIENT, ON BEHALF OF HIMSELF OR HERSELF, OR ON BEHALF OF ANY ENTITY BY WHICH
  * SUCH RECIPIENT IS EMPLOYED AND/OR ENGAGED AGREES TO BE BOUND BY THIS SOFTWARE LICENSE
  * AGREEMENT.
  *
  * Under STMicroelectronics? intellectual property rights, the redistribution,
  * reproduction and use in source and binary forms of the software or any part thereof,
  * with or without modification, are permitted provided that the following conditions
  * are met:
  * 1.  Redistribution of source code (modified or not) must retain any copyright notice,
  *     this list of conditions and the disclaimer set forth below as items 10 and 11.
  * 2.  Redistributions in binary form, except as embedded into a microcontroller or
  *     microprocessor device or a software update for such device, must reproduce any
  *     copyright notice provided with the binary code, this list of conditions, and the
  *     disclaimer set forth below as items 10 and 11, in documentation and/or other
  *     materials provided with the distribution.
  * 3.  Neither the name of STMicroelectronics nor the names of other contributors to this
  *     software may be used to endorse or promote products derived from this software or
  *     part thereof without specific written permission.
  * 4.  This software or any part thereof, including modifications and/or derivative works
  *     of this software, must be used and execute solely and exclusively in combination
  *     with a secure microcontroller device from STSAFE family manufactured by or for
  *     STMicroelectronics.
  * 5.  No use, reproduction or redistribution of this software partially or totally may be
  *     done in any manner that would subject this software to any Open Source Terms.
  *     ?Open Source Terms? shall mean any open source license which requires as part of
  *     distribution of software that the source code of such software is distributed
  *     therewith or otherwise made available, or open source license that substantially
  *     complies with the Open Source definition specified at www.opensource.org and any
  *     other comparable open source license such as for example GNU General Public
  *     License(GPL), Eclipse Public License (EPL), Apache Software License, BSD license
  *     or MIT license.
  * 6.  STMicroelectronics has no obligation to provide any maintenance, support or
  *     updates for the software.
  * 7.  The software is and will remain the exclusive property of STMicroelectronics and
  *     its licensors. The recipient will not take any action that jeopardizes
  *     STMicroelectronics and its licensors' proprietary rights or acquire any rights
  *     in the software, except the limited rights specified hereunder.
  * 8.  The recipient shall comply with all applicable laws and regulations affecting the
  *     use of the software or any part thereof including any applicable export control
  *     law or regulation.
  * 9.  Redistribution and use of this software or any part thereof other than as  permitted
  *     under this license is void and will automatically terminate your rights under this
  *     license.
  * 10. THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS" AND ANY
  *     EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  *     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
  *     OF THIRD PARTY INTELLECTUAL PROPERTY RIGHTS, WHICH ARE DISCLAIMED TO THE FULLEST
  *     EXTENT PERMITTED BY LAW. IN NO EVENT SHALL STMICROELECTRONICS OR CONTRIBUTORS BE
  *     LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  *     DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  *     LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  *     THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  *     NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
  *     ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  * 11. EXCEPT AS EXPRESSLY PERMITTED HEREUNDER, NO LICENSE OR OTHER RIGHTS, WHETHER EXPRESS
  *     OR IMPLIED, ARE GRANTED UNDER ANY PATENT OR OTHER INTELLECTUAL PROPERTY RIGHTS OF
  *     STMICROELECTRONICS OR ANY THIRD PARTY.
  ******************************************************************************
  */

#define DEBUG
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/random.h>

#include "stick1w.h"
#include "stick_api.h"

#define DRIVER_VERSION "0.0.3"

static int testnr = 0;

module_param(testnr, int, 0);
MODULE_PARM_DESC(testnr, "Index of the test to run");

void stickapitest_doTest_st1wire(void)
{
    struct stick_device *sd;
    int ret = 0;

    ret = stick_kernel_open(&sd, true);
    if (ret != 0)
    {
        pr_err("%s : Failed to open STICK 1W\n", __func__);
        return;
    }

    do
    {
        ssize_t r;
        uint8_t echocmd[] = {0x00, 0x17, 0x87, 0x2e, 0xfc, 0xf6};
        uint8_t echorsp[256];

        r = stick_send_frame(sd, echocmd, sizeof(echocmd));
        if (r != 0)
        {
            pr_err("%s : Failed to send frame\n", __func__);
            break;
        }

        msleep(6);

        r = stick_read_frame(sd, echorsp);
        if (r != sizeof(echocmd))
        {
            pr_err("%s : Received unexpected response : %zd\n", __func__, r);
            break;
        }

    } while (0);

    ret = stick_kernel_release(sd);
    if (ret != 0)
    {
        pr_err("%s : Failed to close STICK 1W\n", __func__);
        return;
    }
    pr_info("%s : Test ST1Wire successful~\n", __func__);
}

uint8_t rng_generate_random_number(void)
{
    uint8_t r;
    get_random_bytes(&r, sizeof(r));
    return r;
}

stick_decrement_option_t decrement_option;
stick_update_option_t update_option;
stick_read_option_t read_option;
uint8_t read_buffer[32];
uint8_t write_buffer[32] = {0};

const uint8_t password[16] = {0x0B, 0x52, 0x9E, 0x8C, 0xA7, 0x72, 0x62, 0x8C, 0x67, 0x8F, 0x1F, 0xD3, 0x3D, 0x8A, 0xED, 0xF2};

// by default forbidden to use session in kernel module since no NVM saving possibility.
// Call the following if it is OK to discard session data (i.e. using regen STICK)
extern void stickapi_allow_sessions(void);

void stickapitest_doTest_echo(int loops)
{
    stick_Handler_t *stick_handler;
    stick_ReturnCode_t stick_ret;
    uint32_t lib_version;

    /* - Init STICK handler */
    stick_handler = kzalloc(stick_get_handler_size(), GFP_KERNEL);
    if (stick_handler == NULL)
    {
        pr_err("%s : ### ERROR no memory available\n", __func__);
        return;
    }

    // If the error displayed by below call when no STICK is connected is an issue,
    // it is possible to test the existence of /dev/stick here firstly.
    stick_ret = stick_init(stick_handler);
    if (stick_ret != STICK_OK)
    {
        kfree(stick_handler);
        pr_err("%s : ### ERROR during stick initialization : %x\n", __func__, stick_ret);
        pr_err("%s : Note: this error is normal if no STICK device is connected\n", __func__);
        return;
    }

    {
        lib_version = stick_get_lib_version();
        pr_info("%s : Lib version : %02X.%02X.%02X\n", __func__,
                (unsigned int)(lib_version >> 24) & 0xFF,
                (unsigned int)(lib_version >> 16) & 0xFF,
                (unsigned int)(lib_version >> 8) & 0xFF);
    }

    while (loops-- > 0)
    {
        {
            uint8_t cmdpayload[] = { 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xCC, 0x55, 0xC5, 0x5C };
            uint8_t rsppayload[sizeof(cmdpayload)];

            stick_ret = stick_echo(stick_handler, cmdpayload, rsppayload, sizeof(cmdpayload));
            if ((stick_ret != STICK_OK) || memcmp(cmdpayload, rsppayload, sizeof(cmdpayload))) {
                pr_info("%s : FAILED: %d\n", __func__, stick_ret);
                break;
            };
        }
        {
            uint8_t cmdpayload[252]; // max length for ECHO payload
            uint8_t rsppayload[sizeof(cmdpayload)];
            int i;

            memset(cmdpayload, 0, sizeof(cmdpayload));
            for (i = 1; (i < 80) && (stick_ret == STICK_OK); i++) {
                memset(rsppayload, 0xCA, sizeof(rsppayload));
                stick_ret = stick_echo(stick_handler, cmdpayload, rsppayload, i <= sizeof(cmdpayload) ? i : sizeof(cmdpayload));
                if (stick_ret == STICK_OK) {
                    int cmp = memcmp(cmdpayload, rsppayload, i <= sizeof(cmdpayload) ? i : sizeof(cmdpayload));
                    if (cmp)
                        stick_ret = -1;
                }
            }
            if (stick_ret != STICK_OK) {
                pr_info("%s : FAILED: %d\n", __func__, stick_ret);
                break;
            }

            memset(cmdpayload, 0xFF, sizeof(cmdpayload));
            for (i = 1; (i < 80) && (stick_ret == STICK_OK); i++) {
                memset(rsppayload, 0xCA, sizeof(rsppayload));
                stick_ret = stick_echo(stick_handler, cmdpayload, rsppayload, i <= sizeof(cmdpayload) ? i : sizeof(cmdpayload));
                if (stick_ret == STICK_OK) {
                    int cmp = memcmp(cmdpayload, rsppayload, i <= sizeof(cmdpayload) ? i : sizeof(cmdpayload));
                    if (cmp)
                        stick_ret = -1;
                }
            }
            if (stick_ret != STICK_OK) {
                pr_info("%s : FAILED: %d\n", __func__, stick_ret);
                break;
            }

        }
    }
    pr_info("%s : function ended, remaining loops: %d\n", __func__, loops);

    kfree(stick_handler);
}

void stickapitest_doTest_traceability(void) {
    stick_Handler_t *stick_handler;
    stick_traceability_data_t traceability = {
      .product_type = { 0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE },
      .cpsn = { 0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA },
    };
    stick_ReturnCode_t sr;

    /* - Init STICK handler */
    stick_handler = kzalloc(stick_get_handler_size(), GFP_KERNEL);
    if (stick_handler == NULL)
    {
        pr_err("%s : ### ERROR no memory available\n", __func__);
        return;
    }

    // If the error displayed by below call when no STICK is connected is an issue,
    // it is possible to test the existence of /dev/stick here firstly.
    sr = stick_init(stick_handler);
    if (sr != STICK_OK)
    {
        kfree(stick_handler);
        pr_err("%s : ### ERROR during stick initialization : %x\n", __func__, sr);
        pr_err("%s : Note: this error is normal if no STICK device is connected\n", __func__);
        return;
    }

    sr = stick_get_traceability(stick_handler, &traceability );
    if (sr == STICK_OK) {
        char pt[sizeof(traceability.product_type) * 3 + 1];
        char cpsn[sizeof(traceability.cpsn) * 3 + 1];
        int i;
        for (i = 0; i < sizeof(traceability.product_type); i++)
            sprintf(pt + (3 * i), "%02X ", traceability.product_type[i]);
        for (i = 0; i < sizeof(traceability.cpsn); i++)
            sprintf(cpsn + (3 * i), "%02X ", traceability.cpsn[i]);
        pr_info("%s : Product type: %s\n", __func__, pt);
        pr_info("%s : CSPN: %s\n", __func__, cpsn);
    } else {
      pr_info("%s FAILED: %d\n", __func__, sr);
    }

    kfree(stick_handler);
}

void stickapitest_doTest_api(void)
{
    stick_Handler_t *stick_handler;
    stick_ReturnCode_t stick_ret;
    uint32_t lib_version;
    uint32_t counter_val = 0;
    uint16_t rsp_length;
/* N17 code for HQ-299575 by tongjiacheng at 20230616 start */
    uint16_t i;
/* N17 code for HQ-299575 by tongjiacheng at 20230616 end */

    int loop = 3;

    /* - Init STICK handler */
    stick_handler = kzalloc(stick_get_handler_size(), GFP_KERNEL);
    if (stick_handler == NULL)
    {
        pr_err("%s : ### ERROR no memory available\n", __func__);
        return;
    }

    // If the error displayed by below call when no STICK is connected is an issue,
    // it is possible to test the existence of /dev/stick here firstly.
    stick_ret = stick_init(stick_handler);
    if (stick_ret != STICK_OK)
    {
        kfree(stick_handler);
        pr_err("%s : ### ERROR during stick initialization : %x\n", __func__, stick_ret);
        pr_err("%s : Note: this error is normal if no STICK device is connected\n", __func__);
        return;
    }

    {
        stick_traceability_data_t traceability = {
            .product_type = {0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE},
            .cpsn = {0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA},
        };
        stick_ReturnCode_t sr;

        sr = stick_get_traceability(stick_handler, &traceability);
        if (sr == STICK_OK)
        {
            char pt[17];
            char cpsn[15];
            snprintf(pt, sizeof(pt), "%02X%02X%02X%02X%02X%02X%02X%02X",
                     traceability.product_type[0],
                     traceability.product_type[1],
                     traceability.product_type[2],
                     traceability.product_type[3],
                     traceability.product_type[4],
                     traceability.product_type[5],
                     traceability.product_type[6],
                     traceability.product_type[7]);
            snprintf(cpsn, sizeof(cpsn), "%02X%02X%02X%02X%02X%02X%02X",
                     traceability.cpsn[0],
                     traceability.cpsn[1],
                     traceability.cpsn[2],
                     traceability.cpsn[3],
                     traceability.cpsn[4],
                     traceability.cpsn[5],
                     traceability.cpsn[6]);
            pr_info("%s : SUCCESS Product type: %s CSPN %s\n", __func__, pt, cpsn);
        }
        else
        {
            pr_err("%s : ### failed\n", __func__);
            return;
        }
    }

    pr_info("%s : Simple test API successful, going to full API test~\n", __func__);

    stickapi_allow_sessions();

    {
        lib_version = stick_get_lib_version();
        pr_info("%s : Lib version : %02X.%02X.%02X\n", __func__,
                (unsigned int)(lib_version >> 24) & 0xFF,
                (unsigned int)(lib_version >> 16) & 0xFF,
                (unsigned int)(lib_version >> 8) & 0xFF);
    }

    // pr_info("%s : ### ADMIN APIs\n", __func__);
    // pr_info("%s : - stick_Remove_Information \n", __func__);
    // stick_ret = stick_remove_information(stick_handler);
    // if (stick_ret != STICK_OK)
    // {
    //     pr_info("%s : [ERROR : 0x%02X]\n", __func__, stick_ret);
    //     return;
    // }
    // else
    // {
    //     pr_info("%s : [PASS]\n", __func__);
    // }
    // pr_info("%s : stick_Regenerate (take up to 20 seconds)\n", __func__);
    // stick_ret = stick_regenerate(stick_handler, (uint8_t *)password);
    // if (stick_ret != STICK_OK)
    // {
    //     pr_info("%s : [ERROR : 0x%02X]\n", __func__, stick_ret);
    //     return;
    // }
    // else
    // {
    //     pr_info("%s : [PASS]\n", __func__);
    // }

    while (loop--)
    {
        pr_info("%s : ### AUTHENTICATED USAGE MONITORING APIs - Remaining loops %d\n", __func__, loop);
        /* - OPEN SESSION API TEST */
        pr_info("%s : - stick_Open_Session \n", __func__);
        stick_ret = stick_open_session(stick_handler);
        if (stick_ret != STICK_OK)
        {
            pr_info("%s : [ERROR : 0x%02X]\n", __func__, stick_ret);
            return;
        }
        else
        {
            pr_info("%s : [PASS]\n", __func__);
        }
        pr_info("%s : - stick_Authenticate \n", __func__);
        stick_ret = stick_authenticate(stick_handler);
        if (stick_ret != STICK_OK)
        {
            pr_info("%s : [ERROR : 0x%02X]\n", __func__, stick_ret);
            return;
        }
        else
        {
            pr_info("%s : [PASS]\n", __func__);
        }

        /* - PROTECTED DECREMENT API TEST */
        pr_info("%s : - stick_Protected_Decrement_Counter\n", __func__);
        /* Decrement by 1 the counter from zone 5 */
        decrement_option.filler = 0;
        decrement_option.change_ac_indicator = 0;
        decrement_option.new_update_ac_change_right = 0;
        decrement_option.new_update_ac = 0;
        stick_ret = stick_decrement_counter(stick_handler,    // target STICK Handler
                                            1,                // Protected Decrement
                                            5,                // Zone index
                                            decrement_option, // Option
                                            1,                // Amount
                                            0,                // Offset
                                            0,                // Pointer to data buffer
                                            0,                // Data length
                                            &counter_val      // Pointer to new counter value
        );
        if (stick_ret != STICK_OK)
        {
            pr_info("%s : [ERROR : 0x%02X]\n", __func__, stick_ret);
            return;
        }
        else
        {
            pr_info("%s : [PASS : 0x%06x]\n", __func__, (unsigned int)counter_val);
        }

        /* - Prepare a random Update test buffer */
        for (i = 0; i < 8; i++)
        {
            write_buffer[i] = rng_generate_random_number();
        }

        /* - PROTECTED ZONE UPDATE API TEST */
        pr_info("%s : - stick_Protected_Update_Zone\n", __func__);

        update_option.filler = 0;
        update_option.atomicity = 0;
        update_option.change_ac_indicator = 0;
        update_option.new_update_ac_change_right = 0;
        update_option.new_update_ac = 0;
        stick_ret = stick_update_zone(stick_handler, // Target STICK-A10x Handler
                                      1,             // Session protected
                                      1,             // Zone index
                                      update_option, // Option
                                      0x00,          // Offset
                                      write_buffer,  // pointer to Data/Data-buffer to be written
                                      8              // Data length

        );

        if (stick_ret != STICK_OK)
        {
            pr_info("%s : [ERROR : 0x%02X]\n", __func__, stick_ret);
            return;
        }
        else
        {
            pr_info("%s : [PASS : ", __func__);
            for (i = 0; i < 8; i++)
            {
                pr_info("%s : 0x%02x ", __func__, write_buffer[i]);
            }
            pr_info("%s : ]\n", __func__);
        }

        /* - PROTECTED READ API TEST */
        pr_info("%s : - stick_Protected_Read_Zone\n", __func__);
        read_option.filler = 0;
        read_option.change_ac_indicator = 0;
        read_option.new_read_ac_change_right = 0;
        read_option.new_read_ac = 0;
        stick_ret = stick_read_zone(stick_handler, // Target STICK-A10x Handler
                                    1,             // Session Protected
                                    1,             // Zone index
                                    read_option,   // Option
                                    0x00,          // Offset
                                    read_buffer,   // pointer to Read Data/Data-buffer
                                    8,             // Data length
                                    &rsp_length    // STICK Response data length
        );
        if (stick_ret != STICK_OK)
        {
            pr_info("%s : [ERROR : 0x%02X]\n", __func__, stick_ret);
            return;
        }
        else
        {
            pr_info("%s : [PASS : ", __func__);
            for (i = 0; i < rsp_length; i++)
            {
                pr_info(" 0x%02x ", read_buffer[i]);
            }
            pr_info("]\n");
        }

        /* - CLOSE SESSION API TEST */
        pr_info("%s : - stick_Close_Session \n[PASS]\n", __func__);
        stick_close_session(stick_handler);

        pr_info("%s : ### USAGE MONITORING APIs\n", __func__);

        /* - DECREMENT API TEST */
        pr_info("%s : - stick_Decrement_Counter \n", __func__);
        /* Decrement by 1 the counter from zone 5 */
        decrement_option.filler = 0;
        decrement_option.change_ac_indicator = 0;
        decrement_option.new_update_ac_change_right = 0;
        decrement_option.new_update_ac = 0;
        stick_ret = stick_decrement_counter(stick_handler,    // target STICK Handler
                                            0,                // Not Protected
                                            5,                // Zone index
                                            decrement_option, // Option
                                            1,                // Amount
                                            0,                // Offset
                                            0,                // Pointer to data buffer
                                            0,                // Data length
                                            &counter_val      // Pointer to new counter value
        );
        if (stick_ret != STICK_OK)
        {
            pr_info("%s : [ERROR : 0x%02X]\n", __func__, stick_ret);
            return;
        }
        else
        {
            pr_info("%s : [PASS : ", __func__);
            pr_info(" 0x%06x", (unsigned int)counter_val);
            pr_info("]\n");
        }

        /* - Prepare a random Update test buffer */
        for (i = 0; i < 8; i++)
        {
            write_buffer[i] = rng_generate_random_number();
        }

        /* - UPDATE ZONE API TEST */
        pr_info("%s : - stick_Update_Zone\n", __func__);
        update_option.filler = 0;
        update_option.atomicity = 0;
        update_option.change_ac_indicator = 0;
        update_option.new_update_ac_change_right = 0;
        update_option.new_update_ac = 0;
        stick_ret = stick_update_zone(stick_handler, // Target STICK-A10x Handler
                                      0,             // Not protected
                                      0x01,          // Zone index
                                      update_option, // Option
                                      0x00,          // Offset
                                      write_buffer,  // pointer to Data/Data-buffer to be written
                                      8              // Data length

        );
        if (stick_ret != STICK_OK)
        {
            pr_info("%s : [ERROR : 0x%02X]\n", __func__, stick_ret);
            return;
        }
        else
        {
            pr_info("%s : [PASS : ", __func__);
            for (i = 0; i < 8; i++)
            {
                pr_info(" 0x%02x ", write_buffer[i]);
            }
            pr_info(" ]\n");
        }

        /* - READ ZONE API TEST 2 */
        pr_info("%s : - stick_Read_Zone\n", __func__);
        read_option.filler = 0;
        read_option.change_ac_indicator = 0;
        read_option.new_read_ac_change_right = 0;
        read_option.new_read_ac = 0;
        stick_ret = stick_read_zone(stick_handler, // Target STICK-A10x Handler
                                    0,             // Not Protected
                                    0x01,             // Zone index
                                    read_option,   // Option
                                    0x00,          // Offset
                                    read_buffer,   // pointer to Read Data/Data-buffer
                                    8,             // Data length
                                    &rsp_length    // STICK Response data length
        );
        if (stick_ret != STICK_OK)
        {
            pr_info("%s : [ERROR : 0x%02X]\n", __func__, stick_ret);
            return;
        }
        else
        {
            pr_info("%s : [PASS : ", __func__);
            for (i = 0; i < rsp_length; i++)
            {
                pr_info(" 0x%02x ", read_buffer[i]);
            }
            pr_info(" ]\n");
        }
    }

    pr_info("%s : All loops PASSED !\n", __func__);
}

int stickapitest_doTest_EDDSA_onetime(void)
{
    stick_Handler_t *stick_handler;
    stick_ReturnCode_t stick_ret;
    struct timespec64 tv1, tv2;
    long ms;

    pr_info("%s : Starting EDDSA authentication \n", __func__);
    ktime_get_real_ts64(&tv1);

    /* - Init STICK handler */
    stick_handler = kzalloc(stick_get_handler_size(), GFP_KERNEL);
    if (stick_handler == NULL)
    {
        pr_err("%s : ### ERROR no memory available\n", __func__);
        return STICK_HANDLER_NOT_INITIALISED;
    }

    // If the error displayed by below call when no STICK is connected is an issue,
    // it is possible to test the existence of /dev/stick here firstly.
    stick_ret = stick_init(stick_handler);
    if (stick_ret != STICK_OK)
    {
        kfree(stick_handler);
        pr_err("%s : ### ERROR during stick initialization : %x\n", __func__, stick_ret);
        pr_err("%s : Note: this error is normal if no STICK device is connected\n", __func__);
        return stick_ret;
    }


    //while (loop--)
    {
	   ktime_get_real_ts64(&tv1);
       stick_ret = stick_eddsa_authenticate(stick_handler,14);
	   ktime_get_real_ts64(&tv2);
       ms = (tv2.tv_sec - tv1.tv_sec) * 1000 + (tv2.tv_nsec - tv1.tv_nsec) / 1000000;

	   pr_info("%s: EDDSA auth finish within %ldms (%s)\n", __func__, ms, stick_ret == STICK_OK ? "success":"failed");
    }

    /* N17 code for HQ-297568 by tongjiacheng at 20230601 start */
    stick_ret = stick_hibernate(stick_handler);
    if (stick_ret != STICK_OK)
        pr_err("%s set hibernate fail(%d)\n", __func__, stick_ret);
    /* N17 code for HQ-297568 by tongjiacheng at 20230601 end */

    kfree(stick_handler);

    return stick_ret;
}

void stickapitest_doTest_EDDSA(void)
{
    stick_Handler_t *stick_handler;
    stick_ReturnCode_t stick_ret;
    uint32_t lib_version;
    int loop = 5000;
    struct timespec64 tv1, tv2;
    long ms;

    pr_info("%s : Starting tests %d\n", __func__, testnr);
    ktime_get_real_ts64(&tv1);

    /* - Init STICK handler */
    stick_handler = kzalloc(stick_get_handler_size(), GFP_KERNEL);
    if (stick_handler == NULL)
    {
        pr_err("%s : ### ERROR no memory available\n", __func__);
        return;
    }

    // If the error displayed by below call when no STICK is connected is an issue,
    // it is possible to test the existence of /dev/stick here firstly.
    stick_ret = stick_init(stick_handler);
    if (stick_ret != STICK_OK)
    {
        kfree(stick_handler);
        pr_err("%s : ### ERROR during stick initialization : %x\n", __func__, stick_ret);
        pr_err("%s : Note: this error is normal if no STICK device is connected\n", __func__);
        return;
    }

    {
        stick_traceability_data_t traceability = {
            .product_type = {0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE},
            .cpsn = {0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA},
        };
        stick_ReturnCode_t sr;

        sr = stick_get_traceability(stick_handler, &traceability);
        if (sr == STICK_OK)
        {
            char pt[sizeof(traceability.product_type) * 3 + 1];
            char cpsn[sizeof(traceability.cpsn) * 3 + 1];
            int i;
            for (i = 0; i < sizeof(traceability.product_type); i++)
                sprintf(pt + (3 * i), "%02X ", traceability.product_type[i]);
            for (i = 0; i < sizeof(traceability.cpsn); i++)
                sprintf(cpsn + (3 * i), "%02X ", traceability.cpsn[i]);
            pr_info("%s : SUCCESS Product type: %s CSPN %s\n", __func__, pt, cpsn);
        }
        else
        {
            pr_err("%s : ### failed\n", __func__);
            kfree(stick_handler);
            return;
        }
    }

    pr_info("%s : Simple test API successful, going to Authentication api\n", __func__);

    {
        lib_version = stick_get_lib_version();
        pr_info("%s : Lib version : %02X.%02X.%02X\n", __func__,
                (unsigned int)(lib_version >> 24) & 0xFF,
                (unsigned int)(lib_version >> 16) & 0xFF,
                (unsigned int)(lib_version >> 8) & 0xFF);
    }

    while (loop--)
    {
	   ktime_get_real_ts64(&tv1);
       stick_ret = stick_eddsa_authenticate(stick_handler,14);
	   ktime_get_real_ts64(&tv2);
       ms = (tv2.tv_sec - tv1.tv_sec) * 1000 + (tv2.tv_nsec - tv1.tv_nsec) / 1000000;

	   pr_info("%s: EDDSA auth %ldms (%s)\n", __func__, ms, stick_ret == STICK_OK ? "success":"failed");
    }
    kfree(stick_handler);
}

void stickapitest_doTest_sequence(void)
{
    stick_Handler_t *stick_handler;
    stick_ReturnCode_t stick_ret;
    int loops = 3, loop;

    pr_info("%s : Starting tests %d\n", __func__, testnr);

    /* - Init STICK handler */
    stick_handler = kzalloc(stick_get_handler_size(), GFP_KERNEL);
    if (stick_handler == NULL)
    {
        pr_err("%s : ### ERROR no memory available\n", __func__);
        return;
    }

    // If the error displayed by below call when no STICK is connected is an issue,
    // it is possible to test the existence of /dev/stick here firstly.
    stick_ret = stick_init(stick_handler);
    if (stick_ret != STICK_OK)
    {
        kfree(stick_handler);
        pr_err("%s : ### ERROR during stick initialization : %x\n", __func__, stick_ret);
        pr_err("%s : Note: this error is normal if no STICK device is connected\n", __func__);
        return;
    }

    {
        int i;

        uint32_t counter_val = 0;
        uint16_t rsp_length;
        //uint8_t zone1_content[8] = {0};

        pr_info("--------------------------------------------------\n");
        pr_info("-             STICKDroid DEMO                    -\n");
        pr_info("--------------------------------------------------\n");

        stick_hibernate(stick_handler);

        for (loop = 0; loop < loops; loop ++)
        {
            pr_info("## Wake-up STICK device (loop:%d/%d) \n", loop, loops);
            stick_wakeup(stick_handler);

            stick_ret = stick_eddsa_authenticate(stick_handler,14);
            if (stick_ret == STICK_OK)
            {
                pr_info("Authenticate succeed!\n");
            }
            else
            {
                pr_err("> ERROR : Authentication failed with error code 0x%02X , in loop : %d \n", stick_ret, loop);
                break;
            }

            /* - DECREMENT API call example */
            pr_info("## Decrement by 1 counter from STICK Zone 5\n");
            /* Decrement by 1 the counter from zone 5 */
            decrement_option.filler = 0;
            decrement_option.change_ac_indicator = 0;
            decrement_option.new_update_ac_change_right = 0;
            decrement_option.new_update_ac = 0;
            stick_ret = stick_decrement_counter(stick_handler,    // target STICK Handler
                                                0,                // Not Protected
                                                5,                // Zone index
                                                decrement_option, // Option
                                                1,                // Amount
                                                0,                // Offset
                                                0,                // Pointer to data buffer
                                                0,                // Data length
                                                &counter_val      // Pointer to new counter value
            );
            if (stick_ret != STICK_OK)
            {
                pr_err("> ERROR : 0x%02X , Loop : %d \n", stick_ret, loop);
                break;
            }
            else
            {
                pr_info("> SUCCESS : New Zone 5 counter value : 0x%06X\n", (unsigned int)counter_val);
            }

            pr_info("## Decrement by 1 counter from STICK Zone 5 \n");
            /* Decrement by 1 the counter from zone 5 */
            decrement_option.filler = 0;
            decrement_option.change_ac_indicator = 0;
            decrement_option.new_update_ac_change_right = 0;
            decrement_option.new_update_ac = 0;
            stick_ret = stick_decrement_counter(stick_handler,    // target STICK Handler
                                                0,                // Not Protected
                                                5,                // Zone index
                                                decrement_option, // Option
                                                1,                // Amount
                                                0,                // Offset
                                                0,                // Pointer to data buffer
                                                0,                // Data length
                                                &counter_val      // Pointer to new counter value
            );
            if (stick_ret != STICK_OK)
            {
                pr_err("> ERROR : 0x%02X , Loop : %d \n", stick_ret, loop);
                break;
            }
            else
            {
                pr_info("> SUCCESS : New Zone 5 counter value : 0x%06X", (unsigned int)counter_val);
            }

            /* - UPDATE ZONE API TEST */
            /* - Prepare a random Update test buffer */
            {
                char buf[32 * 3 + 2];
                for (i = 0; i < 32; i++) {
                    write_buffer[i] = rng_generate_random_number();
                    sprintf(&buf[i * 3], "%02X ", write_buffer[i]);
                }
                pr_info("## Update STICK Zone 20 with data : %s\n", buf);
            }

            /* - Prepare and use STICK update API*/
            update_option.filler = 0;
            update_option.atomicity = 0;
            update_option.change_ac_indicator = 0;
            update_option.new_update_ac_change_right = 0;
            update_option.new_update_ac = 0;
            stick_ret = stick_update_zone(stick_handler, // Target STICK-A10x Handler
                                          0,             // Not protected
                                          20,            // Zone index
                                          update_option, // Option
                                          0x00,          // Offset
                                          write_buffer,  // pointer to Data/Data-buffer to be written
                                          32             // Data length

            );
            if (stick_ret != STICK_OK)
            {
                pr_err("> ERROR : 0x%02X , Loop : %d \n", stick_ret, loop);
                break;
            }
            else
            {
                pr_info("> SUCCESS \n");
            }

            /* - READ ZONE API TEST 2 */
            /* - Prepare and use STICK Read API*/
            pr_info("## Read 32 bytes from STICK Zone 20 \n");
            read_option.filler = 0;
            read_option.change_ac_indicator = 0;
            read_option.new_read_ac_change_right = 0;
            read_option.new_read_ac = 0;
            stick_ret = stick_read_zone(stick_handler, // Target STICK-A10x Handler
                                        0,             // Not Protected
                                        20,            // Zone index
                                        read_option,   // Option
                                        0x00,          // Offset
                                        read_buffer,   // pointer to Read Data/Data-buffer
                                        32,            // Data length
                                        &rsp_length    // STICK Response data length
            );
            if (stick_ret != STICK_OK)
            {
                pr_err("> ERROR : 0x%02X , Loop : %d \n", stick_ret, loop);
                break;
            }
            else if (rsp_length != 32)
            {
                pr_err("> ERROR : read %d/32 , Loop : %d \n", rsp_length, loop);
                break;
            }
            else if (memcmp(write_buffer, read_buffer, 32))
            {
                pr_err("> ERROR : read data mismatch written data , Loop : %d \n", loop);
                break;
            }
            else
            {
                char buf[32 * 3 + 2];
                for (i = 0; i < 32; i++) {
                    sprintf(&buf[i * 3], "%02X ", read_buffer[i]);
                }
                pr_info("> SUCCESS : Zone 20 content : %s\n", buf);
            }
            pr_info("## Enter Hibernate mode for 2 sec\n");
            stick_hibernate(stick_handler);
            msleep(2000);
        }
    }
    pr_info("Test ended (loop %d/%d)\n", loop, loops);
    kfree(stick_handler);
}

/* module load/unload record keeping */
int stickapitest_init(int testnr)
{
   // struct timespec64 tv1, tv2;
   //long ms;
    int ret = STICK_INVALID_PARAMETER;

   // pr_info("%s : Starting tests %d\n", __func__);
   // ktime_get_real_ts64(&tv1);
    //testnr=6;
    switch (testnr) {
        case 0:
        stickapitest_doTest_echo(1);
        break;
        case 1:
        stickapitest_doTest_echo(100);
        break;
        case 2:
        stickapitest_doTest_traceability();
        break;
        case 3:
        stickapitest_doTest_EDDSA();
        break;
        case 4:
        stickapitest_doTest_sequence();
        break;
	case 6:
	ret = stickapitest_doTest_EDDSA_onetime();
	break;
        case 9:
        stickapitest_doTest_st1wire();
        stickapitest_doTest_api();
        break;
        default:
        pr_info("%s : Test not supported %d\n", __func__, testnr);
    }
    /*ktime_get_real_ts64(&tv2);
    ms = (tv2.tv_sec - tv1.tv_sec) * 1000 + (tv2.tv_nsec - tv1.tv_nsec) / 1000000;

    pr_info("%s : terminates, run time %ldms\n", __func__, ms);
    */  
    return ret;
}
EXPORT_SYMBOL(stickapitest_init);
