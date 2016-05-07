/* states */
#include <ctype.h>
#include <stdio.h>		/* printf, scanf, NULL */
#include <stdlib.h>     /* malloc, free, rand */
#include <string.h>     /* memcpy */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "tunnel.h"
#include "tunnmsg.h"
#include "cmd.h"

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
}

//
//  function: cmd_stat
//      diaplay status of tunnel server daemon
//  parameters
//      argc:   1
//      argv:   none
//
void cmd_stat(int argc, char* argv[])
{
    
}

//
//  function: cmd_cfg
//      diaplay and program local device ip address
//  parameters
//      argc:   2
//      argv:   -a <device IP address>
//
void cmd_cfg(int argc, char* argv[])
{
/*    
    int c;
    struct sockaddr_in addr;
    while((c=getopt(argc, argv, "a:m:")) != -1)
    {
        switch(c)
        {
            case 'a':
                inet_aton(optarg, &addr.sin_addr); // store IP in antelope
                memcpy(local_ip, (char *)&addr.sin_addr.s_addr, 4);
            break;
#ifdef WIN32            
            case 'm':
                inet_aton(optarg, &addr.sin_addr); // store IP in antelope
                memcpy(my_ip, (char *)&addr.sin_addr.s_addr, 4);
            break;
#endif            
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
