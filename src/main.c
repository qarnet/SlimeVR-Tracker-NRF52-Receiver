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
#include <zephyr/drivers/gpio.h>
#include <nrf52840.h>

#include <zephyr/net/loopback.h>
#include <zephyr/logging/log.h>
#include "echo_server.h"

LOG_MODULE_REGISTER(foo, LOG_LEVEL_ERR);

#include "connectionManager.h"

#define BOOTLOADER_MAGIC_VALUE (0xf01669ef)

#define ADDR_LEN BT_ADDR_LE_STR_LEN

static void start_scan(void);
static bool stop_scan();

#define UUID_SLIME_VR_VAL BT_UUID_128_ENCODE(0x677abafc, 0x4bd7, 0xcfa8, 0x014e, 0xbb1444f02608)
#define UUID_SLIME_VR BT_UUID_DECLARE_128(UUID_SLIME_VR_VAL)
#define UUID_SLIME_VR_CHR_VAL BT_UUID_128_ENCODE(0x6fd1aa9d, 0xd1da, 0xca9f, 0x144b, 0x8118aaae7c9d)
#define UUID_SLIME_VR_CHR BT_UUID_DECLARE_128(UUID_SLIME_VR_CHR_VAL)

CONNECTION_MAP_INIT(connections, 6)

int current_connection_index = -1;

int slimevr_send(struct bt_conn *conn, const uint8_t *data, uint16_t length);

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

	if(cm_get_next_free_object_index(&connections) < 0)
	{
		printk("Too many devices connected\n");
		return;
	}

	bt_addr_le_to_str(device_info->recv_info->addr, addr_str, sizeof(addr_str));
	printk("Device found: %s (RSSI %d)\n", addr_str, device_info->recv_info->rssi);

	bt_data_parse(device_info->adv_data, ad_decode, NULL);

	if (stop_scan()) {
		return;
	}

	current_connection_index = cm_get_next_free_object_index(&connections);
	if(current_connection_index < 0)
	{
		return;
	}

	memcpy(connections.entry[current_connection_index].addr, addr_str, BT_ADDR_LE_STR_LEN); 
	err = bt_conn_le_create(device_info->recv_info->addr, BT_CONN_LE_CREATE_CONN,
				BT_LE_CONN_PARAM_DEFAULT, &connections.entry[current_connection_index].connection);
	if (err) {
		printk("Create conn to %s failed (%d)\n", addr_str, err);
		start_scan();
	}
}

void scan_filter_no_match(struct bt_scan_device_info *device_info,
				bool connectable)
{
	// for(int i = 0; i < device_info->adv_data->len; i++)
	// {
	// 	printk("%c", device_info->adv_data->data[i]);
	// }	

	// printk("\n");
}

BT_SCAN_CB_INIT(scan_cb, scan_filter_match, scan_filter_no_match, NULL, NULL);

bool already_scanning = false;

static void start_scan(void)
{
	int err;

	if(already_scanning)
	{
		return;
	}

	/* This demo doesn't require active scan */
	err = bt_le_scan_start(BT_LE_SCAN_ACTIVE, NULL);
	if (err) {
		printk("Scanning failed to start (err %d)\n", err);
		return;
	}

	printk("Scanning successfully started\n");

	already_scanning = true;
}

static bool stop_scan()
{
	bool err = bt_le_scan_stop();

	if(err)
	{
		return err;
	}

	already_scanning = false;

	return err;
}

uint64_t count_messages = 0;
int64_t timer = 0;

static uint8_t on_received(struct bt_conn *conn,
			struct bt_gatt_subscribe_params *params,
			const void *data, uint16_t length)
{
	int index = cm_get_index_with_conn(&connections, conn);
	connections.entry[index].debug_counter++;
	connections.entry[index].debug_data_counter += length;

	if(k_uptime_get() <= timer + 1000)
	{
		return BT_GATT_ITER_CONTINUE;
	}

	timer = k_uptime_get();
	printk("=======================================\n");
	for(int i = 0; i < connections.size; i++)
	{
		if(connections.entry[i].connection == NULL)
		{
			continue;
		}

		printk("Messages from (%s): %llu (%llu Bytes)\n", connections.entry[i].addr, connections.entry[i].debug_counter, connections.entry[i].debug_data_counter);
		connections.entry[i].debug_counter = 0;
		connections.entry[i].debug_data_counter = 0;
	}
	printk("=======================================\n");


	// uint8_t *data_ptr = (uint8_t *) data;
	// for(int i = 0; i < length; i++)
	// {
	// 	printk("%x", data_ptr[i]);
	// }
	// printk("\n");

	return BT_GATT_ITER_CONTINUE;
}

