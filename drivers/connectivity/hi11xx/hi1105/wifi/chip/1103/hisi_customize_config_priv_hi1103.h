

#ifndef __HISI_CUSTOMIZE_CONFIG_PRIV_HI1103_H__
#define __HISI_CUSTOMIZE_CONFIG_PRIV_HI1103_H__

#include "hisi_customize_wifi_hi1103.h"

/* 私有定制化 PRIV CONFIG ID */
typedef enum {
    /* 校准开关 */
    WLAN_CFG_PRIV_CALI_MASK,
    WLAN_CFG_PRIV_CALI_DATA_MASK,
    WLAN_CFG_PRIV_CALI_AUTOCALI_MASK,

    WLAN_CFG_PRIV_BW_MAX_WITH,
    WLAN_CFG_PRIV_LDPC_CODING,
    WLAN_CFG_PRIV_RX_STBC,
    WLAN_CFG_PRIV_TX_STBC,
    WLAN_CFG_PRIV_SU_BFER,
    WLAN_CFG_PRIV_SU_BFEE,
    WLAN_CFG_PRIV_MU_BFER,
    WLAN_CFG_PRIV_MU_BFEE,
    WLAN_CFG_PRIV_11N_TXBF,
    WLAN_CFG_PRIV_1024_QAM,
    /* DBDC */
    WLAN_CFG_PRIV_DBDC_RADIO_0,
    WLAN_CFG_PRIV_DBDC_RADIO_1,
    WLAN_CFG_PRIV_FASTSCAN_SWITCH,

    /* 天线切换功能 */
    WLAN_CFG_ANT_SWITCH,
    /* 国家码自学习开关 */
    WLAN_CFG_PRIV_COUNRTYCODE_SELFSTUDY_CFG,

    /* M2S */
    WLAN_CFG_PRIV_M2S_FUNCTION_EXT_MASK,
    WLAN_CFG_PRIV_M2S_FUNCTION_MASK,

    /* MCM */
    WLAN_CFG_PRIV_MCM_CUSTOM_FUNCTION_MASK,
    WLAN_CFG_PRIV_MCM_FUNCTION_MASK,

    /* linkloss门限固定开关 */
    WLAN_CFG_PRRIV_LINKLOSS_THRESHOLD_FIXED,

    /* 160M APUT使能 */
    WLAN_CFG_APUT_160M_ENABLE,
    /* 屏蔽硬件上报的雷达信号 */
    WLAN_CFG_RADAR_ISR_FORBID,

    /* 限流 */
    WLAN_CFG_PRIV_DOWNLOAD_RATE_LIMIT_PPS,
#ifdef _PRE_WLAN_FEATURE_TXOPPS
    WLAN_CFG_PRIV_TXOPPS_SWITCH,
#endif
    WLAN_CFG_PRIV_OVER_TEMPER_PROTECT_THRESHOLD,
    WLAN_CFG_PRIV_OVER_TEMP_PRO_ENABLE,
    WLAN_CFG_PRIV_OVER_TEMP_PRO_REDUCE_PWR_ENABLE,
    WLAN_CFG_PRIV_OVER_TEMP_PRO_SAFE_TH,
    WLAN_CFG_PRIV_OVER_TEMP_PRO_OVER_TH,
    WLAN_CFG_PRIV_OVER_TEMP_PRO_PA_OFF_TH,

    WLAN_DSSS2OFDM_DBB_PWR_BO_VAL,
    WLAN_CFG_PRIV_EVM_PLL_REG_FIX,

    /* VOE */
    WLAN_CFG_PRIV_VOE_SWITCH,
    /* 11ax */
    WLAN_CFG_PRIV_11AX_SWITCH,
    WLAN_CFG_PRIV_HTC_SWITCH,

    /* M_BSSID */
    WLAN_CFG_PRIV_MULTI_BSSID_SWITCH,
    WLAN_CFG_PRIV_AC_SUSPEND,

    /* 动态bypass外置LNA方案 */
    WLAN_CFG_PRIV_DYN_BYPASS_EXTLNA,

    WLAN_CFG_PRIV_CTRL_FRAME_TX_CHAIN,

    WLAN_CFG_PRIV_CTRL_UPC_FOR_18DBM_CO,
    WLAN_CFG_PRIV_CTRL_UPC_FOR_18DBM_C1,
    WLAN_CFG_PRIV_CTRL_11B_DOUBLE_CHAIN_BO_POW,

    /* 非高优先级hcc流控类型 0:SDIO 1:GPIO */
    WLAN_CFG_PRIV_HCC_FLOWCTRL_TYPE,

    /* 注册WiFi动态锁频麒麟接口 */
    WLAN_CFG_PRIV_LOCK_CPU_FREQ,

    /* MBO(Multiband Operation)定制化 */
    WLAN_CFG_PRIV_MBO_SWITCH,
    /* 动态dbac 定制化 */
    WLAN_CFG_PRIV_DYNAMIC_DBAC_SWITCH,

    /* DC流控特性定制化开关 */
    WLAN_CFG_PRIV_DC_FLOWCTL_SWITCH,
    /* phy相关能力定制化 */
    WLAN_CFG_PRIV_PHY_CAP_SWITCH,

    WLAN_CFG_PRIV_HAL_PS_RSSI_PARAM,
    WLAN_CFG_PRIV_HAL_PS_PPS_PARAM,
    /* 优化特性开关 */
    WLAN_CFG_PRIV_OPTIMIZED_FEATURE_SWITCH,

    /* ddr */
    WLAN_CFG_PRIV_DDR_SWITCH,

    /* FEM pow saving 50 */
    WLAN_CFG_PRIV_FEM_DELT_POW,
    WLAN_CFG_PRIV_FEM_ADJ_TPC_TBL_START_IDX,
    /* dev hiex cap定制化 */
    WLAN_CFG_PRIV_HIEX_CAP,
    /* FTM cap定制化 */
    WLAN_CFG_PRIV_FTM_CAP,
    /* TV miracast定制化 */
    WLAN_CFG_PRIV_MIRACAST_SINK,
    /* 去掉W58信道 */
    WLAN_CFG_PRIV_DISABLE_W58_CHANNEL,
    /* 作为P2P GO 允许关联最大用户数 */
    WLAN_CFG_PRIV_P2P_GO_ASSOC_USER_MAX_NUM,

#ifdef _PRE_WLAN_FEATURE_MCAST_AMPDU
    /* 组播聚合定制化 */
    WLAN_CFG_PRIV_MCAST_AMPDU_ENABLE,
#endif
    WLAN_CFG_PRIV_PT_MCAST_ENABLE,
    WLAN_CFG_PRIV_CLOSE_FILTER_SWITCH,

    WLAN_CFG_PRIV_BUTT,
} wlan_cfg_priv;
typedef uint8_t wlan_cfg_priv_id_uint8;

int32_t hwifi_config_init_private_custom(void);
int32_t hwifi_get_init_priv_value(int32_t l_cfg_id, int32_t *pl_priv_value);
int32_t hwifi_custom_adapt_device_priv_ini_param(uint8_t *puc_data);

#endif  // hisi_customize_config_priv_hi1103.h
