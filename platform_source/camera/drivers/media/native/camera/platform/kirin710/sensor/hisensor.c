/*
 * sensor_common.c
 *
 * Description: camera driver source file
 *
 * Copyright (c) 2019-2020 Huawei Technologies Co., Ltd.
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

#include <linux/module.h>
#include <linux/printk.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/rpmsg.h>

#include "hwsensor.h"
#include "sensor_commom.h"

#define intf_to_sensor(i) container_of(i, sensor_t, intf)
#define CTL_RESET_HOLD        0
#define CTL_RESET_RELEASE     1
#define GPIO_CONFIG_DELAY_MS  2
#define GPIO_CONFIG_DELAY_US  2000

extern struct hw_csi_pad hw_csi_pad;
extern int rpc_status_change_for_camera(unsigned int status);

int hisensor_config(hwsensor_intf_t *si, void *argp);

struct mutex g_hisensor_power_lock;

char const *hisensor_get_name(hwsensor_intf_t *si)
{
	sensor_t *sensor = NULL;

	if (!si) {
		cam_err("%s. si is null", __func__);
		return NULL;
	}

	sensor = intf_to_sensor(si);
	if (!sensor || !sensor->board_info) {
		cam_err("%s. sensor or board_info->name is NULL", __func__);
		return NULL;
	}

	return sensor->board_info->name;
}

int hisensor_power_up(hwsensor_intf_t *si)
{
	int ret;
	sensor_t *sensor = NULL;

	if (!si) {
		cam_err("%s. si is null", __func__);
		return -EINVAL;
	}

	sensor = intf_to_sensor(si);
	if (!sensor || !sensor->board_info || !sensor->board_info->name) {
		cam_err("%s. sensor or board_info->name is NULL", __func__);
		return -EINVAL;
	}

	cam_info("enter %s. index = %d name = %s", __func__,
		sensor->board_info->sensor_index, sensor->board_info->name);

	ret = hw_sensor_power_up_config(sensor->dev, sensor->board_info);
	if (ret == 0) {
		cam_info("%s. power up config success", __func__);
	} else {
		cam_err("%s. power up config fail", __func__);
		return ret;
	}

	if (hw_is_fpga_board())
		ret = do_sensor_power_on(sensor->board_info->sensor_index,
			sensor->board_info->name);
	else
		ret = hw_sensor_power_up(sensor);

	if (ret == 0)
		cam_info("%s. power up sensor success", __func__);
	else
		cam_err("%s. power up sensor fail", __func__);

	return ret;
}

int hisensor_power_down(hwsensor_intf_t *si)
{
	int ret;
	sensor_t *sensor = NULL;

	if (!si) {
		cam_err("%s. si is null", __func__);
		return -EINVAL;
	}

	sensor = intf_to_sensor(si);
	if (!sensor || !sensor->board_info || !sensor->board_info->name) {
		cam_err("%s. sensor or board_info->name is NULL", __func__);
		return -EINVAL;
	}

	cam_info("enter %s. index = %d name = %s", __func__,
		sensor->board_info->sensor_index, sensor->board_info->name);

	if (hw_is_fpga_board())
		ret = do_sensor_power_off(sensor->board_info->sensor_index,
			sensor->board_info->name);
	else
		ret = hw_sensor_power_down(sensor);

	if (ret == 0)
		cam_info("%s. power down sensor success", __func__);
	else
		cam_err("%s. power down sensor fail", __func__);

	hw_sensor_power_down_config(sensor->board_info);

	return ret;
}

int hisensor_csi_enable(hwsensor_intf_t *si)
{
	(void)si;
	return 0;
}

int hisensor_csi_disable(hwsensor_intf_t *si)
{
	(void)si;
	return 0;
}

static int hisensor_match_id(hwsensor_intf_t *si, void *data)
{
	sensor_t *sensor = NULL;
	struct sensor_cfg_data *cdata = NULL;

	if (!si || !data) {
		cam_err("%s. si or data is NULL", __func__);
		return -EINVAL;
	}

	cam_info("%s enter", __func__);
	sensor = intf_to_sensor(si);
	if (!sensor || !sensor->board_info) {
		cam_err("%s. sensor or board_info is NULL", __func__);
		return -EINVAL;
	}

	cdata = (struct sensor_cfg_data *)data;
	cdata->data = sensor->board_info->sensor_index;

	return 0;
}

static hwsensor_vtbl_t g_hisensor_vtbl = {
	.get_name    = hisensor_get_name,
	.config      = hisensor_config,
	.power_up    = hisensor_power_up,
	.power_down  = hisensor_power_down,
	.match_id    = hisensor_match_id,
	.csi_enable  = hisensor_csi_enable,
	.csi_disable = hisensor_csi_disable,
};

enum camera_metadata_enum_android_hw_dual_primary_mode {
	ANDROID_HW_DUAL_PRIMARY_FIRST  = 0,
	ANDROID_HW_DUAL_PRIMARY_SECOND = 2,
	ANDROID_HW_DUAL_PRIMARY_BOTH   = 3,
};

#define RESET_TYPE_NONE    0
#define RESET_TYPE_PRIMARY 1
#define RESET_TYPE_SECOND  2
#define RESET_TYPE_BOTH    3

static void hisensor_deliver_camera_status(struct work_struct *work)
{
	rpc_info_t *rpc_info = NULL;

	if (!work) {
		cam_err("%s work is null", __func__);
		return;
	}

	rpc_info = container_of(work, rpc_info_t, rpc_work);
	if (!rpc_info) {
		cam_err("%s rpc_info is null", __func__);
		return;
	}

	if (rpc_status_change_for_camera(rpc_info->camera_status))
		cam_err("%s set_rpc %d fail", __func__,
			rpc_info->camera_status);
	else
		cam_info("%s set_rpc %d success", __func__,
			rpc_info->camera_status);
}

static int hisensor_do_hw_mipisw(hwsensor_board_info_t *b_info, int ctl)
{
	int ret = 0;

	if (!b_info) {
		cam_warn("%s. b_info is NULL", __func__);
		return 0;
	}

	if (b_info->dynamic_mipisw_num == 1) {
		if (ctl == CTL_RESET_RELEASE) {
			ret = gpio_request(b_info->gpios[MIPI_SW].gpio,
				"mipisw");
			if (ret) {
				cam_err("%s requeset mipisw pin failed",
					__func__);
				return ret;
			}
			ret = gpio_direction_output(b_info->gpios[MIPI_SW].gpio,
				b_info->mipisw_enable_value0);
			cam_info("enable mipisw, mipisw_enable_value0 = %d",
				b_info->mipisw_enable_value0);
			gpio_free(b_info->gpios[MIPI_SW].gpio);
		}
	}
	return ret;
}

static int hisensor_do_hw_reset(hwsensor_intf_t *si, int ctl, int id)
{
	sensor_t *sensor = intf_to_sensor(si);
	hwsensor_board_info_t *b_info = NULL;
	int ret = 0;

	if (!sensor) {
		cam_err("%s. sensor is NULL", __func__);
		return -EINVAL;
	}
	b_info = sensor->board_info;
	if (!b_info) {
		cam_warn("%s invalid sensor board info", __func__);
		return 0;
	}

	if (b_info->dynamic_mipisw_num > 0) {
		ret = hisensor_do_hw_mipisw(b_info, ctl);
		if (ret)
			cam_err("%s hisensor_do_hw_mipisw failed", __func__);
	}

	if (b_info->need_rpc == 1) {
		if (ctl == CTL_RESET_RELEASE)
			b_info->rpc_info.camera_status = 1; /* set rpc */
		else
			b_info->rpc_info.camera_status = 0;

		if (b_info->rpc_info.rpc_work_queue)
			queue_work(b_info->rpc_info.rpc_work_queue,
				&(b_info->rpc_info.rpc_work));
	}

	if (b_info->reset_type == RESET_TYPE_PRIMARY) {
		ret  = gpio_request(b_info->gpios[RESETB].gpio, "reset-0");
		if (ret) {
			cam_err("%s requeset reset pin failed", __func__);
			return ret;
		}

		if (ctl == CTL_RESET_HOLD) {
			ret  = gpio_direction_output(b_info->gpios[RESETB].gpio,
				b_info->hold_value);
			cam_info("PRIMARY CTL_RESET_HOLD");
		} else {
			ret  = gpio_direction_output(b_info->gpios[RESETB].gpio,
				b_info->release_value);
			cam_info("PRIMARY CTL_RESET_RELEASE");
			hw_camdrv_msleep(GPIO_CONFIG_DELAY_MS);
		}
		gpio_free(b_info->gpios[RESETB].gpio);
	} else if (b_info->reset_type == RESET_TYPE_SECOND) {
		ret = gpio_request(b_info->gpios[RESETB2].gpio, "reset-1");
		if (ret) {
			cam_err("%s requeset reset2 pin failed", __func__);
			return ret;
		}

		if (ctl == CTL_RESET_HOLD) {
			if ((id == ANDROID_HW_DUAL_PRIMARY_SECOND) ||
				(id == ANDROID_HW_DUAL_PRIMARY_BOTH)) {
				ret = gpio_direction_output(
					b_info->gpios[RESETB2].gpio,
					b_info->hold_value);
				cam_info("RESETB2 = CTL_RESET_HOLD");
			}
		} else if (ctl == CTL_RESET_RELEASE) {
			if ((id == ANDROID_HW_DUAL_PRIMARY_SECOND) ||
				(id == ANDROID_HW_DUAL_PRIMARY_BOTH)) {
				ret = gpio_direction_output(
					b_info->gpios[RESETB2].gpio,
					b_info->release_value);
				cam_info("RESETB2 = CTL_RESET_RELEASE");
				hw_camdrv_msleep(GPIO_CONFIG_DELAY_MS);
			}
		}
		gpio_free(b_info->gpios[RESETB2].gpio);
	} else if (b_info->reset_type == RESET_TYPE_BOTH) {
		ret  = gpio_request(b_info->gpios[RESETB].gpio, "reset-0");
		if (ret) {
			cam_err("%s:%d requeset reset pin failed",
				__func__, __LINE__);
			return ret;
		}

		ret = gpio_request(b_info->gpios[RESETB2].gpio, "reset-1");
		if (ret) {
			cam_err("%s:%d requeset reset pin failed",
				__func__, __LINE__);
			gpio_free(b_info->gpios[RESETB].gpio);
			return ret;
		}

		if (ctl == CTL_RESET_HOLD) {
			ret |= gpio_direction_output(b_info->gpios[RESETB].gpio,
				b_info->hold_value);
			ret |= gpio_direction_output(b_info->gpios[RESETB2].gpio,
				b_info->hold_value);
			cam_info("RESETB = CTL_RESET_HOLD, RESETB2 = CTL_RESET_HOLD");
			udelay(GPIO_CONFIG_DELAY_US);
		} else if (ctl == CTL_RESET_RELEASE) {
			if (id == ANDROID_HW_DUAL_PRIMARY_FIRST) {
				ret |= gpio_direction_output(
					b_info->gpios[RESETB].gpio,
					b_info->release_value);
				ret |= gpio_direction_output(
					b_info->gpios[RESETB2].gpio,
					b_info->hold_value);
				cam_info("RESETB = CTL_RESET_RELEASE, RESETB2 = CTL_RESET_HOLD");
			} else if (id == ANDROID_HW_DUAL_PRIMARY_BOTH) {
				ret |= gpio_direction_output(
					b_info->gpios[RESETB].gpio,
					b_info->release_value);
				ret |= gpio_direction_output(
					b_info->gpios[RESETB2].gpio,
					b_info->release_value);
				cam_info("RESETB = CTL_RESET_RELEASE, RESETB2 = CTL_RESET_RELEASE");
			} else if (id == ANDROID_HW_DUAL_PRIMARY_SECOND) {
				ret |= gpio_direction_output(
					b_info->gpios[RESETB2].gpio,
					b_info->release_value);
				ret |= gpio_direction_output(
					b_info->gpios[RESETB].gpio,
					b_info->hold_value);
				cam_info("RESETB = CTL_RESET_HOLD, RESETB2 = CTL_RESET_RELEASE");
			}
		}
		gpio_free(b_info->gpios[RESETB].gpio);
		gpio_free(b_info->gpios[RESETB2].gpio);
	} else {
		return 0;
	}

	if (ret)
		cam_err("%s set reset pin failed", __func__);
	else
		cam_info("%s: set reset state=%d, mode=%d", __func__, ctl, id);

	return ret;
}

