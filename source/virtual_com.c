/*
 * Copyright 2017, 2024 NXP
 * 
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <stdlib.h>
/*${standard_header_anchor}*/
#include "fsl_device_registers.h"
#include "clock_config.h"
#include "fsl_debug_console.h"
#include "board.h"

#include "usb_device_config.h"
#include "usb.h"
#include "usb_device.h"

#include "usb_device_class.h"
#include "usb_device_cdc_acm.h"
#include "usb_device_ch9.h"

#include "usb_device_descriptor.h"
#include "usb_to_i3c.h"
#include "i3c_task_interrupt.h"
#include "fsl_i3c.h"
#include "fsl_ctimer.h"
#include "lpc_ring_buffer.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
/*******************************************************************************
 * Variables
 ******************************************************************************/
extern usb_device_endpoint_struct_t g_cdcVcomDicEndpoints[];
extern usb_device_endpoint_struct_t g_cdcVcomDicEndpoints_2[];
extern usb_device_endpoint_struct_t g_cdcVcomCicEndpoints[];
extern usb_device_endpoint_struct_t g_cdcVcomCicEndpoints_2[];
extern usb_device_class_struct_t g_UsbDeviceCdcVcomConfig[2];
/* Line coding of cdc device */
USB_DMA_INIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
static uint8_t s_lineCoding[USB_DEVICE_CONFIG_CDC_ACM][LINE_CODING_SIZE] = {
    {/* E.g. 0x00,0xC2,0x01,0x00 : 0x0001C200 is 115200 bits per second */
     (LINE_CODING_DTERATE >> 0U) & 0x000000FFU, (LINE_CODING_DTERATE >> 8U) & 0x000000FFU,
     (LINE_CODING_DTERATE >> 16U) & 0x000000FFU, (LINE_CODING_DTERATE >> 24U) & 0x000000FFU, LINE_CODING_CHARFORMAT,
     LINE_CODING_PARITYTYPE, LINE_CODING_DATABITS},
    {/* E.g. 0x00,0xC2,0x01,0x00 : 0x0001C200 is 115200 bits per second */
     (LINE_CODING_DTERATE >> 0U) & 0x000000FFU, (LINE_CODING_DTERATE >> 8U) & 0x000000FFU,
     (LINE_CODING_DTERATE >> 16U) & 0x000000FFU, (LINE_CODING_DTERATE >> 24U) & 0x000000FFU, LINE_CODING_CHARFORMAT,
     LINE_CODING_PARITYTYPE, LINE_CODING_DATABITS},
};

/* Abstract state of cdc device */
USB_DMA_INIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
static uint8_t s_abstractState[USB_DEVICE_CONFIG_CDC_ACM][COMM_FEATURE_DATA_SIZE] = {
    {(STATUS_ABSTRACT_STATE >> 0U) & 0x00FFU, (STATUS_ABSTRACT_STATE >> 8U) & 0x00FFU},
    {(STATUS_ABSTRACT_STATE >> 0U) & 0x00FFU, (STATUS_ABSTRACT_STATE >> 8U) & 0x00FFU},
};

/* Country code of cdc device */
USB_DMA_INIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
static uint8_t s_countryCode[USB_DEVICE_CONFIG_CDC_ACM][COMM_FEATURE_DATA_SIZE] = {
    {(COUNTRY_SETTING >> 0U) & 0x00FFU, (COUNTRY_SETTING >> 8U) & 0x00FFU},
    {(COUNTRY_SETTING >> 0U) & 0x00FFU, (COUNTRY_SETTING >> 8U) & 0x00FFU},
};

/* CDC ACM information */
USB_DMA_INIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
static usb_cdc_acm_info_t s_usbCdcAcmInfo[USB_DEVICE_CONFIG_CDC_ACM] = {
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 0, 0, 0, 0, 0},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 0, 0, 0, 0, 0},
};
/* Data buffer for receiving and sending*/
USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE) static uint8_t s_currRecvBuf[USB_DEVICE_CONFIG_CDC_ACM][DATA_BUFF_SIZE];
USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE) static uint8_t s_currSendBuf[USB_DEVICE_CONFIG_CDC_ACM][4096];
USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE) static uint8_t s_totalRecvBuf[USB_DEVICE_CONFIG_CDC_ACM][4096];
volatile static uint32_t s_recvSize[USB_DEVICE_CONFIG_CDC_ACM] = {0};
volatile static uint32_t s_sendSize[USB_DEVICE_CONFIG_CDC_ACM] = {0};

