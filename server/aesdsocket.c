/**
 * @file    aesdsocket.c
 * @brief   TCP server for AESD Assignment 5 (Part 1)
 *
 * This program implements a simple TCP stream server that:
 *   - Listens on port 9000 (IPv4)
 *   - Accepts multiple client connections concurrently (one thread per client)
 *   - Receives newline-terminated packets from clients
 *   - Appends each packet to a persistent data store
 *       (either /var/tmp/aesdsocketdata or /dev/aesdchar if enabled)
 *   - Sends back the entire contents of the data store after each packet
 *   - Logs accepted/closed connections using syslog
 *   - Supports SIGINT/SIGTERM clean shutdown with file removal
 *   - Supports '-d' flag to run as a background daemon (after bind succeeds)
 *
 * This implementation is designed to pass sockettest.sh validation
 * from the University of Colorado Boulder AESD course.
 *
 * @author  Bhavya Saravanan
 * @date    28/09/2025
 */


#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/queue.h>

/* ---- config ---- */
#define USE_CHAR_DEVICE     0          /* 0=/var/tmp file; 1=/dev/aesdchar */
#define ENABLE_TIMESTAMP    0          
#define TCP_PORT            "9000"
#define BACKLOG             10
#define RX_CHUNK            1024

#if USE_CHAR_DEVICE
  #define DATA_PATH         "/dev/aesdchar"
  #define SEEK_PREFIX       "AESDCHAR_IOCSEEKTO:"
  #include "../aesd-char-driver/aesd_ioctl.h"
#else
  #define DATA_PATH         "/var/tmp/aesdsocketdata"
#endif

/* ---- globals ---- */
static volatile sig_atomic_t g_stop = 0;
static int g_listen_fd = -1;

/* per-connection node */
typedef struct client_node {
    pthread_t tid;
    int cfd;
    bool done;
    pthread_mutex_t *file_mx;
    SLIST_ENTRY(client_node) link;
} client_node_t;

/* ---- signals ---- */
static void sig_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        g_stop = 1;
        syslog(LOG_INFO, "Caught signal, exiting");
        if (g_listen_fd >= 0)
         shutdown(g_listen_fd, SHUT_RDWR); 
    }
}
static void install_signals(void) {
    struct sigaction sa; memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler; 
    if (sigaction(SIGINT,  &sa, NULL) == -1) 
      syslog(LOG_ERR, "sigaction(SIGINT): %s", strerror(errno));
    if (sigaction(SIGTERM, &sa, NULL) == -1) 
      syslog(LOG_ERR, "sigaction(SIGTERM): %s", strerror(errno));
}

/* ---- daemonize (after bind) ---- */
static int daemonize_after_bind(void) {
    pid_t pid = fork(); 

    if (pid < 0){ 
      syslog(LOG_ERR, "Forking failed");
      return -1; 
    }
    if (pid > 0) 
      exit(EXIT_SUCCESS);
    if (setsid() == -1){ 
       syslog(LOG_ERR, "Creating new session failed"); 
       return -1;
     }
    
    if (chdir("/") == -1) {
       syslog(LOG_ERR, "Changing working directory failed");
       return -1;
    }
    int dn = open("/dev/null", O_RDWR); 
    if (dn == -1) 
       return -1;
    (void)dup2(dn, STDIN_FILENO); 
    (void)dup2(dn, STDOUT_FILENO); 
    (void)dup2(dn, STDERR_FILENO);
    if (dn > 2) 
      close(dn);
    return 0;
}

/* ---- listen socket ---- */
static int open_listen_socket(void) {
    struct addrinfo hints, *ai = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; 
    hints.ai_socktype = SOCK_STREAM; 
    hints.ai_flags = AI_PASSIVE;

    int rc = getaddrinfo(NULL, TCP_PORT, &hints, &ai);
    if (rc != 0) { 
      syslog(LOG_ERR, "getaddrinfo: %s", gai_strerror(rc)); 
      return -1; 
    }

    int fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd == -1) { 
      syslog(LOG_ERR, "socket: %s", strerror(errno)); 
      freeaddrinfo(ai); 
      return -1;
    }

    int one = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) == -1)
        syslog(LOG_ERR, "setsockopt(SO_REUSEADDR): %s", strerror(errno));
#ifdef SO_REUSEPORT
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one)) == -1)
        syslog(LOG_ERR, "setsockopt(SO_REUSEPORT): %s", strerror(errno));
