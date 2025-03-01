/*
 * Huawei Touchscreen Driver
 *
 * Copyright (c) 2012-2050 Huawei Technologies Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/string.h>
#include <linux/spi/spi.h>
#include "huawei_thp.h"
#include <linux/time.h>
#include <linux/syscalls.h>

#ifdef CONFIG_HUAWEI_DUBAI
#include <huawei_platform/log/hwlog_kernel.h>
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define SSL_IC_NAME "ssl_thp"
#define THP_SSL_DEV_NODE_NAME "ssl_thp"

#define MXT680U2_FAMILY_ID 166
#define MXT680U2_VARIANT_ID 22

#define MXT_WAKEUP_TIME 10
#define BIT_SHIFT_8 8
#define T144_ACTIVE_TIMER_OFFSET 8
#define T144_ACTIVE_DOZING_READ_LEN 8
// opcodes
#define SPI_WRITE_REQ 0x01
#define SPI_WRITE_OK 0x81
#define SPI_WRITE_FAIL 0x41
#define SPI_READ_REQ 0x02
#define SPI_READ_OK 0x82
#define SPI_READ_FAIL 0x42
#define SPI_INVALID_REQ 0x04
#define SPI_INVALID_CRC  0x08
#define SPI_APP_HEADER_LEN 6
#define SPI_BOOTL_HEADER_LEN 2
#define T117_BYTES_READ_LIMIT 1505 /* 7 x 215 (T117 size) */
#define WRITE_DUMMY_BYTE 400
#define READ_DUMMY_BYTE 400
#define FRAME_READ_DUMMY_BYTE 400
#define NUMBER_OF_HEADER 2
#define SPI_APP_DATA_MAX_LEN 64
#define SPI_APP_BUF_SIZE_WRITE (SPI_APP_HEADER_LEN + SPI_APP_DATA_MAX_LEN + \
				WRITE_DUMMY_BYTE)
#define SPI_APP_BUF_SIZE_READ (NUMBER_OF_HEADER * SPI_APP_HEADER_LEN + \
				T117_BYTES_READ_LIMIT + FRAME_READ_DUMMY_BYTE)
/* for tp ic mxt3662 */
#define MXT3562_FAMILY_ID 166
#define MXT3562_VARIANT_ID 24
#define MXT3662_FAMILY_ID 169
#define MXT3662_VARIANT_ID 01
#define WRITE_DUMMY_BYTE_FOR_MXT3662 600
#define READ_DUMMY_BYTE_FOR_MXT3662 600
#define BOOTLOADER_READ_LEN_FOR_MXT3662 40
#define MXT_OBJECT_START 0x07 /* after struct mxt_info */
#define MXT_INFO_CHECKSUM_SIZE 3    /* after list of struct mxt_object */
#define GETPDSDATA_COMMAND 0x81
#define MXT_T6_DIAGNOSTIC_OFFSET 0x05
#define READ_ID_RETRY_TIMES 3

#define SSL_T117_ADDR 117
#define SSL_T7_ADDR 7
#define SSL_T6_ADDR 6
#define SSL_T37_ADDR 37
#define SSL_T118_ADDR 118
#define SSL_T24_ADDR 24
#define SSL_T144_ADDR 144
#define SSL_T8_ADDR 8
#define SSL_T145_ADDR 145

#define THP_BOOTLOADER_SPI_FREQ_HZ 100000U
#define BOOTLOADER_READ_CNT 2
#define BOOTLOADER_READ_LEN 1
#define BOOTLOADER_CRC_ERROR_CODE 0x60
#define BOOTLOADER_WAITING_CMD_MODE_0 0xC0
#define BOOTLOADER_WAITING_CMD_MODE_1 0xE0

#define PDS_HEADER_OFFSET 4
#define PROJECTID_ARR_LEN 32
#define SSL_T7_COMMAMD_LEN 2
#define T24_GESTURE_ON_MASK 0x03
#define T117_HEADER_STATUS_OFFSET 0x05
#define T117_GESTURE_EVENT 0x40
#define SEND_COMMAND_RETRY 3
#define SSL_WAIT_FOR_SPI_BUS_RESUMED_DELAY 20
#define SSL_WAIT_FOR_SPI_BUS_READ_DELAY 5

#define MXT_T145_SCREEN_OFF_CMD 1
#define SCREEN_OFF_MODE_SCAN_RATE 50
#define SCREEN_OFF_SCAN_MODE 2
#define SCREEN_OFF_MCT_SCAN_MODE 3
#define T8_MEASIDLEDEF_OFFSET 11
#define MXT_T145_CMD_OFFSET 9

#define MOVE_8BIT 8
#define MOVE_16BIT 16
#define MOVE_24BIT 24
#define SSL_SYNC_DATA_REG_DEFAULT_ADDR 238
#define CHIP_IDENTIFIED_START_ADDR 0
#define OVERRIDE_LIMIT_VALUE 0

struct mxt_info {
	u8 family_id;
	u8 variant_id;
	u8 version;
	u8 build;
	u8 matrix_xsize;
	u8 matrix_ysize;
	u8 object_num;
};

struct mxt_object {
	u8 type;
	u16 start_address;
	u8 size_minus_one;
	u8 instances_minus_one;
	u8 num_report_ids;
} __packed;

enum ssl_ic_type {
	MXT680U2,
	MXT3662 = 1,
};

static uint16_t t117_address;
static uint16_t t7_address;
static uint16_t t6_address;
static uint16_t t37_address;
static uint16_t t118_address;
static uint16_t t24_address;
static uint16_t t144_address;
static uint16_t t8_address;
static uint16_t t145_address;

static u8 is_bootloader_mode;
static unsigned int g_thp_udfp_status;

static int touch_driver_wakeup_gesture_en_switch(
	struct thp_device *tdev, u8 switch_value);
static void touch_driver_exit(struct thp_device *tdev);

static u8 get_crc8_iter(u8 crc, u8 data)
{
	static const u8 crc_inter_check = 0x8c;
	u8 index = 8;
	u8 fb;

	do {
		fb = (crc ^ data) & 0x01;
		data >>= 1;
		crc >>= 1;
		if (fb)
			crc ^= crc_inter_check;
	} while (--index);
	return crc;
}

static u8 get_header_crc(u8 *p_msg)
{
	u8 calc_crc = 0;
	int i = 0;

	if (p_msg == NULL) {
		thp_log_err("%s: point null\n", __func__);
		return -EINVAL;
	}
	for (; i < (SPI_APP_HEADER_LEN - 1); i++)
		calc_crc = get_crc8_iter(calc_crc, p_msg[i]);

	return calc_crc;
}

static void spi_prepare_header(u8 *header, u8 opcode,
	u16 start_register, u16 count)
{
	int index = 0;

	if (header == NULL) {
		thp_log_err("%s: point null\n", __func__);
		return;
	}

	header[index++] = opcode;
	header[index++] = start_register & 0xff;
	header[index++] = start_register >> MOVE_8BIT;
	header[index++] = count & 0xff;
	header[index++] = count >> MOVE_8BIT;
	header[index++] = get_header_crc(header);
}

/*
 * The BL protocol header is always 0x00, 0x00.
 * The LSBit of the LSByte is zero to
 * indicate a write. For frame data this is also
 * the case and frame size is embedded
 * in the frame data and is the first 2 bytes of the payload
 */

#define BOOT_XFER_DELAY 18
#define BOOT_HEAD_LEN 4
#define READ_BOOT_HEAD_COUNT 5
static int bootloader_one_byte_transfer(
	struct thp_device *tdev, const char * const tx_buf,
	char * const rx_buf, const unsigned int buf_len)
{
	struct spi_message msg;
	struct spi_device *sdev = NULL;
	struct spi_transfer *xfer = NULL;
	int rc;
	int i;

	thp_log_info("%s call\n", __func__);
	if ((!tdev) || (!tdev->thp_core) || (!tdev->thp_core->sdev) ||
		(!tx_buf) || (!rx_buf) || (!buf_len)) {
		thp_log_err("%s: point null\n", __func__);
		return -EINVAL;
	}
	sdev = tdev->thp_core->sdev;
	if (tdev->thp_core->suspended) {
		thp_log_err("%s: suspended\n", __func__);
		return 0;
	}
	xfer = kzalloc(buf_len * sizeof(*xfer), GFP_KERNEL);
	if (xfer == NULL) {
		thp_log_err("%s: kzalloc failed\n", __func__);
		return -EIO;
	}
	spi_message_init(&msg);
	for (i = 0; i < buf_len; i++) {
		xfer[i].tx_buf = &tx_buf[i];
		xfer[i].rx_buf = &rx_buf[i];
		/* one byte len */
		xfer[i].len = 1;
		xfer[i].delay_usecs = tdev->timing_config.spi_transfer_delay_us;
		spi_message_add_tail(&xfer[i], &msg);
	}
	rc = thp_bus_lock();
	if (rc < 0) {
		thp_log_err("%s:get lock failed:%d\n", __func__, rc);
		kfree(xfer);
		return rc;
	}
	rc = thp_spi_sync(sdev, &msg);
	thp_bus_unlock();
	kfree(xfer);
	return rc;
}

