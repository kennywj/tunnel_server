//
//  module: CVR tunnel daemon
//      main.c
//      entry function of CVR tunnel, provide tunnel service from devices 
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
#include "tunnel_ext.h"

//
// Tunnel control block
//
pthread_t service_thread;
int tunnel_server_on=0;
const char *software_version="TunnelServerDaemon_v0.1";
//
// function declear
//
extern int tunnel_server_init(unsigned short port);
extern void Tunnel_server_exit(void);
extern void do_console(void);
//
//  function: sample_signal_handle
//      handle the signal
//      parameters
//          signal
//      return
//          none
//
int quit = 0;
void sample_signal_handle(int sig)
{
    switch(sig)
    {
    case SIGINT:
        quit = 1;
        break;
    case SIGHUP:
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
//          SC clinet's URL
//      return
//          0       success
//          other   fail
//
int do_service_start(char *sc)
{   
    int ret = 0;
    
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
int do_service_report(char *sc)
{   
    int ret = 0;
    
    return ret;
}

//
//  function: do_service_exit
//      report to that tunnel service closed
//		curl http request
//      parameters
//          SC clinet's URL
//      return
//          0       success
//          other   fail
//
int do_service_exit(char *sc)
{   
    int ret = 0;
    
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
    unsigned short service_port = *(unsigned short *)pdata;
    fd_set readfds, writefds, rfds, wfds;
    int nfds=-1,servfd, sd=-1, ret=-1;
    struct timeval tv;
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    int optval; /* service socket */
    
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    //
    // socket: create the parent socket 
    //
    servfd = socket(AF_INET, SOCK_STREAM, 0);
    if (servfd < 0) 
    {    
        fprintf(stderr, "ERROR opening socket");
        goto exit;
    }
    
    // setsockopt: Handy debugging trick that lets 
    // us rerun the server immediately after we kill it; 
    // otherwise we have to wait about 20 secs. 
    // Eliminates "ERROR on binding: Address already in use" error. 
    //
    optval = 1;
    setsockopt(servfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

    //
    // build the server's Internet address
    //
    bzero((char *) &serveraddr, sizeof(serveraddr));

    // this is an Internet address
    serveraddr.sin_family = AF_INET;

    // let the system figure out our IP address */
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

    // this is the port we will listen on */
    serveraddr.sin_port = htons(service_port);

    // 
    // bind: associate the parent socket with a port 
    //
    if (bind(servfd, (struct sockaddr *) &serveraddr, sizeof(struct sockaddr_in)) < 0) 
	{   
         fprintf(stderr,"ERROR on binding");
         goto exit;
    }
    // 
    // listen: make this socket ready to accept connection requests 
    //
    if (listen(servfd, 5) < 0) // allow 5 requests to queue up
    {    
        fprintf(stderr,"ERROR on listen");
        goto exit;
    }
    FD_SET(servfd, &readfds);
    nfds = servfd;
    // reset timeout value
    tv.tv_sec = 1;  // 1 seconds
    tv.tv_usec = 0;
    
    //
    // tunnel control block init
    //
    if (tunnel_ctrl_init()<0)
  	{
  		fprintf(stderr, "initial tunnel control block fail!");
        goto exit;
  	}  	
    //
    // report to SC that service start
    //
    do_service_start("");
    
    //
    // main loop: wait for a connection request
    //
    while (tunnel_server_on) 
    {
        memcpy(&rfds,&readfds,sizeof(fd_set));
        memcpy(&wfds,&writefds,sizeof(fd_set));
        if(select(nfds+1, &rfds, &wfds, NULL, &tv) == -1)
        {
            fprintf(stderr, "Server-select() error lol!");
            goto exit;
        }
        
        // new conenction incoming?
        if (FD_ISSET(servfd, &rfds))
        {
            // 
            //  accept: wait for a connection request 
            //
            size_t clientlen = sizeof(clientaddr);
            sd = accept(servfd, (struct sockaddr *) &clientaddr, &clientlen);
            if (sd >= 0)
            {   
                ret = new_device_tunnel(sd, &clientaddr);
                if (ret<0)
                {
                    fprintf(stderr,"rejset connection from %s\n",inet_ntoa (clientaddr.sin_addr));
                    close(sd);
                }    
            }    
            else     
                fprintf(stderr,"ERROR on accept");
        }
        // if timer expired, do report to SC
        do_service_report("");
    }   // end of while
    
exit:  
	// close all existing tunnel
	do_service_exit("");
	free_device_tunnel(NULL);
    tunnel_server_on = 0;
    return NULL;
}   // endo fo tunnel server 

//
//  function: tunnel_server_init
//      initial the tunnel service daemon
//      parameters
//          service port number
//      return
//          0       success
//          other   fail
//

int tunnel_server_init(unsigned short port)
{
    pthread_attr_t attr;
    // start a thread running    
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    tunnel_server_on = 1;
    pthread_create(&service_thread, &attr, tunnel_server_handler, &port);
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
        // wait thread complete
        pthread_join(service_thread, (void **)&retstr);
        fprintf(stderr,"%s(): exit %s!\n", __FUNCTION__, retstr);
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
char service_controller[65] = "127.0.0.1:1550";
unsigned short service_port = 12800;

extern int tunnel_server_init(unsigned short port);
extern void tunnel_server_exit(void);

int main(int argc, char *argv[])
{
    int ret=0;
    char product_name[128] = {0};

    signal(SIGPIPE, SIG_IGN);
    
    strcat(product_name, "Triclouds_CVR_Daemon");

    printf("start CVR tunnel deamon service\n");  
    // start Tunnel server service
    ret = tunnel_server_init(service_port);
    // start process commands
    if (ret>=0)
        do_console();
        
    tunnel_server_exit();
    printf("exit CVR tunnel deamon service\n");  
    return 0;
}