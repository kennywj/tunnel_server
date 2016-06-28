//
//  module: device tunnel module
//      tunnutil.h
//      active to builde up tunnel with specified server, multiplex/demutiplex traffice 
//      from server to local service or vice vera
//
#ifndef _TUNNUTIL_H_
#define _TUNNUTIL_H_
#include "list.h"

     
extern struct list_head tunnel_free_list, tunnel_active_list;
extern const char *TunnelStateMag[MAX_TUNNEL_STATE];

extern int remove_active_tunnel(TUNNEL_CTRL *tc);
extern TUNNEL_CTRL *get_free_tunnel_block(void);
extern void put_tunnel_block(struct list_head *head, TUNNEL_CTRL *p);
extern TUNNEL_CTRL *find_tunnel_block_by_sid(int sid);
extern TUNNEL_CTRL *find_tunnel_block_by_addr(char *ip, unsigned short port);
extern TUNNEL_CTRL *find_tunnel_block_by_uid(char *uid);
extern TUNNEL_CTRL *get_tunnel_block(TUNNEL_CTRL *next);
extern int get_tunnel_info(char *buf, int bufsize);

extern int fd_set_blocking(int fd, int blocking);
extern int read_from_sock(int sock, struct _queue_ *q);
extern void dump_frame(char *msg, char *frame, int len);
extern int get_free_port(void);
extern unsigned int get_time_milisec(void);
extern int p2p_log_msg(char *fmt, ...);
extern int clouds_service(char *url, char *cmd, char *resp, int bufsize);
extern int clouds_put_service(char *url, char *cmd, char *resp, int bufsize);
extern int get_server_ip(char *buf, int bufsize);

#endif
//_TUNNUTIL_H_