/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2017-2020. All rights reserved.
 * Description: peripheral head file for thermal
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

#ifndef __PERIPHERAL_TM_H
#define __PERIPHERAL_TM_H
#include <linux/thermal.h>
#include <linux/adc.h>
#define NTC_TEMP_MIN_VALUE	(-40)
#define NTC_TEMP_MAX_VALUE	125
#define ADC_RTMP_DEFAULT_VALUE	44

#ifdef CONFIG_THERMAL_CONTEXTHUB
struct contexthub_chip {
	void __iomem *share_addr;
	struct hw_chan_table adc_table[0];
};
#endif

struct periph_tsens_tm_device_sensor {
	struct thermal_zone_device *tz_dev;
	enum thermal_device_mode mode;
	unsigned int sensor_num;
	struct work_struct work;
	const char *ntc_name;
	int chanel; /* hw */
	int state;
#ifdef CONFIG_THERMAL_TRIP
	s32 temp_throttling;
	s32 temp_shutdown;
	s32 temp_below_vr_min;
	s32 temp_over_skin;
#endif
};

struct peripheral_tm_chip {
	struct platform_device *pdev;
	struct device *dev;
	struct notifier_block nb;
	struct work_struct tsens_work;
	struct delayed_work tsens_periph_tm_work;
	int average_period;
#ifdef CONFIG_THERMAL_CONTEXTHUB
	struct contexthub_chip *chub;
#endif
	int tsens_num_sensor;
	struct periph_tsens_tm_device_sensor sensor[0];
};

struct chub_ddr_info_t {
	int chub_hdr_len;
	int chub_len;
	char *p_chub_ddr;
};

extern int peripheral_ntc_2_temp(struct periph_tsens_tm_device_sensor *chip,
				      int *temp, int ntc);
extern int peripheral_temp_2_ntc(struct periph_tsens_tm_device_sensor *chip,
				      int temp, u16 *ntc);
int peripheral_get_table_info(const char *ntc_name, unsigned long *dest,
				   enum hkadc_table_id *table_id);
#endif
