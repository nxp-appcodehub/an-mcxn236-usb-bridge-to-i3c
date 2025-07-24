/*
 * Copyright (c) 2015 - 2016, Freescale Semiconductor, Inc.
 * Copyright 2016 - 2017, 2024 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "i3c_task_interrupt.h"
#include "fsl_i3c.h"
#include "stdint.h"
#include "fsl_common.h"

static void i3c_master_ibi_callback(I3C_Type *base,
                                    i3c_master_handle_t *handle,
                                    i3c_ibi_type_t ibiType,
                                    i3c_ibi_state_t ibiState);
static void i3c_master_callback(I3C_Type *base, i3c_master_handle_t *handle, status_t status, void *userData);
/*******************************************************************************
 * Variables
 ******************************************************************************/
uint8_t g_master_ibiBuff[10];

i3c_master_handle_t g_i3c0_m_handle;
i3c_master_handle_t g_i3c1_m_handle;

const i3c_master_transfer_callback_t masterCallback = {
    .slave2Master = NULL, .ibiCallback = i3c_master_ibi_callback, .transferComplete = i3c_master_callback};

volatile bool g_masterCompletionFlag = false;
volatile bool g_ibiWonFlag           = false;
volatile uint8_t g_transferCompletionCounts = 0;
volatile status_t g_completionStatus = kStatus_Success;
uint8_t g_ibiUserBuff[10U];
uint8_t g_ibiUserBuffUsed = 0;
uint8_t g_i3c_tx_buffer[1024];
uint8_t g_i3c_rx_buffer[1024];
uint8_t g_slaveAddr;
uint8_t g_addressList[8];
uint8_t g_ibiList[5];
uint8_t g_deviceList[8][9];

static void i3c_master_ibi_callback(I3C_Type *base,
                                    i3c_master_handle_t *handle,
                                    i3c_ibi_type_t ibiType,
                                    i3c_ibi_state_t ibiState)
{
	uint32_t reg32;

    switch (ibiType)
    {
        case kI3C_IbiNormal:
            if (ibiState == kI3C_IbiDataBuffNeed)
            {
                handle->ibiBuff = g_master_ibiBuff;
            }
            else
            {
                memcpy(g_ibiUserBuff, (void *)handle->ibiBuff, handle->ibiPayloadSize);
                g_ibiUserBuffUsed = handle->ibiPayloadSize;
            }
            break;
        case kI3C_IbiHotJoin:
            reg32 = base->MCTRL;
            reg32 |= 0x3 | (0x3 << 6);// request IBIACKNACK(used for hot join and request IBI ACK) & manual response for DAA operation
            base->MCTRL = reg32;
        	break;
        default:
            assert(false);
            break;
    }
}

static void i3c_master_callback(I3C_Type *base, i3c_master_handle_t *handle, status_t status, void *userData)
{
    if (status == kStatus_I3C_IBIWon)
    {
        g_ibiWonFlag = true;
    }
    else
    {
        /* Signal transfer complete when received complete status. */
        g_masterCompletionFlag = true;
    }

    g_completionStatus = status;
}

void usb_to_i3c_init(uint8_t interface_idex, usb_i3c_param_t param, i3c_master_handle_t *s_i3c_m_handle)
{
    i3c_master_config_t masterConfig;
    I3C_Type* i3c_index;

    i3c_index = (interface_idex == 0x0) ? I3C0 : I3C1;

    I3C_MasterGetDefaultConfig(&masterConfig);
    masterConfig.baudRate_Hz.i2cBaud          = param.i2c_freq;
    masterConfig.baudRate_Hz.i3cPushPullBaud  = param.pp_freq;
    masterConfig.baudRate_Hz.i3cOpenDrainBaud = param.od_freq;
    masterConfig.enableOpenDrainStop          = false;

    I3C_MasterInit(i3c_index, &masterConfig, CLOCK_GetI3cClkFreq(interface_idex));

    I3C_MasterTransferCreateHandle(i3c_index, s_i3c_m_handle, &masterCallback, NULL);
}

void usb_i3c_ibi_reg(uint8_t interface_idex, usb_i3c_param_t param)
{
	I3C_Type* i3c_index;

	i3c_index = (interface_idex == 0x0) ? I3C0 : I3C1;

    i3c_register_ibi_addr_t ibiRecord; //= {.address = {param.ibi_address}, .ibiHasPayload = true};

    for(uint32_t index = 0; index < param.ibi_address_num; index++)
    {
        ibiRecord.address[index] = g_ibiList[index];
    }
    ibiRecord.ibiHasPayload = true;
    I3C_MasterRegisterIBI(i3c_index, &ibiRecord);
}

