//
//  module: CVR tunnel daemon
//      main.c
//      entry function of CVR tunnel, provide tunnel service from devices 
//
#include <stdio.h>
#include <stdlib.h>
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
#include "cJSON.h"
#ifdef _OPEN_SECURED_CONN
#include "openssl/bio.h"
#include "openssl/ssl.h"
#include "openssl/err.h"
#include "openssl/pem.h"
#include "openssl/x509.h"
#include "openssl/x509_vfy.h"
#endif
#include "tunnel_ext.h"

//
// Tunnel control block
//
pthread_t service_thread;
int tunnel_server_on=0;
const char *software_version="TunnelServerDaemon_v0.1";
unsigned char local_ip[4] = {127, 0, 0, 1};
unsigned short secure_service_port = 995;
unsigned short service_port = 996;

//
// function declear
//
extern int tunnel_server_init(void);
extern void tunnel_server_exit(void);
extern void do_console(void);
extern unsigned int get_time_milisec(void);
extern int do_exit;
//
//  function: sample_signal_handle
//      handle the signal
//      parameters
//          signal
//      return
//          none
//
void sample_signal_handle(int sig)
{
    printf("%s(): sig=%x\n",__FUNCTION__, sig);
    switch(sig)
    {
    case SIGHUP:
    case SIGINT:
        do_exit = sig;
        break;
    default:
        break;
    }
}


//
//  function: do_service_start
//      execute registry procedure with SC client
//		curl http request
//      parameters
//          message
//      return
//          0       success
//          other   fail
//
int do_service_start(char *msg)
{   
    int ret = 0;
    char buf[1024], resp[1024];
    
    printf("%s(): %s\n",__FUNCTION__, msg);
#ifndef TEST_TUNNEL    
    snprintf(buf, 1023, "%s", msg );
    ret = clouds_service(sc_report_url, buf, resp, 1023);
#endif    
    return ret;
}

//
//  function: do_service_report
//      if wait time expired, do report to SC
//		curl http request
//      parameters
//          SC clinet's URL
//      return
//          0       success
//          other   fail
//
int do_service_report()
{   
    int ret = 0, dlen;
    char *buf, resp[1024];
    extern int get_tunnel_info(char *buf, int bufsize);
#ifndef TEST_TUNNEL
    buf = malloc(16384);
    if (buf)
    {    
  		//printf( "show report\n");
        dlen = get_tunnel_info(buf, 16384);
        if (dlen>0)
            ret = clouds_service(sc_report_url, buf, resp, 1023);
  		//printf( "%s\n",buf);
        free(buf);
    }
#endif    
    return ret;
}

//
//  function: do_service_exit
//      report to that tunnel service closed
//		curl http request
//      parameters
//          mesasge
//      return
//          0       success
//          other   fail
//
int do_service_exit(char *msg)
{   
    int ret = 0;
    char buf[1024], resp[1024];
    
    printf("%s(): %s\n",__FUNCTION__, msg);
#ifndef TEST_TUNNEL    
    snprintf(buf, 1023, "%s signal = %d", msg, do_exit );
    
    ret = clouds_service(sc_report_url, buf, resp, 1023);
#endif    
    return ret;
}

//
// do_bind_service_port:
//      open and listen on specified service port
//
int do_bind_service_port(unsigned short port)
{
    int ret=-1, fd=-1;
    struct sockaddr_in serveraddr; /* server's addr */
    
    //
    // socket: create the parent socket 
    //
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) 
    {    
        p2p_log_msg( "ERROR opening socket");
        goto end_do_bind_service_port;
    }
    
    // setsockopt: Handy debugging trick that lets 
    // us rerun the server immediately after we kill it; 
    // otherwise we have to wait about 20 secs. 
    // Eliminates "ERROR on binding: Address already in use" error. 
    //
    int optval = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

    //
    // build the server's Internet address
    //
    bzero((char *) &serveraddr, sizeof(serveraddr));

    // this is an Internet address
    serveraddr.sin_family = AF_INET;

    // let the system figure out our IP address */
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

    // this is the port we will listen on */
    serveraddr.sin_port = htons(port);

    // 
    // bind: associate the parent socket with a port 
    //
    if ((ret=bind(fd, (struct sockaddr *) &serveraddr, sizeof(struct sockaddr_in))) < 0) 
	{   
         p2p_log_msg("ERROR on binding error = %d errno=%d\n", ret, errno);
         goto end_do_bind_service_port;
    }
    // 
    // listen: make this socket ready to accept connection requests 
    //
    if ((ret=listen(fd, 5)) < 0) // allow 5 requests to queue up
    {    
        p2p_log_msg("ERROR on listen, error = %d errno=%d\n",ret, errno);
        goto end_do_bind_service_port;
    }
    
    return fd;
end_do_bind_service_port:    
    if (fd>=0)
        close(fd);
    return ret;
}