static int touch_driver_bootloader_write(struct thp_device *tdev,
	struct spi_device *client, unsigned char const *buf, int count)
{
	unsigned char op_header[] = { 0U, 0U };
	int ret_val;
	struct spi_message spimsg;
	struct spi_transfer spitr;

	if ((tdev == NULL) || (client == NULL) || (buf == NULL) ||
		(tdev->tx_buff == NULL) || (tdev->rx_buff == NULL) ||
		(count == 0)) {
		thp_log_err("%s: point null\n", __func__);
		return -EINVAL;
	}
	spi_message_init(&spimsg);

	memset(&spitr, 0, sizeof(spitr));
	spitr.tx_buf = tdev->tx_buff;
	spitr.rx_buf = tdev->rx_buff;
	spitr.len = sizeof(op_header) + count;
	memcpy(tdev->tx_buff, op_header, sizeof(op_header));
	memcpy(tdev->tx_buff + sizeof(op_header), buf, count);
	spi_message_add_tail(&spitr, &spimsg);
	ret_val = thp_bus_lock();
	if (ret_val) {
		thp_log_err("%s: get lock failed\n", __func__);
		return -EINVAL;
	}
	ret_val = spi_sync(client, &spimsg);
	thp_bus_unlock();
	if (ret_val < 0)
		thp_log_err("%s:Error reading from spi %d\n",
			__func__, ret_val);
	return ret_val;
}

static int touch_driver_bootloader_read(struct thp_device *tdev,
	struct spi_device *client, unsigned char *buf, int count)
{
	/* Require SPI LSB 0x01 for read op */
	static const char op_header[] = { 0x01, 0x00 };
	int ret_val;
	struct spi_message spi_msg;
	struct spi_transfer transfer;

	if ((tdev == NULL) || (client == NULL) || (buf == NULL) ||
		(tdev->tx_buff == NULL) || (tdev->rx_buff == NULL)) {
		thp_log_err("%s: point null\n", __func__);
		return -EINVAL;
	}
	spi_message_init(&spi_msg);
	memset(&transfer, 0, sizeof(struct spi_transfer));

	transfer.tx_buf = tdev->tx_buff;
	transfer.rx_buf = tdev->rx_buff;
	transfer.len = sizeof(op_header) + count;
	memcpy(tdev->tx_buff, op_header, sizeof(op_header));
	spi_message_add_tail(&transfer, &spi_msg);
	ret_val = thp_bus_lock();
	if (ret_val) {
		thp_log_err("%s: get lock failed\n", __func__);
		return -EINVAL;
	}
	ret_val = spi_sync(client, &spi_msg);
	thp_bus_unlock();
	if (ret_val < 0) {
		thp_log_err("%s: Error reading from spi ret = %d\n",
			__func__, ret_val);
		return ret_val;
	}
	memcpy(buf, tdev->rx_buff + sizeof(op_header), count);
	return ret_val;
}

static int touch_driver_wait_for_chg_is_low(struct thp_device *tdev)
{
	int ret_val;
	int count_timeout = 100; // 100 msec

	if (tdev == NULL) {
		thp_log_err("%s: tdev null\n", __func__);
		return -EINVAL;
	}
	while ((gpio_get_value(tdev->gpios->irq_gpio) != 0) &&
		(count_timeout > 0)) {
		count_timeout--;
		mdelay(1); /* 1 msec */
	}
	ret_val = (count_timeout > 0) ? 0 : -1;
	return ret_val;
}

#define PDS_DELIMITER_BYTE_1 0xDE
#define PDS_DELIMITER_BYTE_2 0xAD
#define PDS_DELIMITER_BYTE_3 0xAD
#define PDS_DELIMITER_BYTE_4 0xDA
#define BOOTLOADER_STATUS_BYTE_LEN 8
static int touch_driver_check_bootloader_status(struct thp_device *tdev,
	u8 const *status_byte, int status_byte_size)
{
	int i;
	/* 4:project id delimiter size */
	int check_head_offset = (status_byte_size - 4 - THP_PROJECT_ID_LEN);

	if (status_byte_size < BOOTLOADER_STATUS_BYTE_LEN + THP_PROJECT_ID_LEN)
		return -EINVAL;
	/* 0x60: CRC error  0xE0: waiting cmd mode */
	if ((status_byte[0] == 0xE0) || (status_byte[0] == 0x60)) {
		thp_log_info("bootloader mode found status byte is 0x%x\n",
			status_byte[0]);
		for (i = 0; i < check_head_offset; i++) {
			/* 1~4:project id delimiter offset */
			if ((status_byte[i] == PDS_DELIMITER_BYTE_1) &&
				(status_byte[i + 1] == PDS_DELIMITER_BYTE_2) &&
				(status_byte[i + 2] == PDS_DELIMITER_BYTE_3) &&
				(status_byte[i + 3] == PDS_DELIMITER_BYTE_4)) {
				memcpy(&tdev->thp_core->project_id,
					(status_byte + i + 4),
					THP_PROJECT_ID_LEN);
				tdev->thp_core->project_id[THP_PROJECT_ID_LEN] =
					'\0';
				thp_log_info("projectid %s\n",
					tdev->thp_core->project_id);
				break;
			}
		}
		return 0;
	}
	return -EINVAL;
}

static int touch_driver_check_is_bootloader_for_3662(
	struct thp_device *tdev)
{
	u8 status_byte[BOOTLOADER_READ_LEN_FOR_MXT3662] = {0};
	int ret_value = 0;
	int bootloader_read_cnt = BOOTLOADER_READ_CNT;
	int max_thp_spi_clock = tdev->thp_core->sdev->max_speed_hz;
	int ret;
	/* option header command ,0x01 0x00 */
	static const char op_header[] = { 0x01, 0x00 };

	thp_log_info("%s:max thp spi clock= %d\n", __func__, max_thp_spi_clock);
	tdev->thp_core->sdev->max_speed_hz = THP_BOOTLOADER_SPI_FREQ_HZ;
	thp_log_info("%s:set max thp spi clock= %d\n",
		__func__, tdev->thp_core->sdev->max_speed_hz);
	do {
		thp_log_info("%s:call bootloader_one_byte_transfer\n",
			__func__);
		memcpy(tdev->tx_buff, op_header, sizeof(op_header));
		ret = bootloader_one_byte_transfer(tdev, tdev->tx_buff,
			tdev->rx_buff, BOOTLOADER_READ_LEN_FOR_MXT3662);
		if (ret)
			thp_log_err("%s: transfer failed\n", __func__);
		memcpy(status_byte, tdev->rx_buff,
			BOOTLOADER_READ_LEN_FOR_MXT3662);
		thp_log_info("%s:status_byte %*ph\n", __func__,
			BOOTLOADER_READ_LEN_FOR_MXT3662,
			status_byte);
		ret = touch_driver_check_bootloader_status(tdev, status_byte,
			BOOTLOADER_READ_LEN_FOR_MXT3662);
		if (ret == 0) {
			/* it is bootloader mode ,return 1 */
			ret_value = 1;
			break;
		}
	} while (bootloader_read_cnt-- > 0);
	tdev->thp_core->sdev->max_speed_hz = max_thp_spi_clock;
	thp_log_info("%s:call end ret_value = %d\n", __func__, ret_value);
	return ret_value;
}

static int touch_driver_check_is_bootloader(struct thp_device *tdev)
{
	u8 status_byte = 0;
	int ret_value = 0;
	int bootloader_read_cnt = BOOTLOADER_READ_CNT;
	int max_thp_spi_clock;
	int ret;
	struct thp_core_data *cd = NULL;

	if (tdev == NULL || tdev->thp_core == NULL ||
		(tdev->thp_core->sdev == NULL)) {
		thp_log_err("%s: tdev null\n", __func__);
		return -EINVAL;
	}
	cd = tdev->thp_core;
	if (cd->support_vendor_ic_type == MXT3662)
		return touch_driver_check_is_bootloader_for_3662(tdev);

	max_thp_spi_clock = tdev->thp_core->sdev->max_speed_hz;
	thp_log_info("max thp spi clock= %d\n", max_thp_spi_clock);
	tdev->thp_core->sdev->max_speed_hz = THP_BOOTLOADER_SPI_FREQ_HZ;
	thp_log_info("set max thp spi clock= %d\n",
		tdev->thp_core->sdev->max_speed_hz);

	do {
		if (touch_driver_wait_for_chg_is_low(tdev) < 0) {
			thp_log_err("%s:CHG doesn't change to LOW\n", __func__);
		} else {
			ret = touch_driver_bootloader_read(tdev,
				tdev->thp_core->sdev,
				&status_byte, BOOTLOADER_READ_LEN);
			if (ret)
				thp_log_err("%s: bootloader_read failed\n",
					__func__);
			thp_log_info("the bootloader status byte is %d\n",
				status_byte);
			if ((status_byte == BOOTLOADER_CRC_ERROR_CODE) ||
				(status_byte ==
					BOOTLOADER_WAITING_CMD_MODE_0) ||
				(status_byte ==
					BOOTLOADER_WAITING_CMD_MODE_1)) {
				/* 0x60: CRC error  0xE0: waiting cmd mode */
				thp_log_info(
					"bootloader mode found -status is 0x%x\n",
					status_byte);
				ret_value = 1;
				break;
			}
		}
	} while (bootloader_read_cnt-- > 0);
	tdev->thp_core->sdev->max_speed_hz = max_thp_spi_clock;
	return ret_value;
}

