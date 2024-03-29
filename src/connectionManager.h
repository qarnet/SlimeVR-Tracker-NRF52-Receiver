#ifndef CONNECTION_MANAGER_H_
#define CONNECTION_MANAGER_H_

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>

typedef struct {
    char addr[BT_ADDR_LE_STR_LEN];
	struct bt_conn *connection;
    struct bt_gatt_subscribe_params sub_params;
    struct bt_gatt_write_params write_params;
    uint64_t debug_counter;
    uint64_t debug_data_counter;
} connection_entry;

typedef struct {
	connection_entry *entry;
    int size;
} connection_map;

#define CONNECTION_MAP_INIT(name, amount) \
    connection_entry name##entry[amount] = {0} ; \
    connection_map name = { \
        .entry = name##entry, \
        .size = amount, \
    };

int cm_get_next_free_object_index(connection_map *cm);
int cm_remove_object_with_index(connection_map *cm, int index);
int cm_remove_object_with_addr(connection_map *cm, char addr[BT_ADDR_LE_STR_LEN]);
int cm_add_object(connection_map *cm, connection_entry entry);
int cm_get_index_with_addr(connection_map *cm, char addr[BT_ADDR_LE_STR_LEN]);
int cm_get_index_with_conn(connection_map *cm, struct bt_conn *conn);

#endif