volatile static usb_device_composite_struct_t *g_deviceComposite;

extern uint8_t s_ringBuf[4096];
extern ring_buffer_t usb_ringBuffer;
extern ctimer_match_config_t matchConfig;
volatile static uint32_t s_frameNum = 0;
volatile static bool i3c_transfer_start = false;

extern i3c_master_handle_t g_i3c0_m_handle;
extern i3c_master_handle_t g_i3c1_m_handle;
uint8_t g_daaListAuto[16] = {0x01, 0x04, 0x01, 0x50};
bool auto_dda_flag = false;
usb_i3c_state_t state;

gpio_pin_config_t gpio_test_h = {
		kGPIO_DigitalOutput,
		1,
};
gpio_pin_config_t gpio_test_l = {
		kGPIO_DigitalOutput,
		0,
};
/*******************************************************************************
 * Code
 ******************************************************************************/
void i3cInit(void)
{
    usb_i3c_param_t param;
    i3c_master_handle_t *s_i3c_m_handle;

    for (uint8_t i = 1; i < USB_DEVICE_CONFIG_CDC_ACM; i++)
    {
    	s_i3c_m_handle = (i == 0x0) ? &g_i3c0_m_handle : &g_i3c1_m_handle;

		param.i2c_freq = 1000000;
		param.od_freq  = 1000000;
		param.pp_freq  = 12500000;

		usb_to_i3c_init(i, param, s_i3c_m_handle);
    }
}

/*!
 * @brief CDC class specific callback function.
 *
 * This function handles the CDC class specific requests.
 *
 * @param handle          The CDC ACM class handle.
 * @param event           The CDC ACM class event type.
 * @param param           The parameter of the class specific request.
 *
 * @return A USB error code or kStatus_USB_Success.
 */
