#include "connectionManagement.h"

// int cm_get_next_free_object_index(connection_map *cm)
// {
//     for(int i = 0; i < cm->size; i++)
//     {
//         if(cm->entry[i].connection == NULL)
//         {
//             return i;
//         }
//     }

//     return -1;
// }

// int cm_remove_object_with_index(connection_map *cm, int index)
// {
//     if(cm->size < index)
//     {
//         return -1;
//     }

//     cm->entry[index].connection = NULL;

//     return 0;
// }

// int cm_remove_object_with_addr(connection_map *cm, char addr[BT_ADDR_STR_LEN])
// {
//     for(int i = 0; i < cm->size; i++)
//     {
//         if(strcmp(addr, cm->entry[i].addr) == 0)
//         {
//             cm->entry[i].connection = NULL;

//             return 0;
//         }
//     }

//     return -1;
// }

// int cm_add_object(connection_map *cm, connection_entry entry)
// {
//     int index = cm_get_next_free_object_index(cm);

//     if(index < 0)
//     {
//         return -1;
//     }

//     memcpy(&cm->entry[index], &entry, sizeof(connection_entry));

//     return 0;
// }