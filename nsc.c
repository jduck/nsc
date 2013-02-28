/*
 * libnsock netcat implementation
 * 
 * written by Joshua J. Drake (jduck@EFNet, libnsock@qoop.org)
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <limits.h>

/* libnsock includes */
#include <nsock/nsock.h>
#include <nsock/errors.h>

#include "nsc.h"
#include "io_pipe.h"


/* globals.. */
options_t opts;

void parse_argv(u_int, u_char **);
nsock_t *get_incoming(void);
nsock_t *connect_to_host(u_char *, u_char *, u_char);
u_char *reverse_host(nsock_t *, struct sockaddr_storage *);
void show_usage(void);
int exec_prog(int *, int *, pid_t *);


int
main(c, v)
   u_int c;
   u_char *v[];
{
   nsock_t *csd = NULL, *dsd = NULL;
   int io_ret;
   u_char iop_opts = 0;

   /* check out parameters */
   parse_argv(c, v);
   
   /* attempt to setup the listener/first connection */
   if ((opts.flags & MODE_MASK) == MODE_LISTEN)
     csd = get_incoming();

   /* connect to host */
   else if ((opts.flags & MODE_MASK) == MODE_CONNECT)
     csd = connect_to_host(opts.shost, opts.dhost, 0);
   if (!csd)
     return 1;
   
   /* setup io_pipe options */
   if (opts.flags & FLAG_TELNET)
     iop_opts |= NSCIOP_ACK_TELNET;
   if (opts.flags & FLAG_STDOUT)
     iop_opts |= NSCIOP_STDOUT_TOO;
   
   /* ok we have our first side setup.  what we do now
    * depends on whether or not a -d has been specified.
    */
   if (opts.flags & FLAG_DATAPIPE)
     {
	dsd = connect_to_host(opts.pshost, opts.phost, 1);
	if (!dsd)
	  return 1;
#ifdef USE_NSOCK_IOP
	io_ret = nsock_io_pipe(csd, -1, -1, dsd, -1, -1);
#else
	io_ret = nsc_io_pipe(csd, -1, -1, dsd, -1, -1, iop_opts);
#endif
     }
   else if (opts.flags & FLAG_EXECPIPE)
     {
	int to, from;
	pid_t cpid;
	
	to = csd->sd;
	from = csd->sd;
	if (exec_prog(&to, &from, &cpid) == -1)
	  return 1;
#ifdef USE_NSOCK_IOP
	io_ret = nsock_io_pipe(csd, -1, -1, NULL, from, to);
#else
	io_ret = nsc_io_pipe(csd, -1, -1, NULL, from, to, iop_opts);
#endif
	if (waitpid(cpid, NULL, WNOHANG) != cpid)
	  {
	     kill(cpid, SIGTERM);
	     wait(NULL);
	  }
     }
   else
#ifdef USE_NSOCK_IOP
     io_ret = nsock_io_pipe(csd, -1, -1, NULL, fileno(stdin), fileno(stdout));
#else
     io_ret = nsc_io_pipe(csd, -1, -1, NULL, fileno(stdin), fileno(stdout), iop_opts);
#endif
     
   if (io_ret != NSERR_SUCCESS && opts.verbosity > 0)
     {
	/* decide which part of the pipe caused the error, and print it */
	if (dsd && dsd->ns_errno != NSERR_SUCCESS)
	  fprintf(stderr, "nsc_io_pipe: %s\n", nsock_strerror_full(dsd));
	else if (csd->ns_errno != NSERR_SUCCESS)
	  fprintf(stderr, "nsc_io_pipe: %s\n", nsock_strerror_full(csd));
	else if (errno != 0)
	  fprintf(stderr, "nsc_io_pipe: stdin/stdout: %s\n", strerror(errno));
	else
	  fprintf(stderr, "nsc_io_pipe: stdin/stdout: EOF\n");
     }
   else if (opts.verbosity > 1)
     fprintf(stderr, "input/output finished successfully\n");
   
   if (opts.flags & FLAG_DATAPIPE)
     nsock_close(dsd);
   nsock_close(csd);
   
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
#ifdef INET6
	   "    -4           force IPv4 mode\n"
	   "    -6           force IPv6 mode\n"
#endif
#ifdef HAVE_SSL
	   "    -c <file>    use this SSL cert file (for connect/listen)\n"
	   "    -C <file>    use this SSL cert file (for pipe host)\n"
#endif
	   /* new netcat -D: debugging */
	   /* new netcat -d: dont read stdin */
	   "    -d <phost>   pipe data to and from the specified host\n"
	   "    -e <prog>    pipe data to and from the specified program\n"
	   /* not implemented: -g, -G: src routing */
	   "    -f           fork into background (for pipe host mode only)\n"
	   "    -h           version and usage information (this is it)\n"
	   /* not implemented: -i: delay for line i/o */
	   /* new netcat -k: socket serv option (listen+fork) */
#ifdef HAVE_SSL
	   "    -k <file>    use this SSL private key file (for connect/listen)\n"
	   "    -K <file>    use this SSL private key file (for pipe host)\n"
#endif
	   "    -l           listen mode\n"
	   "    -n           do not reverse resolve hosts\n"
	   /* not implemented: -o: hexdump */
	   "    -O           also output to stdout (for datapipe/execpipe)\n"
	   "    -p <port>    netcat -p emulation\n"
	   "    -q           include out-of-band data\n"
	   "    -R           randomize listen port\n"
	   "    -r           randomize connect source port\n"
	   "    -S <shost>   specify source for pipe recipient (used with -d)\n"
	   /* new netcat -S: tcp md5 option */
	   "    -s <shost>   specify source for connection (connect out)\n"
	   "    -t           answer telnet options with DONT and WONT\n"
	   /* new netcat -U: unix sockets */
	   "    -u           UDP mode\n"
	   "    -v           increase verbosity level\n"
	   /* new netcat -w: stdin/socket idle limit */
	   "    -w <secs>    only wait <secs> for a connection (0 disables)\n"
#ifdef HAVE_SSL
	   /* new netcat -x: proxy addr/port */
	   "    -x           turn on SSL (for connect/listen)\n"
	   /* new netcat -X: proxy versions connec,socks4,socks5 */
	   "    -X           turn on SSL (for pipe host)\n"
#endif
	   "    -z           \"zero i/o mode\"\n"
	   "\n"
	   "notes:\n"
	   "   - if the source or listen port are omitted, a psuedo-random port will be used.\n"
	   "   - a source or listen port of 0 will yield the OS's default behavior.\n"
	   "   - if the source or listen address are omitted, INADDR_ANY will be bound.\n"
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
   opts.family = PF_UNSPEC;
   
   while ((ch = getopt(c, (char **)v,
		       "d:e:fhi:lnOp:qRrS:s:tuvw:z"
#ifdef HAVE_SSL
		       "C:c:K:k:Xx"
#endif
#ifdef INET6
		       "46"
#endif
		       )) != -1)
     {
	switch (ch)
	  {
	     
#ifdef INET6
	   case '4':
	     opts.family = PF_INET;
	     break;
	     
	   case '6':
	     opts.family = PF_INET6;
	     break;
#endif
	     
#ifdef HAVE_SSL
	   case 'C':
	     opts.flags |= FLAG_USE_SSL_P;
	     opts.pcert = (u_char *)optarg;
	     break;
	     
	   case 'c':
	     opts.flags |= FLAG_USE_SSL_D;
	     opts.dcert = (u_char *)optarg;
	     break;
#endif
	     
	   case 'd':
	     opts.phost = (u_char *)optarg;
	     opts.flags |= FLAG_DATAPIPE;
	     break;
	     
	   case 'e':
	     opts.pprog = (u_char *)optarg;
	     opts.flags |= FLAG_EXECPIPE;
	     break;
	     
	   case 'f':
	     opts.flags |= FLAG_FORK;
	     /* validated later */
	     break;
	     
	   case 'h':
	     show_usage();
	     break;

#ifdef HAVE_SSL
	   case 'K':
	     opts.pkey = (u_char *)optarg;
	     break;
	     
	   case 'k':
	     opts.dkey = (u_char *)optarg;
	     break;
#endif
	     
	   case 'l':
	     if ((opts.flags & MODE_MASK))
	       {
		  fprintf(stderr, "%s: -%c: mode already specified!\n", v[0], (u_char)ch);
		  exit(1);
	       }
	     opts.flags |= MODE_LISTEN;
	     break;
	     
	   case 'n':
	     opts.flags |= FLAG_NO_REV;
	     break;
	     
	   case 'O':
	     opts.flags |= FLAG_STDOUT;
	     break;
	     
	   case 'p':
	     lport = atoi(optarg);
	     if (lport > USHRT_MAX)
	       {
		  fprintf(stderr, "%s: -%c: invalid local port: %s\n", v[0], (u_char)ch, optarg);
		  exit(1);
	       }
	     break;
	     
	   case 'q':
	     opts.flags |= FLAG_OOBIN;
	     break;

	   case 'R':
	     opts.flags |= FLAG_RAND_LIST;
	     break;
	     
	   case 'r':
	     opts.flags |= FLAG_RAND_SRC;
	     break;
	     
	   case 'S':
	     opts.pshost = (u_char *)optarg;
	     break;
	     
	   case 's':
	     opts.shost = (u_char *)optarg;
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
	     
	   case 't':
	     opts.flags |= FLAG_TELNET;
	     break;
	     
	   case 'u':
	     opts.flags |= FLAG_USE_UDP;
	     break;
	     
	   case 'v':
	     opts.verbosity++;
	     break;
	     
	   case 'w':
	     opts.connect_timeout = atoi(optarg);
	     break;
	     
#ifdef HAVE_SSL
	   case 'X':
	     opts.flags |= FLAG_USE_SSL_P;
	     break;
	     
	   case 'x':
	     opts.flags |= FLAG_USE_SSL_D;
	     break;
#endif
	     
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
	     fprintf(stderr, "please specify an action or destination host\n");
	     exit(1);
	  }
     }

#ifdef HAVE_SSL
   /* if listening, require certificate and key file */
   if ((opts.flags & MODE_MASK) == MODE_LISTEN
       && opts.flags & FLAG_USE_SSL_D
       && (!opts.dcert || !opts.dkey))
     {
	fprintf(stderr, "a valid certificate and key must be specified for SSL listening\n");
	exit(1);
     }
#endif
   
   /* get any addresses that we need */
   if (c < 1 || *v[0] == '\0')
     {
	/* nothing specified, we can assume defaults for LISTEN */
	if ((opts.flags & MODE_MASK) == MODE_LISTEN)
	  opts.lhost = (u_char *)"0";
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
	     if (!nsock_inet_host_has_port(opts.lhost))
	       /* add the port to the listen host (memory leaked) */
	       opts.lhost = (u_char *)strdup((char *)nsock_inet_host(opts.lhost, (u_short)lport));
	  }
	else
	  /* add the port to the source host if one is not specified (memory leaked) */
	  opts.shost = (u_char *)strdup((char *)nsock_inet_host(opts.shost, (u_short)lport));
     }
   
   /* check to see if there is a netcat style port supplied */
   if (c > 0)
     {
	if ((opts.flags & MODE_MASK) != MODE_CONNECT
	    && !nsock_inet_host_has_port(opts.lhost))
	  opts.lhost = (u_char *)strdup((char *)nsock_inet_host_str(opts.lhost, v[0]));
	if ((opts.flags & MODE_MASK) == MODE_CONNECT
	    && !nsock_inet_host_has_port(opts.dhost))
	  opts.dhost = (u_char *)strdup((char *)nsock_inet_host_str(opts.dhost, v[0]));
     }
}