static int touch_driver_read_reg(struct thp_device *tdev,
	struct spi_device *client, u16 start_register, u16 len, u8 *val)
{
	u8 attempt = 0;
	int ret_val;
	int i;
	int dummy_byte;
	int dummy_offset = 0;
	u8 *rx_buf = NULL;
	u8 *tx_buf = NULL;
	struct spi_message  spi_msg;
	struct spi_transfer transfer;
	struct thp_core_data *cd = NULL;

	if ((tdev == NULL) || (client == NULL) || (val == NULL) ||
		(tdev->rx_buff == NULL) || (tdev->tx_buff == NULL) ||
		(tdev->thp_core == NULL)) {
		thp_log_err("%s: tdev or client or val null\n", __func__);
		return -EINVAL;
	}
	cd = tdev->thp_core;

	rx_buf = tdev->rx_buff;
	tx_buf = tdev->tx_buff;

	if (cd->support_vendor_ic_type == MXT3662) {
		dummy_byte = READ_DUMMY_BYTE_FOR_MXT3662;
	} else {
		if (len == T117_BYTES_READ_LIMIT)
			dummy_byte = FRAME_READ_DUMMY_BYTE;
		else
			dummy_byte = READ_DUMMY_BYTE;
	}
	do {
		attempt++;
		if (attempt > 1) {
			/* 5 is spi write max Retries */
			if (attempt > 5) {
				thp_log_err("%s:Too many Retries\n", __func__);
				return -EIO;
			}
			if (len < T117_BYTES_READ_LIMIT)
				/* SPI2 need some delay time */
				mdelay(MXT_WAKEUP_TIME);
			else
				return  -EINVAL;
		}

		memset(tx_buf, 0xFF, (NUMBER_OF_HEADER * SPI_APP_HEADER_LEN +
			dummy_byte));
		spi_prepare_header(tx_buf, SPI_READ_REQ, start_register, len);

		spi_message_init(&spi_msg);
		memset(&transfer, 0,  sizeof(struct spi_transfer));
		transfer.tx_buf = tx_buf;
		transfer.rx_buf = rx_buf;
		transfer.len = NUMBER_OF_HEADER * SPI_APP_HEADER_LEN +
			dummy_byte + len;
		spi_message_add_tail(&transfer, &spi_msg);
		ret_val = spi_sync(client, &spi_msg);
		if (ret_val < 0) {
			thp_log_err("%s: Error reading from spi ret = %d\n",
				__func__, ret_val);
			return ret_val;
		}
		for (i = 0; i < dummy_byte; i++) {
			if (rx_buf[SPI_APP_HEADER_LEN + i] == SPI_READ_OK) {
				dummy_offset = i + SPI_APP_HEADER_LEN;
				if (dummy_offset > READ_DUMMY_BYTE / 2)
					thp_log_info(
						"Found read dummy offset %d\n",
						dummy_offset);
				break;
			}
		}
		if (dummy_offset == 0) {
			thp_log_err("cannot find dummy byte offset %u\n",
				start_register);
			if (len == T117_BYTES_READ_LIMIT)
				return -EINVAL;
		} else {
			/* tx[1] or rx[1] :LSB of start register address
			 * tx[2] or rx[2] :MSB of start register address
			 * tx[3] or rx[3] :LSB of len
			 * tx[4] or rx[4] :MSB of len
			 */
			if ((tx_buf[1] != rx_buf[1 + dummy_offset]) ||
				(tx_buf[2] != rx_buf[2 + dummy_offset])) {
				thp_log_err("%s: Unexpected offset %u != %u\n",
					__func__,
					(rx_buf[1 + dummy_offset] |
					(rx_buf[2 + dummy_offset] << 8)),
					start_register);
				/* normal register read retry */
				if (len < T117_BYTES_READ_LIMIT)
					dummy_offset = 0;
				else   /* T117 should not retry */
					return -EINVAL;
			} else if ((tx_buf[3] != rx_buf[3 + dummy_offset]) ||
					(tx_buf[4] != rx_buf[4 +
					dummy_offset])) {
				thp_log_err(
					"%s: Unexpected count %d != %d reading from spi\n",
					__func__, rx_buf[3 + dummy_offset] |
					(rx_buf[4 + dummy_offset] << 8), len);
				/* normal register read retry */
				if (len < T117_BYTES_READ_LIMIT)
					dummy_offset = 0;
				else   /* T117 should not retry */
					return -EINVAL;
			}
		}
	} while ((get_header_crc(rx_buf + dummy_offset) !=
		rx_buf[SPI_APP_HEADER_LEN - 1 + dummy_offset]) ||
		(dummy_offset == 0));
	memcpy(val, rx_buf + SPI_APP_HEADER_LEN + dummy_offset, len);
	return 0;
}

static int touch_driver_write_reg(struct thp_device *tdev,
	struct spi_device *client, u16 start_register,
	u16 len, const u8 *val)
{
	int i;
	int ret_val;
	int attempt = 0;
	struct spi_message  spi_msg;
	struct spi_transfer transfer;

	u8 *rx_buf = NULL;
	u8 *tx_buf = NULL;
	int dummy_byte = WRITE_DUMMY_BYTE;
	int dummy_offset = 0;
	struct thp_core_data *cd = NULL;

	if ((tdev == NULL) || (client == NULL) || (val == NULL) ||
		(tdev->rx_buff == NULL) || (tdev->tx_buff == NULL) ||
		(tdev->thp_core == NULL)) {
		thp_log_err("%s:point is  null\n", __func__);
		return -EINVAL;
	}
	cd = tdev->thp_core;
	if (cd->support_vendor_ic_type == MXT3662)
		dummy_byte = WRITE_DUMMY_BYTE_FOR_MXT3662;

	rx_buf = tdev->rx_buff;
	tx_buf = tdev->tx_buff;

	do {
		attempt++;
		if (attempt > 1) {
			/* 5 is spi write max Retries */
			if (attempt > 5) {
				thp_log_err("Too many spi write Retries\n");
				return -EIO;
			}
			thp_log_info("%s: retry %d after write fail\n",
				__func__, attempt - 1);
			mdelay(MXT_WAKEUP_TIME);
		}

		/* WRITE SPI_WRITE_REQ */

		memset(tx_buf, 0xFF, NUMBER_OF_HEADER * SPI_APP_HEADER_LEN
			+ dummy_byte);
		spi_prepare_header(tx_buf, SPI_WRITE_REQ, start_register, len);
		memcpy(tx_buf + SPI_APP_HEADER_LEN, val, len);
		spi_message_init(&spi_msg);
		memset(&transfer, 0,  sizeof(struct spi_transfer));
		transfer.tx_buf = tx_buf;
		transfer.rx_buf = rx_buf;
		transfer.len = NUMBER_OF_HEADER * SPI_APP_HEADER_LEN +
			dummy_byte + len;
		spi_message_add_tail(&transfer, &spi_msg);
		ret_val = spi_sync(client, &spi_msg);
		if (ret_val < 0) {
			thp_log_err("%s:Error writing to spi\n", __func__);
			continue;
		}

		for (i = 0; i < dummy_byte; i++) {
			if (rx_buf[SPI_APP_HEADER_LEN + i] == SPI_WRITE_OK) {
				dummy_offset = i + SPI_APP_HEADER_LEN;
				if (dummy_offset > WRITE_DUMMY_BYTE / 2)
					thp_log_info(
						"Found big write dummy offset %d\n",
						dummy_offset);
				break;
			}
		}

		if (dummy_offset) {
			// tx[1] or rx[1] :LSB of start register address
			// tx[2] or rx[2] :MSB of start register address
			// tx[3] or rx[3] :LSB of len
			// tx[4] or rx[4] :MSB of len
			if ((tx_buf[1] != rx_buf[1 + dummy_offset]) ||
				(tx_buf[2] != rx_buf[2 + dummy_offset])) {
				thp_log_err(
					"Unexpected register %u != %u\n",
					(rx_buf[1 + dummy_offset] |
					(rx_buf[2 + dummy_offset] <<
					MOVE_8BIT)), start_register);
				dummy_offset = 0;
			} else if ((tx_buf[3] != rx_buf[3 + dummy_offset]) ||
					(tx_buf[4] != rx_buf[4 +
					dummy_offset])) {
				thp_log_err(
					"Unexpected count %d != %d reading from spi\n",
					(rx_buf[3 + dummy_offset] |
					(rx_buf[4 + dummy_offset] <<
					MOVE_8BIT)), len);
				dummy_offset = 0;
			}
		} else {
			thp_log_err("%s: Cannot found write dummy offset %u\n",
				__func__, start_register);
		}
	} while ((get_header_crc(rx_buf + dummy_offset) !=
		rx_buf[SPI_APP_HEADER_LEN - 1 + dummy_offset]) ||
		(dummy_offset == 0));
	return 0;
}