int hisensor_config(hwsensor_intf_t *si, void *argp)
{
	int ret = 0;
	struct sensor_cfg_data *data = NULL;
	sensor_t *sensor = NULL;

	if (!si || !argp || !si->vtbl) {
		cam_err("%s si or argp is null.\n", __func__);
		return -EINVAL;
	}

	sensor = intf_to_sensor(si);
	if (!sensor) {
		cam_err("%s sensor is null.\n", __func__);
		return -EINVAL;
	}

	data = (struct sensor_cfg_data *)argp;
	cam_debug("hisensor cfgtype = %d", data->cfgtype);

	switch (data->cfgtype) {
	case SEN_CONFIG_POWER_ON:
		mutex_lock(&g_hisensor_power_lock);
		if (sensor->state == HWSENSRO_POWER_DOWN) {
			if (!si->vtbl->power_up) {
				cam_err("%s. si->vtbl->power_up is null",
					__func__);
				mutex_unlock(&g_hisensor_power_lock);
				return -EINVAL;
			} else {
				ret = si->vtbl->power_up(si);
				if (ret == 0)
					sensor->state = HWSENSOR_POWER_UP;
				else
					cam_err("%s. power up fail", __func__);
			}
		}
		mutex_unlock(&g_hisensor_power_lock);
		break;
	case SEN_CONFIG_POWER_OFF:
		mutex_lock(&g_hisensor_power_lock);
		if (sensor->state == HWSENSOR_POWER_UP) {
			if (!si->vtbl->power_down) {
				cam_err("%s. si->vtbl->power_down is null",
					__func__);
				mutex_unlock(&g_hisensor_power_lock);
				return -EINVAL;
			} else {
				ret = si->vtbl->power_down(si);
				if (ret != 0)
					cam_err("%s. power_down fail", __func__);
				sensor->state = HWSENSRO_POWER_DOWN;
			}
		}
		mutex_unlock(&g_hisensor_power_lock);
		break;
	case SEN_CONFIG_WRITE_REG:
		break;
	case SEN_CONFIG_READ_REG:
		break;
	case SEN_CONFIG_WRITE_REG_SETTINGS:
		break;
	case SEN_CONFIG_READ_REG_SETTINGS:
		break;
	case SEN_CONFIG_ENABLE_CSI:
		break;
	case SEN_CONFIG_DISABLE_CSI:
		break;
	case SEN_CONFIG_MATCH_ID:
		if (!si->vtbl->match_id) {
			cam_err("%s. si->vtbl->match_id is null", __func__);
			ret = -EINVAL;
		} else {
			ret = si->vtbl->match_id(si, argp);
		}
		break;
	case SEN_CONFIG_RESET_HOLD:
		ret = hisensor_do_hw_reset(si, CTL_RESET_HOLD, data->mode);
		break;
	case SEN_CONFIG_RESET_RELEASE:
		ret = hisensor_do_hw_reset(si, CTL_RESET_RELEASE, data->mode);
		break;
	default:
		cam_err("%s cfgtype %d is error", __func__, data->cfgtype);
		break;
	}

	cam_debug("%s exit %d", __func__, ret);
	return ret;
}

