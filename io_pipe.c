/*
 * pipe data between 2 end points..
 *
 * Copyright (C) 2002,2004 Joshua J. Drake <libnsock@qoop.org>
 */

#include <nsock/nsock.h>
#include <nsock/errors.h>

#include "nsc.h"
#include "io_pipe.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define TELOPTS
#define TELCMDS
#include <arpa/telnet.h>


/* lil struct for telnet stuff */
typedef struct __nsc_telnet_opt_stru
{
   u_char iac;
   u_char cmd;
   u_char opt;
} telnet_t;


/* a little struct to make things look a bit better */
typedef struct __nsc_io_pipe_buf_stru
{
   u_char *buf;
   size_t len;
} iobuf_t;


/* some helpers */
static void io_pipe_decide_desc(nsock_t *, int *, int *, nsock_t *, int *, int *, int *);
static void io_pipe_setfds(fd_set *, fd_set *, size_t, size_t, int, int, int, int);
static ssize_t io_pipe_buf_append(nsock_t *, int, iobuf_t *, int, iobuf_t *, u_char);
static ssize_t io_pipe_buf_flush(nsock_t *, int, iobuf_t *, u_char);

/*
 * use a select() loop to act just as netcat does...
 */
int
nsc_io_pipe(ns1, in1_sd, out1_sd, 
	      ns2, in2_sd, out2_sd, iop_opts)
   nsock_t *ns1;
   int in1_sd, out1_sd;
   nsock_t *ns2;
   int in2_sd, out2_sd;
   u_char iop_opts;
{
   fd_set rd, wr;
   iobuf_t local = { NULL, 0 };
   iobuf_t remote = { NULL, 0 };
   int sret, high_desc;
   ssize_t len;
   
   /* decide which descriptors to use */
   io_pipe_decide_desc(ns1, &in1_sd, &out1_sd,
		       ns2, &in2_sd, &out2_sd,
		       &high_desc);

   /* get some heap memory for our buffers */
   if (!(local.buf = calloc(1, NSOCK_IOP_BLOCKSZ))
       || !(remote.buf = calloc(1, NSOCK_IOP_BLOCKSZ)))
     {
	if (local.buf)
	  free(local.buf);
	return NSERR_OUT_OF_MEMORY;
     }
   
   /* loop until there is a problem ... */
   while (1)
     {
	io_pipe_setfds(&rd, &wr,
		       remote.len, local.len,
		       in1_sd, out1_sd, 
		       in2_sd, out2_sd);
	
	/* wait forever for something to happen.. */
	sret = select(high_desc, &rd, &wr, NULL, NULL);
	if (sret == -1)
	  {
	     if (ns1)
	       ns1->ns_errno = NSERR_IOP_SELECT_FAILED;
	     if (ns2)
	       ns2->ns_errno = NSERR_IOP_SELECT_FAILED;
	     return -1;
	  }
	
	/* ok to send stuff to remote? */
	if (FD_ISSET(out1_sd, &wr))
	  {
	     len = io_pipe_buf_flush(ns1, out1_sd, &local, iop_opts);
	     if (len < 0)
#ifdef DEBUG_PIPE_BUFS
	       {
		  fprintf(stderr, "local %u remote %u\n", local.len, remote.len);
#endif
		  return len;
#ifdef DEBUG_PIPE_BUFS
	       }
#endif
	     if (!(--sret))
	       continue;
	  }
	
	/* ok to send stuff to local? */
	if (FD_ISSET(out2_sd, &wr))
	  {
	     len = io_pipe_buf_flush(ns2, out2_sd, &remote, iop_opts);
	     if (len < 0)
#ifdef DEBUG_PIPE_BUFS
	       {
		  fprintf(stderr, "local %u remote %u\n", local.len, remote.len);
#endif
		  return len;
#ifdef DEBUG_PIPE_BUFS
	       }
#endif
	     if (!(--sret))
	       continue;
	  }
	
	/* stuff coming from remote? */
	if (FD_ISSET(in1_sd, &rd))
	  {
	     len = io_pipe_buf_append(ns1, in1_sd, &remote, out1_sd, &local, iop_opts);
	     if (len < 0)
#ifdef DEBUG_PIPE_BUFS
	       {
		  fprintf(stderr, "local %u remote %u\n", local.len, remote.len);
#endif
		  return len;
#ifdef DEBUG_PIPE_BUFS
	       }
#endif
	     if (!(--sret))
	       continue;
	  }
	
	/* stuff coming from local? */
	if (FD_ISSET(in2_sd, &rd))
	  {
	     len = io_pipe_buf_append(ns2, in2_sd, &local, -1, NULL, iop_opts);
	     if (len < 0)
#ifdef DEBUG_PIPE_BUFS
	       {
		  fprintf(stderr, "local %u remote %u\n", local.len, remote.len);
#endif
		  return len;
#ifdef DEBUG_PIPE_BUFS
	       }
#endif
	     if (!(--sret))
	       continue;
	  }
		
	if (sret > 0)
	  fprintf(stderr, "%s: %d unhandled sockets!\n", __func__, sret);
     }
   
   free(local.buf);
   free(remote.buf);
   return NSERR_SUCCESS;
}


