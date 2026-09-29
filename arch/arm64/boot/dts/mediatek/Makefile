# SPDX-License-Identifier: GPL-2.0
dtb-$(CONFIG_ARCH_MEDIATEK) += mt2712-evb.dtb
dtb-$(CONFIG_ARCH_MEDIATEK) += mt6755-evb.dtb
dtb-$(CONFIG_ARCH_MEDIATEK) += mt6795-evb.dtb
dtb-$(CONFIG_ARCH_MEDIATEK) += mt6797-evb.dtb
dtb-$(CONFIG_ARCH_MEDIATEK) += mt6797-x20-dev.dtb
dtb-$(CONFIG_ARCH_MEDIATEK) += mt6873-evb.dtb
dtb-$(CONFIG_ARCH_MEDIATEK) += mt7622-rfb1.dtb
dtb-$(CONFIG_ARCH_MEDIATEK) += mt7622-bananapi-bpi-r64.dtb
dtb-$(CONFIG_ARCH_MEDIATEK) += mt8173-elm.dtb
dtb-$(CONFIG_ARCH_MEDIATEK) += mt8173-elm-hana.dtb
dtb-$(CONFIG_ARCH_MEDIATEK) += mt8173-elm-hana-rev7.dtb
dtb-$(CONFIG_ARCH_MEDIATEK) += mt8173-evb.dtb
dtb-$(CONFIG_ARCH_MEDIATEK) += mt8183-evb.dtb
dtb-$(CONFIG_ARCH_MEDIATEK) += mt8183-kukui-krane-sku176.dtb
dtb-$(CONFIG_ARCH_MEDIATEK) += mt8188.dtb
dtb-$(CONFIG_ARCH_MEDIATEK) += mt8516-pumpkin.dtb

dtbo-$(CONFIG_ARCH_MEDIATEK) += sunstone_proto.dtbo
dtbo-$(CONFIG_ARCH_MEDIATEK) += sunstone_hvt.dtbo
dtbo-$(CONFIG_ARCH_MEDIATEK) += sunstone_evt.dtbo
dtbo-$(CONFIG_ARCH_MEDIATEK) += sunstone_dvt.dtbo

extra-y += $(dtbo-y)

quiet_cmd_dtbo = DTCO    $@
      cmd_dtbo = $(subst -O dtbo,-O dtb,$(cmd_dtc))

$(obj)/%.dtbo: $(src)/%.dts $(DTC) FORCE
	$(call if_changed_dep,dtbo)
