#include "receiver.h"

#include <assert.h>

void init_receiver(Receiver* receiver, int id) {
    pthread_cond_init(&receiver->buffer_cv, NULL);
    pthread_mutex_init(&receiver->buffer_mutex, NULL);

    receiver->recv_id = id;
    receiver->input_framelist_head = NULL;

    // Initialise new expected sequence number array corresponding to senders to
    // 0
    receiver->seq_num_arr =
        (uint8_t*) calloc(glb_senders_array_length, sizeof(uint8_t));

    // Initialise ptrs to messages (strings) for each sender
    receiver->messages_arr =
        (char**) calloc(glb_senders_array_length, sizeof(char*));
}

void handle_incoming_msgs(Receiver* receiver,
                          LLnode** outgoing_frames_head_ptr) {
    // TODO: Suggested steps for handling incoming frames
    //    1) Dequeue the Frame from the sender->input_framelist_head
    //    2) Convert the char * buffer to a Frame data type
    //    3) Check whether the frame is for this receiver
    //    4) Acknowledge that this frame was received

    int incoming_msgs_length = ll_get_length(receiver->input_framelist_head);
    while (incoming_msgs_length > 0) {
        // Pop a node off the front of the link list and update the count
        LLnode* ll_inmsg_node = ll_pop_node(&receiver->input_framelist_head);
        incoming_msgs_length = ll_get_length(receiver->input_framelist_head);

        // DUMMY CODE: Print the raw_char_buf
        // NOTE: You should not blindly print messages!
        //      Ask yourself: Is this message really for me?
        char* raw_char_buf = ll_inmsg_node->value;
        Frame* inframe = convert_char_to_frame(raw_char_buf);

        // Free raw_char_buf
        free(raw_char_buf);

        // Only care about message if intended for current receiver
        if (inframe->dst_id == receiver->recv_id) {

            //        	printf("<RECV_%d>:[%s]\n", receiver->recv_id,
            //        inframe->data);

            int sender_id = inframe->src_id;

            // TODO acknowledge by adding msg to outgoing_frames_head_ptr
            Frame* ack_frame = malloc(sizeof(Frame));
            assert(ack_frame);
            ack_frame->src_id = receiver->recv_id;
            ack_frame->dst_id = sender_id; // sender id (from frame)
            ack_frame->seq_num = inframe->seq_num;

            // Update seq num and curr message only if this is new data (not
            // duplicate)
            if (receiver->seq_num_arr[sender_id] == inframe->seq_num) {

                fprintf(stderr,
                        "received msg from sender %d to receiver %d with "
                        "ACCEPTED seq num %d\n",
                        inframe->src_id, inframe->dst_id, inframe->seq_num);

                receiver->seq_num_arr[sender_id]++;

                // Add data to whatever current message is (could be nothing)

                // Temporary current string pointer
                char* current_str = receiver->messages_arr[sender_id];

                // If a current message exists, concatenate to it
                if (current_str) {
                    // Add 1 to account for null char
                    int new_str_len =
                        1 + strlen(current_str) + strlen(inframe->data);
                    // Allocate space for new string
                    receiver->messages_arr[sender_id] =
                        (char*) malloc(new_str_len * sizeof(char));

                    // Copy over old string
                    strcpy(receiver->messages_arr[sender_id], current_str);
                    // Concatenate new message with string
                    strcat(receiver->messages_arr[sender_id], inframe->data);

                    // Free old string
                    free(current_str);

                } else {
                    // Else means no existing string exists, just copy over
                    // message
                    int msg_length = 1 + strlen(inframe->data);
                    receiver->messages_arr[sender_id] =
                        (char*) malloc(msg_length * sizeof(char));
                    strcpy(receiver->messages_arr[sender_id], inframe->data);
                }

                // If end of message
                if (inframe->message_end == 1) {
                    // Print entire message
                    printf("<RECV_%d>:[%s]\n", receiver->recv_id,
                           receiver->messages_arr[sender_id]);

                    // Free message
                    free(receiver->messages_arr[sender_id]);

                    // Reset message pointer to null to indicate no existing
                    // string
                    receiver->messages_arr[sender_id] = NULL;
                }

            } else {

                fprintf(stderr,
                        "received msg from sender %d to receiver %d with "
                        "UNACCEPTED seq num %d\n",
                        inframe->src_id, inframe->dst_id, inframe->seq_num);
            }

            // Convert the message to char buf
            char* ack_charbuf = convert_frame_to_char(ack_frame);
            // Ready to send ack frame
            ll_append_node(outgoing_frames_head_ptr, ack_charbuf);

            free(ack_frame);
        }

        free(inframe);
        free(ll_inmsg_node);
    }
}

void* run_receiver(void* input_receiver) {
    struct timespec time_spec;
    struct timeval curr_timeval;
    const int WAIT_SEC_TIME = 0;
    const long WAIT_USEC_TIME = 100000;
    Receiver* receiver = (Receiver*) input_receiver;
    LLnode* outgoing_frames_head;

    // This incomplete receiver thread, at a high level, loops as follows:
    // 1. Determine the next time the thread should wake up if there is nothing
    // in the incoming queue(s)
    // 2. Grab the mutex protecting the input_msg queue
    // 3. Dequeues messages from the input_msg queue and prints them
    // 4. Releases the lock
    // 5. Sends out any outgoing messages

    while (1) {
        // NOTE: Add outgoing messages to the outgoing_frames_head pointer
        outgoing_frames_head = NULL;
        gettimeofday(&curr_timeval, NULL);

        // Either timeout or get woken up because you've received a datagram
        // NOTE: You don't really need to do anything here, but it might be
        // useful for debugging purposes to have the receivers periodically
        // wakeup and print info
        time_spec.tv_sec = curr_timeval.tv_sec;
        time_spec.tv_nsec = curr_timeval.tv_usec * 1000;
        time_spec.tv_sec += WAIT_SEC_TIME;
        time_spec.tv_nsec += WAIT_USEC_TIME * 1000;
        if (time_spec.tv_nsec >= 1000000000) {
            time_spec.tv_sec++;
            time_spec.tv_nsec -= 1000000000;
        }

        //*****************************************************************************************
        // NOTE: Anything that involves dequeing from the input frames should go
        //      between the mutex lock and unlock, because other threads
        //      CAN/WILL access these structures
        //*****************************************************************************************
        pthread_mutex_lock(&receiver->buffer_mutex);

        // Check whether anything arrived
        int incoming_msgs_length =
            ll_get_length(receiver->input_framelist_head);
        if (incoming_msgs_length == 0) {
            // Nothing has arrived, do a timed wait on the condition variable
            // (which releases the mutex). Again, you don't really need to do
            // the timed wait. A signal on the condition variable will wake up
            // the thread and reacquire the lock
            pthread_cond_timedwait(&receiver->buffer_cv,
                                   &receiver->buffer_mutex, &time_spec);
        }

        handle_incoming_msgs(receiver, &outgoing_frames_head);

        pthread_mutex_unlock(&receiver->buffer_mutex);

        // CHANGE THIS AT YOUR OWN RISK!
        // Send out all the frames user has appended to the outgoing_frames list
        int ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        while (ll_outgoing_frame_length > 0) {
            LLnode* ll_outframe_node = ll_pop_node(&outgoing_frames_head);
            char* char_buf = (char*) ll_outframe_node->value;

            // The following function frees the memory for the char_buf object
            send_msg_to_senders(char_buf);

            // Free up the ll_outframe_node
            free(ll_outframe_node);

            ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        }
    }
    pthread_exit(NULL);
}
