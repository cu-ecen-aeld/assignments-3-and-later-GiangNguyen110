/*
** aesdsocket.c -- Native Socket Server Demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h> // for openlog, syslog, closelog, LOG_ERR, LOG_INFO, etc.
#include <stdarg.h>

#define PORT "9000"  // the port users will be connecting to

#define BACKLOG 10   // how many pending connections queue will hold

#define MAXBUFLEN 1024

#define FILEPATH "/var/tmp/aesdsocketdata"

// Flag set when SIGINT/SIGTERM received
volatile sig_atomic_t stop_server = 0;

// Flag set when activating deamon mode
int active_deamon = 0;

/* ------------ Logging function ------------ */
void log_message(int priority, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    if (active_deamon) {
        vsyslog(priority, fmt, args);
    } else {
        vprintf(fmt, args);
        printf("\n");
    }

    va_end(args);
}

/* ------------ Daemonize function ------------ */
void daemon_mode() {
    pid_t pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "Fork failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        exit(EXIT_SUCCESS); // parent exiting
    }

    // child process
    if (setsid() < 0) {
        syslog(LOG_ERR, "Failed to create a new session: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // fork again to fully detach
    pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    // redirect std streams to /dev/null
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_RDWR);
    open("/dev/null", O_RDWR);

    // Reset file mode creation mask
    umask(0);
    // Change working directory to root
    if (chdir("/") == -1) {
        perror("chdir");
        exit(EXIT_FAILURE);
    }

    // Open syslog
    openlog("aesdsocket", LOG_PID, LOG_DAEMON);
}

/* ------------ Signal handler ------------ */
void sigchld_handler(int s)
{
    (void)s; // quiet unused variable warning

    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

// SIGINT and SIGTERM handler: request shutdown
void termination_handler(int signum)
{
    (void)signum;
    stop_server = 1;
}

/* ------------ Helper: get sockaddr IPv4 or IPv6 ------------ */ 
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/* ------------ Main ------------ */
int main(int argc, char *argv[])
{
    // listen on sock_fd, new connection on new_fd
    int sockfd, new_fd;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address info
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    int numbytes;
    char buf[MAXBUFLEN];
    ssize_t recv_data;
    // int active_deamon = 0;

    // --- Check if deamon mode is chosen ---
    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        active_deamon = 1;
    }

    // --- Setup hints for getaddrinfo ---
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // AF_INET here requests IPv4 only
    hints.ai_socktype = SOCK_STREAM; // SOCK_STREAM = TCP (stream)
    hints.ai_flags = AI_PASSIVE; // use my IP

    // Opens a stream socket bound to port 9000, failing and
    // returning -1 if any of the socket connection steps fail.
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            log_message(LOG_ERR, "server: socket: %s", strerror(errno));
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            log_message(LOG_ERR, "setsockopt: %s", strerror(errno));
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            log_message(LOG_ERR, "server: bind: %s", strerror(errno));
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        log_message(LOG_ERR, "server: failed to bind");
        exit(EXIT_FAILURE);
    }

    // listen to connection
    if (listen(sockfd, BACKLOG) == -1) {
        log_message(LOG_ERR, "listen: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // --- If running in deamon mode ----
    if (active_deamon) {
        daemon_mode();
        // Open syslog for daemon logging
        // You can find those in /var/log/syslog (usually) or by command 'journalctl -t aesdsocket -f'
        log_message(LOG_INFO, "Daemon started successfully");
    }

    // --- Install signal handlers ---
    sa.sa_handler = sigchld_handler; // Reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        // perror("sigaction");
        log_message(LOG_ERR, "sigaction: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // --- Gracefully terminate with SIGTERM and SIGINT ---
    struct sigaction term_action;
    term_action.sa_handler = termination_handler;
    sigemptyset(&term_action.sa_mask);
    term_action.sa_flags = 0;
    if (sigaction(SIGINT, &term_action, NULL) == -1 ||
        sigaction(SIGTERM, &term_action, NULL) == -1)
    {
        log_message(LOG_ERR, "sigaction termination: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    log_message(LOG_INFO, "listener: waiting for connections...");

    // --- Main accept loop ---
    while(!stop_server) {
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);

        if (new_fd == -1) {
            if(stop_server) {
                // accept interrupted by SIGINT/SIGTERM → exit loop
                log_message(LOG_INFO, "\nCaught signal, exiting");
                break;
            }
            log_message(LOG_ERR, "accept: %s", strerror(errno));
            continue;
        }

        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  s, 
                  sizeof s);
        log_message(LOG_INFO, "server: accepted connection from %s", s);

        if (!fork()) { 
            // this is the child process
            close(sockfd); // child doesn't need the listener

#ifdef DEBUG
            // === For testing only ===
            (void)recv_data;
            while((numbytes = recv(new_fd, buf, MAXBUFLEN, 0)) > 0) {
                buf[numbytes] = '\0';
                log_message(LOG_INFO, "Received: %s size %d", buf, numbytes);
            }
#else
            (void)numbytes;
            // --- Open file for reading and writing --- 
            // create if it doesn't exist; set permissions to 0644
            int fd = open(FILEPATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if(fd < 0) {
                log_message(LOG_ERR, "open file: %s", strerror(errno));
                break;
            }

            // --- Receive data in chunks ---
            while((recv_data = recv(new_fd, buf, MAXBUFLEN, 0)) > 0) {
                log_message(LOG_INFO, "Received data!");
                write(fd, buf, recv_data);

                // check for end of packet based on '\n' as the delimiter
                if (buf[recv_data - 1] == '\n') {
                    break;
                }
            }
            close(fd);

            // --- Read all content in file ---
            fd = open(FILEPATH, O_RDONLY);
            if(fd < 0) {
                log_message(LOG_ERR, "open file: %s", strerror(errno));
                break;
            }

            // --- Send back the entire file ---
            while((recv_data = read(fd, buf, MAXBUFLEN)) > 0) {
                log_message(LOG_INFO, "Echo back to client");
                send(new_fd, buf, recv_data, 0);
            }
            close(fd);
#endif 
            close(new_fd);
            exit(EXIT_SUCCESS);
        }
        close(new_fd);  // parent doesn't need this
    }

    // --- Cleanup on termination ---
    log_message(LOG_INFO, "server: caught signal, exiting...");
    log_message(LOG_INFO, "server: closed connection from %s", s);
    if(sockfd != -1)
        close(sockfd);

    if(remove(FILEPATH) == 0)
        log_message(LOG_INFO, "Delete file %s", FILEPATH);
    else
        log_message(LOG_ERR, "remove: %s", strerror(errno));

    if(active_deamon) {
        log_message(LOG_INFO, "Daemon shutting down!");
        closelog();
    }

    return 0;
}