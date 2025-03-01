/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2015-2020. All rights reserved.
 * Description: hifi USB mailbox
 * Create: 2015
 *
 * This software is distributed under the terms of the GNU General
 * Public License ("GPL") as published by the Free Software Foundation,
 * either version 2 of that License or (at your option) any later version.
 */

#ifndef _HIFI_USB_MAIL_H
#define _HIFI_USB_MAIL_H

#include <linux/kernel.h>
#include <linux/usb/ch9.h>
#include "drv_mailbox_msg.h"
#include "dsp_misc.h"

/*
 * hcd message
 */
enum hifi_usb_opcode {
	AP_HIFI_USB_NOP = 0,
	AP_HIFI_USB_HUB_CONTROL,
	AP_HIFI_USB_ALLOC_DEV,
	AP_HIFI_USB_FREE_DEV,
	AP_HIFI_USB_ENABLE_DEV,
	AP_HIFI_USB_RESET_DEV,
	AP_HIFI_USB_ADDRESS_DEV,
	AP_HIFI_USB_UPDATE_DEV,
	AP_HIFI_USB_URB_ENQUEUE, /* AP to HIFI */

	HIFI_AP_USB_URB_COMPL, /* HIFI to AP */

	AP_HIFI_USB_URB_DEQUEUE,
	AP_HIFI_USB_ADD_ENDPOINT,
	AP_HIFI_USB_DROP_ENDPOINT,
	AP_HIFI_USB_CHECK_BANDWIDTH,
	AP_HIFI_USB_RESET_BANDWIDTH,

	HIFI_USB_HUB_STATUS_CHANGE,
	HIFI_AP_USB_HCD_DIED,
	AP_HIFI_USB_HID_KEY_PRESSED,
	HIFI_USB_OP_CODE_MAX = 0xff,
};

enum hifi_usb_test_msg_type {
	AP_HIFI_USB_TEST_MESG_START,
	AP_HIFI_USB_TEST_ISOC,
	AP_HIFI_USB_TEST_FAULTINJECT,
	AP_HIFI_USB_TEST_HC_DIED,
	AP_HIFI_USB_TEST_SR,
	AP_HIFI_USB_TEST_DEBUG,
	AP_HIFI_USB_TEST_REG,
	AP_HIFI_USB_TEST_MESG_END = 0xF,
};

#define HIFI_USB_MSG_TIMEOUT (2 * HZ)
#define HIFI_USB_MSG_MAX_LEN 26
#define HIFI_USB_MSG_HDR_LEN 12
#define HIFI_USB_MSG_MAX_DATA_LEN (HIFI_USB_MSG_MAX_LEN - HIFI_USB_MSG_HDR_LEN)

struct hifi_usb_faultinject_data {
	__u8 fault;
	__u8 point;
	__u32 data;
};

struct hifi_usb_test_sr_data {
#define USB_TEST_SUSPEND        0
#define USB_TEST_RESUME         1
#define USB_TEST_INQUIRE_SR_STATE	2
#define USB_TEST_SR_STATE_SUSPENDED	0
#define USB_TEST_SR_STATE_RUNNING	1
#define USB_TEST_SR_STATE_UNKNOWN	0xff
	union {
		__u8 action;
		__u8 state;
	};
};

struct hifi_usb_test_debug_print_data {
#define USB_PRINT_DEBUG_MASK (0xF)
	union {
		struct {
			__u32 usbcore_print_dbg : 1;
			__u32 xhci_print_dbg : 1;
		};
		__u32 flags;
	};
};

struct hifi_usb_test_reg_data {
#define HIFI_USB_TEST_REG_READ 0x4
#define HIFI_USB_TEST_REG_WRITE 0x8
	__u32 flags;
	__u32 addr;
	__u32 val;
};

struct hifi_usb_test_msg {
	__u16 msg_id; /* should be set as ID_AP_HIFI_USB_TEST */
	__u16 reserved;

