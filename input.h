#ifndef __INPUT_H__
#define __INPUT_H__

#include "common.h"
#include "util.h"
#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#define DEFAULT_INPUT_BUFFER_SIZE 1024

void* run_stdinthread(void*);
ssize_t getline(char** lineptr, size_t* n, FILE* stream);

#endif
