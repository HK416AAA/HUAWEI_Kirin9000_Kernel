/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * rt9471_charger.h
 *
 * rt9471 driver
 *
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2019. All rights reserved.
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

#ifndef _RT9471_CHARGER_H_
#define _RT9471_CHARGER_H_

#define RT9471_SLAVE_ADDR                0x53
#define RT9471_DEVID                     0x0D
#define RT9471D_DEVID                    0x0E

enum rt9471_reg_addr {
	RT9471_REG_OTGCFG = 0x00,
	RT9471_REG_TOP,
	RT9471_REG_FUNCTION,
	RT9471_REG_IBUS,
	RT9471_REG_VBUS,
	RT9471_REG_PRECHG,
	RT9471_REG_REGU,
	RT9471_REG_VCHG,
	RT9471_REG_ICHG,
	RT9471_REG_CHGTIMER,
	RT9471_REG_EOC,
	RT9471_REG_INFO,
	RT9471_REG_JEITA,
	RT9471_REG_STATUS = 0x0F,
	RT9471_REG_STAT0,
	RT9471_REG_STAT1,
	RT9471_REG_STAT2,
	RT9471_REG_STAT3,
	RT9471_REG_IRQ0 = 0x20,
	RT9471_REG_IRQ1,
	RT9471_REG_IRQ2,
	RT9471_REG_IRQ3,
	RT9471_REG_MASK0 = 0x30,
	RT9471_REG_MASK1,
	RT9471_REG_MASK2,
	RT9471_REG_MASK3,
	RT9471_REG_HIDDEN_0 = 0x40,
	RT9471_REG_TOP_HDEN = 0x43,
	RT9471_REG_BUCK_HDEN3 = 0x54,
	RT9471_REG_BUCK_HDEN4,
	RT9471_REG_OTG_HDEN2 = 0x58,
	RT9471_REG_BUCK_HDEN5,
};

/* OTGCFG 0x01 */
#define RT9471_OTGCC_SHIFT                0
#define RT9471_OTGCC_MASK                 BIT(RT9471_OTGCC_SHIFT)

/* TOP 0x01 */
#define RT9471_WDT_SHIFT                  0
#define RT9471_WDT_MASK                   0x03
#define RT9471_WDTCNTRST_MASK             BIT(2)
#define RT9471_DISI2CTO_MASK              BIT(3)
#define RT9471_QONRST_MASK                BIT(7)

/* FUNCTION 0x02 */
#define RT9471_CHG_EN_SHIFT               0
#define RT9471_CHG_EN_MASK                BIT(RT9471_CHG_EN_SHIFT)
#define RT9471_OTG_EN_SHIFT               1
#define RT9471_OTG_EN_MASK                BIT(RT9471_OTG_EN_SHIFT)
#define RT9471_HZ_SHIFT                   5
#define RT9471_HZ_MASK                    BIT(RT9471_HZ_SHIFT)
#define RT9471_BATFETDIS_SHIFT            7
#define RT9471_BATFETDIS_MASK             BIT(RT9471_BATFETDIS_SHIFT)

/* IBUS 0x03 */
#define RT9471_AICR_SHIFT                 0
#define RT9471_AICR_MASK                  0x3F
#define RT9471_AICR_MIN                   50
#define RT9471_AICR_MAX                   3200
#define RT9471_AICR_STEP                  50
#define RT9471_AICR_500                   500
#define RT9471_AICR_100                   100
#define RT9471_AUTOAICR_MASK              BIT(6)

/* VBUS 0x04 */
#define RT9471_ACOV_SHIFT                 6
#define RT9471_ACOV_MASK                  0xC0
#define RT9471_ACOV_5P8                   5800
#define RT9471_ACOV_6P5                   6500
#define RT9471_ACOV_10P9                  10900
#define RT9471_ACOV_14                    14000
#define RT9471_MIVR_SHIFT                 0
#define RT9471_MIVR_MASK                  0x0F
#define RT9471_MIVR_MIN                   3900
#define RT9471_MIVR_MAX                   5400
#define RT9471_MIVR_4P5                   4500
#define RT9471_MIVR_STEP                  100
#define RT9471_MIVRTRACK_SHIFT            4
#define RT9471_MIVRTRACK_MASK             0x30

