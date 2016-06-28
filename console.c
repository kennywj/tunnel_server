#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "cmd.h"

#define MAX_ARGS 12
char args[MAX_ARGS + 1][65];
struct termios saved_attributes;
int do_exit =0;
extern int parser(unsigned inflag,char *token,int tokmax,char *line,
    char *brkused,int *next, char *quoted);
extern int fd_set_blocking(int fd, int blocking) ;

struct cmd_tbl commands[]=
{
	{"help",    cmd_help, "display all commands and help",    ""},
	{"quit",    cmd_quit, "exit this program",                ""},
	{"stat",    cmd_stat, "display P2P device agent status",  ""},
	{"cfg",     cmd_cfg,  "program local device ip, port",    "-a <ip address>"},
	{"ver",     cmd_ver,  "firmware version",                 ""},
	{NULL,        NULL}
};
// current command pointer
struct cmd_tbl *curr_cmd;

void
reset_input_mode (void)
{
  tcsetattr (STDIN_FILENO, TCSANOW, &saved_attributes);
  printf("recover terminal setting, bye\n");
}

void
set_input_mode (void)
{
  struct termios tattr;
//  char *name;

  /* Make sure stdin is a terminal. */
  if (!isatty (STDIN_FILENO))
    {
      fprintf (stderr, "Not a terminal.\n");
      exit (EXIT_FAILURE);
    }

  /* Save the terminal attributes so we can restore them later. */
  tcgetattr (STDIN_FILENO, &saved_attributes);
  atexit (reset_input_mode);

  /* Set the funny terminal modes. */
  tcgetattr (STDIN_FILENO, &tattr);
  tattr.c_lflag &= ~(ICANON|ECHO); /* Clear ICANON and ECHO. */
  //tattr.c_lflag &= ~(ICANON); /* Clear ICANON. */
  tattr.c_cc[VMIN] = 1;
  tattr.c_cc[VTIME] = 0;
  tcsetattr (STDIN_FILENO, TCSAFLUSH, &tattr);
}
 
// move to tunnutil.c 
/*int fd_set_blocking(int fd, int blocking) {
    // Save the current flags 
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return 0;

    if (blocking)
        flags &= ~O_NONBLOCK;
    else
        flags |= O_NONBLOCK;
    return fcntl(fd, F_SETFL, flags) != -1;
}
*/
//
// process command string and do command
//
int do_command(char *cmdbuf)
{
	int ret=-1, argc;
	struct cmd_tbl  *p = &commands[0];
    char *argv[MAX_ARGS + 1],brkused,quoted;
    int next;
    argc=0; next= 0;	
    while(parser(0,args[argc],64,cmdbuf,&brkused,&next,&quoted)==0)
    {
        if (brkused=='\r')	/* <CR> is a break so it won't be included  */
  	       break;		/* in the token.  treat as end-of-line here */
  	    argv[argc] = args[argc];
        argc++;
        if (argc>=MAX_ARGS)
            break;
    }    
    if (argc==0)
        return 1;
	while(p->name!=NULL)
	{
		if (strncmp(argv[0],p->name, strlen(p->name))==0)
		{
			opterr = 0;
  			optind = 1;
  			curr_cmd = p;
			(p->func)(argc, argv);
			ret = 0;
			break;
		}		
		p++;
	}    
	return ret;
}
//
// process the stdin
//
void do_console()
{
    int ret, maxfd=-1,count=0;
    // sent non-blocking mode
    fd_set rfd, efd, wfd, readset, writeset, errset;
    struct timeval tv;
    char cmdbuf[256],*cmd;
    
    FD_ZERO(&readset);
	FD_ZERO(&writeset);
	FD_ZERO(&errset);
    // set stdin select, initial STDIN
    set_input_mode();
    fd_set_blocking(STDIN_FILENO,0);  // set non-blocking mode
    FD_SET(STDIN_FILENO, &readset);
    FD_SET(STDIN_FILENO, &errset);
    maxfd = (STDIN_FILENO > maxfd ? STDIN_FILENO : maxfd);
    printf("Server>");      // prompt
    fflush(stdout);
    // process service protocol
    while (1)
    {
        /* Block until input arrives on one or more active sockets. */
        memcpy(&rfd,&readset,sizeof(fd_set));
  	    memcpy(&efd,&errset,sizeof(fd_set));
  		memcpy(&wfd,&writeset,sizeof(fd_set));
  		tv.tv_sec = 0;
  		tv.tv_usec = 100000; // 100ms
        ret = select (maxfd+1, &rfd, &wfd, &efd, &tv);
        if (ret<0)
        {
            perror ("select");
            break;
        }
        if (do_exit)
        {
            printf("do exit\n");    
            break;
        }
        //read from terminal
        if (FD_ISSET(STDIN_FILENO, &rfd))
        {
            int c;
            char ch;
            ret=read(STDIN_FILENO,&c,1);
            if (ret<=0)
                break;  
            ch = (char)c; 
            if (ch=='\n')
            {    
		        putchar('\r');
		        putchar('\n');
		        if (count>0)
			    {    
			        /* Skip leading blanks. */
	                cmd = cmdbuf;
   	                while ( isblank( (int)*cmd ) )
       	                cmd++;
			        if (do_command(cmd)<0)
			            printf("unknown command '%s'\n", cmd);
			    }
			    memset(cmdbuf,0,256);
			    count=0;
   			    printf("Server>");      // prompt
		    }
		    else if ((ch >= ' ') && (ch < 127))		// got printable char
	        {
	            cmdbuf[count++]=ch;
		        putchar(ch);
	        }
	        else if ((ch == 0x08 || ch==0x7f) && count)				// backspace
	        {
	            cmdbuf[count]='\0';
                count--;
        	    putchar(0x08);
		        putchar(' ');
		        putchar(0x08);
	        }
	        else if (ch == 0x1B)						// escape
	        {
		        while (count)					/* reset buffer */
		        {
			        count--;
			        putchar(0x08);
			        putchar(' ');
			        putchar(0x08);
		        }
		        cmdbuf[0] = 0x1B;							/* leave ESC in first byte */
		        cmdbuf[1] = 0;	
		        // next char will overwrite ESC */
	        }
	        fflush(stdout);
        }
    }// end while
}   //do_console
