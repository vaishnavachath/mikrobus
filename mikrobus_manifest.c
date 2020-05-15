/*
 * mikroBUS manifest parsing
 * based on Greybus Manifest Parsing logic
 *
 */
#define pr_fmt(fmt) "mikrobus_manifest: " fmt

#include <linux/bits.h>
#include <linux/types.h>
#include <linux/property.h>

#include "mikrobus_manifest.h"

struct manifest_desc
{
	struct list_head links;
	size_t size;
	void *data;
	enum mikrobus_descriptor_type type;
};

static void release_manifest_descriptor(struct manifest_desc *descriptor)
{
	list_del(&descriptor->links);
	kfree(descriptor);
}

static void release_manifest_descriptors(struct click_board_info *info)
{
	struct manifest_desc *descriptor;
	struct manifest_desc *next;

	list_for_each_entry_safe(descriptor, next, &info->manifest_descs, links)
		release_manifest_descriptor(descriptor);
}

static int identify_descriptor(struct click_board_info *info,
							   struct mikrobus_descriptor *desc, size_t size)
{
	struct mikrobus_descriptor_header *desc_header = &desc->header;
	struct manifest_desc *descriptor;
	size_t desc_size;
	size_t expected_size;

	if (size < sizeof(*desc_header))
	{
		return -EINVAL;
	}

	desc_size = le16_to_cpu(desc_header->size);
	if (desc_size > size)
	{
		return -EINVAL;
	}

	expected_size = sizeof(*desc_header);

	switch (desc_header->type)
	{

	case MIKROBUS_TYPE_STRING:
		expected_size += sizeof(struct mikrobus_descriptor_string);
		expected_size += desc->string.length;
		expected_size = ALIGN(expected_size, 4);
		break;
	case MIKROBUS_TYPE_PROPERTY:
		expected_size += sizeof(struct mikrobus_descriptor_property);
		expected_size += desc->property.length;
		expected_size = ALIGN(expected_size, 4);
		break;
	case MIKROBUS_TYPE_DEVICE:
		expected_size += sizeof(struct mikrobus_descriptor_device);
		expected_size += desc->device.num_properties;
		expected_size = ALIGN(expected_size, 4);
		break;
	case MIKROBUS_TYPE_INVALID:
	default:
		return -EINVAL;
	}

	descriptor = kzalloc(sizeof(*descriptor), GFP_KERNEL);
	if (!descriptor)
		return -ENOMEM;

	descriptor->size = desc_size;
	descriptor->data = (char *)desc + sizeof(*desc_header);
	descriptor->type = desc_header->type;
	list_add_tail(&descriptor->links, &info->manifest_descs);

	return desc_size;
}

static char *mikrobus_string_get(struct click_board_info *info, u8 string_id)
{
	struct mikrobus_descriptor_string *desc_string;
	struct manifest_desc *descriptor;
	bool found = false;
	char *string;

	if (!string_id)
		return NULL;

	list_for_each_entry(descriptor, &info->manifest_descs, links)
	{
		if (descriptor->type != MIKROBUS_TYPE_STRING)
			continue;

		desc_string = descriptor->data;
		if (desc_string->id == string_id)
		{
			found = true;
			break;
		}
	}
	if (!found)
		return ERR_PTR(-ENOENT);

	string = kmemdup(&desc_string->string, desc_string->length + 1,
					 GFP_KERNEL);
	if (!string)
		return ERR_PTR(-ENOMEM);
	string[desc_string->length] = '\0';

	release_manifest_descriptor(descriptor);

	return string;
}

static struct property_entry *mikrobus_property_get(struct click_board_info *info, u8 prop_id)
{
	struct mikrobus_descriptor_property *desc_property;
	struct manifest_desc *descriptor;
	struct property_entry prop;
	struct property_entry *prop_ptr;
	char *prop_name;
	unsigned short el_count;
	bool found = false;
	u8 *val_u8;
	u16 *val_u16;
	u32 *val_u32;
	u64 *val_u64;

	if (!prop_id)
		return NULL;

	list_for_each_entry(descriptor, &info->manifest_descs, links)
	{
		if (descriptor->type != MIKROBUS_TYPE_PROPERTY)
			continue;

		desc_property = descriptor->data;
		if (desc_property->id == prop_id)
		{
			found = true;
			break;
		}
	}
	if (!found)
		return ERR_PTR(-ENOENT);

	prop_name = mikrobus_string_get(info, desc_property->propname_stringid);