static int32_t hisensor_platform_probe(struct platform_device *pdev)
{
	int rc;

	sensor_t *sensor_ctrl = NULL;
	atomic_t *hisensor_powered = NULL;
	hwsensor_board_info_t *b_info = NULL;

	cam_info("%s enter", __func__);
	if (!pdev) {
		cam_err("%s:%d pdev is null", __func__, __LINE__);
		return -EINVAL;
	}

	sensor_ctrl = kzalloc(sizeof(*sensor_ctrl), GFP_KERNEL);
	if (!sensor_ctrl) {
		cam_err("%s:%d kzalloc failed", __func__, __LINE__);
		return -ENOMEM;
	}

	cam_info("%s:%d sensor_ctrl: %pK", __func__, __LINE__, sensor_ctrl);
	rc = hw_sensor_get_dt_data(pdev, sensor_ctrl);
	if (rc < 0) {
		cam_err("%s:%d no dt data rc %d", __func__, __LINE__, rc);
		rc = -ENODEV;
		goto hisensor_probe_fail;
	}

	rc = hw_sensor_get_dt_power_setting_data(pdev, sensor_ctrl);
	if (rc < 0) {
		cam_err("%s:%d no dt power setting data rc %d",
			__func__, __LINE__, rc);
		rc = -ENODEV;
		goto hisensor_probe_fail;
	}

	mutex_init(&g_hisensor_power_lock);

	hisensor_powered = kzalloc(sizeof(*hisensor_powered), GFP_KERNEL);
	if (!hisensor_powered) {
		cam_err("%s:%d kzalloc failed", __func__, __LINE__);
		goto hisensor_probe_fail;
	}
	sensor_ctrl->intf.vtbl = &g_hisensor_vtbl;
	sensor_ctrl->p_atpowercnt = hisensor_powered;
	sensor_ctrl->dev = &pdev->dev;
	rc = hwsensor_register(pdev, &(sensor_ctrl->intf));
	if (rc != 0) {
		cam_err("%s:%d hwsensor_register fail", __func__, __LINE__);
		goto hisensor_probe_fail;
	}

	rc = rpmsg_sensor_register(pdev, (void *)sensor_ctrl);
	if (rc != 0) {
		cam_err("%s:%d rpmsg_sensor_register fail",
			__func__, __LINE__);
		hwsensor_unregister(pdev);
		goto hisensor_probe_fail;
	}
	b_info = sensor_ctrl->board_info;
	if (b_info && b_info->need_rpc == 1) {
		b_info->rpc_info.rpc_work_queue =
			create_singlethread_workqueue(
				"camera_radio_power_ctl");
		if (!b_info->rpc_info.rpc_work_queue) {
			cam_err("%s - create workqueue error", __func__);
		} else {
			INIT_WORK(&(b_info->rpc_info.rpc_work),
				hisensor_deliver_camera_status);
			cam_info("%s - create workqueue success", __func__);
		}
	}

	return rc;

hisensor_probe_fail:
	if (sensor_ctrl->power_setting_array.power_setting)
		kfree(sensor_ctrl->power_setting_array.power_setting);

	if (sensor_ctrl->power_down_setting_array.power_setting)
		kfree(sensor_ctrl->power_down_setting_array.power_setting);

	if (sensor_ctrl->board_info) {
		kfree(sensor_ctrl->board_info);
		sensor_ctrl->board_info = NULL;
	}
	if (sensor_ctrl->p_atpowercnt) {
		kfree((atomic_t *)sensor_ctrl->p_atpowercnt);
		sensor_ctrl->p_atpowercnt = NULL;
	}
	if (sensor_ctrl) {
		kfree(sensor_ctrl);
		sensor_ctrl = NULL;
	}

	return rc;
}