/* VCHG 0x07 */
#define RT9471_VRECHG_MASK                BIT(7)
#define RT9471_CV_SHIFT                   0
#define RT9471_CV_MASK                    0x7F
#define RT9471_CV_MIN                     3900
#define RT9471_CV_MAX                     4700
#define RT9471_CV_STEP                    10
#define RT9471_CV_4P2                     4200

/* ICHG 0x08 */
#define RT9471_ICHG_SHIFT                 0
#define RT9471_ICHG_MASK                  0x3F
#define RT9471_ICHG_MIN                   0
#define RT9471_ICHG_MAX                   3150
#define RT9471_ICHG_STEP                  50
#define RT9471_ICHG_2000                  2000

/* CHGTIMER 0x09 */
#define RT9471_SAFETMR_EN_SHIFT           7
#define RT9471_SAFETMR_EN_MASK            BIT(RT9471_SAFETMR_EN_SHIFT)
#define RT9471_SAFETMR_SHIFT              4
#define RT9471_SAFETMR_MASK               0x30
#define RT9471_SAFETMR_MIN                5
#define RT9471_SAFETMR_MAX                20
#define RT9471_SAFETMR_STEP               5
#define RT9471_SAFETMR_15                 15

/* EOC 0x0A */
#define RT9471_IEOC_SHIFT                 4
#define RT9471_IEOC_MASK                  0xF0
#define RT9471_IEOC_MIN                   50
#define RT9471_IEOC_MAX                   800
#define RT9471_IEOC_STEP                  50
#define RT9471_IEOC_200                   200
#define RT9471_TE_SHIFT                   1
#define RT9471_TE_MASK                    BIT(RT9471_TE_SHIFT)
#define RT9471_EOC_RST_SHIFT              0
#define RT9471_EOC_RST_MASK               BIT(RT9471_EOC_RST_SHIFT)

/* INFO 0x0B */
#define RT9471_REGRST_MASK                BIT(7)
#define RT9471_DEVID_SHIFT                3
#define RT9471_DEVID_MASK                 0x78
#define RT9471_DEVREV_SHIFT               0
#define RT9471_DEVREV_MASK                0x03

/* JEITA 0x0C */
#define RT9471_JEITA_EN_MASK              BIT(7)

/* STATUS 0x0F */
#define RT9471_ICSTAT_SHIFT               0
#define RT9471_ICSTAT_MASK                0x0F

/* STAT0 0x10 */
#define RT9471_ST_VBUSGD_SHIFT            7
#define RT9471_ST_VBUSGD_MASK             BIT(RT9471_ST_VBUSGD_SHIFT)

/*  STAT1 0x11 */
#define RT9471_ST_MIVR_SHIFT              7
#define RT9471_ST_MIVR_MASK               BIT(RT9471_ST_MIVR_SHIFT)
#define RT9471_ST_AICR_MASK               BIT(6)
#define RT9471_ST_BATOV_MASK              BIT(1)

/* STAT3 0x13 */
#define RT9471_ST_VACOV_SHIFT             6
#define RT9471_ST_VACOV_MASK              BIT(RT9471_ST_VACOV_SHIFT)

/* HIDDEN_0 0x40 */
#define RT9471_CHIP_REV_SHIFT             5
#define RT9471_CHIP_REV_MASK              0xE0
#define RT9471_CHIP_VER_3                 3

/* TOP_HDEN 0x43 */
#define RT9471_FORCE_EN_VBUS_SINK_SHIFT   4
#define RT9471_FORCE_EN_VBUS_SINK_MASK    BIT(RT9471_FORCE_EN_VBUS_SINK_SHIFT)

/* OTG_HDEN2 0x58 */
#define RT9471_REG_OTG_RES_COMP_SHIFT     4
#define RT9471_REG_OTG_RES_COMP_MASK      0x30

#define WDT_40S                           40
#define BIT_LEN                           8
#define RT9471_REG_NONE                   0x00
#define RT9471_REG_NONE_MASK              0xFF
#define RT9471_REG_NONE_SHIFT             0x00

#define CHG_NAME_EX_LEN_10                10
#define CHG_NAME_EX_LEN_5                 5
#define RT9471_IRQ_GROUP_LEN              8

#define CONVERT_MULTI_1000                1000
#define CUST_MIN_CV                       4350
#define HIZ_IIN_FLAG_TRUE                 1
#define HIZ_IIN_FLAG_FALSE                0

#define BIT_TRUE                          1

#endif /* _RT9471_CHARGER_H_ */