nsock_t *
get_incoming(void)
{
#ifdef INET6
   int family = PF_INET6;
#else
   int family = PF_INET;
#endif
   int flags = NSF_REUSE_ADDR;
   nsock_t *listener, *cli;
   u_int ns_errno;
   int sock_type = SOCK_STREAM;
   
   /* get an incoming connection */
   if (!nsock_inet_host_has_port(opts.lhost) || opts.flags & FLAG_RAND_LIST)
     flags |= NSF_RAND_SRC_PORT;
   if (opts.flags & FLAG_USE_UDP)
     sock_type = SOCK_DGRAM;
   if (opts.flags & FLAG_NO_REV)
     flags |= NSF_NO_REVERSE_NAME;
   if (opts.family != PF_UNSPEC)
     {
	flags |= NSF_USE_FAMILY_HINT;
	family = opts.family;
     }
   if (opts.flags & FLAG_OOBIN)
     flags |= NSF_OOB_INLINE;
   
   if (!(listener = nsock_listen_init(family, sock_type, opts.lhost, 1, flags, &ns_errno)))
     {
	if (opts.verbosity > 0)
	  fprintf(stderr, "error: %s\n", nsock_strerror_full_n(ns_errno));
	return NULL;
     }
   
   /* listen for someone */
   if (flags == NSF_RAND_SRC_PORT || opts.verbosity > 0)
     {
	printf("listening on %s [%s]\n", opts.lhost,
	       reverse_host(listener, &(listener->inet_fin)));
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
	     nsock_free(&listener);
	     return NULL;
	  }
	if (cpid > 0)
	  exit(0);
	/* now only child exists =) */
	if (opts.verbosity < 2)
	  {
	     close(fileno(stdin));
	     if (!(opts.flags & FLAG_STDOUT))
	       close(fileno(stdout));
	     close(fileno(stderr));
	  }
     }
   
   /* for udp connections, we must recv some data before we can 
    * create a "connection-less" circuit
    */
   if ((opts.flags & FLAG_USE_UDP))
     {
	socklen_t slen = sizeof(listener->inet_tin);
	
	if (recvfrom(listener->sd, NULL, 0, MSG_PEEK, 
		     (struct sockaddr *)&(listener->inet_tin), &slen) == -1)
	  {
	     if (opts.verbosity > 0)
	       perror("recvfrom");
	     nsock_free(&listener);
	     return NULL;
	  }
	/* now that we have an address that sent data, we must connect back */
	if (nsock_connect(listener) != NSERR_SUCCESS)
	  {
	     if (opts.verbosity > 0)
	       fprintf(stderr, "udp connect: %s\n", nsock_strerror_full(listener));
	     nsock_free(&listener);
	     return NULL;
	  }
	return listener;
     }
   
   /* get some storage for the incoming client */
   ns_errno = NSERR_SUCCESS;
   if (!(cli = nsock_new(listener->domain, SOCK_STREAM, 0, &ns_errno)))
     {
	if (opts.verbosity > 0)
	  fprintf(stderr, "error: %s\n", nsock_strerror_full_n(ns_errno));
	nsock_free(&listener);
	return NULL;
     }
   if ((opts.flags & FLAG_NO_REV))
     cli->opt |= NSF_NO_REVERSE_NAME;
   
