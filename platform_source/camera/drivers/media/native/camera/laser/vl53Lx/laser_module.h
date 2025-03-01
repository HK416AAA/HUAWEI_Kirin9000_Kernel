/*
 * laser_module.h
 *
 * SOC camera driver source file
 *
 * Copyright (C) 2017-2020 Huawei Technology Co., Ltd.
 *
 * Date:      2017-12-08
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
 *
 */

#ifndef _LASER_MODULE_H_
#define _LASER_MODULE_H_

#include <linux/module.h>
#include <linux/i2c.h>
#include <platform_include/camera/native/laser_cfg.h>

#define laser_err(fmt, ...) pr_err("[laser]ERROR: " fmt "\n", ##__VA_ARGS__)
#define laser_info(fmt, ...) pr_info("[laser]INFO: " fmt "\n", ##__VA_ARGS__)
#define laser_dbg(fmt, ...) pr_debug("[laser]DBG: " fmt "\n", ##__VA_ARGS__)

typedef struct _tag_laser_module_intf_t {
	char *laser_name;
	int (*data_init)(struct i2c_client *client,
		const struct i2c_device_id *id);
	int (*data_remove)(struct i2c_client *client);
	long (*laser_ioctl)(void *hw_data, unsigned int cmd, void *p);
} laser_module_intf_t;

#endif

