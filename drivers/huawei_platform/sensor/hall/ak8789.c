/*
 * ak8987.c
 *
 * hall_ak8987 driver
 *
 * Copyright (c) 2016-2019 Huawei Technologies Co., Ltd.
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

#include "ak8789.h"

#include <linux/delay.h>
#include <linux/version.h>
#ifdef CONFIG_GPIO_HI6502
#include <platform_include/basicplatform/linux/hisi_hi6502_gpio.h>
#endif
#include <huawei_platform/inputhub/default/sensor_detect.h>
#include "hall_sensor.h"

#include <linux/fb.h>
#include <linux/completion.h>
#include <linux/time64.h>

#ifdef HWLOG_TAG
#undef HWLOG_TAG
#endif

#define HWLOG_TAG ak8789
HWLOG_REGIST();

static const struct of_device_id ak8789_match_table[] = {
	{ .compatible = "huawei,hall_sensor,ak8789", },
	{},
};

MODULE_DEVICE_TABLE(of, ak8789_match_table);

#ifdef HALL_DATA_REPORT_INPUTHUB
static struct ak8789_data *data_ak8789;
#endif

#ifdef HALL_DSM_CONFIG
static struct dsm_dev dsm_hall = {
	.name = "dsm_hall",
	.device_name = "hal",
	.ic_name = NULL,
	.module_name = NULL,
	.fops = NULL,
	.buff_size = 1024, // hall dsm buf size
};
#endif

#define PEN_BOTH_IRQ_VALUE       48
#define MINUTES_TO_NS 60000000000L
static atomic_t cur_irq_num = ATOMIC_INIT(0);
static int hall_irq_num[8] = {0};
static bool is_screen_on = true;
static bool is_pen_irq_disable = false;
static unsigned int pen_irq_cnt = 0;
static unsigned int pen_both_on_cnt = 0;
static struct timespec64 pre_ts;
static int64_t limit_time = 10 * MINUTES_TO_NS;
static unsigned int limit_irq_cnt = 40;
static unsigned int limit_both_on_cnt = 3;
static spinlock_t irq_lock;
static unsigned long irq_flag = 0;
static unsigned int workaround_is_support = 0;
static unsigned int limit_duration = 0;
static unsigned int limit_pen_irq_cnt = 0;
static unsigned int limit_pen_both_on_cnt = 0;


#if defined(HALL_DATA_REPORT_INPUTHUB) && defined(HALL_INPUTHUB_ROUTE)
extern int sensor_pmic_power_check(void);
#else
static inline int sensor_pmic_power_check(void)
{
	return 0;
}
#endif

static char *pole_name[MAX_TYPE_NUM] = { "north", "south" };

static unsigned int hall_gpio_get_value(unsigned int gpio_type, int gpio)
{
	int value;

	/* judge bit 0 is 1 */
	if (gpio_type & 0x01)
		value = gpio_get_value_cansleep(gpio);
	else
		value = gpio_get_value(gpio);

	return (unsigned int)value;
}

static void process_pen_irq(unsigned int value)
{
	struct timespec64 cur_ts;
	struct timespec64 sub_ts;
	int64_t ns;
	int irq_num = atomic_read(&cur_irq_num);

	atomic_set(&cur_irq_num, 0);
	if (is_screen_on) {
		pen_irq_cnt = 0;
		pen_both_on_cnt = 0;
		return;
	}
	if (!(irq_num == hall_irq_num[4] || irq_num == hall_irq_num[5]))
		return;

	if (pen_irq_cnt == 0) {
		ktime_get_real_ts64(&pre_ts);
		hwlog_info("set pre_ts first time when screen off\n");
	}

	pen_irq_cnt++;
	if ((value & PEN_BOTH_IRQ_VALUE) == PEN_BOTH_IRQ_VALUE)
		pen_both_on_cnt++;

	ktime_get_real_ts64(&cur_ts);
	sub_ts = timespec64_sub(cur_ts, pre_ts);
	ns = timespec64_to_ns(&sub_ts);
	if (ns >= limit_time) {
		pen_irq_cnt = 0;
		pen_both_on_cnt = 0;
		hwlog_info("sub_ts is over 10min\n");
	}

	if (pen_irq_cnt >= limit_irq_cnt || pen_both_on_cnt >= limit_both_on_cnt) {
		pen_irq_cnt = 0;
		pen_both_on_cnt = 0;
		spin_lock_irqsave(&irq_lock, irq_flag);
		if (is_pen_irq_disable == false) {
			disable_irq(hall_irq_num[4]);
			if (hall_irq_num[5])
				disable_irq(hall_irq_num[5]);
			is_pen_irq_disable = true;
		}
		hwlog_info("Shield pen interrupt.\n");
		spin_unlock_irqrestore(&irq_lock, irq_flag);
	}

	hwlog_info("sub_ts:%llu, pen_irq_cnt:%u, pen_both_on_cnt:%u, is_pen_irq_disable:%d.\n",
			ns, pen_irq_cnt, pen_both_on_cnt, is_pen_irq_disable);
}

