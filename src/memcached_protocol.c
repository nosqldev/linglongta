/* © Copyright 2011 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | parse memcached protocol                                             |
 * +----------------------------------------------------------------------+
 * | Author: mi.jing@jiepang.com                                          |
 * +----------------------------------------------------------------------+
 * | Created: 2011-12-02 18:25                                            |
 * +----------------------------------------------------------------------+
 */

#include "memcached_protocol.h"
#include "global.h"

enum parse_state_s
{
    s_ready,
    s_get,
    s_set,
    s_delete,
    s_sync,
    s_power,
    s_show, /* TODO show status */
    s_done,
    s_fatal,
} parse_state_t;

enum parse_set_state_s
{
    s_set_key,
    s_set_flag,
    s_set_exptime,
    s_set_bytes,
    s_set_done,
    s_set_failed,
};

/* TODO ignore all the tailing spaces in ScanKey */
#define ScanKey(ptr, result) do {                   \
    while (**ptr == ' ') ++*ptr;                    \
    size_t len = strlen(*ptr);                      \
    if (*((*ptr)+len-1) == '\n' && *((*ptr)+len-2) == '\r') { \
        if (*((*ptr)+len-3) == ' ') {               \
            *((*ptr)+len-3) = '\0';                 \
        } else {                                    \
            *((*ptr)+len-2) = '\0';                 \
        }                                           \
    } else { return false; }                        \
    strncpy(result, *ptr, sizeof(result));          \
    result[ sizeof(result) - 1 ] = '\0';            \
} while (0)

STATIC_MODIFIER bool
parse_power_cmd(char **saveptr, struct query_request_s *qr)
{
    /* Accept command format(\s means space):
     * power halt\s?\r\n
     */
    ScanKey(saveptr, qr->key);
    if (strcasecmp(qr->key, "halt") == 0)
        return true;
    else
        return false;
}

STATIC_MODIFIER bool
parse_sync_cmd(char **saveptr, struct query_request_s *qr)
{
    /* Accept command format:
     * sync index\s?\r\n
     */
    ScanKey(saveptr, qr->key);
    if (strcasecmp(qr->key, "index") == 0)
        return true;
    else if (strcasecmp(qr->key, "all") == 0)
        return true;
    else
        return false;
}

STATIC_MODIFIER bool
parse_get_cmd(char **saveptr, struct query_request_s *qr)
{
    /* Accept get command format:
     * get\s+$KeyName\s?\r\n
     */
    ScanKey(saveptr, qr->key);

    return true;
}

STATIC_MODIFIER bool
parse_delete_cmd(char **saveptr, struct query_request_s *qr)
{
    ScanKey(saveptr, qr->key);

    return true;
}

STATIC_MODIFIER bool
parse_set_cmd(char **saveptr, struct query_request_s *qr)
{
    /* TODO check flag, exptime here */
    bool retval;
    enum parse_set_state_s state;
    char *token;
    size_t len;

    state = s_set_key;
    retval = true;

    len = strlen(*saveptr);
    if (*((*saveptr)+len-1) == '\n' && *((*saveptr)+len-2) == '\r')
        *((*saveptr)+len-2) = '\0';
    else
        return false;

    for ( ; state != s_set_done; )
    {
        switch (state)
        {
        case s_set_key:
            token = strtok_r(NULL, " ", saveptr);
            if (token == NULL)
                state = s_set_failed;
            else
            {
                if (strlen(token) >= MAX_KEY_SIZE)
                {
                    state = s_set_failed;
                }
                else
                {
                    strncpy(qr->key, token, sizeof(qr->key));
                    state = s_set_flag;
                }
            }
            break;

        case s_set_flag:
            token = strtok_r(NULL, " ", saveptr);
            if (token == NULL)
                state = s_set_failed;
            else
            {
                qr->flag = atoi(token);
                state = s_set_exptime;
            }
            break;

        case s_set_exptime:
            token = strtok_r(NULL, " ", saveptr);
            if (token == NULL)
                state = s_set_failed;
            else
            {
                qr->exptime = atoi(token);
                state = s_set_bytes;
            }
            break;

        case s_set_bytes:
            token = strtok_r(NULL, " ", saveptr);
            if (token == NULL)
                state = s_set_failed;
            else
            {
                qr->data_size = (size_t)atol(token);
                state = s_set_done;
            }
            break;

        case s_set_failed:
            state = s_set_done;
            retval = false;
            break;

        default:
            state = s_set_failed;
            break;
        }
    }

    return retval;
}

int
parse_memcached_string_head(struct query_request_s *qr)
{
    int retval;
    char query_head_buf[MAX_QUERY_HEADER_SIZE] = {0};
    char *token;
    char *saveptr;
    enum parse_state_s state;

    retval = RET_SUCCESS;
    qr->cmd_type = cmd_unknown;
    state = s_ready;
    memmove(query_head_buf, qr->query_head_buf, sizeof query_head_buf);
    query_head_buf[sizeof query_head_buf - 1] = '\0';
    token = strtok_r(query_head_buf, " ", &saveptr);

    if (token == NULL)
    {
        retval = RET_PARSE_REQ_FAILED;
        qr->parse_status = ps_failed;
        goto outlet;
    }

#define HandleCmd(type) case s_##type:      \
    if (parse_##type##_cmd(&saveptr, qr)) { \
        qr->cmd_type = cmd_##type;          \
        qr->parse_status = ps_succ;         \
        state = s_done;                     \
    } else {                                \
        qr->cmd_type = cmd_##type;          \
        qr->parse_status = ps_failed;       \
        state = s_fatal;                    \
    }; \
    break

    for ( ; state != s_done; )
    {
        switch (state)
        {
        case s_ready:
            if (strcasecmp(token, "get") == 0)
            {
                state = s_get;
            }
            else if (strcasecmp(token, "set") == 0)
            {
                state = s_set;
            }
            else if (strcasecmp(token, "delete") == 0)
            {
                state = s_delete;
            }
            else if (strcasecmp(token, "sync") == 0)
            {
                state = s_sync;
            }
            else if (strcasecmp(token, "power") == 0)
            {
                state = s_power;
            }
            else
            {
                /* qr->cmd_type should be cmd_unknown here */
                state = s_fatal;
            }
            break;

        HandleCmd(get);
        HandleCmd(set);
        HandleCmd(delete);
        HandleCmd(sync);
        HandleCmd(power);

        case s_fatal:
            state = s_done;
            retval = RET_PARSE_REQ_FAILED;
            qr->parse_status = ps_failed;
            break;

        default:
            /* should never reach here */
            state = s_fatal;
            break;
        }
    }

outlet:
    return retval;
}

char *
try_parse_memcached_string_head(struct query_request_s *qr)
{
    return strstr(qr->query_head_buf, MC_TERM_STR);
}

char *
cmd_type_name(enum cmd_type_e cmd_type)
{
    switch (cmd_type)
    {
    case cmd_unknown:
        return "unknow cmd_type";

    case cmd_set:
        return "SET";

    case cmd_get:
        return "GET";

    case cmd_delete:
        return "DELETE";

    case cmd_sync:
        return "SYNC";

    case cmd_power:
        return "POWER";

    default:
        return "unexpected cmd_type";
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
