#ifndef _COMLINK_H_
#define _COMLINK_H_

#include <pthread.h>

/*****************************************************************************/

#define MAX_CONNECTIONS (100)

/*****************************************************************************/

/* params for comlink */
typedef struct comlink_params_s {
    char *buffer;
    int buf_len;

    unsigned int local_ip;
    unsigned short local_port;

    unsigned int remote_ip;
    unsigned short remote_port;

    void (*receive_cb)(int connection, char *buf, int len);
    void (*shutdown_cb)(void);
}comlink_params_t;

/*****************************************************************************/

/* params for server side */
typedef struct comlink_server_s {
    int skt_listen; /* listener socket */

    /* params for reply to client */
    int nr_clients;
    int skt_clients[MAX_CONNECTIONS]; /* connected clients for reply */
}comlink_server_t;

/*****************************************************************************/

/* params for client side */
typedef struct comlink_client_s {
    int nr_conns;
    int skt_conns[MAX_CONNECTIONS]; /* connections */
}comlink_client_t;

/*****************************************************************************/

/* comlink context */
typedef struct comlink_s {
    /* params for the comlink main task */
    int comlink_break;

    comlink_params_t params;
    comlink_server_t server;
    comlink_client_t client;
}comlink_t;

/*****************************************************************************/

#endif /* _COMLINK_H_ */
