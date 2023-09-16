#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libgen.h>
#include <string.h>
#include <dirent.h>
#include <assert.h>


/* Syslog, refer from https://linux.die.net/man/3/syslog */
#include <syslog.h>


int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  //! 1 Current executed program path
  //! 2 A full path to a file."
  //! 3 Text string which will be written within the file."
  if(argc != 3) {
    return 1;
  }
  printf("%s\n", argv[0]);
  printf("%s\n", argv[1]);
  printf("%s\n", argv[2]);

  char *file_path = argv[1];
  int fileLen = strlen(file_path);
  char *dir = malloc(fileLen+1);
  memcpy(dir, file_path, fileLen);
  dir = dirname(dir);
  dir[fileLen] = '\0';

  char *text = argv[2];
  int ret;


  //! Logging open
  openlog("AESD-Assignment2", 0, LOG_USER);

  if(NULL == opendir(dir)) {
    // directory does not exist
    syslog(LOG_DEBUG, "Create directory %s\n", dir);
    ret = mkdir(dir, 0775);
    assert(0 == ret);
  }
  else {
    // directory already exists
    syslog(LOG_DEBUG, "Directory already exists\n");
  }

  FILE *wf = fopen(file_path, "wb");
  if(NULL == wf) {
    syslog(LOG_ERR, "Can not open file %s\n", file_path);
  }

  ret = fputs(text, wf); // 1 -> success
  if(ret != 1) {
    syslog(LOG_ERR, "Write string to file fail\n");
  }

  free(dir);
  closelog();

  return ret != 1;
}

