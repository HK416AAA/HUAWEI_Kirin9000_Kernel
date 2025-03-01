/*
 * sensor_common.c
 *
 * Description: common driver code for sensor all.
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

#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/thermal.h>
#include "sensor_commom.h"
#include "hw_pmic.h"
#include "../../clt/kernel_clt_flag.h"

static int g_is_fpga; /* default is no fpga */
static atomic_t volatile g_powered = ATOMIC_INIT(0);

struct power_seq_type_tab {
	const char *seq_name;
	enum sensor_power_seq_type_t seq_type;
};

static struct power_seq_type_tab g_seq_type_tab[] = {
	{ "sensor_suspend", SENSOR_SUSPEND },
	{ "sensor_suspend2", SENSOR_SUSPEND2 },
	{ "sensor_pwdn", SENSOR_PWDN },
	{ "sensor_rst", SENSOR_RST },
	{ "sensor_rst2", SENSOR_RST2 },
	{ "sensor_vcm_avdd", SENSOR_VCM_AVDD },
	{ "sensor_vcm_avdd2", SENSOR_VCM_AVDD2 },
	{ "sensor_vcm_pwdn", SENSOR_VCM_PWDN },
	{ "sensor_avdd", SENSOR_AVDD },
	{ "sensor_avdd0", SENSOR_AVDD0 },
	{ "sensor_avdd1", SENSOR_AVDD1 },
	{ "sensor_avdd2", SENSOR_AVDD2 },
	{ "sensor_misp_vdd", SENSOR_MISP_VDD },
	{ "sensor_avdd1_en", SENSOR_AVDD1_EN },
	{ "sensor_avdd2_en", SENSOR_AVDD2_EN },
	{ "sensor_dvdd", SENSOR_DVDD },
	{ "sensor_dvdd2", SENSOR_DVDD2 },
	{ "sensor_ois_drv", SENSOR_OIS_DRV },
	{ "sensor_dvdd0_en", SENSOR_DVDD0_EN },
	{ "sensor_dvdd1_en", SENSOR_DVDD1_EN },
	{ "sensor_iovdd", SENSOR_IOVDD },
	{ "sensor_iovdd_en", SENSOR_IOVDD_EN },
	{ "sensor_mclk", SENSOR_MCLK },
	{ "sensor_i2c", SENSOR_I2C },
	{ "sensor_ldo_en", SENSOR_LDO_EN },
	{ "sensor_mispdcdc_en", SENSOR_MISPDCDC_EN },
	{ "sensor_check_level", SENSOR_CHECK_LEVEL },
	{ "sensor_ois", SENSOR_OIS },
	{ "sensor_ois2", SENSOR_OIS2 },
	{ "sensor_pmic", SENSOR_PMIC },
	{ "sensor_rxdphy_clk", SENSOR_RXDPHY_CLK },
	{ "sensor_cs", SENSOR_CS },
	{ "sensor_mipi_sw", SENSOR_MIPI_SW },
	{ "sensor_mipi_sw2", SENSOR_MIPI_SW2 },
};

int hw_sensor_get_thermal(const char *name, int *temp)
{
	struct thermal_zone_device *tz = NULL;
	int temperature = 0;
	int rc = 0;

	if (!temp)
		return -1;

	tz = thermal_zone_get_zone_by_name(name);
	if (IS_ERR(tz)) {
		cam_err("Error getting sensor thermal zone (%s), not yet ready?\n",
			name);
		rc = -1;
	} else {
		rc = tz->ops->get_temp(tz, &temperature);
		if (rc) {
			cam_err("Error reading temperature for gpu thermal zone: %d\n",
				rc);
			rc = -1;
		}
	}

	*temp = temperature;
	return rc;
}

int mclk_config(sensor_t *s_ctrl, unsigned int id, unsigned int clk, int on)
{
	int ret = 0;
	/* 2 , 3, 4 for different sensor */
	bool fSnclk2 = (id == 2 || id == 3 || id == 4);
	struct device *dev = NULL;

	if (!s_ctrl) {
		cam_err("%s invalid parameter\n", __func__);
		return -1;
	}
	dev = s_ctrl->dev;

	cam_info("%s enter.id %u, clk %u, on %d", __func__, id, clk, on);

	/* clk_isp_snclk max value is 48000000 */
	if ((id > 4) || (clk > 48000000)) { /* 4 for sensor 48000000 clk */
		cam_err("input(id[%d],clk[%d]) is error!\n", id, clk);
		return -1;
	}

	if (on) {
		if (id == 0) {
			s_ctrl->isp_snclk0 = devm_clk_get(dev,
				"clk_isp_snclk0");

			if (IS_ERR_OR_NULL(s_ctrl->isp_snclk0)) {
				dev_err(dev, "could not get snclk0\n");
				ret = PTR_ERR(s_ctrl->isp_snclk0);
				return ret;
			}

			ret = clk_set_rate(s_ctrl->isp_snclk0, clk);
			if (ret < 0) {
				dev_err(dev, "failed set_rate snclk0 rate\n");
				return ret;
			}

			ret = clk_prepare_enable(s_ctrl->isp_snclk0);
			if (ret) {
				dev_err(dev, "cloud not prepare_enalbe clk_isp_snclk0\n");
				return ret;
			}
		} else if (id == 1) {
			s_ctrl->isp_snclk1 = devm_clk_get(dev,
				"clk_isp_snclk1");

			if (IS_ERR_OR_NULL(s_ctrl->isp_snclk1)) {
				dev_err(dev, "could not get snclk1\n");
				ret = PTR_ERR(s_ctrl->isp_snclk1);
				return ret;
			}

			ret = clk_set_rate(s_ctrl->isp_snclk1, clk);
			if (ret < 0) {
				dev_err(dev, "failed set_rate snclk1 rate\n");
				return ret;
			}

			ret = clk_prepare_enable(s_ctrl->isp_snclk1);
			if (ret) {
				dev_err(dev, "cloud not prepare_enalbe clk_isp_snclk1\n");
				return ret;
			}
		} else if (fSnclk2) {
			s_ctrl->isp_snclk2 = devm_clk_get(dev,
				"clk_isp_snclk2");

			if (IS_ERR_OR_NULL(s_ctrl->isp_snclk2)) {
				dev_err(dev, "could not get snclk2\n");
				ret = PTR_ERR(s_ctrl->isp_snclk2);
				return ret;
			}

			ret = clk_set_rate(s_ctrl->isp_snclk2, clk);
			if (ret < 0) {
				dev_err(dev, "failed set_rate snclk2 rate\n");
				return ret;
			}

			ret = clk_prepare_enable(s_ctrl->isp_snclk2);
			if (ret) {
				dev_err(dev, "cloud not prepare_enalbe clk_isp_snclk2\n");
				return ret;
			}
		}
	} else {
		if ((id == 0) && s_ctrl->isp_snclk0) {
			clk_disable_unprepare(s_ctrl->isp_snclk0);
			cam_info("clk_disable_unprepare snclk0\n");
		} else if ((id == 1) && s_ctrl->isp_snclk1) {
			clk_disable_unprepare(s_ctrl->isp_snclk1);
			cam_info("clk_disable_unprepare snclk1\n");
		} else if (fSnclk2 && s_ctrl->isp_snclk2) {
			clk_disable_unprepare(s_ctrl->isp_snclk2);
			cam_info("clk_disable_unprepare snclk2\n");
		}
	}

	return 0;
}

