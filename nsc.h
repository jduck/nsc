/*
 * stuff needed by both nsc.c and the modified io_pipe.c
 */
#ifndef __nsc_h
#define __nsc_h

#define MODE_CONNECT 	0x00000001
#define MODE_LISTEN 	0x00000002
#define MODE_MASK 	0x0000000f

#define FLAG_FORK 	0x00000010
#define FLAG_RAND_LIST 	0x00000020
#define FLAG_RAND_SRC 	0x00000040
#define FLAG_ZERO_IO 	0x00000080
#define FLAG_DATAPIPE 	0x00000100
#define FLAG_USE_SSL_D  0x00000200
#define FLAG_USE_SSL_P  0x00000400
#define FLAG_NO_REV 	0x00000800
#define FLAG_TELNET 	0x00001000
#define FLAG_STDOUT 	0x00002000
#define FLAG_EXECPIPE 	0x00004000
#define FLAG_USE_UDP 	0x00008000
#define FLAG_OOBIN 	0x00010000
#define FLAG_MASK 	0xfffffff0

typedef struct __options_stru_
{
   int family;
   u_long flags;
   
   u_char *lhost; 		/* host to listen on */

   u_char *shost; 		/* src for outgoing connect */
   u_char *dhost; 		/* host to connect to */
   
   u_char *pshost; 		/* src for connect to pipe host */
   union 
     {
	u_char *phost; 		/* host to pipe data with */
	u_char *pprog; 		/* program to pipe data with */
     } pipe_un;
#define phost pipe_un.phost
#define pprog pipe_un.pprog
   
#ifdef HAVE_SSL
   u_char *dcert;
   u_char *dkey;
   u_char *pcert;
   u_char *pkey;
#endif
   
   u_int connect_timeout;
   u_int verbosity;
} options_t;


extern options_t opts;

#endif
