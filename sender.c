#include "sender.h"

#include <assert.h>

void init_sender(Sender* sender, int id) {
    // TODO: You should fill in this function as necessary
    pthread_cond_init(&sender->buffer_cv, NULL);
    pthread_mutex_init(&sender->buffer_mutex, NULL);

    sender->send_id = id;
    sender->input_cmdlist_head = NULL;
    sender->input_framelist_head = NULL;

    sender->timeout_relevant = 0;
    sender->last_outgoing_charbuf = NULL;

    sender->outgoing_charbuf_buffer = NULL;

    // Zero initialise sequence number array corresponding to receivers
    sender->seq_num_arr =
        (uint8_t*) calloc(glb_receivers_array_length, sizeof(uint8_t));

    // Zero initialise sequence number array to assign next frame corresponding
    // to receivers
    sender->assign_seq_num_arr =
        (uint8_t*) calloc(glb_receivers_array_length, sizeof(uint8_t));
}

// Update timeout time right after frame sent
void update_timeout_time(Sender* sender) {
    struct timeval curr_timeval;

    // Get the current time
    gettimeofday(&curr_timeval, NULL);

    // Set to current time
    sender->timeout_time.tv_sec = curr_timeval.tv_sec;
    sender->timeout_time.tv_usec = curr_timeval.tv_usec;

    // Add timeout duration to determine next timeout
    sender->timeout_time.tv_usec += 90000;

    if (sender->timeout_time.tv_usec >= 1000000) {
        sender->timeout_time.tv_sec += 1;
        sender->timeout_time.tv_usec -= 1000000;
    }

    sender->timeout_relevant = 1;

    //	fprintf(stderr, "updating timeout time to %d secs and %d usecs\n",
    //			sender->timeout_time.tv_sec,
    //sender->timeout_time.tv_usec);
}

struct timeval* sender_get_next_expiring_timeval(Sender* sender) {
    // TODO: You should fill in this function so that it returns the next
    // timeout that should occur

    // Only return timeout time if waiting for ack and time out info is relevant
    if (sender->last_outgoing_charbuf && sender->timeout_relevant) {
        //		fprintf(stderr, "returning timeout\n");
        return &sender->timeout_time;
    }

    return NULL;
}

void handle_incoming_acks(Sender* sender, LLnode** outgoing_frames_head_ptr) {
    // TODO: Suggested steps for handling incoming ACKs
    //    1) Dequeue the ACK from the sender->input_framelist_head
    //    2) Convert the char * buffer to a Frame data type
    //    3) Check whether the frame is for this sender
    //    4) Do stop-and-wait for sender/receiver pair

    int accepted = 0;

    while (ll_get_length(sender->input_framelist_head) > 0) {
        //		fprintf(stderr, "input_framelist_head for sender %d is
        //%p\n", 				sender->send_id, sender->input_framelist_head);

        LLnode* ack_node = ll_pop_node(&sender->input_framelist_head);

        // If good ack already received, done (cannot correctly receive more
        // than one good ack for stop and wait, as the next packet is yet to be
        // sent)
        if (accepted) {
            free(ack_node);
            free(ack_node->value);
            continue;
        }

        char* ack_char_buf = (char*) ack_node->value;

        Frame* ack_frame = convert_char_to_frame(ack_char_buf);

        free(ack_node);
        free(ack_char_buf);

        // Only �are if acknowledgment is for this sender (otherwise ignore)
        if (ack_frame->dst_id == sender->send_id) {

            //			fprintf(stderr, "Ack frame received for
            //sender\n");

            int receiver_id = ack_frame->src_id;

            // If latest ack received, can increment ack seq_num and "reset"
            // timeout Note: Only do if seq num acknowledged is latest (ignore
            // ack for prev)
            if (ack_frame->seq_num == sender->seq_num_arr[receiver_id]) {

                accepted = 1;

                //				fprintf(stderr, "Accepted ack arrived
                //with sender %d, receiver %d, seq_num %d\n", 								ack_frame->dst_id,
                //ack_frame->src_id, ack_frame->seq_num);

                sender->seq_num_arr[receiver_id]++; // Increment acked seq num

                // Reset last send frame charbuf info since successfully acked
                free(sender->last_outgoing_charbuf);

                // Important reset as indicates that not waiting for ack
                sender->last_outgoing_charbuf = NULL;

                // Old timeout value no longer relevant
                sender->timeout_relevant = 0;

                // If there is message in buffer send it
                // This accounts for regular and partition case
                if (ll_get_length(sender->outgoing_charbuf_buffer) > 0) {

                    LLnode* next_node =
                        ll_pop_node(&sender->outgoing_charbuf_buffer);

                    char* outgoing_charbuf = next_node->value;
                    // Save charbuf for retransmission in case dropped
                    sender->last_outgoing_charbuf = malloc(sizeof(Frame));
                    memcpy(sender->last_outgoing_charbuf, outgoing_charbuf,
                           sizeof(Frame));

                    ll_append_node(outgoing_frames_head_ptr, outgoing_charbuf);

                    free(next_node);
                }

            } else {
                //				fprintf(stderr, "Old ack arrived (prev
                //seq num) with sender %d, receiver %d, seq_num %d\n",
                //						ack_frame->dst_id,
                //ack_frame->src_id, ack_frame->seq_num);
            }
        }

        free(ack_frame);
    }
}

