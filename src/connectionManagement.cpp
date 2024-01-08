#include "connectionManagement.hpp"

#include <zephyr/bluetooth/bluetooth.h>
#include <vector>
#include <map>
#include <string>

std::map<std::string, struct bt_conn *> connections;
std::map<std::string, struct bt_conn *>::iterator iterator;

int cm_add_entry(char addr[BT_ADDR_STR_LEN], struct bt_conn *conn)
{
    connections.emplace(std::string(addr), conn);

    return 0;
}

int cm_delete_entry(char addr[BT_ADDR_LE_STR_LEN])
{
    iterator = connections.find(std::string(addr));

    connections.erase(iterator);

    return 0;
}

struct bt_conn *cm_get_conn(char addr[BT_ADDR_LE_STR_LEN])
{
    return connections[std::string(addr)];
}