int hw_mclk_config(sensor_t *s_ctrl,
	struct sensor_power_setting *power_setting, int state)
{
	int sensor_index;

	cam_info("%s enter.state:%d!", __func__, state);

	if (hw_is_fpga_board())
		return 0;

	if (power_setting->sensor_index != SENSOR_INDEX_INVALID)
		sensor_index = power_setting->sensor_index;
	else
		sensor_index = s_ctrl->board_info->sensor_index;

	mclk_config(s_ctrl, sensor_index,
		s_ctrl->board_info->mclk, state);

	if (power_setting->delay != 0)
		hw_camdrv_msleep(power_setting->delay);

	return 0;
}

int hw_sensor_gpio_config(gpio_t pin_type, hwsensor_board_info_t *sensor_info,
	struct sensor_power_setting *power_setting, int state)
{
	int rc = -1;

	if (kernel_is_clt_flag()) {
		cam_info("%s just return for CLT camera", __func__);
		return 0;
	}

	cam_info("%s enter, pin_type:%d state:%d delay:%u",
		__func__, pin_type, state, power_setting->delay);

	if (hw_is_fpga_board())
		return 0;

	if (sensor_info->gpios[pin_type].gpio == 0) {
		cam_err("gpio type[%d] is not actived", pin_type);
		return 0; /* skip this */
	}

	rc = gpio_request(sensor_info->gpios[pin_type].gpio, NULL);
	if (rc < 0) {
		cam_err("failed to request gpio[%d]",
			sensor_info->gpios[pin_type].gpio);
		return rc;
	}

	if (pin_type == FSIN) {
		cam_info("pin_level: %d",
			gpio_get_value(sensor_info->gpios[pin_type].gpio));
		rc = 0;
	} else {
		rc = gpio_direction_output(sensor_info->gpios[pin_type].gpio,
			state ? (power_setting->config_val + 1) % 2 :
			power_setting->config_val); /* 1, 2 for power */
		if (rc < 0)
			cam_err("failed to control gpio[%d]",
				sensor_info->gpios[pin_type].gpio);
		else
			/* 1, 2 for power */
			cam_info("%s config gpio[%d] output[%d]", __func__,
				sensor_info->gpios[pin_type].gpio,
				(state ? (power_setting->config_val + 1) % 2 :
				power_setting->config_val));
	}

	gpio_free(sensor_info->gpios[pin_type].gpio);

	if (power_setting->delay != 0)
		hw_camdrv_msleep(power_setting->delay);

	return rc;
}

int hw_sensor_ldo_config(ldo_index_t ldo, hwsensor_board_info_t *sensor_info,
	struct sensor_power_setting *power_setting, int state)
{
	int index;
	int rc = -1;
	const char *ldo_names[LDO_MAX] = {
		"dvdd", "dvdd2", "avdd", "avdd2",
		"vcm", "vcm2", "iopw", "misp",
		"avdd0", "avdd1", "miniisp",
		"iovdd", "oisdrv"
	};

	cam_info("%s enter, ldo:%s state:%d", __func__, ldo_names[ldo], state);

	if (hw_is_fpga_board())
		return 0;

	for (index = 0; index < sensor_info->ldo_num; index++)
		if (!strcmp(sensor_info->ldo[index].supply,
			ldo_names[ldo]))
			break;

	if (index == sensor_info->ldo_num) {
		cam_err("ldo [%s] is not actived", ldo_names[ldo]);
		return 0; /* skip this */
	}
	if (state == POWER_ON) {
		if (ldo != LDO_IOPW) {
			rc = regulator_bulk_get(sensor_info->dev, 1,
				&sensor_info->ldo[index]);
			if (rc < 0) {
				cam_err("failed to get ldo[%s]",
					ldo_names[ldo]);
				return rc;
			}

			rc = regulator_set_voltage(
				sensor_info->ldo[index].consumer,
				power_setting->config_val,
				power_setting->config_val);
			if (rc < 0) {
				cam_err("failed to set ldo[%s] to %d V",
					ldo_names[ldo],
					power_setting->config_val);
				return rc;
			}
		}
		rc = regulator_bulk_enable(1, &sensor_info->ldo[index]);
		if (rc) {
			cam_err("failed to enable regulators %d\n", rc);
			return rc;
		}
		if (power_setting->delay != 0)
			hw_camdrv_msleep(power_setting->delay);
	} else {
		if (sensor_info->ldo[index].consumer) {
			rc = regulator_bulk_disable(1,
				&sensor_info->ldo[index]);
			if (rc) {
				cam_err("failed to disable regulators %d\n",
					rc);
				return rc;
			}
			regulator_bulk_free(1, &sensor_info->ldo[index]);
		}
		rc = 0;
	}

	return rc;
}

void hw_sensor_i2c_config(sensor_t *s_ctrl,
	struct sensor_power_setting *power_setting, int state)
{
	cam_info("enter %s, state:%d", __func__, state);

	if (hw_is_fpga_board())
		return;

	if (state == POWER_ON) {
		if (power_setting->delay != 0)
			hw_camdrv_msleep(power_setting->delay);
	}
}

int hw_sensor_pmic_config(hwsensor_board_info_t *sensor_info,
	struct sensor_power_setting *power_setting, int state)
{
	int rc = 0;
	struct kernel_pmic_ctrl_t *pmic_ctrl = NULL;

	cam_info("%s enter", __func__);
	cam_info("%s seq_val=%d, config_val=%d, state=%d",
		__func__, power_setting->seq_val,
		power_setting->config_val, state);

	pmic_ctrl = kernel_get_pmic_ctrl();
	if (pmic_ctrl)
		rc = pmic_ctrl->func_tbl->pmic_seq_config(pmic_ctrl,
			power_setting->seq_val,
			power_setting->config_val, state);
	else
		cam_err("kernel_get_pmic_ctrl is null");

	if (power_setting->delay != 0)
		hw_camdrv_msleep(power_setting->delay);

	return rc;
}

int hw_sensor_phy_clk_enable(hwsensor_board_info_t *sensor_info)
{
	unsigned int i = 0;
	unsigned int clk_num = sensor_info->phy_clk_num;
	int ret = 0;

	if (clk_num == 0)
		return 0;

	if (clk_num > CSI_INDEX_MAX) {
		cam_err("clock-num is not invaild %d\n", clk_num);
		return -1;
	}

	for (i = 0; i < clk_num; i++) {
		if (IS_ERR_OR_NULL(sensor_info->phy_clk[i])) {
			cam_err("phy clk err %d\n", i);
			return -1;
		}

		ret = clk_set_rate(sensor_info->phy_clk[i],
			sensor_info->phy_clk_freq);
		if (ret < 0) {
			cam_err("set rate error %d\n",
				sensor_info->phy_clk_freq);
			return -1;
		}

		ret = clk_prepare_enable(sensor_info->phy_clk[i]);
		if (ret < 0) {
			cam_err("clk_prepare_enable err %d\n", i);
			return -1;
		}
	}

	return 0;
}

