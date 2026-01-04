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
#include "kubevirt.h"
#include "settings.h"
#include "ssl.h"

#include <guacamole/client.h>
#include <guacamole/display.h>
#include <guacamole/layer.h>
#include <guacamole/protocol.h>
#include <guacamole/rect.h>
#include <guacamole/socket.h>

#include <libwebsockets.h>
#include <openssl/ssl.h>
#include <rfb/rfbproto.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* Global reference to current client for libwebsockets callbacks */
static guac_client* current_client = NULL;

/* Forward declarations */
static void* guac_kubevirt_vnc_client_thread(void* data);
static void* guac_kubevirt_socket_to_ws_thread(void* data);

/**
 * libwebsockets callback for WebSocket events.
 */
static int guac_kubevirt_lws_callback(struct lws* wsi,
        enum lws_callback_reasons reason, void* user,
        void* in, size_t len) {

    guac_client* client = current_client;
    if (client == NULL)
        return -1;

    guac_kubevirt_client* kubevirt_client =
        (guac_kubevirt_client*) client->data;

    switch (reason) {

        /* Complete initialization of SSL */
        case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS:
            guac_kubevirt_init_ssl(client, (SSL_CTX*) user);
            break;

        /* Add authentication header during WebSocket handshake */
        case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER: {
            guac_kubevirt_settings* settings = kubevirt_client->settings;
            unsigned char **p = (unsigned char **)in;
            unsigned char *end = (*p) + len;

            /* Add Authorization header with bearer token if provided */
            if (settings->token != NULL) {
                /* Format the authorization header value as "Bearer <token>" */
                char auth_header[1024];
                snprintf(auth_header, sizeof(auth_header), "Bearer %s", settings->token);

                if (lws_add_http_header_by_name(wsi,
                        (unsigned char *)"Authorization:",
                        (unsigned char *)auth_header,
                        strlen(auth_header), p, end)) {
                    guac_client_log(client, GUAC_LOG_ERROR,
                            "Failed to add Authorization header");
                    return -1;
                }
                guac_client_log(client, GUAC_LOG_DEBUG,
                        "Added Authorization header to WebSocket handshake");
            }
            break;
        }

        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            guac_client_log(client, GUAC_LOG_INFO,
                    "WebSocket connection established to KubeVirt VNC console");
            kubevirt_client->wsi = wsi;
            kubevirt_client->vnc_state = GUAC_KUBEVIRT_STATE_WAITING_FOR_VERSION;

            /* Start VNC client thread (will initialize rfbClient and handle VNC messages) */
            if (pthread_create(&kubevirt_client->vnc_client_thread, NULL,
                        guac_kubevirt_vnc_client_thread, client) != 0) {
                guac_client_log(client, GUAC_LOG_ERROR,
                        "Failed to create VNC client thread");
                return -1;
            }

            /* Start socket → WebSocket forwarding thread */
            if (pthread_create(&kubevirt_client->socket_to_ws_thread, NULL,
                        guac_kubevirt_socket_to_ws_thread, client) != 0) {
                guac_client_log(client, GUAC_LOG_ERROR,
                        "Failed to create socket to WebSocket forwarding thread");
                return -1;
            }

            guac_client_log(client, GUAC_LOG_DEBUG,
                    "Started VNC client thread and socket forwarding thread");
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE:
            /* Forward WebSocket data to socketpair (libvncclient will read from it) */
            guac_client_log(client, GUAC_LOG_DEBUG,
                    "Received %zu bytes from VNC server, forwarding to socketpair", len);

            pthread_mutex_lock(&kubevirt_client->message_lock);
            if (kubevirt_client->vnc_socket_pair[1] >= 0) {
                ssize_t written = write(kubevirt_client->vnc_socket_pair[1], in, len);
                if (written != (ssize_t)len) {
                    guac_client_log(client, GUAC_LOG_ERROR,
                            "Failed to write all data to socketpair: %zd/%zu bytes",
                            written, len);
                    pthread_mutex_unlock(&kubevirt_client->message_lock);
                    return -1;
                }
            }
            pthread_mutex_unlock(&kubevirt_client->message_lock);

            /* Request callback when writable in case we need to send data */
            lws_callback_on_writable(wsi);
            break;

        case LWS_CALLBACK_CLIENT_WRITEABLE:
            /* Ready to send data if needed */
            break;

        case LWS_CALLBACK_CLOSED:
            guac_client_log(client, GUAC_LOG_INFO,
                    "WebSocket connection closed");
            kubevirt_client->wsi = NULL;
            guac_client_stop(client);
            break;

        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            guac_client_log(client, GUAC_LOG_ERROR,
                    "WebSocket connection error: %s",
                    in ? (char*)in : "unknown error");
            kubevirt_client->wsi = NULL;
            guac_client_stop(client);
            break;

        default:
            break;
    }

    return 0;
}

