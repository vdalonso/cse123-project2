#ifndef __SENDER_H__
#define __SENDER_H__

#include "common.h"
#include "communicate.h"
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

void init_sender(Sender*, int);
void* run_sender(void*);

#endif
