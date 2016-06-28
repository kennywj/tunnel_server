//
//  module: device link service
//      link.h
//      control the specified link of service
//
#ifndef TUNNEL_LINK
#define TUNNEL_LINK

#ifdef _OPEN_SECURED_CONN
#include "openssl/bio.h"
#include "openssl/ssl.h"
#include "openssl/err.h"
#include "openssl/pem.h"
#include "openssl/x509.h"
#include "openssl/x509_vfy.h"
#endif

//
// Link state
//
#define LINK_IDLE         0
#define LINK_CONNECTING   1
#define LINK_CONNECTED    2
#define LINK_CLOSEING     3
#define MAX_LINK_STATE    4

#define MAX_LINK_NUM        16

//
// structure of tunnel control block
//
typedef struct _tunn_link_
{
    int                 state;  // link state;
    unsigned char       id;     // link id  0 ~ 127;
    unsigned short      port;   // link port;
    int                 sock;   // local socket id
    unsigned int        time;   // wait timer
    struct sockaddr_in  addr;   // local connect peer address 
#ifdef _OPEN_SECURED_CONN
    SSL             *ssl;       // for SSL link control
#endif    
    struct _queue_      txqu;   // recived data from peer, before write to socket
    unsigned int        rx_pcnt;    // received from local packet count
    unsigned int        rx_bcnt;    // received from local bytes count
    unsigned int        tx_pcnt;    // transmit to local packet count
    unsigned int        tx_bcnt;    // transmit to local bytes count
    unsigned int        rx_pcnt_ps;         // received from peer packet count per second
    unsigned int        rx_bcnt_ps;         // received from peer bytes count per second
    unsigned int        tx_pcnt_ps;         // transmit to peer packet count per second
    unsigned int        tx_bcnt_ps;         // transmit to peer bytes count per second
}TUNN_LINK;

extern const char *msg_link_state[MAX_LINK_STATE];
extern void link_update_counter(TUNN_LINK *link);

#endif
//TUNNEL_LINK