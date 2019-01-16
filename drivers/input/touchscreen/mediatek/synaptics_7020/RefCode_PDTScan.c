#include "RefCode.h"
#include "RefCode_PDTScan.h"

//void buildInterruptMask(U08 funcNum, U08 num);


unsigned short F11_Query_Base = 0;
unsigned short F11_Ctrl_Base = 0;
unsigned short F11_Data_Base = 0;
unsigned short F11_Cmd_Base = 0;
unsigned short F34_Query_Base = 0;
unsigned short F34_Cmd_Base = 0;
unsigned short F34_Ctrl_Base = 0;
unsigned short F34_Data_Base = 0;
unsigned short F01_Query_Base = 0;
unsigned short F01_Ctrl_Base;
unsigned short F01_Data_Base;
unsigned short F01_Cmd_Base;
unsigned short F54_Query_Base;
unsigned short F54_Command_Base;
unsigned short F54_Control_Base;
unsigned short F54_Data_Base;
unsigned short F1A_Query_Base;
unsigned short F1A_Command_Base;
unsigned short F1A_Control_Base;
unsigned short F1A_Data_Base;

unsigned short F54_Data_LowIndex;
unsigned short F54_Data_HighIndex;
unsigned short F54_Data_Buffer;
unsigned short F54_PhysicalTx_Addr;
unsigned short F54_PhysicalRx_Addr;
unsigned short F54_CBCSettings;
unsigned short F11_MaxNumberOfTx_Addr;
unsigned short F11_MaxNumberOfRx_Addr;
unsigned short F1A_Button_Mapping;


unsigned char MaxNumberTx;
unsigned char MaxNumberRx;
unsigned char numberOfTx;
unsigned char numberOfRx;
unsigned char MaxButton;
unsigned char TxChannelUsed[CFG_F54_TXCOUNT];
unsigned char RxChannelUsed[CFG_F54_TXCOUNT];
unsigned char ButtonMapping[16];
unsigned char CheckButton[CFG_F54_TXCOUNT][CFG_F54_RXCOUNT];
unsigned char ButtonTXUsed[CFG_F54_TXCOUNT];
unsigned char ButtonRXUsed[CFG_F54_TXCOUNT];

void SYNA_PDTScan(void)
{
    unsigned char j, tmp = 255;
    unsigned char FUNC_EXISTS = 0;
    unsigned char in[10];
#ifdef F54_Porting
    unsigned short i;
     char buf[512] = {0};
     int ret = 0;
#else
    unsigned short i, tmp16, l, m;
#endif

    i = 0;

//    constructF01F81();

    do {
        readRMI((i << 8) | PDT_ADDR, &tmp, 1);
        if(tmp & 0x40)
        {
#ifdef F54_Porting
            ret = sprintf(buf, "\nNon-Standard Page Description Table not supported\n");
#else
            printk("\nNon-Standard Page Description Table not supported\n");
#endif
            cleanExit(1);
        }

        FUNC_EXISTS = 0;    // reset flag
        j = 0;                // reset func addr info index

        while(1)
        {
            j++;
            readRMI((i << 8) | (PDT_ADDR - PDT_SIZE*j), in, 6);
            readRMI((i << 8) | 0xFF, &tmp, 1);

            if(in[5] == 0x00)
            {        // No more functions on this page
                if(FUNC_EXISTS == 0)
                {
            //        constructPrivRMI();
#ifdef F54_Porting
                    write_log(buf);
#endif
                    return;
                }
                else
                {
                    i++;
                }
                break;
            }
            else if(in[5] == 0x11)
            {        // Function11
                F11_Query_Base = (i << 8) | in[0];
                F11_Cmd_Base = (i << 8) | in[1];
                F11_Ctrl_Base = (i << 8) | in[2];
                F11_Data_Base = (i << 8) | in[3];
#ifdef F54_Porting
                ret += sprintf(buf+ret, "\n-- RMI Function $%02X, Address = 0x%02x --\n", in[5], (PDT_ADDR - PDT_SIZE*j));
#else
                printk("\n-- RMI Function $%02X, Address = 0x%02x --\n", in[5], (PDT_ADDR - PDT_SIZE*j));
#endif
            }
            else if(in[5] == 0x34)
            {        // Function34
                F34_Query_Base = (i << 8) | in[0];
                F34_Cmd_Base = (i << 8) | in[1];
                F34_Ctrl_Base = (i << 8) | in[2];
                F34_Data_Base = (i << 8) | in[3];
#ifdef F54_Porting
                ret += sprintf(buf+ret, "\n-- RMI Function $%02X, Address = 0x%02x --\n", in[5], (PDT_ADDR - PDT_SIZE*j));
#else
                printk("\n-- RMI Function $%02X, Address = 0x%02x --\n", in[5], (PDT_ADDR - PDT_SIZE*j));
#endif
            }
            else if(in[5] == 0x01)
            {         // Function01
                F01_Query_Base = (i << 8) | in[0];
                F01_Cmd_Base = (i << 8) | in[1];
                F01_Ctrl_Base = (i << 8) | in[2];
                F01_Data_Base = (i << 8) | in[3];
#ifdef F54_Porting
                ret += sprintf(buf+ret, "\n-- RMI Function $%02X, Address = 0x%02x --\n", in[5], (PDT_ADDR - PDT_SIZE*j));
#else
                printk("\n-- RMI Function $%02X, Address = 0x%02x --\n", in[5], (PDT_ADDR - PDT_SIZE*j));
#endif
            }
            else if(in[5] == 0x54)
            {
                F54_Query_Base = (i << 8) | in[0];
                F54_Command_Base = (i << 8) | in[1];
                F54_Control_Base = (i << 8) | in[2];
                F54_Data_Base = (i << 8) | in[3];

                F54_Data_LowIndex = F54_Data_Base + 1;
                F54_Data_HighIndex = F54_Data_Base + 2;
                F54_Data_Buffer = F54_Data_Base + 3;
                F54_CBCSettings = F54_Control_Base + 8;
#ifdef _DS4_3_0_
                F54_PhysicalRx_Addr = F54_Control_Base + 18;
#endif

#ifdef F54_Porting
                ret += sprintf(buf+ret, "\n-- RMI Function $%02X, Address = 0x%02x --\n", in[5], (PDT_ADDR - PDT_SIZE*j));
#else
                printk("\n-- RMI Function $%02X, Address = 0x%02x --\n", in[5], (PDT_ADDR - PDT_SIZE*j));
#endif
            }
            else if(in[5] == 0x1A)
            {
                F1A_Query_Base = (i << 8) | in[0];
                F1A_Command_Base = (i << 8) | in[1];
                F1A_Control_Base = (i << 8) | in[2];
                F1A_Data_Base = (i << 8) | in[3];

                F1A_Button_Mapping = F1A_Control_Base + 2;

#ifdef F54_Porting
                ret += sprintf(buf+ret, "\n-- RMI Function $%02X, Address = 0x%02x --\n", in[5], (PDT_ADDR - PDT_SIZE*j));
#else
                printk("\n-- RMI Function $%02X, Address = 0x%02x --\n", in[5], (PDT_ADDR - PDT_SIZE*j));
#endif
            }
            else
            {
#ifdef F54_Porting
                ret += sprintf(buf+ret, "\n-- RMI Function $%02X not supported --\n", in[5]);
#else
                printk("\n-- RMI Function $%02X not supported --\n", in[5]);
#endif
            }
            FUNC_EXISTS = 1;
        }
    } while(FUNC_EXISTS == 1);
}

