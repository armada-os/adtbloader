// SPDX-License-Identifier: BSD-3-Clause
/* Copyright (c) 2024 Nikita Travkin <nikita@trvn.ru> */

#include <efi.h>
#include <efilib.h>
#include <libfdt.h>

#include <util.h>
#include <device.h>
#include <chid.h>

#pragma section(".devs", read)

__declspec(allocate(".devs$a")) struct device *__start_dtbloader_dev = NULL;
__declspec(allocate(".devs$d")) struct device *__stop_dtbloader_dev = NULL;

struct armada_device_match {
	struct device device;
	const char *android_compatible;
	const char *primary_panel;
	const char *secondary_panel;
};

#define ANDROID_DT_TABLE_MAGIC 0xd7b7ab1e

struct android_dt_table_header {
	fdt32_t magic;
	fdt32_t total_size;
	fdt32_t header_size;
	fdt32_t entry_size;
	fdt32_t entry_count;
	fdt32_t entries_offset;
	fdt32_t page_size;
	fdt32_t version;
};

struct android_dt_table_entry {
	fdt32_t dt_size;
	fdt32_t dt_offset;
	fdt32_t id;
	fdt32_t rev;
	fdt32_t custom[4];
};

static struct armada_device_match armada_devices[] = {
	{
		.device = {
			.name = L"Retroid Pocket Nova",
			.dtb = L"qcom\\qcs8550-retroidpocket-rpnova.dtb",
		},
		.android_compatible = "qcom,kalamap-hdk",
		.primary_panel = "il97680a amoled panel without DSC",
	},
	{
		.device = {
			.name = L"AYN Thor",
			.dtb = L"qcom\\qcs8550-ayn-thor.dtb",
		},
		.android_compatible = "qcom,kalamap-hdk",
		.primary_panel = "icna3520 amoled panel with DSC",
		.secondary_panel = "ch13726a video mode dsi boe panel with DSC",
	},
	{
		.device = {
			.name = L"AYN Odin 3",
			.dtb = L"qcom\\cq8725s-ayn-odin3.dtb",
		},
		.android_compatible = "qcom,sunp-hdk",
		.primary_panel = "icna3520 amoled panel with DSC",
	},
};

static bool panel_name_is(void *dtb, const char *label, const char *expected)
{
	const fdt32_t *panel_phandle;
	const char *display_label, *panel_name;
	int display, panel, len;

	display = -1;
	while ((display = fdt_node_offset_by_compatible(
			dtb, display, "qcom,dsi-display")) >= 0) {
		display_label = fdt_getprop(dtb, display, "label", &len);
		if (display_label && fdt_stringlist_contains(display_label, len, label))
			break;
	}

	if (display < 0)
		return false;

	panel_phandle = fdt_getprop(dtb, display, "qcom,dsi-default-panel", &len);
	if (!panel_phandle || len != sizeof(*panel_phandle))
		return false;

	panel = fdt_node_offset_by_phandle(dtb, fdt32_to_cpu(*panel_phandle));
	if (panel < 0)
		return false;

	panel_name = fdt_getprop(dtb, panel, "qcom,mdss-dsi-panel-name", &len);
	return panel_name && fdt_stringlist_contains(panel_name, len, expected);
}

static struct device *match_armada_dtb(void *android_dtb)
{
	struct device *match = NULL;
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(armada_devices); ++i) {
		struct armada_device_match *dev = &armada_devices[i];

		if (fdt_node_check_compatible(android_dtb, 0, dev->android_compatible))
			continue;

		if (!panel_name_is(android_dtb, "primary",
				   dev->primary_panel))
			continue;

		if (dev->secondary_panel &&
		    !panel_name_is(android_dtb, "secondary",
				   dev->secondary_panel))
			continue;

		if (match)
			return NULL;

		match = &dev->device;
	}

	return match;
}

static struct device *match_armada_dtbo(void *dtbo, UINTN len)
{
	struct android_dt_table_header *header = dtbo;
	struct device *match = NULL;
	UINT32 entries_offset, entry_count, entry_size;
	unsigned i;

	if (len < sizeof(*header) ||
	    fdt32_to_cpu(header->magic) != ANDROID_DT_TABLE_MAGIC ||
	    fdt32_to_cpu(header->total_size) > len ||
	    fdt32_to_cpu(header->header_size) < sizeof(*header))
		return NULL;

	entries_offset = fdt32_to_cpu(header->entries_offset);
	entry_count = fdt32_to_cpu(header->entry_count);
	entry_size = fdt32_to_cpu(header->entry_size);
	if (entry_size < sizeof(struct android_dt_table_entry) ||
	    entries_offset > len ||
	    entry_count > (len - entries_offset) / entry_size)
		return NULL;

