################################################################################
#
# cog-ai-extension
#
################################################################################

COG_AI_EXTENSION_VERSION = 1.0
COG_AI_EXTENSION_SITE = $(NERVES_DEFCONFIG_DIR)/package/cog-ai-extension/src
COG_AI_EXTENSION_SITE_METHOD = local
COG_AI_EXTENSION_LICENSE = MIT
COG_AI_EXTENSION_DEPENDENCIES = cog libsoup3

COG_AI_EXTENSION_INSTALL_DIR = /usr/lib/cog-extensions

define COG_AI_EXTENSION_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) \
		$(shell $(HOST_DIR)/bin/pkg-config --cflags wpe-web-process-extension-2.0 glib-2.0 libsoup-3.0) \
		-shared -fPIC -o $(@D)/cog-ai-extension.so \
		$(@D)/extension.c \
		$(shell $(HOST_DIR)/bin/pkg-config --libs glib-2.0 libsoup-3.0) \
		$(TARGET_LDFLAGS)
endef

define COG_AI_EXTENSION_INSTALL_TARGET_CMDS
	$(INSTALL) -d $(TARGET_DIR)$(COG_AI_EXTENSION_INSTALL_DIR)
	$(INSTALL) -m 0755 $(@D)/cog-ai-extension.so \
		$(TARGET_DIR)$(COG_AI_EXTENSION_INSTALL_DIR)/
	$(INSTALL) -m 0644 $(@D)/overlay.js \
		$(TARGET_DIR)$(COG_AI_EXTENSION_INSTALL_DIR)/
endef

$(eval $(generic-package))
