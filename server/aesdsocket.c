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

/* Force close the printf */
#define printf(...) ;

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
      printf("Write bytes and bytes providing are not matched %d, %d\n", write_bytes, bytes);
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

static void *SocketClientThread(void * fd_);

static void *startSockerServerThread(void *fd_) {
  int sockfd = *((int *)fd_);
  free(fd_);
  int ret = 0;
  struct timespec tp, prev_tp;
  ret = clock_gettime(CLOCK_MONOTONIC, &prev_tp);
  char buf[200];
  printf("Socket id: %d\n", sockfd);
  threadPara_t *head = NULL;
  threadPara_t *node = NULL;
  /* Hanlde client connection */
  while(!fg_sigint && !fg_sigterm) {
    int clienfd;
    struct sockaddr_in client_address;
    socklen_t client_socket_len = sizeof(struct sockaddr_in);
    clienfd = accept(sockfd, (struct sockaddr *)&client_address, &client_socket_len);
    if(-1 != clienfd) {
      printf("Accept successfully from %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
    
      node = createThreadPara(&head);
      int *clientid_new = malloc(sizeof(int));
      *clientid_new = clienfd;
      ret = pthread_create(&node->thread_, NULL, SocketClientThread, (void *)clientid_new);
      assert(0 == ret);
      if(ret != 0) {
        printf("Fail to create pthreat");
        break;
      }
    }
    else {
      syslog(LOG_DEBUG, "File des is NULL\n");
    }

    ret = clock_gettime(CLOCK_MONOTONIC, &tp);
    assert(0 == ret);
    if(tp.tv_sec - prev_tp.tv_sec >= 10) {
      printf("===== Write time =====\n");
      ret = pthread_mutex_lock(&file_mutex);
      assert(0 == ret);
      printf("===== LOCK =====\n");
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
      printf("===== UNLOCK =====\n");
      prev_tp.tv_sec = tp.tv_sec;
    }
  }
  disposeThreadPara(head);
  printf("End server handler\n");

  return NULL;
}

static void *SocketClientThread(void * fd_) {
  assert(NULL != fd_);
  int fd = *((int *)fd_);
  free(fd_);

  // Request 2048 bytes buffer
  int ret;
  assert(0 == ret);
  char buf[204800];
  int bytes = 0;
  while(!fg_sigint && !fg_sigterm) {
    bytes = recv(fd, buf, 204800, 0);
    if(bytes > 0) {
      // printf("Recv: %d\n", bytes);
      // printf("%s\n", buf);

      /* Lock mutex */
      int ret;
      ret = pthread_mutex_lock(&file_mutex);
      assert(0 == ret);
      printf("===== LOCK =====\n");

      writeToAesdFile(buf, bytes);

      FILE *f = fopen(WRITE_FILENAME, "rb");
      if(NULL != f) {
        fseek(f, 0, SEEK_END);
        long file_size = ftell(f);
        if(file_size > 0) {
          char *buffer = (char *)malloc(file_size+1);
          fseek(f, 0, SEEK_SET);
          fread(buffer, 1, file_size, f);
          send(fd, buffer, file_size, 0);
          // printf("Transmit: \n%s\n", buffer);
          free(buffer);
        }
      }
      ret = pthread_mutex_unlock(&file_mutex);
      assert(0 == ret);
      printf("===== UNLOCK =====\n");
      /* Unlock mutex */
      break;
    }
  }
  printf("End client handler\n");

  return NULL;
}



int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  if(argc == 2 && strcmp("-d", argv[1]) == 0) {
    /* start daemon */
    pid_t pid;
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

  int ret;
  
  //! Logging open
  openlog("AESD-Assignment5", 0, LOG_USER);

  /* Create AESD data folder */
  if(NULL == opendir(WRITE_DIR)) {
    // directory does not exist
    syslog(LOG_DEBUG, "Create directory %s\n", WRITE_DIR);
    ret = mkdir(WRITE_DIR, 0775);
    assert(0 == ret);
    printf("Create directory %s\n", WRITE_DIR);
  }
  else {
    // directory already exists
    syslog(LOG_DEBUG, "Directory already exists\n");
    printf("Directory already exists\n");
  }


  /* Buid the address information using getaddrinfo method */
  struct addrinfo hints = {
    .ai_flags = AI_PASSIVE,
    .ai_family = AF_UNSPEC,
    .ai_socktype = SOCK_STREAM,

  };
  struct addrinfo *res = NULL;
  printf("Get info\n");
  ret = getaddrinfo(SOCKER_SERVER_HOST, SOCKER_SERVER_PORT, &hints, &res);
  if(-1 == ret) {
    printf("Get address info fail\n");
    goto cleanup;
  }
  char buf[20];
  (void)inet_ntop(res->ai_family, &((struct sockaddr_in *)res->ai_addr)->sin_addr, buf, sizeof(buf));
  printf("Server IP %s with port %s\n", buf, SOCKER_SERVER_PORT);
  
  assert(res != NULL);
  // printf("%s\n", r->ai_addr);
  
  /* Create a socket server with port 9000 */
  int sockfd;

  sockfd = socket(res->ai_family,res->ai_socktype,res->ai_protocol);
  if(-1 == sockfd) {
    printf("Fail to create an endpoint\n");
    return -1;
  }
  printf("Create an endpoint for communication successfully. %d\n", sockfd);

  /* Setting socket file descriptor to non-blocking mode */
  int socket_file_flags = fcntl(sockfd, F_GETFL, 0);
  socket_file_flags |= O_NONBLOCK;
  ret = fcntl(sockfd, F_SETFL, socket_file_flags);
  if(-1 == ret) {
    printf("Fail to change to non-blocking\n");
    goto cleanup;
  }
  int dummy =1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &dummy, sizeof(int)) == -1) {	
		perror("setsockopt error");
  }

  ret = bind(sockfd, res->ai_addr, sizeof(struct sockaddr));
  if(-1 == ret) {
    printf("Fail to bind\n");
    goto cleanup;
  }
  printf("Bind successfully\n");

  ret = listen(sockfd, 10);
  if(-1 == ret) {
    printf("Fail to listen\n");
    goto cleanup;
  }
  printf("Start listening\n");


  pthread_t thread = 0;
  /* Signal handling */
  struct sigaction new_actions;
  bool success = true;
  memset(&new_actions, 0 ,sizeof(sigaction));
  new_actions.sa_flags = SA_SIGINFO;
  new_actions.sa_handler = Signal_Handler;
  /* Register signal information and callback */
  if(sigaction(SIGINT, &new_actions, NULL) != 0) {
    printf("Fail to register SIGINT\n");
    success = false;  
  }
  if(sigaction(SIGTERM, &new_actions, NULL) != 0) {
    printf("Fail to register SIGTERM\n");
    success = false;  
  }

  if(success) {
    printf("Waiting forever for a signal to terminate program\n");

    /* Server handler thread */
    ret = pthread_mutex_init(&file_mutex, NULL);
    assert(0 == ret);
    int *sockfd_new = malloc(sizeof(int));
    *sockfd_new = sockfd;
    ret = pthread_create(&thread, NULL, startSockerServerThread, (void *)sockfd_new);
    if(ret != 0) {
      printf("Fail to create pthreat");
      goto cleanup;
    }  

    if(fg_sigint) {
      printf("Caught SIGINT\n");
    }
    if(fg_sigterm) {
      printf("Caught SIGTERM\n");
    }
  }

  void *para;
  pthread_join(thread, &para);
  (void)para;


cleanup:
  printf("Closing socket server ...\n");
  close(sockfd);
  if(res) {
    freeaddrinfo(res);
  }
  
  printf("Deleting aesdsocketdata ...\n");
  system("rm" " -f " WRITE_FILENAME);
  closelog();

  return 0;
}


