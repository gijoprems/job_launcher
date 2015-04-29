#ifndef _COMLINK_H_
#define _COMLINK_H_

#include <netinet/in.h>

/*****************************************************************************/

#define MAX_CONNECTIONS (100)

/*****************************************************************************/
/* params for comlink */

typedef struct comlink_params_s {
    char *buffer;
    int buf_len;
    int rx_len;
    
    /* for the server sock initialization */
    unsigned int local_ip;
    unsigned short local_port;

    /* for client side */  
    unsigned int remote_ip;
    unsigned short remote_port;

    int init_done; /* To avoid multiple init of comlink */
    
    void (*receive_cb)(int fd, struct sockaddr_in *addr,
            unsigned int type, char *buf, int len);
    void (*shutdown_cb)(int fd);
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

    /* for select */
    int max_fds;
    fd_set rd_fds;
    fd_set t_fds;
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
/* header for comlink; needs further improvement to add serialization etc */

typedef struct comlink_header_s {
    unsigned int type;
    unsigned int len;
}comlink_header_t;

/*****************************************************************************/

int comlink_server_setup(comlink_params_t *cl_params);
int comlink_server_start(void);
int comlink_server_shutdown(void);

int comlink_client_setup(comlink_params_t *cl_params);
int comlink_client_start(void);
int comlink_client_shutdown(void);
int comlink_sendto_server(int con_index, comlink_header_t *header,
        char *buf, int buf_len);
void comlink_client_close(int fd);

int hostname_to_netaddr(char *hostname, struct sockaddr *addr);

#endif /* _COMLINK_H_ */