void handle_input_cmds(Sender* sender, LLnode** outgoing_frames_head_ptr) {
    // TODO: Suggested steps for handling input cmd
    //    1) Dequeue the Cmd from sender->input_cmdlist_head
    //    2) Convert to Frame
    //    3) Set up the frame according to the protocol

    int input_cmd_length = ll_get_length(sender->input_cmdlist_head);

    // Recheck the command queue length to see if stdin_thread dumped a command
    // on us
    input_cmd_length = ll_get_length(sender->input_cmdlist_head);
    while (input_cmd_length > 0) {
        // Pop a node off and update the input_cmd_length
        LLnode* ll_input_cmd_node = ll_pop_node(&sender->input_cmdlist_head);
        input_cmd_length = ll_get_length(sender->input_cmdlist_head);

        // Cast to Cmd type and free up the memory for the node
        Cmd* outgoing_cmd = (Cmd*) ll_input_cmd_node->value;
        free(ll_input_cmd_node);

        // Skip if not for this sender
        if (outgoing_cmd->src_id != sender->send_id) {
            fprintf(stderr,
                    "ERROR: NOT MATCHING Command for sender %d but this sender "
                    "is %d\n",
                    outgoing_cmd->src_id, sender->send_id);
            continue;
        }

        // DUMMY CODE: Add the raw char buf to the outgoing_frames list
        // NOTE: You should not blindly send this message out!
        //      Ask yourself: Is this message actually going to the right
        //      receiver (recall that default behavior of send is to broadcast
        //      to all receivers)?
        //                    Were the previous messages sent to this receiver
        //                    ACTUALLY delivered to the receiver?

        // Add 1 to account for space for null character
        int msg_length = 1 + strlen(outgoing_cmd->message);
        if (msg_length > FRAME_PAYLOAD_SIZE) {
            // Do something about msg that would make frame size > 64 bytes

            // Remaining length to process in outgoing_cmd->message
            int remaining_msg_length = msg_length;
            // Position in outgoing_cmd->message
            int curr_msg_idx = 0;

            while (remaining_msg_length > 0) {
                // Break messages into frames
                Frame* outgoing_frame = malloc(sizeof(Frame));

                int dst_id = outgoing_cmd->dst_id;

                outgoing_frame->src_id = outgoing_cmd->src_id; // = sender id
                outgoing_frame->dst_id = dst_id;
                // Assign next seq num for each frame
                outgoing_frame->seq_num =
                    sender->assign_seq_num_arr[outgoing_cmd->dst_id];
                // Increment next seq num to assign
                sender->assign_seq_num_arr[outgoing_cmd->dst_id]++;

                // If this is end of message
                outgoing_frame->message_end =
                    remaining_msg_length <= FRAME_PAYLOAD_SIZE - 1;

                if (outgoing_frame->message_end) {
                    strcpy(outgoing_frame->data,
                           outgoing_cmd->message + curr_msg_idx);
                    //					fprintf(stderr, "message end reached and is :
                    //%s\n", outgoing_frame->data);
                } else {
                    // Copy over part of message
                    strncpy(outgoing_frame->data,
                            outgoing_cmd->message + curr_msg_idx,
                            FRAME_PAYLOAD_SIZE - 1);
                    // End every partition message with null to work with easier
                    // on receiver
                    outgoing_frame->data[FRAME_PAYLOAD_SIZE - 1] = '\0';
                }

                // Convert the frame to the charbuf
                char* outgoing_charbuf = convert_frame_to_char(outgoing_frame);

                // Check to see if should send now (last message is acked and
                // none in buffer)
                if (sender->last_outgoing_charbuf != NULL ||
                    ll_get_length(sender->outgoing_charbuf_buffer) > 0) {

                    // Put frame in buffer
                    ll_append_node(&sender->outgoing_charbuf_buffer,
                                   outgoing_charbuf);

                } else {
                    // Ready to send

                    // Save frame charbuf for retransmission in case dropped
                    sender->last_outgoing_charbuf = malloc(sizeof(Frame));
                    memcpy(sender->last_outgoing_charbuf, outgoing_charbuf,
                           sizeof(Frame));

                    ll_append_node(outgoing_frames_head_ptr, outgoing_charbuf);
                }

                free(outgoing_frame);

                remaining_msg_length -= FRAME_PAYLOAD_SIZE - 1;
                // Update position in message to start for next iteration
                curr_msg_idx += FRAME_PAYLOAD_SIZE - 1;
            }

            // Done with outgoing command
            free(outgoing_cmd->message);
            free(outgoing_cmd);

        } else {
            // This is probably ONLY one step you want
            Frame* outgoing_frame = malloc(sizeof(Frame));
            assert(outgoing_frame);

            int dst_id = outgoing_cmd->dst_id;

            outgoing_frame->src_id = outgoing_cmd->src_id; // = sender id
            outgoing_frame->dst_id = dst_id;
            outgoing_frame->message_end = 1;

            outgoing_frame->seq_num = sender->assign_seq_num_arr[dst_id];
            sender->assign_seq_num_arr[dst_id]++;

            strcpy(outgoing_frame->data, outgoing_cmd->message);

            // At this point, we don't need the outgoing_cmd
            free(outgoing_cmd->message);
            free(outgoing_cmd);

            // Convert the frame to the outgoing_charbuf
            char* outgoing_charbuf = convert_frame_to_char(outgoing_frame);

            // Check to see if should send now (last message is acked and none
            // in buffer)
            if (sender->last_outgoing_charbuf != NULL ||
                ll_get_length(sender->outgoing_charbuf_buffer) > 0) {

                // Put frame in buffer
                ll_append_node(&sender->outgoing_charbuf_buffer,
                               outgoing_charbuf);

            } else {
                // Ready to send

                // Save frame charbuf for retransmission in case dropped
                sender->last_outgoing_charbuf = malloc(sizeof(Frame));
                memcpy(sender->last_outgoing_charbuf, outgoing_charbuf,
                       sizeof(Frame));

                ll_append_node(outgoing_frames_head_ptr, outgoing_charbuf);
            }

            free(outgoing_frame);
        }
    }
}

