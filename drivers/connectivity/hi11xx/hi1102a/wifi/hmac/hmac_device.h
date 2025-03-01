

#ifndef __HMAC_DEVICE_H__
#define __HMAC_DEVICE_H__

/* 1 其他头文件包含 */
#include "oal_ext_if.h"
#include "oam_ext_if.h"
#include "wlan_spec.h"
#include "hal_ext_if.h"
#include "dmac_ext_if.h"
#include "mac_vap.h"
#include "hmac_vap.h"
#ifdef _PRE_WLAN_TCP_OPT
#include "hmac_tcp_opt_struc.h"
#include "oal_hcc_host_if.h"
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#undef THIS_FILE_ID
#define THIS_FILE_ID OAM_FILE_ID_HMAC_DEVICE_H
/* 2 宏定义 */
#ifdef _PRE_WLAN_TCP_OPT
#define HCC_TRANS_THREAD_POLICY   SCHED_FIFO
#define HCC_TRANS_THERAD_PRIORITY (10)
#define HCC_TRANS_THERAD_NICE     (-10)
#endif
#define is_equal_rates(rate1, rate2) (((rate1)&0x7f) == ((rate2)&0x7f))

#if (_PRE_WLAN_FEATURE_PMF != _PRE_PMF_NOT_SUPPORT)
#define PMF_BLACK_LIST_MAX_CNT 6
#endif

/* 4 全局变量声明 */
/* 5 消息头定义 */
/* 6 消息定义 */
/* 7 STRUCT定义 */
/* 存储每个扫描到的bss信息 */
typedef struct {
    oal_dlist_head_stru st_dlist_head;  /* 链表指针 */
    mac_bss_dscr_stru st_bss_dscr_info; /* bss描述信息，包括上报的管理帧 */
} hmac_scanned_bss_info;

/* 存储在hmac device下的扫描结果维护的结构体 */
typedef struct {
    oal_spin_lock_stru st_lock;
    oal_dlist_head_stru st_bss_list_head;
    oal_uint32 ul_bss_num;
} hmac_bss_mgmt_stru;

/* 扫描运行结果记录 */
typedef struct {
    hmac_bss_mgmt_stru st_bss_mgmt;                                  /* 存储扫描BSS结果的管理结构 */
    mac_scan_chan_stats_stru ast_chan_results[WLAN_MAX_CHANNEL_NUM]; /* 信道统计/测量结果 */
    oal_uint8 uc_chan_numbers;                                       /* 此次扫描总共需要扫描的信道个数 */
    oal_uint8 uc_device_id : 4;
    oal_uint8 uc_chip_id : 4;
    oal_uint8 uc_vap_id;                           /* 本次执行扫描的vap id */
    mac_scan_status_enum_uint8 en_scan_rsp_status; /* 本次扫描完成返回的状态码，是成功还是被拒绝 */

    oal_time_us_stru st_scan_start_timestamp; /* 扫描维测使用 */
    mac_scan_cb_fn p_fn_cb;                   /* 此次扫描结束的回调函数指针 */

    oal_uint64 ull_cookie;                      /* 保存P2P 监听结束上报的cookie 值 */
    mac_vap_state_enum_uint8 en_vap_last_state; /* 保存VAP进入扫描前的状态,AP/P2P GO模式下20/40M扫描专用 */
    oal_time_t_stru st_scan_start_time;         /* 扫描起始时间戳 */
    // 增加记录扫描类型，以便识别CHBA RRM扫描的相关处理
    wlan_scan_mode_enum_uint8 en_scan_mode;
} hmac_scan_record_stru;

