/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2016-2020. All rights reserved.
 * Description: camera buf
 * Create: 2016-03-15
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 */

#ifndef __HW_KERNEL_CAM_OIS_CFG_H__
#define __HW_KERNEL_CAM_OIS_CFG_H__

#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <platform_include/camera/native/camera.h>

/* cfg type define */
#define CFG_OIS_I2C_READ 0
#define CFG_OIS_I2C_WRITE 1
#define CFG_OIS_GET_OIS_NAME 2
#define CFG_OIS_SET_OIS_CODE 3

#define CFG_OIS_SET_CFG 21
#define CFG_OIS_GET_OTP_AF 22
#define CFG_OIS_GET_SUPPORT_FLAG 23

#define MAX_OIS_NAME_LEN 16

/* cfg data define */
struct ois_i2c_reg {
	uint16_t address;
	uint32_t value;
};

struct ois_otp {
	uint32_t start;
	uint32_t end;
};

struct hw_ois_cfg_data {
	uint32_t cfgtype;
	union {
		char name[MAX_OIS_NAME_LEN];
		struct ois_i2c_reg reg;
		struct ois_otp otp;
		uint8_t ois_sup;
	} cfg;
};

/* v4l2 subdev ioctl case id define */
#define VIDIOC_KERNEL_OIS_CFG _IOWR('V', BASE_VIDIOC_PRIVATE + 23, \
	struct hw_ois_cfg_data)
#endif
