//
//  module: device link service
//      link.c
//      control the specified link of service
//
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "tunnel.h"
#include "tunnutil.h"
#include "tunnmsg.h"
#include "link.h"
#include "tunnel_ext.h"

const char *msg_link_state[MAX_LINK_STATE]={"IDLE","CONNECTING","CONNECTED","CLOSING"};
//
//  function: get_free_link
//      find a free link block in tunnel control block
//      parameters
//			TUNNEL_CTRL *tc
//      return
//          link pointer    find
//          NULL            not found
//
TUNN_LINK *get_free_link(TUNNEL_CTRL *tc, char id)
{
    int i;
    TUNN_LINK *link = tc->link;
    
    for(i=0;i<MAX_LINK_NUM;i++,link++)
    {
        if (link->state != LINK_IDLE)
            continue;
            
        if (queue_init(&link->txqu, XMT_QU_SIZE)<0)
            return NULL;
        link->sock = -1;    
        link->id = id;   //(0~127), same as server
        return link;
    }
    return NULL;
}

//
//  function: free_link
//      free a active link
//      parameters
//          tunnel control block
//          link id, if -1, free all
//      return
//          0       success
//          other   fail
//
void free_link(TUNNEL_CTRL *tc, int id)
{
    int i;
    TUNN_LINK *link = tc->link;
    
    for(i=0;i<MAX_LINK_NUM;i++,link++)
    {
        if (link->id != id)
            continue;
            
        if (link->state == LINK_IDLE)
            continue;
            
        p2p_log_msg("%s(): free %d, socke=%d, %s!\n", __FUNCTION__, link->id, link->sock, msg_link_state[link->state]);
        if (link->sock>=0)
        {
            FD_CLR(link->sock, tc->rfds);
            FD_CLR(link->sock, tc->wfds);
#ifdef _OPEN_SECURED_CONN
            if (link->ssl)
            {    
                ssl_server_close(link->ssl);
                link->ssl = NULL;
            }
            else
#endif            
            close(link->sock);
            link->sock =-1;
        }
        queue_exit(&link->txqu);
        link->state = LINK_IDLE;
    }
}

//
//  function: close_link
//      close a specified active link
//      parameters
//          tunnel control block
//          link id
//      return
//          0       success
//          other   fail
//
void close_link(TUNNEL_CTRL *tc, char id, char signal)
{
    int i,size;
    struct _link_head_ *msg;
    char buf[128];
    TUNN_LINK *link = tc->link;
    
    for(i=0;i<MAX_LINK_NUM;i++,link++)
    {
        if (link->id != id)
            continue;
        if (link->state==LINK_IDLE)    
            continue;
            
        msg = (struct _link_head_ *)buf;
        // send FIN or RST
        if (link->state != LINK_CLOSEING)
        {    
            link->time = get_time_milisec() + 1000;  //wait time 1 seconds
            link->state = LINK_CLOSEING;
            msg->signal = signal;
        }
        else
            msg->signal = RST;
        msg->id = link->id;
        msg->dlen = 0;    
        do_sent_message(tc, DATA_TAG+ENCRYPT_TAG, (char *)msg, sizeof(struct _link_head_));
        size = queue_data_size(&link->txqu);
        
        p2p_log_msg("%s(): close %d, %s sock=%d, signal %x, queued %d!\n", 
            __FUNCTION__, link->id, msg_link_state[link->state],  link->sock, (unsigned char)msg->signal, size);
            
        // if not data waiting to xmt to local, close socket
        if (link->sock>=0 && size==0)
        {    
            FD_CLR(link->sock, tc->rfds);
            FD_CLR(link->sock, tc->wfds);
#ifdef _OPEN_SECURED_CONN
            if (link->ssl)
            {    
                ssl_server_close(link->ssl);
                link->ssl = NULL;
            }
            else
#endif             
            close(link->sock);
            link->sock =-1;
        }
    }
}