/* 扫描相关相关控制信息 */
typedef struct {
    /* scan 相关控制信息 */
    oal_bool_enum_uint8 en_is_scanning;             /* host侧的扫描请求是否正在执行 */
    oal_bool_enum_uint8 en_is_random_mac_addr_scan; /* 是否为随机mac addr扫描，默认关闭(定制化宏开启下废弃) */
    oal_bool_enum_uint8 en_complete;                /* 内核普通扫描请求是否完成标志 */
    oal_bool_enum_uint8 en_sched_scan_complete;     /* 调度扫描是否正在运行标记 */

    oal_cfg80211_scan_request_stru *pst_request;              /* 内核下发的扫描请求结构体 */
    oal_cfg80211_sched_scan_request_stru *pst_sched_scan_req; /* 内核下发的调度扫描请求结构体 */

    oal_wait_queue_head_stru st_wait_queue;
    oal_spin_lock_stru st_scan_request_spinlock; /* 内核下发的request资源锁 */

    frw_timeout_stru st_scan_timeout; /* 扫描模块host侧的超时保护所使用的定时器 */
#if defined(_PRE_WLAN_FEATURE_20_40_80_COEXIST)
    frw_timeout_stru st_init_scan_timeout;
#endif
    oal_uint8 auc_random_mac[WLAN_MAC_ADDR_LEN]; /* 扫描时候用的随机MAC */
    uint8_t random_mac_from_kernel; /* kernel下发随机mac扫描 */
    uint8_t auc_resv[1]; // 1代表保留字节个数，4字节对齐

    hmac_scan_record_stru st_scan_record_mgmt; /* 扫描运行记录管理信息，包括扫描结果和发起扫描者的相关信息 */
} hmac_scan_stru;

typedef struct {
    oal_uint32 ul_tx_large_pps;       /* 本周期上报的tx pps */
    oal_uint32 ul_rx_large_pps;       /* 本周期上报的rx pps */
    oal_uint32 ul_tx_small_pps;       /* 本周期上报的tx pps */
    oal_uint32 ul_rx_small_pps;       /* 本周期上报的rx pps */
    oal_uint32 ul_rx_large_amsdu_pps; /* 本周期上报的rx 大包amsdu pps */
    oal_uint32 ul_base_th;            /* 当前大包描述符基准值 */
    oal_uint32 ul_large_queue_th;     /* 当前大包队列描述符门限 */
    oal_uint32 ul_small_queue_th;     /* 当前小包队列描述符门限 */
    oal_bool_enum_uint8 en_flow_type_tx; /* 当前是否为发送场景 */
} hmac_dscr_th_opt_stru;

#if (_PRE_WLAN_FEATURE_PMF != _PRE_PMF_NOT_SUPPORT)
typedef struct {
    oal_uint8 uc_cnt;
    oal_uint8 uc_first_idx; /* 用于记录最早的黑名单下标 */
    oal_uint8 auc_black_list[PMF_BLACK_LIST_MAX_CNT][WLAN_MAC_ADDR_LEN];
} hmac_pmf_black_list_stru;
#endif

typedef struct {
    oal_bool_enum_uint8 en_pkt_check_on;
    oal_bool_enum_uint8 en_pkt_check_completed;
    oal_bool_enum_uint8 en_pkt_check_result;
    oal_uint8 auc_reserved[1];
    oal_wait_queue_head_stru st_check_wait_q;
} hmac_packet_check_stru;

#ifdef _PRE_WLAN_FEATURE_NRCOEX
typedef struct {
    oal_wait_queue_head_stru st_wait_queue;
    wlan_nrcoex_info_stru st_nrcoex_info;
    oal_bool_enum_uint8 en_query_completed_flag;
} hmac_nrcoex_info_query_stru;
#endif
typedef struct {
    oal_wait_queue_head_stru st_wait_queue;
    oal_bool_enum_uint8 auc_complete_flag[MAC_PSM_QUERY_TYPE_BUTT];
    mac_psm_query_stat_stru ast_psm_stat[MAC_PSM_QUERY_TYPE_BUTT];
}hmac_psm_flt_stat_query_stru;