static struct lws_protocols protocols[] = {
    {
        GUAC_KUBEVIRT_LWS_PROTOCOL,
        guac_kubevirt_lws_callback,
        0,
        4096,
        0, NULL, 0
    },
    { NULL, NULL, 0, 0, 0, NULL, 0 }
};

/**
 * Callback invoked by libvncclient when framebuffer size changes.
 */
static rfbBool guac_kubevirt_resize_framebuffer(rfbClient* rfb_client) {

    guac_client* client = rfbClientGetClientData(rfb_client, NULL);
    guac_kubevirt_client* kubevirt_client =
        (guac_kubevirt_client*) client->data;

    guac_client_log(client, GUAC_LOG_INFO,
            "VNC framebuffer resized to %dx%d",
            rfb_client->width, rfb_client->height);

    /* Update display size */
    kubevirt_client->width = rfb_client->width;
    kubevirt_client->height = rfb_client->height;

    /* Create display if it doesn't exist yet */
    if (kubevirt_client->display == NULL) {
        kubevirt_client->display = guac_display_alloc(client);
    }

    /* Ensure the display layer is the correct size */
    guac_display_layer* default_layer =
        guac_display_default_layer(kubevirt_client->display);
    guac_display_layer_resize(default_layer,
            rfb_client->width, rfb_client->height);

    /* Start render thread if not already started */
    if (kubevirt_client->render_thread == NULL) {
        kubevirt_client->render_thread =
            guac_display_render_thread_create(kubevirt_client->display);
        guac_client_log(client, GUAC_LOG_INFO,
                "Render thread started for display updates");

        /* Sync display with already-connected users
         * This sends the current display state (size info) to prevent handshake timeout
         * join_pending_handler will handle users that join later */
        if (client->socket != NULL) {
            guac_display_dup(kubevirt_client->display, client->socket);
            guac_socket_flush(client->socket);
        }
    }

    /* Allocate framebuffer */
    rfb_client->frameBuffer = malloc(rfb_client->width * rfb_client->height * 4);
    if (!rfb_client->frameBuffer) {
        guac_client_log(client, GUAC_LOG_ERROR,
                "Failed to allocate framebuffer");
        return FALSE;
    }

    guac_client_log(client, GUAC_LOG_INFO,
            "Allocated framebuffer at %p, size=%d bytes",
            rfb_client->frameBuffer,
            rfb_client->width * rfb_client->height * 4);

    return TRUE;
}

/**
 * Callback invoked by libvncclient when a framebuffer update is received.
 */
static void guac_kubevirt_framebuffer_update(rfbClient* rfb_client,
        int x, int y, int w, int h) {

    guac_client* client = rfbClientGetClientData(rfb_client, NULL);
    guac_kubevirt_client* kubevirt_client =
        (guac_kubevirt_client*) client->data;

    /* Use the current context that was opened in the VNC client thread */
    guac_display_layer_raw_context* context = kubevirt_client->current_context;
    if (context == NULL) {
        guac_client_log(client, GUAC_LOG_WARNING,
                "Framebuffer update received but no context is open");
        return;
    }

    /* Copy updated region from framebuffer to display layer
     * rfb_client->frameBuffer is in BGRA format (32 bits per pixel) */
    for (int row = 0; row < h; row++) {
        size_t dst_offset = (y + row) * context->stride + x * 4;
        size_t src_offset = (y + row) * rfb_client->width * 4 + x * 4;
        memcpy(
            context->buffer + dst_offset,
            rfb_client->frameBuffer + src_offset,
            w * 4
        );
    }

    /* Mark the updated region as dirty */
    guac_rect update_rect;
    guac_rect_init(&update_rect, x, y, w, h);
    guac_rect_extend(&context->dirty, &update_rect);

    /* Notify the render thread that the display has been modified */
    guac_display_render_thread_notify_modified(kubevirt_client->render_thread);
}