status_t usb_i3c_ccc_write_broadcast(uint8_t interface_idex, usb_i3c_param_t param, i3c_master_handle_t *s_i3c_m_handle)
{
    status_t result;
	I3C_Type* i3c_index;

	i3c_master_transfer_t masterXfer;

    i3c_index = (interface_idex == 0x0) ? I3C0 : I3C1;

	/* Send ccc command code*/
	memset(&masterXfer, 0, sizeof(masterXfer));
	masterXfer.slaveAddress   = 0x7EU; /* Broadcast address */
	masterXfer.data           = g_i3c_tx_buffer;
	masterXfer.dataSize       = param.len;
	masterXfer.subaddress     = param.ccc_code;
	masterXfer.subaddressSize = 1U;
	masterXfer.direction      = kI3C_Write;
	masterXfer.busType        = kI3C_TypeI3CSdr;
	masterXfer.flags          = kI3C_TransferDefaultFlag;
	masterXfer.ibiResponse    = kI3C_IbiRespAckMandatory;
	result                    = I3C_MasterTransferNonBlocking(i3c_index, s_i3c_m_handle, &masterXfer);
    if (kStatus_Success != result)
    {
        return result;
    }

	return kStatus_Success;
}

status_t usb_i3c_ccc_write_direct(uint8_t interface_idex, usb_i3c_param_t param, i3c_master_handle_t *s_i3c_m_handle)
{
    status_t result;
	I3C_Type* i3c_index;

	i3c_master_transfer_t masterXfer;

    i3c_index = (interface_idex == 0x0) ? I3C0 : I3C1;

	/* Reset dynamic address before DAA */
	memset(&masterXfer, 0, sizeof(masterXfer));
	masterXfer.slaveAddress   = 0x7EU; /* Broadcast address */
	masterXfer.data           = g_i3c_tx_buffer;
	masterXfer.dataSize       = param.len;
	masterXfer.subaddress     = param.ccc_code;
	masterXfer.subaddressSize = 1U;
	masterXfer.direction      = kI3C_Write;
	masterXfer.busType        = kI3C_TypeI3CSdr;
	masterXfer.flags          = kI3C_TransferNoStopFlag;
	masterXfer.ibiResponse    = kI3C_IbiRespAckMandatory;
	result                    = I3C_MasterTransferNonBlocking(i3c_index, s_i3c_m_handle, &masterXfer);
    if (kStatus_Success != result)
    {
        return result;
    }

    if(g_masterCompletionFlag && (g_completionStatus == kStatus_Success))
    {
    	g_transferCompletionCounts++;
    }

	/* Send ccc command code*/
	memset(&masterXfer, 0, sizeof(masterXfer));
	masterXfer.slaveAddress   = param.slaveAddress; /* Direct address */
    masterXfer.data           = g_i3c_rx_buffer;
    if(param.ccc_code == KUSB_I3C_GETSTATUS)
    {
    	masterXfer.dataSize   = 2U;
    }
    else
    {
        masterXfer.dataSize   = 1U;
    }
	masterXfer.direction      = kI3C_Read;
	masterXfer.busType        = kI3C_TypeI3CSdr;
	masterXfer.flags          = kI3C_TransferRepeatedStartFlag;
	masterXfer.ibiResponse    = kI3C_IbiRespAckMandatory;
	result                    = I3C_MasterTransferNonBlocking(i3c_index, s_i3c_m_handle, &masterXfer);
    if (kStatus_Success != result)
    {
        return result;
    }

    if(g_masterCompletionFlag && (g_completionStatus == kStatus_Success))
    {
    	g_transferCompletionCounts++;
    }

	return kStatus_Success;
}

status_t usb_i3c_reg_data_write(uint8_t interface_idex, usb_i3c_param_t param, i3c_master_handle_t *s_i3c_m_handle)
{
    status_t result;
	I3C_Type* i3c_index;

	i3c_master_transfer_t masterXfer;

    i3c_index = (interface_idex == 0x0) ? I3C0 : I3C1;

    memset(&masterXfer, 0, sizeof(masterXfer));

    masterXfer.slaveAddress = param.slaveAddress;
    masterXfer.data         = g_i3c_tx_buffer;
    masterXfer.dataSize     = param.len;
    masterXfer.subaddress   = param.regAddress;
    masterXfer.subaddressSize = 1;
    masterXfer.direction    = kI3C_Write;
    masterXfer.busType      = param.i3c_mode ;
    masterXfer.flags        = kI3C_TransferDefaultFlag;
    masterXfer.ibiResponse  = kI3C_IbiRespAckMandatory;
    result                  = I3C_MasterTransferNonBlocking(i3c_index, s_i3c_m_handle, &masterXfer);
    if (kStatus_Success != result)
    {
        return result;
    }

	return kStatus_Success;
}