#ifdef HAVE_SSL
   /* setup ssl stuff */
   if (opts.flags & FLAG_USE_SSL_D)
     {
	listener->opt |= NSF_USE_SSL;
	if (opts.dcert)
	  {
	     listener->ns_ssl.cert_file = opts.dcert;
	     if (opts.dkey)
	       listener->ns_ssl.key_file = opts.dkey;
	  }
     }
   
   /* copy the ssl info to the client struct */
   memcpy(&(cli->ns_ssl), &(listener->ns_ssl), sizeof(nsock_ssl_t));
   cli->sd = -1;
#endif
   if ((opts.flags & FLAG_NO_REV))
     listener->opt |= NSF_NO_REVERSE_NAME;
   
   /* wait for their connection */
   if (nsock_accept(listener, cli) != NSERR_SUCCESS)
     {
	if (opts.verbosity > 0)
	  fprintf(stderr, "accept error: %s\n", nsock_strerror_full(listener));
	nsock_free(&listener);
	nsock_free(&cli);
	return NULL;
     }
   
   if (opts.verbosity > 1)
     {
	fprintf(stderr, "connection from [%s] accepted\n",
		reverse_host(cli, &(cli->inet_fin)));
     }
   
   /* dont need listener anymore */
   nsock_free(&listener);
   
   /* zero i/o mode? */
   if (opts.flags & FLAG_ZERO_IO)
     {
	nsock_free(&cli);
	return NULL;
     }
   return cli;
}


