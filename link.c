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
#include "link.h"


//
//  function: new_link_process
//      add new local connection to tunnel control block
//      parameters
//          socket
//          rfds, wfds,
//			client address
//      return
//          0       success
//          other   fail
//
int new_link_process(int sd, fd_set *rfds, fd_set *wfds, struct sockaddr_in *addr)
{
	return 0;
}
