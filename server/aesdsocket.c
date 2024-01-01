#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libgen.h>
#include <string.h>
#include <dirent.h>
#include <assert.h>
#include <pthread.h>
#include <fcntl.h>
#include <time.h>

/* Include signal header files */
#include <signal.h>

/* Include socket header files */
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "thread_para.h"

/* Syslog, refer from https://linux.die.net/man/3/syslog */
#include <syslog.h>

/* Force close the DEBUG_PRINTF */
#define DEBUG 1
#if DEBUG
#define DEBUG_PRINTF(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINTF(...) ;
#endif /* DEBUG */

/* Assignment information */
#define WRITE_DIR   "/var/tmp"
#define STR_CONCATE(a, b) a b
#define WRITE_FILENAME   STR_CONCATE(WRITE_DIR, "/aesdsocketdata")

static pthread_mutex_t file_mutex;
static inline void writeToAesdFile(const char *data, int bytes) {
  if(bytes <= 0) {
    return;
  }
  FILE *f = fopen(WRITE_FILENAME, "a");
  if(NULL != f) {
    int write_bytes = fwrite(data, 1, bytes, f);
    if(write_bytes != bytes) {
      DEBUG_PRINTF("Write bytes and bytes providing are not matched %d, %d\n", write_bytes, bytes);
    }
    fclose(f);
  }
}

/* Socket information */
#define SOCKER_SERVER_HOST  "0.0.0.0"
#define SOCKER_SERVER_PORT  "9000"
const uint8_t server_ipv4_address[] = {127, 0, 0, 1};

/* Signal handler */
bool fg_sigint = false;
bool fg_sigterm = false;
static void Signal_Handler(int sig_num) {
  if(SIGINT == sig_num) {
    fg_sigint = true;
  }
  else if(SIGTERM == sig_num) {
    fg_sigterm = true;
  }
}

/* AESDCHAR Driver */
#define AESDCHAR_DEVICE "/dev/aesdchar"

/* Handle AESDCHAR IOCSEEKTO, get X and Y, find: return 0 */
int handle_aesdchar_ioseekto(const char *buf, int *x, int *y)
{
  int ret = sscanf(buf, "AESDCHAR_IOCSEEKTO:%u,%u", x, y);
  if(2 == ret) {
    // find !!!
    DEBUG_PRINTF("Find IOCSEEKTO: %d, %d\n", *x, *y);
    return 1;
  }
  return 0;
}

static void *SocketClientThread(void * fd_);