static int32_t hisensor_platform_remove(struct platform_device *pdev)
{
	struct v4l2_subdev *sd = platform_get_drvdata(pdev);
	sensor_t *sensor_ctrl = NULL;
	hwsensor_board_info_t *b_info = NULL;

	if (!sd) {
		cam_err("%s: Subdevice is NULL", __func__);
		return 0;
	}

	sensor_ctrl = (sensor_t *)v4l2_get_subdevdata(sd);
	if (!sensor_ctrl) {
		cam_err("%s: eeprom device is NULL", __func__);
		return 0;
	}
	b_info = sensor_ctrl->board_info;
	if (b_info && b_info->need_rpc == 1 && b_info->rpc_info.rpc_work_queue) {
		destroy_workqueue(b_info->rpc_info.rpc_work_queue);
		b_info->rpc_info.rpc_work_queue = NULL;
		cam_info("%s - destroy workqueue success", __func__);
	}

	rpmsg_sensor_unregister((void *)sensor_ctrl);

	hwsensor_unregister(pdev);

	if (sensor_ctrl->power_setting_array.power_setting)
		kfree(sensor_ctrl->power_setting_array.power_setting);

	if (sensor_ctrl->power_down_setting_array.power_setting)
		kfree(sensor_ctrl->power_down_setting_array.power_setting);

	if (sensor_ctrl->p_atpowercnt) {
		kfree((atomic_t *)sensor_ctrl->p_atpowercnt);
		sensor_ctrl->p_atpowercnt = NULL;
	}
	if (sensor_ctrl->board_info)
		kfree(sensor_ctrl->board_info);

	kfree(sensor_ctrl);

	return 0;
}

static const struct of_device_id g_hisensor_dt_match[] = {
	{
		.compatible = "huawei,sensor",
	},
	{
	},
};

MODULE_DEVICE_TABLE(of, g_hisensor_dt_match);

static struct platform_driver g_hisensor_platform_driver = {
	.driver = {
		.name   = "huawei,sensor",
		.owner  = THIS_MODULE,
		.of_match_table = g_hisensor_dt_match,
	},
	.probe = hisensor_platform_probe,
	.remove = hisensor_platform_remove,
};

static int __init hisensor_init_module(void)
{
	cam_info("enter %s", __func__);
	return platform_driver_register(&g_hisensor_platform_driver);
}

static void __exit hisensor_exit_module(void)
{
	platform_driver_unregister(&g_hisensor_platform_driver);
}

module_init(hisensor_init_module);
module_exit(hisensor_exit_module);
MODULE_DESCRIPTION("hisensor");
MODULE_LICENSE("GPL v2");
