#ifndef THREAD_PARA_H_
#define THREAD_PARA_H_

#include "thread_para.h"

#include <unistd.h>
#include <pthread.h>

typedef struct _threadPara_t_ {
  pthread_t thread_;
  struct _threadPara_t_ *next_;
} threadPara_t;

threadPara_t *createThreadPara(threadPara_t **head);
void appendThreadPara(threadPara_t *head, threadPara_t *node);
void removeThreadPara(threadPara_t **head, threadPara_t *rm_node);
void disposeThreadPara(threadPara_t *head);

#endif /* THREAD_PARA_H_ */
