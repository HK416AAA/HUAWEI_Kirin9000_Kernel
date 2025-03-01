/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */
#include "dp_avgen_base.h"
#include "dpu_dp_dbg.h"
#include "dp_edid.h"
#include "drm_dp_helper_additions.h"
#include "dp_aux.h"
#include "hidptx/hidptx_reg.h"

uint8_t dptx_audio_get_sample_freq_cfg(struct audio_params *aparams)
{
	uint8_t iec_orig_samp_freq = 0;
	uint8_t iec_samp_freq = 0;
	uint8_t sample_freq_cfg = 0;

	dpu_check_and_return(!aparams, 0, err, "[DP] aparams is NULL");

	iec_orig_samp_freq = aparams->iec_orig_samp_freq;
	iec_samp_freq = aparams->iec_samp_freq;

	if (iec_orig_samp_freq == IEC_ORIG_SAMP_FREQ_32K && iec_samp_freq == IEC_SAMP_FREQ_32K)
		sample_freq_cfg = DPTX_AUDIO_SAMPLE_FREQ_32K;
	else if (iec_orig_samp_freq == IEC_ORIG_SAMP_FREQ_48K && iec_samp_freq == IEC_SAMP_FREQ_48K)
		sample_freq_cfg = DPTX_AUDIO_SAMPLE_FREQ_48K;
	else if (iec_orig_samp_freq == IEC_ORIG_SAMP_FREQ_96K && iec_samp_freq == IEC_SAMP_FREQ_96K)
		sample_freq_cfg = DPTX_AUDIO_SAMPLE_FREQ_96K;
	else if (iec_orig_samp_freq == IEC_ORIG_SAMP_FREQ_192K && iec_samp_freq == IEC_SAMP_FREQ_192K)
		sample_freq_cfg = DPTX_AUDIO_SAMPLE_FREQ_192K;
	else
		sample_freq_cfg = DPTX_AUDIO_REFER_TO_STREAM_HEADER;

	return sample_freq_cfg;
}

uint8_t dptx_audio_get_data_width_cfg(struct audio_params *aparams)
{
	uint8_t data_width_cfg = 0;

	dpu_check_and_return(!aparams, 0, err, "[DP] aparams is NULL");

	if (aparams->data_width == 16)
		data_width_cfg = DPTX_AUDIO_SAMPLE_SIZE_16BIT;
	else if (aparams->data_width == 24)
		data_width_cfg = DPTX_AUDIO_SAMPLE_SIZE_24BIT;
	else
		data_width_cfg = DPTX_AUDIO_REFER_TO_STREAM_HEADER;

	return data_width_cfg;
}

uint8_t dptx_audio_get_num_channels_cfg(struct audio_params *aparams)
{
	uint8_t num_channels_cfg = 0;

	dpu_check_and_return(!aparams, 0, err, "[DP] aparams is NULL");

	if (aparams->num_channels == 2)
		num_channels_cfg = DPTX_AUDIO_CHANNEL_CNT_2CH;
	else if (aparams->num_channels == 8)
		num_channels_cfg = DPTX_AUDIO_CHANNEL_CNT_8CH;
	else
		num_channels_cfg = DPTX_AUDIO_REFER_TO_STREAM_HEADER;

	return num_channels_cfg;
}

uint8_t dptx_audio_get_speaker_map_cfg(struct audio_params *aparams)
{
	uint8_t speaker_map_cfg = 0;

	dpu_check_and_return(!aparams, 0, err, "[DP] aparams is NULL");

	if (aparams->num_channels == 2)
		speaker_map_cfg = DPTX_AUDIO_SPEAKER_MAPPING_2CH;
	else
		speaker_map_cfg = DPTX_AUDIO_SPEAKER_MAPPING_8CH;

	return speaker_map_cfg;
}