int slimevr_handles_get(struct bt_gatt_dm *dm)
{
	// char uuid_str[37];

	const struct bt_gatt_dm_attr *gatt_chrc_attr = 
		bt_gatt_dm_char_by_uuid(dm, UUID_SLIME_VR_CHR);
	const struct bt_gatt_chrc *gatt_chrc = 
		bt_gatt_dm_attr_chrc_val(gatt_chrc_attr);
	
	char addr[ADDR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(bt_gatt_dm_conn_get(dm)), addr, ADDR_LEN);
	int index = cm_get_index_with_addr(&connections, addr);

	connections.entry[index].write_params.handle = gatt_chrc->value_handle;

	// bt_uuid_to_str(gatt_chrc->uuid, uuid_str, sizeof(uuid_str));
	// printk("CHRC: %s\n", uuid_str);

	return 0;
}

struct bt_gatt_subscribe_params sub_params;

void on_subscribed(struct bt_conn *conn, uint8_t err,
					 struct bt_gatt_subscribe_params *params)
{
	char handshake_part[] = "Hey OVR =D 5";

	char handshake[sizeof(handshake_part) + 1];

	handshake[0] = 3;
	memcpy(handshake + 1, handshake_part, sizeof(handshake_part));

	slimevr_send(conn, (const uint8_t *)handshake, sizeof(handshake));
}

int slimevr_subscribe(struct bt_gatt_dm *dm)
{
	char addr[ADDR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(bt_gatt_dm_conn_get(dm)), addr, ADDR_LEN);
	int index = cm_get_index_with_addr(&connections, addr);
	
	connections.entry[index].sub_params.subscribe = on_subscribed;
	connections.entry[index].sub_params.notify = on_received;
	connections.entry[index].sub_params.value = BT_GATT_CCC_NOTIFY;

	const struct bt_gatt_dm_attr *gatt_chrc_attr = 
		bt_gatt_dm_char_by_uuid(dm, UUID_SLIME_VR_CHR);
	
	const struct bt_gatt_dm_attr *gatt_desc;

	gatt_desc = bt_gatt_dm_desc_by_uuid(dm, gatt_chrc_attr, UUID_SLIME_VR_CHR);

	connections.entry[index].sub_params.value_handle = gatt_desc->handle;

	gatt_desc = bt_gatt_dm_desc_by_uuid(dm, gatt_chrc_attr, BT_UUID_GATT_CCC);

	connections.entry[index].sub_params.ccc_handle = gatt_desc->handle;

	return bt_gatt_subscribe(bt_gatt_dm_conn_get(dm), &connections.entry[index].sub_params);
}

static void discover_all_completed(struct bt_gatt_dm *dm, void *ctx)
{
	char uuid_str[37];

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

void on_write(struct bt_conn *conn, uint8_t err,
				     struct bt_gatt_write_params *params)
{
	printk("Written\n");
	start_scan();
}

int slimevr_send(struct bt_conn *conn, const uint8_t *data, uint16_t length)
{
	int index = cm_get_index_with_conn(&connections, conn);

	connections.entry[index].write_params.func = on_write;
	connections.entry[index].write_params.offset = 0;
	connections.entry[index].write_params.data = data;
	connections.entry[index].write_params.length = length;

	return bt_gatt_write(conn, &connections.entry[index].write_params);
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
	.timeout = 10
};

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		printk("Failed to connect to %s (%u)\n", addr, err);

		bt_conn_unref(connections.entry[current_connection_index].connection);
		connections.entry[current_connection_index].connection = NULL;

		start_scan();
		return;
	}

	if (conn != connections.entry[current_connection_index].connection) {
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

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	int index = cm_get_index_with_addr(&connections, addr);

	if (conn != connections.entry[index].connection) {
		return;
	}

	printk("Disconnected: %s (reason 0x%02x)\n", addr, reason);

	bt_conn_unref(connections.entry[index].connection);
	connections.entry[index].connection = NULL;

	start_scan();
}

struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

struct bt_scan_init_param scan_init = {
		.connect_if_match = 0,
		.scan_param = BT_LE_SCAN_ACTIVE,
		.conn_param = BT_LE_CONN_PARAM_DEFAULT
	};


#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

#include <zephyr/net/net_if.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_context.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/dns_resolve.h>

#define DHCP_OPTION_NTP (42)

static uint8_t ntp_server[4];

static struct net_mgmt_event_callback mgmt_cb;

static struct net_dhcpv4_option_callback dhcp_cb;

static void start_dhcpv4_client(struct net_if *iface, void *user_data)
{
	ARG_UNUSED(user_data);

	// LOG_INF("Start on %s: index=%d", net_if_get_device(iface)->name,
	// 	net_if_get_by_iface(iface));
	net_dhcpv4_start(iface);
}