static packet_data get_single_pole_data(struct hall_device *h_dev,
					enum hall_type type)
{
	packet_data p_data = 0;
	unsigned int value;

	value = hall_gpio_get_value(h_dev->hall_gpio_type,
				    h_dev->h_info.gpio[type]);

	if (h_dev->h_info.auxiliary_io[type] == 0) {
		p_data |= (!value) << type;
		return p_data;
	}

	if (value == 0) {
		p_data |= (!value) << type;
		return p_data;
	}

	hwlog_info("%s : type = %d\n", __func__, type);
	return p_data;
}

static packet_data ak8789_get_data(struct hall_device *h_dev)
{
	packet_data p_data = 0;

	if (h_dev == NULL) {
		hwlog_err("%s done\n", __func__);
		return -1;
	}

	if (support_type_north(h_dev->hall_type))
		p_data |= get_single_pole_data(h_dev, NORTH);

	if (support_type_south(h_dev->hall_type))
		p_data |= get_single_pole_data(h_dev, SOUTH);

	return p_data;
}

static int ak8789_get_info(struct hall_cdev *cdev, char *buf, unsigned int len)
{
	unsigned int lave_size = len;
	const unsigned int max_size = len;
	unsigned int use_size = 0;
	int nwr_size;
	char *temp = buf;
	struct ak8789_data *data = container_of(cdev, struct ak8789_data, cdev);
	struct hall_device *temp_dev = NULL;

	if (data == NULL || buf == NULL) {
		hwlog_err("%s: null point\n", __func__);
		return -1;
	}

	nwr_size = snprintf(temp, lave_size,
			    "[hall]total count:%d\n",
			    data->count);
	if (nwr_size < 0) {
		hwlog_err("Get ak8789 info err");
		return -1;
	}
	temp += nwr_size;
	lave_size -= nwr_size;
	use_size += nwr_size;
	list_for_each_entry(temp_dev, &data->head, list) {
		if (lave_size <= 0) {
			hwlog_err("%s: too many info\n", __func__);
			return -1;
		}

		nwr_size = snprintf(temp, lave_size,
			"[hall]id:%u; type:%u;wakeup:%d; x_coor:%d; y_coor:%d\n",
			temp_dev->hall_id,
			temp_dev->hall_type,
			temp_dev->hall_hw_wakeup_support,
			temp_dev->hall_x_coordinate,
			temp_dev->hall_y_coordinate);
		if (nwr_size < 0) {
			hwlog_err("Get ak8789 info err");
			return -1;
		}

		temp += nwr_size;
		lave_size -= nwr_size;
		use_size += nwr_size;
	}

	if (use_size >= max_size - 2) { // buf is full
		*(temp + max_size - 2) = '\n';
		*(temp + max_size - 1) = '\0';
	} else {
		*(temp + use_size + 1) = '\n';
		*(temp + use_size + 2) = '\0';
	}
	return 0;
}

static int ak8789_get_status(struct hall_cdev *cdev)
{
	packet_data temp_data;
	packet_data pdata = 0;
	unsigned int index;
	struct ak8789_data *data = container_of(cdev, struct ak8789_data, cdev);
	struct hall_device *temp_dev = NULL;

	if (data == NULL) {
		hwlog_err("%s: null point\n", __func__);
		return pdata;
	}

	list_for_each_entry(temp_dev, &data->head, list) {
		temp_data = ak8789_get_data(temp_dev);
		/* not care overflow */
		index = temp_dev->hall_id ? 1 << (temp_dev->hall_id) : 0;
		pdata |= temp_data << index;
	}

	return pdata;
}

static bool ak8789_lightstrap_get_hall_status(void)
{
	int value;
	uint32_t light_status;
	uint32_t hall_lightstrap_value = get_hall_lightstrap_value();

	value = ak8789_get_status(&data_ak8789->cdev);
	light_status = (uint32_t)value & hall_lightstrap_value;

	hwlog_info("%s：hall value is %d, status is %d\n",
		__func__, value, light_status);

	if (!light_status)
		return false;

	return true;
}

static struct hall_interface_ops ak8789_ops = {
	.get_hall_status = ak8789_lightstrap_get_hall_status,
};

static int ak8789_report_status(struct hall_cdev *cdev)
{
	const unsigned int delay_time = 50; // delay 50ms

	return ak8789_register_report_data(delay_time);
}

#ifdef HALL_TEST
static inline void ak8789_test_info_init(struct hall_cdev *cdev)
{
	cdev->count = HALL_REPORT_MIN_COUNT;
	cdev->period = HALL_REPORT_MAX_PERIOD;
	cdev->enable = 0;
	cdev->value = 0;
	cdev->flag = 0;
}