void dptx_config_hdr_payload(struct sdp_full_data *hdr_sdp_data, struct hdr_infoframe *hdr_infoframe,
	uint8_t enable)
{
	int i, j;
	uint32_t hdr_infoframe_data = 0;

	dpu_check_and_no_retval(!hdr_sdp_data, err, "[DP] hdr_sdp_data is NULL");
	dpu_check_and_no_retval(!hdr_infoframe, err, "[DP] hdr_infoframe is NULL");

	memset(hdr_sdp_data, 0, sizeof(*hdr_sdp_data));
	hdr_sdp_data->en = enable;
	hdr_sdp_data->payload[0] = HDR_INFOFRAME_HEADER;
	hdr_sdp_data->payload[1] = (hdr_infoframe->data[1] << 24) | (hdr_infoframe->data[0] << 16) |
		(HDR_INFOFRAME_LENGTH << 8) | HDR_INFOFRAME_VERSION;

	for (i = 2; i < HDR_INFOFRAME_LENGTH; i++) {
		for (j = 0; j < DATA_NUM_PER_REG; j++) {
			hdr_infoframe_data |= (uint32_t)hdr_infoframe->data[i] <<
				((j % DATA_NUM_PER_REG) * INFOFRAME_DATA_SIZE);

			if (j < (DATA_NUM_PER_REG - 1))
				i++;

			if (i >= HDR_INFOFRAME_LENGTH)
				break;
		}

		hdr_sdp_data->payload[i / DATA_NUM_PER_REG + 1] = hdr_infoframe_data;
		hdr_infoframe_data = 0;
	}
}

int dptx_update_dss_and_hwc(struct dp_ctrl *dptx)
{
	int retval = 0;

	dpu_check_and_return(!dptx, -EINVAL, err, "[DP] null pointer");

	if (dptx->dptx_vr) {
		if (dptx_check_low_temperature(dptx)) {
			dpu_pr_err("[DP] VR device can't work on low temperature!");
			return -EINVAL;
		}
		if (dptx->dptx_resolution_switch)
			retval = dptx->dptx_resolution_switch(dptx, HOT_PLUG_IN_VR);
		if (retval)
			dpu_pr_err("[DP] HOT_PLUG_IN_VR DSS init fail !!!");
	} else {
		retval = dptx_change_video_mode_user(dptx);
		if (retval)
			dpu_pr_err("[DP] Change mode by user setting error!");

		if (dptx->dptx_resolution_switch)
			retval = dptx->dptx_resolution_switch(dptx, HOT_PLUG_IN);
		if (retval)
			dpu_pr_err("[DP] HOT_PLUG_IN DSS init fail !!!");
	}
	return 0;
}

/*
 * MISC0 bit7:5 for Pixel Encoding/Colorimetry Format Indicationa, According to Table 2-96 of DisplayPort spec 1.4
 */
static const struct video_bpc_mapping g_sink_bpc_maping[] = {
	{RGB, COLOR_DEPTH_6, 0},
	{RGB, COLOR_DEPTH_8, 1},
	{RGB, COLOR_DEPTH_10, 2},
	{RGB, COLOR_DEPTH_12, 3},
	{RGB, COLOR_DEPTH_16, 4},
	{YCBCR444, COLOR_DEPTH_8, 1},
	{YCBCR444, COLOR_DEPTH_10, 2},
	{YCBCR444, COLOR_DEPTH_12, 3},
	{YCBCR444, COLOR_DEPTH_16, 4},
	{YCBCR422, COLOR_DEPTH_8, 1},
	{YCBCR422, COLOR_DEPTH_10, 2},
	{YCBCR422, COLOR_DEPTH_12, 3},
	{YCBCR422, COLOR_DEPTH_16, 4},
	{YONLY, COLOR_DEPTH_8, 1},
	{YONLY, COLOR_DEPTH_10, 2},
	{YONLY, COLOR_DEPTH_12, 3},
	{YONLY, COLOR_DEPTH_16, 4},
	{RAW, COLOR_DEPTH_6, 1},
	{RAW, COLOR_DEPTH_8, 3},
	{RAW, COLOR_DEPTH_10, 4},
	{RAW, COLOR_DEPTH_12, 5},
	{RAW, COLOR_DEPTH_16, 7},
};

