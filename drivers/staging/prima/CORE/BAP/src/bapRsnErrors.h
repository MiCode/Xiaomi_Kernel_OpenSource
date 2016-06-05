/*
 * Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/*
 * File: $File: //depot/software/projects/feature_branches/gen5_phase1/os/linux/classic/ap/apps/include/aniErrors.h $
 *
 * Contains definitions of error codes that are globally visible
 * across WNI applications. There are two kinds of error codes:
 * generic error codes and module specific error codes. Module specific
 * error codes can further be of two kinds: those that are used
 * internally within sub-modules, and those that are made visible
 * to other modules. Each module will be pre-allocated a range of
 * negative integers from which to choose both internal and external
 * error codes uniquely. Note that internally used error codes need
 * not be defined in this file so long as they adhere to the allocated
 * range.
 *
 * Author:      Mayank D. Upadhyay
 * Date:        17-June-2002
 * History:-
 * Date         Modified by     Modification Information
 * ------------------------------------------------------
 *
 */

#ifndef _ANI_ERRORS_H_
#define _ANI_ERRORS_H_

/**
 * Indicates that the function returned successfully and no error was
 * reported.
 */
#define ANI_OK      0
#define ANI_ERROR   ANI_E_FAILED

/**
 * The range -1 to -1024 is reserved for generic error codes that are
 * applicable to all modules.
 */

#define ANI_E_FAILED        -1 /// Generic error code for failure
#define ANI_E_MALLOC_FAILED -2 /// Mem allocation failed
#define ANI_E_ILLEGAL_ARG   -3 /// Function argument is illegal
#define ANI_E_NULL_VALUE    -4 /// Encountered unexpected NULL value
#define ANI_E_RAND_FAILED   -5 /// RNG failed
#define ANI_E_SHORT_PACKET  -6 /// Packet is too small to contain data
#define ANI_E_ELEM_NOT_FND  -7 /// Element not found
#define ANI_E_INVALID_LEN   -8 /// Element has invalid Length 
#define ANI_E_INVALID_DT    -9 /// Invalid Data Type 
#define ANI_E_TIMEOUT       -10 /// Timeout occurred
#define ANI_E_DBM_INVALID_ID    -11 /// Invalid DB id sent to server
#define ANI_E_DBM_KEY_NOT_FOUND -12 /// Key's mapping was not found
#define ANI_E_FILE_EMPTY    -13 /// file empty
#define ANI_E_INVALID_MT    -14 /// Invalid ANI message type
#define ANI_E_NOT_IMPLEMENTED   -15 /// Feature not implemented
#define ANI_E_INVALID_PT    -16 /// Invalid Parameter Type 
#define ANI_E_INVALID_PV    -17 /// Invalid Paramter Value
#define ANI_E_IPCOPEN    -18 /// IPC open failed
#define ANI_E_IPCCONNECT    -19 /// IPC connect failed
#define ANI_E_IPCSEND    -20 /// IPC send failed
#define ANI_E_FILE_NOT_FOUND    -21 /// file not found
#define ANI_E_FILE_INVALID_CONTENT    -22 /// invalid file content
#define ANI_E_FILE_READ_FAILED    -23 /// file read failed

// ***** SSM libraries and applications use the range -1025 to -2999 *****

/*
 * The range -1025 to -2048 is reserved for use by the
 * authentication agent and its sub-modules. The sub-range -1029 to
 * -1150 is reserved for CLI usage.
 */
#define ANI_E_RANGE_START_AAG                   -1025

