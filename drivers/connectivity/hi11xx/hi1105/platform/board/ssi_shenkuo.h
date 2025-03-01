

#ifdef _PRE_CONFIG_GPIO_TO_SSI_DEBUG

#ifndef __SSI_SHENKUO_H__
#define __SSI_SHENKUO_H__

/* 以下寄存器是shenkuo device定义 */
#define SHENKUO_GLB_CTL_BASE                    0x40000000
#define SHENKUO_GLB_CTL_SOFT_RST_BCPU_REG       (SHENKUO_GLB_CTL_BASE + 0x98)
#define SHENKUO_GLB_CTL_SOFT_RST_GCPU_REG       (SHENKUO_GLB_CTL_BASE + 0x9C)
#define SHENKUO_GLB_CTL_SYS_TICK_CFG_W_REG      (SHENKUO_GLB_CTL_BASE + 0xC0) /* 写1清零systick，写0无效 */
#define SHENKUO_GLB_CTL_SYS_TICK_VALUE_W_0_REG  (SHENKUO_GLB_CTL_BASE + 0xD0)
#define SHENKUO_GLB_CTL_SYS_TICK_CFG_B_REG      (SHENKUO_GLB_CTL_BASE + 0xE0) /* 写1清零systick，写0无效 */
#define SHENKUO_GLB_CTL_SYS_TICK_VALUE_B_0_REG  (SHENKUO_GLB_CTL_BASE + 0xF0)
#define SHENKUO_GLB_CTL_PWR_ON_LABLE_REG        (SHENKUO_GLB_CTL_BASE + 0x200) /* 芯片上电标记寄存器 */
#define SHENKUO_GLB_CTL_WCPU0_LOAD_REG          (SHENKUO_GLB_CTL_BASE + 0x1750) /* WCPU0_LOAD */
#define SHENKUO_GLB_CTL_WCPU0_PC_L_REG          (SHENKUO_GLB_CTL_BASE + 0x1754) /* WCPU0_PC低16bit */
#define SHENKUO_GLB_CTL_WCPU0_PC_H_REG          (SHENKUO_GLB_CTL_BASE + 0x1758) /* WCPU0_PC高16bit */
#define SHENKUO_GLB_CTL_WCPU1_LOAD_REG          (SHENKUO_GLB_CTL_BASE + 0x175C) /* WCPU1_LOAD */
#define SHENKUO_GLB_CTL_WCPU1_PC_L_REG          (SHENKUO_GLB_CTL_BASE + 0x1760) /* WCPU1_PC低16bit */
#define SHENKUO_GLB_CTL_WCPU1_PC_H_REG          (SHENKUO_GLB_CTL_BASE + 0x1764) /* WCPU1_PC高16bit */
#define SHENKUO_GLB_CTL_GCPU_LOAD_REG           (SHENKUO_GLB_CTL_BASE + 0x1B00) /* GCPU_LOAD */
#define SHENKUO_GLB_CTL_GCPU_PC_L_REG           (SHENKUO_GLB_CTL_BASE + 0x1B04) /* GCPU_PC低16bit */
#define SHENKUO_GLB_CTL_GCPU_PC_H_REG           (SHENKUO_GLB_CTL_BASE + 0x1B08) /* GCPU_PC高16bit */
#define SHENKUO_GLB_CTL_GCPU_LR_L_REG           (SHENKUO_GLB_CTL_BASE + 0x1B0C) /* GCPU_LR低16bit */
#define SHENKUO_GLB_CTL_GCPU_LR_H_REG           (SHENKUO_GLB_CTL_BASE + 0x1B10) /* GCPU_LR高16bit */
#define SHENKUO_GLB_CTL_GCPU_SP_L_REG           (SHENKUO_GLB_CTL_BASE + 0x1B14) /* GCPU_SP低16bit */
#define SHENKUO_GLB_CTL_GCPU_SP_H_REG           (SHENKUO_GLB_CTL_BASE + 0x1B18) /* GCPU_SP高16bit */
#define SHENKUO_GLB_CTL_BCPU_LOAD_REG           (SHENKUO_GLB_CTL_BASE + 0x1C00) /* BCPU_LOAD */
#define SHENKUO_GLB_CTL_BCPU_PC_L_REG           (SHENKUO_GLB_CTL_BASE + 0x1C04) /* BCPU_PC低16bit */
#define SHENKUO_GLB_CTL_BCPU_PC_H_REG           (SHENKUO_GLB_CTL_BASE + 0x1C08) /* BCPU_PC高16bit */
#define SHENKUO_GLB_CTL_BCPU_LR_L_REG           (SHENKUO_GLB_CTL_BASE + 0x1C0C) /* BCPU_LR低16bit */
#define SHENKUO_GLB_CTL_BCPU_LR_H_REG           (SHENKUO_GLB_CTL_BASE + 0x1C10) /* BCPU_LR高16bit */
#define SHENKUO_GLB_CTL_BCPU_SP_L_REG           (SHENKUO_GLB_CTL_BASE + 0x1C14) /* BCPU_SP低16bit */
#define SHENKUO_GLB_CTL_BCPU_SP_H_REG           (SHENKUO_GLB_CTL_BASE + 0x1C18) /* BCPU_SP高16bit */
#define SHENKUO_GLB_CTL_TCXO_DET_CTL_REG        (SHENKUO_GLB_CTL_BASE + 0x800) /* TCXO时钟检测控制寄存器 */
#define SHENKUO_GLB_CTL_TCXO_32K_DET_CNT_REG    (SHENKUO_GLB_CTL_BASE + 0x804) /* TCXO时钟检测控制寄存器 */
#define SHENKUO_GLB_CTL_TCXO_32K_DET_RESULT_REG (SHENKUO_GLB_CTL_BASE + 0x808) /* TCXO时钟检测控制寄存器 */
#define SHENKUO_GLB_CTL_WCPU_WAIT_CTL_REG       (SHENKUO_GLB_CTL_BASE + 0xF00)
#define SHENKUO_GLB_CTL_BCPU_WAIT_CTL_REG       (SHENKUO_GLB_CTL_BASE + 0xF04)