uint8_t dptx_get_sink_bpc_mapping(enum pixel_enc_type pix_enc, uint8_t bpc)
{
	uint8_t bpc_mapping = 0;
	uint32_t i;
	const struct video_bpc_mapping *vmap = NULL;

	for (i = 0; i < ARRAY_SIZE(g_sink_bpc_maping); i++) {
		vmap = &(g_sink_bpc_maping[i]);
		if (pix_enc == vmap->pix_enc && bpc == vmap->bpc) {
			bpc_mapping = vmap->bpc_mapping;
			break;
		}
	}

	return bpc_mapping;
}

/* default (bpc) -> (colordep) mapping */
static const struct default_colordep_map g_default_colordep_maps[] = {
	{COLOR_DEPTH_6, 18},
	{COLOR_DEPTH_8, 24},
	{COLOR_DEPTH_10, 30},
	{COLOR_DEPTH_12, 36},
	{COLOR_DEPTH_16, 48},
};

/* (bpc, encoding) -> (colordep) mapping */
static const struct colordep_map g_colordep_maps[] = {
	{COLOR_DEPTH_8, YCBCR420, 12},
	{COLOR_DEPTH_8, YCBCR422, 16},
	{COLOR_DEPTH_8, YONLY, 8},
	{COLOR_DEPTH_10, YCBCR420, 15},
	{COLOR_DEPTH_10, YCBCR422, 20},
	{COLOR_DEPTH_10, YONLY, 10},
	{COLOR_DEPTH_12, YCBCR420, 18},
	{COLOR_DEPTH_12, YCBCR422, 24},
	{COLOR_DEPTH_12, YONLY, 12},
	{COLOR_DEPTH_16, YCBCR420, 24},
	{COLOR_DEPTH_16, YCBCR422, 32},
	{COLOR_DEPTH_16, YONLY, 16},
};

int dptx_get_color_depth(int bpc, int encoding)
{
	int colordep;
	uint32_t i;

	/* set default value of color_depth */
	colordep = 18;

	/* set default value of color_depth according to different bpc */
	for (i = 0; i < ARRAY_SIZE(g_default_colordep_maps); i++) {
		if (bpc == g_default_colordep_maps[i].bpc) {
			colordep = g_default_colordep_maps[i].default_colordep;
			break;
		}
	}

	/* set value of color_depth according to bpc and encoding */
	for (i = 0; i < ARRAY_SIZE(g_colordep_maps); i++) {
		if (bpc == g_colordep_maps[i].bpc && encoding == g_colordep_maps[i].encoding) {
			colordep = g_colordep_maps[i].colordep;
			break;
		}
	}

	return colordep;
}

int dptx_br_to_link_clk(int rate)
{
	int link_clk;

	switch (rate) {
	case DPTX_PHYIF_CTRL_RATE_RBR:
		link_clk = 40500;
		break;
	case DPTX_PHYIF_CTRL_RATE_HBR:
		link_clk = 67500;
		break;
	case DPTX_PHYIF_CTRL_RATE_HBR2:
		link_clk = 135000;
		break;
	case DPTX_PHYIF_CTRL_RATE_HBR3:
		link_clk = 202500;
		break;
	default:
		return -EINVAL;
	}

	return link_clk;
}

int dptx_br_to_link_rate(int rate)
{
	int link_rate;

	switch (rate) {
	case DPTX_PHYIF_CTRL_RATE_RBR:
		link_rate = 162;
		break;
	case DPTX_PHYIF_CTRL_RATE_HBR:
		link_rate = 270;
		break;
	case DPTX_PHYIF_CTRL_RATE_HBR2:
		link_rate = 540;
		break;
	case DPTX_PHYIF_CTRL_RATE_HBR3:
		link_rate = 810;
		break;
	default:
		return -EINVAL;
	}

	return link_rate;
}