void handle_timedout_frames(Sender* sender, LLnode** outgoing_frames_head_ptr) {
    // TODO: Handle timeout by resending the appropriate message

    struct timeval curr_timeval;
    struct timeval* expiring_timeval;

    // Get the current time
    gettimeofday(&curr_timeval, NULL);

    // Check for the next event we should handle
    expiring_timeval = sender_get_next_expiring_timeval(sender);

    // If not waiting for ack do nothing
    if (!expiring_timeval) {
        return;
    }

    long diff = timeval_usecdiff(expiring_timeval, &curr_timeval);
    // If timed out and no ack received, resend frame charbuf
    if (diff >= 0) {
        //		fprintf(stderr, "timed out with diff = %ld\n", diff);

        // If not acked, last charbuf pointer in sender will be non-null
        if (sender->last_outgoing_charbuf) {
            // Create copy to resend (so that send_msg_to_receiver frees the
            // copy and we still have the charbuf in case of another dropped
            // packet)
            char* resend_charbuf = malloc(sizeof(Frame));
            memcpy(resend_charbuf, sender->last_outgoing_charbuf,
                   sizeof(Frame));

            ////////// debug prints
            //			Frame* resend_frame =
            //convert_char_to_frame(resend_charbuf); 			fprintf(stderr, "Resending
            //frame with sender %d, receiver %d, seq_num %d," 					"and data %s\n",
            //resend_frame->src_id, resend_frame->dst_id, 					resend_frame->seq_num,
            //resend_frame->data); 			fprintf(stderr, "Curr time is %d secs and %d
            //usecs\n", 					curr_timeval.tv_sec, curr_timeval.tv_usec);
            //////////

            // Add to list to resend last frame charbuf info
            ll_append_node(outgoing_frames_head_ptr, resend_charbuf);

        } else {
            fprintf(stderr,
                    "Error: should not reach here: timeout yet acked\n");
        }
    }
}