// CLI Range starts here...
#define ANI_E_RANGE_START_AAG_CLI               -1029
#define ANI_E_CLI_ARG_MISSING                   -1029 // CLI command param missing
#define ANI_E_CLI_ARG_INVALID                   -1030 // CLI command param invalid
#define ANI_E_CLI_PAM_RADIUS_DB_UPDATE_FAILURE  -1031 // Failure to update PAM RADIUS DB
#define ANI_E_CLI_AUTH_SERVER_NOT_FOUND         -1032 // Auth server not found
#define ANI_E_CLI_SYS_INTERNAL_ERROR_PACKET_NULL -1033 // Internal error
#define ANI_E_CLI_INVALID_DEVICE                -1034 // Invalid device
#define ANI_E_CLI_AUTH_SERVER_INVALID_IPADDR    -1035 // Invalid IP addr
#define ANI_E_CLI_AUTH_ZONE_NOT_EMPTY           -1036 // zone not empty
#define ANI_E_CLI_AUTH_ZONE_AUTH_SERVERS_NOT_COPIED -1037 // zone not copied
#define ANI_E_CLI_AUTH_ZONE_NAME_MISSING        -1038 // zone name missing
#define ANI_E_CLI_AUTH_ZONE_EMPTY               -1039 // zone empty
#define ANI_E_CLI_AUTH_ZONE_NOT_FOUND           -1040 // zone not found
#define ANI_E_CLI_AUTH_SERVER_ARG_NOT_FOUND     -1041 // Cli Auth server arg missing
#define ANI_E_CLI_IFNAME_ARG_NOT_FOUND          -1042 // Ifname arg missing
#define ANI_E_CLI_KEYINDEX_ARG_NOT_FOUND        -1043 // Keyindex arg missing
#define ANI_E_CLI_KEYLENGTH_ARG_NOT_FOUND       -1044 // KeyLength arg missing
#define ANI_E_CLI_KEYLENGTH_ARG_INVALID         -1045 // KeyLength arg invalid
#define ANI_E_CLI_SET_WEP_KEY_FAILED            -1046 // Set Wep key failed
#define ANI_E_CLI_SYS_INTERNAL_ERROR            -1047 // Internal error
#define ANI_E_CLI_OLD_PASSWORD_MISSING          -1048 // Old password missing
#define ANI_E_CLI_NEW_PASSWORD_MISSING          -1049 // New password
                                                      // missing
#define ANI_E_CLI_WEP_KEY_LEN_ERROR             -1051 // key length error
#define ANI_E_CLI_WEP_KEY_HEX_ERROR             -1052 // no hex character
#define ANI_E_CLI_WPA_MODES_CFG_ERROR           -1053 // WPA config error
#define ANI_E_CLI_WEP_AND_OPEN_CFG_ERROR        -1054 // WEP and open error
#define ANI_E_CLI_OPEN_AND_WEP_CFG_ERROR        -1055 // open and WEP error
#define ANI_E_CLI_LEGACY_WEP_AND_OPEN_CFG_ERROR -1056 // WEP and open error
#define ANI_E_CLI_OPEN_AND_LEGACY_WEP_CFG_ERROR -1057 // open and WEP error
#define ANI_E_CLI_WPA_MODES_NOT_AVAILABLE       -1058 // WPA modes not active
#define ANI_E_CLI_INVALID_LISENCE_KEY           -1059 // invalid license
#define ANI_E_CLI_EXISTING_LISENCE_KEY          -1060 // duplicated license
#define ANI_E_CLI_WEP_INVALID_LENGTH_CHANGE     -1061 // invalid length change
#define ANI_E_CLI_CNF_PASSWORD_MISMATCH         -1062 // confirm password mismatch
#define ANI_E_CLI_INVALID_DISABLE               -1063 // cannot disable all sec modes
#define ANI_E_CLI_PACKNUM_RANGE_ERROR           -1064 // cannot disable all sec modes
#define ANI_E_CLI_IP_ADDR_INVALID               -1065 // IP address invalid
#define ANI_E_CLI_AUTH_ZONE_NAME_INVALID        -1066 // zone name invalid
#define ANI_E_CLI_OLD_PASSWORD_INVALID          -1067 // Old password invalid
#define ANI_E_CLI_NO_EXT_RAD_ON_NONSECP         -1068 // Cannot add ext auth-server on
                                                      // non-SEC/P with RAD proxying on
