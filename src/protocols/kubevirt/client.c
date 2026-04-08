/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "config.h"

#include "client.h"
#include "common/clipboard.h"
#include "keyboard.h"
#include "kubevirt.h"
#include "settings.h"
#include "user.h"

#include <guacamole/client.h>
#include <guacamole/display.h>
#include <guacamole/mem.h>
#include <guacamole/recording.h>

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/**
 * A pending join handler implementation that will synchronize the connection
 * state for all pending users prior to them being promoted to full user.
 *
 * @param client
 *     The client whose pending users are about to be promoted.
 *
 * @return
 *     Always zero.
 */
static int guac_kubevirt_join_pending_handler(guac_client* client) {

    guac_kubevirt_client* kubevirt_client = (guac_kubevirt_client*) client->data;
    guac_socket* broadcast_socket = client->pending_socket;

    /* Synchronize with current display */
    if (kubevirt_client->display != NULL) {
        guac_display_dup(kubevirt_client->display, broadcast_socket);
        guac_socket_flush(broadcast_socket);
    }

    return 0;
}

int guac_client_init(guac_client* client) {

    /* Set client args */
    client->args = GUAC_KUBEVIRT_CLIENT_ARGS;

    /* Alloc client data */
    guac_kubevirt_client* kubevirt_client = guac_mem_zalloc(sizeof(guac_kubevirt_client));
    client->data = kubevirt_client;

    /* Initialize the message lock */
    pthread_mutex_init(&kubevirt_client->message_lock, NULL);

    /* Initialize socketpair file descriptors to -1 */
    kubevirt_client->vnc_socket_pair[0] = -1;
    kubevirt_client->vnc_socket_pair[1] = -1;

    /* Initialize clipboard */
    kubevirt_client->clipboard = guac_common_clipboard_alloc(262144);

    /* Set handlers */
    client->join_handler = guac_kubevirt_user_join_handler;
    client->join_pending_handler = guac_kubevirt_join_pending_handler;
    client->leave_handler = guac_kubevirt_user_leave_handler;
    client->free_handler = guac_kubevirt_client_free_handler;

    return 0;
}

int guac_kubevirt_client_free_handler(guac_client* client) {

    guac_kubevirt_client* kubevirt_client = (guac_kubevirt_client*) client->data;
    guac_kubevirt_settings* settings = kubevirt_client->settings;

    /* Ensure all background rendering processes are stopped before freeing */
    if (kubevirt_client->display != NULL)
        guac_display_stop(kubevirt_client->display);

    /* Clean up client thread */
    if (kubevirt_client->client_thread) {
        pthread_join(kubevirt_client->client_thread, NULL);
    }

    /* Clean up recording, if in progress */
    if (kubevirt_client->recording != NULL)
        guac_recording_free(kubevirt_client->recording);

    /* Free clipboard */
    if (kubevirt_client->clipboard != NULL)
        guac_common_clipboard_free(kubevirt_client->clipboard);

    /* Free keyboard */
    if (kubevirt_client->keyboard != NULL)
        guac_kubevirt_keyboard_free(kubevirt_client->keyboard);

    /* Free display */
    if (kubevirt_client->display != NULL)
        guac_display_free(kubevirt_client->display);

    /* Free server name */
    if (kubevirt_client->server_name != NULL)
        free(kubevirt_client->server_name);

    /* Free rectangle buffer */
    if (kubevirt_client->rect_buffer != NULL)
        free(kubevirt_client->rect_buffer);

    /* Free parsed settings */
    if (settings != NULL)
        guac_kubevirt_settings_free(settings);

    /* Clean up the message lock */
    pthread_mutex_destroy(&kubevirt_client->message_lock);

    /* Free generic data struct */
    guac_mem_free(client->data);

    return 0;
}
