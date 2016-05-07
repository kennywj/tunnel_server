//
//  module: device tunnel daemon
//      tunnel.h
//      active to builde up tunnel with specified server, multiplex/demutiplex traffice 
//      from server to local service or vice vera
//
#ifndef _TUNNEL_
#define _TUNNEL_
#include "list.h"
#include "queue.h"
//
// Tunnel state
//
#define TUNNEL_IDLE         0
#define TUNNEL_CONNECTING   1
#define TUNNEL_AUTH         2
#define TUNNEL_SERVICE      3
#define TUNNEL_CLOSEING     4
#define MAX_TUNNEL_STATE    5

#define MAX_UID_LEN	 		 32
#define MAX_SESSION_ID_LEN	 64
#define P2P_ENC_KEY_LEN		 16
//
// structure of tunnel control block
//
typedef struct _tunnel_
{
    struct list_head list;              // link list
    pthread_t tunnel_thread;			// thread control block
    int             sock;               //  connect tunnel service socket
    unsigned char   ip[4];              // device ip
    unsigned short  port;               // device port;
    int             state;				// connecting, do auth then strat service
    fd_set          *rfds, *wfds;
    char            uid[MAX_UID_LEN+1];            		// device uid
    char            session_id[MAX_SESSION_ID_LEN+1];   // for identify session
    char            session_key[P2P_ENC_KEY_LEN+1];     // for encryption
    struct _queue_  rxqu;               // receive queue for peer data
    int				http_sock;			// local http port for access camera
    int				rtsp_sock;			// local rtsp port for access streaming
    struct list_head	link_head;		// active link process
    int 				link_num;    
}TUNNEL_CTRL;

#define MAX_TUNNEL_NUM  128

//
// Tunnel error code
//
#define TUNN_OK             0
#define TUNN_NO_MEM         -1
#define TUNN_NOT_AVAILABLE  -2
#define TUNN_QUEUE_FULL     -3
#define TUNN_SOCK_ERR       -4

//
// tunnel auth message
//
#define _tunne_auth_msg_    \
"{\"uid\":\"%s\",\"session_id\":\"%s\",\"action\":\"%s\"}"

//
// function and external variable declear
//
#include "tunnutil.h"
#include "tunnel_ext.h"

#endif
// end of define TUNNEL