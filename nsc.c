/*
 * libnsock netcat implementation
 * 
 * written by Joshua J. Drake (jduck@EFNet, libnsock@qoop.org)
 */

#include "nsock_tcp.h"
#include "nsock_errors.h"
#include "nsock_io_pipe.h"


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>


#define VERSTR 		"1.0"


#define MODE_CONNECT 	0x0001
#define MODE_LISTEN 	0x0002
#define MODE_MASK 	0x000f

#define FLAG_FORK 	0x0010
#define FLAG_RAND_LIST 	0x0020
#define FLAG_RAND_SRC 	0x0040
#define FLAG_ZERO_IO 	0x0080
#define FLAG_DATAPIPE 	0x0100
#define FLAG_MASK 	0xfff0


typedef struct __options_stru_
{
   u_short flags;
   u_char *lhost; 		/* host to listen on */
   u_char *shost; 		/* src for outgoing connect */
   u_char *dhost; 		/* host to connect to */
   u_char *pshost; 		/* src for connect to pipe host */
   u_char *phost; 		/* host to piped data to */
   u_int connect_timeout;
   u_int io_timeout;
   u_int verbosity;
} options_t;


/* globals.. */
options_t opts;


void parse_argv(u_int, u_char **);
int get_incoming(void);
int connect_to_host(u_char *, u_char *);
void show_usage(void);


int
main(c, v)
   u_int c;
   u_char *v[];
{
   int csd, dsd = -1;
   int io_ret;

   /* check out parameters */
   parse_argv(c, v);
   
   /* attempt to setup the listener/first connection */
   if ((opts.flags & MODE_MASK) == MODE_LISTEN)
     csd = get_incoming();
   /* connect to host */
   else if ((opts.flags & MODE_MASK) == MODE_CONNECT)
     csd = connect_to_host(opts.shost, opts.dhost);
   if (csd == -1)
     return 1;
   
   /* ok we have our first side setup.  what we do now
    * depends on whether or not a -d has been specified.
    */
   if (opts.flags & FLAG_DATAPIPE)
     {
	dsd = connect_to_host(opts.pshost, opts.phost);
	if (dsd == -1)
	  return 1;
	io_ret = nsock_io_pipe(csd, csd, dsd, dsd);
     }
   else
     io_ret = nsock_io_pipe(fileno(stdin), fileno(stdout), csd, csd);
     
   if (io_ret != 1
       && opts.verbosity > 0)
     {
	extern nsockerror_t nsock_iop_errors[];
	
	io_ret *= -1;
	if (nsock_iop_errors[io_ret].has_errno)
	  fprintf(stderr, "nsock_io_pipe: %s: %s\n",
		  nsock_iop_errors[io_ret].str,
		  strerror(errno));
	else
	  fprintf(stderr, "nsock_io_pipe: %s\n",
		  nsock_iop_errors[io_ret].str);
     }
   else if (opts.verbosity > 1)
     fprintf(stderr, "input/output finished successfully\n");
   
   if (opts.flags & FLAG_DATAPIPE)
     close(dsd);
   close(csd);
   
   /* yay! */
   return 0;
}



/*
 * show the usage..
 */
void
show_usage(void)
{
   fprintf(stderr,
	   "nsc v" VERSTR " - libnsock netcat implementation\n"
	   "by Joshua J. Drake (jduck@EFNet, libnsock@qoop.org)\n"
	   "\n"
	   "usage:\n"
	   "   connect out:  nsc [<options>] <dhost>:<dport>\n"
	   "   listen:       nsc -l [<options>] <lhost>[:<lport>]\n"
	   "\n"
	   "valid options:\n"
	   /* not implemented: src routing */
	   "    -d <phost>   pipe data to the specified host\n"
	   "    -f           fork into background (for datapipe mode only)\n"
	   "    -h           version and usage information (you're seeing it now)\n"
	   "    -i <secs>    only wait <secs> for read/writes (0 disables)\n"
	   "    -l           listen mode\n"
	   /* not implemented: -n: always not reverse resolve, but always forward */
	   /* not implemented: -o: hexdump */
	   "    -p <port>    netcat -p emulation\n"
	   "    -R           randomize listen port\n"
	   "    -r           randomize connect source port\n"
	   "    -S <shost>   specify source for pipe recipient (used with -d)\n"
	   "    -s <shost>   specify source for connection (connect out)\n"
	   /* not implemented: udp mode */
	   "    -v           turn on verbose mode (more == more)\n"
	   "    -w <secs>    only wait <secs> for a connection (0 disables)\n"
	   "    -z           \"zero i/o mode\"\n"
	   "\n"
	   "notes:\n"
	   "   - if you omit the source or listen port, a psuedo-random port will be used.\n"
	   "   - a source or listen port of 0 will yield the OS's default port selection behavior\n"
	   "   - if you omit the source or listen address, all local addresses will be bound\n"
	   "\n"
	   );
   exit(0);
}



