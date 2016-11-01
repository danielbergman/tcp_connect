/*  
 *  tcp_connect.c v0.99
 *  A simple utility for checking connections to generic TCP servers
 *
 *  Daniel Bergman 2004-04-27
 *  sweden, d-b@home.se
 *
 *  History
 *  -------
 *  v0.982  DaBe  Added memset of socket structure, removed some newlines, corrected some spelling
 *  v0.99   DaBe  Replaced errno with h_errno for resolv operations, 
 *
 *  TODO: 
 *   * possibly use getopt for future complex cmdparsing
 *   * bzero struct before using it
 *   * verify that gethostbyname *really* works with IP-addr as argument
 *   * implement service-name lookup e.g. tcp_connect <host/IP> <service/port> (timeout)
 */


/*
 *
 *   H  E  A  D  E  R  S
 *
 */

#include <stdio.h>                              /* atoi */
#include <unistd.h>                             /* close */
#include <stdlib.h>                             /* sprintf */
#include <sys/socket.h>                         /* socket, connect, setsockopt */
#include <netdb.h>                              /* gethostbyname, hostent */
#include <errno.h>                              /* h_errno, errno */
#include <string.h>                             /* memcpy */
#include <fcntl.h>                              /* fcntl */
#include <sys/select.h>                         /* select, FD_macros */
#include <time.h>                               /* timeval */


/*
 *
 *   D  E  F  I  N  I  T  I  O  N  S
 *
 */


/* just for the beauty of it */
#define ERROR                       -1
#define ZERO                         0

/* define to debug, undefine not to 
#define DEBUG */

/* program information */
#define PROGRAM_NAME                "tcp_connect"
#define PROGRAM_VERSION             "v0.99"
#define PROGRAM_AUTHOR              "Daniel Bergman"
#define PROGRAM_DATE                "2004-04-27"

/* default timeout value, sec */
#define DEFAULT_TIMEOUT              10


/*
 *
 *   G  L  O  B  A  L
 *
 */

/* contains id of last error */
extern int errno;
extern int h_errno;


/*
 *
 *   P  R  O  T  O  T  Y  P  E  S
 *
 */

/* print usage, takes running binary name as argument */
void print_usage(char *binary_name);


/*
 *
 *   M  A  I  N
 *
 */

