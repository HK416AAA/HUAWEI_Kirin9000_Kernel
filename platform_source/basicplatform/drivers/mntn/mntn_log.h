/*
 * mntn_log.h
 *
 * head of mntn log, log print format of mntn
 *
 * Copyright (c) 2012-2020 Huawei Technologies Co., Ltd.
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
#ifndef _MNTN_H_
#define _MNTN_H_

/* Debug info */
#ifndef MDEBUG_LEVEL
#define MDEBUG_LEVEL    0
#endif
#ifndef MINFO_LEVEL
#define MINFO_LEVEL     0
#endif
#ifndef MNOTICE_LEVEL
#define MNOTICE_LEVEL   0
#endif
#ifndef MWARNING_LEVEL
#define MWARNING_LEVEL  0
#endif
#ifndef MERROR_LEVEL
#define MERROR_LEVEL    1
#endif

#ifndef MLOG_TAG
#define MLOG_TAG        "mntn"
#endif

#undef pr_fmt
#define pr_fmt(fmt)     MLOG_TAG ":" fmt

#if MDEBUG_LEVEL
#define mlog_d(fmt, ...) \
	pr_info("[D]:%s(%d): " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#else
#define mlog_d(fmt, ...)
#endif

#if MINFO_LEVEL
#define mlog_i(fmt, ...) \
	pr_info("[I]:%s(%d): " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#else
#define mlog_i(fmt, ...)
#endif

#if MNOTICE_LEVEL
#define mlog_n(fmt, ...) \
	pr_notice("[N]:%s(%d): " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#else
#define mlog_n(fmt, ...)
#endif

#if MWARNING_LEVEL
#define mlog_w(fmt, ...) \
	pr_warn("[W]:%s(%d): " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#else
#define mlog_w(fmt, ...)
#endif

#if MERROR_LEVEL
#define mlog_e(fmt, ...) \
	pr_err("[E]:%s(%d): " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#else
#define mlog_e(fmt, ...)
#endif

#endif /* _MNTN_H_ */