//
//  function: get_active_link
//      find a  link block with same id
//      parameters
//			TUNNEL_CTRL *tc
//          link id
//      return
//          link pointer    find
//          NULL            not found
//
TUNN_LINK *get_active_link(TUNNEL_CTRL *tc, char id)
{
    int i;
    TUNN_LINK *link = tc->link;
    
    for(i=0;i<MAX_LINK_NUM;i++,link++)
    {
        if (link->state != LINK_IDLE && 
            link->id == id)
            return link;
    }
    return NULL;
}

//
//  function: new_link_process
//      add new local connection to tunnel control block
//      parameters
//          socket
//          remote_port: HTTP 80, RTSP 554
//          rfds, wfds,
//			client address
//      return
//          0       success
//          other   fail
//
unsigned char link_id = 0; // 0~127
#ifdef _OPEN_SECURED_CONN
int new_link_process_local(TUNNEL_CTRL *tc, int sd, unsigned short remote_port, struct sockaddr_in *addr, SSL *ssl)
#else
int new_link_process_local(TUNNEL_CTRL *tc, int sd, unsigned short remote_port, struct sockaddr_in *addr)
#endif
{
    TUNN_LINK *link=NULL;
    int ret = TUNN_OK;
    char buf[512];
    struct _link_head_ *msg = (struct _link_head_ *)buf;
    
    // get a link control block,
    link = get_free_link(tc, (link_id++)&0x7f); // 0 ~ 127
    if (!link)
    {    
        ret = TUNN_NO_MEM;
#ifdef _OPEN_SECURED_CONN
        if (ssl)
            ssl_server_close(ssl);
#endif        
        goto end_new_link_process;
    }    
    link->sock = sd;
    link->port = remote_port;
#ifdef _OPEN_SECURED_CONN
    link->ssl = ssl;
#endif    
    link->state = LINK_CONNECTING;
    memcpy(&link->addr, addr, sizeof(struct sockaddr_in));
    // set the signal to remote
    msg->signal = SYN;
    msg->id = link->id;
    msg->dlen = sizeof(unsigned short);    
    *(unsigned short *)msg->data = link->port;
    // sent message to peer
    ret = do_sent_message(tc, DATA_TAG+ENCRYPT_TAG, (char *)msg, sizeof(struct _link_head_)+ sizeof(unsigned short));
    if (ret<0)
    {
        ret = TUNN_QUEUE_FULL;
        goto end_new_link_process;
    }
    
    // connecting
    fd_set_blocking(link->sock,0);
    FD_SET(link->sock, tc->rfds);
    *tc->nfds = (link->sock > *tc->nfds?link->sock: *tc->nfds);
    link->time = get_time_milisec() + 3000;     // wait 3 seconds
    p2p_log_msg("%s(): link %s link id=%d, sock=%d!\n", __FUNCTION__, msg_link_state[link->state], link->id, link->sock);
    
end_new_link_process:
    if (ret<0)
    {    
        p2p_log_msg("%s(): connect fail %d!\n", __FUNCTION__, ret);
        if (link)
            free_link(tc, link->id);
            //queue_exit(&link->txqu);
    }
	return ret;
}


