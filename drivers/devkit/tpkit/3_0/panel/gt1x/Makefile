#goodix gt1x
obj-$(CONFIG_GOODIX_GT1X_TS_KIT_3_0) += gt1x_ts.o
gt1x_ts-objs += gt1x.o gt1x_update.o gt1x_test.o gt1x_tools.o

EXTRA_CFLAGS += -Iarch/arm/mach-hisi
EXTRA_CFLAGS += -I$(srctree)/drivers/hisi/tzdriver
EXTRA_CFLAGS += -I$(srctree)/drivers/tzdriver
EXTRA_CFLAGS += -I$(srctree)/drivers/devkit/lcdkit/lcdkit3.0/kernel/common/include
EXTRA_CFLAGS += -I$(srctree)/drivers/devkit/tpkit/3_0