int dptx_update_panel_info(struct dp_ctrl *dptx)
{
	struct dtd *mdtd = NULL;
	struct dkmd_object_info *upload_pinfo = NULL;
	struct dkmd_connector_info *pinfo = NULL;
	struct video_params *vparams = NULL;
	uint32_t pixels;

	dpu_check_and_return(!dptx, -EINVAL, err, "[DP] dptx is NULL");

	vparams = &(dptx->vparams);
	mdtd = &(dptx->vparams.mdtd);
	pinfo = dptx->connector->conn_info;
	upload_pinfo = pinfo->base.comp_obj_info;

	upload_pinfo->xres = pinfo->base.xres = mdtd->h_active;
	upload_pinfo->yres = pinfo->base.yres = mdtd->v_active;
	upload_pinfo->dsc_out_width = pinfo->base.dsc_out_width = pinfo->base.xres;
	upload_pinfo->dsc_out_height = pinfo->base.dsc_out_height = pinfo->base.yres;
	dptx->connector->ldi.h_back_porch =
		(mdtd->h_blanking - mdtd->h_sync_offset - mdtd->h_sync_pulse_width);
	dptx->connector->ldi.h_front_porch = mdtd->h_sync_offset;
	dptx->connector->ldi.h_pulse_width = mdtd->h_sync_pulse_width;
	dptx->connector->ldi.v_back_porch =
		(mdtd->v_blanking - mdtd->v_sync_offset - mdtd->v_sync_pulse_width);
	dptx->connector->ldi.v_front_porch = mdtd->v_sync_offset;
	dptx->connector->ldi.v_pulse_width = mdtd->v_sync_pulse_width;
	dptx->connector->ldi.pxl_clk_rate_div = 1;
	upload_pinfo->width = pinfo->base.width = 530;
	upload_pinfo->height = pinfo->base.height = 300;
	if (dptx->edid_info.video.max_himage_size != 0) {
		upload_pinfo->width = pinfo->base.width = dptx->edid_info.video.max_himage_size * 10;
		upload_pinfo->height = pinfo->base.height = dptx->edid_info.video.max_vimage_size * 10;
	} else {
		dpu_pr_err("[DP] The size of display device cannot be got from edid information");
	}

	dptx->connector->ldi.pxl_clk_rate = mdtd->pixel_clock * 1000;
	pinfo->ifbc_type = IFBC_TYPE_NONE;

	if (((mdtd->h_active + mdtd->h_blanking) * (mdtd->v_active + mdtd->v_blanking)) > 0) {
		pixels = ((mdtd->h_active + mdtd->h_blanking) * (mdtd->v_active + mdtd->v_blanking));
		if (pixels != 0)
			vparams->m_fps = (uint32_t)(dptx->connector->ldi.pxl_clk_rate / pixels);
		else
			vparams->m_fps = 60;
	} else {
		return -1;
	}
	upload_pinfo->fps = pinfo->base.fps = vparams->m_fps;

	return 0;
}

void dptx_debug_resolution_info(struct dp_ctrl *dptx)
{
	struct dtd *mdtd = NULL;
	struct dkmd_connector_info *pinfo = NULL;
	struct video_params *vparams = NULL;

	dpu_check_and_no_retval(!dptx, err, "[DP] dptx is NULL");

	vparams = &(dptx->vparams);
	mdtd = &(dptx->vparams.mdtd);
	pinfo = dptx->connector->conn_info;

	dpu_pr_info("[DP] xres=%d\n"
		"yres=%d\n"
		"h_back_porch=%d\n"
		"h_front_porch=%d\n"
		"h_pulse_width=%d\n"
		"v_back_porch=%d\n"
		"v_front_porch=%d\n"
		"v_pulse_width=%d\n"
		"hsync_plr=%d\n"
		"vsync_plr=%d\n"
		"pxl_clk_rate_div=%d\n"
		"pxl_clk_rate=%llu\n"
		"m_fps=%d\n",
		pinfo->base.xres,
		pinfo->base.yres,
		dptx->connector->ldi.h_back_porch,
		dptx->connector->ldi.h_front_porch,
		dptx->connector->ldi.h_pulse_width,
		dptx->connector->ldi.v_back_porch,
		dptx->connector->ldi.v_front_porch,
		dptx->connector->ldi.v_pulse_width,
		dptx->connector->ldi.hsync_plr,
		dptx->connector->ldi.vsync_plr,
		dptx->connector->ldi.pxl_clk_rate_div,
		dptx->connector->ldi.pxl_clk_rate,
		vparams->m_fps);

	dp_imonitor_set_param(DP_PARAM_WIDTH, &(mdtd->h_active));
	dp_imonitor_set_param(DP_PARAM_HIGH,  &(mdtd->v_active));
	dp_imonitor_set_param(DP_PARAM_FPS,   &(vparams->m_fps));
}

