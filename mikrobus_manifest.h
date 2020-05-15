/*
 * mikroBUS manifest definition
 * based on Greybus Manifest
 *
 */

#ifndef __MIKROBUS_MANIFEST_H
#define __MIKROBUS_MANIFEST_H

#include <linux/bits.h>
#include <linux/types.h>
#include <linux/property.h>

#include "mikrobus_core.h"

#define MIKROBUS_VERSION_MAJOR 0
#define MIKROBUS_VERSION_MINOR 1

enum mikrobus_descriptor_type
{
	MIKROBUS_TYPE_INVALID = 0x00,
	MIKROBUS_TYPE_STRING = 0x01,
	MIKROBUS_TYPE_PROPERTY = 0x02,
	MIKROBUS_TYPE_DEVICE = 0x03,
};

struct mikrobus_descriptor_string
{
	__u8 length;
	__u8 id;
	__u8 string[0];
} __packed;

struct mikrobus_descriptor_property
{
	__u8 length;
	__u8 id;
	__u8 propname_stringid;
	__u8 type;
	__u8 value[0];
} __packed;

struct mikrobus_descriptor_device
{
	__u8 driver_stringid;
	__u8 num_properties;
	__u8 protocol;
	__u8 reg;
	__u8 cs_gpio;
	__u8 irq;
	__u8 prop_ids[0];
} __packed;

struct mikrobus_descriptor_header
{
	__le16 size;
	__u8 type;
	__u8 pad;
} __packed;

struct mikrobus_descriptor
{
	struct mikrobus_descriptor_header header;
	union {
		struct mikrobus_descriptor_string string;
		struct mikrobus_descriptor_device device;
		struct mikrobus_descriptor_property property;
	};
} __packed;

struct mikrobus_manifest_header
{
	__le16 size;
	__u8 version_major;
	__u8 version_minor;
	__u8 click_stringid;
	__u8 num_devices;
	__u8 rst_gpio_state;
	__u8 pwm_gpio_state;
	__u8 int_gpio_state;
	__u8 pad[3];
} __packed;

struct mikrobus_manifest
{
	struct mikrobus_manifest_header header;
	struct mikrobus_descriptor descriptors[0];
} __packed;

bool mikrobus_manifest_parse(struct click_board_info *info, void *data, size_t size);

#endif /* __MIKROBUS_MANIFEST_H */