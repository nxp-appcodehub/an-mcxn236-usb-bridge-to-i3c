/*
 * Copyright (c) 2015 - 2016, Freescale Semiconductor, Inc.
 * Copyright 2016 - 2017, 2024 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef I3C_TASK_INTERRUPT_H_
#define I3C_TASK_INTERRUPT_H_

#include "stdint.h"
#include "fsl_common.h"
#include "fsl_i3c.h"


// CCC command code
typedef enum _usb_i3c_ccc
{
    KUSB_I3C_RSTDAA = 0x06U,
	KUSB_I3C_ENTDAA = 0x07U,
    KUSB_I3C_SETDASA = 0x87U,
	KUSB_I3C_ENEC = 0x00U,
    KUSB_I3C_DISENEC = 0x1U,
	KUSB_I3C_GETBCR = 0x8EU,
	KUSB_I3C_GETDCR = 0x8FU,
	KUSB_I3C_GETSTATUS = 0x90U,
} usb_i3c_ccc_t;

/* Define the information relates to abstract control model */
typedef struct _usb_i3c_daa_param
{
	uint8_t static_address;
	uint8_t dynamic_address;
	uint8_t *dda_buf;
	uint8_t address_num;
	uint32_t vendor_id;
} usb_i3c_daa_param_t;

/* Define the information relates to abstract control model */
typedef struct _usb_i3c_param
{
	i3c_bus_type_t  i3c_mode;
	usb_i3c_daa_param_t daa_para;
	usb_i3c_ccc_t ccc_code;
	uint8_t slaveAddress;
	uint8_t regAddress;
    uint8_t *buf;
    uint32_t len;
	uint32_t pp_freq;
	uint32_t od_freq;
	uint32_t i2c_freq;
	uint8_t ibi_address_num;
	i3c_ibi_response_t ibi_type;
} usb_i3c_param_t;

typedef enum _usb_i3c_state
{
    KUSB_I3C_CCCBroadcastSend = 0x0U,
	KUSB_I3C_CCCDirectSend,
	KUSB_I3C_DataWrite,
	KUSB_I3C_RegDataWrite,
    KUSB_I3C_DataRead,
    KUSB_I3C_RegDataRead,
	KUSB_I3C_DAAList,
	KUSB_I3C_IBI_Register,
	KUSB_I3C_IBI_Check,
	KUSB_I3C_Hotjoin,
	KUSB_I3C_Hotjoin_Check,
	KUSB_I3C_NoneProcess,
} usb_i3c_state_t;

void usb_to_i3c_init(uint8_t interface_idex, usb_i3c_param_t param, i3c_master_handle_t *s_i3c_m_handle);
void usb_i3c_ibi_reg(uint8_t interface_idex, usb_i3c_param_t param);
usb_i3c_state_t usb_i3c_slave_request_check(uint8_t interface_idex, uint8_t *buf, uint32_t *send_size);
status_t usb_i3c_ccc_write_direct(uint8_t interface_idex, usb_i3c_param_t param, i3c_master_handle_t *s_i3c_m_handle);
status_t usb_i3c_ccc_write_broadcast(uint8_t interface_idex, usb_i3c_param_t param, i3c_master_handle_t *s_i3c_m_handle);
status_t usb_i3c_reg_data_write(uint8_t interface_idex, usb_i3c_param_t param, i3c_master_handle_t *s_i3c_m_handle);
status_t usb_i3c_data_write(uint8_t interface_idex, usb_i3c_param_t param, i3c_master_handle_t *s_i3c_m_handle);
status_t usb_i3c_reg_data_read(uint8_t interface_idex, usb_i3c_param_t param, i3c_master_handle_t *s_i3c_m_handle);
status_t usb_i3c_data_read(uint8_t interface_idex, usb_i3c_param_t param, i3c_master_handle_t *s_i3c_m_handle);
status_t usb_i3c_list_DAA(uint8_t interface_idex, usb_i3c_param_t param, i3c_master_handle_t *s_i3c_m_handle);
void usb_i3c_get_send_buf(uint8_t *buf, usb_i3c_state_t usb_i3c_state, usb_i3c_param_t param, uint32_t *send_size);
void usb_i3c_send_recv_buf(uint8_t *buf, usb_i3c_state_t usb_i3c_state, uint32_t len);
bool usb_i3c_get_transfer_status(status_t *transfer_status);
void usb_i3c_clear_status(void);
#endif /* I3C_TASK_INTERRUPT_H_ */
