#ifndef _MASTER_INFO_H_
#define _MASTER_INFO_H_
/* © Copyright 2012 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | master sharing information                                           |
 * +----------------------------------------------------------------------+
 * | Author: mi.jing@jiepang.com                                          |
 * +----------------------------------------------------------------------+
 * | Created: 2012-02-23 01:14                                            |
 * +----------------------------------------------------------------------+
 */

#define MAX_MASTER_CNT (8)
#define MAX_DATA_NODE_CNT  (1024)
#define MASTER_HEARTBEAT_INTERVAL (100) /* ms */
#define DATA_NODE_HEARTBEAT_INTERVAL (500) /* ms */
#define DATA_CHUNK_CNT (0xFFFF)
#define MAX_CHUNK_CNT  (4)

typedef uint16_t node_id_t;

typedef struct node_s
{
    struct in_addr  ip;
    node_id_t       id;
    in_port_t       port;
    bool            is_alive;
    char            avg_load;

    uint64_t        heartbeat_id;
    uint64_t        heartbeat_timeline_status; /* bit array */
    struct timeval  last_heartbeat_timestamp;
} node_t;

typedef struct master_node_s
{
    size_t cnt;
    node_t node_info[MAX_MASTER_CNT];
} master_node_t;

typedef struct data_node_s
{
    size_t cnt;
    node_t node_info[MAX_DATA_NODE_CNT];
} data_node_t;

typedef struct chunk_s
{
    node_id_t primary_replica_id;
    node_id_t secondary_replica_id[MAX_CHUNK_CNT - 1];
} chunk_t;

typedef struct partition_policy_s
{
    chunk_t chunk_distribution; /* map chunk id to host id */
} partition_policy_t;

struct master_share_info_s
{
    pthread_mutex_t lock;
};

#endif /* ! _MASTER_INFO_H_ */
/* vim: set expandtab tabstop=4 shiftwidth=4 ft=c foldmethod=marker: */