/**
 * Initializes libvncclient with the socketpair.
 *
 * @param client
 *     The guac_client associated with the KubeVirt connection.
 *
 * @return
 *     Zero on success, non-zero on failure.
 */
static int guac_kubevirt_init_rfb_client(guac_client* client) {

    guac_kubevirt_client* kubevirt_client =
        (guac_kubevirt_client*) client->data;
    guac_kubevirt_settings* settings = kubevirt_client->settings;

    /* Create rfbClient for 32-bit color (8 bits per channel, 4 bytes per pixel) */
    kubevirt_client->rfb_client = rfbGetClient(8, 3, 4);
    if (!kubevirt_client->rfb_client) {
        guac_client_log(client, GUAC_LOG_ERROR,
                "Failed to create rfbClient");
        return -1;
    }

    /* Store guac_client reference in rfbClient for use in callbacks */
    rfbClientSetClientData(kubevirt_client->rfb_client, NULL, client);

    /* Set up callbacks */
    kubevirt_client->rfb_client->MallocFrameBuffer = guac_kubevirt_resize_framebuffer;
    kubevirt_client->rfb_client->GotFrameBufferUpdate = guac_kubevirt_framebuffer_update;

    /* Set the socket to our socketpair */
    kubevirt_client->rfb_client->sock = kubevirt_client->vnc_socket_pair[0];
    kubevirt_client->rfb_client->serverHost = strdup("kubevirt-websocket");
    kubevirt_client->rfb_client->serverPort = 0;

    guac_client_log(client, GUAC_LOG_DEBUG,
            "Initialized rfbClient with socketpair fd=%d",
            kubevirt_client->rfb_client->sock);

    /* Perform VNC handshake using InitialiseRFBConnection which works with
     * an already-connected socket (doesn't try to connect) */
    if (!InitialiseRFBConnection(kubevirt_client->rfb_client)) {
        guac_client_log(client, GUAC_LOG_ERROR,
                "Failed to initialize RFB connection");
        rfbClientCleanup(kubevirt_client->rfb_client);
        kubevirt_client->rfb_client = NULL;
        return -1;
    }

    /* Copy framebuffer dimensions from ServerInit message to rfbClient structure.
     * InitialiseRFBConnection reads the ServerInit but doesn't populate these fields. */
    kubevirt_client->rfb_client->width = kubevirt_client->rfb_client->si.framebufferWidth;
    kubevirt_client->rfb_client->height = kubevirt_client->rfb_client->si.framebufferHeight;

    guac_client_log(client, GUAC_LOG_INFO,
            "VNC connection initialized. Screen: %dx%d, manually allocating framebuffer",
            kubevirt_client->rfb_client->width,
            kubevirt_client->rfb_client->height);

    /* Manually call MallocFrameBuffer if it wasn't called during initialization */
    if (kubevirt_client->rfb_client->frameBuffer == NULL &&
        kubevirt_client->rfb_client->MallocFrameBuffer != NULL) {

        guac_client_log(client, GUAC_LOG_DEBUG,
                "Calling MallocFrameBuffer callback manually");

        if (!kubevirt_client->rfb_client->MallocFrameBuffer(kubevirt_client->rfb_client)) {
            guac_client_log(client, GUAC_LOG_ERROR,
                    "Failed to allocate framebuffer");
            rfbClientCleanup(kubevirt_client->rfb_client);
            kubevirt_client->rfb_client = NULL;
            return -1;
        }
    }

    guac_client_log(client, GUAC_LOG_INFO,
            "Framebuffer allocated. Screen: %dx%d, now setting format and encodings",
            kubevirt_client->rfb_client->width,
            kubevirt_client->rfb_client->height);

    /* Set pixel format to match guac_display expectations (BGRA/BGRX format)
     * This must match the server's native format (red shift 16, green 8, blue 0) */
    kubevirt_client->rfb_client->format.bitsPerPixel = 32;
    kubevirt_client->rfb_client->format.depth = 24;
    kubevirt_client->rfb_client->format.bigEndian = 0;
    kubevirt_client->rfb_client->format.trueColour = 1;
    kubevirt_client->rfb_client->format.redMax = 255;
    kubevirt_client->rfb_client->format.redShift = 16;
    kubevirt_client->rfb_client->format.greenMax = 255;
    kubevirt_client->rfb_client->format.greenShift = 8;
    kubevirt_client->rfb_client->format.blueMax = 255;
    kubevirt_client->rfb_client->format.blueShift = 0;

    guac_client_log(client, GUAC_LOG_INFO,
            "Set pixel format to BGRA: 32 bpp, depth 24, "
            "red max 255 shift 16, green max 255 shift 8, blue max 255 shift 0");

    /* Use lossless compression only if requested (otherwise, use default heuristics) */
    guac_display_layer_set_lossless(guac_display_default_layer(kubevirt_client->display),
            settings->lossless);

    /* Configure VNC compression and quality from settings */
    if (settings->compress_level >= 0 && settings->compress_level <= 9)
        kubevirt_client->rfb_client->appData.compressLevel = settings->compress_level;

    if (settings->quality_level >= 0 && settings->quality_level <= 9)
        kubevirt_client->rfb_client->appData.qualityLevel = settings->quality_level;

    /* Configure remote cursor based on settings */
    kubevirt_client->rfb_client->appData.useRemoteCursor = settings->remote_cursor;

    /* Set appropriate cursor display based on read-only mode and remote cursor */
    if (!settings->read_only) {
        if (settings->remote_cursor)
            guac_display_set_cursor(kubevirt_client->display, GUAC_DISPLAY_CURSOR_DOT);
        else
            guac_display_set_cursor(kubevirt_client->display, GUAC_DISPLAY_CURSOR_POINTER);
    }

    guac_client_log(client, GUAC_LOG_INFO,
            "Configured VNC: compress=%d, quality=%d, remote_cursor=%s",
            kubevirt_client->rfb_client->appData.compressLevel,
            kubevirt_client->rfb_client->appData.qualityLevel,
            settings->remote_cursor ? "true" : "false");

    /* Send SetPixelFormat and SetEncodings messages */
    if (!SetFormatAndEncodings(kubevirt_client->rfb_client)) {
        guac_client_log(client, GUAC_LOG_ERROR,
                "Failed to set format and encodings");
        rfbClientCleanup(kubevirt_client->rfb_client);
        kubevirt_client->rfb_client = NULL;
        return -1;
    }

    guac_client_log(client, GUAC_LOG_INFO,
            "VNC client fully initialized. Screen: %dx%d",
            kubevirt_client->rfb_client->width,
            kubevirt_client->rfb_client->height);

    return 0;
}