static int touch_driver_read_blks(struct thp_device *tdev,
	struct spi_device *client, u16 start, u16 count,
	u8 *buf, u16 override_limit)
{
	u16 offset = 0;
	int ret_val;
	u16 size;

	if ((tdev == NULL) || (client == NULL) || (buf == NULL)) {
		thp_log_err("%s:point is  null\n", __func__);
		return -EINVAL;
	}
	ret_val = thp_bus_lock();
	if (ret_val < 0) {
		thp_log_err("%s:get lock failed\n", __func__);
		return -EINVAL;
	}
	while (offset < count) {
		if (override_limit == 0)
			size = min(SPI_APP_DATA_MAX_LEN, count - offset);
		else
			size = min(override_limit, (u16)(count - offset));
		ret_val = touch_driver_read_reg(tdev, client, (start + offset),
			size, (buf + offset));
		if (ret_val)
			break;
		offset += size;
	}
	thp_bus_unlock();
	return ret_val;
}

static int touch_driver_write_blks(struct thp_device *tdev,
	struct spi_device *client, u16 start, u16 count, u8 *buf)
{
	u16 offset = 0;
	int ret_val;
	u16 size;

	if ((tdev == NULL) || (client == NULL) || (buf == NULL)) {
		thp_log_err("%s:point is  null\n", __func__);
		return -EINVAL;
	}
	ret_val = thp_bus_lock();
	if (ret_val < 0) {
		thp_log_err("%s:get lock failed\n", __func__);
		return -EINVAL;
	}
	while (offset < count) {
		size = min(SPI_APP_DATA_MAX_LEN, count - offset);

		ret_val = touch_driver_write_reg(tdev, client,
			start + offset, size, buf + offset);
		if (ret_val)
			break;
		offset += size;
	}
	thp_bus_unlock();
	return ret_val;
}

static int touch_driver_init(struct thp_device *tdev)
{
	int rc;
	struct thp_core_data *cd = NULL;
	struct device_node *ssl_node = NULL;

	thp_log_info("%s: called\n", __func__);

	if (tdev == NULL) {
		thp_log_err("%s: tdev is  null\n", __func__);
		return -EINVAL;
	}
	cd = tdev->thp_core;
	ssl_node = of_get_child_by_name(cd->thp_node, THP_SSL_DEV_NODE_NAME);
	if (ssl_node == NULL) {
		thp_log_info("%s: syna dev not config in dts\n", __func__);
		return -ENODEV;
	}

	thp_log_info("%s >>>\n", __func__);

	rc = thp_parse_spi_config(ssl_node, cd);
	if (rc)
		thp_log_err("%s: spi config parse fail\n", __func__);

	rc = thp_parse_timing_config(ssl_node, &tdev->timing_config);
	if (rc)
		thp_log_err("%s: timing config parse fail\n", __func__);

	rc = thp_parse_feature_config(ssl_node, cd);
	if (rc)
		thp_log_err("%s: feature_config fail\n", __func__);

	rc = thp_parse_trigger_config(ssl_node, cd);
	if (rc)
		thp_log_err("%s: trigger_config fail\n", __func__);
	if (cd->support_gesture_mode) {
		cd->easy_wakeup_info.sleep_mode = TS_POWER_OFF_MODE;
		cd->easy_wakeup_info.easy_wakeup_gesture = false;
		cd->easy_wakeup_info.off_motion_on = false;
	}
	return 0;
}

static int touch_driver_get_project_id(struct thp_device *tdev, char *buf,
	unsigned int len)
{
	int ret = -EINVAL;
	int i;
	char buff[PROJECTID_ARR_LEN] = {0};
	int8_t retry = READ_ID_RETRY_TIMES;
	unsigned char write_value = GETPDSDATA_COMMAND;
	struct thp_core_data *cd = NULL;
	char *project_id = NULL;

	memset(buff, 0, sizeof(buff));

	if ((tdev == NULL) || (buf == NULL) || (tdev->thp_core == NULL)) {
		thp_log_err("%s:tdev or buf is  null\n", __func__);
		return -EINVAL;
	}
	cd = tdev->thp_core;

	if (is_bootloader_mode) {
		if (cd->support_vendor_ic_type == MXT3662) {
			memcpy(buf, cd->project_id, len);
		} else {
			project_id = cd->project_id_dummy;
			memcpy(buf, project_id, len);
		}
		thp_log_info("The project id is set to %s in bootloader mode\n",
			buf);
		return 0;
	}

	if ((t37_address == 0) || (t6_address == 0) ||
		(len > (sizeof(buff) - PDS_HEADER_OFFSET - 1))) {
		thp_log_err("%s: condition check fail, read len = %u\n",
			__func__, len);
		return ret;
	}
	do {
		ret = touch_driver_write_blks(tdev, tdev->thp_core->sdev,
			t6_address + MXT_T6_DIAGNOSTIC_OFFSET, 1, &write_value);
		if (ret != 0)
			thp_log_err("Failed to send T6 diagnositic command\n");
		mdelay(3 + READ_ID_RETRY_TIMES - retry);

		ret = touch_driver_read_blks(tdev, tdev->thp_core->sdev,
			t37_address, len + PDS_HEADER_OFFSET, buff, 0);
		for (i = 0; i < len; i++)
			thp_log_info(" [%2x] ", buff[i]);

		if ((ret != 0) || (buff[0] != 0x81) || (buff[1] != 0) ||
			(buff[2] != 0x24) || (buff[PDS_HEADER_OFFSET] == 0)) {
			thp_log_err("Failed to read T37 data for project ID\n");
		} else {
			thp_log_info("read T37 data for project ID\n");
			break;
		}
	} while (retry-- > 0);

	if (retry < 0) {
		thp_log_err("read T37 data for projecd ID timeout\n");
		return -1;
	}
	memcpy(buf, buff + PDS_HEADER_OFFSET, len);
	thp_log_info("the project id is %s\n", buff + PDS_HEADER_OFFSET);
	return 0;
}

static void touch_driver_update_addr(struct mxt_object *object)
{
	if (object == NULL) {
		thp_log_err("[%s] object is  NULL\n", __func__);
		return;
	}
	switch (object->type) {
	case SSL_T117_ADDR:
		t117_address = object->start_address;
		break;
	case SSL_T7_ADDR:
		t7_address = object->start_address;
		break;
	case SSL_T6_ADDR:
		t6_address = object->start_address;
		break;
	case SSL_T37_ADDR:
		t37_address = object->start_address;
		break;
	case SSL_T118_ADDR:
		t118_address = object->start_address;
		break;
	case SSL_T24_ADDR:
		t24_address = object->start_address;
		break;
	case SSL_T8_ADDR:
		t8_address = object->start_address;
		break;
	case SSL_T145_ADDR:
		t145_address = object->start_address;
		break;
	case SSL_T144_ADDR:
		t144_address = object->start_address;
		break;
	default:
		thp_log_debug("%s: unused reg address\n", __func__);
		break;
	}
}

static int touch_driver_update_obj_addr(struct thp_device *tdev)
{
	int ret_val;
	int i;
	struct mxt_info mxtinfo;
	u8 *buff = NULL;
	size_t curr_size;
	struct mxt_object *object_table = NULL;
	struct mxt_object *object = NULL;

	memset(&mxtinfo, 0, sizeof(mxtinfo));

	thp_log_info("%s: called\n", __func__);
	if (tdev == NULL) {
		thp_log_err("%s: tdev is  null\n", __func__);
		return -EINVAL;
	}

	curr_size = MXT_OBJECT_START;

	ret_val = touch_driver_read_blks(tdev, tdev->thp_core->sdev,
		CHIP_IDENTIFIED_START_ADDR, curr_size,
		(u8 *)&mxtinfo, OVERRIDE_LIMIT_VALUE);
	if (ret_val) {
		thp_log_err("touch_driver_read_blks--info block read error\n");
		return -EINVAL;
	}

	curr_size += mxtinfo.object_num * sizeof(struct mxt_object) +
		MXT_INFO_CHECKSUM_SIZE;
	buff = kzalloc(curr_size, GFP_KERNEL);
	if (buff == NULL) {
		thp_log_err("%s:buff alloc faild\n", __func__);
		return -ENOMEM;
	}

	ret_val = touch_driver_read_blks(tdev, tdev->thp_core->sdev,
		MXT_OBJECT_START, curr_size, buff, OVERRIDE_LIMIT_VALUE);
	if (ret_val) {
		thp_log_err("touch_driver_read_blks--info block read error\n");
		goto error_free;
	}

	t117_address = 0;
	object_table = (struct mxt_object *)(buff);
	for (i = 0; i < mxtinfo.object_num; i++) {
		object = object_table + i;
		le16_to_cpus(&object->start_address);
		touch_driver_update_addr(object);
	}

	tdev->thp_core->frame_data_addr = t117_address;
	thp_log_info("%s:frame_data_addr %d\n", __func__,
			tdev->thp_core->frame_data_addr);
error_free:
	kfree(buff);
	return ret_val;
}