/*
 * parse the arguments into the global opts structure
 */
void
parse_argv(c, v)
   u_int c;
   u_char *v[];
{
   /* parsing vars */
   u_int ch;
   int lport = -1;
   
   memset(&opts, 0, sizeof(opts));
   while ((ch = getopt(c, (char **)v, "d:fhi:lp:Rrs:vw:z")) != -1)
     {
	switch (ch)
	  {
	   case 'd':
	     opts.phost = optarg;
	     opts.flags |= FLAG_DATAPIPE;
	     break;
	     
	   case 'f':
	     opts.flags |= FLAG_FORK;
	     /* validated later */
	     break;

	   case 'h':
	     show_usage();
	     break;
	     
	   case 'i':
	     opts.connect_timeout = atoi(optarg);
	     break;
	     
	   case 'l':
	     if ((opts.flags & MODE_MASK))
	       {
		  fprintf(stderr, "%s: -%c: mode already specified!\n", v[0], (u_char)ch);
		  exit(1);
	       }
	     opts.flags |= MODE_LISTEN;
	     break;
	     
	   case 'p':
	     lport = atoi(optarg);
	     if (lport > USHRT_MAX)
	       {
		  fprintf(stderr, "%s: -%c: invalid local port: %s\n", v[0], (u_char)ch, optarg);
		  exit(1);
	       }
	     break;
	     
	   case 'R':
	     opts.flags |= FLAG_RAND_LIST;
	     break;
	     
	   case 'r':
	     opts.flags |= FLAG_RAND_SRC;
	     break;
	     
	   case 'S':
	     opts.pshost = optarg;
	     break;
	     
	   case 's':
	     opts.shost = optarg;
	     /*
	      * 
	     opts.shost = strdup(optarg);
	     if (!opts.shost)
	       {
		  fprintf(stderr, "%s: -%c: unable to set source host: %s\n",
			  v[0], (u_char)ch, strerror(errno));
		  exit(1);
	       }
	      * 
	      */
	     break;
	     
	   case 'v':
	     opts.verbosity++;
	     break;
	     
	   case 'w':
	     opts.io_timeout = atoi(optarg);
	     break;
	     
	   case 'z':
	     opts.flags |= FLAG_ZERO_IO;
	     break;

	   default:
	     /* not reached */
	     exit(1);
	  }
     }
   c -= optind;
   v += optind;
   
   /* if nothing was chosen, error (netcat asks for cmdline, ew) */
   if ((opts.flags & MODE_MASK) == 0)
     {
	if (c > 0)
	  opts.flags |= MODE_CONNECT;
	else
	  {
	     fprintf(stderr, "please specify an action or host to connect to\n");
	     exit(1);
	  }
     }

   /* get any addresses that we need */
   if (c < 1 || *v[0] == '\0')
     {
	/* nothing specified, we can assume defaults for LISTEN */
	if ((opts.flags & MODE_MASK) == MODE_LISTEN)
	  opts.lhost = "0";
	else
	  {
	     fprintf(stderr, "please specify a host and/or port to connect to\n");
	     exit(1);
	  }
     }
   else
     {
	if ((opts.flags & MODE_MASK) == MODE_CONNECT)
	  opts.dhost = v[0];
	else
	  opts.lhost = v[0];
	v++;
	c--;
     }
   
   /* emulate netcat -p */
   if (lport >= 0)
     {
	if ((opts.flags & MODE_MASK) != MODE_CONNECT)
	  {
	     if (!strchr(opts.lhost, ':'))
	       /* add the port to the listen host (memory leaked) */
	       opts.lhost = strdup(nsock_tcp_host(opts.lhost, (u_short)lport));
	  }
	else
	  /* add the port to the source host if one is not specified (memory leaked) */
	  opts.shost = strdup(nsock_tcp_host(opts.shost, (u_short)lport));
     }
   
   /* ignore the remainder of the args 
    * 
    * maybe should check to see if netcat style ports are following
    */
}