int dptx_video_mode_change(struct dp_ctrl *dptx, uint8_t vmode, int stream)
{
	int retval = 0;
	struct video_params *vparams = NULL;
	struct dtd mdtd;

	dpu_check_and_return(!dptx, -EINVAL, err, "[DP] null pointer");

	vparams = &dptx->vparams;
	vparams->mode = vmode;

	if (!convert_code_to_dtd(&mdtd, vparams->mode, vparams->refresh_rate, vparams->video_format)) {
		dpu_pr_err("[DP] Invalid video mode value %d", vparams->mode);
		return -EINVAL;
	}
	vparams->mdtd = mdtd;
	if (dptx->dptx_video_ts_calculate)
		retval = dptx->dptx_video_ts_calculate(dptx, dptx->link.lanes,
					 dptx->link.rate, vparams->bpc,
					 vparams->pix_enc, mdtd.pixel_clock);

	dpu_pr_info("[DP] The mode is changed as [%d]", vparams->mode);

	return retval;
}

int dptx_change_video_mode_user(struct dp_ctrl *dptx)
{
	struct video_params *vparams = NULL;
	int retval;
	bool needchanged = false;

	dpu_check_and_return(!dptx, -EINVAL, err, "[DP] null pointer");
	dptx->same_source = (g_dp_same_source_debug == 1);

	vparams = &dptx->vparams;
	if (!dptx->same_source) {
		if ((vparams->mdtd.h_active > FHD_TIMING_H_ACTIVE) || (vparams->mdtd.v_active > FHD_TIMING_V_ACTIVE)) {
			vparams->video_format = VCEA;
			vparams->mode = 16; /* switch to 1080p on PC mode */
			needchanged = true;
			dpu_pr_info("[DP] video mode is changed by different source!");
		}
	}

	if (dptx->user_mode != 0) {
		vparams->video_format = dptx->user_mode_format;
		vparams->mode = dptx->user_mode; /* switch to user setting */
		needchanged = true;
		dpu_pr_info("[DP] video mode is changed by user setting!");
	}

	if (needchanged) {
		retval = dptx_video_mode_change(dptx, vparams->mode, 0);
		if (retval) {
			dpu_pr_err("[DP] Change mode error!");
			return retval;
		}
	}

	if (dptx_check_low_temperature(dptx)) {
		if ((vparams->mdtd.h_active > FHD_TIMING_H_ACTIVE) || (vparams->mdtd.v_active > FHD_TIMING_V_ACTIVE)) {
			vparams->video_format = VCEA;
			vparams->mode = 16; /* switch to 1080p on pc mode */
			dpu_pr_info("[DP] video mode is changed by low temperature!");

			retval = dptx_video_mode_change(dptx, vparams->mode, 0);
			if (retval) {
				dpu_pr_err("[DP] Change mode error!");
				return retval;
			}
		}
	}
	dp_imonitor_set_param(DP_PARAM_SOURCE_MODE, &(dptx->same_source));
	dp_imonitor_set_param(DP_PARAM_USER_MODE,   &(vparams->mode));
	dp_imonitor_set_param(DP_PARAM_USER_FORMAT, &(vparams->video_format));

	return 0;
}

bool dptx_sink_enabled_ssc(struct dp_ctrl *dptx)
{
	uint8_t byte = 0;
	int retval = 0;

	retval = dptx_read_dpcd(dptx, DP_MAX_DOWNSPREAD, &byte);
	if (retval)
		dpu_pr_err("[DP] dptx_read_dpcd fail");

	return byte & 1;
}