int hw_sensor_phy_clk_disable(hwsensor_board_info_t *sensor_info)
{
	unsigned int i = 0;
	unsigned int clk_num = sensor_info->phy_clk_num;

	if (clk_num == 0)
		return 0;

	if (clk_num > CSI_INDEX_MAX) {
		cam_err("clock-num is not invaild %d\n", clk_num);
		return -1;
	}

	for (i = 0; i < clk_num; i++) {
		if (IS_ERR_OR_NULL(sensor_info->phy_clk[i])) {
			cam_err("phy clk err %d\n", i);
			return -1;
		}

		clk_disable_unprepare(sensor_info->phy_clk[i]);
	}

	return 0;
}

static int kernel_sensor_power_config(sensor_t *s_ctrl,
	struct sensor_power_setting *power_setting, int on_off)
{
	int rc = 0;

	switch (power_setting->seq_type) {
	case SENSOR_DVDD:
		cam_info("%s, seq_type:%u SENSOR_DVDD", __func__,
			power_setting->seq_type);
		rc = hw_sensor_ldo_config(LDO_DVDD, s_ctrl->board_info,
			power_setting, on_off);
		break;
	case SENSOR_DVDD2:
		cam_info("%s, seq_type:%u SENSOR_DVDD2", __func__,
			power_setting->seq_type);
		rc = hw_sensor_ldo_config(LDO_DVDD2, s_ctrl->board_info,
			power_setting, on_off);
		if (rc) {
			cam_err("%s power up procedure error", __func__);
			rc = 0;
		}
		break;
	case SENSOR_OIS_DRV:
		cam_info("%s, seq_type:%u SENSOR_OIS_DRV", __func__,
			power_setting->seq_type);
		rc = hw_sensor_ldo_config(LDO_OISDRV, s_ctrl->board_info,
			power_setting, on_off);
		break;
	case SENSOR_IOVDD:
		cam_info("%s, seq_type:%u SENSOR_IOVDD", __func__,
			power_setting->seq_type);
		rc = hw_sensor_ldo_config(LDO_IOVDD, s_ctrl->board_info,
			power_setting, on_off);
		break;
	case SENSOR_AVDD:
		cam_info("%s, seq_type:%u SENSOR_AVDD", __func__,
			power_setting->seq_type);
		rc = hw_sensor_ldo_config(LDO_AVDD, s_ctrl->board_info,
			power_setting, on_off);
		break;
	case SENSOR_AVDD2:
		cam_info("%s, seq_type:%u SENSOR_AVDD2", __func__,
			power_setting->seq_type);
		rc = hw_sensor_ldo_config(LDO_AVDD2, s_ctrl->board_info,
			power_setting, on_off);
		if (rc) {
			cam_err("%s power up procedure error", __func__);
			rc = 0;
		}
		break;
	case SENSOR_VCM_AVDD:
		cam_info("%s, seq_type:%u SENSOR_VCM_AVDD", __func__,
			power_setting->seq_type);
		rc = hw_sensor_ldo_config(LDO_VCM, s_ctrl->board_info,
			power_setting, on_off);
		break;
	case SENSOR_VCM_AVDD2:
		cam_info("%s, seq_type:%u SENSOR_VCM_AVDD2", __func__,
			power_setting->seq_type);
		rc = hw_sensor_ldo_config(LDO_VCM2, s_ctrl->board_info,
			power_setting, on_off);
		if (rc) {
			cam_err("%s power up procedure error", __func__);
			rc = 0;
		}
		break;
	case SENSOR_AVDD0:
		cam_info("%s, seq_type:%u SENSOR_AVDD0", __func__,
			power_setting->seq_type);
		rc = hw_sensor_ldo_config(LDO_AVDD0, s_ctrl->board_info,
			power_setting, on_off);
		break;
	case SENSOR_AVDD1:
		cam_info("%s, seq_type:%u SENSOR_AVDD1", __func__,
			power_setting->seq_type);
		rc = hw_sensor_ldo_config(LDO_AVDD1, s_ctrl->board_info,
			power_setting, on_off);
		break;
	case SENSOR_MISP_VDD:
		cam_info("%s, seq_type:%u SENSOR_MISP_VDD", __func__,
			power_setting->seq_type);
		rc = hw_sensor_ldo_config(LDO_MISP, s_ctrl->board_info,
			power_setting, on_off);
		break;
	case SENSOR_MCLK:
		cam_info("%s, seq_type:%u SENSOR_MCLK", __func__,
			power_setting->seq_type);
		rc = hw_mclk_config(s_ctrl, power_setting, on_off);
		break;
	case SENSOR_I2C:
		if (on_off == POWER_ON) {
			cam_info("%s, seq_type:%u SENSOR_I2C", __func__,
				power_setting->seq_type);
			hw_sensor_i2c_config(s_ctrl, power_setting, on_off);
		}
		break;
	case SENSOR_LDO_EN:
		cam_info("%s, seq_type:%u SENSOR_LDO_EN", __func__,
			power_setting->seq_type);
		rc = hw_sensor_gpio_config(LDO_EN, s_ctrl->board_info,
			power_setting, on_off);
		break;
	case SENSOR_AVDD1_EN:
		cam_info("%s, seq_type:%u SENSOR_AVDD1_EN", __func__,
			power_setting->seq_type);
		rc = hw_sensor_gpio_config(AVDD1_EN, s_ctrl->board_info,
			power_setting, on_off);
		break;
	case SENSOR_DVDD0_EN:
		cam_info("%s, seq_type:%u SENSOR_DVDD0_EN", __func__,
			power_setting->seq_type);
		rc = hw_sensor_gpio_config(DVDD0_EN, s_ctrl->board_info,
			power_setting, on_off);
		break;
	case SENSOR_DVDD1_EN:
		cam_info("%s, seq_type:%u SENSOR_DVDD1_EN", __func__,
			power_setting->seq_type);
		rc = hw_sensor_gpio_config(DVDD1_EN, s_ctrl->board_info,
			power_setting, on_off);
		break;
	case SENSOR_IOVDD_EN:
		cam_info("%s, seq_type:%u SENSOR_IOVDD_EN", __func__,
			power_setting->seq_type);
		rc = hw_sensor_gpio_config(IOVDD_EN, s_ctrl->board_info,
			power_setting, on_off);
		break;
	case SENSOR_MISPDCDC_EN:
		cam_info("%s, seq_type:%u SENSOR_MISPDCDC_EN", __func__,
			power_setting->seq_type);
		rc = hw_sensor_gpio_config(MISPDCDC_EN, s_ctrl->board_info,
			power_setting, on_off);
		break;
	case SENSOR_CHECK_LEVEL:
		if (on_off == POWER_ON) {
			cam_info("%s, seq_type:%u SENSOR_CHECK_LEVEL", __func__,
				power_setting->seq_type);
			rc = hw_sensor_gpio_config(FSIN, s_ctrl->board_info,
				power_setting, on_off);
		}
		break;
	case SENSOR_RST:
		cam_info("%s, seq_type:%u SENSOR_RST", __func__,
			power_setting->seq_type);
		rc = hw_sensor_gpio_config(RESETB, s_ctrl->board_info,
			power_setting, on_off);
		break;
	case SENSOR_PWDN:
		cam_info("%s, seq_type:%u SENSOR_PWDN", __func__,
			power_setting->seq_type);
		rc = hw_sensor_gpio_config(PWDN, s_ctrl->board_info,
			power_setting, on_off);
		break;
	case SENSOR_VCM_PWDN:
		cam_info("%s, seq_type:%u SENSOR_VCM_PWDN", __func__,
			power_setting->seq_type);
		rc = hw_sensor_gpio_config(VCM, s_ctrl->board_info,
			power_setting, on_off);
		break;
	case SENSOR_SUSPEND:
		cam_info("%s, seq_type:%u SENSOR_SUSPEND", __func__,
			power_setting->seq_type);
		rc = hw_sensor_gpio_config(SUSPEND, s_ctrl->board_info,
			power_setting, POWER_OFF);
		break;
	case SENSOR_SUSPEND2:
		cam_info("%s, seq_type:%u SENSOR_SUSPEND2", __func__,
			power_setting->seq_type);
		rc = hw_sensor_gpio_config(SUSPEND2, s_ctrl->board_info,
			power_setting, POWER_OFF);
		break;
	case SENSOR_RST2:
		cam_info("%s, seq_type:%u SENSOR_RST2", __func__,
			power_setting->seq_type);
		rc = hw_sensor_gpio_config(RESETB2, s_ctrl->board_info,
			power_setting, on_off);
		break;
	case SENSOR_OIS:
		cam_info("%s, seq_type:%u SENSOR_OIS", __func__,
			power_setting->seq_type);
		rc = hw_sensor_gpio_config(OIS, s_ctrl->board_info,
			power_setting, on_off);
		break;
	case SENSOR_OIS2:
		cam_info("%s, seq_type:%u SENSOR_OIS2", __func__,
			power_setting->seq_type);
		rc = hw_sensor_gpio_config(OIS2, s_ctrl->board_info,
			power_setting, on_off);
		if (rc) {
			cam_err("%s power up procedure error", __func__);
			rc = 0;
		}
		break;
	case SENSOR_PMIC:
		cam_info("%s, seq_type:%u SENSOR_PMIC", __func__,
			power_setting->seq_type);
		rc = hw_sensor_pmic_config(s_ctrl->board_info,
			power_setting, on_off);
		break;
	case SENSOR_CS:
		break;
	case SENSOR_AVDD2_EN:
		cam_info("%s, seq_type:%u SENSOR_AVDD2_EN", __func__,
			power_setting->seq_type);
		rc = hw_sensor_gpio_config(AVDD2_EN, s_ctrl->board_info,
			power_setting, on_off);
		break;
	case SENSOR_MIPI_SW:
		cam_info("%s, seq_type:%u SENSOR_MIPI_SW", __func__,
			power_setting->seq_type);
		rc = hw_sensor_gpio_config(MIPI_SW, s_ctrl->board_info,
			power_setting, on_off);
		break;
	case SENSOR_MIPI_SW2:
		cam_info("%s, seq_type:%u SENSOR_MIPI_SW2", __func__,
			power_setting->seq_type);
		rc = hw_sensor_gpio_config(MIPI_SW2, s_ctrl->board_info,
			power_setting, on_off);
		break;
	default:
		cam_err("%s invalid seq_type %d", __func__,
			power_setting->seq_type);
		break;
	}

