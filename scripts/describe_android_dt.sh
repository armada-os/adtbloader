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
panel=$(find /sys/firmware/devicetree/base/soc/qcom,mdss_mdp@* -maxdepth 2 -type f -name phandle -exec sh -c 'cmp -s /sys/firmware/devicetree/base/soc/qcom,dsi-display-primary/qcom,dsi-default-panel "$1" && dirname "$1"' sh {} \; | head -n 1); if [ -n "$panel" ]; then printf 'default-panel-node: %s\n' "$panel"; if [ -r "$panel/compatible" ]; then printf 'default-panel-compatible:\n'; tr '\000' '\n' < "$panel/compatible"; fi; if [ -r "$panel/qcom,mdss-dsi-panel-name" ]; then printf 'default-panel-name:\n'; tr '\000' '\n' < "$panel/qcom,mdss-dsi-panel-name"; fi; fi >> /sdcard/adtbloader/identity.txt
printf 'fdt: /sdcard/adtbloader/android.dtb\n' >> /sdcard/adtbloader/identity.txt
chmod 0644 /sdcard/adtbloader/identity.txt /sdcard/adtbloader/android.dtb