/*
 * decide which descriptors to use
 */
static void
io_pipe_decide_desc(ns1, in1p, out1p,
		    ns2, in2p, out2p,
		    highp)
   nsock_t *ns1, *ns2;
   int *in1p, *out1p, *in2p, *out2p, *highp;
{
   /* use the nsock descriptors if set */
   if (ns1)
     *in1p = ns1->sd;
   if (ns2)
     *in2p = ns2->sd;
   
   /* if output sockets were not specified, assume they are the same
    * as the input sockets */
   if (*out1p < 0)
     *out1p = *in1p;
   if (*out2p < 0)
     *out2p = *in2p;

   /* figure out our highest descriptor */
   *highp = *out2p;
   if (*in1p > *highp)
     *highp = *in1p;
   if (*out1p > *highp)
     *highp = *out1p;
   if (*in2p > *highp)
     *highp = *in2p;
   *highp += 1;
   
   /* ok everything is set */
}
   

/*
 * decide which descriptors to select on
 */
static void
io_pipe_setfds(rd, wr,
	       rbl, lbl, 
	       in1_sd, out1_sd, 
	       in2_sd, out2_sd)
   fd_set *rd, *wr;
   size_t rbl, lbl;
   int in1_sd, out1_sd;
   int out2_sd, in2_sd;
{
   /* reset the fd_sets.. */
   FD_ZERO(rd);
   FD_ZERO(wr);
   
   /* want to read more (if buffer size allows) from remote */
   if (rbl >= 0 && rbl < NSOCK_IOP_BLOCKSZ)
     FD_SET(in1_sd, rd);
   
   /* want to read more (if buffer size allows) from local */
   if (lbl >= 0 && lbl < NSOCK_IOP_BLOCKSZ)
     FD_SET(in2_sd, rd);
   
   /* need to write to remote? */
   if (lbl)
     FD_SET(out1_sd, wr);
   
   /* need to write to local? */
   if (rbl)
     FD_SET(out2_sd, wr);
}



/*
 * append to the end of a buffer..
 */