nsock_t *
connect_to_host(source, target, pipe_host)
   u_char *source;
   u_char *target;
   u_char pipe_host;
{
#ifdef INET6
   int family = PF_INET6;
#else
   int family = PF_INET;
#endif
   int flags = NSF_REUSE_ADDR;
   nsock_t *dest;
   u_int ns_errno;
   int sock_type = SOCK_STREAM;
   
   if (!source
       || !nsock_inet_host_has_port(source)
       || opts.flags & FLAG_RAND_SRC)
     flags |= NSF_RAND_SRC_PORT;
   
   if (opts.flags & FLAG_USE_UDP)
     sock_type = SOCK_DGRAM;
   if (opts.family != PF_UNSPEC)
     {
	flags |= NSF_USE_FAMILY_HINT;
	family = opts.family;
     }
   if (opts.flags & FLAG_OOBIN)
     flags |= NSF_OOB_INLINE;
   
   /* try to connect to the desintation */
   if (!(dest = nsock_connect_init(family, sock_type, source, target, flags, &ns_errno)))
     {
	if (opts.verbosity > 0)
	  fprintf(stderr, "error: %s\n", nsock_strerror_full_n(ns_errno));
	return NULL;
     }
   
#ifdef HAVE_SSL
   if (pipe_host)
     {
	/* setup ssl stuff */
	if (opts.flags & FLAG_USE_SSL_P)
	  {
	     dest->opt |= NSF_USE_SSL;
	     if (opts.pcert)
	       {
		  dest->ns_ssl.cert_file = opts.pcert;
		  if (opts.pkey)
		    dest->ns_ssl.key_file = opts.pkey;
	       }
	  }
     }
   else
     {
	/* setup ssl stuff */
	if (opts.flags & FLAG_USE_SSL_D)
	  {
	     dest->opt |= NSF_USE_SSL;
	     if (opts.dcert)
	       {
		  dest->ns_ssl.cert_file = opts.dcert;
		  if (opts.dkey)
		    dest->ns_ssl.key_file = opts.dkey;
	       }
	  }
     }
#endif
   if ((opts.flags & FLAG_NO_REV))
     dest->opt |= NSF_NO_REVERSE_NAME;
   
   /* set other options */
   dest->connect_timeout = opts.connect_timeout;
   
   /* try to connect */
   if (nsock_connect_out(dest) != NSERR_SUCCESS)
     {
	if (opts.verbosity > 0)
	  fprintf(stderr, "error: %s\n", nsock_strerror_full(dest));
	return NULL;
     }
   
   /* possibly tell our outgoing info */
   if (opts.verbosity > 1)
     {
	char fmt_buf[128];
	char dhost_buf[1024 + 1], shost_buf[1024 + 1];
	
	strcpy(fmt_buf, "connection to %s ");
	snprintf(dhost_buf, sizeof(dhost_buf) - 1, "%s [%s]",
		 dest->inet_to,
		 reverse_host(dest, &(dest->inet_tin)));
	dhost_buf[sizeof(dhost_buf) - 1] = '\0';
	
	if (source)
	  {
	     strcat(fmt_buf, "from %s ");
	     snprintf(shost_buf, sizeof(shost_buf) - 1, "%s [%s]",
		      dest->inet_from,
		      reverse_host(dest, &(dest->inet_fin)));
	     shost_buf[sizeof(shost_buf) - 1] = '\0';
	  }
	strcat(fmt_buf, "established\n");
	fprintf(stderr, fmt_buf, dhost_buf, shost_buf);
     }

   if (opts.flags & FLAG_ZERO_IO)
     {
	nsock_close(dest);
	return NULL;
     }
   return dest;
}

   