usb_status_t USB_DeviceCdcVcomCallback(class_handle_t handle, uint32_t event, void *param)
{
    uint32_t len;
    uint8_t *uartBitmap;
    usb_cdc_acm_info_t *acmInfo;
    usb_device_cdc_acm_request_param_struct_t *acmReqParam;
    usb_device_endpoint_callback_message_struct_t *epCbParam;
    volatile usb_cdc_vcom_struct_t *vcomInstance;
    usb_status_t error = kStatus_USB_InvalidRequest;
    uint8_t i;
    acmReqParam = (usb_device_cdc_acm_request_param_struct_t *)param;
    epCbParam   = (usb_device_endpoint_callback_message_struct_t *)param;

    for (i = 0; i < USB_DEVICE_CONFIG_CDC_ACM; i++)
    {
        if (handle == g_deviceComposite->cdcVcom[i].cdcAcmHandle)
        {
            break;
        }
    }
    if (i >= USB_DEVICE_CONFIG_CDC_ACM)
    {
        return error;
    }
    vcomInstance = &g_deviceComposite->cdcVcom[i];
    acmInfo      = vcomInstance->usbCdcAcmInfo;
    switch (event)
    {
        case kUSB_DeviceCdcEventSendResponse:
	        {
	            if ((epCbParam->length != 0) && (!(epCbParam->length % vcomInstance->bulkInEndpointMaxPacketSize)))
	            {
	                /* If the last packet is the size of endpoint, then send also zero-ended packet,
	                 ** meaning that we want to inform the host that we do not have any additional
	                 ** data, so it can flush the output.
	                 */
	                error = USB_DeviceCdcAcmSend(handle, vcomInstance->bulkInEndpoint, NULL, 0);
	            }
	            else if ((1 == vcomInstance->attach) && (1 == vcomInstance->startTransactions))
	            {
	                if ((epCbParam->buffer != NULL) || ((epCbParam->buffer == NULL) && (epCbParam->length == 0)))
	                {
	                    /* User: add your own code for send complete event */
	                    /* Schedule buffer for next receive event */
	                    error = USB_DeviceCdcAcmRecv(handle, vcomInstance->bulkOutEndpoint, vcomInstance->currRecvBuf,
	                                                 vcomInstance->bulkOutEndpointMaxPacketSize);
	                }
	            }
	            else
	            {
	            }
	        }
	        break;
        case kUSB_DeviceCdcEventRecvResponse:
	        {
	            if ((1 == vcomInstance->attach) && (1 == vcomInstance->startTransactions))
	            {
	                vcomInstance->recvSize = epCbParam->length;

	                if (!vcomInstance->recvSize)
	                {
	                    /* Schedule buffer for next receive event */
	                    error = USB_DeviceCdcAcmRecv(handle, vcomInstance->bulkOutEndpoint, vcomInstance->currRecvBuf,
	                                                 vcomInstance->bulkOutEndpointMaxPacketSize);
	                }
	            }
	        }
	        break;
        case kUSB_DeviceCdcEventSerialStateNotif:
            ((usb_device_cdc_acm_struct_t *)handle)->hasSentState = 0;
            error                                                 = kStatus_USB_Success;
            break;
        case kUSB_DeviceCdcEventSendEncapsulatedCommand:
            break;
        case kUSB_DeviceCdcEventGetEncapsulatedResponse:
            break;
        case kUSB_DeviceCdcEventSetCommFeature:
            if (USB_DEVICE_CDC_FEATURE_ABSTRACT_STATE == acmReqParam->setupValue)
            {
                if (1 == acmReqParam->isSetup)
                {
                    *(acmReqParam->buffer) = vcomInstance->abstractState;
                    *(acmReqParam->length) = COMM_FEATURE_DATA_SIZE;
                }
                else
                {
                    /* no action, data phase, s_abstractState has been assigned */
                }
                error = kStatus_USB_Success;
            }
            else if (USB_DEVICE_CDC_FEATURE_COUNTRY_SETTING == acmReqParam->setupValue)
            {
                if (1 == acmReqParam->isSetup)
                {
                    *(acmReqParam->buffer) = vcomInstance->countryCode;
                    *(acmReqParam->length) = COMM_FEATURE_DATA_SIZE;
                }
                else
                {
                    /* no action, data phase, s_countryCode has been assigned */
                }
                error = kStatus_USB_Success;
            }
            else
            {
                /* no action, return kStatus_USB_InvalidRequest */
            }
            break;
        case kUSB_DeviceCdcEventGetCommFeature:
            if (USB_DEVICE_CDC_FEATURE_ABSTRACT_STATE == acmReqParam->setupValue)
            {
                *(acmReqParam->buffer) = vcomInstance->abstractState;
                *(acmReqParam->length) = COMM_FEATURE_DATA_SIZE;
                error                  = kStatus_USB_Success;
            }
            else if (USB_DEVICE_CDC_FEATURE_COUNTRY_SETTING == acmReqParam->setupValue)
            {
                *(acmReqParam->buffer) = vcomInstance->countryCode;
                *(acmReqParam->length) = COMM_FEATURE_DATA_SIZE;
                error                  = kStatus_USB_Success;
            }
            else
            {
                /* no action, return kStatus_USB_InvalidRequest */
            }
            break;
        case kUSB_DeviceCdcEventClearCommFeature:
            break;
        case kUSB_DeviceCdcEventGetLineCoding:
            *(acmReqParam->buffer) = vcomInstance->lineCoding;
            *(acmReqParam->length) = LINE_CODING_SIZE;
            error                  = kStatus_USB_Success;
            break;
        case kUSB_DeviceCdcEventSetLineCoding:
        {
            if (1 == acmReqParam->isSetup)
            {
                *(acmReqParam->buffer) = vcomInstance->lineCoding;
                *(acmReqParam->length) = LINE_CODING_SIZE;
            }
            else
            {
                /* no action, data phase, s_lineCoding has been assigned */
            }
            error = kStatus_USB_Success;
        }
        break;
        case kUSB_DeviceCdcEventSetControlLineState:
        {
            error                                  = kStatus_USB_Success;
            vcomInstance->usbCdcAcmInfo->dteStatus = acmReqParam->setupValue;
            /* activate/deactivate Tx carrier */
            if (acmInfo->dteStatus & USB_DEVICE_CDC_CONTROL_SIG_BITMAP_CARRIER_ACTIVATION)
            {
                acmInfo->uartState |= USB_DEVICE_CDC_UART_STATE_TX_CARRIER;
            }
            else
            {
                acmInfo->uartState &= (uint16_t)~USB_DEVICE_CDC_UART_STATE_TX_CARRIER;
            }

            /* activate carrier and DTE. Com port of terminal tool running on PC is open now */
            if (acmInfo->dteStatus & USB_DEVICE_CDC_CONTROL_SIG_BITMAP_DTE_PRESENCE)
            {
                acmInfo->uartState |= USB_DEVICE_CDC_UART_STATE_RX_CARRIER;
            }
            /* Com port of terminal tool running on PC is closed now */
            else
            {
                acmInfo->uartState &= (uint16_t)~USB_DEVICE_CDC_UART_STATE_RX_CARRIER;
            }

            /* Indicates to DCE if DTE is present or not */
            acmInfo->dtePresent = (acmInfo->dteStatus & USB_DEVICE_CDC_CONTROL_SIG_BITMAP_DTE_PRESENCE) ? true : false;

            /* Initialize the serial state buffer */
            acmInfo->serialStateBuf[0] = NOTIF_REQUEST_TYPE;                /* bmRequestType */
            acmInfo->serialStateBuf[1] = USB_DEVICE_CDC_NOTIF_SERIAL_STATE; /* bNotification */
            acmInfo->serialStateBuf[2] = 0x00;                              /* wValue */
            acmInfo->serialStateBuf[3] = 0x00;
            acmInfo->serialStateBuf[4] = 0x00; /* wIndex */
            acmInfo->serialStateBuf[5] = 0x00;
            acmInfo->serialStateBuf[6] = UART_BITMAP_SIZE; /* wLength */
            acmInfo->serialStateBuf[7] = 0x00;
            /* Notify to host the line state */
            acmInfo->serialStateBuf[4] = acmReqParam->interfaceIndex;
            /* Lower byte of UART BITMAP */
            uartBitmap    = (uint8_t *)&acmInfo->serialStateBuf[NOTIF_PACKET_SIZE + UART_BITMAP_SIZE - 2];
            uartBitmap[0] = acmInfo->uartState & 0xFFu;
            uartBitmap[1] = (acmInfo->uartState >> 8) & 0xFFu;
            len           = (uint32_t)(NOTIF_PACKET_SIZE + UART_BITMAP_SIZE);
            if (0 == ((usb_device_cdc_acm_struct_t *)handle)->hasSentState)
            {
                error = USB_DeviceCdcAcmSend(handle, vcomInstance->interruptEndpoint, acmInfo->serialStateBuf, len);
                if (kStatus_USB_Success != error)
                {
                    usb_echo("kUSB_DeviceCdcEventSetControlLineState error!");
                }
                ((usb_device_cdc_acm_struct_t *)handle)->hasSentState = 1;
            }

            /* Update status */
            if (acmInfo->dteStatus & USB_DEVICE_CDC_CONTROL_SIG_BITMAP_CARRIER_ACTIVATION)
            {
                /*  To do: CARRIER_ACTIVATED */
            }
            else
            {
                /* To do: CARRIER_DEACTIVATED */
            }

            if (1U == vcomInstance->attach)
            {
                vcomInstance->startTransactions = 1;
            }
            error = kStatus_USB_Success;
        }
        break;
        case kUSB_DeviceCdcEventSendBreak:
            break;
        default:
            break;
    }

    return error;
}