static void ak8789_set_report_value(struct hall_cdev *cdev)
{
	int i;
	int temp;
	struct ak8789_data *data = container_of(cdev, struct ak8789_data, cdev);
	int count = cdev->count;
	packet_data pdata = cdev->value;
	int period = cdev->period;
	int flag = cdev->flag;

	if (cdev->enable == 0) {
		hwlog_info("test switch off, open the switch to test\n");
		return;
	}

	hwlog_info("value:%u; flag:%d; count:%d; period:%d\n", pdata, flag,
		   count, period);

	for (i = 0; i < count; i++) {
		ak8789_report_data(data, pdata);
		msleep(period);
		temp = pdata & (~flag);
		pdata = ~pdata;
		pdata &= flag;
		pdata |= temp;
	}

	ak8789_test_info_init(cdev);
}
#endif

#ifdef HALL_DATA_REPORT_INPUTHUB
int hall_first_report(bool enable)
{
	packet_data temp_data;
	packet_data pdata = 0;
	struct hall_device *temp_dev = NULL;
	struct ak8789_data *data = data_ak8789;

	if (data == NULL || list_empty(&data->head)) {
		hwlog_err("none ak8789 data or list is null\n");
		return 0;
	}

	if (!enable)
		return 0;

	list_for_each_entry(temp_dev, &data->head, list) {
		temp_data = ak8789_get_data(temp_dev);
		pdata |= temp_data << (temp_dev->hall_id << 1);
	}

	hwlog_info("[%s]pdata:%u\n", __func__, pdata);
	return ak8789_report_data(data, pdata);
}
EXPORT_SYMBOL_GPL(hall_first_report);

int ak8789_register_report_data(int ms)
{
	struct hall_device *temp_dev = NULL;
	struct ak8789_data *data = data_ak8789;

	if (data == NULL || list_empty(&data->head)) {
		hwlog_err("%s: none ak8789 data\n", __func__);
		return 0;
	}

	temp_dev = list_first_entry(&data->head, struct hall_device, list);
	queue_delayed_work(data->hall_wq,
			   &temp_dev->h_delayed_work,
			   HZ / 20); // use fixed time 20

	hwlog_info("%s: ms = %d\n", __func__, ms);
	return 1;
}
EXPORT_SYMBOL_GPL(ak8789_register_report_data);
#else
int hall_first_report(bool enable)
{
	hwlog_info("%s: enable = %d\n", __func__, enable);
	return 1;
}
EXPORT_SYMBOL_GPL(hall_first_report);

int ak8789_register_report_data(int ms)
{
	hwlog_info("%s: ms = %d\n", __func__, ms);
	return 1;
}
EXPORT_SYMBOL_GPL(ak8789_register_report_data);
#endif

static int ak8789_report_debug_data(struct hall_cdev *cdev, packet_data pdata)
{
	struct ak8789_data *data = container_of(cdev, struct ak8789_data, cdev);

	return ak8789_report_data(data, pdata);
}

static void ak8789_report_delayed_work(struct work_struct *work)
{
	packet_data temp_data;
	packet_data pdata = 0;
	struct hall_device *h_dev = NULL;
	struct hall_device *temp_dev = NULL;
	struct ak8789_data *data = NULL;

	h_dev = container_of(work, struct hall_device, h_delayed_work.work);
	if (h_dev == NULL) {
		hwlog_err("%s: ak8789 h_dev null\n", __func__);
		return;
	}

	data = platform_get_drvdata(h_dev->pdev);
	if (data == NULL) {
		hwlog_err("%s: ak8789 data null\n", __func__);
		return;
	}

	list_for_each_entry(temp_dev, &data->head, list) {
		temp_data = ak8789_get_data(temp_dev);
		pdata |= temp_data << (temp_dev->hall_id << 1);
	}
	hwlog_info("report value:%u, workaround_is_support:%u\n", pdata, workaround_is_support);
	if (workaround_is_support)
		process_pen_irq(pdata);

	if (ak8789_report_data(data, pdata) == 0)
		hwlog_err("report data err\n");
}

static int ak8789_get_common_configs(struct hall_device *hall_dev,
				     struct device_node *node)
{
	int ret;
	unsigned int auxiliary_flag = 0;