/* hmac device结构体，记录只保存在hmac的device公共信息 */
typedef struct {
    hmac_scan_stru st_scan_mgmt; /* 扫描管理结构体 */
#if (_PRE_OS_VERSION_WIN32 == _PRE_OS_VERSION)
    oal_uint8 uc_desired_bss_num; /* 扫描到的期望的bss个数 */
    oal_uint8 auc_resv[3];
    oal_uint8 auc_desired_bss_idx[WLAN_MAX_SCAN_BSS_NUM]; /* 期望加入的bss在bss list中的位置 */
#endif
    oal_uint32 ul_p2p_intf_status;
    oal_wait_queue_head_stru st_netif_change_event;
    mac_device_stru *pst_device_base_info; /* 指向公共部分mac device */
#if defined(_PRE_WLAN_FEATURE_20_40_80_COEXIST)
    oal_bool_enum_uint8 en_init_scan : 1;
    oal_bool_enum_uint8 en_start_via_priv : 1;
    oal_bool_enum_uint8 en_in_init_scan : 1;
    oal_bool_enum_uint8 en_rescan_idle : 1;
    oal_uint8 uc_resv_bit : 4;
    oal_uint8 auc_resvx[3];
    mac_channel_stru ast_best_channel[WLAN_BAND_BUTT];
#endif
#ifdef _PRE_WLAN_TCP_OPT
    oal_bool_enum_uint8 sys_tcp_rx_ack_opt_enable;
    oal_bool_enum_uint8 sys_tcp_tx_ack_opt_enable;
    oal_uint8 auc_resev[2];

#endif
#if ((_PRE_MULTI_CORE_MODE_OFFLOAD_DMAC == _PRE_MULTI_CORE_MODE) && (_PRE_OS_VERSION_LINUX == _PRE_OS_VERSION))
#ifdef CONFIG_HAS_EARLYSUSPEND
    struct early_suspend early_suspend;  // early_suspend支持
#endif
    oal_spin_lock_stru st_suspend_lock;
#endif
#ifdef _PRE_WLAN_FEATURE_BTCOEX
    hmac_device_btcoex_stru st_hmac_device_btcoex;
#endif
    oal_bool_enum_uint8 en_txop_limit;    /* 是否开启txop */
    hmac_dscr_th_opt_stru st_dscr_th_opt; /* 描述符门限优化结构体 */

#if (_PRE_WLAN_FEATURE_PMF != _PRE_PMF_NOT_SUPPORT)
    hmac_pmf_black_list_stru st_pmf_black_list;
#endif
    hmac_packet_check_stru st_packet_check;
#ifdef _PRE_WLAN_FEATURE_NRCOEX
    hmac_nrcoex_info_query_stru st_nrcoex_query;
#endif
    hmac_psm_flt_stat_query_stru st_psm_flt_stat_query;
#ifdef _PRE_WLAN_CHBA_MGMT
    mac_chba_island_coex_info island_coex_info;
#endif
} hmac_device_stru;

/* 8 UNION定义 */
/* 9 OTHERS定义 */
extern oal_uint8 wlan_pm_get_switch(void);
extern void wlan_pm_set_switch(oal_uint8 pm_switch);
extern oal_uint32 hmac_board_exit(mac_board_stru *pst_board);

#if (_PRE_MULTI_CORE_MODE_OFFLOAD_DMAC == _PRE_MULTI_CORE_MODE)
extern oal_uint32 hmac_config_host_dev_init(mac_vap_stru *pst_mac_vap, oal_uint16 us_len, oal_uint8 *puc_param);
extern oal_uint32 hmac_config_host_dev_exit(mac_vap_stru *pst_mac_vap);
extern oal_uint32 hmac_board_init(mac_board_stru *pst_board);
#else
extern oal_uint32 hmac_board_init(oal_uint32 ul_chip_max_num, mac_chip_stru *pst_chip);
#endif
extern oal_void hmac_device_set_random_mac_for_scan(mac_device_stru *pst_mac_dev, mac_vap_stru *pst_mac_vap);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* end of mac_device.h */