#define ANI_E_CLI_NO_EXT_AUTH_ZONE_ALLOWED      -1069 // Cannot add ext auth-zone on
                                                      // with RAD proxying on
#define ANI_E_CLI_PORTAL_ZONE_AUTO_CONFIGURED   -1070 // Cannot manage portal auth-zone
#define ANI_E_CLI_DEL_REQ_ON_REF_AUTH_SERVER    -1071 // Cannot delete auth-server
                                                      // that is in a zone
#define ANI_E_CLI_DEL_REQ_ON_REF_AUTH_ZONE      -1072 // Cannot delete auth-zone
                                                      // that is used by SSID
#define ANI_E_CLI_INVALID_INTERIM_UPDT_VALUE    -1073 // Invalid Accounting interim update interval
#define ANI_E_RANGE_END_AAG_CLI                 -1199
// ...CLI Range ends here

// Non-CLI related error codes..
#define ANI_E_MIC_FAILED                        -1200 // A MIC check failed
#define ANI_E_REPLAY_CHECK_FAILED               -1201 // Replay Ctr mismatch
#define ANI_E_RADIUS_PROFILE_MISSING            -1202 // User profile
                                                      // not found
#define ANI_E_AUTH_FAILED                       -1203 // Authentication failed
#define ANI_E_RADIUS_PRIV_LEVEL_MISSING         -1204 // ANI_ADMIN_LEVEL is missing
#define ANI_E_RADIUS_PRIV_LEVEL_INCORRECT       -1205 // ANI_ADMIN_LEVEL is incorrect

// For some reason this is not contiguous with the other error codes(???)
#define ANI_E_INVALID_COOKIE                    -1300 // Invalid cookie

#define ANI_E_RANGE_END_AAG                     -2048

/*
 * The range -2049 to -2148 is reserved for use by the
 * RADIUS client side library.
 */
#define ANI_E_RANGE_START_RAD       -2049
#define ANI_E_RAD_FAILED            -2049 /// RADIUS operation failed
#define ANI_E_RAD_ATTR_TOO_LONG     -2050 /// Attribute too long
#define ANI_E_RAD_UNSOLICITED_RESP  -2051 /// Unsolicited response
#define ANI_E_RAD_BAD_RESP_AUTH     -2052 /// Response auth check failed
#define ANI_E_RAD_BAD_MESSG_AUTH    -2053 /// Response signature invalid
#define ANI_E_RAD_ATTR_NOT_FOUND    -2054 /// Requested attr not found
#define ANI_E_RAD_TIMEOUT           -2055 /// Request timed out waiting for server
#define ANI_E_RAD_REJECT            -2056 /// Radius server did not accept user
#define ANI_E_RANGE_END_RAD         -2148

/*
 * The range -2149 to -2999 is reserved for use by the SSM library.
 */
#define ANI_E_RANGE_START_SSM -2149
#define ANI_E_SSM_CERT_UNPARSEABLE          (ANI_E_RANGE_START_SSM - 1)
#define ANI_E_SSM_CERT_EXPIRED              (ANI_E_RANGE_START_SSM - 2)
#define ANI_E_SSM_CERT_THUMBPRINT_MISMATCH  (ANI_E_RANGE_START_SSM - 3)
#define ANI_E_SSM_CERT_NEW_ID               (ANI_E_RANGE_START_SSM - 4)
#define ANI_E_RANGE_END_SSM   -2999

/*
 * The range -3000 to -3500 is reserved for use by the
 * NetSim Server, Client, Client Modules and Pseudo driver
 */
#define ANI_E_RANGE_START_NETSIM     -3000
#define ANI_E_RANGE_END_NETSIM       -3500

/*
 * The range -3501 to -4000 is reserved for use by the
 * Discovery Server and its libraries.
 */