ssize_t
io_pipe_buf_append(ns, sd, io, osd, oio, opts)
   nsock_t *ns;
   int sd, osd;
   iobuf_t *io, *oio;
   u_char opts;
{
   ssize_t len;

   /* append to the buffer, possibly fill it */
#ifdef HAVE_SSL
   if (ns && ns->opt & NSF_USE_SSL)
     len = SSL_read(ns->ns_ssl.ssl, io->buf + io->len,
		    NSOCK_IOP_BLOCKSZ - io->len);
   else
#endif
     len = read(sd, io->buf + io->len,
		NSOCK_IOP_BLOCKSZ - io->len);
   switch (len)
     {
      case -1:
	if (ns)
	  return nsock_error(ns, NSERR_READ_ERROR);
	return -1;
	break;
	
      case 0:
	if (ns)
	  return nsock_error(ns, NSERR_READ_EOF);
	return -1;
	break;
	
      default:
	io->len += len;
	
	/* if we are dealing with telnet stuff look for some options */
	if (osd != -1 && oio && opts & NSCIOP_ACK_TELNET)
	  {
	     u_char *p, *op;
#ifdef DEBUG_TELNET
	     telnet_t tel;
#endif
	     telnet_t tout;
	     u_int new_len = 0;
	     
	     p = io->buf;
	     while ((p = memchr(p, IAC, io->len - (p - io->buf))))
	       {
		  tout.iac = IAC;
		  tout.cmd = 0;
		  switch (*(p + 1))
		    {
		     case WILL:
		     case WONT:
		       tout.cmd = DONT;
		       break;
		     case DO:
		     case DONT:
		       tout.cmd = WONT;
		       break;
		    }
		  
		  /* remember where we found the IAC */
		  if (tout.cmd != 0)
		    {
		       tout.opt = *(p + 2);
		       op = p;
#ifdef DEBUG_TELNET
		       memcpy(&tel, p, 3);
#endif
		       p += 3;
		       
#ifdef DEBUG_TELNET
		       fprintf(stderr, "tel: %s %s %s\n", TELCMD(tel.iac),
			       TELCMD(tel.cmd), TELOPT(tel.opt));
		       fprintf(stderr, "tout: %s %s %s\n", TELCMD(tout.iac),
			       TELCMD(tout.cmd), TELOPT(tout.opt));
#endif
		       
		       /* shift data back */
		       new_len = io->len - (p - io->buf);
		       if (new_len > 0)
			 memmove(op, p, io->len - (p - io->buf));
		       p -= 3;
		       io->len -= 3;
		       len -= 3;
		       
		       /* add to output buffer */
		       if (oio->len + 3 < NSOCK_IOP_BLOCKSZ)
			 {
			    memcpy(oio->buf + oio->len, &tout, 3);
			    oio->len += 3;
			 }
		       else
			 {
			    /* write it out directly if no room in buffer */
			    write(osd, &tout, 3);
			 }
		    }
	       }
	  }
	break;
     }
   /* ok.. we success full read the stuf... */
   return len;
}


/*
 * flush the buffer and reset it..
 */
ssize_t
io_pipe_buf_flush(ns, sd, io, opts)
   nsock_t *ns;
   int sd;
   iobuf_t *io;
   u_char opts;
{
   ssize_t len;

#ifdef HAVE_SSL
   if (ns && ns->opt & NSF_USE_SSL)
     len = SSL_write(ns->ns_ssl.ssl, io->buf, io->len);
   else
#endif
     len = write(sd, io->buf, io->len);
   switch (len)
     {
      case -1:
	if (ns)
	  return nsock_error(ns, NSERR_WRITE_ERROR);
	return -1;
	break;
	
      case 0:
	if (ns)
	  return nsock_error(ns, NSERR_WRITE_EOF);
	return -1;
	break;
	
      default:
	if (len < io->len)
	  {
	     /* move the data that remains to the beginning of the buffer */
	     memmove(io->buf, io->buf + len, io->len - len);
	     io->len -= len;
	  }
	else
	  io->len = 0;
	break;
     }
   
   /* possibly additionally write the buffer to stdout
    * (not if already writing to stdout)
    */
   if (opts & NSCIOP_STDOUT_TOO
       && sd != fileno(stdout))
     write(fileno(stdout), io->buf, len);
   
   /* reset buffer.. */
   if (io->len == 0)
     memset(io->buf, 0, NSOCK_IOP_BLOCKSZ);
   return len;
}