static void handler(struct net_mgmt_event_callback *cb,
		    uint32_t mgmt_event,
		    struct net_if *iface)
{
	int i = 0;

	if (mgmt_event != NET_EVENT_IPV4_ADDR_ADD) {
		return;
	}

	for (i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
		char buf[NET_IPV4_ADDR_LEN];

		if (iface->config.ip.ipv4->unicast[i].addr_type !=
							NET_ADDR_DHCP) {
			continue;
		}

		LOG_INF("   Address[%d]: %s", net_if_get_by_iface(iface),
			net_addr_ntop(AF_INET,
			    &iface->config.ip.ipv4->unicast[i].address.in_addr,
						  buf, sizeof(buf)));
		LOG_INF("    Subnet[%d]: %s", net_if_get_by_iface(iface),
			net_addr_ntop(AF_INET,
				       &iface->config.ip.ipv4->netmask,
				       buf, sizeof(buf)));
		LOG_INF("    Router[%d]: %s", net_if_get_by_iface(iface),
			net_addr_ntop(AF_INET,
						 &iface->config.ip.ipv4->gw,
						 buf, sizeof(buf)));
		LOG_INF("Lease time[%d]: %u seconds", net_if_get_by_iface(iface),
			iface->config.dhcpv4.lease_time);
	}
}

static void print_dhcpv4_addr(struct net_if *iface, struct net_if_addr *if_addr,
			      void *user_data)
{
	bool *found = (bool *)user_data;
	char hr_addr[NET_IPV4_ADDR_LEN];
	struct in_addr netmask;

	if (*found) {
		return;
	}

	if (if_addr->addr_type != NET_ADDR_DHCP) {
		return;
	}

	LOG_INF("IPv4 address: %s",
		net_addr_ntop(AF_INET, &if_addr->address.in_addr,
			      hr_addr, NET_IPV4_ADDR_LEN));
	LOG_INF("Lease time: %u seconds", iface->config.dhcpv4.lease_time);

	// netmask = net_if_ipv4_get_netmask_by_addr(iface,
	// 					  &if_addr->address.in_addr);
	// LOG_INF("Subnet: %s",
	// 	net_addr_ntop(AF_INET, &netmask, hr_addr, NET_IPV4_ADDR_LEN));
	// LOG_INF("Router: %s",
	// 	net_addr_ntop(AF_INET,
	// 		      &iface->config.ip.ipv4->gw,
	// 		      hr_addr, NET_IPV4_ADDR_LEN));

	*found = true;
}

static void option_handler(struct net_dhcpv4_option_callback *cb,
			   size_t length,
			   enum net_dhcpv4_msg_type msg_type,
			   struct net_if *iface)
{
	char buf[NET_IPV4_ADDR_LEN];

	LOG_INF("DHCP Option %d: %s", cb->option,
		net_addr_ntop(AF_INET, cb->data, buf, sizeof(buf)));
}

BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart),
	     "Console device is not ACM CDC UART device");

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

int main(void)
{
	int err;

	start_echo_server();

	if (!gpio_is_ready_dt(&led)) {
		// return 0;
	}

	err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (err < 0) {
		// return 0;
	}

	const struct device *const dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	uint32_t dtr = 0;

	if (usb_enable(NULL)) {
		return 0;
	}

	net_mgmt_init_event_callback(&mgmt_cb, handler,
				     NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&mgmt_cb);

	net_dhcpv4_init_option_callback(&dhcp_cb, option_handler,
					DHCP_OPTION_NTP, ntp_server,
					sizeof(ntp_server));

	net_dhcpv4_add_option_callback(&dhcp_cb);

	net_if_foreach(start_dhcpv4_client, NULL);

	gpio_pin_set_dt(&led, 1);
	

	while(true)
	{
		LOG_ERR("HI");
		k_msleep(500);
	}

	return 0;

	// Don't know why this is enabled, but it stops device from working sometimes
	/* Poll if the DTR flag was set */
	while (!dtr) {
		uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
		/* Give CPU resources to low priority threads. */
		k_sleep(K_MSEC(100));
	}

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	bt_conn_cb_register(&conn_callbacks);

	printk("Bluetooth initialized\n");
	k_msleep(1000);

	bt_scan_init(&scan_init);
	bt_scan_cb_register(&scan_cb);

	bt_scan_filter_add(BT_SCAN_FILTER_TYPE_UUID, UUID_SLIME_VR);

	bt_scan_filter_enable(BT_SCAN_UUID_FILTER, false);

	start_scan();

	// while(true)
	// {
	// 	printk("Test\r\n");
	// 	k_msleep(1000);
	// }
	

	return 0;
}
