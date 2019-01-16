#ifndef __CCCI_PMIC_H__
#define __CCCI_PMIC_H__

typedef enum
{
    PMIC6326_VSIM_ENABLE = 0,
    PMIC6326_VSIM_SET_AND_ENABLE = 1,
    PMIC6236_LOCK = 2,
    PMIC6326_UNLOCK = 3,
    PMIC6326_VSIM2_ENABLE = 4,
    PMIC6326_VSIM2_SET_AND_ENABLE = 5,
    PMIC6326_MAX
}pmic6326_ccci_op;

typedef enum
{
    PMIC6326_REQ = 0,        // Local side send request to remote side
    PMIC6326_RES = 1        // Remote side send response to local side
}pmic6326_ccci_type;

/*
    The CCCI message format (CCIF Mailbox port)
    | 4 bytes        | 4 bytes       | 4 bytes            | 4 bytes         |
      Magic number     Message ID      Logical channel      Reserved
                       PMIC msg                             PMIC msg info
*/

/*
    PMIC msg format
    (MSB)                                                      (LSB)
    |  1 byte        | 1 byte        | 1 byte        | 1 byte      |
       Param2          Param1          Type            Op
*/

/*
    PMIC msg info format
    (MSB)                                                (LSB)
    |  1 byte        | 1 byte        | 2 bytes               |
       Param2          Param1          Exec_time
*/



typedef struct
{
    unsigned short    pmic6326_op;        // Operation
    unsigned short    pmic6326_type;        // message type: Request or Response
    unsigned short    pmic6326_param1;
    unsigned short    pmic6326_param2;
}pmic6326_ccci_msg;

typedef struct
{
    unsigned int     pmic6326_exec_time;        // Operation execution time (In ms)
    unsigned short    pmic6326_param1;
    unsigned short    pmic6326_param2;
}pmic6326_ccci_msg_info;

/*
    PMIC share memory
    (MSB)                                                   (LSB)
    |  1 byte        | 1 byte        | 1 byte        | 1 byte   |
       Param2          Param1          Type            Op
    |  1 byte        | 1 byte        | 2 bytes                  |
       Param2          Param1          Exec_time
*/

typedef struct
{
    pmic6326_ccci_msg ccci_msg;
    pmic6326_ccci_msg_info ccci_msg_info;
}pmic6326_share_mem_info;

typedef struct
{
    pmic6326_ccci_msg      ccci_msg;
    pmic6326_ccci_msg_info ccci_msg_info;
} shared_mem_pmic_t;

int __init ccci_pmic_init(void);
void __exit ccci_pmic_exit(void);

#define CCCI_PMIC_SMEM_SIZE sizeof(shared_mem_pmic_t)
#endif // __CCCI_PMIC_H__