status_t usb_i3c_data_write(uint8_t interface_idex, usb_i3c_param_t param, i3c_master_handle_t *s_i3c_m_handle)
{
    status_t result;
	I3C_Type* i3c_index;

	i3c_master_transfer_t masterXfer;

    i3c_index = (interface_idex == 0x0) ? I3C0 : I3C1;

    memset(&masterXfer, 0, sizeof(masterXfer));

    masterXfer.slaveAddress = param.slaveAddress;
    masterXfer.data         = g_i3c_tx_buffer;
    masterXfer.dataSize     = param.len;
    masterXfer.direction    = kI3C_Write;
    masterXfer.busType      = param.i3c_mode ;
    masterXfer.flags        = kI3C_TransferDefaultFlag;
    masterXfer.ibiResponse  = kI3C_IbiRespAckMandatory;
    result                  = I3C_MasterTransferNonBlocking(i3c_index, s_i3c_m_handle, &masterXfer);
    if (kStatus_Success != result)
    {
        return result;
    }

	return kStatus_Success;
}

status_t usb_i3c_reg_data_read(uint8_t interface_idex, usb_i3c_param_t param, i3c_master_handle_t *s_i3c_m_handle)
{
    status_t result;
	I3C_Type* i3c_index;

	i3c_master_transfer_t masterXfer;

    i3c_index = (interface_idex == 0x0) ? I3C0 : I3C1;

    memset(&masterXfer, 0, sizeof(masterXfer));

    masterXfer.slaveAddress = param.slaveAddress;
    masterXfer.data         = g_i3c_rx_buffer;
    masterXfer.dataSize     = param.len;
    masterXfer.subaddress   = param.regAddress;
    masterXfer.subaddressSize = 1;
    masterXfer.direction    = kI3C_Read;
    masterXfer.busType      = param.i3c_mode ;
    masterXfer.flags        = kI3C_TransferDefaultFlag;
    masterXfer.ibiResponse  = kI3C_IbiRespAckMandatory;
    result                  = I3C_MasterTransferNonBlocking(i3c_index, s_i3c_m_handle, &masterXfer);
    if (kStatus_Success != result)
    {
        return result;
    }

	return kStatus_Success;
}

status_t usb_i3c_data_read(uint8_t interface_idex, usb_i3c_param_t param, i3c_master_handle_t *s_i3c_m_handle)
{
    status_t result;
	I3C_Type* i3c_index;

	i3c_master_transfer_t masterXfer;

    i3c_index = (interface_idex == 0x0) ? I3C0 : I3C1;

    memset(&masterXfer, 0, sizeof(masterXfer));

    masterXfer.slaveAddress = param.slaveAddress;
    masterXfer.data         = g_i3c_rx_buffer;
    masterXfer.dataSize     = param.len;
    masterXfer.direction    = kI3C_Read;
    masterXfer.busType      = param.i3c_mode ;
    masterXfer.flags        = kI3C_TransferDefaultFlag;
    masterXfer.ibiResponse  = kI3C_IbiRespAckMandatory;
    result                  = I3C_MasterTransferNonBlocking(i3c_index, s_i3c_m_handle, &masterXfer);
    if (kStatus_Success != result)
    {
        return result;
    }

	return kStatus_Success;
}

status_t usb_i3c_list_DAA(uint8_t interface_idex, usb_i3c_param_t param, i3c_master_handle_t *s_i3c_m_handle)
{
	status_t result;
	I3C_Type* i3c_index;

    i3c_index = (interface_idex == 0x0) ? I3C0 : I3C1;

    result = I3C_MasterProcessDAA(i3c_index, g_addressList, param.daa_para.address_num);
    if (result != kStatus_Success)
    {
        return -1;
    }

    i3c_device_info_t *devList;
    uint8_t devIndex;
    uint8_t devCount;
    devList = I3C_MasterGetDeviceListAfterDAA(i3c_index, &devCount);

    for(devIndex = 0; devIndex < devCount; devIndex++)
    {
    	g_deviceList[devIndex][0] = devList[devIndex].vendorID;
    	g_deviceList[devIndex][1] = devList[devIndex].vendorID >> 8U;
    	g_deviceList[devIndex][2] = devList[devIndex].partNumber >> 24U;
    	g_deviceList[devIndex][3] = devList[devIndex].partNumber >> 16U;
    	g_deviceList[devIndex][4] = devList[devIndex].partNumber >> 8U;
    	g_deviceList[devIndex][5] = devList[devIndex].partNumber;
    	g_deviceList[devIndex][6] = devList[devIndex].bcr;
    	g_deviceList[devIndex][7] = devList[devIndex].dcr;

    	if (devList[devIndex].vendorID == param.daa_para.vendor_id)
        {
        	g_slaveAddr = devList[devIndex].dynamicAddr;
            break;
        }
    }

    if (devIndex == devCount)
    {
        return -1;
    }
    return result;
}