static int touch_driver_power_init(struct thp_device *tdev)
{
	int ret;

	thp_log_info("%s: called\n", __func__);

	if (tdev == NULL) {
		thp_log_err("%s: tdev is  null\n", __func__);
		return -EINVAL;
	}
	ret = thp_power_supply_get(THP_VCC);
	if (ret)
		thp_log_err("%s: failed to get vcc\n", __func__);
	ret = thp_power_supply_get(THP_IOVDD);
	if (ret)
		thp_log_err("%s: failed to get vddio\n", __func__);
	return 0;
}

static int touch_driver_power_on_for_3662(struct thp_device *tdev)
{
	int ret;

	if (tdev == NULL) {
		thp_log_err("%s: tdev is  null\n", __func__);
		return -EINVAL;
	}
	gpio_direction_output(tdev->gpios->cs_gpio, GPIO_HIGH);
	thp_log_info("%s call cs high\n", __func__);
	gpio_direction_output(tdev->gpios->rst_gpio, GPIO_HIGH);
	thp_log_info("%s call reset high\n", __func__);

	gpio_direction_output(tdev->gpios->rst_gpio, GPIO_LOW);
	thp_log_info("%s call reset low\n", __func__);
	thp_do_time_delay(tdev->timing_config.boot_reset_low_delay_ms);
	thp_log_info("%s call delay %dms\n", __func__,
		tdev->timing_config.boot_reset_low_delay_ms);

	ret = thp_power_supply_ctrl(THP_VCC, THP_POWER_ON, 0); /* 0ms */
	if (ret)
		thp_log_err("%s:power on ctrl vcc failed\n", __func__);

	ret = thp_power_supply_ctrl(THP_IOVDD, THP_POWER_ON, 0); /* 0ms */
	if (ret)
		thp_log_err("%s:power on ctrl vddio failed\n", __func__);

	thp_do_time_delay(tdev->timing_config.boot_vddio_on_after_delay_ms);
	thp_log_info("%s call delay %dms\n", __func__,
		tdev->timing_config.boot_vddio_on_after_delay_ms);

	thp_log_info("%s pull up tp ic reset\n", __func__);
	gpio_set_value(tdev->gpios->rst_gpio, GPIO_HIGH);
	thp_do_time_delay(tdev->timing_config.boot_reset_hi_delay_ms);
	thp_log_info("%s call delay %dms\n", __func__,
		tdev->timing_config.boot_reset_hi_delay_ms);
	return ret;
}

static void touch_driver_power_release(void)
{
	thp_power_supply_put(THP_VCC);
	thp_power_supply_put(THP_IOVDD);
}

static int touch_driver_power_on(struct thp_device *tdev)
{
	int ret;

	if (tdev == NULL) {
		thp_log_err("%s: tdev is  null\n", __func__);
		return -EINVAL;
	}
	gpio_direction_output(tdev->gpios->rst_gpio, GPIO_LOW);
	mdelay(1);
	ret = thp_power_supply_ctrl(THP_IOVDD, THP_POWER_ON, 1); /* 1ms */
	if (ret)
		thp_log_err("%s:power on ctrl vddio failed\n", __func__);
	ret = thp_power_supply_ctrl(THP_VCC, THP_POWER_ON, 1); /* 1ms */
	if (ret)
		thp_log_err("%s:power on ctrl vcc failed\n", __func__);
	thp_log_info("%s pull up tp ic reset\n", __func__);
	gpio_set_value(tdev->gpios->rst_gpio, GPIO_HIGH);
	return ret;
}

static int touch_driver_power_off(struct thp_device *tdev)
{
	int ret;

	if (tdev == NULL) {
		thp_log_err("%s: tdev is  null\n", __func__);
		return -EINVAL;
	}
	thp_log_info("%s pull down tp ic reset\n", __func__);
	gpio_set_value(tdev->gpios->rst_gpio, GPIO_LOW);
	ret = thp_power_supply_ctrl(THP_IOVDD, THP_POWER_OFF, 0);
	if (ret)
		thp_log_err("%s:power off ctrl vddio failed\n", __func__);
	ret = thp_power_supply_ctrl(THP_VCC, THP_POWER_OFF, 1); /* 1ms */
	if (ret)
		thp_log_err("%s:power off ctrl vcc failed\n", __func__);
	return ret;
}

#define BOOTLOADER_MODE_STATUS 0xC0
#define BOOTLOADER_NEXT_FRAME_STATUS 0xC0
#define APP_MODE_CMD_LEN 2
static int set_bootloader_to_normal_mode(
	struct thp_device *tdev)
{
	u8 status_byte = 0;
	int ret;
	/* app mode cmd */
	u8 appmode_sequence[APP_MODE_CMD_LEN] = { 0x00, 0x00 };

	thp_log_info("%s call\n", __func__);
	/*
	 * Try to kick the device out of bootloader mode
	 * Do another bootloader read (ensure 0xE0 state
	 * rather than 0x60)
	 */
	ret = touch_driver_bootloader_read(tdev, tdev->thp_core->sdev,
		&status_byte, 1);
	if (ret)
		thp_log_info("%s:bootloader read failed\n", __func__);
	thp_log_info("%s:status_byte = %d\n", __func__, status_byte);
	if ((status_byte & BOOTLOADER_MODE_STATUS) == BOOTLOADER_MODE_STATUS) {
		/* Reset the device into application mode */
		ret = touch_driver_bootloader_write(tdev, tdev->thp_core->sdev,
			appmode_sequence, APP_MODE_CMD_LEN);
		if (ret)
			thp_log_info("%s:bootloader write failed\n", __func__);
		/* delay 1200ms for bootloader mode at 0x60 */
		msleep(1200);
	}
	thp_log_info("%s call end\n", __func__);
	return ret;
}

static int touch_driver_is_support_ic_type(const struct mxt_info *mxtinfo)
{
	if (((mxtinfo->family_id == MXT680U2_FAMILY_ID) &&
		(mxtinfo->variant_id == MXT680U2_VARIANT_ID)) ||
		((mxtinfo->family_id == MXT3562_FAMILY_ID) &&
		(mxtinfo->variant_id == MXT3562_VARIANT_ID)) ||
		((mxtinfo->family_id == MXT3662_FAMILY_ID) &&
		(mxtinfo->variant_id == MXT3662_VARIANT_ID)))
		return true;
	else
		return false;
}

#define SUPPORT_IC_TYPE 1
#define IS_BOOTLOADE_MODE 0
#define NOT_SSL_DEVICE (-1)
static int touch_driver_check_boot_mode(struct thp_device *tdev,
	struct mxt_info *mxtinfo)
{
	int ret_val;
	u8 status_byte = 0;

	if (touch_driver_is_support_ic_type(mxtinfo)) {
		thp_log_info("%s: support ic,unnecessary check boot mode\n",
			__func__);
		return SUPPORT_IC_TYPE;
	}
	thp_log_err("%s:chip is not identified, try to check bootloader mode\n",
		__func__);
	if (touch_driver_check_is_bootloader(tdev)) {
		if (tdev->thp_core->support_vendor_ic_type == MXT3662) {
			ret_val = set_bootloader_to_normal_mode(tdev);
			if (ret_val)
				thp_log_err(
					"%s:set_bootloader_to_normal_mode failed\n",
					__func__);
			ret_val = touch_driver_bootloader_read(tdev,
				tdev->thp_core->sdev,
				&status_byte, 1); /* 1: status_byte len */
			if (ret_val)
				thp_log_info("%s:bootloader read failed\n",
					__func__);
			thp_log_info("%s status_byte = %u\n", __func__,
				status_byte);
			if (status_byte == BOOTLOADER_CRC_ERROR_CODE ||
				status_byte == BOOTLOADER_WAITING_CMD_MODE_1) {
				/* 1: bootloader mode flag */
				is_bootloader_mode = 1;
				/* set default value */
				t117_address = SSL_SYNC_DATA_REG_DEFAULT_ADDR;
				return IS_BOOTLOADE_MODE;
			}
		} else {
			/* 1: bootloader mode flag */
			is_bootloader_mode = 1;
			/* set default value */
			t117_address = SSL_SYNC_DATA_REG_DEFAULT_ADDR;
			return IS_BOOTLOADE_MODE;
		}
	}
	thp_log_err("chip is not identified return no dev\n");
	ret_val = touch_driver_power_off(tdev);
	if (ret_val)
		thp_log_err("%s:power_off failed\n", __func__);
	touch_driver_power_release();
	return NOT_SSL_DEVICE;
}

static void touch_driver_chip_identified(struct thp_device *tdev,
	struct mxt_info *mxtinfo)
{
	int ret_val;
	int retry_time = READ_ID_RETRY_TIMES;