void SYNA_PDTScan_BootloaderMode()
{
    unsigned char j, tmp = 255;
    unsigned char in[10];
    unsigned short i;

        i = 0;
        j = 0;                // reset func addr info index
        readRMI((i << 8) | PDT_ADDR, &tmp, 1);
        if(tmp & 0x40)
        {
            printk("\nNon-Standard Page Description Table not supported\n");
            cleanExit(1);
        }

        while(1)
        {
            j++;
            readRMI((i << 8) | (PDT_ADDR - PDT_SIZE*j), in, 6);

            if(in[5] == 0x00)
            {        // No more functions on this page
                return;
            }
            else if(in[5] == 0x34)
            {        // Function34
                F34_Query_Base = (i << 8) | in[0];
                F34_Cmd_Base = (i << 8) | in[1];
                F34_Ctrl_Base = (i << 8) | in[2];
                F34_Data_Base = (i << 8) | in[3];
                printk("\n-- RMI Function $%02X, Address = 0x%02x --\n", in[5], (PDT_ADDR - PDT_SIZE*j));
            }
            else if(in[5] == 0x01)
            {         // Function01
                F01_Query_Base = (i << 8) | in[0];
                F01_Cmd_Base = (i << 8) | in[1];
                F01_Ctrl_Base = (i << 8) | in[2];
                F01_Data_Base = (i << 8) | in[3];
                printk("\n-- RMI Function $%02X, Address = 0x%02x --\n", in[5], (PDT_ADDR - PDT_SIZE*j));
            }
            else
            {
                printk("\n-- RMI Function $%02X not supported --\n", in[5]);
            }
        }
}