void usb_i3c_send_recv_buf(uint8_t *buf, usb_i3c_state_t usb_i3c_state, uint32_t len)
{
	switch(usb_i3c_state)
	{
		case KUSB_I3C_CCCBroadcastSend:
			memcpy(g_i3c_tx_buffer, buf, len);
			break;
		case KUSB_I3C_CCCDirectSend:
			memcpy(g_i3c_tx_buffer, buf, len);
			break;
		case KUSB_I3C_RegDataRead:
			break;
		case KUSB_I3C_DataRead:
			break;
		case KUSB_I3C_RegDataWrite:
		case KUSB_I3C_DataWrite:
			memcpy(g_i3c_tx_buffer, buf, len);
			break;
		case KUSB_I3C_DAAList:
			memcpy(g_addressList, buf, len);
			break;
		case KUSB_I3C_IBI_Register:
			memcpy(g_ibiList, buf, len);
			break;
		default:
			break;
	}
}

usb_i3c_state_t usb_i3c_slave_request_check(uint8_t interface_idex, uint8_t *buf, uint32_t *send_size)
{
	I3C_Type* i3c_index;

    i3c_index = (interface_idex == 0x0) ? I3C0 : I3C1;

	if(g_ibiWonFlag && (I3C_GetIBIType(i3c_index) == kI3C_IbiNormal))
	{
		memcpy(buf, g_ibiUserBuff, g_ibiUserBuffUsed);
		*send_size = g_ibiUserBuffUsed;
		g_ibiWonFlag = false;
		return KUSB_I3C_IBI_Check;
	}
	else if(g_ibiWonFlag && (I3C_GetIBIType(i3c_index) == kI3C_IbiHotJoin))
	{
		g_ibiWonFlag = false;
		return KUSB_I3C_DAAList;
	}
	else
	{
		return KUSB_I3C_NoneProcess;
	}
}

bool usb_i3c_get_transfer_status(status_t *transfer_status)
{
	bool status = false;

	status = g_masterCompletionFlag;
	*transfer_status = g_completionStatus;

	if(g_masterCompletionFlag)
	{
		g_masterCompletionFlag = false;
	}

	if(g_transferCompletionCounts == 0x2)
	{
		status = true;
		*transfer_status = kStatus_Success;
		g_transferCompletionCounts = 0;
	}

	return status;
}

void usb_i3c_clear_status(void)
{
	g_masterCompletionFlag = false;
}

void usb_i3c_get_send_buf(uint8_t *buf, usb_i3c_state_t usb_i3c_state, usb_i3c_param_t param, uint32_t *send_size)
{
	switch(usb_i3c_state)
	{
		case KUSB_I3C_CCCBroadcastSend:
    		buf[0] = 0x4F;
    		buf[1] = 0x4B;
			*send_size = 2;
			break;
		case KUSB_I3C_CCCDirectSend:
			if(param.ccc_code == KUSB_I3C_GETSTATUS)
			{
	            memcpy(buf, g_i3c_rx_buffer, 2);
	            *send_size = 2;
			}
			else
			{
				memcpy(buf, g_i3c_rx_buffer, 1);
				*send_size = 1;
			}
			break;
		case KUSB_I3C_RegDataRead:
		case KUSB_I3C_DataRead:
			memcpy(buf, g_i3c_rx_buffer, param.len);
			memset(g_i3c_rx_buffer, 0, sizeof(g_i3c_rx_buffer));
            *send_size = param.len;
			break;
		case KUSB_I3C_RegDataWrite:
		case KUSB_I3C_DataWrite:
    		buf[0] = 0x4F;
    		buf[1] = 0x4B;
			*send_size = 2;
			break;
		case KUSB_I3C_DAAList:
    		memset(g_addressList, 0, param.daa_para.address_num);
			for(uint32_t devIndex = 0; devIndex < param.daa_para.address_num; devIndex++)
			{
				memcpy(buf, &g_deviceList[devIndex][0], 8);
				buf = (buf + 8);
			}
			*send_size = ((param.daa_para.address_num * 8));
			break;
		default:
			break;
	}
}