	__u16 msg_type;
	__u16 data_len;

	union {
		__u8 data[16];
		struct hifi_usb_faultinject_data faultinject;
		struct hifi_usb_test_sr_data sr_control;
		struct hifi_usb_test_debug_print_data debug;
		struct hifi_usb_test_reg_data reg;
	};
};


/* for hub_control */
struct usb_hub_control_data {
	__u16 type_req;
	__u16 w_value;
	__u16 w_index;
	__u16 w_length;
	__u8 buf[4];
}; /* __attribute__ ((packed)) */

/*
 * for alloc_dev, free_dev
 * enable_device, reset_Device, check_bandwidth, reset_bandwidth
 */
struct usb_dev_control_data {
	__u16 slot_id;
	__u16 speed; /* enum usb_device_speed */
	__u16 state; /* enum usb_device_state */
	__u16 ep0_mps;
}; /* __attribute__ ((packed)) */

/* for add_endpoint and drop_endpoint */
struct usb_ep_control_data {
	struct usb_endpoint_descriptor ep_desc;
	__u16 slot_id;
}; /* __attribute__ ((packed)) */

/*
 * This is the data structure in share memory.
 */
struct hifi_urb_msg {
	__s32 status; /* (return) non-ISO status */
	__u32 transfer_flags; /* (in) URB_SHORT_NOT_OK | ... */
	__u32 transfer_buffer_length; /* (in) data buffer length */
	__u32 actual_length; /* (return) actual transfer length */
	__s32 interval; /* (modify) transfer interval */
	__u32 reserved;
	__u32 reserved_2;
	__u32 reserved_3;

	union {
		struct {
			struct usb_ctrlrequest ctrlrequest;
			__u8 ctrldata[0];
		};
		__u8 buf[0];
	};
}; /* __attribute__ ((packed)) */

/*
 * for urb transfer.
 */
struct usb_urb_data {
	__u32 urb_addr_lo;
	__u32 urb_addr_hi;
	__u32 pipe;
	__u16 slot_id;
} __attribute__ ((packed));

/*
 * for hub status change, form hifi to AP.
 */
struct usb_port_status_change_data {
	__u32 port_bitmap;
};

/*
 * for hifi usb update device.
 */
struct usb_update_device_data {
	struct usb_ext_cap_descriptor ext_cap;
	__u16 slot_id;
};

/*
 * This is the MAIL.
 */
struct hifi_usb_mesg_header {
	__u16 msg_id;
	__u16 reserved;
};

struct hifi_usb_op_msg {
	__u16 msg_id;
	__u16 reserved;

	__u16 msg_type;
	__u16 data_len;

	__s32 result;

	union {
		__u8 data[HIFI_USB_MSG_MAX_DATA_LEN];
		struct usb_hub_control_data hub_ctrl;
		struct usb_dev_control_data dev_ctrl;
		struct usb_ep_control_data ep_ctrl;
		struct usb_urb_data urb;
		struct usb_port_status_change_data port_data;
		struct usb_update_device_data bos_data;
	};
}; /* __attribute__ ((packed)) */

struct hifi_usb_runstop_msg {
	u16 mesg_id;
	u16 reserved;
	u8 runstop;
	s32 result;
};

struct hifi_usb_init_msg {
	u16 mesg_id;
	u16 reserved;
	s32 result;
};

#ifdef CONFIG_HIFI_MAILBOX
int hifi_usb_mailbox_init(void);
void hifi_usb_mailbox_exit(void);
int hifi_usb_send_mailbox(const struct hifi_usb_op_msg *op_msg, unsigned int len);
#else
static inline int hifi_usb_mailbox_init(void)
{
	return -ENOENT;
}
static inline void hifi_usb_mailbox_exit(void) {}
static inline int hifi_usb_send_mailbox(const struct hifi_usb_op_msg *op_msg, unsigned int len)
{
	return -ENOENT;
}
#endif

#endif