/*!
 * @brief Application task function.
 *
 * This function runs the task for application.
 *
 * @return None.
 */
void USB_DeviceCdcVcomTask(void)
{
    usb_status_t error = kStatus_USB_Error;
    volatile usb_cdc_vcom_struct_t *vcomInstance;
    bool send_flag = false;
    status_t transfer_status;
    uint32_t send_size;

    usb_i3c_param_t param;
    usb_i3c_state_t i3c_state = KUSB_I3C_NoneProcess;
    i3c_master_handle_t *s_i3c_m_handle;

    for (uint8_t i = 1; i < USB_DEVICE_CONFIG_CDC_ACM; i++)
    {
    	s_i3c_m_handle = (i == 0x0) ? &g_i3c0_m_handle : &g_i3c1_m_handle;
    	vcomInstance = &g_deviceComposite->cdcVcom[i];

    	if ((1 == vcomInstance->attach) && (1 == vcomInstance->startTransactions))
        {
            /* Enter critical can not be added here because of the loop */
            /* endpoint callback length is USB_CANCELLED_TRANSFER_LENGTH (0xFFFFFFFFU) when transfer is canceled */
            if ((0 != vcomInstance->recvSize) && (USB_CANCELLED_TRANSFER_LENGTH != vcomInstance->recvSize))
            {
            	s_frameNum++;

            	if(s_frameNum == 0x1)
            	{
            	    /* Start counting */
            		CTIMER_StartTimer(CTIMER0);
            	}

    			RingBuf_Write(&usb_ringBuffer, vcomInstance->currRecvBuf, vcomInstance->recvSize);
    			vcomInstance->recvSize = 0;
                error = USB_DeviceCdcAcmSend(vcomInstance->cdcAcmHandle, vcomInstance->bulkInEndpoint, NULL, 0);
            }
            if(CTIMER0->TC == matchConfig.matchValue)
            {
            	CTIMER_StopTimer(CTIMER0);
            	CTIMER_Reset(CTIMER0);
            	vcomInstance->totalrecvSize = RingBuf_GetUsedBytes(&usb_ringBuffer);
            	RingBuf_Read(&usb_ringBuffer, vcomInstance->totalRecvBuf, 0x1000);
            	s_frameNum = 0;
            	i3c_transfer_start = true;
            }

    		/* User Code */
            /* endpoint callback length is USB_CANCELLED_TRANSFER_LENGTH (0xFFFFFFFFU) when transfer is canceled */
            if (i3c_transfer_start)
            {
            	i3c_transfer_start = false;

            	/*write without register address 'WN(0x57 0x44) I3C/I2C mode(0x00: I3C mode, 0x01: I2C mode) slaveAddress() len() data()'*/
            	if((vcomInstance->totalRecvBuf[0] == 0x57) && (vcomInstance->totalRecvBuf[1] == 0x44))
            	{
            		memset(&param, 0, sizeof(param));
            		i3c_state = KUSB_I3C_DataWrite;
            		param.i3c_mode = (i3c_bus_type_t)vcomInstance->totalRecvBuf[2];
            		param.slaveAddress = vcomInstance->totalRecvBuf[3];
            		param.len = ((vcomInstance->totalRecvBuf[4] << 0x8) | vcomInstance->totalRecvBuf[5]);
            		usb_i3c_send_recv_buf((vcomInstance->totalRecvBuf + 6), i3c_state, param.len);
					if(param.len)
            		{
						usb_i3c_data_write(i, param, s_i3c_m_handle);
					}
					else
					{
						send_size = 0;
					}
            	}
				/*Write with register address 'WA(0x57 0x41) slaveAddress() regAddress() len() data()'*/
            	else if((vcomInstance->totalRecvBuf[0] == 0x57) && (vcomInstance->totalRecvBuf[1] == 0x41))
            	{
            		memset(&param, 0, sizeof(param));
            		i3c_state = KUSB_I3C_RegDataWrite;
            		param.i3c_mode = (i3c_bus_type_t)vcomInstance->totalRecvBuf[2];
            		param.slaveAddress = vcomInstance->totalRecvBuf[3];
            		param.regAddress = vcomInstance->totalRecvBuf[4];
            		param.len = ((vcomInstance->totalRecvBuf[5] << 0x8) | vcomInstance->totalRecvBuf[6]);
            		usb_i3c_send_recv_buf((vcomInstance->totalRecvBuf + 7), i3c_state, param.len);
            		if(param.len)
					{
            			usb_i3c_reg_data_write(i, param, s_i3c_m_handle);
					}
					else
					{
						send_size = 0;
					}
            	}
            	/*Read without register address 'RN(0x52 0x44) slaveAddress() len()*/
            	else if((vcomInstance->totalRecvBuf[0] == 0x52) && (vcomInstance->totalRecvBuf[1] == 0x44))
            	{
            		memset(&param, 0, sizeof(param));
            		i3c_state = KUSB_I3C_DataRead;
            		param.i3c_mode = (i3c_bus_type_t)vcomInstance->totalRecvBuf[2];
            		param.slaveAddress = vcomInstance->totalRecvBuf[3];
            		param.len = ((vcomInstance->totalRecvBuf[4] << 0x8) | vcomInstance->totalRecvBuf[5]);
            		if(param.len)
            		{
                		usb_i3c_data_read(i, param, s_i3c_m_handle);
            		}
            		else
            		{
            			send_size = 0;
            		}
            	}
            	/*Read with register address 'RA(0x52 0x41) slaveAddress() regAddress() len()*/
            	else if((vcomInstance->totalRecvBuf[0] == 0x52) && (vcomInstance->totalRecvBuf[1] == 0x41))
            	{
            		memset(&param, 0, sizeof(param));
            		i3c_state = KUSB_I3C_RegDataRead;
            		param.i3c_mode = (i3c_bus_type_t)vcomInstance->totalRecvBuf[2];
            		param.slaveAddress = vcomInstance->totalRecvBuf[3];
            		param.regAddress = vcomInstance->totalRecvBuf[4];
            		param.len = ((vcomInstance->totalRecvBuf[5] >> 0x8) | vcomInstance->totalRecvBuf[6]);
            		if(param.len)
					{
            			usb_i3c_reg_data_read(i, param, s_i3c_m_handle);
					}
					else
					{
						send_size = 0;
					}
            	}
            	/*List DAA 'LDAA(0x4C 0x44 0x41 0x41) addressNum() vendorID(u16) dynamicAddress()'*/
            	else if((vcomInstance->totalRecvBuf[0] == 0x4C) && (vcomInstance->totalRecvBuf[1] == 0x44)
            		 && (vcomInstance->totalRecvBuf[2] == 0x41) && (vcomInstance->totalRecvBuf[3] == 0x41))
            	{
            		memset(&param, 0, sizeof(param));
            		i3c_state = KUSB_I3C_DAAList;
            		param.ccc_code = KUSB_I3C_RSTDAA;
            		param.daa_para.address_num = vcomInstance->totalRecvBuf[4];
            		usb_i3c_send_recv_buf((vcomInstance->totalRecvBuf + 5), i3c_state, param.daa_para.address_num);
            		usb_i3c_ccc_write_broadcast(i, param, s_i3c_m_handle);
            	}
            	/*CCC broadcast command send 'CCC(0x43 0x43 0x43 0x42) command(), len, data'*/
            	else if((vcomInstance->totalRecvBuf[0] == 0x43) && (vcomInstance->totalRecvBuf[1] == 0x43)
            		 && (vcomInstance->totalRecvBuf[2] == 0x43) && (vcomInstance->totalRecvBuf[3] == 0x42))//CCC command
            	{
            		memset(&param, 0, sizeof(param));
            		i3c_state = KUSB_I3C_CCCBroadcastSend;
					param.ibi_type = kI3C_IbiRespAckMandatory;
            		param.ccc_code = vcomInstance->totalRecvBuf[4];
            		param.len = vcomInstance->totalRecvBuf[5];
            		usb_i3c_send_recv_buf((vcomInstance->totalRecvBuf + 6), i3c_state, param.len);
            		usb_i3c_ccc_write_broadcast(i, param, s_i3c_m_handle);
            	}
            	/*CCC direct command send 'CCC(0x43 0x43 0x43 0x44) salve, command(), len, data'*/
            	else if((vcomInstance->totalRecvBuf[0] == 0x43) && (vcomInstance->totalRecvBuf[1] == 0x43)
            		 && (vcomInstance->totalRecvBuf[2] == 0x43) && (vcomInstance->totalRecvBuf[3] == 0x44))//CCC command
            	{
            		memset(&param, 0, sizeof(param));
            		i3c_state = KUSB_I3C_CCCDirectSend;
            		param.slaveAddress = vcomInstance->totalRecvBuf[4];
            		param.ccc_code = vcomInstance->totalRecvBuf[5];
            		param.len = vcomInstance->totalRecvBuf[6];
            		usb_i3c_send_recv_buf((vcomInstance->totalRecvBuf + 7), i3c_state, param.len);
            		usb_i3c_ccc_write_direct(i, param, s_i3c_m_handle);
            	}

            	if(i3c_state == KUSB_I3C_DAAList)
            	{
            		/*Wait RSTDAA transfer completion and get deviceList to send*/
            		if(usb_i3c_get_transfer_status(&transfer_status))
            		{
            			usb_i3c_clear_status();
            			if(transfer_status == kStatus_Success)
            			{
            				usb_i3c_list_DAA(i, param, s_i3c_m_handle);
                    		usb_i3c_get_send_buf(vcomInstance->currSendBuf, i3c_state, param, &send_size);
            			}
            			else
            			{
                			send_size = 0;
            			}
            		}
            		else
            		{
            			send_size = 0;
            		}
            	}
            	else
            	{
            		/*Wait transfer completion and get rx_data or tx_data to send*/
            		for(uint32_t time_index = 0; time_index < 3000; time_index++);

            		if(usb_i3c_get_transfer_status(&transfer_status))
            		{
            			usb_i3c_clear_status();
            			if(transfer_status == kStatus_Success)
            			{
                    		usb_i3c_get_send_buf(vcomInstance->currSendBuf, i3c_state, param, &send_size);
            			}
            			else
            			{
                			send_size = 0;
            			}
            		}
            		else
            		{
            			send_size = 0;
            		}
            	}
               	memset(vcomInstance->totalRecvBuf, 0, vcomInstance->totalrecvSize);
    			vcomInstance->recvSize = 0;
    			send_flag = true;
            }

            /* Check IBI type (Normal IBI/Hot-join)*/
            state = usb_i3c_slave_request_check(i, vcomInstance->currSendBuf, &send_size);

            /* check slave IBI and get IBI data */
            if(state == KUSB_I3C_IBI_Check)
            {
            	send_flag = true;
            }
            else if(state == KUSB_I3C_DAAList)
            {
            	//used for hot-join DAA process
    			memset(&param, 0, sizeof(param));
				i3c_state = KUSB_I3C_DAAList;
				param.ccc_code = KUSB_I3C_RSTDAA;
				param.daa_para.address_num = g_daaListAuto[0];
				param.daa_para.vendor_id = (g_daaListAuto[1] | (g_daaListAuto[2] << 8));
				usb_i3c_send_recv_buf(&g_daaListAuto[3], i3c_state, param.daa_para.address_num);
				usb_i3c_ccc_write_broadcast(i, param, s_i3c_m_handle);
				auto_dda_flag = true;
				send_flag = true;
            }

            if(auto_dda_flag)
            {
        		if(usb_i3c_get_transfer_status(&transfer_status))
        		{
        			usb_i3c_clear_status();
        			if(transfer_status == kStatus_Success)
        			{
        				usb_i3c_list_DAA(i, param, s_i3c_m_handle);
                		usb_i3c_get_send_buf(vcomInstance->currSendBuf, i3c_state, param, &send_size);
                		auto_dda_flag = false;
        			}
        			else
        			{
            			send_size = 0;
        			}
        		}
        		else
        		{
        			send_size = 0;
        		}
            }

            if(send_flag)
            {
            	send_flag = false;

            	error = USB_DeviceCdcAcmSend(vcomInstance->cdcAcmHandle, vcomInstance->bulkInEndpoint,
                                             vcomInstance->currSendBuf, send_size);

                if (error != kStatus_USB_Success)
                {
                    /* Failure to send Data Handling code here */
                }
            }
        }
    }
}