//
//  function: new_link_process_remort
//      get the port number and try to connect to local server (none-blocking mode)
//      then wait response from server
//
//      parameters
//			TUNNEL_CTRL *tc
//          received data
//          len of data
//      return
//          link pointer    find
//          NULL            not found
//
#ifdef _OPEN_SECURED_CONN
void new_link_process_remort(TUNNEL_CTRL *tc, char id, int len, char *data, SSL *ssl)
#else
void new_link_process_remort(TUNNEL_CTRL *tc, char id, int len, char *data)
#endif
{
    extern unsigned char local_ip[4];
    TUNN_LINK *link=NULL;
    int ret = TUNN_OK, sid=-1;
    struct sockaddr_in client;
    char *addr;
    char buf[64];
    struct _link_head_ *msg;
    
    link = get_free_link(tc, id);
    if (link)
    {
        // do local connect
        if ((sid = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            ret = TUNN_SOCK_ERR;
#ifdef _OPEN_SECURED_CONN
            if (ssl)
                ssl_server_close(ssl);
#endif            
            goto reset_link;
        }
        link->sock = sid;
        link->port = *(unsigned short *)data;
#ifdef _OPEN_SECURED_CONN
        link->ssl = ssl;
#endif         
        memset(&client, 0, sizeof(client));
        client.sin_family = AF_INET;
        client.sin_port = htons(link->port); // RTSP/http port
        memcpy(&client.sin_addr.s_addr, local_ip, sizeof(unsigned int));
        memcpy(&link->addr, &client, sizeof(struct sockaddr_in));
        // do block conenct
        addr = inet_ntoa(client.sin_addr); // return the IP
        // set none-blocking mode, reuse address?
        fd_set_blocking(link->sock, 0);
        
        p2p_log_msg("%s(): link %d sock=%d connect to %s:%u!\n",__FUNCTION__,link->id, link->sock, addr, link->port);
#ifdef _OPEN_SECURED_CONN
        p2p_log_msg("%s(): nees secure_connect!\n",__FUNCTION__);
        ret = -1;
        goto reset_link;        
#else        
        ret = connect(link->sock, (struct sockaddr *) &client, sizeof(struct sockaddr));
#endif        
        if (ret < 0)
        {    
            if (errno==EINPROGRESS)
            {
                // in progress
                link->state = LINK_CONNECTING;
                link->time = get_time_milisec() + 1000;;  //wait time 1 seconds
                FD_SET(link->sock, tc->wfds);              // enable write to wait ready
                *tc->nfds = (link->sock > *tc->nfds ? link->sock : *tc->nfds);
                p2p_log_msg("%s(): link connect %d %s!\n",__FUNCTION__, link->id, msg_link_state[link->state]);
            }    
            else
            {        
                // we do not chanel link state, so it still idle 
                p2p_log_msg("%s(): link connect fail %d, errno=%d!\n",__FUNCTION__,ret, errno);
                goto reset_link;
            }
        }
        else
        {
            msg = (struct _link_head_ *)buf;
            // send SYNACK reaponse        
            msg->signal = SYN+ACK;
            msg->id = link->id;
            msg->dlen = 0;    
            ret = do_sent_message(tc, DATA_TAG+ENCRYPT_TAG, buf, sizeof(struct _link_head_));
            if (ret<0)
                goto reset_link;
            // connected    
            link->state = LINK_CONNECTED;
            link->time = get_time_milisec() + 10000;  //wait time 10 seconds, refresh by received data
            p2p_log_msg("%s(): link %d %s!\n",__FUNCTION__,link->id, msg_link_state[link->state]);
        }
    }    
    else
    {
reset_link: 
        p2p_log_msg("%s(): link %d reset: %d!\n",__FUNCTION__, id, ret);
        free_link(tc, id);
        // send reset to peer
        msg = (struct _link_head_ *)buf;
        msg->signal = RST;
        msg->id = id;
        msg->dlen = 0;    
        do_sent_message(tc, DATA_TAG+ENCRYPT_TAG, buf, sizeof(struct _link_head_));
    }
    return;    
}

//
//  function: link_recv_process
//      processing the received data from tunnel peer
//      parameters
//			TUNNEL_CTRL *tc
//          received data
//          len of data
//      return
//          link pointer    find
//          NULL            not found
//
void link_recv_process(TUNNEL_CTRL *tc, char *data, int dlen)
{
    struct _link_head_ *msg = (struct _link_head_ *)data;
    TUNN_LINK *link;
    int len;
    
    // process the data from peer  
    link = get_active_link(tc, msg->id);
    if (!link)
    {
        static unsigned int last_time=0;        
        // if SYN?
        if (msg->signal==SYN)
#ifdef _OPEN_SECURED_CONN
            // create SSL control block?
            new_link_process_remort(tc, msg->id, msg->dlen, msg->data, NULL);
#else            
            new_link_process_remort(tc, msg->id, msg->dlen, msg->data);
#endif            
        else if (msg->signal!=RST)
        {    
            if (last_time < get_time_milisec())            
                p2p_log_msg("%s(): ignore %x , RST link id=%d data size=%d!\n",__FUNCTION__,msg->signal, msg->id, msg->dlen);    
            msg->signal = RST;
            msg->dlen = 0;    
            do_sent_message(tc, DATA_TAG+ENCRYPT_TAG, (char *)msg, sizeof(struct _link_head_));
        }
        else if (last_time < get_time_milisec())
            p2p_log_msg("%s(): ignore %x, id=%d data size=%d!\n",__FUNCTION__,(unsigned char)msg->signal,msg->id, msg->dlen);    
        last_time = get_time_milisec()+1000;            
    }    
    else 
    {   
        if (msg->signal & RST)
        {
           p2p_log_msg("%s(): warning! Got RST, close link %d\n",__FUNCTION__, link->id);
           free_link(tc, link->id); 
           return;
        }   
        //
        // handle the data from tunnel peer
        // if link is > connected, other state will ignore?
        // forward the received data
        // check the signal bit
        // if close, set to closing state, and start timer
        // 
        //p2p_log_msg("%s(): link %s, signal=%x link id=%d, dlen=%d!\n", 
        //    __FUNCTION__, msg_link_state[link->state], (unsigned char)msg->signal, link->id, msg->dlen); 
        switch(link->state)
        {
            case LINK_CONNECTING:
        
                if ((msg->signal & (SYN+ACK))==(SYN+ACK))
                {    
                    link->state = LINK_CONNECTED;
                    link->time = get_time_milisec() + 10000;     // wait 10 seconds
                    p2p_log_msg("%s(): Got %x link %d sock=%d %s\n", 
                        __FUNCTION__, (unsigned char)msg->signal, link->id, link->sock, msg_link_state[link->state]);
                }
                else
                {    
                    break;
                }
            case LINK_CONNECTED:    
                if (msg->dlen)
                {   
                    int ret, offset;
                    
                    len = msg->dlen;
                    // no data in queue?, write it immediately
                    if (queue_data_size(&link->txqu)==0)
	                {    
        #ifdef _OPEN_SECURED_CONN
                        if (link->ssl)
                            ret = ssl_server_send(link->ssl, link->sock, msg->data, len);
                        else
        #endif	                    
	                        ret = send(link->sock, msg->data, len, MSG_NOSIGNAL );  
	                    if (ret<0)  
	                    {      
                            p2p_log_msg("%s(): warning! link %d write sock=%d err %d, errno=%d \n",
                                __FUNCTION__, link->id, link->sock, ret, errno);
	                        break;
	                    }
    	                len -= ret;
    	                offset = ret;
    	                //update counter
    	                link->tx_pcnt+=1;
       	                link->tx_bcnt+=ret;
	                }
	                else
	                    offset = 0;
                    
                    if (len)
                    {
                        // put received data in queue
                        //dump_frame("sent to local", msg->data,msg->dlen); 
                        len = queue_put(&link->txqu, &msg->data[offset], len);
                        if (len<0)
                            p2p_log_msg("%s(): warning! put data in link %d queue fail len=%d, err=%d \n",
                                __FUNCTION__, link->id, msg->dlen, len);
	                    FD_SET(link->sock, tc->wfds);
	                }
                    link->time = get_time_milisec() + 10000;     // wait 10 seconds
                }
                
                if (msg->signal & FIN)
                {    
                    p2p_log_msg("%s(): Got %x, link %d sock=%d %s\n", 
                        __FUNCTION__, (unsigned char)msg->signal, link->id, link->sock, msg_link_state[link->state]);
                    close_link(tc, link->id, FIN+ACK);
                    // if no data need tx, close it?
                    if (queue_data_size(&link->txqu)==0)
                        free_link(tc, link->id);
                }
                break;    
            case LINK_CLOSEING:
                if (msg->dlen)
                    p2p_log_msg("%s(): warning! Got %x, link %d %s ignore data %d \n",
                        __FUNCTION__, msg->signal, link->id, msg_link_state[link->state], msg->dlen);
                // got fin/fin+ACK close socket        
                if (msg->signal & (FIN+ACK))
                    free_link(tc, link->id);    
                break;
        }
    }            
}

//
// function: do_sent_link
//      write data to link socket.
//      parameters:
//          tunnel control block
//          link id
//      return
//          
int do_sent_link(TUNNEL_CTRL *tc, TUNN_LINK *link, char *buf, int bufsize)
{
    int len, dlen, err, ret=TUNN_OK;
    // write data to link
    do{
        len=-1;
        dlen = queue_peek(&link->txqu, buf, bufsize);
       	// sent out
        if (dlen>0)
        {    
#ifdef _OPEN_SECURED_CONN
            if (link->ssl)
                ret = ssl_server_send(link->ssl, link->sock, buf, dlen);
            else
#endif            
       	        len = send(link->sock, buf, dlen, MSG_NOSIGNAL );
       	}
        else
       	    len = dlen;
       	// update queue pointer    
       	if (len>0)
       	{    
       	    queue_move(&link->txqu,len);
       	    link->tx_pcnt+=1;
       	    link->tx_bcnt+=len;
       	}
        else           	            
       	{   
       	    err = errno;
       		FD_CLR(link->sock,tc->wfds);
       		if (len<0)
       		{
       		    ret = len;
       		    p2p_log_msg("%s(): link %d %s sock=%d write error %d errno=%d!\n",
       		        __FUNCTION__,link->id, msg_link_state[link->state], link->sock, ret, err);
           		break; 
            }    
       	}
    }while(len>0);
    
    return ret;
}   // end of do_sent_link


//
//  function: tunnel_link_process
//      process all active links and handle the required 
//      parameters
//          tc
//      return
//          none
//
void tunnel_link_process(TUNNEL_CTRL *tc, fd_set *rfds, fd_set *wfds)
{
    int i,dlen, ret;
    TUNN_LINK *link = tc->link;
    char buf[SIZE_OF_MSGBUF];
    struct _link_head_ *msg;
    
    for(i=0;i<MAX_LINK_NUM;i++,link++)
    {
        if (link->state == LINK_IDLE)
            continue;
            
        msg = (struct _link_head_ *)buf; 
        switch(link->state)
        {
            case LINK_CONNECTING:
                // get data and put in queue,
            break;
            case LINK_CONNECTED:
                // check writable?, forward data to device
                if (FD_ISSET(link->sock, wfds))
                {
                    ret = do_sent_link(tc, link, buf, SIZE_OF_DATABUF);
                    if (ret>=0)
                        link->time = get_time_milisec() + 10000;     // extend wait 10 seconds
                }   // end process txqu
                
                // receive data from device, only work in connected
                if (FD_ISSET(link->sock, rfds))
                {
                    FD_CLR(link->sock, rfds);
                    
                    // check if tunnel tx queue has space?
                    if (queue_space(&tc->txqu)< (SIZE_OF_DATABUF*2))
                    {    
                         static int last_time = 0;
                         link->time = get_time_milisec() + 10000;;  //extend time 10 seconds
                        
                        if (last_time < get_time_milisec())
                        {    
       	                    p2p_log_msg("%s(): link %d, sock=%d, congestion, queue data %d \n",
       	                        __FUNCTION__,link->id, link->sock, queue_data_size(&tc->txqu));
       	                    last_time = get_time_milisec()+1000;
       	                }
                        break;
                    }
#ifdef RAW_TUNN_TEST
    #ifdef _OPEN_SECURED_CONN
                    if (link->ssl)
                        dlen = ssl_server_recv(link->ssl, link->sock, msg, SIZE_OF_DATABUF);
                    else
    #endif
                        dlen = recv(link->sock, (void *)msg, SIZE_OF_DATABUF, 0);
                    if (dlen>0)
                    {    
                        do_sent_raw(tc, msg, dlen);
                        link->time = get_time_milisec() + 10000;     // extend wait 10 seconds
                    }
#else                    
                    // select will always has data in socket
    #ifdef _OPEN_SECURED_CONN
                    if (link->ssl)
                        dlen = ssl_server_recv(link->ssl, link->sock, msg->data, SIZE_OF_DATABUF);
                    else
    #endif                    
                        dlen = recv(link->sock, (void *)msg->data, SIZE_OF_DATABUF, 0);
                    if (dlen>0)
                    {
       	                link->rx_pcnt+=1;
       	                link->rx_bcnt+=dlen;
       	           
                        // encode data and forward to peer
                        msg->signal = 0;    //no signal
                        msg->id = link->id;
                        msg->dlen = dlen;    
                        do_sent_message(tc, DATA_TAG+ENCRYPT_TAG, (char *)msg, dlen+sizeof(struct _link_head_));
                        link->time = get_time_milisec() + 10000;     // extend wait 10 seconds
                    }
#endif                    
                    else    
                    {    
                        // if get 0 (close) or <0 (sock error)
       	                p2p_log_msg("%s(): link %d, sock=%d, recv %d, errno=%d\n",__FUNCTION__,link->id, link->sock, dlen, errno);     
                        close_link(tc, link->id, FIN);
                    }
                }
            break;
            case LINK_CLOSEING:
                // check writable?, forward data to device
                if (link->sock>=0 && FD_ISSET(link->sock, wfds))
                {
                    do_sent_link(tc, link, buf, SIZE_OF_DATABUF);
                }   // end process txqu
                // if no data need tx, close it? may be data not imcoming?
                if (queue_data_size(&link->txqu)==0)
                    free_link(tc, link->id);
                break;
            default:
                p2p_log_msg("%s(): link %d unknow state %d!\n", __FUNCTION__, link->id, link->state);
                close_link(tc, link->id, RST);
                free_link(tc, link->id);
            break;
        }    
    }   // end for loop
}

//
//  function: link_update_counter
//      calculate a statistic counters
//      parameters
//          none
//      return
//          none
//   
void link_update_counter(TUNN_LINK *link)
{
    link->rx_pcnt_ps = link->rx_pcnt;
    link->rx_pcnt = 0;
    link->rx_bcnt_ps = link->rx_bcnt;
    link->rx_bcnt = 0;
    link->tx_pcnt_ps = link->tx_pcnt;
    link->tx_pcnt = 0;
    link->tx_bcnt_ps = link->tx_bcnt;
    link->tx_bcnt = 0;
    
    //p2p_log_msg("%s(): id=%d\ntx: %u p/s, %u b/s, rx %u p/s, %u b/s, queued %u bytes\n", __FUNCTION__, link->id,
    //    link->tx_pcnt_ps, link->tx_bcnt_ps, link->rx_pcnt_ps, link->rx_bcnt_ps, queue_data_size(&link->txqu));
}

//
//  function: tunnel_time_process
//      process all active links's timer
//      parameters
//          tc
//      return
//          none
//
void tunnel_time_process(TUNNEL_CTRL *tc)
{
    int i, update=0;
    TUNN_LINK *link = tc->link;
    unsigned int current = get_time_milisec();
    static unsigned int last_secs = 0;
    
    // update statistic count per second
    if (last_secs < current)
    {
        last_secs = current + 1000; // next seconds
        update = 1;
    }    
        
    for(i=0;i<MAX_LINK_NUM;i++,link++)
    {
        if (link->state == LINK_IDLE)
            continue;
            
        if (update)
            link_update_counter(link);
        
        if (link->time > current)
            continue;
                 
        p2p_log_msg("%s(): link %d expired, state %s!\n", __FUNCTION__, link->id, msg_link_state[link->state]);
        switch(link->state)
        {
            case LINK_CONNECTING:
                free_link(tc, link->id);
            break;
            case LINK_CONNECTED:
                close_link(tc, link->id, FIN);
            break;
            case LINK_CLOSEING:
                free_link(tc, link->id);
            break;
            default:
                free_link(tc, link->id);
                p2p_log_msg("%s(): link %d unknow state %d!\n", __FUNCTION__, link->id, link->state);
            break;
        }    
    }   // end for loop
}




//
//  function: show_link
//      display active link status
//      parameters
//          tc
//      return
//          none
//
void show_links(TUNNEL_CTRL *tc)
{
    TUNN_LINK *link = tc->link;
    int i;
    
    for(i=0;i<MAX_LINK_NUM;i++,link++)
    {
        if (link->state == LINK_IDLE)
            continue;
        printf("   link %d %s, port %d\n",link->id, msg_link_state[link->state], link->port);
        printf("      tx: %u p/s, %u b/s, rx %u p/s, %u b/s, queued %u bytes\n", 
            link->tx_pcnt_ps, link->tx_bcnt_ps, link->rx_pcnt_ps, link->rx_bcnt_ps, queue_data_size(&link->txqu));
    }
}