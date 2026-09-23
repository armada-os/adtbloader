#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# RPSettings executes each physical line as an independent root command.

mkdir -p /sdcard/adtbloader
cp /sys/firmware/fdt /sdcard/adtbloader/android.dtb
printf 'model:\n' > /sdcard/adtbloader/identity.txt
tr '\000' '\n' < /sys/firmware/devicetree/base/model >> /sdcard/adtbloader/identity.txt
printf 'compatible:\n' >> /sdcard/adtbloader/identity.txt
tr '\000' '\n' < /sys/firmware/devicetree/base/compatible >> /sdcard/adtbloader/identity.txt
printf 'qcom,msm-id:' >> /sdcard/adtbloader/identity.txt
od -An -t x1 /sys/firmware/devicetree/base/qcom,msm-id >> /sdcard/adtbloader/identity.txt
printf 'qcom,board-id:' >> /sdcard/adtbloader/identity.txt
od -An -t x1 /sys/firmware/devicetree/base/qcom,board-id >> /sdcard/adtbloader/identity.txt
printf 'default-panel-phandle:' >> /sdcard/adtbloader/identity.txt; od -An -t x1 /sys/firmware/devicetree/base/soc/qcom,dsi-display-primary/qcom,dsi-default-panel >> /sdcard/adtbloader/identity.txt
printf 'default-panel-node:\n' >> /sdcard/adtbloader/identity.txt
find /sys/firmware/devicetree/base/soc/qcom,mdss_mdp@* -maxdepth 2 -type f -name phandle -exec cmp -s /sys/firmware/devicetree/base/soc/qcom,dsi-display-primary/qcom,dsi-default-panel {} \; -exec dirname {} \; >> /sdcard/adtbloader/identity.txt
if [ -r /sys/firmware/devicetree/base/soc/qcom,dsi-display-secondary/qcom,dsi-default-panel ]; then printf 'secondary-panel-phandle:' >> /sdcard/adtbloader/identity.txt; od -An -t x1 /sys/firmware/devicetree/base/soc/qcom,dsi-display-secondary/qcom,dsi-default-panel >> /sdcard/adtbloader/identity.txt; fi
if [ -r /sys/firmware/devicetree/base/soc/qcom,dsi-display-secondary/qcom,dsi-default-panel ]; then printf 'secondary-panel-node:\n' >> /sdcard/adtbloader/identity.txt; find /sys/firmware/devicetree/base/soc/qcom,mdss_mdp@* -maxdepth 2 -type f -name phandle -exec cmp -s /sys/firmware/devicetree/base/soc/qcom,dsi-display-secondary/qcom,dsi-default-panel {} \; -exec dirname {} \; >> /sdcard/adtbloader/identity.txt; fi
printf 'fdt: /sdcard/adtbloader/android.dtb\n' >> /sdcard/adtbloader/identity.txt
chmod 0644 /sdcard/adtbloader/identity.txt /sdcard/adtbloader/android.dtb