	return rc;
}

int hw_sensor_power_up(sensor_t *s_ctrl)
{
	struct sensor_power_setting_array *power_setting_array = NULL;
	struct sensor_power_setting *power_setting = NULL;
	int index = 0;
	int rc = 0;
	struct kernel_pmic_ctrl_t *pmic_ctrl = NULL;

	if (!s_ctrl) {
		cam_err("%s s_ctrl is null\n", __func__);
		return -1;
	}

	if (kernel_is_clt_flag()) {
		cam_info("%s just return for CLT camera", __func__);
		return 0;
	}

	cam_info("%s enter", __func__);
	power_setting_array = &s_ctrl->power_setting_array;
	if (!power_setting_array) {
		cam_err("%s power_setting_array is null\n", __func__);
		return -1;
	}

	if (s_ctrl->p_atpowercnt) {
		if (atomic_read(s_ctrl->p_atpowercnt)) {
			cam_info("%s %d: sensor has already powered up, p_atpowercnt",
				__func__, __LINE__);
			return 0;
		}
	} else {
		if (atomic_read(&g_powered)) {
			cam_info("%s %d: sensor has already powered up",
				__func__, __LINE__);
			return 0;
		}
	}

	/* fpga board compatibility */
	if (hw_is_fpga_board())
		return 0;

	rc = hw_sensor_phy_clk_enable(s_ctrl->board_info);
	if (rc)
		return rc;

	pmic_ctrl = kernel_get_pmic_ctrl();
	if (pmic_ctrl) {
		cam_info("pmic power on!");
		pmic_ctrl->func_tbl->pmic_on(pmic_ctrl, 0);
	} else {
		cam_info("%s pimc ctrl is null", __func__);
	}
	for (index = 0; index < power_setting_array->size; index++) {
		power_setting = &power_setting_array->power_setting[index];

		rc = kernel_sensor_power_config(s_ctrl, power_setting, POWER_ON);
		if (rc) {
			cam_err("%s power up procedure error", __func__);
			break;
		}
	}

	if (s_ctrl->p_atpowercnt) {
		atomic_set(s_ctrl->p_atpowercnt, 1);
		cam_info("%s %d: sensor powered up finish",
			__func__, __LINE__);
	} else {
		atomic_set(&g_powered, 1);
		cam_info("%s %d: sensor powered up finish",
			__func__, __LINE__);
	}

	return rc;
}

