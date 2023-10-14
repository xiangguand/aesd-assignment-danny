#ifndef SOCKET_THREAD_H_
#define SOCKET_THREAD_H_

#include <pthread.h>

/* My service information configuration */
#define MAX_CLIENTS_NUM     (20)


typedef struct socket_thread
{
  int socket_id_;
  pthread_t threads_[MAX_CLIENTS_NUM];
};




#endif /* SOCKET_THREAD_H_ */