	thp_log_info("%s call\n", __func__);
	do {
		ret_val = touch_driver_read_blks(tdev, tdev->thp_core->sdev,
			CHIP_IDENTIFIED_START_ADDR,
			sizeof(*mxtinfo), (u8 *)mxtinfo, OVERRIDE_LIMIT_VALUE);
		if (ret_val) {
			thp_log_err("%s:read device ID failed\n", __func__);
			continue;
		}
		if (touch_driver_is_support_ic_type(mxtinfo)) {
			thp_log_info("%s: Chip is identified OK %d, %d\n",
				__func__, mxtinfo->family_id,
				mxtinfo->variant_id);
			break;
		}
		thp_log_info("%s:retry time is %d\n", __func__, retry_time);
	} while (retry_time-- > 0);
	thp_log_info("%s call end\n", __func__);
}

static int touch_driver_read_obj_addr(struct thp_device *tdev,
	size_t curr_size, const struct mxt_info *mxtinfo)
{
	int ret_val;
	int i;
	u8 *buff = NULL;
	struct mxt_object *object = NULL;
	struct mxt_object *object_table = NULL;

	thp_log_info("%s call\n", __func__);

	curr_size += mxtinfo->object_num * sizeof(*object) +
		MXT_INFO_CHECKSUM_SIZE;
	buff = kzalloc(curr_size, GFP_KERNEL);
	if (buff == NULL) {
		thp_log_err("%s:buff alloc failed\n", __func__);
		ret_val = -ENOMEM;
		goto error_free;
	}
	ret_val = touch_driver_read_blks(tdev, tdev->thp_core->sdev,
		MXT_OBJECT_START, curr_size, buff, OVERRIDE_LIMIT_VALUE);
	if (ret_val) {
		thp_log_err("%s:info block reading error\n", __func__);
		goto error_free;
	}
	object_table = (struct mxt_object *)(buff);
	for (i = 0; i < mxtinfo->object_num; i++) {
		object = object_table + i;
		/*
		 * object start_address from little endian
		 * to local CPU endianness **IN PLACE**
		 * IMPORTANT: this is only for the first
		 * loop through the object table
		 */
		le16_to_cpus(&object->start_address);
		touch_driver_update_addr(object);
	}
	/* 0:invalid addr */
	if (t117_address == 0) {
		t117_address = SSL_SYNC_DATA_REG_DEFAULT_ADDR;
		thp_log_err("%s: get t117 reg failed, set to default\n",
			__func__);
	}
	tdev->thp_core->frame_data_addr = t117_address;
	thp_log_info("%s:tui_tp_addr %d\n", __func__,
		tdev->thp_core->frame_data_addr);
error_free:
	kfree(buff);
	thp_log_info("%s call end\n", __func__);
	return ret_val;
}

static int touch_driver_chip_detect(struct thp_device *tdev)
{
	int ret_val;
	struct mxt_info mxtinfo;
	size_t curr_size;

	memset(&mxtinfo, 0, sizeof(mxtinfo));

	if (tdev == NULL) {
		thp_log_err("%s: tdev is  null\n", __func__);
		return -EINVAL;
	}
	gpio_set_value(tdev->gpios->rst_gpio, GPIO_LOW);
	thp_log_info("%s: called\n", __func__);
	ret_val = touch_driver_power_init(tdev);
	if (ret_val)
		thp_log_err("%s:touch_driver_power_init failed\n", __func__);
	curr_size = MXT_OBJECT_START;

	if (tdev->thp_core->support_vendor_ic_type == MXT3662)
		ret_val = touch_driver_power_on_for_3662(tdev);
	else
		ret_val = touch_driver_power_on(tdev);

	if (ret_val)
		thp_log_err("%s:touch_driver_power_on failed\n", __func__);

	thp_do_time_delay(tdev->timing_config.boot_reset_after_delay_ms);
	touch_driver_chip_identified(tdev, &mxtinfo);
	ret_val = touch_driver_check_boot_mode(tdev, &mxtinfo);
	/* 0:bootloader mode,-ENODEV: no device ,need return */
	if (ret_val == NOT_SSL_DEVICE) {
		touch_driver_exit(tdev);
		return ret_val;
	}
	if (ret_val == IS_BOOTLOADE_MODE) {
		thp_log_info("%s:bootloader mode,update fw\n", __func__);
		return ret_val;
	}
	ret_val = touch_driver_read_obj_addr(tdev, curr_size, &mxtinfo);
	if (ret_val)
		thp_log_err("%s:read obj addr failed\n", __func__);
	thp_log_info("%s call end\n", __func__);
	return ret_val;
}

#define T117_HEADER_DATA_READ_LEN 13
#define T117_PAYLOAD_LEN_MSB_OFFSET 11
#define T117_PAYLOAD_LEN_LSB_OFFSET 12
static int touch_driver_get_frame_for_3662(
	struct thp_device *tdev, char *buf)
{
	int ret_val;
	unsigned int payload_len;

	ret_val = touch_driver_read_blks(tdev, tdev->thp_core->sdev,
		t117_address, T117_HEADER_DATA_READ_LEN, buf, 0);
	if (ret_val)
		thp_log_err("%s:read len failed\n", __func__);
	payload_len = (buf[T117_PAYLOAD_LEN_MSB_OFFSET] << MOVE_8BIT);
	payload_len |= buf[T117_PAYLOAD_LEN_LSB_OFFSET];
	thp_log_debug("%s:T117 2stage SPI Read payload len %d %x %x\n",
		__func__, payload_len, buf[T117_PAYLOAD_LEN_MSB_OFFSET],
		buf[T117_PAYLOAD_LEN_LSB_OFFSET]);
	ret_val = touch_driver_read_blks(tdev, tdev->thp_core->sdev,
		t117_address + T117_HEADER_DATA_READ_LEN, payload_len,
		(buf + T117_HEADER_DATA_READ_LEN),
		T117_BYTES_READ_LIMIT);
	if (ret_val)
		thp_log_err("%s:read frame failed\n", __func__);

	return ret_val;
}

static int touch_driver_get_frame(struct thp_device *tdev, char *buf,
	unsigned int len)
{
	int ret_val;
	struct thp_core_data *cd = NULL;

	if ((tdev == NULL) || (buf == NULL) ||
		(tdev->thp_core == NULL) ||
		(tdev->thp_core->sdev == NULL)) {
		thp_log_info("%s: input dev or buf null\n", __func__);
		return -ENOMEM;
	}

	if (!len) {
		thp_log_info("%s: read len illegal\n", __func__);
		return -ENOMEM;
	}
	cd = tdev->thp_core;
	if (cd->support_vendor_ic_type == MXT3662)
		return touch_driver_get_frame_for_3662(tdev, buf);
	ret_val = touch_driver_read_blks(tdev, tdev->thp_core->sdev,
		t117_address, len, buf, T117_BYTES_READ_LIMIT);
	return ret_val;
}

static int touch_driver_set_screen_off_mode(struct thp_device *tdev)
{
	int ret_val;

	u8 t7_idle_scan_rate = SCREEN_OFF_MODE_SCAN_RATE;
	/* 2 is scan mode len */
	u8 t8_scan_mode[2] = { SCREEN_OFF_SCAN_MODE, SCREEN_OFF_MCT_SCAN_MODE };
	u8 t145_cmd = MXT_T145_SCREEN_OFF_CMD;

	if (tdev == NULL) {
		thp_log_err("%s: input dev null\n", __func__);
		return -ENOMEM;
	}

	if ((t7_address == 0) || (t8_address == 0) || (t145_address == 0)) {
		thp_log_err("Object address for screen off is not correct\n");
		return -ENOMEM;
	}

	ret_val = touch_driver_write_blks(tdev, tdev->thp_core->sdev,
		t7_address, sizeof(t7_idle_scan_rate), &t7_idle_scan_rate);
	if (ret_val != 0)
		thp_log_err("write t7_address failed\n");
	ret_val = touch_driver_write_blks(tdev, tdev->thp_core->sdev,
		t8_address + T8_MEASIDLEDEF_OFFSET,
		sizeof(t8_scan_mode), t8_scan_mode);
	if (ret_val != 0)
		thp_log_err("write t8_address failed\n");
	ret_val = touch_driver_write_blks(tdev, tdev->thp_core->sdev,
		t145_address + MXT_T145_CMD_OFFSET,
		sizeof(t145_cmd), &t145_cmd);
	if (ret_val != 0)
		thp_log_err("write t145_address failed\n\n");
	thp_log_info("mxt set screen off mode done\n");
	return ret_val;
}

