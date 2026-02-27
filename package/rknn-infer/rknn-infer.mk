################################################################################
#
# rknn-infer
#
################################################################################

RKNN_INFER_VERSION = 1.0
RKNN_INFER_SITE = $(NERVES_DEFCONFIG_DIR)/package/rknn-infer/src
RKNN_INFER_SITE_METHOD = local
RKNN_INFER_DEPENDENCIES = rockchip-rknpu2

define RKNN_INFER_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) \
		-I$(STAGING_DIR)/usr/include \
		-o $(@D)/rknn_infer \
		$(@D)/rknn_infer.c \
		-L$(STAGING_DIR)/usr/lib -lrknnrt -lpthread -ldl -lm
endef

define RKNN_INFER_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/rknn_infer $(TARGET_DIR)/usr/bin/rknn_infer
endef

$(eval $(generic-package))