void* run_sender(void* input_sender) {
    struct timespec time_spec;
    struct timeval curr_timeval;
    const int WAIT_SEC_TIME = 0;
    const long WAIT_USEC_TIME = 100000;
    Sender* sender = (Sender*) input_sender;
    LLnode* outgoing_frames_head;
    struct timeval* expiring_timeval;
    long sleep_usec_time, sleep_sec_time;

    // This incomplete sender thread, at a high level, loops as follows:
    // 1. Determine the next time the thread should wake up
    // 2. Grab the mutex protecting the input_cmd/inframe queues
    // 3. Dequeues messages from the input queue and adds them to the
    // outgoing_frames list
    // 4. Releases the lock
    // 5. Sends out the messages

    while (1) {
        outgoing_frames_head = NULL;

        // Get the current time
        gettimeofday(&curr_timeval, NULL);

        // time_spec is a data structure used to specify when the thread should
        // wake up The time is specified as an ABSOLUTE (meaning, conceptually,
        // you specify 9/23/2010 @ 1pm, wakeup)
        time_spec.tv_sec = curr_timeval.tv_sec;
        time_spec.tv_nsec = curr_timeval.tv_usec * 1000;

        // Check for the next event we should handle
        expiring_timeval = sender_get_next_expiring_timeval(sender);

        // Perform full on timeout
        if (expiring_timeval == NULL) {
            time_spec.tv_sec += WAIT_SEC_TIME;
            time_spec.tv_nsec += WAIT_USEC_TIME * 1000;
        } else {
            // Take the difference between the next event and the current time
            sleep_usec_time = timeval_usecdiff(&curr_timeval, expiring_timeval);

            // Sleep if the difference is positive
            if (sleep_usec_time > 0) {
                sleep_sec_time = sleep_usec_time / 1000000;
                sleep_usec_time = sleep_usec_time % 1000000;
                time_spec.tv_sec += sleep_sec_time;
                time_spec.tv_nsec += sleep_usec_time * 1000;
            }
        }

        // Check to make sure we didn't "overflow" the nanosecond field
        if (time_spec.tv_nsec >= 1000000000) {
            time_spec.tv_sec++;
            time_spec.tv_nsec -= 1000000000;
        }

        //*****************************************************************************************
        // NOTE: Anything that involves dequeing from the input frames or input
        // commands should go
        //      between the mutex lock and unlock, because other threads
        //      CAN/WILL access these structures
        //*****************************************************************************************
        pthread_mutex_lock(&sender->buffer_mutex);

        // Check whether anything has arrived
        int input_cmd_length = ll_get_length(sender->input_cmdlist_head);
        int inframe_queue_length = ll_get_length(sender->input_framelist_head);

        // Nothing (cmd nor incoming frame) has arrived, so do a timed wait on
        // the sender's condition variable (releases lock) A signal on the
        // condition variable will wakeup the thread and reaquire the lock
        if (input_cmd_length == 0 && inframe_queue_length == 0) {
            pthread_cond_timedwait(&sender->buffer_cv, &sender->buffer_mutex,
                                   &time_spec);
        }
        // Implement this
        handle_incoming_acks(sender, &outgoing_frames_head);

        // Implement this
        handle_input_cmds(sender, &outgoing_frames_head);

        pthread_mutex_unlock(&sender->buffer_mutex);

        // Implement this
        handle_timedout_frames(sender, &outgoing_frames_head);

        // CHANGE THIS AT YOUR OWN RISK!
        // Send out all the frames
        int ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);

        while (ll_outgoing_frame_length > 0) {
            LLnode* ll_outframe_node = ll_pop_node(&outgoing_frames_head);
            char* char_buf = (char*) ll_outframe_node->value;

            update_timeout_time(sender);

            // Don't worry about freeing the char_buf, the following function
            // does that
            send_msg_to_receivers(char_buf);

            // Free up the ll_outframe_node
            free(ll_outframe_node);

            ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        }
    }
    pthread_exit(NULL);
    return 0;
}