u_char *
reverse_host(ns, cli)
   nsock_t *ns;
   struct sockaddr_storage *cli;
{
   static char host_rev[2048 + 1];
   
   /* resolve possibly */
   if (nsock_inet_resolve_rev(ns, cli, (u_char *)host_rev, sizeof(host_rev) - 1) != NSERR_SUCCESS)
     strcpy(host_rev, "unknown:?");
   host_rev[sizeof(host_rev) - 1] = '\0';
   return (u_char *)host_rev;
}


/*
 * execute a program as a child with a pipe going to it
 */
int
exec_prog(to, from, cpid)
   int *to, *from;
   pid_t *cpid;
{
   u_int i;
   int pipe_to[2], pipe_from[2];
   
   /* open the pipes (unidirectional pipes suck) */
   if (pipe(pipe_to) == -1)
     {
	if (opts.verbosity > 0)
	  perror("to pipe creation failed");
	return -1;
     }
   if (pipe(pipe_from) == -1)
     {
	if (opts.verbosity > 0)
	  perror("from pipe creation failed");
	close(pipe_to[0]);
	close(pipe_to[1]);
	return -1;
     }
   
   /* fork off for child execution */
   *cpid = fork();
   if (*cpid == -1)
     {
	if (opts.verbosity > 0)
	  perror("fork failed");
	for (i = 0; i < 2; i++)
	  {
	     close(pipe_to[i]);
	     close(pipe_from[i]);
	  }
	return -1;
     }
   
   /* the child shall now exec the program */
   if (*cpid == 0)
     {
	if (opts.verbosity > 1)
	  fprintf(stderr, "executing %s\n", opts.pprog);
		  
	dup2(pipe_to[0], fileno(stdin));
	dup2(pipe_from[1], fileno(stdout));
	dup2(pipe_from[1], fileno(stderr));
	
	for (i = 0; i < 1; i++)
	  {
	     close(pipe_to[i]);
	     close(pipe_from[i]);
	  }
	
	/* possibly allow args passing at some point */
	execlp((char *)opts.pprog, (char *)opts.pprog, NULL);
	return -1;
     }
   
   /* the parent simply closes the child sides of the pipes and returns */
   close(pipe_to[0]);
   close(pipe_from[1]);

   *to = pipe_to[1];
   *from = pipe_from[0];
   return 0;
}