int hw_sensor_power_down(sensor_t *s_ctrl)
{
	struct sensor_power_setting_array *power_setting_array = NULL;
	struct sensor_power_setting *power_setting = NULL;
	unsigned int index = 0;
	int rc = 0;
	struct kernel_pmic_ctrl_t *pmic_ctrl = NULL;

	if (!s_ctrl) {
		cam_err("%s s_ctrl is null\n", __func__);
		return -1;
	}

	if (!s_ctrl->power_down_setting_array.power_setting ||
		(s_ctrl->power_down_setting_array.size == 0)) {
		cam_err("%s invalid power down setting", __func__);
		return rc;
	}

	if (kernel_is_clt_flag()) {
		cam_info("%s just return for CLT camera", __func__);
		return 0;
	}

	power_setting_array = &s_ctrl->power_down_setting_array;
	if (!power_setting_array) {
		cam_err("%s power_setting_array is null\n", __func__);
		return -1;
	}

	cam_info("%s enter", __func__);

	if (s_ctrl->p_atpowercnt) {
		if (!atomic_read(s_ctrl->p_atpowercnt)) {
			cam_info("%s %d: sensor hasn't powered up",
				__func__, __LINE__);
			return 0;
		}
	} else {
		if (!atomic_read(&g_powered)) {
			cam_info("%s %d: sensor hasn't powered up",
				__func__, __LINE__);
			return 0;
		}
	}

	for (index = 0; index < power_setting_array->size; index++) {
		power_setting = &power_setting_array->power_setting[index];
		rc = kernel_sensor_power_config(s_ctrl, power_setting, POWER_OFF);
	}

	pmic_ctrl = kernel_get_pmic_ctrl();
	if (pmic_ctrl)
		pmic_ctrl->func_tbl->pmic_off(pmic_ctrl);

	rc = hw_sensor_phy_clk_disable(s_ctrl->board_info);

	if (s_ctrl->p_atpowercnt)
		atomic_set(s_ctrl->p_atpowercnt, 0);
	else
		atomic_set(&g_powered, 0);

	return rc;
}

int hw_sensor_power_on(void *psensor)
{
	sensor_t *s_ctrl = NULL;

	if (!psensor) {
		cam_err("%s psensor is NULL!\n", __func__);
		return -1;
	}

	s_ctrl = (sensor_t *)(psensor);
	return hw_sensor_power_up(s_ctrl);
}
EXPORT_SYMBOL(hw_sensor_power_on);

int hw_sensor_power_off(void *psensor)
{
	sensor_t *s_ctrl = NULL;

	if (!psensor) {
		cam_err("%s psensor is NULL!\n", __func__);
		return -1;
	}

	s_ctrl = (sensor_t *)(psensor);
	return hw_sensor_power_down(s_ctrl);
}
EXPORT_SYMBOL(hw_sensor_power_off);

int hw_sensor_get_phy_clk(struct device *pdev, struct device_node *of_node,
	hwsensor_board_info_t *sensor_info)
{
	unsigned int i = 0;
	const char *clk_name[CSI_INDEX_MAX] = {""};
	unsigned int clk_num = 0;
	int ret = 0;

	ret = of_property_read_u32(of_node, "phy-clock-num", &clk_num);
	if (ret) {
		cam_info("invalid phy-clock-num\n");
		return -1;
	}

	if (clk_num > CSI_INDEX_MAX) {
		cam_err("phy-clock-num is not invaild %d\n", clk_num);
		return -1;
	}

	/* get clk parameters */
	ret = of_property_read_string_array(of_node, "clock-names",
		clk_name, clk_num);
	if (ret < 0) {
		cam_err("[%s] Failed : of_property_read_string_array.%d\n",
			__func__, ret);
		return -1;
	}

	for (i = 0; i < clk_num; ++i)
		cam_info("[%s] clk_name[%d] = %s\n", __func__, i, clk_name[i]);

	for (i = 0; i < clk_num; i++) {
		sensor_info->phy_clk[i] = devm_clk_get(pdev, clk_name[i]);
		if (IS_ERR_OR_NULL(sensor_info->phy_clk[i])) {
			cam_err("[%s] Failed : phyclk.%s.%d.%li\n",
				__func__, clk_name[i], i,
				PTR_ERR(sensor_info->phy_clk[i]));
			return -1;
		}
	}

	sensor_info->phy_clk_num = clk_num;

	return 0;
}

int hw_sensor_get_phy_dt_data(struct device *pdev, struct device_node *of_node,
	hwsensor_board_info_t *sensor_info)
{
	int ret = 0;

	ret = of_property_read_u32(of_node, "vendor,phyclk",
		&sensor_info->phy_clk_freq);
	if (ret) {
		cam_info("invalid vendor,phyclk\n");
		return -1;
	}

	ret = hw_sensor_get_phy_clk(pdev, of_node, sensor_info);
	if (ret)
		return -1;

	return 0;
}

int hw_sensor_get_phyinfo_data(struct device_node *of_node,
	hwsensor_board_info_t *sensor_info, int info_count)
{
	int ret = 0;
	int count = 0;

	if (!of_node || !sensor_info) {
		cam_err("%s param is invalid", __func__);
		return -1;
	}
	count = of_property_count_elems_of_size(of_node,
		"vendor,is_master_sensor", sizeof(u32));
	if (count != info_count) {
		cam_err("%s %d, count = %d", __func__, __LINE__, count);
		return -1;
	}
	ret = of_property_read_u32_array(of_node, "vendor,is_master_sensor",
		(unsigned int *)sensor_info->phyinfo.is_master_sensor,
			count);

	count = of_property_count_elems_of_size(of_node, "vendor,phy_id",
		sizeof(u32));
	if (count != info_count) {
		cam_err("%s %d, count = %d", __func__, __LINE__, count);
		return -1;
	}
	ret = (int)((unsigned int)ret |
		(unsigned int)of_property_read_u32_array(of_node,
			"vendor,phy_id",
			(unsigned int *)sensor_info->phyinfo.phy_id, count));

	count = of_property_count_elems_of_size(of_node, "vendor,phy_mode",
		sizeof(u32));
	if (count != info_count) {
		cam_err("%s %d, count = %d", __func__, __LINE__, count);
		return -1;
	}
	ret = (int)((unsigned int)ret |
		(unsigned int)of_property_read_u32_array(of_node,
			"vendor,phy_mode",
			(unsigned int *)sensor_info->phyinfo.phy_mode, count));

	count = of_property_count_elems_of_size(of_node,
		"vendor,phy_freq_mode", sizeof(u32));
	if (count != info_count) {
		cam_err("%s %d, count = %d", __func__, __LINE__, count);
		return -1;
	}
	ret = (int)((unsigned int)ret |
		(unsigned int)of_property_read_u32_array(of_node,
			"vendor,phy_freq_mode",
			(unsigned int *)sensor_info->phyinfo.phy_freq_mode,
			count));

	count = of_property_count_elems_of_size(of_node, "vendor,phy_freq",
		sizeof(u32));
	if (count != info_count) {
		cam_err("%s %d, count = %d", __func__, __LINE__, count);
		return -1;
	}
	ret = (int)((unsigned int)ret |
		(unsigned int)of_property_read_u32_array(of_node,
			"vendor,phy_freq",
			(unsigned int *)sensor_info->phyinfo.phy_freq,
			count));

	count = of_property_count_elems_of_size(of_node,
		"vendor,phy_work_mode", sizeof(u32));
	if (count != info_count) {
		cam_err("%s %d, count = %d", __func__, __LINE__, count);
		return -1;
	}
	ret = (int)((unsigned int)ret |
		(unsigned int)of_property_read_u32_array(of_node,
			"vendor,phy_work_mode",
			(unsigned int *)sensor_info->phyinfo.phy_work_mode,
			count));

	cam_info("%s, info_count = %d\n"
		"is_master_sensor[0] = %d, is_master_sensor[1] = %d\n"
		"phy_id[0] = %d, phy_id[1] = %d\n"
		"phy_mode[0] = %d, phy_mode[1] = %d\n"
		"phy_freq_mode[0] = %d, phy_freq_mode[1] = %d\n"
		"phy_freq[0] = %d, phy_freq[1] = %d\n"
		"phy_work_mode[0] = %d, phy_work_mode[1] = %d",
		__func__, info_count,
		sensor_info->phyinfo.is_master_sensor[0],
		sensor_info->phyinfo.is_master_sensor[1],
		sensor_info->phyinfo.phy_id[0],
		sensor_info->phyinfo.phy_id[1],
		sensor_info->phyinfo.phy_mode[0],
		sensor_info->phyinfo.phy_mode[1],
		sensor_info->phyinfo.phy_freq_mode[0],
		sensor_info->phyinfo.phy_freq_mode[1],
		sensor_info->phyinfo.phy_freq[0],
		sensor_info->phyinfo.phy_freq[1],
		sensor_info->phyinfo.phy_work_mode[0],
		sensor_info->phyinfo.phy_work_mode[1]
	);

	return ret;
}

