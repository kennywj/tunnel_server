//
//  module: device link service
//      link.h
//      control the specified link of service
//
#ifndef TUNNEL_LINK
#define TUNNEL_LINK
//
// Link state
//
#define LINK_IDLE         0
#define LINK_CONNECTING   1
#define LINK_AUTH         2
#define LINK_SERVICE      3
#define LINK_CLOSEING     4

//
// structure of tunnel control block
//
typedef struct _tunn_link_
{
    struct list_head list; /*link list*/
    char    state;  // link state;
    char    id;     // link id;
    int     sock;   // local socket id
    fd_set  *rfds, *wfds;   // select fd;
    struct _queue_   *txq;   // recived data from peer, before write to socket
    
    
}TUNN_LINK;

extern int new_link_process(int sd, fd_set *rfds, fd_set *wfds, struct sockaddr_in *addr);

#endif
//TUNNEL_LINK