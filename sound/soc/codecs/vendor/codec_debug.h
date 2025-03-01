/*
 * Copyright (c) 2017-2021 Huawei Technologies Co., Ltd.
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

#ifndef __HICODEC_DEBUG_H__
#define __HICODEC_DEBUG_H__

#include <sound/soc.h>

#include "platform_base_addr_info.h"

#define ASP_CODECLESS_START_OFFSET 0x00
#define ASP_CODECLESS_END_OFFSET   0x28C
#ifndef CONFIG_AUDIO_COMMON_IMAGE
#define PAGE_CODECLESS_BASE_ADDR (SOC_ACPU_ASP_CODEC_BASE_ADDR)
#else
#define PAGE_CODECLESS_BASE_ADDR 0
#endif
#define DBG_CODECLESS_START_ADDR \
	(PAGE_CODECLESS_BASE_ADDR + ASP_CODECLESS_START_OFFSET)
#define DBG_CODECLESS_END_ADDR (PAGE_CODECLESS_BASE_ADDR + ASP_CODECLESS_END_OFFSET)

enum asp_codec_type {
	ASP_CODECLESS = 0,
	ASP_CODEC,
	ASP_CODEC_CNT,
};

enum {
	HICODEC_DEBUG_FLAG_READ = 0,
	HICODEC_DEBUG_FLAG_WRITE = 1,
};

/*
 * seg_name: name to print for this entry
 *           NULL for condition that this entry has same seg_name with the previous one
 * start:    address of first reg to dump
 * end:      address of last reg to dump
 * reg_size: register value size in bytes, for example:
 *             soc register --- 4Bytes (32bit)
 *             pmu register --- 1Byte (8bit)
 */
struct hicodec_dump_reg_entry
{
	const char *seg_name;
	unsigned int start;
	unsigned int end;
	unsigned int reg_size;
};

struct hicodec_dump_reg_info
{
	struct hicodec_dump_reg_entry *entry;
	unsigned int count;
};

/*
 * hicodec_debug_init:
 *     init when codec driver is probing
 * @codec:
 * @info: codec-specific infos
 */
int hicodec_debug_init(struct snd_soc_component *codec, const struct hicodec_dump_reg_info *info);

/*
 * hicodec_debug_uninit:
 *     uninit when codec driver is removing
 * @codec:
 */
void hicodec_debug_uninit(struct snd_soc_component *codec);

/*
 * hicodec_debug_reg_rw_cache:
 *     record operations of codec registers
 * @reg: register address
 * @val: register value
 * @rw:  HICODEC_DEBUG_FLAG_READ or HICODEC_DEBUG_FLAG_WRITE
 */
void hicodec_debug_reg_rw_cache(unsigned int reg, unsigned int val, int rw);

#ifdef CONFIG_AUDIO_COMMON_IMAGE
void bind_dump_reg_info(struct hicodec_dump_reg_info *info);
#endif

#endif