	for (i = 0; i < entry_count; ++i) {
		struct android_dt_table_entry *entry;
		struct device *candidate;
		UINT32 dt_offset, dt_size;
		void *dtb;

		entry = (void *)((UINT8 *)dtbo + entries_offset + i * entry_size);
		dt_offset = fdt32_to_cpu(entry->dt_offset);
		dt_size = fdt32_to_cpu(entry->dt_size);
		if (dt_offset > len || dt_size > len - dt_offset)
			continue;

		dtb = (UINT8 *)dtbo + dt_offset;
		if (fdt_check_header(dtb) || fdt_totalsize(dtb) > dt_size)
			continue;

		candidate = match_armada_dtb(dtb);
		if (!candidate)
			continue;
		if (match && match != candidate)
			return NULL;

		match = candidate;
	}

	return match;
}

static struct device *match_armada_device(void)
{
	EFI_GUID dtb_table_guid = EFI_DTB_TABLE_GUID;
	struct device *match = NULL;
	void *android_dtb, *dtbo;
	EFI_STATUS status;
	UINTN dtbo_len;

	status = LibGetSystemConfigurationTable(&dtb_table_guid, &android_dtb);
	if (!EFI_ERROR(status) && !fdt_check_header(android_dtb)) {
		match = match_armada_dtb(android_dtb);
		if (match)
			return match;
	}

	status = qcom_read_active_dtbo(&dtbo, &dtbo_len);
	if (EFI_ERROR(status))
		return NULL;

	match = match_armada_dtbo(dtbo, dtbo_len);
	FreePool(dtbo);
	return match;
}


/**
 * match_device() - Detect the device.
 *
 * This function attempts to find the device structure for the
 * machine that dtbloader is running on.
 *
 * Returns: Pointer to the device structure or NULL on failure.
 */
struct device *match_device(void)
{
	EFI_STATUS status;
	struct device **dev;
	static struct device *cached_dev = NULL;
	EFI_GUID hwids[15] = {0};
	int priority[] = { /* From most to least specific. */
		3,  /* Manufacturer + Family + ProductName + ProductSku + BaseboardManufacturer + BaseboardProduct */
		6,  /* Manufacturer +                        ProductSku + BaseboardManufacturer + BaseboardProduct */
		8,  /* Manufacturer +          ProductName +              BaseboardManufacturer + BaseboardProduct */
		10, /* Manufacturer + Family +                            BaseboardManufacturer + BaseboardProduct */
		4,  /* Manufacturer + Family + ProductName + ProductSku */
		5,  /* Manufacturer + Family + ProductName */
		7,  /* Manufacturer +                        ProductSku */
		9,  /* Manufacturer +          ProductName */
		11, /* Manufacturer + Family */
	};
	int i, j;

	if (cached_dev)
		return cached_dev;

	status = populate_board_hwids(hwids);
	if (EFI_ERROR(status)) {
		cached_dev = match_armada_device();
		if (!cached_dev)
			Print(L"Failed to populate board hwids: %r\n", status);
		return cached_dev;
	}

	for (i = 0; i < ARRAY_SIZE(priority); ++i) {
		for (dev = &__start_dtbloader_dev + 1; dev < &__stop_dtbloader_dev; dev++) {
			for (j = 0; (*dev)->hwids[j].Data1; ++j) {
				if (!CompareGuid(&hwids[i], &(*dev)->hwids[j])) {
					if ((*dev)->extra_match && (*dev)->extra_match(*dev) != EFI_SUCCESS)
						continue;

					cached_dev = *dev;
					return *dev;
				}
			}
		}
	}

	cached_dev = match_armada_device();
	return cached_dev;
}

static bool dt_check_existing_mac_prop(void *dtb, int node, const char *prop)
{
	const uint8_t *val;
	int len, i;

	val = fdt_getprop(dtb, node, prop, &len);
	if (!val)
		return false;

	if (len != MAC_ADDR_SIZE)
		return false;

	/* Check if the prop is all zero as placeholder */
	for (i = 0; i < MAC_ADDR_SIZE; ++i)
		if (val[i])
			return true;

	return false;
}

/**
 * dt_update_mac() - Insert mac property if not present.
 * @dtb:	 DT blob.
 * @compatibles: Array of compatibles to add the mac to.
 * @num:	 Count of compatibles.
 * @prop:	 Name of the prop to add (i.e. "local-mac-address")
 * @mac:	 Raw MAC value to add.
 */
EFI_STATUS dt_update_mac(void *dtb, const char * const compatibles[], unsigned num,
			 const char *prop, UINT8 mac[MAC_ADDR_SIZE])
{
	unsigned i;

	for (i = 0; i < num; ++i) {
		int node, ret;

		node = fdt_node_offset_by_compatible(dtb, -1, compatibles[i]);
		if (node == -FDT_ERR_NOTFOUND)
			continue;

		if (dt_check_existing_mac_prop(dtb, node, prop))
			continue;

		ret = fdt_setprop(dtb, node, prop, mac, MAC_ADDR_SIZE);
		if (ret < 0) {
			return EFI_INVALID_PARAMETER;
		}
	}

	return EFI_SUCCESS;
}
