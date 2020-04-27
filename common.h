#ifndef __COMMON_H__
#define __COMMON_H__

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

#define MAX_COMMAND_LENGTH 16
#define AUTOMATED_FILENAME 512
typedef unsigned char uchar_t;

// System configuration information
struct SysConfig_t {
    float drop_prob;
    float corrupt_prob;
    unsigned char automated;
    char automated_file[AUTOMATED_FILENAME];
};
typedef struct SysConfig_t SysConfig;

// Command line input information
struct Cmd_t {
    uint16_t src_id;
    uint16_t dst_id;
    char* message;
};
typedef struct Cmd_t Cmd;

// Linked list information
enum LLtype { llt_string, llt_frame, llt_integer, llt_head } LLtype;

struct LLnode_t {
    struct LLnode_t* prev;
    struct LLnode_t* next;
    enum LLtype type;

    void* value;
};
typedef struct LLnode_t LLnode;

// Receiver and sender data structures
struct Receiver_t {
    // DO NOT CHANGE:
    // 1) buffer_mutex
    // 2) buffer_cv
    // 3) input_framelist_head
    // 4) recv_id
    pthread_mutex_t buffer_mutex;
    pthread_cond_t buffer_cv;
    LLnode* input_framelist_head;

    int recv_id;

    uint8_t* seq_num_arr; // Next expected packet seq num for each sender.
    			  // THIS IS LAST FRAME RECEIVED (LFR) + 1
			  // aka Next Frame Expected (NFE)
    char** messages_arr;  // Pointer to array char pointers)

    //added receiver window size to calculate largest acceptable frame (LAF)
    //by calculating: LAF = LFR + RWS
    int RWS;

    char*** window_buffer;
    int* window_buffer_dim; 
};

struct Sender_t {
    // DO NOT CHANGE:
    // 1) buffer_mutex
    // 2) buffer_cv
    // 3) input_cmdlist_head
    // 4) input_framelist_head
    // 5) send_id
    pthread_mutex_t buffer_mutex;
    pthread_cond_t buffer_cv;
    LLnode* input_cmdlist_head;
    LLnode* input_framelist_head;
    int send_id;

    // whether or not the timeout_time struct below holds relevant info
    int timeout_relevant;

    struct timeval timeout_time;

    // Buffer of next frame charbufs to be sent (to be put in outgoing list)
    // this will be the"right side" of the window and will slowly enter the window as we receive acks
    LLnode* outgoing_charbuf_buffer;



    char* last_outgoing_charbuf; // holds last frame in case dropped in
                                 // transmission. THIS IF CONVERTED TO FRAME HOLDS
				 // LAST FRAME SENT (LFS)
				 // we will change this to be a our window 

    uint8_t* assign_seq_num_arr; // Next seq num to assigon corresponding to each
                                 // receiver

    uint8_t* seq_num_arr; // Last seq num to successfully acked corresponding to
                          // each receiver. THIS IS LAST ACKNOWLEDGE RECEIVED (LAR)
    //added send window size variable for each sender.
    int SWS;
};

enum SendFrame_DstType { ReceiverDst, SenderDst } SendFrame_DstType;

typedef struct Sender_t Sender;
typedef struct Receiver_t Receiver;

#define MAX_FRAME_SIZE 64

// TODO: You should change this!
// Remember, your frame can be AT MOST 64 bytes!
// Make sure payload size > 1 byte (otherwise will send infinite '\0' data
// payload)
#define FRAME_PAYLOAD_SIZE 58

struct Frame_t {
    uint16_t src_id;
    uint16_t dst_id;
    uint8_t seq_num;
    uint8_t message_end;

    char data[FRAME_PAYLOAD_SIZE];
};
typedef struct Frame_t Frame;

// Declare global variables here
// DO NOT CHANGE:
//   1) glb_senders_array
//   2) glb_receivers_array
//   3) glb_senders_array_length
//   4) glb_receivers_array_length
//   5) glb_sysconfig
//   6) CORRUPTION_BITS
Sender* glb_senders_array;
Receiver* glb_receivers_array;
int glb_senders_array_length;
int glb_receivers_array_length;
SysConfig glb_sysconfig;
int CORRUPTION_BITS;

#endif