int main(int argc, char **argv)
{
    fd_set read_fdset, write_fdset;                           /* file-descriptor sets for reading and checking status on socket */
    struct sockaddr_in server_sock;                           /* structure with server info */
    struct hostent *host_entry = NULL;                        /* structure with server IP-addr */
    struct timeval time_struct;                               /* time structure used for timeout value */
    int    timeout = ZERO;                                    /* seconds before timeout */
    int    socket_flags = ZERO;                               /* socket flags/options */
    int    socket_error = ZERO;                               /* where getsockopt stores error */
    int    sizeof_socket_error = (int) sizeof(socket_error);  /* sizeof error-variable */
    int    port = ZERO;                                       /* destination server port */
    int    socket_handle = ZERO;                              /* handle to socket */



    /*
     *     S  A  N  I  T  Y      C  H  E  C  K  S
     */


    /* sanity check - the two first arguments are mandantory, third one, timeout, is optional */
    if (argc < 3 || argc > 4) {

       print_usage(argv[0]);
       return(EXIT_FAILURE);  
    }



    /*
     *     I  N  I  T
     */

    /* retrieve port integer */
    if ((port = atoi(argv[2])) == ZERO) {

       fprintf(stderr, "ERROR: Invalid port: %s\n", argv[2]);
       print_usage(argv[0]);
       exit(EXIT_FAILURE);  
    }

    /* retrieve timeout integer */
    if (argc == 4) {
       if ((timeout = atoi(argv[3])) == ZERO) {
          fprintf(stderr, "ERROR: Invalid timeout: %s, using default timeout: %d\n", argv[3], DEFAULT_TIMEOUT);
          timeout = DEFAULT_TIMEOUT;
       } 
    }  else {
#ifdef DEBUG
       fprintf(stderr, "DEBUG: No timeout value found on cmdline, using default: %d\n", DEFAULT_TIMEOUT);
#endif
       timeout = DEFAULT_TIMEOUT;
    }

    /* resolv hostname to IP-addr */
    if ((host_entry = gethostbyname(argv[1])) == NULL) {
    
       fprintf(stderr, "ERROR: Unable to resolve host: %s h_errno=%d (%s)\n", argv[1], h_errno, (h_errno > ZERO) ? hstrerror(h_errno) : "");
       exit(EXIT_FAILURE);

    }  else {
#ifdef DEBUG
       fprintf(stderr, "DEBUG: Successfully resolved host or IP-addr\n");
#endif
	}

    /* init socket */
    if ((socket_handle = socket(AF_INET, SOCK_STREAM, ZERO)) == ERROR) {
    
       fprintf(stderr, "ERROR: Unable to initialize socket, errno=%d (%s)\n", errno, (errno > ZERO) ? strerror(errno) : "");
       exit(EXIT_FAILURE);
    }

    /* init socket with server info */
    memset(&server_sock, ZERO, sizeof(struct sockaddr_in));
    server_sock.sin_family = AF_INET;
    server_sock.sin_port = htons((uint16_t) port);
    memcpy(&server_sock.sin_addr.s_addr, (struct in_addr *) host_entry->h_addr, sizeof(host_entry->h_addr));

    /* retrieve current socket flags */
    if ((socket_flags = fcntl(socket_handle, F_GETFL, ZERO)) == ERROR) {

       fprintf(stderr, "ERROR: Unable to retrieve current socket flags, errno=%d (%s)\n", errno, (errno > ZERO) ? strerror(errno) : "");
       exit(EXIT_FAILURE);
    } 

    /* make socket non_blocking by adding O_NONBLOCK to socket flags */
    if (fcntl(socket_handle, F_SETFL, socket_flags | O_NONBLOCK) == ERROR) {

       fprintf(stderr, "ERROR: Unable to setup non_blocking socket. errno=%d (%s)\n", errno, (errno > ZERO) ? strerror(errno) : "");
       exit(EXIT_FAILURE);
    }


    /* reset/clear fd-set */
    FD_ZERO(&read_fdset);

    /* add socket_handle to fd-set */
    FD_SET(socket_handle, &read_fdset);

    /* "copy" read_fdset into write_fdset */
    write_fdset = read_fdset;

    /* specify timeout value */
    time_struct.tv_sec  = timeout;
    time_struct.tv_usec = ZERO;



    /*
     *     M  A  I  N      P  R  O  G  R  A  M
     */

    
    /* initiate nonblocking connect to server */
    if ((connect(socket_handle, (struct sockaddr *) &server_sock, sizeof(server_sock))) < ZERO) {
       if (errno == EINPROGRESS) {
#ifdef DEBUG
          fprintf(stderr, "DEBUG: Successfully initiated non_blocking connect to host: %s on port: %d, timeout:%d\n", argv[1], port, timeout);
#endif
          /* reset errno just to be sure - otherwise code later on might be confused by EINPROGRESS (if errno been untouched) */
          errno = ZERO;

       }  else {
    
          fprintf(stderr, "ERROR: Unable to connect to host: %s on port: %d, timeout: %d, errno=%d (%s)\n", argv[1], port, timeout, errno, 
			                                                                                                (errno > ZERO) ? strerror(errno) : "");
          exit(EXIT_FAILURE);
       }
    
    }  else { /* fast answer - localhost maybe? */
    
       fprintf(stdout, "Successfully connected to host: %s on port: %d\n", argv[1], port);
       close(socket_handle);
       exit(EXIT_SUCCESS);
    }


    /* wait for data on socket during timeout period */
    if (select(socket_handle + 1, &read_fdset, &write_fdset, NULL, &time_struct) == ZERO) {

       fprintf(stderr, "ERROR: Unable to connect, timed out, to host: %s on port: %d, timeout: %d, errno=%d (%s)\n", argv[1], port, timeout, errno, 
		                                                                                                             (errno > ZERO) ? strerror(errno) : "");
       close(socket_handle);
       exit(EXIT_FAILURE);

    }  else {
#ifdef DEBUG
       fprintf(stderr, "DEBUG: Found information on socket to host: %s on port: %d, timeout:%d\n", argv[1], port, timeout);
#endif
    }


    /* if socket if readable or writable */
    if (FD_ISSET(socket_handle, &read_fdset) || (FD_ISSET(socket_handle, &write_fdset))) {

       /* get possible error from pending connect() */
       if ((getsockopt(socket_handle, SOL_SOCKET, SO_ERROR, &socket_error, &sizeof_socket_error) == ERROR) || (socket_error != ZERO)) {

          fprintf(stderr, "ERROR: Unable to connect to host: %s on port: %d, timeout: %d, errno=%d (%s)\n", argv[1], port, timeout, errno, 
		                                                                                                    (errno > ZERO) ? strerror(errno) : "");
          close(socket_handle);
          exit(EXIT_FAILURE);

       }  else {  /* looks good */

          fprintf(stdout, "Successfully connected to host: %s on port: %d\n", argv[1], port);
          close(socket_handle);
          exit(EXIT_SUCCESS);
       }

    }  else {

       fprintf(stderr, "ERROR: Unable to check status of socket to host: %s on port: %d, timeout: %d, errno=%d (%s)\n", argv[1], port, timeout, errno, 
		                                                                                                                  (errno > ZERO) ? strerror(errno) : "");
       close(socket_handle);
       exit(EXIT_FAILURE);
    }


    /* close socket */
    close(socket_handle);
    
    /* return to OS */
    return(EXIT_SUCCESS);

}    


/* function which prints usage, 
 * takes name of running binary as argument
 */
void print_usage(char *binary_name)
{

    /* print usage */
    (void) fprintf(stderr, "A simple utility for checking connections to generic TCP servers\n");
    (void) fprintf(stderr, "%s %s %s %s\n", PROGRAM_NAME, PROGRAM_VERSION, PROGRAM_AUTHOR, PROGRAM_DATE);
    (void) fprintf(stderr, "Usage: %s [host] [port] (timeout)\n", binary_name);
    (void) fprintf(stderr, "Example:\n\n");
    (void) fprintf(stderr, " # %s 172.16.10.13 22 \n\n", binary_name);
    (void) fprintf(stderr, " # %s pseudo 8888 5 \n\n", binary_name);

}
