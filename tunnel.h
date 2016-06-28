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
#include "link.h"

#ifdef _OPEN_SECURED_CONN
#include "openssl/bio.h"
#include "openssl/ssl.h"
#include "openssl/err.h"
#include "openssl/pem.h"
#include "openssl/x509.h"
#include "openssl/x509_vfy.h"
#endif
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
#define MAX_SESSION_KEY_LEN	 42
#define P2P_ENC_KEY_LEN		 16

#define SIZE_OF_DATABUF     MAX_TUNNEL_DATALEN
#define SIZE_OF_MSGBUF      (TUNNEL_HEAD_LEN+SIZE_OF_DATABUF)

#define XMT_QU_SIZE          0x10000
#define TUNN_QU_SIZE         0x80000

#define MIN_LINK_SERVICE_PORT   18000
#define MAX_LINK_SERVICE_PORT   19000
//
// remote device service port
#define HTTP_PORT 80
#define RTSP_PORT 554
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
    unsigned int    time;               // wait timer
    fd_set          *rfds, *wfds;       // global read/write fd set
    int             *nfds;              // max fd number pointer
    char            do_exit;            // do close tunnel
#ifdef _OPEN_SECURED_CONN
    SSL             *ssl;               // for SSL tunnel control
#endif    
    char            uid[MAX_UID_LEN+1];            		// device uid
    char            session_id[MAX_SESSION_ID_LEN+1];   // for identify session
    char            session_key[MAX_SESSION_KEY_LEN+1];     // for encryption
    struct _queue_  rxqu;               // receive queue for peer data
    struct _queue_  txqu;               // transmit queue to peer data
    unsigned char   cvr_ip[4];          // wan port IP address of CVR
    unsigned short  http_port;          // http server port forward to device http server;
    unsigned short  rtsp_port;          // rtsp server port forward to device http server;
    int				http_sock;			// local http port for access camera
    int				rtsp_sock;			// local rtsp port for access streaming
    TUNN_LINK	    link[MAX_LINK_NUM]; // active link process
    unsigned int    rx_pcnt;            // received from peer packet count
    unsigned int    rx_bcnt;            // received from peer bytes count
    unsigned int    tx_pcnt;            // transmit to peer packet count
    unsigned int    tx_bcnt;            // transmit to peer bytes count
    unsigned int    echo_count;         // echo counter
    unsigned int    rx_pcnt_ps;         // received from peer packet count per second
    unsigned int    rx_bcnt_ps;         // received from peer bytes count per second
    unsigned int    tx_pcnt_ps;         // transmit to peer packet count per second
    unsigned int    tx_bcnt_ps;         // transmit to peer bytes count per second
}TUNNEL_CTRL;

#define MAX_TUNNEL_NUM  128

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })
     
//
// Tunnel error code
//
#define TUNN_OK             0
#define TUNN_NO_MEM         -1
#define TUNN_NOT_AVAILABLE  -2
#define TUNN_ERR_PARAMETER  -3
#define TUNN_QUEUE_FULL     -4
#define TUNN_SOCK_ERR       -5
#define TUNN_WRITE_ERR      -6
#define TUNN_AUTH_FAIL      -7
#define TUNN_ERR_RESET      -8
#define TUNN_MUTEX_FAIL     -9

#ifdef RAW_TUNN_TEST
extern int do_sent_raw(TUNNEL_CTRL *tc, char *data, int dlen);
#else
extern int do_sent_message(TUNNEL_CTRL *tc, unsigned char tag, char *data, int dlen);
#endif
extern void link_recv_process(TUNNEL_CTRL *tc, char *data, int dlen);
#ifdef _OPEN_SECURED_CONN
extern int new_link_process_local(TUNNEL_CTRL *tc, int sd, unsigned short remote_port, struct sockaddr_in *addr, SSL *ssl);
#else
extern int new_link_process_local(TUNNEL_CTRL *tc, int sd, unsigned short remote_port, struct sockaddr_in *addr);
#endif
extern void tunnel_time_process(TUNNEL_CTRL *tc);
extern void tunnel_link_process(TUNNEL_CTRL *tc, fd_set *rfds, fd_set *wfds);
//
// function and external variable declear
//
#include "tunnutil.h"
#include "tunnel_ext.h"

#endif
// end of define TUNNEL