static int touch_driver_get_active_idle_timer(struct thp_device *tdev,
	uint32_t *active_time, uint32_t *dozing_time)
{
	int ret_val;
	u8 buf[T144_ACTIVE_DOZING_READ_LEN] = {0};

	if (tdev == NULL) {
		thp_log_err("%s: input dev null\n", __func__);
		return -ENOMEM;
	}

	if (t144_address == 0) {
		thp_log_err("T144 address is not correct\n");
		return -ENOMEM;
	}
	ret_val = touch_driver_read_blks(tdev, tdev->thp_core->sdev,
		t144_address + T144_ACTIVE_TIMER_OFFSET,
		T144_ACTIVE_DOZING_READ_LEN, buf, 0);
	if (ret_val) {
		thp_log_err("touch_driver_read_blks--info block read error\n");
		ret_val = -ENOMEM;
	}
	/*
	 * use buf[0],buf[1],buf[2],buf[3] to concatenate active time
	 * use buf[4],buf[5],buf[6],buf[7] to concatenate dozing time
	 */
	*active_time = (buf[0] << MOVE_24BIT) | (buf[1] << MOVE_16BIT) |
			(buf[2] << MOVE_8BIT) | buf[3];
	*dozing_time = (buf[4] << MOVE_24BIT) | (buf[5] << MOVE_16BIT) |
			(buf[6] << MOVE_8BIT) | buf[7];

	thp_log_info("%s: Active time%d, Dozing time %d\n", __func__,
		*active_time, *dozing_time);
	return ret_val;
}

static int touch_driver_resume(struct thp_device *tdev)
{
	int ret = 0;
	uint32_t active_time = 0;
	uint32_t dozing_time = 0;

	if (tdev == NULL) {
		thp_log_err("%s: tdev is  null\n", __func__);
		return -EINVAL;
	}
	if (is_pt_test_mode(tdev)) {
		gpio_set_value(tdev->gpios->rst_gpio, GPIO_LOW);
		mdelay(tdev->timing_config.resume_reset_after_delay_ms);
		gpio_set_value(tdev->gpios->rst_gpio, GPIO_HIGH);
	} else if (g_thp_udfp_status || tdev->thp_core->support_ring_feature ||
		(tdev->thp_core->easy_wakeup_info.sleep_mode ==
			TS_GESTURE_MODE)) {
		thp_log_info("get_active_idle_timer\n");
		ret = touch_driver_get_active_idle_timer(tdev, &active_time,
							&dozing_time);
		if (ret)
			thp_log_err("%s: get_active_idle_timer failed\n",
				__func__);
#ifdef CONFIG_HUAWEI_DUBAI
		HWDUBAI_LOGE("DUBAI_TAG_TP_DURATION", "active=%d dozing=%d",
					active_time, dozing_time);
#endif
		thp_log_info("%s TS_GESTURE_MODE or tp_ud enable ,so reset\n",
			__func__);
		gpio_set_value(tdev->gpios->rst_gpio, GPIO_LOW);
		mdelay(tdev->timing_config.resume_reset_after_delay_ms);
		gpio_set_value(tdev->gpios->rst_gpio, GPIO_HIGH);
	} else {
		if (tdev->thp_core->support_vendor_ic_type == MXT3662)
			ret = touch_driver_power_on_for_3662(tdev);
		else
			ret = touch_driver_power_on(tdev);
	}
	thp_log_info("%s: called end\n", __func__);
	return ret;
}

static int touch_driver_after_resume(struct thp_device *tdev)
{
	int ret = 0;

	thp_log_info("%s: called\n", __func__);
	if (tdev == NULL) {
		thp_log_err("%s: tdev null\n", __func__);
		return -EINVAL;
	}
	if (!g_thp_udfp_status)
		thp_do_time_delay(
			tdev->timing_config.boot_reset_after_delay_ms);
	return ret;
}

static int pt_mode_set(struct thp_device *tdev)
{
	int ret;
	u8 t7_active[SSL_T7_COMMAMD_LEN] = { 8, 8 }; /* pt station cmd */

	if (tdev == NULL) {
		thp_log_err("%s: tdev null\n", __func__);
		return -EINVAL;
	}
	if (t7_address == 0) {
		thp_log_err("%s: tdev null\n", __func__);
		return -EINVAL;
	}
	ret = touch_driver_write_blks(tdev, tdev->thp_core->sdev, t7_address,
			SSL_T7_COMMAMD_LEN, t7_active);
	if (ret != 0) {
		thp_log_err("Failed to send T7 always active command\n");
		return -EINVAL;
	}
	return ret;
}

static int touch_driver_suspend(struct thp_device *tdev)
{
	int ret;

	if (tdev == NULL) {
		thp_log_err("%s: tdev is  null\n", __func__);
		return -EINVAL;
	}
	g_thp_udfp_status = thp_get_status(THP_STATUS_UDFP);
	thp_log_info("%s: called udfp_status = %u\n",
		__func__, g_thp_udfp_status);
	if (is_pt_test_mode(tdev)) {
		thp_log_info("%s: suspend PT mode\n", __func__);
		ret = pt_mode_set(tdev);
		if (ret != 0)
			thp_log_err(
				"Failed to send T7 always active command\n");
	} else if (g_thp_udfp_status ||
		tdev->thp_core->support_ring_feature ||
		(tdev->thp_core->easy_wakeup_info.sleep_mode ==
			TS_GESTURE_MODE)) {
		if (tdev->thp_core->easy_wakeup_info.sleep_mode ==
			TS_GESTURE_MODE) {
			thp_log_info("%s TS_GESTURE_MODE\n", __func__);
			ret = touch_driver_wakeup_gesture_en_switch(tdev, 1);
			if (ret != 0)
				thp_log_err(
					"Failed to send wakeup gesture enable command\n");
			mutex_lock(&tdev->thp_core->thp_wrong_touch_lock);
			tdev->thp_core->easy_wakeup_info.off_motion_on = true;
			mutex_unlock(&tdev->thp_core->thp_wrong_touch_lock);
		}
		ret = touch_driver_set_screen_off_mode(tdev);
		if (ret != 0)
			thp_log_err("Failed to set_screen_off_mode command\n");
	} else {
		ret = touch_driver_power_off(tdev);
		thp_log_info("enter poweroff mode\n");
	}
	thp_log_info("%s: called end\n", __func__);
	return ret;
}

static void touch_driver_exit(struct thp_device *tdev)
{
	thp_log_info("%s: called\n", __func__);
	if (tdev != NULL) {
		if (tdev->tx_buff != NULL) {
			kfree(tdev->tx_buff);
			tdev->tx_buff = NULL;
		}
		if (tdev->rx_buff != NULL) {
			kfree(tdev->rx_buff);
			tdev->rx_buff = NULL;
		}
		kfree(tdev);
		tdev = NULL;
	}
}

static int touch_driver_afe_notify_callback(struct thp_device *tdev,
	unsigned long event)
{
	if (tdev == NULL) {
		thp_log_err("%s: tdev null\n", __func__);
		return -EINVAL;
	}
	return touch_driver_update_obj_addr(tdev);
}

static int touch_driver_set_fw_update_mode(struct thp_device *tdev,
	struct thp_ioctl_set_afe_status set_afe_status)
{
	int rc = -EINVAL;

	if (tdev == NULL) {
		thp_log_err("%s: tdev null\n", __func__);
		return -EINVAL;
	}
	if (set_afe_status.status == THP_AFE_FW_UPDATE_SET_SPI_COM_MODE)
		rc = thp_set_spi_com_mode(tdev->thp_core,
			set_afe_status.parameter);

	return rc;
}

#define SSL_SCREEN_OFF_MODE 0x04
static void touch_driver_check_screen_off_mode(struct thp_device *tdev)
{
	int ret_val;
	u8 buffer = 0;

	if (!tdev) {
		thp_log_info("%s: input dev null\n", __func__);
		return;
	}

	if (!t118_address) {
		thp_log_info("%s: invalid T118 frame data address\n", __func__);
		return;
	}

	ret_val = touch_driver_read_blks(tdev, tdev->thp_core->sdev,
		t118_address, 0, &buffer, 0);
	if (!ret_val) {
		if ((buffer & SSL_SCREEN_OFF_MODE) != SSL_SCREEN_OFF_MODE) {
			thp_log_err(
				"%s: TPIC is NOT in screen off mode [%d], try to set screen off mode\n",
				__func__, buffer);

			ret_val = touch_driver_wakeup_gesture_en_switch(tdev,
				1);
			if (ret_val != 0)
				thp_log_err(
					"Failed to send wakeup gesture enable command\n");

			ret_val = touch_driver_set_screen_off_mode(tdev);
			if (ret_val != 0)
				thp_log_err(
					"Failed to set_screen_off_mode command\n");
		} else {
			thp_log_info("%s: TPIC is in screen off mode [%d]\n",
				__func__, buffer);
		}
	}
}

/*
 * add this function in ISR to get the double tap gesture event
 * buffer=0 -> no event   buffer = 0x40 -> double tap gesture event
 */