#define ANI_E_RANGE_START_DISC     -3501
#define ANI_E_RANGE_END_DISC       -4000

/*
 * The range -4001 to -4500 is reserved for use by the
 * Ezcfg Server
 */
#define ANI_E_RANGE_START_EZC     -4001

// See file aniNmpEzcSvcMsgs.h for EZC specific error codes and messages
#define ANI_E_RANGE_END_EZC       -4500

/*
 * The range -4501 to -4600 is reserved for use by the
 * Software Download (SWD) Server
 */
#define ANI_E_RANGE_START_SWD     -4501

// See file aniSwdSvcMsgs.h for SWD specific error codes and messages
#define ANI_E_RANGE_END_SWD       -4600

/*
 * The range -4601 to -4700 is reserved for use by the
 * Data Distribution Service (DDS) Server
 */
#define ANI_E_RANGE_START_DDS     -4601

// See file aniDdsSvcMsgs.h for DDS specific error codes and messages
#define ANI_E_RANGE_END_DDS       -4700

/*
 * The range -4701 to -4800 is reserved for use by
 * HTTPS components.
 */
#define ANI_E_RANGE_START_HTTPS -4701
#define ANI_E_HTTPS_UNREACHABLE      (ANI_E_RANGE_START_HTTPS - 0)
#define ANI_E_HTTPS_UNTRUSTED_CERT   (ANI_E_RANGE_START_HTTPS - 1)
#define ANI_E_HTTPS_RECVD_ALERT      (ANI_E_RANGE_START_HTTPS - 2)
#define ANI_E_HTTPS_FAILED           (ANI_E_RANGE_START_HTTPS - 3)
#define ANI_E_RANGE_END_HTTPS   -4800

/*
 * The range -4801 to -4900 is reserved for use by
 * enrollment components.
 */
#define ANI_E_RANGE_START_ENROLLMENT -4801
#define ANI_E_ENROLL_TP_AVAILABLE     (ANI_E_RANGE_START_ENROLLMENT - 0)
#define ANI_E_ENROLL_ALREADY_TRUSTED  (ANI_E_RANGE_START_ENROLLMENT - 1)
#define ANI_E_ENROLL_NOT_FOUND        (ANI_E_RANGE_START_ENROLLMENT - 2)
#define ANI_E_ENROLL_PWD_FAILED       (ANI_E_RANGE_START_ENROLLMENT - 3)
#define ANI_E_ENROLL_FAILED           (ANI_E_RANGE_START_ENROLLMENT - 4)
#define ANI_E_ENROLL_NOT_PRISTINE     (ANI_E_RANGE_START_ENROLLMENT - 5)
#define ANI_E_RANGE_END_ENROLLMENT   -4900


/*
 * The range -4901 to -5000 is reserved for use by NSM.
 */
#define ANI_E_RANGE_START_NSM -4901
#define ANI_E_NSM_IPADDR_ASSIGNED     (ANI_E_RANGE_START_NSM - 0)
#define ANI_E_RANGE_END_NSM   -5000

/*
 * The range -5001 to -5100 is reserved for use by the image
 * validation library.
 */
#define ANI_E_RANGE_START_IMAGE -5001
#define ANI_E_IMAGE_INVALID         (ANI_E_RANGE_START_IMAGE - 0)
#define ANI_E_IMAGE_UNSUPPORTED     (ANI_E_RANGE_START_IMAGE - 1)
#define ANI_E_RANGE_END_IMAGE   -5100

/*
 * The range -5101 to -5200 is reserved for use by CM
 */
#define ANI_E_RANGE_START_CM -5101
#define ANI_E_MESG_UNAVAILABLE      (ANI_E_RANGE_START_IMAGE - 0)
#define ANI_E_RANGE_END_CM   -5200


#define ANI_IS_STATUS_SUCCESS( retVal )  ( ( retVal >= 0 ) )

#endif //_ANI_ERRORS_H_