static void *startSockerServerThread(void *fd_) {
  int sockfd = *((int *)fd_);
  free(fd_);
  int ret = 0;
  struct timespec tp, prev_tp;
  char buf[200];
  DEBUG_PRINTF("Socket id: %d\n", sockfd);
  bool fg_start_write_timestamp = false;
  threadPara_t *head = NULL;
  threadPara_t *node = NULL;
  /* Hanlde client connection */
  while(!fg_sigint && !fg_sigterm) {
    int clienfd;
    struct sockaddr_in client_address;
    socklen_t client_socket_len = sizeof(struct sockaddr_in);
    clienfd = accept(sockfd, (struct sockaddr *)&client_address, &client_socket_len);
    if(-1 != clienfd) {
      if(false == fg_start_write_timestamp) {
        ret = clock_gettime(CLOCK_MONOTONIC, &prev_tp);
        if(0 != ret) {
          syslog(LOG_DEBUG, "clock_gettime error\n");
        }
        fg_start_write_timestamp = true;
      }
      DEBUG_PRINTF("Accept successfully from %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
    
      node = createThreadPara(&head);
      int *clientid_new = malloc(sizeof(int));
      *clientid_new = clienfd;
      ret = pthread_create(&node->thread_, NULL, SocketClientThread, (void *)clientid_new);
      assert(0 == ret);
      if(ret != 0) {
        DEBUG_PRINTF("Fail to create pthreat");
        break;
      }
      void *ptr = NULL;
      pthread_join(node->thread_, &ptr) ;
    }
    else {
      syslog(LOG_DEBUG, "File des is NULL\n");
    }

    ret = clock_gettime(CLOCK_MONOTONIC, &tp);
    assert(0 == ret);
#if CONFIG_WRITE_TIMESTAMP_PERIODICLY // write time stamp
    if(fg_start_write_timestamp && tp.tv_sec - prev_tp.tv_sec >= 10) {
      DEBUG_PRINTF("===== Write time =====\n");
      ret = pthread_mutex_lock(&file_mutex);
      assert(0 == ret);
      DEBUG_PRINTF("===== LOCK =====\n");
      time_t currentTime;
      struct tm* timeInfo;
      time(&currentTime);
      timeInfo = localtime(&currentTime);
      ret = strftime(buf, sizeof(buf), "timestamp: %a, %d %b %Y %H:%M:%S %z", timeInfo);
      buf[ret++] = '\n';
      buf[ret++] = '\0';
      writeToAesdFile(buf, ret);
      ret = pthread_mutex_unlock(&file_mutex);
      assert(0 == ret);
      DEBUG_PRINTF("===== UNLOCK =====\n");
      prev_tp.tv_sec = tp.tv_sec;
    }
#endif /* CONFIG_WRITE_TIMESTAMP_PERIODICLY */
  }
  disposeThreadPara(head);
  DEBUG_PRINTF("End server handler\n");

  return NULL;
}

static void *SocketClientThread(void * fd_) {
  assert(NULL != fd_);
  int fd = *((int *)fd_);
  free(fd_);



  // Request 204800 bytes buffer
  int ret = 0;
  int x, y;
  assert(0 == ret);
  char buf[204800] = {0};
  int bytes = 0;
  while(!fg_sigint && !fg_sigterm) {
    bytes = recv(fd, buf, 204800, 0);
    if(bytes > 0) {
      // DEBUG_PRINTF("Recv: %d\n", bytes);
      // DEBUG_PRINTF("%s\n", buf);

      /* Lock mutex */
      ret = pthread_mutex_lock(&file_mutex);
      assert(0 == ret);
      DEBUG_PRINTF("===== LOCK =====\n");

      /* Open AESDCHAR Device */
      int aesdchar_fd = open(AESDCHAR_DEVICE, O_RDONLY);
      if(-1 == aesdchar_fd) {
        DEBUG_PRINTF("Fail to open AESDCHAR Device\n");
      }
      /* if find AESD_IOCSEEKTO */
      // if(aesdchar_fd != -1) {
        // aesdchar driver is available
        if(handle_aesdchar_ioseekto(buf, &x, &y)) {
          lseek(aesdchar_fd, x, SEEK_SET);
          lseek(aesdchar_fd, y, SEEK_CUR);
      }
      // }
      // if(aesdchar_fd != -1) {
        (void)close(aesdchar_fd);
      // }

      writeToAesdFile(buf, bytes);

      FILE *f = fopen(WRITE_FILENAME, "rb");
      if(NULL != f) {
        fseek(f, 0, SEEK_END);
        long file_size = ftell(f);
        if(file_size > 0) {
          fseek(f, 0, SEEK_SET);
          fread(buf, 1, file_size, f);
          send(fd, buf, file_size, 0);
          DEBUG_PRINTF("Transmit: \n%s\n", buf);
        }
        fclose(f);
      }
      ret = pthread_mutex_unlock(&file_mutex);
      assert(0 == ret);
      DEBUG_PRINTF("===== UNLOCK =====\n");
      /* Unlock mutex */
      break;
    }
  }
  DEBUG_PRINTF("End client handler\n");

  return NULL;
}



int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  if(argc == 2 && strcmp("-d", argv[1]) == 0) {
    /* start daemon */
    pid_t pid = 0;
    pid = fork();

    if(pid < 0) {
      perror("Fork failed");
      exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    pid_t sid;
    sid = setsid();
    if(sid < 0) {
      perror("setsid failed");
      exit(EXIT_FAILURE);
    }
  }

  int ret = -1;
  
  //! Logging open
  openlog("AESD-Assignment5", 0, LOG_USER);

  /* Create AESD data folder */
  DIR *dir = opendir(WRITE_DIR);
  if(NULL == dir) {
    // directory does not exist
    syslog(LOG_DEBUG, "Create directory %s\n", WRITE_DIR);
    ret = mkdir(WRITE_DIR, 0775);
    assert(0 == ret);
    DEBUG_PRINTF("Create directory %s\n", WRITE_DIR);
  }
  else {
    // directory already exists
    syslog(LOG_DEBUG, "Directory already exists\n");
    DEBUG_PRINTF("Directory already exists\n");
    closedir(dir);
  }


  /* Buid the address information using getaddrinfo method */
  struct addrinfo hints = {
    .ai_flags = AI_PASSIVE,
    .ai_family = AF_UNSPEC,
    .ai_socktype = SOCK_STREAM,

  };
  struct addrinfo *res = NULL;
  DEBUG_PRINTF("Get info\n");
  ret = getaddrinfo(SOCKER_SERVER_HOST, SOCKER_SERVER_PORT, &hints, &res);
  if(-1 == ret) {
    DEBUG_PRINTF("Get address info fail\n");
    goto cleanup;
  }
  char buf[20];
  (void)inet_ntop(res->ai_family, &((struct sockaddr_in *)res->ai_addr)->sin_addr, buf, sizeof(buf));
  DEBUG_PRINTF("Server IP %s with port %s\n", buf, SOCKER_SERVER_PORT);
  
  assert(res != NULL);
  // DEBUG_PRINTF("%s\n", r->ai_addr);
  
  /* Create a socket server with port 9000 */
  int sockfd;

  sockfd = socket(res->ai_family,res->ai_socktype,res->ai_protocol);
  if(-1 == sockfd) {
    DEBUG_PRINTF("Fail to create an endpoint\n");
    return -1;
  }
  DEBUG_PRINTF("Create an endpoint for communication successfully. %d\n", sockfd);

  /* Setting socket file descriptor to non-blocking mode */
  int socket_file_flags = fcntl(sockfd, F_GETFL, 0);
  socket_file_flags |= O_NONBLOCK;
  ret = fcntl(sockfd, F_SETFL, socket_file_flags);
  if(-1 == ret) {
    DEBUG_PRINTF("Fail to change to non-blocking\n");
    goto cleanup;
  }
  int dummy =1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &dummy, sizeof(int)) == -1) {	
		perror("setsockopt error");
  }

  ret = bind(sockfd, res->ai_addr, sizeof(struct sockaddr));
  if(-1 == ret) {
    DEBUG_PRINTF("Fail to bind\n");
    goto cleanup;
  }
  DEBUG_PRINTF("Bind successfully\n");

  ret = listen(sockfd, 10);
  if(-1 == ret) {
    DEBUG_PRINTF("Fail to listen\n");
    goto cleanup;
  }
  DEBUG_PRINTF("Start listening\n");

  pthread_t thread = 0;
  /* Signal handling */
  struct sigaction new_actions = {0};
  bool success = true;
  memset(&new_actions, 0 ,sizeof(sigaction));
  new_actions.sa_flags = SA_SIGINFO;
  new_actions.sa_handler = Signal_Handler;
  /* Register signal information and callback */
  if(sigaction(SIGINT, &new_actions, NULL) != 0) {
    DEBUG_PRINTF("Fail to register SIGINT\n");
    success = false;  
  }
  if(sigaction(SIGTERM, &new_actions, NULL) != 0) {
    DEBUG_PRINTF("Fail to register SIGTERM\n");
    success = false;  
  }

  if(success) {
    DEBUG_PRINTF("Waiting forever for a signal to terminate program\n");

    /* Server handler thread */
    ret = pthread_mutex_init(&file_mutex, NULL);
    assert(0 == ret);
    int *sockfd_new = malloc(sizeof(int));
    *sockfd_new = sockfd;
    ret = pthread_create(&thread, NULL, startSockerServerThread, (void *)sockfd_new);
    if(ret != 0) {
      DEBUG_PRINTF("Fail to create pthreat");
      goto cleanup;
    }  

    if(fg_sigint) {
      DEBUG_PRINTF("Caught SIGINT\n");
    }
    if(fg_sigterm) {
      DEBUG_PRINTF("Caught SIGTERM\n");
    }
  }

  void *para = NULL;
  pthread_join(thread, &para);
  (void)para;


cleanup:
  DEBUG_PRINTF("Closing socket server ...\n");
  close(sockfd);
  if(res) {
    freeaddrinfo(res);
  }
  
  DEBUG_PRINTF("Deleting aesdsocketdata ...\n");
  system("rm" " -f " WRITE_FILENAME);
  closelog();

  return 0;
}