/**
 * Thread function for libvncclient VNC processing.
 * This thread initializes the VNC client and processes VNC events.
 *
 * @param data
 *     The guac_client instance.
 *
 * @return
 *     Always NULL.
 */
static void* guac_kubevirt_vnc_client_thread(void* data) {

    guac_client* client = (guac_client*) data;
    guac_kubevirt_client* kubevirt_client =
        (guac_kubevirt_client*) client->data;

    guac_client_log(client, GUAC_LOG_DEBUG,
            "VNC client thread started");

    /* Initialize rfbClient and perform VNC handshake */
    if (guac_kubevirt_init_rfb_client(client) < 0) {
        guac_client_log(client, GUAC_LOG_ERROR,
                "Failed to initialize VNC client");
        guac_client_stop(client);
        return NULL;
    }

    /* Request initial framebuffer update (full, non-incremental) */
    if (!SendFramebufferUpdateRequest(kubevirt_client->rfb_client,
                0, 0,
                kubevirt_client->rfb_client->width,
                kubevirt_client->rfb_client->height,
                FALSE)) {
        guac_client_log(client, GUAC_LOG_ERROR,
                "Failed to send initial framebuffer update request");
        rfbClientCleanup(kubevirt_client->rfb_client);
        kubevirt_client->rfb_client = NULL;
        return NULL;
    }

    guac_client_log(client, GUAC_LOG_DEBUG,
            "Sent initial framebuffer update request (%dx%d)",
            kubevirt_client->rfb_client->width,
            kubevirt_client->rfb_client->height);

    /* Get the default layer for drawing operations */
    guac_display_layer* default_layer =
        guac_display_default_layer(kubevirt_client->display);

    /* Main VNC event loop */
    while (!kubevirt_client->stop_threads &&
           client->state == GUAC_CLIENT_RUNNING) {

        /* Wait for and handle VNC messages */
        int result = WaitForMessage(kubevirt_client->rfb_client, 500000); /* 500ms timeout */

        if (result < 0) {
            guac_client_log(client, GUAC_LOG_ERROR,
                    "Error waiting for VNC message");
            break;
        }

        if (result > 0) {
            /* Open raw context for drawing operations */
            guac_display_layer_raw_context* context =
                guac_display_layer_open_raw(default_layer);
            kubevirt_client->current_context = context;

            /* Process incoming VNC messages - callbacks will use current_context */
            if (!HandleRFBServerMessage(kubevirt_client->rfb_client)) {
                guac_client_log(client, GUAC_LOG_ERROR,
                        "Error handling VNC server message");
                guac_display_layer_close_raw(default_layer, context);
                kubevirt_client->current_context = NULL;
                break;
            }

            /* Close the raw context - commits changes for render thread */
            guac_display_layer_close_raw(default_layer, context);
            kubevirt_client->current_context = NULL;

            /* Request next framebuffer update (incremental) after processing messages */
            if (!SendIncrementalFramebufferUpdateRequest(kubevirt_client->rfb_client)) {
                guac_client_log(client, GUAC_LOG_DEBUG,
                        "Failed to send incremental framebuffer update request");
                /* Don't break - this might be a transient error */
            }
        }
    }

    guac_client_log(client, GUAC_LOG_DEBUG,
            "VNC client thread terminated");

    return NULL;
}

