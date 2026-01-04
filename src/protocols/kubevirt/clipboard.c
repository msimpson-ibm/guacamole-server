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

#include "clipboard.h"
#include "common/clipboard.h"
#include "kubevirt.h"

#include <guacamole/client.h>
#include <guacamole/stream.h>
#include <guacamole/user.h>

#include <rfb/rfbproto.h>
#include <stdlib.h>
#include <string.h>

int guac_kubevirt_clipboard_receive_handler(guac_user* user,
        guac_stream* stream, char* mimetype) {

    guac_client* client = user->client;
    guac_kubevirt_client* kubevirt_client =
        (guac_kubevirt_client*) client->data;

    /* Ignore stream creation if no clipboard structure is available */
    guac_common_clipboard* clipboard = kubevirt_client->clipboard;
    if (clipboard == NULL)
        return 0;

    /* Clear clipboard and prepare for new data */
    guac_common_clipboard_reset(clipboard, mimetype);

    /* Set handlers for clipboard stream */
    stream->blob_handler = guac_kubevirt_clipboard_blob_handler;
    stream->end_handler = guac_kubevirt_clipboard_end_handler;

    return 0;
}

int guac_kubevirt_clipboard_blob_handler(guac_user* user, guac_stream* stream,
        void* data, int length) {

    guac_client* client = user->client;
    guac_kubevirt_client* kubevirt_client =
        (guac_kubevirt_client*) client->data;

    /* Append data to clipboard */
    guac_common_clipboard_append(kubevirt_client->clipboard, (char*) data, length);

    return 0;
}

int guac_kubevirt_clipboard_end_handler(guac_user* user, guac_stream* stream) {

    guac_client* client = user->client;
    guac_kubevirt_client* kubevirt_client =
        (guac_kubevirt_client*) client->data;

    const char* clipboard_data = kubevirt_client->clipboard->buffer;
    int length = kubevirt_client->clipboard->length;

    /* Broadcast to all users */
    guac_common_clipboard_send(kubevirt_client->clipboard, client);

    /* Send ClientCutText message to VNC server via WebSocket */
    pthread_mutex_lock(&kubevirt_client->message_lock);
    if (kubevirt_client->wsi != NULL && kubevirt_client->vnc_state == GUAC_KUBEVIRT_STATE_CONNECTED) {

        /* VNC ClientCutText message format:
         * 1 byte: message type (6)
         * 3 bytes: padding
         * 4 bytes: length
         * N bytes: text
         */
        int message_length = 8 + length;
        unsigned char* buffer = malloc(message_length);

        buffer[0] = rfbClientCutText;
        buffer[1] = 0;
        buffer[2] = 0;
        buffer[3] = 0;
        buffer[4] = (length >> 24) & 0xFF;
        buffer[5] = (length >> 16) & 0xFF;
        buffer[6] = (length >> 8) & 0xFF;
        buffer[7] = length & 0xFF;
        memcpy(buffer + 8, clipboard_data, length);

        lws_write(kubevirt_client->wsi, buffer, message_length, LWS_WRITE_BINARY);
        free(buffer);
    }
    pthread_mutex_unlock(&kubevirt_client->message_lock);

    return 0;
}

void guac_kubevirt_clipboard_send(guac_client* client,
        const char* data, int length) {

    guac_kubevirt_client* kubevirt_client =
        (guac_kubevirt_client*) client->data;

    /* Update internal clipboard and broadcast to all users */
    guac_common_clipboard_reset(kubevirt_client->clipboard, "text/plain");
    guac_common_clipboard_append(kubevirt_client->clipboard, data, length);
    guac_common_clipboard_send(kubevirt_client->clipboard, client);
}
