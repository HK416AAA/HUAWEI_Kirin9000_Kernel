/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 * Description: SOC camera driver source file
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __HW_ALAN_KERNEL_CAM_DRIVERIC_CFG_H__
#define __HW_ALAN_KERNEL_CAM_DRIVERIC_CFG_H__

#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <platform_include/camera/native/camera.h>

typedef enum _tag_hwdriveric_config_type {
	CAM_DRIVERIC_POWERON,
	CAM_DRIVERIC_POWEROFF,
}hwdriveric_config_type_t;

enum {
	HWDRIVERIC_NAME_SIZE                          =   32,
	HWDRIVERIC_V4L2_EVENT_TYPE                    =   V4L2_EVENT_PRIVATE_START + 0x00080000,

	HWDRIVERIC_HIGH_PRIO_EVENT                    =   0x1500,
	HWDRIVERIC_MIDDLE_PRIO_EVENT                  =   0x2000,
	HWDRIVERIC_LOW_PRIO_EVENT                     =   0x3000,
};

typedef struct _tag_hwdriveric_config_data {
	uint32_t cfgtype;
}hwdriveric_config_data_t;

typedef struct _tag_hwdriveric_info {
	char     name[HWDRIVERIC_NAME_SIZE];
	int      i2c_idx;
	int      position;
} hwdriveric_info_t;

typedef enum _tag_hwdriveric_event_kind {
	HWDRIVERIC_INFO_ERROR,
} hwdriveric_event_kind_t;

typedef struct _tag_hwdriveric_event {
	hwdriveric_event_kind_t                          kind;
	union { // can ONLY place 10 int fields here.
		struct {
			uint32_t                            id;
		} error;
	} data;
}hwdriveric_event_t;

#define HWDRIVERIC_IOCTL_GET_INFO                _IOR('D', BASE_VIDIOC_PRIVATE + 1, hwdriveric_info_t)
#define HWDRIVERIC_IOCTL_CONFIG               	_IOWR('D', BASE_VIDIOC_PRIVATE + 2, hwdriveric_config_data_t)

#endif // __HW_ALAN_KERNEL_CAM_DRIVERIC_CFG_H__