#define GESTRUE_EVENT_RETRY_TIME 10
int touch_driver_check_gesture_event(struct thp_device *tdev,
	unsigned int *gesture_wakeup_value)
{
	int ret_val;
	u8 buffer[T117_HEADER_STATUS_OFFSET + 2] = {0};
	int i;

	if (tdev == NULL) {
		thp_log_info("%s: input dev null\n", __func__);
		return -ENOMEM;
	}

	if (!t117_address) {
		thp_log_info("%s: invalid T117 frame data address\n", __func__);
		return -ENOMEM;
	}

	thp_log_info("%s\n", __func__);
	/* wait spi bus resume */
	msleep(SSL_WAIT_FOR_SPI_BUS_RESUMED_DELAY);
	for (i = 0; i < GESTRUE_EVENT_RETRY_TIME; i++) {
		ret_val = touch_driver_read_blks(tdev, tdev->thp_core->sdev,
			t117_address, T117_HEADER_STATUS_OFFSET + 1,
			buffer, 0);
		if (ret_val == 0)
			break;
		thp_log_err("%s: spi not work normal, ret %d\n",
			__func__, ret_val);
		msleep(SSL_WAIT_FOR_SPI_BUS_READ_DELAY);
	}
	if (!ret_val) {
		if ((buffer[T117_HEADER_STATUS_OFFSET] & T117_GESTURE_EVENT) ==
			T117_GESTURE_EVENT) {
			thp_log_info("THP found double tap gesture [%x]\n",
					buffer[T117_HEADER_STATUS_OFFSET]);
			mutex_lock(&tdev->thp_core->thp_wrong_touch_lock);
			if (tdev->thp_core->easy_wakeup_info.off_motion_on ==
				true) {
				tdev->thp_core->easy_wakeup_info.off_motion_on =
					false;
				*gesture_wakeup_value = TS_DOUBLE_CLICK;
			}
			mutex_unlock(&tdev->thp_core->thp_wrong_touch_lock);
		} else {
			thp_log_err("NO gesture event found status %x,%x\n",
				buffer[T117_HEADER_STATUS_OFFSET],
				buffer[T117_HEADER_STATUS_OFFSET + 1]);
			touch_driver_check_screen_off_mode(tdev);
		}
	}
	return ret_val;
}

/* enable=1:to enable gesture function, enable=0:to disable gesture function */
static int touch_driver_wakeup_gesture_en_switch(
	struct thp_device *tdev, u8 switch_value)
{
	int ret_val;
	u8 t24_ctrl = 0;
	u16 retry = 0;

	if (tdev == NULL) {
		thp_log_info("%s: input dev null\n", __func__);
		return -EINVAL;
	}
	if (!t24_address) {
		thp_log_info("%s: invalid T24 frame data address\n", __func__);
		return -ENOMEM;
	}
	while (retry++ < SEND_COMMAND_RETRY) {
		ret_val = touch_driver_read_blks(tdev, tdev->thp_core->sdev,
			t24_address, 1, &t24_ctrl, 0);
		if (!ret_val)
			t24_ctrl = switch_value ?
			(t24_ctrl | T24_GESTURE_ON_MASK) :
			(t24_ctrl & (~T24_GESTURE_ON_MASK));
		ret_val = touch_driver_write_blks(tdev, tdev->thp_core->sdev,
			t24_address, sizeof(t24_ctrl), &t24_ctrl);
		if (ret_val != 0)
			thp_log_err("write t24_address failed\n");
		thp_log_info("%s t24_ctrl is 0x%x,switch_value is %d\n",
				__func__, t24_ctrl, switch_value);
		if (ret_val == 0)
			break;
	}
	return ret_val;
}

static int touch_driver_wrong_touch(struct thp_device *tdev)
{
	if (!tdev) {
		thp_log_err("%s: input dev null\n", __func__);
		return -EINVAL;
	}
	if (tdev->thp_core->support_gesture_mode) {
		mutex_lock(&tdev->thp_core->thp_wrong_touch_lock);
		tdev->thp_core->easy_wakeup_info.off_motion_on = true;
		mutex_unlock(&tdev->thp_core->thp_wrong_touch_lock);
		thp_log_info("%s: done\n", __func__);
	}
	return 0;
}

static int touch_driver_gesture_report(struct thp_device *tdev,
	unsigned int *gesture_wakeup_value)
{
	int retval;

	retval = touch_driver_check_gesture_event(tdev, gesture_wakeup_value);
	if (retval != 0) {
		thp_log_err("[%s] retval-> %d\n", __func__, retval);
		return -EINVAL;
	}
	return 0;
}

#define BOOT_HEAD_LEN 4
#define READ_BOOT_HEAD_COUNT 5
static int touch_driver_spi_transfer_one_byte_bootloader(
	struct thp_device *tdev, const char * const tx_buf,
	char * const rx_buf, const unsigned int buf_len)
{
	register unsigned int idx;
	struct spi_message msg;
	struct spi_device *sdev = NULL;
	struct spi_transfer *xfer = NULL;
	int rc;
	int i;

	if ((!tdev) || (!tdev->thp_core) || (!tdev->thp_core->sdev) ||
		(!tx_buf) || (!rx_buf) || (!buf_len)) {
		thp_log_err("%s: point null\n", __func__);
		return -EINVAL;
	}
	sdev = tdev->thp_core->sdev;
	if (tdev->thp_core->suspended) {
		thp_log_err("%s - suspended\n", __func__);
		return 0;
	}

	xfer = kzalloc(buf_len * sizeof(*xfer), GFP_KERNEL);
	if (xfer == NULL) {
		thp_log_info("%s -> zalloc failed\n", __func__);
		return -EIO;
	}
	spi_message_init(&msg);
	/*
	 * Solomon firmware upgrade process is very special
	 * 1:use ssl mxt3662 only
	 */
	if (tdev->thp_core->support_vendor_ic_type == MXT3662) {
		for (idx = 0; idx < READ_BOOT_HEAD_COUNT; idx++) {
			xfer[idx].tx_buf = &tx_buf[idx];
			xfer[idx].rx_buf = &rx_buf[idx];
			if ((idx == BOOT_HEAD_LEN) &&
				(buf_len > READ_BOOT_HEAD_COUNT))
				xfer[idx].len = buf_len - BOOT_HEAD_LEN;
			else
				xfer[idx].len = 1;
			xfer[idx].delay_usecs =
				tdev->timing_config.spi_transfer_delay_us;
			spi_message_add_tail(&xfer[idx], &msg);
		}
	} else {
		for (i = 0; i < buf_len; i++) {
			xfer[i].tx_buf = &tx_buf[i];
			xfer[i].rx_buf = &rx_buf[i];
			xfer[i].len = 1;
			xfer[i].delay_usecs = DELAY_AFTER_NINE_BYTE;
			spi_message_add_tail(&xfer[i], &msg);
		}
	}
	rc = thp_bus_lock();
	if (rc < 0) {
		thp_log_err("%s:get lock failed:%d\n", __func__, rc);
		kfree(xfer);
		return rc;
	}
	rc = thp_spi_sync(sdev, &msg);
	thp_bus_unlock();
	kfree(xfer);
	return rc;
}

struct thp_device_ops ssl_dev_ops = {
	.init = touch_driver_init,
	.detect = touch_driver_chip_detect,
	.get_frame = touch_driver_get_frame,
	.resume = touch_driver_resume,
	.after_resume = touch_driver_after_resume,
	.suspend = touch_driver_suspend,
	.get_project_id = touch_driver_get_project_id,
	.exit = touch_driver_exit,
	.afe_notify = touch_driver_afe_notify_callback,
	.set_fw_update_mode = touch_driver_set_fw_update_mode,
	.chip_wakeup_gesture_enable_switch =
		touch_driver_wakeup_gesture_en_switch,
	.chip_wrong_touch = touch_driver_wrong_touch,
	.chip_gesture_report = touch_driver_gesture_report,
	.spi_transfer_one_byte_bootloader =
		touch_driver_spi_transfer_one_byte_bootloader,
};

static int __init touch_driver_module_init(void)
{
	int rc;
	struct thp_device *dev = NULL;
	struct thp_core_data *cd = thp_get_core_data();

	thp_log_info("%s: called\n", __func__);
	dev = kzalloc(sizeof(struct thp_device), GFP_KERNEL);
	if (dev == NULL) {
		thp_log_err("%s: thp device malloc fail\n", __func__);
		return -ENOMEM;
	}

	dev->tx_buff = kzalloc(THP_MAX_FRAME_SIZE, GFP_KERNEL);
	dev->rx_buff = kzalloc(THP_MAX_FRAME_SIZE, GFP_KERNEL);
	if ((dev->tx_buff == NULL) || (dev->rx_buff == NULL)) {
		thp_log_err("%s: out of memory\n", __func__);
		rc = -ENOMEM;
		goto err;
	}

	dev->ic_name = SSL_IC_NAME;
	dev->ops = &ssl_dev_ops;
	if (cd && cd->fast_booting_solution) {
		thp_send_detect_cmd(dev, NO_SYNC_TIMEOUT);
		/*
		 * The thp_register_dev will be called later to complete
		 * the real detect action.If it fails, the detect function will
		 * release the resources requested here.
		 */
		return 0;
	}
	rc = thp_register_dev(dev);
	if (rc) {
		thp_log_err("%s: register fail\n", __func__);
		goto err;
	} else {
		thp_log_info("%s: register success\n", __func__);
	}

	return rc;
err:
	touch_driver_exit(dev);
	return rc;
}

static void __exit touch_driver_module_exit(void)
{
	thp_log_err("%s: called\n", __func__);
};

module_init(touch_driver_module_init);
module_exit(touch_driver_module_exit);
