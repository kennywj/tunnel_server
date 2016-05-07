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

extern TUNNEL_CTRL *get_tunnel_block(void);
extern void free_tunnel_block(TUNNEL_CTRL *p);
extern TUNNEL_CTRL *find_tunnel_block_by_sid(int sid);
extern TUNNEL_CTRL *find_tunnel_block_by_addr(char *ip, unsigned short port);
extern TUNNEL_CTRL *find_tunnel_block_by_uid(char *uid);

extern int fd_set_blocking(int fd, int blocking);
extern int read_from_sock(int sock, struct _queue_ *q);


#endif
//_TUNNUTIL_H_