int
get_incoming(void)
{
   int flags = NSTCP_REUSE_ADDR;
   int lsd, csd;
   nsocktcp_t *listener;
   u_char error_buf[256];
   struct sockaddr_in cli;
   socklen_t clilen;
   
   /* get an incoming connection */
   if (!strchr(opts.lhost, ':') || opts.flags & FLAG_RAND_LIST)
     flags |= NSTCP_RAND_SRC_PORT;
   if (!(listener = nsock_tcp_init_listen(opts.lhost, flags, error_buf, sizeof(error_buf))))
     {
	if (opts.verbosity > 0)
	  fprintf(stderr, "nsock_tcp_init_listen failed, probably out of memory.\n");
	return -1;
     }
   /* listen for someone */
   lsd = nsock_tcp_listen(listener, 1);
   if (flags == NSTCP_RAND_SRC_PORT || opts.verbosity > 0)
     printf("listening on %s [%s:%hu]\n", opts.lhost,
	    inet_ntoa(listener->fin.sin_addr), ntohs(listener->fin.sin_port));
   nsock_tcp_free(&listener);
   if (lsd < 0)
     {
	if (opts.verbosity > 0)
	  fprintf(stderr, "unable to listen: %s\n", error_buf);
	return -1;
     }
   /* become daemon if requested */
   if ((opts.flags & FLAG_DATAPIPE)
       && (opts.flags & FLAG_FORK))
     {
	int cpid;
	
	cpid = fork();
	if (cpid == -1)
	  {
	     if (opts.verbosity > 0)
	       perror("fork failed");
	     return -1;
	  }
	if (cpid > 0)
	  exit(0);
	/* now only child exists =) */
	if (opts.verbosity < 2)
	  {
	     close(fileno(stdin));
	     close(fileno(stdout));
	     close(fileno(stderr));
	  }
     }
   /* wait for their connection */
   csd = accept(lsd, (struct sockaddr *)&cli,  &clilen);
   if (csd == -1)
     {
	if (opts.verbosity > 0)
	  perror("unable to accept client");
	return -1;
     }
   if (opts.verbosity > 1)
     fprintf(stderr, "connection from [%s:%u] accepted\n",
	     inet_ntoa(cli.sin_addr), ntohs(cli.sin_port));
   /* dont need listener anymore */
   close(lsd);
   
   /* zero i/o mode? */
   if (opts.flags & FLAG_ZERO_IO)
     {
	close(csd);
	return -1;
     }
   return csd;
}



int
connect_to_host(source, target)
   u_char *source;
   u_char *target;
{
   int dsd;
   int flags = NSTCP_REUSE_ADDR;
   nsocktcp_t *dest;
   u_char error_buf[256];
   
   if (!source
       || !strchr(source, ':')
       || opts.flags & FLAG_RAND_SRC)
     flags |= NSTCP_RAND_SRC_PORT;
   
   /* initialize connection info */
   if (!(dest = nsock_tcp_init_connect(source, target, flags,
				       error_buf, sizeof(error_buf))))
     {
	if (opts.verbosity > 0)
	  fprintf(stderr, "nsock_tcp_init_connect failed, probably out of memory.\n");
	return -1;
     }
   /* try to connect */
   dsd = nsock_tcp_connect(dest, opts.connect_timeout);
   if (dsd < 0)
     {
	nsock_tcp_free(&dest);
	if (opts.verbosity > 0)
	  fprintf(stderr, "nsock_tcp_connect: %s\n", error_buf);
	return -1;
     }
   
   /* possibly tell our outgoing info */
   if (opts.verbosity > 1)
     {
	u_char fmt_buf[128], dhost_buf[1024], shost_buf[1024];
	
	strcpy(fmt_buf, "connection to %s ");
	snprintf(dhost_buf, sizeof(dhost_buf) - 1, "%s [%s:%u]",
		 dest->to,
		 inet_ntoa(dest->tin.sin_addr), ntohs(dest->tin.sin_port));
	dhost_buf[sizeof(dhost_buf) - 1] = '\0';
	if (source)
	  {
	     strcat(fmt_buf, "from %s ");
	     snprintf(shost_buf, sizeof(shost_buf) - 1, "%s [%s:%u]",
		      dest->from,
		      inet_ntoa(dest->fin.sin_addr), ntohs(dest->fin.sin_port));
	     shost_buf[sizeof(shost_buf) - 1] = '\0';
	  }
	strcat(fmt_buf, "established\n");
	fprintf(stderr, fmt_buf, dhost_buf, shost_buf);
     }
   nsock_tcp_free(&dest);
   if (opts.flags & FLAG_ZERO_IO)
     {
	close(dsd);
	return -0;
     }
   return dsd;
}
