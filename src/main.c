/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <bluetooth/scan.h>
#include <bluetooth/gatt_dm.h>
#include <zephyr/sys/byteorder.h>

static void start_scan(void);

#define UUID_SLIME_VR_VAL BT_UUID_128_ENCODE(0x677abafc, 0x4bd7, 0xcfa8, 0x014e, 0xbb1444f02608)
#define UUID_SLIME_VR BT_UUID_DECLARE_128(UUID_SLIME_VR_VAL)
#define UUID_SLIME_VR_CHR_VAL BT_UUID_128_ENCODE(0x6fd1aa9d, 0xd1da, 0xca9f, 0x144b, 0x8118aaae7c9d)
#define UUID_SLIME_VR_CHR BT_UUID_DECLARE_128(UUID_SLIME_VR_CHR_VAL)

static struct bt_conn *default_conn;

int slimevr_send(const uint8_t *data, uint16_t length);

bool ad_decode(struct bt_data *data, void *user_data)
{
	switch(data->type)
	{
		case BT_DATA_UUID128_ALL:
			for(int i = 0; i < data->data_len; i++)
			{
				printk("%x", data->data[i]);
			}
			
			return true;
		case BT_DATA_NAME_COMPLETE:
			printk("%s\n", data->data);
			
			return true;
		default: 
	}

	return false;
}

void scan_filter_match(struct bt_scan_device_info *device_info,
			     struct bt_scan_filter_match *filter_match,
			     bool connectable)
{
	char addr_str[BT_ADDR_LE_STR_LEN];
	int err;

	if (default_conn) {
		return;
	}

	bt_addr_le_to_str(device_info->recv_info->addr, addr_str, sizeof(addr_str));
	printk("Device found: %s (RSSI %d)\n", addr_str, device_info->recv_info->rssi);

	bt_data_parse(device_info->adv_data, ad_decode, NULL);

	if (bt_le_scan_stop()) {
		return;
	}

	err = bt_conn_le_create(device_info->recv_info->addr, BT_CONN_LE_CREATE_CONN,
				BT_LE_CONN_PARAM_DEFAULT, &default_conn);
	if (err) {
		printk("Create conn to %s failed (%d)\n", addr_str, err);
		start_scan();
	}
}

BT_SCAN_CB_INIT(scan_cb, scan_filter_match, NULL, NULL, NULL);

static void start_scan(void)
{
	int err;

	struct bt_scan_init_param scan_init = {
		.connect_if_match = 0,
		.scan_param = BT_LE_SCAN_ACTIVE,
		.conn_param = BT_LE_CONN_PARAM_DEFAULT
	};

	bt_scan_init(&scan_init);
	bt_scan_cb_register(&scan_cb);

	bt_scan_filter_add(BT_SCAN_FILTER_TYPE_UUID, UUID_SLIME_VR);

	bt_scan_filter_enable(BT_SCAN_UUID_FILTER, false);

	/* This demo doesn't require active scan */
	err = bt_le_scan_start(BT_LE_SCAN_ACTIVE, NULL);
	if (err) {
		printk("Scanning failed to start (err %d)\n", err);
		return;
	}

	printk("Scanning successfully started\n");
}

uint64_t count_messages = 0;
int64_t timer = 0;

static uint8_t on_received(struct bt_conn *conn,
			struct bt_gatt_subscribe_params *params,
			const void *data, uint16_t length)
{
	count_messages++;

	if(k_uptime_get() <= timer + 1000)
	{
		return BT_GATT_ITER_CONTINUE;
	}

	timer = k_uptime_get();
	printk("Messages: %lli\n", count_messages);
	printk("Current message length: %u\n", length);
	count_messages = 0;

	// uint8_t *data_ptr = (uint8_t *) data;
	// for(int i = 0; i < length; i++)
	// {
	// 	printk("%x", data_ptr[i]);
	// }
	// printk("\n");

	return BT_GATT_ITER_CONTINUE;
}

uint16_t write_handle;

int slimevr_handles_get(struct bt_gatt_dm *dm)
{
	char uuid_str[37];

	const struct bt_gatt_dm_attr *gatt_chrc_attr = 
		bt_gatt_dm_char_by_uuid(dm, UUID_SLIME_VR_CHR);
	const struct bt_gatt_chrc *gatt_chrc = 
		bt_gatt_dm_attr_chrc_val(gatt_chrc_attr);
	
	write_handle = gatt_chrc->value_handle;

	bt_uuid_to_str(gatt_chrc->uuid, uuid_str, sizeof(uuid_str));
	printk("CHRC: %s\n", uuid_str);

	return 0;
}

struct bt_gatt_subscribe_params sub_params;
volatile bool subscribed = false;

void on_subscribed(struct bt_conn *conn, uint8_t err,
					 struct bt_gatt_subscribe_params *params)
{
	subscribed = true;