void SYNA_PrintRMI()
{
    printk("F11_Query_Base = 0x%x\n", F11_Query_Base);
    printk("F11_Cmd_Base = 0x%x\n", F11_Cmd_Base);
    printk("F11_Ctrl_Base = 0x%x\n", F11_Ctrl_Base);
    printk("F11_Data_Base = 0x%x\n", F11_Data_Base);
    printk("F34_Query_Base = 0x%x\n", F34_Query_Base);
    printk("F34_Cmd_Base = 0x%x\n", F34_Cmd_Base);
    printk("F34_Ctrl_Base = 0x%x\n", F34_Ctrl_Base);
    printk("F34_Data_Base = 0x%x\n", F34_Data_Base);
    printk("F01_Query_Base = 0x%x\n", F01_Query_Base);
    printk("F01_Cmd_Base = 0x%x\n", F01_Cmd_Base);
    printk("F01_Ctrl_Base = 0x%x\n", F01_Ctrl_Base);
    printk("F01_Data_Base = 0x%x\n", F01_Data_Base);
    printk("F54_Query_Base = 0x%x\n", F54_Query_Base);
    printk("F54_Command_Base = 0x%x\n", F54_Command_Base);
    printk("F54_Control_Base = 0x%x\n", F54_Control_Base);
    printk("F54_Data_Base = 0x%x\n", F54_Data_Base);
    printk("F54_Data_LowIndex = 0x%x\n", F54_Data_LowIndex);
    printk("F54_Data_HighIndex = 0x%x\n", F54_Data_HighIndex);
    printk("F54_Data_Buffer = 0x%x\n", F54_Data_Buffer);
    printk("F54_CBCSettings = 0x%x\n", F54_CBCSettings);
#ifdef _DS4_3_0_
    printk("F54_PhysicalRx_Addr = 0x%x\n", F54_PhysicalRx_Addr);
#endif
    printk("F1A_Query_Base = 0x%x\n", F1A_Query_Base);
    printk("F1A_Command_Base = 0x%x\n", F1A_Command_Base);
    printk("F1A_Control_Base = 0x%x\n", F1A_Control_Base);
    printk("F1A_Data_Base = 0x%x\n", F1A_Data_Base);
}

void SYNA_ConstructRMI_F54(void)
{
#ifdef F54_Porting
#else
    unsigned char command;
#endif

    int i;

    numberOfRx = 0;
    numberOfTx = 0;
    MaxButton = 0;

    for (i = 0 ; i < CFG_F54_TXCOUNT; i++)
    {
        ButtonRXUsed[i] = 0;
        ButtonTXUsed[i] = 0;
    }

    F11_MaxNumberOfTx_Addr = F11_Query_Base + 2;
    F11_MaxNumberOfRx_Addr = F11_Query_Base + 3;

    //Check Used Rx channels
    readRMI(F11_MaxNumberOfRx_Addr, &MaxNumberRx, 1);
#ifdef _DS4_3_0_
    F54_PhysicalTx_Addr= F54_PhysicalRx_Addr + MaxNumberRx;
    readRMI(F54_PhysicalRx_Addr, &RxChannelUsed[0], MaxNumberRx);
    readRMI(F11_MaxNumberOfTx_Addr, &MaxNumberTx, 1);
    readRMI(F54_PhysicalTx_Addr, &TxChannelUsed[0], MaxNumberTx);
#else
#ifdef _DS4_3_2_
    readRMI(F55_PhysicalRx_Addr, &RxChannelUsed[0], MaxNumberRx);
    readRMI(F11_MaxNumberOfTx_Addr, &MaxNumberTx, 1);
    readRMI(F55_PhysicalRx_Addr + 1, &TxChannelUsed[0], MaxNumberTx);
#endif
#endif

    //Check used number of Rx
    for(i=0; i<MaxNumberRx; i++)
    {
        if(RxChannelUsed[i] == 0xff) break;
        numberOfRx++;
    }

    //Check used number of Tx
    for(i=0; i<MaxNumberTx; i++)
    {
        if(TxChannelUsed[i] == 0xff) break;
        numberOfTx++;
    }
}

void SYNA_ConstructRMI_F1A(void)
{
#ifdef F54_Porting
#else
    unsigned char command;
#endif
    int i, j, k;
    unsigned char temp;

    readRMI(F1A_Button_Mapping, ButtonMapping, 16);

    for(j=0; j<numberOfTx; j++)
        for(i=0; i<numberOfRx; i++)
            CheckButton[j][i] = 0;

    for(k=0; k<8; k++)
    {
        if((temp = ButtonMapping[k*2+1]) == 0xFF)    break;
        else
        {
            for(i=0; i<numberOfRx; i++)
                if(RxChannelUsed[i] == temp)    break;

            if(CheckButton[0][i] == 0)
            {
                for(j=0; j<numberOfTx; j++)
                    CheckButton[j][i] = 1;

                for(j=0; j<numberOfTx; j++)
                    if(TxChannelUsed[j] == ButtonMapping[k*2])    break;
                    CheckButton[j][i] = 2;
            }
            else
            {
                for(j=0; j<numberOfTx; j++)
                    if(TxChannelUsed[j] == ButtonMapping[k*2])    break;
                    CheckButton[j][i] = 2;
            }
        }

/////////////////////////////////////////////////////////////////////////////
// For Button Delta Image Display
        if((temp = ButtonMapping[k*2+1]) == 0xFF)    break;     // Button RX
        else
        {
            for(i=0; i<MaxNumberRx; i++)
            {
                if(RxChannelUsed[i] == temp) break;
                ButtonRXUsed[k]++;
            }
            MaxButton++;
        }

        if((temp = ButtonMapping[k*2]) == 0xFF)    break;     // Button TX
        else
        {
            for(i=0; i<MaxNumberTx; i++)
            {
                if(TxChannelUsed[i] == temp) break;
                ButtonTXUsed[k]++;
            }
        }
////////////////////////////////////////////////////////////////////////////////
    }
}