	ret = of_property_read_u32(node, HALL_TYPE, &hall_dev->hall_type);
	if (ret != 0) {
		hwlog_err("%s: Failed to get type\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(node, HALL_WAKEUP_FLAG,
				   &hall_dev->hall_wakeup_flag);
	if (ret != 0) {
		hwlog_err("%s: Failed to get wake_up\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(node, HALL_ID, &hall_dev->hall_id);
	if (ret != 0) {
		hwlog_err("%s: Failed to get id\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(node, HALL_HW_WAKEUP_SUPPORT,
				   &hall_dev->hall_hw_wakeup_support);
	if (ret != 0) {
		hwlog_err("%s: Failed to get hw wakeup support\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(node, HALL_X_COORDINATE,
				   &hall_dev->hall_x_coordinate);
	if (ret != 0) {
		hwlog_err("%s: Failed to get x coordinate", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(node, HALL_Y_COORDINATE,
				   &hall_dev->hall_y_coordinate);
	if (ret != 0) {
		hwlog_err("%s: Failed to get y coordinate\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(node, HALL_AUXILIARY_ID, &auxiliary_flag);
	if (ret != 0) {
		hwlog_err("%s: Failed to get auxiliary\n", __func__);
	} else {
		if (support_type_north(auxiliary_flag)) {
			ret = of_property_read_u32(node, HALL_AUXILIARY_N,
				&hall_dev->h_info.auxiliary_io[NORTH]);
			if (ret != 0)
				hwlog_err("%s: get auxiliary io fail\n",
					__func__);
		} else {
			hall_dev->h_info.auxiliary_io[NORTH] = 0;
		}

		if (support_type_south(auxiliary_flag)) {
			ret = of_property_read_u32(node, HALL_AUXILIARY_S,
				&hall_dev->h_info.auxiliary_io[SOUTH]);
			if (ret != 0)
				hwlog_err("%s: get auxiliary io fail\n",
					__func__);
		} else {
			hall_dev->h_info.auxiliary_io[SOUTH] = 0;
		}
	}

	ret = of_property_read_u32(node, HALL_GPIO_TYPE,
			&hall_dev->hall_gpio_type);
	if (ret != 0)
		hwlog_warn("%s: Failed to get gpio_type\n", __func__);

	ret = of_property_read_u32(node, HALL_INT_DELAY,
				   &hall_dev->hall_int_delay);
	if (ret != 0) {
		hwlog_warn("%s: Failed to get int delay time, use default time\n",
			__func__);
		hall_dev->hall_int_delay = HALL_DEFAULT_INT_DELAY;
	}

	ret = of_property_read_u32(node, HALL_HI6502_GPIO,
		&hall_dev->hi6502_gpio_flag);
	if (ret != 0) {
		hwlog_err("%s: Failed to get hi6502_gpio_flag, use normal gpio\n",
			__func__);
		hall_dev->hi6502_gpio_flag = HI6502_GPIO_DEFAULT;
	}
	hwlog_info("%s: get hi6502_gpio_flag 0x%x\n", __func__,
		hall_dev->hi6502_gpio_flag);

	hwlog_info("%s: get conf successed\n", __func__);
	return 0;
}

#ifdef HALL_TEST
static int ak8789_hall_irq_time_test(int irq,
				     struct hall_device *h_dev,
				     struct ak8789_data *data)
{
	long irq_time;
	int index;
	struct timespec64 now;

	ktime_get_real_ts64(&now);

	if (irq == h_dev->h_info.irq[NORTH])
		index = NORTH;
	else if (irq == h_dev->h_info.irq[SOUTH])
		index = SOUTH;
	else
		return -1; // not support hall type

	irq_time = (now.tv_sec - h_dev->h_info.last_time[index].tv_sec) *
		   (1000000L) +
		   (now.tv_nsec - h_dev->h_info.last_time[index].tv_nsec)/(1000L);

	if (irq_time > HALL_IRQ_ABNORMAL_TIME) {
		atomic_set(&h_dev->h_info.irq_err_count[index], 0);
	} else if (irq_time <= HALL_IRQ_ABNORMAL_TIME && irq_time > 0) {
		atomic_inc(&h_dev->h_info.irq_err_count[index]);
		queue_work(data->hall_wq, &data->inter_work);
	} else {
		hwlog_debug("%s: irq_time = %ld\n", __func__, irq_time);
	}
	h_dev->h_info.last_time[index].tv_sec = now.tv_sec;
	h_dev->h_info.last_time[index].tv_nsec = now.tv_nsec;
	return 0;
}
#endif

static irqreturn_t ak8789_event_handler(int irq, void *hall_dev)
{
	unsigned int trig_val;
	int ret;
	unsigned int hall_gpio_type;
	struct ak8789_data *data = NULL;
	struct irq_desc *h_desc = NULL;
	struct hall_device *h_dev = hall_dev;

	if (h_dev == NULL)
		return IRQ_NONE;

	h_desc = irq_to_desc(irq);
	if (h_desc == NULL)
		return IRQ_NONE;
	atomic_set(&cur_irq_num, irq);
#if (KERNEL_VERSION(4, 4, 0) > LINUX_VERSION_CODE)
	trig_val = (h_desc->irq_data.state_use_accessors) &
		   IRQD_TRIGGER_MASK;
#else
	trig_val = (h_desc->irq_data.common->state_use_accessors) &
		   IRQD_TRIGGER_MASK;
#endif
	hall_gpio_type = h_dev->hall_gpio_type;
	/* judge bit 0 is 1 */
	if (hall_gpio_type & 0x01) {
		if (trig_val & IRQF_TRIGGER_FALLING) {
			ret = irq_set_irq_type(irq, IRQF_TRIGGER_RISING);
			if (ret < 0)
				return IRQ_NONE;
		} else if (trig_val & IRQF_TRIGGER_RISING) {
			ret = irq_set_irq_type(irq, IRQF_TRIGGER_FALLING);
			if (ret < 0)
				return IRQ_NONE;
		} else {
			return IRQ_NONE;
		}
	} else {
		if (trig_val & IRQF_TRIGGER_LOW) {
			ret = irq_set_irq_type(irq, IRQF_TRIGGER_HIGH);
			if (ret < 0)
				return IRQ_NONE;
		} else if (trig_val & IRQF_TRIGGER_HIGH) {
			ret = irq_set_irq_type(irq, IRQF_TRIGGER_LOW);
			if (ret < 0)
				return IRQ_NONE;
		} else {
			return IRQ_NONE;
		}
	}

	data = platform_get_drvdata(h_dev->pdev);
	if (data == NULL)
		return IRQ_NONE;

	__pm_wakeup_event(data->wakelock, jiffies_to_msecs(HZ));
#ifdef HALL_TEST
	ret = ak8789_hall_irq_time_test(irq, h_dev, data);
	if (ret != 0) {
		hwlog_err("%s: ak8789_hall_irq_time_test fail\n", __func__);
		return IRQ_NONE;
	}
#endif
	queue_delayed_work(data->hall_wq,
			   &h_dev->h_delayed_work,
			   msecs_to_jiffies(h_dev->hall_int_delay));

	return IRQ_HANDLED;
}

static void hall_upload_error_interrupt(struct ak8789_data *data,
					struct hall_device *hall,
					enum hall_type type)
{
	int count;
	int check_result;
	const int max_irq_time = 3; // max interrupt error time

	count = atomic_read(&hall->h_info.irq_err_count[type]);

	if (count >= max_irq_time) {
		check_result = sensor_pmic_power_check();
#ifdef HALL_DSM_CONFIG
		/* dsm report err */
		if (!dsm_client_ocuppy(data->hall_dclient)) {
			dsm_client_record(data->hall_dclient,
				"hall id[%u], %s irq abnormity.result:%d\n",
				hall->hall_id, pole_name[type], check_result);
			dsm_client_notify(data->hall_dclient,
				DSM_HALL_ERROR_NO);
		}
#endif
		hwlog_err("%s:id is %u, %s.state:%d\n",
			  __func__, hall->hall_id,
			  pole_name[type], check_result);
		atomic_set(&hall->h_info.irq_err_count[type], 0);
	}
}

static int hall_drm_notifier_call(struct notifier_block *nb, unsigned long event, void *data)
{
	struct fb_event *fb_event = data;
	int *blank = NULL;

	if (!fb_event || !fb_event->data) {
		hwlog_err("%s: data is null.\n", __func__);
		return NOTIFY_DONE;
	}
	blank = fb_event->data;
	if (event != FB_EVENT_BLANK)
		return NOTIFY_OK;

	if (*blank == FB_BLANK_UNBLANK) {
		is_screen_on = true;
		spin_lock_irqsave(&irq_lock, irq_flag);
		if (is_pen_irq_disable == true) {
			enable_irq(hall_irq_num[4]);
			if (hall_irq_num[5])
				enable_irq(hall_irq_num[5]);
			is_pen_irq_disable = false;
		}
		spin_unlock_irqrestore(&irq_lock, irq_flag);
		hwlog_info("screen on! is_pen_irq_disable:%d\n", is_pen_irq_disable);
	} else if (*blank == FB_BLANK_POWERDOWN) {
		is_screen_on = false;
		pen_irq_cnt = 0;
		pen_both_on_cnt = 0;
		hwlog_info("screen off!\n");
	}

	return NOTIFY_OK;
}

static struct notifier_block hall_drm_notify = {
	.notifier_call = hall_drm_notifier_call,
};

static void hall_interrupt_abnormity_work(struct work_struct *work)
{
	unsigned int hall_type;
	struct ak8789_data *data = NULL;
	struct hall_device *temp_dev = NULL;

	data = container_of(work, struct ak8789_data, inter_work);
	if (data == NULL) {
		hwlog_info("%s data is null\n", __func__);
		return;
	}

	list_for_each_entry(temp_dev, &data->head, list) {
		hall_type = temp_dev->hall_type;
		hwlog_info("%s done [hall id:%u][hall type:%u]\n", __func__,
			   temp_dev->hall_id, hall_type);
		if (support_type_north(hall_type))
			hall_upload_error_interrupt(data, temp_dev, NORTH);

		if (support_type_south(hall_type))
			hall_upload_error_interrupt(data, temp_dev, SOUTH);
	}

	hwlog_info("%s done\n", __func__);
}

static int ak8789l_request_gpio(struct hall_device *hall_dev,
				struct device_node *node)
{
	int ret;
	int gpio;
	unsigned int hall_type = hall_dev->hall_type;

	if (support_type_north(hall_type)) {
		gpio = of_get_named_gpio(node, HALL_SINGLE_N_POLE, 0);
		hwlog_err("north gpio %d\n", gpio);
		if (!gpio_is_valid(gpio)) {
			hwlog_err("north pole gpio is invalid\n");
			return -EINVAL;
		}
		ret = gpio_request((unsigned int)gpio, NORTH_POLE_NAME);
		if (ret < 0) {
			hwlog_err("request north pole gpio err\n");
			return ret;
		}
		hall_dev->h_info.gpio[NORTH] = (unsigned int)gpio;
		ret = snprintf(hall_dev->h_info.name[NORTH],
			       HALL_MAX_STRING_LEN,
			       "hall%d_north",
			       hall_dev->hall_id);
		if (ret < 0) {
			hwlog_err("set north irq err. hall id:%u\n",
				hall_dev->hall_id);
			return ret;
		}
		hall_dev->h_info.hi6502_gpio[NORTH] =
			hall_dev->hi6502_gpio_flag & 0x01; // bit 0 means north
	}

	if (support_type_south(hall_type)) {
		gpio = of_get_named_gpio(node, HALL_SINGLE_S_POLE, 0);
		hwlog_err("south gpio %d\n", gpio);
		if (!gpio_is_valid(gpio)) {
			hwlog_err("south pole gpio is invalid\n");
			return -EINVAL;
		}
		ret = gpio_request((unsigned int)gpio, SOUTH_POLE_NAME);
		if (ret < 0) {
			hwlog_err("request south pole gpio err\n");
			return ret;
		}
		hall_dev->h_info.gpio[SOUTH] = (unsigned int)gpio;
		ret = snprintf(hall_dev->h_info.name[SOUTH],
			       HALL_MAX_STRING_LEN,
			       "hall%d_south",
			       hall_dev->hall_id);
		if (ret < 0) {
			hwlog_err("set south irq err. hall id:%u\n",
				hall_dev->hall_id);
			return ret;
		}
		hall_dev->h_info.hi6502_gpio[SOUTH] =
			hall_dev->hi6502_gpio_flag & 0x02; // bit 1 means south
	}

	hwlog_info("gpio request done\n");
	return 0;
}

static void ak8789_set_hall_irq_num(int hall_id, enum hall_type type, int irq)
{
	if (workaround_is_support) {
		int hall_value = 0;
		if ((type & NORTH) == NORTH) {
			hall_value = 1 << (hall_id * 2);
		} else if ((type & SOUTH) == SOUTH) {
			hall_value = 1 << (hall_id * 2 + 1);
		}
		switch (hall_value) {
		case 0x01:
			hall_irq_num[0] = irq;
			break;
		case 0x04:
			hall_irq_num[2] = irq;
			break;
		case 0x10:
			hall_irq_num[4] = irq;
			break;
		case 0x20:
			hall_irq_num[5] = irq;
			break;
		default:
			break;
		}
		hwlog_info("%s: irq = %d, hall_value = %d, id = %d, type = %d",
			__func__, irq, hall_value, hall_id, type);
	}
}

static int requst_irq_by_hall_type(struct hall_device *hall_dev,
				   enum hall_type type)
{
	unsigned int gpio = hall_dev->h_info.gpio[type];
	unsigned int gpio_type = hall_dev->hall_gpio_type;
	unsigned int flag = hall_dev->hall_wakeup_flag;
	unsigned int hi6502_gpio = hall_dev->h_info.hi6502_gpio[type];
	int irq = 0;
	unsigned int trigger_val;
	int ret;

	if (hi6502_gpio) {
#ifdef CONFIG_GPIO_HI6502
		irq = hi6502_gpio_to_irq(gpio);
#endif
		hwlog_info("hi6502_gpio_to_irq succse %d\n", irq);
	} else {
		irq = gpio_to_irq(gpio);
	}
	hall_dev->h_info.irq[type] = irq;

	/* judge bit 0 is 1 */
	if (gpio_type & 0x01) {
		trigger_val = gpio_get_value_cansleep(gpio) ?
			      IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING;
		ret = request_threaded_irq(irq, NULL, ak8789_event_handler,
					   trigger_val | flag | IRQF_ONESHOT,
					   hall_dev->h_info.name[type],
					   hall_dev);
	} else {
		trigger_val = gpio_get_value(gpio) ?
			      IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH;
		ret = request_irq(irq, ak8789_event_handler,
				  trigger_val | flag,
				  hall_dev->h_info.name[type],
				  hall_dev);
	}
	hwlog_info("%s: trigger_val = %u, irq = %d, ret = %d", __func__, trigger_val, irq, ret);
	ak8789_set_hall_irq_num(hall_dev->hall_id, type, irq);
	return ret;
}

static int ak8789_request_irq(struct hall_device *hall_dev)
{
	int ret = 0;
	unsigned int hall_type = hall_dev->hall_type;

	if (support_type_north(hall_type)) {
		ret = requst_irq_by_hall_type(hall_dev, NORTH);
		if (ret < 0) {
			hwlog_err("%s: requst_irq fail\n", __func__);
			return ret;
		}
	}

	if (support_type_south(hall_type)) {
		ret = requst_irq_by_hall_type(hall_dev, SOUTH);
		if (ret < 0) {
			hwlog_err("%s: requst_irq fail\n", __func__);
			return ret;
		}
	}
	return ret;
}

static int init_single_hall(struct platform_device *pdev,
			    struct hall_device **hall,
			    struct device_node *node)
{
	struct hall_device *p = NULL;
	int ret;

	p = devm_kzalloc(&pdev->dev, sizeof(*p), GFP_KERNEL);
	if (p == NULL)
		return -ENOMEM;

	p->pdev = pdev;
	INIT_DELAYED_WORK(&(p->h_delayed_work), ak8789_report_delayed_work);
	ret = ak8789_get_common_configs(p, node);
	if (ret != 0) {
		hwlog_err("%s, ak8789_get_common_configs fail\n", __func__);
		return -EINVAL;
	}
	ret = snprintf(p->name, HALL_MAX_STRING_LEN, "hall%d\n", p->hall_id);
	if (ret < 0) {
		hwlog_err("Failed to set hall dev name\n");
		return -EINVAL;
	}
	ret = ak8789l_request_gpio(p, node);
	if (ret < 0) {
		hwlog_err("%s: hall request gpio err\n", __func__);
		return -EINVAL;
	}

	ret = ak8789_request_irq(p);
	if (ret < 0) {
		hwlog_err("%s: hall request irq failed\n", __func__);
		return -EINVAL;
	}

	(*hall) = p;
	return 0;
}

static void ak8789_delete_list(struct ak8789_data *data)
{
	struct hall_device *hall = NULL;

	while (!list_empty(&data->head)) {
		hall = list_first_entry(&data->head, struct hall_device, list);
		list_del(&hall->list);
		kfree(hall);
	}
}

static void ak8789_prases_hall_workaround_param(struct device_node *node)
{
	unsigned int tmp = 0;
	if (!of_property_read_u32(node, "support_hall_workaround", &tmp)) {
		workaround_is_support = tmp;
		hwlog_info("workaround_is_support = %u.\n", workaround_is_support);
	}
	if (workaround_is_support == 0)
		return;
	if (!of_property_read_u32(node, "limit_duration", &tmp)) {
		limit_duration = tmp;
		hwlog_info("limit_duration = %u.\n", limit_duration);
	}

	if (!of_property_read_u32(node, "limit_pen_irq_cnt", &tmp)) {
		limit_pen_irq_cnt = tmp;
		hwlog_info("limit_pen_irq_cnt = %u.\n", limit_pen_irq_cnt);
	}

	if (!of_property_read_u32(node, "limit_pen_both_on_cnt", &tmp)) {
		limit_pen_both_on_cnt = tmp;
		hwlog_info("limit_pen_both_on_cnt = %u.\n", limit_pen_both_on_cnt);
	}
}

static void ak8789_init_hall_check_para()
{
	if (workaround_is_support) {
		fb_register_client(&hall_drm_notify);
		limit_time = limit_duration * MINUTES_TO_NS;
		limit_irq_cnt = limit_pen_irq_cnt;
		limit_both_on_cnt = limit_pen_both_on_cnt;
		hwlog_info("limit_time:%lld, limit_irq_cnt:%u, limit_both_on_cnt:%u\n",
			limit_time, limit_irq_cnt, limit_both_on_cnt);
	}
}

static int ak8789_probe(struct platform_device *pdev)
{
	struct hall_device *hall = NULL;
	struct ak8789_data *data = NULL;
	struct device_node *node = NULL;
	struct device_node *temp = NULL;
	int ret;
	int num_halls = 0;
	int count = 0;
	bool default_state = false;
	const char *state = NULL;

	node = pdev->dev.of_node;
	if (node == NULL)
		return -ENODEV;

	while ((temp = of_get_next_child(node, temp)))
		num_halls++;

	if (num_halls == 0) // have no hall
		return -ECHILD;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;

	data->state = 0;
	data->pdev = pdev;

	INIT_LIST_HEAD(&data->head);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	data->wakelock = wakeup_source_register(&data->pdev->dev, "hall");
#else
	data->wakelock = wakeup_source_register("hall");
#endif
	spin_lock_init(&data->spinlock);
	INIT_WORK(&data->inter_work, hall_interrupt_abnormity_work);
	spin_lock_init(&irq_lock);
	data->hall_wq = create_singlethread_workqueue("hall_wq");
	if (IS_ERR_OR_NULL(data->hall_wq)) {
		hwlog_err("%s: work_wq kmalloc error\n", __func__);
		ret = -ENOMEM;
		goto free_wake_lock;
	}
#ifdef HALL_DATA_REPORT_INPUTDEV
	ret = ak8789_input_register(data);
	if (ret != 0) {
		hwlog_err("hall input register failed\n");
		goto free_wq;
	}
#endif

#ifdef HALL_TEST
/* ak8789 init_data */
	ak8789_test_info_init(&data->cdev);
#endif
	ak8789_prases_hall_workaround_param(node);
	ret = hall_register(&data->cdev);
	if (ret != 0)
		goto err_register_err;

	for_each_child_of_node(node, temp) {
		ret = of_property_read_string(temp, "huawei,default-state",
			&state);
		if (ret != 0) {
			hwlog_err("%s: read default-state fail\n", __func__);
			goto free_hall_dev;
		}

		if (strncmp(state, "on", sizeof("on")) == 0)
			default_state = true;
		else
			default_state = false;

		if (default_state) {
			count++;
			ret = init_single_hall(pdev, &hall, temp);
			if (ret != 0) {
				hwlog_err("%s: init_single_hall error\n",
					__func__);
				goto free_hall_dev;
			}
			list_add_tail(&hall->list, &data->head);
#ifdef HALL_TEST
			atomic_set(&hall->h_info.irq_err_count[NORTH], 0);
			atomic_set(&hall->h_info.irq_err_count[SOUTH], 0);
			hall->h_info.last_time[NORTH].tv_sec = 0;
			hall->h_info.last_time[NORTH].tv_nsec = 0;
			hall->h_info.last_time[SOUTH].tv_sec = 0;
			hall->h_info.last_time[SOUTH].tv_nsec = 0;
#endif
		}
	}

	data->count = count;
	data->cdev.hall_count = count;
	data->cdev.hall_get_state = ak8789_get_status;
	data->cdev.hall_report_debug_data = ak8789_report_debug_data;
	data->cdev.hall_report_state = ak8789_report_status;
	data->cdev.hall_get_info = ak8789_get_info;
#ifdef HALL_TEST
	data->cdev.hall_set_report_value = ak8789_set_report_value;

#ifdef HALL_DSM_CONFIG
	data->hall_dclient = dsm_register_client(&dsm_hall);
	if (!data->hall_dclient) {
		hwlog_err("hall dsm register err\n");
		goto free_hall_dev;
	}

	hwlog_info("ak8789 dsm register success\n");
#endif
#endif
	hall_interface_ops_register(&ak8789_ops);

	platform_set_drvdata(pdev, data);
#ifdef HALL_DATA_REPORT_INPUTHUB
	data_ak8789 = data;
#endif
	ak8789_init_hall_check_para();
	hwlog_info("ak8789 probe success\n");
	return 0;

free_hall_dev:
	ak8789_delete_list(data);
	hall_unregister(&data->cdev);

err_register_err:
#ifdef HALL_DATA_REPORT_INPUTDEV
	ak8789_input_unregister(data);
free_wq:
#endif
	destroy_workqueue(data->hall_wq);
free_wake_lock:
	wakeup_source_unregister(data->wakelock);
	data->wakelock = NULL;

	return ret;
}

static int ak8789_remove(struct platform_device *pdev)
{
	struct ak8789_data *data = platform_get_drvdata(pdev);

	if (data == NULL) {
		hwlog_err("ak8789 data null\n");
		return -ENOMEM;
	}

	ak8789_delete_list(data);
	hall_unregister(&data->cdev);

#ifdef HALL_DATA_REPORT_INPUTDEV
	ak8789_input_unregister(data);
#endif

	destroy_workqueue(data->hall_wq);
	wakeup_source_unregister(data->wakelock);
	data->wakelock = NULL;

	return 0;
}

struct platform_driver ak8789_driver = {
	.probe = ak8789_probe,
	.remove = ak8789_remove,
	.driver = {
		.name = HALL_SENSOR_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ak8789_match_table),
	},
};

static int __init ak8789_init(void)
{
	hwlog_info("init\n");
	return platform_driver_register(&ak8789_driver);
}

static void __exit ak8789_exit(void)
{
	hwlog_info("exit\n");
	platform_driver_unregister(&ak8789_driver);
}

module_init(ak8789_init);
module_exit(ak8789_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Ak8789 driver");
MODULE_AUTHOR("Huawei Technologies Co., Ltd.");
