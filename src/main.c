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
#include <zephyr/sys/byteorder.h>

static void start_scan(void);

#define UUID_SLIME_VR BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0x677abafc, 0x4bd7, 0xcfa8, 0x014e, 0xbb1444f02608))
#define UUID_SLIME_VR_CHR BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0x6fd1aa9d, 0xd1da, 0xca9f, 0x144b, 0x8118aaae7c9d));

static struct bt_conn *default_conn;

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

	return;

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

	bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
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
	return 0;
}