int hw_sensor_get_dt_data(struct platform_device *pdev,
	sensor_t *sensor)
{
	struct device_node *of_node = pdev->dev.of_node;
	hwsensor_board_info_t *sensor_info = NULL;
	int rc = 0;
	int ret = 0;
	int count = 0;
	u32 i;
	u32 index = 0;
	char *gpio_tag = NULL;
	/* enum gpio_t */
	const char *gpio_ctrl_types[IO_MAX] = {
		"reset", "fsin", "pwdn", "vcm_pwdn",
		"suspend", "suspend2", "reset2", "ldo_en",
		"ois", "ois2", "dvdd0-en", "dvdd1-en",
		"iovdd-en", "mispdcdc-en", "mipisw", "reset3",
		"pwdn2", "avdd1_en", "avdd2_en",
		" ", " ", " ", "mipisw2"
	};

	cam_info("enter %s", __func__);
	sensor_info = kzalloc(sizeof(hwsensor_board_info_t), GFP_KERNEL);
	if (!sensor_info) {
		cam_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}
	sensor->board_info = sensor_info;
	sensor_info->dev = &(pdev->dev);

	rc = of_property_read_string(of_node, "huawei,sensor_name",
		&sensor_info->name);
	cam_info("%s huawei,sensor_name %s, rc %d\n", __func__,
		sensor_info->name, rc);
	if (rc < 0) {
		cam_err("%s failed %d\n", __func__, __LINE__);
		goto fail;
	}

	rc = of_property_read_u32(of_node, "vendor,is_fpga",
		&g_is_fpga);
	cam_info("%s vendor,is_fpga: %d, rc %d\n", __func__,
		g_is_fpga, rc);
	if (rc < 0) {
		cam_err("%s failed %d\n", __func__, __LINE__);
		goto fail;
	}

	rc = of_property_read_u32(of_node, "huawei,sensor_index",
		(u32 *)(&sensor_info->sensor_index));
	cam_info("%s huawei,sensor_index %d, rc %d\n", __func__,
		sensor_info->sensor_index, rc);
	if (rc < 0) {
		cam_err("%s failed %d\n", __func__, __LINE__);
		goto fail;
	}

	rc = of_property_read_u32(of_node, "vendor,pd_valid",
		&sensor_info->power_conf.pd_valid);
	cam_info("%s vendor,pd_valid %d, rc %d\n", __func__,
		sensor_info->power_conf.pd_valid, rc);
	if (rc < 0) {
		cam_err("%s failed %d\n", __func__, __LINE__);
		goto fail;
	}

	rc = of_property_read_u32(of_node, "vendor,reset_valid",
		&sensor_info->power_conf.reset_valid);
	cam_info("%s vendor,reset_valid %d, rc %d\n", __func__,
		sensor_info->power_conf.reset_valid, rc);
	if (rc < 0) {
		cam_err("%s failed %d\n", __func__, __LINE__);
		goto fail;
	}

	rc = of_property_read_u32(of_node, "vendor,vcmpd_valid",
		&sensor_info->power_conf.vcmpd_valid);
	cam_info("%s vendor,vcmpd_valid %d, rc %d\n", __func__,
		sensor_info->power_conf.vcmpd_valid, rc);
	if (rc < 0) {
		cam_err("%s failed %d\n", __func__, __LINE__);
		goto fail;
	}

	/* add csi_index and i2c_index for dual camera. */
	count = of_property_count_elems_of_size(of_node, "vendor,csi_index",
		sizeof(u32));
	if (count > 0) {
		ret = of_property_read_u32_array(of_node, "vendor,csi_index",
			(unsigned int *)&sensor_info->csi_id, count);
	} else {
		sensor_info->csi_id[0] = sensor_info->sensor_index;
		sensor_info->csi_id[1] = -1;
	}
	cam_info("sensor:%s csi_id[0-1]=%d:%d", sensor_info->name,
		sensor_info->csi_id[0], sensor_info->csi_id[1]);

	count = of_property_count_elems_of_size(of_node, "vendor,i2c_index",
		sizeof(u32));
	if (count > 0) {
		ret = of_property_read_u32_array(of_node, "vendor,i2c_index",
			(unsigned int *)&sensor_info->i2c_id, count);
	} else {
		sensor_info->i2c_id[0] = sensor_info->sensor_index;
		sensor_info->i2c_id[1] = -1;
	}
	cam_info("sensor:%s i2c_id[0-1]=%d:%d", sensor_info->name,
		sensor_info->i2c_id[0], sensor_info->i2c_id[1]);

	/* FPGA IGNORE */
	if (hw_is_fpga_board())
		return rc;

	rc = hw_sensor_get_phy_dt_data(&pdev->dev, of_node, sensor_info);
	if (rc < 0)
		cam_info("%s phy data not ready %d\n", __func__, __LINE__);

	rc = of_property_read_u32(of_node, "vendor,mclk",
		&sensor_info->mclk);
	cam_info("%s vendor,mclk 0x%x, rc %d\n", __func__,
		sensor_info->mclk, rc);
	if (rc < 0) {
		cam_err("%s failed %d\n", __func__, __LINE__);
		goto fail;
	}

	/* get ldo */
	sensor_info->ldo_num = of_property_count_strings(of_node,
		"vendor,ldo-names");
	if (sensor_info->ldo_num < 0) {
		cam_err("%s failed %d\n", __func__, __LINE__);
	} else {
		cam_info("ldo num = %d", sensor_info->ldo_num);
		for (i = 0; i < sensor_info->ldo_num; i++) {
			rc = of_property_read_string_index(of_node,
				"vendor,ldo-names", i,
				&sensor_info->ldo[i].supply);
			if (rc < 0) {
				cam_err("%s failed %d\n", __func__, __LINE__);
				goto fail;
			}
		}
	}

	sensor_info->gpio_num = of_gpio_count(of_node);
	if (sensor_info->gpio_num < 0) {
		cam_err("%s failed %d, ret is %d\n", __func__,
			__LINE__, sensor_info->gpio_num);
		goto fail;
	}

	cam_info("gpio num = %d", sensor_info->gpio_num);
	for (i = 0; i < sensor_info->gpio_num; i++) {
		rc = of_property_read_string_index(of_node,
			"huawei,gpio-ctrl-types",
			i, (const char **)&gpio_tag);
		if (rc < 0) {
			cam_err("%s failed %d", __func__, __LINE__);
			goto fail;
		}
		for (index = 0; index < IO_MAX; index++) {
			if (gpio_ctrl_types[index]) {
				if (!strcmp(gpio_ctrl_types[index], gpio_tag))
					sensor_info->gpios[index].gpio =
						of_get_gpio(of_node, i);
			}
		}
		cam_info("gpio ctrl types: %s", gpio_tag);
	}

	rc = of_property_read_u32(of_node, "module_type",
		&sensor_info->module_type);
	cam_info("%s module_type 0x%x, rc %d\n", __func__,
		sensor_info->module_type, rc);
	if (rc < 0) {
		sensor_info->module_type = 0;
		cam_warn("%s read module_type failed, rc %d, set default value %d\n",
		__func__, rc, sensor_info->module_type);
		rc = 0;
	}

	rc = of_property_read_u32(of_node, "reset_type",
		&sensor_info->reset_type);
	cam_info("%s reset_type 0x%x, rc %d\n", __func__,
		sensor_info->reset_type, rc);
	if (rc < 0) {
		sensor_info->reset_type = 0;
		cam_warn("%s read reset_type failed, rc %d, set default value %d\n",
		__func__, rc, sensor_info->reset_type);
		rc = 0;
	}

	rc = of_property_read_u32(of_node, "release_value",
		&sensor_info->release_value);
	cam_info("%s release_value 0x%x, rc %d\n", __func__,
		sensor_info->release_value, rc);
	if (rc < 0) {
		sensor_info->release_value = 0;
		cam_warn("%s read release_value failed, rc %d, set default value %d\n",
		__func__, rc, sensor_info->release_value);
		rc = 0;
	}

	rc = of_property_read_u32(of_node, "hold_value",
		&sensor_info->hold_value);
	cam_info("%s hold_value 0x%x, rc %d\n", __func__,
		sensor_info->hold_value, rc);
	if (rc < 0) {
		sensor_info->hold_value = 0;
		cam_warn("%s read hold_value failed, rc %d, set default value %d\n",
		__func__, rc, sensor_info->hold_value);
		rc = 0;
	}

	rc = of_property_read_u32(of_node, "vendor,topology_type",
		&sensor_info->topology_type);
	cam_info("%s topology_type 0x%x, rc %d\n", __func__,
		sensor_info->topology_type, rc);
	if (rc < 0) {
		/* set invalid(-1) as default */
		sensor_info->topology_type = -1;
		cam_warn("%s read topology_type failed, rc %d, set default value %d\n",
			__func__, rc, sensor_info->topology_type);
		rc = 0;
	}

	rc = of_property_read_u32(of_node, "vendor,phyinfo_valid",
		(u32 *)&sensor_info->phyinfo_valid);
	cam_info("%s sensor_info->phyinfo_valid %d, rc %d\n", __func__,
		sensor_info->phyinfo_valid, rc);
	if (!rc && (sensor_info->phyinfo_valid == 1 ||
		sensor_info->phyinfo_valid == 2))
		/* 2 for phy sensor */
		rc = hw_sensor_get_phyinfo_data(of_node, sensor_info,
			sensor_info->phyinfo_valid);

	rc = of_property_read_u32(of_node, "dynamic_mipisw_num",
		&sensor_info->dynamic_mipisw_num);
	cam_info("%s dynamic_mipisw_num 0x%x, rc %d\n", __func__,
		sensor_info->dynamic_mipisw_num, rc);
	if (rc < 0) {
		sensor_info->dynamic_mipisw_num = 0;
		cam_warn("%s read dynamic_mipisw_num failed, rc %d, set default value %d\n",
		__func__, rc, sensor_info->dynamic_mipisw_num);
		rc = 0;
	}

	if (sensor_info->dynamic_mipisw_num == 1) {
		rc = of_property_read_u32(of_node, "mipisw_enable_value0",
			&sensor_info->mipisw_enable_value0);
		cam_info("%s mipisw_enable_value0 0x%x, rc %d\n", __func__,
			sensor_info->mipisw_enable_value0, rc);
		if (rc < 0) {
			sensor_info->mipisw_enable_value0 = 0;
			cam_warn("%s read mipisw_enable_value0 failed, rc %d, set default value %d\n",
			__func__, rc, sensor_info->mipisw_enable_value0);
			rc = 0;
		}
	} else if (sensor_info->dynamic_mipisw_num == 2) {
		/* 2 for dynamic sensor */
		rc = of_property_read_u32(of_node, "mipisw_enable_value0",
			&sensor_info->mipisw_enable_value0);
		cam_info("%s mipisw_enable_value0 0x%x, rc %d\n", __func__,
			sensor_info->mipisw_enable_value0, rc);
		if (rc < 0) {
			sensor_info->mipisw_enable_value0 = 0;
			cam_warn("%s read mipisw_enable_value0 failed, rc %d, set default value %d\n",
			__func__, rc, sensor_info->mipisw_enable_value0);
			rc = 0;
		}
		rc = of_property_read_u32(of_node, "mipisw_enable_value1",
			&sensor_info->mipisw_enable_value1);
		cam_info("%s mipisw_enable_value1 0x%x, rc %d\n", __func__,
			sensor_info->mipisw_enable_value1, rc);
		if (rc < 0) {
			sensor_info->mipisw_enable_value1 = 0;
			cam_warn("%s read mipisw_enable_value1 failed, rc %d, set default value %d\n",
			__func__, rc, sensor_info->mipisw_enable_value1);
			rc = 0;
		}
	}

	rc = of_property_read_u32(of_node, "need_rpc", &sensor_info->need_rpc);
	cam_info("%s need_rpc 0x%x, rc %d\n", __func__,
		sensor_info->need_rpc, rc);
	if (rc < 0) {
		sensor_info->need_rpc = 0;
		cam_warn("%s read need_rpc failed, rc %d, set default value %d\n",
			  __func__, rc, sensor_info->need_rpc);
		rc = 0;
	}

	return rc;
fail:
	cam_err("%s error exit\n", __func__);
	kfree(sensor_info);
	sensor_info = NULL;
	sensor->board_info = NULL;
	return rc;
}