/**
 * Thread function that reads from socketpair and writes to WebSocket.
 * This bridges libvncclient's output to the WebSocket connection.
 *
 * @param data
 *     The guac_client instance.
 *
 * @return
 *     Always NULL.
 */
static void* guac_kubevirt_socket_to_ws_thread(void* data) {

    guac_client* client = (guac_client*) data;
    guac_kubevirt_client* kubevirt_client =
        (guac_kubevirt_client*) client->data;

    unsigned char buffer[LWS_PRE + 4096];

    guac_client_log(client, GUAC_LOG_DEBUG,
            "Socket to WebSocket forwarding thread started");

    while (!kubevirt_client->stop_threads && client->state == GUAC_CLIENT_RUNNING) {

        /* Read from socketpair (data from libvncclient) */
        ssize_t bytes_read = read(kubevirt_client->vnc_socket_pair[1],
                buffer + LWS_PRE, 4096);

        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            guac_client_log(client, GUAC_LOG_ERROR,
                    "Error reading from socketpair: %s", strerror(errno));
            break;
        }

        if (bytes_read == 0) {
            guac_client_log(client, GUAC_LOG_DEBUG,
                    "Socketpair closed by libvncclient");
            break;
        }

        guac_client_log(client, GUAC_LOG_DEBUG,
                "Read %zd bytes from socketpair, forwarding to WebSocket", bytes_read);

        /* Write to WebSocket */
        pthread_mutex_lock(&kubevirt_client->message_lock);
        if (kubevirt_client->wsi != NULL) {
            ssize_t written = lws_write(kubevirt_client->wsi,
                    buffer + LWS_PRE, bytes_read, LWS_WRITE_BINARY);
            if (written != bytes_read) {
                guac_client_log(client, GUAC_LOG_ERROR,
                        "Failed to write all data to WebSocket: %zd/%zd bytes",
                        written, bytes_read);
                pthread_mutex_unlock(&kubevirt_client->message_lock);
                break;
            }
            lws_callback_on_writable(kubevirt_client->wsi);
        }
        pthread_mutex_unlock(&kubevirt_client->message_lock);
    }

    guac_client_log(client, GUAC_LOG_DEBUG,
            "Socket to WebSocket forwarding thread terminated");

    return NULL;
}