//
//  function: tunnel_server_handler
//      opne a service port and process the service request from devices
//      parameters
//          service port number
//      return
//          0       success
//          other   fail
//
void *tunnel_server_handler(void *pdata)
{
    fd_set readfds, writefds, rfds, wfds;
    int nfds=-1,servfd=-1, sd=-1, ret=-1;
    struct timeval tv;
    struct sockaddr_in clientaddr; /* client addr */
    int optval; /* service socket */
    unsigned int next_time, current_time;
#ifdef _OPEN_SECURED_CONN
    SSL *ssl=NULL;
    int securefd = -1;
#endif                
    //
    // tunnel control block init
    //
    if (tunnel_ctrl_init()<0)
  	{
  		p2p_log_msg( "initial tunnel control block fail!");
        goto end_tunnel_server_handler;
  	} 
  	
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    
    // wait mone secure connection
    servfd = do_bind_service_port(service_port);
    if (servfd>=0)
    {    
        FD_SET(servfd, &readfds);
        nfds = (servfd>nfds?servfd:nfds);
    }
    else
       goto end_tunnel_server_handler;     
       
#ifdef _OPEN_SECURED_CONN
    // wait mone secure connection
    securefd = do_bind_service_port(secure_service_port);
    if (securefd>=0)
    {    
        FD_SET(securefd, &readfds);
        nfds = (securefd>nfds?securefd:nfds);
    }
    else
       goto end_tunnel_server_handler;
#endif    
     	
    //
    // report to SC that service start
    //
    do_service_start("tunnel server start");
    
    //
    // main loop: wait for a connection request
    //
    current_time = get_time_milisec();
    next_time  = current_time + 10000;   // next seconds
    while (tunnel_server_on) 
    {
        current_time = get_time_milisec();
        if (current_time > next_time)
        {    
            // if timer expired, do report to SC
            do_service_report();
            next_time = current_time + 10000;   // 10 seconds
        }
        memcpy(&rfds,&readfds,sizeof(fd_set));
        memcpy(&wfds,&writefds,sizeof(fd_set));
        // reset timeout value
        tv.tv_sec = 1;  // 1 seconds
        tv.tv_usec = 0;
        if((ret=select(nfds+1, &rfds, &wfds, NULL, &tv)) == -1)
        {
            p2p_log_msg( "Server-select() error lol!");
            goto end_tunnel_server_handler;
        }
#ifdef _OPEN_SECURED_CONN
        if (FD_ISSET(securefd, &rfds))
        {
            socklen_t clientlen = sizeof(clientaddr);
            sd = ssl_server_accept(securefd,&ssl);
            if (sd >= 0)
            {   
            	getpeername(sd, (struct sockaddr*)&clientaddr, &clientlen);
                p2p_log_msg("new secure connection from %s\n",inet_ntoa (clientaddr.sin_addr));
                ret = new_device_tunnel(sd, &clientaddr, ssl);
                if (ret<0)
                {
                    p2p_log_msg("reject secure connection from %s\n",inet_ntoa (clientaddr.sin_addr));
                    ssl_server_close(ssl);
                }
            }
            else     
                p2p_log_msg("ERROR on accept");
        }    
#endif        
        // new conenction incoming?
        if (FD_ISSET(servfd, &rfds))
        {
            // 
            //  accept: wait for a connection request 
            //
            socklen_t clientlen = sizeof(clientaddr);
            sd = accept(servfd, (struct sockaddr *) &clientaddr, &clientlen);
            if (sd >= 0)
            {   
            	getpeername(sd, (struct sockaddr*)&clientaddr, &clientlen);
                p2p_log_msg("new connection from %s\n",inet_ntoa (clientaddr.sin_addr));
#ifdef _OPEN_SECURED_CONN                
                ret = new_device_tunnel(sd, &clientaddr, NULL);
#else                
                ret = new_device_tunnel(sd, &clientaddr);
#endif                
                if (ret<0)
                {
                    p2p_log_msg("reject connection from %s\n",inet_ntoa (clientaddr.sin_addr));
                    close(sd);
                }    
            }    
            else     
                p2p_log_msg("ERROR on accept");
        }
    }   // end of while
    
end_tunnel_server_handler:  
	// close all existing tunnel
	if (servfd>0)
	    close(servfd);
#ifdef _OPEN_SECURED_CONN
    if (securefd>=0)
	    close(securefd);
#endif	    
	do_service_exit("tunnel server exit");
	tunnel_ctrl_free();
    tunnel_server_on = 0;
    return NULL;
}   // endo fo tunnel server 

//
//  function: tunnel_server_init
//      initial the tunnel service daemon
//      parameters
//          none
//      return
//          0       success
//          other   fail
//

int tunnel_server_init()
{
    pthread_attr_t attr;
    // start a thread running    
    pthread_attr_init(&attr);
    //pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    tunnel_server_on = 1;
    pthread_create(&service_thread, &attr, tunnel_server_handler, NULL);
    return 0;
}

//
// function: TUNNEL_Stop
//  stop a active tunnel
//  parameters:
//      none
//  return:
//      none

void tunnel_server_exit()
{
	char *retstr;
	
    if (tunnel_server_on)
    {
        tunnel_server_on = 0;
        // wait thread complete
        pthread_join(service_thread, (void **)&retstr);
        p2p_log_msg("%s(): exit %s!\n", __FUNCTION__, retstr);
    }    
}


//
//  function: main
//      entry function of CVR tunnel daemon
//      parameters:
//          SC clinet's URL
//          tunnel server service port
//      return:
//          0       success
//          other   fail
//

int main(int argc, char *argv[])
{
    int ret=0;
    char product_name[128] = {0};

    signal(SIGPIPE, SIG_IGN);
    
    strcat(product_name, "Triclouds_CVR_Daemon");
#ifdef _OPEN_SECURED_CONN
    ssl_server_init("cacert.pem","privkey.pem");        
    printf("\n ==== start secure CVR tunnel deamon service at port %d ====\n",secure_service_port);  
#endif
    printf("\n ==== start CVR tunnel deamon service at port %d ====\n",service_port);  
    // start Tunnel server service
    ret = tunnel_server_init();
    // start process commands
#ifndef TEST_TUNNEL     
    while(!do_exit)
        sleep(1);
#else
    if (ret>=0)
        do_console();
#endif        
    tunnel_server_exit();
    printf("exit CVR tunnel deamon service\n");  
    return 0;
}