/*!
 * @brief Virtual COM device set configuration function.
 *
 * This function sets configuration for CDC class.
 *
 * @param handle The CDC ACM class handle.
 * @param configure The CDC ACM class configure index.
 *
 * @return A USB error code or kStatus_USB_Success.
 */
usb_status_t USB_DeviceCdcVcomSetConfigure(class_handle_t handle, uint8_t configure)
{
    if (USB_COMPOSITE_CONFIGURE_INDEX == configure)
    {
        /*endpoint information for cdc 1*/
        g_deviceComposite->cdcVcom[0].attach = 1;

        g_deviceComposite->cdcVcom[0].interruptEndpoint              = USB_CDC_VCOM_CIC_INTERRUPT_IN_ENDPOINT;
        g_deviceComposite->cdcVcom[0].interruptEndpointMaxPacketSize = g_cdcVcomCicEndpoints[0].maxPacketSize;

        g_deviceComposite->cdcVcom[0].bulkInEndpoint              = USB_CDC_VCOM_DIC_BULK_IN_ENDPOINT;
        g_deviceComposite->cdcVcom[0].bulkInEndpointMaxPacketSize = g_cdcVcomDicEndpoints[0].maxPacketSize;

        g_deviceComposite->cdcVcom[0].bulkOutEndpoint              = USB_CDC_VCOM_DIC_BULK_OUT_ENDPOINT;
        g_deviceComposite->cdcVcom[0].bulkOutEndpointMaxPacketSize = g_cdcVcomDicEndpoints[1].maxPacketSize;

        /* Schedule buffer for receive */
        USB_DeviceCdcAcmRecv(g_deviceComposite->cdcVcom[0].cdcAcmHandle, g_deviceComposite->cdcVcom[0].bulkOutEndpoint,
                             s_currRecvBuf[0], g_deviceComposite->cdcVcom[0].bulkOutEndpointMaxPacketSize);

        /*endpoint information for cdc 2*/
        g_deviceComposite->cdcVcom[1].attach = 1;

        g_deviceComposite->cdcVcom[1].interruptEndpoint              = USB_CDC_VCOM_CIC_INTERRUPT_IN_ENDPOINT_2;
        g_deviceComposite->cdcVcom[1].interruptEndpointMaxPacketSize = g_cdcVcomCicEndpoints_2[0].maxPacketSize;

        g_deviceComposite->cdcVcom[1].bulkInEndpoint              = USB_CDC_VCOM_DIC_BULK_IN_ENDPOINT_2;
        g_deviceComposite->cdcVcom[1].bulkInEndpointMaxPacketSize = g_cdcVcomDicEndpoints_2[0].maxPacketSize;

        g_deviceComposite->cdcVcom[1].bulkOutEndpoint              = USB_CDC_VCOM_DIC_BULK_OUT_ENDPOINT_2;
        g_deviceComposite->cdcVcom[1].bulkOutEndpointMaxPacketSize = g_cdcVcomDicEndpoints_2[1].maxPacketSize;

        /* Schedule buffer for receive */
        USB_DeviceCdcAcmRecv(g_deviceComposite->cdcVcom[1].cdcAcmHandle, g_deviceComposite->cdcVcom[1].bulkOutEndpoint,
                             s_currRecvBuf[1], g_deviceComposite->cdcVcom[1].bulkOutEndpointMaxPacketSize);
    }
    return kStatus_USB_Success;
}