#define SHENKUO_PMU_CMU_CTL_BASE                    0x40002000
#define SHENKUO_PMU_CMU_CTL_SYS_STATUS_0_REG        (SHENKUO_PMU_CMU_CTL_BASE + 0x1E0) /* 系统状态 */
#define SHENKUO_PMU_CMU_CTL_CLOCK_TCXO_PRESENCE_DET (SHENKUO_PMU_CMU_CTL_BASE + 0x040) /* TCXO时钟在位检测 */

#define SHENKUO_B_CTL_BASE                      0x40200000
#define SHENKUO_DIRECT_BCPU_LOAD_REG          (SHENKUO_B_CTL_BASE + 0x760) /* BCPU_LOAD */
#define SHENKUO_DIRECT_BCPU_PC_L_REG          (SHENKUO_B_CTL_BASE + 0x764) /* BCPU_PC低16bit */
#define SHENKUO_DIRECT_BCPU_PC_H_REG          (SHENKUO_B_CTL_BASE + 0x768) /* BCPU_PC高16bit */
#define SHENKUO_DIRECT_BCPU_LR_L_REG          (SHENKUO_B_CTL_BASE + 0x76C) /* BCPU_LR低16bit */
#define SHENKUO_DIRECT_BCPU_LR_H_REG          (SHENKUO_B_CTL_BASE + 0x770) /* BCPU_LR高16bit */
#define SHENKUO_DIRECT_BCPU_SP_L_REG          (SHENKUO_B_CTL_BASE + 0x774) /* BCPU_SP低16bit */
#define SHENKUO_DIRECT_BCPU_SP_H_REG          (SHENKUO_B_CTL_BASE + 0x778) /* BCPU_SP高16bit */

#define SHENKUO_G_CTL_BASE                      0x40300000
#define SHENKUO_DIRECT_GCPU_LOAD_REG          (SHENKUO_G_CTL_BASE + 0x760) /* GCPU_LOAD */
#define SHENKUO_DIRECT_GCPU_PC_L_REG          (SHENKUO_G_CTL_BASE + 0x764) /* GCPU_PC低16bit */
#define SHENKUO_DIRECT_GCPU_PC_H_REG          (SHENKUO_G_CTL_BASE + 0x768) /* GCPU_PC高16bit */
#define SHENKUO_DIRECT_GCPU_LR_L_REG          (SHENKUO_G_CTL_BASE + 0x76C) /* GCPU_LR低16bit */
#define SHENKUO_DIRECT_GCPU_LR_H_REG          (SHENKUO_G_CTL_BASE + 0x770) /* GCPU_LR高16bit */
#define SHENKUO_DIRECT_GCPU_SP_L_REG          (SHENKUO_G_CTL_BASE + 0x774) /* GCPU_SP低16bit */
#define SHENKUO_DIRECT_GCPU_SP_H_REG          (SHENKUO_G_CTL_BASE + 0x778) /* GCPU_SP高16bit */

int shenkuo_ssi_read_wcpu_pc_lr_sp(void);
int shenkuo_ssi_read_bpcu_pc_lr_sp(void);
int shenkuo_ssi_read_gpcu_pc_lr_sp(void);
int shenkuo_ssi_read_device_arm_register(void);
int shenkuo_ssi_device_regs_dump(unsigned long long module_set);
int shenkuo_ssi_check_tcxo_available(void);

#endif /* #ifndef __SSI_SHENKUO_H__ */
#endif /* #ifdef __PRE_CONFIG_GPIO_TO_SSI_DEBUG */