	switch (desc_property->type)
	{

	case MIKROBUS_PROPERTY_TYPE_U8:
		el_count = desc_property->length / sizeof(u8);
		val_u8 = kmemdup(&desc_property->value, desc_property->length, GFP_KERNEL);
		if (el_count < 2)
			prop = PROPERTY_ENTRY_U8(prop_name, *val_u8);
		else
			prop = PROPERTY_ENTRY_U8_ARRAY_LEN(prop_name, (void *)desc_property->value, el_count);
		break;
	case MIKROBUS_PROPERTY_TYPE_U16:
		el_count = desc_property->length / sizeof(u16);
		val_u16 = kmemdup(&desc_property->value, desc_property->length, GFP_KERNEL);
		if (el_count < 2)
			prop = PROPERTY_ENTRY_U8(prop_name, *val_u16);
		else
			prop = PROPERTY_ENTRY_U8_ARRAY_LEN(prop_name, (void *)desc_property->value, el_count);
		break;
	case MIKROBUS_PROPERTY_TYPE_U32:
		el_count = desc_property->length / sizeof(u32);
		val_u32 = kmemdup(&desc_property->value, desc_property->length, GFP_KERNEL);
		if (el_count < 2)
			prop = PROPERTY_ENTRY_U8(prop_name, *val_u32);
		else
			prop = PROPERTY_ENTRY_U8_ARRAY_LEN(prop_name, (void *)desc_property->value, el_count);
		break;
	case MIKROBUS_PROPERTY_TYPE_U64:
		el_count = desc_property->length / sizeof(u64);
		val_u64 = kmemdup(&desc_property->value, desc_property->length, GFP_KERNEL);
		if (el_count < 2)
			prop = PROPERTY_ENTRY_U8(prop_name, *val_u64);
		else
			prop = PROPERTY_ENTRY_U8_ARRAY_LEN(prop_name, (void *)desc_property->value, el_count);
		break;
	default:
		return ERR_PTR(-EINVAL);
	}

	prop_ptr = kmemdup(&prop, sizeof(prop), GFP_KERNEL);

	release_manifest_descriptor(descriptor);

	return prop_ptr;
}

static int mikrobus_manifest_parse_devices(struct click_board_info *info)
{
	struct mikrobus_descriptor_device *desc_device;
	struct manifest_desc *desc, *next;
	int i;

	//dev = kzalloc(sizeof(*port), GFP_KERNEL);

	if (WARN_ON(!list_empty(&info->devices)))
		return false;

	list_for_each_entry_safe(desc, next, &info->manifest_descs, links)
	{
		if (desc->type != MIKROBUS_TYPE_DEVICE)
			continue;
		desc_device = desc->data;

		pr_info("device name is : %s \n", mikrobus_string_get(info, desc_device->driver_stringid));
		pr_info("device protocol is : %d \n", desc_device->protocol);
		pr_info("device address is : %d \n", desc_device->reg);
		pr_info("device cs_gpio is : %d \n", desc_device->cs_gpio);
		pr_info("device irq is : %d \n", desc_device->irq);
		pr_info("device protocol is : %d \n", desc_device->num_properties);

		for (; i < desc_device->num_properties; ++i)
		{
			pr_info("device property %d : id=%d \n", i, desc_device->prop_ids[i]);
		}
	}

	return 0; /* Error; count should also be 0 */
}

bool mikrobus_manifest_parse(struct click_board_info *info, void *data, size_t size)
{
	struct mikrobus_manifest *manifest;
	struct mikrobus_manifest_header *header;
	struct mikrobus_descriptor *desc;
	u16 manifest_size;
	bool result;

	if (WARN_ON(!list_empty(&info->manifest_descs)))
		return false;

	if (size < sizeof(*header))
		return false;

	manifest = data;
	header = &manifest->header;
	manifest_size = le16_to_cpu(header->size);

	pr_info("manifest size: %d \n", manifest_size);
	pr_info("manifest version major : %d \n", header->version_major);
	pr_info("click string id : %d \n", header->click_stringid);
	pr_info("num_devices : %d \n", header->num_devices);
	pr_info("rst_gpio_state : %d \n", header->rst_gpio_state);
	pr_info("pwm_gpio_state : %d \n", header->pwm_gpio_state);
	pr_info("int_gpio_state : %d \n", header->int_gpio_state);

	if (manifest_size != size)
		return false;

	if (header->version_major > MIKROBUS_VERSION_MAJOR)
	{
		return false;
	}

	desc = manifest->descriptors;
	size -= sizeof(*header);
	while (size)
	{
		int desc_size;

		desc_size = identify_descriptor(info, desc, size);
		if (desc_size < 0)
		{
			result = false;
		}
		desc = (struct mikrobus_descriptor *)((char *)desc + desc_size);
		size -= desc_size;
	}

	info->name = mikrobus_string_get(info, header->click_stringid);
	info->num_devices = header->num_devices;
	info->rst_gpio_state = header->rst_gpio_state;
	info->pwm_gpio_state = header->pwm_gpio_state;
	info->int_gpio_state = header->int_gpio_state;

	pr_info("click name : %s \n", info->name);

	result = mikrobus_manifest_parse_devices(info);

	release_manifest_descriptors(info);

	return result;
}
EXPORT_SYMBOL_GPL(mikrobus_manifest_parse);