#ifndef __COMMUNICATE_H__
#define __COMMUNICATE_H__

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

void send_msg_to_receivers(char*);
void send_msg_to_senders(char*);
void send_frame(char*, enum SendFrame_DstType);

#endif