#endif

    if (bind(fd, ai->ai_addr, ai->ai_addrlen) == -1) {
        syslog(LOG_ERR, "bind: %s", strerror(errno));
        freeaddrinfo(ai); close(fd); return -1;
    }
    freeaddrinfo(ai);

    if (listen(fd, BACKLOG) == -1) { 
    syslog(LOG_ERR, "listen: %s", strerror(errno)); 
    close(fd); 
    return -1;
    }

    syslog(LOG_INFO, "Listening on port %s", TCP_PORT);
    return fd;
}

/* ---- append packet (file mode) ---- */
#if !USE_CHAR_DEVICE
static int append_packet_locked(int fdw, pthread_mutex_t *mx, const char *buf, size_t len) {
    if (pthread_mutex_lock(mx) != 0) 
      return -1;
    size_t off = 0;
    while (off < len) {
        ssize_t w = write(fdw, buf + off, len - off);
        if (w < 0) { 
         pthread_mutex_unlock(mx); 
         return -1; 
        }
        off += (size_t)w;
    }
    pthread_mutex_unlock(mx);
    fsync(fdw);
    return 0;
}
#endif

/* ---- send entire store back ---- */
static int send_whole_store(int cfd, pthread_mutex_t *mx) {
    int fdr = open(DATA_PATH, O_RDONLY);
    if (fdr < 0) { 
     syslog(LOG_ERR, "open(%s,read): %s", DATA_PATH, strerror(errno)); 
     return -1; 
    }

#if !USE_CHAR_DEVICE
    if (pthread_mutex_lock(mx) != 0) { 
      close(fdr); 
      return -1; 
    }
#endif

    char buf[RX_CHUNK];
    for (;;) {
        ssize_t r = read(fdr, buf, sizeof(buf));
        if (r == 0) 
          break;
        if (r < 0) { 
          if (errno == EINTR) 
            continue; 
          syslog(LOG_ERR, "read: %s", strerror(errno)); 
          break; 
        }
        
        size_t off = 0;
        while (off < (size_t)r) {
            ssize_t s = send(cfd, buf + off, (size_t)r - off, 0);
            if (s < 0) { 
              if (errno == EINTR) 
                continue;
             syslog(LOG_ERR, "send: %s", strerror(errno));
             goto out; 
            }
            off += (size_t)s;
        }
    }

out:
#if !USE_CHAR_DEVICE
    pthread_mutex_unlock(mx);
#endif
    close(fdr);
    return 0;
}

/* ---- client worker ---- */
static void *client_thread(void *arg) {
    client_node_t *node = (client_node_t*)arg;
    node->done = false;

    struct sockaddr_in peer; socklen_t plen = sizeof(peer);
    char ip[INET_ADDRSTRLEN] = "unknown";
    if (getpeername(node->cfd, (struct sockaddr*)&peer, &plen) == 0)
        (void)inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));
    syslog(LOG_INFO, "Accepted connection from %s", ip);

    /* accumulator buffer for arbitrary recv() chunking */
    char *acc = NULL; size_t acc_sz = 0;

    for (;;) {
        char chunk[RX_CHUNK];
        ssize_t n = recv(node->cfd, chunk, sizeof(chunk), 0);
        if (n == 0) 
          break;                      // peer closed
        if (n < 0) {
          if (errno == EINTR) 
            continue; 
          syslog(LOG_ERR,"recv: %s",strerror(errno)); 
          goto done; 
          
        }

        /* grow buffer and append */
        char *tmp = realloc(acc, acc_sz + (size_t)n);
        if (!tmp) { 
          syslog(LOG_ERR, "realloc failed"); 
          goto done; 
        }
        acc = tmp; memcpy(acc + acc_sz, chunk, (size_t)n); acc_sz += (size_t)n;

        /* process ALL newline-terminated packets currently in acc */
        
        for (size_t i = 0; i < acc_sz; ++i) {
            if (acc[i] == '\n') {
                size_t pkt_len = i + 1;  // include newline

#if USE_CHAR_DEVICE
                if (pkt_len >= strlen(SEEK_PREFIX) &&
                    strncmp(acc, SEEK_PREFIX, strlen(SEEK_PREFIX)) == 0) {
                    struct aesd_seekto s = {0};
                    if (2 == sscanf(acc, "AESDCHAR_IOCSEEKTO:%d,%d", &s.write_cmd, &s.write_cmd_offset)) {
                        int fd = open(DATA_PATH, O_RDWR);
                        if (fd >= 0) { 
                           if (ioctl(fd, AESDCHAR_IOCSEEKTO, &s) != 0)
                              syslog(LOG_ERR,"ioctl: %s",strerror(errno)); close(fd); 
                        }
                    }
                } else
#endif
                {
#if !USE_CHAR_DEVICE
                    int fdw = open(DATA_PATH, O_CREAT|O_WRONLY|O_APPEND, 0644);
                    if (fdw < 0) { 
                      syslog(LOG_ERR,"open(%s,append): %s",DATA_PATH,strerror(errno)); 
                      goto done; 
                    }
                    if (append_packet_locked(fdw, node->file_mx, acc, pkt_len) != 0) {
                        syslog(LOG_ERR, "append failed: %s", strerror(errno));
                        close(fdw); goto done;
                    }
                    close(fdw);
#endif
                }

                /* send whole store after this packet */
                (void)send_whole_store(node->cfd, node->file_mx);

                /* consume the processed packet and keep remainder (if any) */
                size_t remain = acc_sz - pkt_len;
                if (remain > 0) memmove(acc, acc + pkt_len, remain);
                acc_sz = remain;
                i = (size_t)-1;              // restart scan from beginning of buffer
            }
        }
    }

