#ifndef CONNECTION_MANAGER_H_
#define CONNECTION_MANAGER_H_

#include <zephyr/bluetooth/bluetooth.h>

typedef struct {
    char addr[BT_ADDR_STR_LEN];
	struct bt_conn *connection;
} connection_entry;

typedef struct {
	connection_entry *entry;
    int size;
} connection_map;

#define CONNECTION_MAP_INIT(name, size) \
    connection_map name[size]; \
    name.size = size;

int cm_get_next_free_object_index(connection_map *cm);
int cm_remove_object_with_index(connection_map *cm, int index);
int cm_remove_object_with_addr(connection_map *cm, char addr[BT_ADDR_STR_LEN]);
int cm_add_object(connection_map *cm, connection_entry entry);

#endif