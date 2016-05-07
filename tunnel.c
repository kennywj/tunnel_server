//
//  module: CVR tunnel daemon
//      tunnel.c
//      provide tunnel service from devices 
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
#include "tunnmsg.h"
#include "link.h"



//
// const definition
//
const char *TunnelStateMag[MAX_TUNNEL_STATE]={"Idle","Connecting","Authentication","Service","Closing"};






//
//  function: do_device_auth
//      read first authentication message from socke and 
//		thne formward to SC clent to do authentication and authorize.
//
//      parameters
//          data biffer
//          length of data
//      return
//          0       success
//          other   fail
//
int do_device_auth(char *data, int len)
{
	int ret =0;
	
	
	return ret;
}


//
//  function: tunnel_device_handler
//      opne a service port and process the service request from devices
//      parameters
//          service port number
//      return
//          0       success
//          other   fail
//
void *tunnel_device_handler(void *pdata)
{
    #define SIZE_OF_MSGBUF  1600
    #define SIZE_OF_DATABUF 1600
    TUNNEL_CTRL *tc = (TUNNEL_CTRL *)pdata;
    fd_set readfds, writefds, rfds, wfds;
    int nfds=-1, sd=-1, ret=-1,len;
    struct timeval tv;
    struct sockaddr_in clientaddr; /* client addr */
    char data[SIZE_OF_DATABUF];
    
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    
    // receive the data from socke and do authentication
    len = read(tc->sock, data, SIZE_OF_DATABUF);
    if (len<0)
    {
        fprintf(stderr,"read socket %d error %d!", len, errno);
        goto exit;
    }
    ret = do_device_auth(data, len);
    if (ret <0)
   	{
        fprintf(stderr,"auth fail %d!",ret);
        goto exit;
    }
    	
	// set socket none-block
	fd_set_blocking(tc->sock,0);   
    FD_SET(tc->sock, &readfds);
    nfds = tc->sock;
    // reset timeout value
    tv.tv_sec = 1;  // 1 seconds
    tv.tv_usec = 0;
    //
    // listen on 2 local sockets, rtsp and http 
    //
    //ret = do_local_service(tc);
    
    //
    // main loop: wait for a connection request
    //
    
    while (1) 
    {
        memcpy(&rfds,&readfds,sizeof(fd_set));
        memcpy(&wfds,&writefds,sizeof(fd_set));
        
        if(select(nfds+1, &rfds, &wfds, NULL, &tv) == -1)
        {
            fprintf(stderr,"Server-select() error lol!");
            goto exit;
        }
        
        // new data incoming from peer
        if (FD_ISSET(tc->sock, &rfds))
       	{
       		
    	} 	
       	// local http connection 	
        if (FD_ISSET(tc->http_sock, &rfds))
        {
            // 
            //  accept: wait for a connection request 
            //
            size_t clientlen = sizeof(clientaddr);
            sd = accept(tc->http_sock, (struct sockaddr *) &clientaddr, &clientlen);
            if (sd >= 0)
            {    
            	// accetp new link from local
                ret = new_link_process(sd, &readfds, &writefds, &clientaddr);
                if (ret<0)
                {
                    fprintf(stderr,"rejset http connection from %s\n",inet_ntoa (clientaddr.sin_addr));
                    close(sd);
                }    
            }    
            else     
                fprintf(stderr,"ERROR on accept");
        }
        
        // servcie tunnels processing
        //tunnel_link_process(&rfds, &wfds);
        // time process
        //tunnel_time_process();
        
        // error handler if rfd not be cleared?
        
    }   // end of while
    
exit:   
	close(tc->sock);
	
    return NULL;
}   // endo fo tunnel server 

//
//  function: new_device_tunnel
//      handle the new device connection, add to list 
//      fork a new process to handle the device's traffic and then start do auth
//      parameters
//          socket id,
//          address of connection
//      return
//          0       success
//          other   fail
//
int new_device_tunnel(int sid,struct sockaddr_in *addr)
{
    pthread_attr_t attr;
    TUNNEL_CTRL *tc = get_tunnel_block();
    
    if (!tc)
        return TUNN_NOT_AVAILABLE;
    tc->sock = sid;
    tc->state = TUNNEL_CONNECTING;					 // do connect to TC and sent information
    *(unsigned int *)tc->ip = addr->sin_addr.s_addr; // device's the IP
    tc->port = ntohs(addr->sin_port);
    queue_init(&tc->rxqu, 2048);					 // buffer size 2048
    
    // start a thread running    
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	// start the device service thread
    pthread_create(&tc->tunnel_thread, &attr, tunnel_device_handler, (void *)tc);
    return TUNN_OK;
}


//
//  function: free_device_tunnel
//      find all active device thread and cforce to close it

//      parameters
//          none	free all active device process
//			tc		free specified tc
//      return
//          0       success
//          other   fail
//
int free_device_tunnel(void *tc)
{
    return TUNN_OK;
}
   
