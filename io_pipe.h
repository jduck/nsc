#ifndef __nsc_io_p_h
#define __nsc_io_p_h

#define NSCIOP_ACK_TELNET 	0x01
#define NSCIOP_STDOUT_TOO 	0x02

int nsc_io_pipe(nsock_t *, int, int, nsock_t *, int, int, u_char);

#endif