static int hw_sensor_get_dt_power_setting(struct device_node *dev_node,
	struct sensor_power_setting_array *power_setting_array, int is_power_on)
{
	int rc = 0;
	int i = 0;
	int j = 0;
	int count = 0;
	int seq_size = 0;
	const char *seq_type_name = NULL;
	const char *seq_val_name = NULL;
	const char *seq_cfg_name = NULL;
	const char *seq_sensor_index_name = NULL;
	const char *seq_delay_name = NULL;
	const char *seq_name = NULL;
	uint32_t *seq_vals = NULL;
	uint32_t *cfg_vals = NULL;
	uint32_t *sensor_indexs = NULL;
	uint32_t *seq_delays = NULL;
	struct sensor_power_setting *power_settings = NULL;

	cam_info("%s:%d is_power_on = %d", __func__, __LINE__, is_power_on);
	seq_type_name = (is_power_on ? "vendor,cam-power-seq-type" :
		"vendor,cam-power-down-seq-type");
	count = of_property_count_strings(dev_node, seq_type_name);
	if (count <= 0) {
		cam_warn("%s:%d power settings not to config",
			__func__, __LINE__);
		return -EINVAL;
	}

	seq_vals = kzalloc(sizeof(uint32_t) * count, GFP_KERNEL);
	if (!seq_vals) {
		cam_err("%s:%d failed\n", __func__, __LINE__);
		return -ENOMEM;
	}

