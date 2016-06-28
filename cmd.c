/* states */
#include <ctype.h>
#include <stdio.h>		/* printf, scanf, NULL */
#include <stdlib.h>     /* malloc, free, rand */
#include <string.h>     /* memcpy */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#include "tunnel.h"
#include "tunnel_ext.h"
#include "cmd.h"

extern const char *TunnelStateMag[MAX_TUNNEL_STATE];
extern pthread_mutex_t lock;

void cmd_help(int argc, char* argv[])
{
    struct cmd_tbl  *p = &commands[0];
    
    while(p && p->name!=NULL)
    {
        printf("%-10s -- %10s,\n\t%10s\n",p->name, p->desc, p->usage); 
        p++;   
    }
}

//
//  function: cmd_quit
//      exit the tunnel server daemon
//  parameters
//      argc:   1
//      argv:   none
//
void cmd_quit(int argc, char* argv[])
{
    extern int do_exit;
    do_exit = -1; // user exit
}

//
//  function: cmd_stat
//      diaplay status of tunnel server daemon
//  parameters
//      argc:   1
//      argv:   none
//
extern void show_links(TUNNEL_CTRL *tc);

void cmd_stat(int argc, char* argv[])
{
    TUNNEL_CTRL *tc=NULL;
    struct list_head *head;
    
    pthread_mutex_lock(&lock);
    head = &tunnel_active_list;
    if (list_empty(head))
    {  
        pthread_mutex_unlock(&lock);  
        return;
    }    
    list_for_each_entry(tc, head, list)
    {
        printf("device %s from %u.%u.%u.%u:%u, sock=%d, state: %s\n",tc->uid,
            tc->ip[0],tc->ip[1],tc->ip[2],tc->ip[3],tc->port, tc->sock, TunnelStateMag[tc->state]);
        printf("   CVR WAN IP %u.%u.%u.%u\n",tc->cvr_ip[0],tc->cvr_ip[1],tc->cvr_ip[2],tc->cvr_ip[3]);            
        printf("   local service: http port %u, socket=%d, RTSP port %u, socket=%d\n",tc->http_port, tc->http_sock, tc->rtsp_port, tc->rtsp_sock);
        printf("   session id %s\n   session key %s\n",tc->session_id, tc->session_key);
        printf("   tx: %u p/s, %u B/s, rx: %u p/s, %u B/s, echo %u, tx queued %u, rx queue %u\n",
            tc->tx_pcnt_ps, tc->tx_bcnt_ps, tc->rx_pcnt_ps,tc->rx_bcnt_ps, tc->echo_count, queue_data_size(&tc->txqu), queue_data_size(&tc->rxqu));
        show_links(tc);
    }
    pthread_mutex_lock(&lock);
}

//
//  function: cmd_cfg
//      diaplay and program local device ip address
//  parameters
//      argc:   2
//      argv:   -r<report time, unit seconds, 0 disable)
//
void cmd_cfg(int argc, char* argv[])
{
/*    
    int c;
    struct sockaddr_in addr;
    
    while((c=getopt(argc, argv, "r:m:")) != -1)
    {
        switch(c)
        {
            case 'r':
                
            break;
            default:
                puts("wrong command\n");
                printf("usgae: %s\n",curr_cmd->usage);
            return;
        }
    }   // end while
*/    
}

//=============================================================================
//  function: cmd_ver
//      diaplay device agent firmware version
//  parameters
//      argc:   0
//      argv:   NULL
//=============================================================================
void cmd_ver(int argc, char* argv[])
{
    extern const char *software_version;
    printf("%s\n",software_version);
}