void* guac_kubevirt_client_thread(void* data) {

    guac_client* client = (guac_client*) data;
    guac_kubevirt_client* kubevirt_client =
        (guac_kubevirt_client*) client->data;
    guac_kubevirt_settings* settings = kubevirt_client->settings;

    current_client = client;

    /* Initialize socketpair for bridging WebSocket to libvncclient
     * vnc_socket_pair[0] = libvncclient end
     * vnc_socket_pair[1] = WebSocket forwarding end */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, kubevirt_client->vnc_socket_pair) != 0) {
        guac_client_abort(client, GUAC_PROTOCOL_STATUS_SERVER_ERROR,
                "Failed to create socket pair for VNC bridge");
        return NULL;
    }

    kubevirt_client->stop_threads = 0;

    guac_client_log(client, GUAC_LOG_DEBUG,
            "Created socket pair: libvncclient fd=%d, ws fd=%d",
            kubevirt_client->vnc_socket_pair[0],
            kubevirt_client->vnc_socket_pair[1]);

    /* Build WebSocket URL path for KubeVirt VNC */
    char path[1024];
    snprintf(path, sizeof(path),
            "/apis/subresources.kubevirt.io/v1/namespaces/%s/"
            "virtualmachineinstances/%s/vnc",
            settings->namespace, settings->vm_name);

    /* Set up libwebsockets context */
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    /* Disable SSL verification if ignore_cert is set */
    if (settings->ignore_cert) {
        info.options |= LWS_SERVER_OPTION_PEER_CERT_NOT_REQUIRED;
        /* Suppress SSL error logging */
        lws_set_log_level(LLL_ERR & ~LLL_WARN, NULL);
    }

    kubevirt_client->context = lws_create_context(&info);
    if (!kubevirt_client->context) {
        guac_client_abort(client, GUAC_PROTOCOL_STATUS_UPSTREAM_ERROR,
                "Failed to create libwebsockets context");
        return NULL;
    }

    /* Set up WebSocket client connection */
    struct lws_client_connect_info ccinfo;
    memset(&ccinfo, 0, sizeof(ccinfo));
    ccinfo.context = kubevirt_client->context;
    ccinfo.address = settings->hostname;
    ccinfo.port = settings->port;
    ccinfo.path = path;
    ccinfo.host = settings->hostname;
    ccinfo.origin = settings->hostname;
    ccinfo.protocol = GUAC_KUBEVIRT_LWS_PROTOCOL;

    /* Use SSL if requested. Note: Certificate validation and hostname checks
     * are configured via the LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS
     * callback in guac_kubevirt_init_ssl() */
    if (settings->use_ssl) {
#ifdef HAVE_LCCSCF_USE_SSL
        ccinfo.ssl_connection = LCCSCF_USE_SSL | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
#else
        ccinfo.ssl_connection = 2; /* SSL + no hostname check (we do our own validation) */
#endif
    }

    /* Note: Header authentication is handled via libwebsockets context */
    ccinfo.alpn = "http/1.1";

    /* Connect */
    struct lws* wsi = lws_client_connect_via_info(&ccinfo);
    if (!wsi) {
        guac_client_abort(client, GUAC_PROTOCOL_STATUS_UPSTREAM_ERROR,
                "Failed to connect to KubeVirt API");
        lws_context_destroy(kubevirt_client->context);
        return NULL;
    }

    /* Service the WebSocket connection */
    while (client->state == GUAC_CLIENT_RUNNING) {
        lws_service(kubevirt_client->context, GUAC_KUBEVIRT_SERVICE_INTERVAL);
    }

    /* Signal threads to stop */
    kubevirt_client->stop_threads = 1;

    /* Wait for VNC client thread to finish */
    if (kubevirt_client->vnc_client_thread) {
        pthread_join(kubevirt_client->vnc_client_thread, NULL);
        guac_client_log(client, GUAC_LOG_DEBUG,
                "VNC client thread joined");
    }

    /* Wait for socket → WebSocket forwarding thread to finish */
    if (kubevirt_client->socket_to_ws_thread) {
        pthread_join(kubevirt_client->socket_to_ws_thread, NULL);
        guac_client_log(client, GUAC_LOG_DEBUG,
                "Socket to WebSocket forwarding thread joined");
    }

    /* Clean up rfbClient */
    if (kubevirt_client->rfb_client != NULL) {
        rfbClientCleanup(kubevirt_client->rfb_client);
        kubevirt_client->rfb_client = NULL;
    }

    /* Clean up */
    lws_context_destroy(kubevirt_client->context);

    /* Close socketpair */
    if (kubevirt_client->vnc_socket_pair[0] >= 0) {
        close(kubevirt_client->vnc_socket_pair[0]);
        kubevirt_client->vnc_socket_pair[0] = -1;
    }
    if (kubevirt_client->vnc_socket_pair[1] >= 0) {
        close(kubevirt_client->vnc_socket_pair[1]);
        kubevirt_client->vnc_socket_pair[1] = -1;
    }

    return NULL;
}