	seq_val_name = (is_power_on ? "vendor,cam-power-seq-val" :
		"vendor,cam-power-down-seq-val");
	rc = of_property_read_u32_array(dev_node, seq_val_name,
		seq_vals, count);
	if (rc < 0)
		cam_warn("%s:%d seq val not to config", __func__, __LINE__);

	cfg_vals = kzalloc(sizeof(uint32_t) * count, GFP_KERNEL);
	if (!cfg_vals) {
		cam_err("%s:%d failed\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto OUT;
	}

	seq_cfg_name = (is_power_on ? "vendor,cam-power-seq-cfg-val" :
		"vendor,cam-power-down-seq-cfg-val");
	rc = of_property_read_u32_array(dev_node, seq_cfg_name,
		cfg_vals, count);
	if (rc < 0)
		cam_warn("%s:%d seq val not to config", __func__, __LINE__);

	sensor_indexs = kzalloc(sizeof(uint32_t) * count, GFP_KERNEL);
	if (!sensor_indexs) {
		cam_err("%s failed %d", __func__, __LINE__);
		rc = -ENOMEM;
		goto OUT;
	}

	seq_sensor_index_name = (is_power_on ?
		"vendor,cam-power-seq-sensor-index" :
		"vendor,cam-power-down-seq-sensor-index");
	rc = of_property_read_u32_array(dev_node, seq_sensor_index_name,
		sensor_indexs, count);
	if (rc < 0)
		cam_warn("%s:%d sensor index not to config",
			__func__, __LINE__);

	seq_delays = kzalloc(sizeof(uint32_t) * count, GFP_KERNEL);
	if (!seq_delays) {
		cam_err("%s failed %d", __func__, __LINE__);
		rc = -ENOMEM;
		goto OUT;
	}

	seq_delay_name = (is_power_on ? "vendor,cam-power-seq-delay" :
		"vendor,cam-power-down-seq-delay");
	rc = of_property_read_u32_array(dev_node, seq_delay_name,
		seq_delays, count);
	if (rc < 0)
		cam_err("%s:%d seq delay not to config", __func__, __LINE__);

	power_settings = kzalloc(sizeof(struct sensor_power_setting) *
		count, GFP_KERNEL);
	if (!power_settings) {
		cam_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto OUT;
	}
	power_setting_array->power_setting = power_settings;
	power_setting_array->size = count;

	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(dev_node, seq_type_name,
			i, &seq_name);
		if (rc < 0) {
			cam_err("%s failed %d\n", __func__, __LINE__);
			goto OUT;
		}

		seq_size = (int)ARRAY_SIZE(g_seq_type_tab);
		for (j = 0; j < seq_size; j++) {
			if (!strcmp(seq_name, g_seq_type_tab[j].seq_name)) {
				power_settings[i].seq_type =
					g_seq_type_tab[j].seq_type;
				break;
			}
		}

		if (j >= seq_size) {
			cam_warn("%s: unrecognized seq-type\n", __func__);
			rc = -EINVAL;
			goto OUT;
		}

		power_settings[i].seq_val = seq_vals[i];
		power_settings[i].config_val = cfg_vals[i];
		power_settings[i].sensor_index = ((sensor_indexs[i] >= 0xFF) ?
			0xFFFFFFFF : sensor_indexs[i]);
		power_settings[i].delay = seq_delays[i];
		cam_info("%s:%d index[%d] seq_name[%s] seq_type[%d] cfg_vals[%d] seq_delay[%d] sensor_index[0x%x]",
			__func__, __LINE__, i, seq_name,
			power_settings[i].seq_type,
			cfg_vals[i], seq_delays[i],
			sensor_indexs[i]);
		cam_info("%s:%d sensor_index = %d", __func__,
			__LINE__, power_settings[i].sensor_index);
	}

OUT:
	if (seq_vals) {
		kfree(seq_vals);
		seq_vals = NULL;
	}
	if (cfg_vals) {
		kfree(cfg_vals);
		cfg_vals = NULL;
	}
	if (sensor_indexs) {
		kfree(sensor_indexs);
		sensor_indexs = NULL;
	}
	if (seq_delays) {
		kfree(seq_delays);
		seq_delays = NULL;
	}

	return rc;
}

int hw_sensor_get_dt_power_setting_data(struct platform_device *pdev,
	sensor_t *sensor)
{
	int rc = 0;
	struct device_node *dev_node = NULL;

	if (!pdev || !pdev->dev.of_node || !sensor) {
		cam_err("%s dev_node is NULL", __func__);
		return -EINVAL;
	}

	dev_node = pdev->dev.of_node;

	rc = hw_sensor_get_dt_power_setting(dev_node,
		&sensor->power_setting_array, 1);
	if (rc < 0) {
		cam_err("%s:%d get dt power on setting fail",
			__func__, __LINE__);
		return rc;
	}

	rc = hw_sensor_get_dt_power_setting(dev_node,
		&sensor->power_down_setting_array, 0); /* 0 is power down */
	if (rc < 0) {
		cam_warn("%s:%d get dt power down setting fail, need not to config",
			__func__, __LINE__);
		return 0;
	}
	return 0;
}

void hw_camdrv_msleep(unsigned int ms)
{
	/* 1000 * 1000 for ktime  */
	ktime_t wait = ns_to_ktime((u64)ms * 1000 * 1000);

	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_hrtimeout(&wait, HRTIMER_MODE_REL);
}
EXPORT_SYMBOL(hw_camdrv_msleep);

int hw_is_fpga_board(void)
{
	cam_info("%s is_fpga=%d", __func__, g_is_fpga);
	return g_is_fpga;
}
EXPORT_SYMBOL(hw_is_fpga_board);