	char handshake_part[] = "Hey OVR =D 5";

	char handshake[sizeof(handshake_part) + 1];

	handshake[0] = 3;
	memcpy(handshake + 1, handshake_part, sizeof(handshake_part));

	slimevr_send(handshake, sizeof(handshake));
}

int slimevr_subscribe(struct bt_gatt_dm *dm)
{
	sub_params.subscribe = on_subscribed;
	sub_params.notify = on_received;
	sub_params.value = BT_GATT_CCC_NOTIFY;

	const struct bt_gatt_dm_attr *gatt_chrc_attr = 
		bt_gatt_dm_char_by_uuid(dm, UUID_SLIME_VR_CHR);
	
	const struct bt_gatt_dm_attr *gatt_desc;

	gatt_desc = bt_gatt_dm_desc_by_uuid(dm, gatt_chrc_attr, UUID_SLIME_VR_CHR);

	sub_params.value_handle = gatt_desc->handle;

	gatt_desc = bt_gatt_dm_desc_by_uuid(dm, gatt_chrc_attr, BT_UUID_GATT_CCC);

	sub_params.ccc_handle = gatt_desc->handle;

	return bt_gatt_subscribe(bt_gatt_dm_conn_get(dm), &sub_params);
}

static void discover_all_completed(struct bt_gatt_dm *dm, void *ctx)
{
	char uuid_str[37];
	struct bt_conn *conn = bt_gatt_dm_conn_get(dm);

	const struct bt_gatt_dm_attr *gatt_service_attr =
			bt_gatt_dm_service_get(dm);
	const struct bt_gatt_service_val *gatt_service =
			bt_gatt_dm_attr_service_val(gatt_service_attr);

	size_t attr_count = bt_gatt_dm_attr_cnt(dm);

	bt_uuid_to_str(gatt_service->uuid, uuid_str, sizeof(uuid_str));
	printk("Found service %s\n", uuid_str);
	printk("Attribute count: %d\n", attr_count);

	slimevr_handles_get(dm);
	slimevr_subscribe(dm);
	// bt_gatt_dm_data_print(dm);
	bt_gatt_dm_data_release(dm);
	bt_gatt_dm_continue(dm, NULL);
}

struct bt_gatt_write_params write_params;
void on_write(struct bt_conn *conn, uint8_t err,
				     struct bt_gatt_write_params *params)
{
	printk("Written\n");
}

int slimevr_send(const uint8_t *data, uint16_t length)
{
	write_params.func = on_write;
	write_params.handle = write_handle;
	write_params.offset = 0;
	write_params.data = data;
	write_params.length = length;

	return bt_gatt_write(default_conn, &write_params);
}

static void discover_all_service_not_found(struct bt_conn *conn, void *ctx)
{
	printk("No more services\n");
}

static void discover_all_error_found(struct bt_conn *conn, int err, void *ctx)
{
	printk("The discovery procedure failed, err %d\n", err);
}

static struct bt_gatt_dm_cb discover_all_cb = {
	.completed = discover_all_completed,
	.service_not_found = discover_all_service_not_found,
	.error_found = discover_all_error_found,
};

void mtu_exchange_func(struct bt_conn *conn, uint8_t err,
		     struct bt_gatt_exchange_params *params)
{
	printk("MTU Exchanged: %d\n", err);

	printk("MTU: %u\n", bt_gatt_get_mtu(conn));
}

struct bt_gatt_exchange_params exchange_params = {
		.func = mtu_exchange_func
};

struct bt_le_conn_param conn_param = {
	.interval_max = 6,
	.interval_min = 6,
	.latency = 0,
	.timeout = 200
};

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		printk("Failed to connect to %s (%u)\n", addr, err);

		bt_conn_unref(default_conn);
		default_conn = NULL;

		start_scan();
		return;
	}

	if (conn != default_conn) {
		return;
	}

	printk("Connected: %s\n", addr);

	bt_gatt_exchange_mtu(conn, &exchange_params);

	printk("Updated phy?: %d\n", bt_conn_le_phy_update(conn, BT_CONN_LE_PHY_PARAM_2M));

	printk("Updated params?: %d\n", bt_conn_le_param_update(conn, &conn_param));

	printk("Updated len?: %d\n", bt_conn_le_data_len_update(conn, BT_LE_DATA_LEN_PARAM_MAX));

	err = bt_gatt_dm_start(conn, UUID_SLIME_VR, &discover_all_cb, NULL);
	if (err) {
		printk("Failed to start discovery (err %d)\n", err);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (conn != default_conn) {
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Disconnected: %s (reason 0x%02x)\n", addr, reason);

	bt_conn_unref(default_conn);
	default_conn = NULL;

	start_scan();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

int main(void)
{
	int err;

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	printk("Bluetooth initialized\n");

	start_scan();

	// while(true)
	// {
	// 	if(subscribed == true);
	// }
	return 0;
}
