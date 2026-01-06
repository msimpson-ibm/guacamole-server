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

#ifndef GUAC_KUBEVIRT_KUBEVIRT_H
#define GUAC_KUBEVIRT_KUBEVIRT_H

#include "common/clipboard.h"
#include "settings.h"

#include <guacamole/client.h>
#include <guacamole/display.h>
#include <guacamole/layer.h>
#include <guacamole/recording.h>

#include <libwebsockets.h>
#include <pthread.h>
#include <rfb/rfbclient.h>
#include <rfb/rfbproto.h>

/**
 * The WebSocket subprotocol to use for KubeVirt VNC console connections.
 */
#define GUAC_KUBEVIRT_LWS_PROTOCOL "binary.k8s.io"

/**
 * The maximum number of outbound messages to buffer.
 */
#define GUAC_KUBEVIRT_MAX_OUTBOUND_MESSAGES 32

/**
 * The maximum amount of time to wait for a libwebsockets event, in milliseconds.
 */
#define GUAC_KUBEVIRT_SERVICE_INTERVAL 50

/**
 * Buffer size for VNC protocol messages.
 */
#define GUAC_KUBEVIRT_VNC_BUFFER_SIZE 16384

/**
 * KubeVirt-specific client data.
 */
typedef struct guac_kubevirt_client {

    /**
     * The KubeVirt client thread.
     */
    pthread_t client_thread;

    /**
     * Lock which synchronizes access to the WebSocket connection.
     */
    pthread_mutex_t message_lock;

    /**
     * Client settings, parsed from args.
     */
    guac_kubevirt_settings* settings;

    /**
     * The current display state.
     */
    guac_display* display;

    /**
     * The context of the current drawing (update) operation, if any. If no
     * operation is in progress, this will be NULL.
     */
    guac_display_layer_raw_context* current_context;

    /**
     * The current instance of the guac_display render thread. If the thread
     * has not yet been started, this will be NULL.
     */
    guac_display_render_thread* render_thread;

    /**
     * Internal clipboard.
     */
    guac_common_clipboard* clipboard;

    /**
     * The in-progress session recording, or NULL if no recording is in
     * progress.
     */
    guac_recording* recording;

    /**
     * The libwebsockets context associated with the WebSocket connection.
     */
    struct lws_context* context;

    /**
     * The connected WebSocket.
     */
    struct lws* wsi;

    /**
     * Unix socket pair for bridging WebSocket to libvncclient.
     * vnc_socket_pair[0] = libvncclient end
     * vnc_socket_pair[1] = WebSocket forwarding end
     */
    int vnc_socket_pair[2];

    /**
     * Thread that runs the VNC client (libvncclient).
     */
    pthread_t vnc_client_thread;

    /**
     * Thread that reads from vnc_socket_pair[1] and writes to WebSocket.
     */
    pthread_t socket_to_ws_thread;

    /**
     * Flag to signal threads to terminate.
     */
    int stop_threads;

    /**
     * The libvncclient rfbClient instance.
     */
    rfbClient* rfb_client;

    /**
     * VNC framebuffer width.
     */
    int width;

    /**
     * VNC framebuffer height.
     */
    int height;

    /**
     * VNC protocol state.
     */
    enum {
        GUAC_KUBEVIRT_STATE_WAITING_FOR_VERSION,
        GUAC_KUBEVIRT_STATE_WAITING_FOR_SECURITY,
        GUAC_KUBEVIRT_STATE_WAITING_FOR_SECURITY_RESULT,
        GUAC_KUBEVIRT_STATE_WAITING_FOR_SERVER_INIT,
        GUAC_KUBEVIRT_STATE_CONNECTED
    } vnc_state;

    /**
     * Buffer for incoming VNC data.
     */
    unsigned char vnc_buffer[GUAC_KUBEVIRT_VNC_BUFFER_SIZE];

    /**
     * Number of bytes currently in the VNC buffer.
     */
    int vnc_buffer_length;

    /**
     * VNC pixel format.
     */
    rfbPixelFormat pixel_format;

    /**
     * VNC server name.
     */
    char* server_name;

    /**
     * Whether the client should be in read-only mode.
     */
    int read_only;

    /**
     * Number of rectangles remaining to parse in current framebuffer update.
     */
    int rectangles_remaining;

    /**
     * Current rectangle being parsed.
     */
    struct {
        int x, y, width, height;
        int32_t encoding;
        int bytes_received;
        int bytes_expected;
    } current_rect;

    /**
     * Buffer for accumulating rectangle pixel data.
     */
    unsigned char* rect_buffer;

    /**
     * Size of the rectangle buffer.
     */
    size_t rect_buffer_size;

} guac_kubevirt_client;

/**
 * KubeVirt client thread. This thread initiates the WebSocket connection
 * to the KubeVirt API and handles the VNC protocol over WebSocket.
 *
 * @param data
 *     The guac_client instance associated with the requested KubeVirt connection.
 *
 * @return
 *     Always NULL.
 */
void* guac_kubevirt_client_thread(void* data);

#endif