done:
    if (acc) free(acc);
    shutdown(node->cfd, SHUT_RDWR);
    close(node->cfd);
    syslog(LOG_INFO, "Closed connection from %s", ip);
    node->done = true;
    return NULL;
}


int main(int argc, char **argv) {
    openlog("aesdsocket", LOG_PID, LOG_USER);

    bool want_daemon = (argc == 2 && strcmp(argv[1], "-d") == 0);

    install_signals();

    g_listen_fd = open_listen_socket();
    if (g_listen_fd < 0) { 
      closelog(); 
      return -1; 
    }   // socket step failure

    if (want_daemon) {
        if (daemonize_after_bind() != 0) { 
          close(g_listen_fd); 
          closelog(); 
          return -1; 
        }
        syslog(LOG_INFO, "daemon mode enabled");
    }

    SLIST_HEAD(, client_node) clients; SLIST_INIT(&clients);
    pthread_mutex_t file_mx = PTHREAD_MUTEX_INITIALIZER;

    while (!g_stop) {
        struct sockaddr_in peer; socklen_t plen = sizeof(peer);
        int cfd = accept(g_listen_fd, (struct sockaddr*)&peer, &plen);
        if (cfd == -1) {
            if (errno == EINTR && g_stop) 
              break;
            if (errno == EINTR) 
              continue;
            syslog(LOG_ERR, "accept: %s", strerror(errno)); 
            continue;
        }

        client_node_t *node = calloc(1, sizeof(*node));
        if (!node) { 
          shutdown(cfd, SHUT_RDWR); 
          close(cfd);
          continue;
        }
        node->cfd = cfd; node->file_mx = &file_mx; node->done = false;

        int prc = pthread_create(&node->tid, NULL, client_thread, node);
        if (prc != 0) { 
          shutdown(cfd, SHUT_RDWR); close(cfd);
          free(node); 
          continue; 
        }
        SLIST_INSERT_HEAD(&clients, node, link);

        /* reclaim finished workers */
        client_node_t *it = SLIST_FIRST(&clients), *tmp = NULL;
        while (it) {
            tmp = SLIST_NEXT(it, link);
            if (it->done) {
                pthread_join(it->tid, NULL);
                SLIST_REMOVE(&clients, it, client_node, link);
                free(it);
            }
            it = tmp;
        }
    }

    shutdown(g_listen_fd, SHUT_RDWR);
    close(g_listen_fd);
    g_listen_fd = -1;

    client_node_t *it2 = SLIST_FIRST(&clients), *tmp2 = NULL;
    while (it2) {
        tmp2 = SLIST_NEXT(it2, link);
        pthread_join(it2->tid, NULL);
        SLIST_REMOVE(&clients, it2, client_node, link);
        free(it2);
        it2 = tmp2;
    }
    pthread_mutex_destroy(&file_mx);

#if !USE_CHAR_DEVICE
    if (unlink(DATA_PATH) == -1 && errno != ENOENT)
        syslog(LOG_ERR, "unlink(%s): %s", DATA_PATH, strerror(errno));
    else
        syslog(LOG_INFO, "Removed %s", DATA_PATH);
#endif

    closelog();
    return 0;
}