/*!
 * @brief Virtual COM device initialization function.
 *
 * This function initializes the device with the composite device class information.
 *
 * @param deviceComposite The pointer to the composite device structure.
 *
 * @return A USB error code or kStatus_USB_Success.
 */
usb_status_t USB_DeviceCdcVcomInit(usb_device_composite_struct_t *deviceComposite)
{
    g_deviceComposite = deviceComposite;
    for (uint8_t i = 0; i < USB_DEVICE_CONFIG_CDC_ACM; i++)
    {
        g_deviceComposite->cdcVcom[i].lineCoding    = (uint8_t *)&s_lineCoding[i];
        g_deviceComposite->cdcVcom[i].abstractState = (uint8_t *)&s_abstractState[i];
        g_deviceComposite->cdcVcom[i].countryCode   = (uint8_t *)&s_countryCode[i];
        g_deviceComposite->cdcVcom[i].usbCdcAcmInfo = &s_usbCdcAcmInfo[i];
        g_deviceComposite->cdcVcom[i].currRecvBuf   = (uint8_t *)&s_currRecvBuf[i][0];
        ;
        g_deviceComposite->cdcVcom[i].currSendBuf  = (uint8_t *)&s_currSendBuf[i][0];
        g_deviceComposite->cdcVcom[i].totalRecvBuf = (uint8_t *)&s_totalRecvBuf[i][0];
    }
    return kStatus_USB_Success;
}
