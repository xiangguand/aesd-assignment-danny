#include "thread_para.h"

#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

static threadPara_t *genNode() {
  threadPara_t *node = malloc(sizeof(threadPara_t));
  node->next_ = NULL;
  return node;
}

threadPara_t *createThreadPara(threadPara_t **head) {
  threadPara_t *node = genNode();
  if(!(*head)) {
    *head = node;
  }
  else {
    appendThreadPara(*head, node);
  }

  return node;
}

void appendThreadPara(threadPara_t *head, threadPara_t *node) {
  if(head) {
    while(head->next_ != NULL) {
      head = head->next_;
    }
    head->next_ = node;
  }
}

void removeThreadPara(threadPara_t **head, threadPara_t *rm_node) {
  threadPara_t *prev = *head;
  if(!prev || !rm_node) {
    return;
  }
  threadPara_t *node = (*head)->next_;
  
  if(prev == rm_node) {
    *head = node;
  }
  else {
    while(node) {
      if(node == rm_node) {
        prev->next_ = node->next_;
        free(rm_node);
        break;
      }

      prev = node;
      node = node->next_;
    }
  }
  
}

void disposeThreadPara(threadPara_t *head) {
  threadPara_t *node = head;
  while(head != NULL) {
    (void)pthread_join(head->thread_, NULL);
    node = head;
    head = head->next_;
    free(node);
  }
}




