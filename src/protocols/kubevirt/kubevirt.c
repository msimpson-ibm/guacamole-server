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
 * Processes incoming VNC protocol data.
 * DISABLED: Now using libvncclient for VNC protocol handling.
 */
#if 0
static int guac_kubevirt_process_vnc_data(guac_client* client,
        unsigned char* data, size_t length) {

    guac_kubevirt_client* kubevirt_client =
        (guac_kubevirt_client*) client->data;

    /* Special handling for rectangle pixel data in CONNECTED state
     * This must happen BEFORE buffering to avoid corrupting the protocol stream */
    if (kubevirt_client->vnc_state == GUAC_KUBEVIRT_STATE_CONNECTED &&
        kubevirt_client->current_rect.bytes_expected > 0 &&
        kubevirt_client->current_rect.bytes_received < kubevirt_client->current_rect.bytes_expected) {

        /* Log start of pixel accumulation */
        if (kubevirt_client->current_rect.bytes_received == 0) {
            guac_client_log(client, GUAC_LOG_DEBUG,
                    "Starting pixel data accumulation for %dx%d rectangle (%d bytes)",
                    kubevirt_client->current_rect.width,
                    kubevirt_client->current_rect.height,
                    kubevirt_client->current_rect.bytes_expected);
        }

        /* Process rectangle pixel data directly without buffering */
        int bytes_needed = kubevirt_client->current_rect.bytes_expected -
                          kubevirt_client->current_rect.bytes_received;
        int bytes_to_copy = (bytes_needed < length) ? bytes_needed : length;

        memcpy(kubevirt_client->rect_buffer + kubevirt_client->current_rect.bytes_received,
               data, bytes_to_copy);

        kubevirt_client->current_rect.bytes_received += bytes_to_copy;

        /* Log progress for large rectangles */
        if (kubevirt_client->current_rect.bytes_received % 1000000 == 0 ||
            kubevirt_client->current_rect.bytes_received == kubevirt_client->current_rect.bytes_expected) {
            guac_client_log(client, GUAC_LOG_DEBUG,
                    "Pixel data: %d/%d bytes (%.1f%%)",
                    kubevirt_client->current_rect.bytes_received,
                    kubevirt_client->current_rect.bytes_expected,
                    (100.0 * kubevirt_client->current_rect.bytes_received) / kubevirt_client->current_rect.bytes_expected);
        }

        /* If rectangle complete, render it */
        if (kubevirt_client->current_rect.bytes_received >=
            kubevirt_client->current_rect.bytes_expected) {

            /* Render the rectangle using guac_display */
            guac_display_layer* default_layer = guac_display_default_layer(kubevirt_client->display);

            if (kubevirt_client->current_rect.encoding == rfbEncodingRaw) {
                /* Raw encoding - copy pixel data */
                guac_display_layer_raw_context* context = guac_display_layer_open_raw(default_layer);

                /* Copy pixel data in BGRA format */
                for (int y = 0; y < kubevirt_client->current_rect.height; y++) {
                    memcpy(context->buffer +
                           ((kubevirt_client->current_rect.y + y) * context->stride) +
                           (kubevirt_client->current_rect.x * 4),
                           kubevirt_client->rect_buffer + (y * kubevirt_client->current_rect.width * 4),
                           kubevirt_client->current_rect.width * 4);
                }

                /* Mark region as modified */
                guac_rect dirty_region;
                guac_rect_init(&dirty_region,
                        kubevirt_client->current_rect.x,
                        kubevirt_client->current_rect.y,
                        kubevirt_client->current_rect.width,
                        kubevirt_client->current_rect.height);
                guac_rect_extend(&context->dirty, &dirty_region);

                guac_display_layer_close_raw(default_layer, context);
            } else if (kubevirt_client->current_rect.encoding == rfbEncodingCopyRect) {
                /* CopyRect encoding - copy from another screen region */
                uint16_t src_x = (kubevirt_client->rect_buffer[0] << 8) | kubevirt_client->rect_buffer[1];
                uint16_t src_y = (kubevirt_client->rect_buffer[2] << 8) | kubevirt_client->rect_buffer[3];

                guac_display_layer_raw_context* context = guac_display_layer_open_raw(default_layer);

                /* Copy rectangle from source to destination */
                for (int y = 0; y < kubevirt_client->current_rect.height; y++) {
                    memcpy(context->buffer +
                           ((kubevirt_client->current_rect.y + y) * context->stride) +
                           (kubevirt_client->current_rect.x * 4),
                           context->buffer +
                           ((src_y + y) * context->stride) +
                           (src_x * 4),
                           kubevirt_client->current_rect.width * 4);
                }

                /* Mark region as modified */
                guac_rect dirty_region;
                guac_rect_init(&dirty_region,
                        kubevirt_client->current_rect.x,
                        kubevirt_client->current_rect.y,
                        kubevirt_client->current_rect.width,
                        kubevirt_client->current_rect.height);
                guac_rect_extend(&context->dirty, &dirty_region);

                guac_display_layer_close_raw(default_layer, context);
            }

            guac_client_log(client, GUAC_LOG_DEBUG,
                    "Rendered rectangle at (%d,%d) %dx%d",
                    kubevirt_client->current_rect.x,
                    kubevirt_client->current_rect.y,
                    kubevirt_client->current_rect.width,
                    kubevirt_client->current_rect.height);

            /* Reset for next rectangle */
            kubevirt_client->current_rect.bytes_expected = 0;
            kubevirt_client->current_rect.bytes_received = 0;
            kubevirt_client->rectangles_remaining--;

            /* If all rectangles processed, send frame and request next update */
            if (kubevirt_client->rectangles_remaining == 0) {
                /* Flush the display updates */
                guac_display_end_frame(kubevirt_client->display);

                /* Request incremental update */
                unsigned char framebuffer_update_request[LWS_PRE + 10];
                framebuffer_update_request[LWS_PRE + 0] = rfbFramebufferUpdateRequest;
                framebuffer_update_request[LWS_PRE + 1] = 1;  /* incremental */
                framebuffer_update_request[LWS_PRE + 2] = 0;
                framebuffer_update_request[LWS_PRE + 3] = 0;
                framebuffer_update_request[LWS_PRE + 4] = 0;
                framebuffer_update_request[LWS_PRE + 5] = 0;
                framebuffer_update_request[LWS_PRE + 6] = (kubevirt_client->width >> 8) & 0xFF;
                framebuffer_update_request[LWS_PRE + 7] = kubevirt_client->width & 0xFF;
                framebuffer_update_request[LWS_PRE + 8] = (kubevirt_client->height >> 8) & 0xFF;
                framebuffer_update_request[LWS_PRE + 9] = kubevirt_client->height & 0xFF;

                pthread_mutex_lock(&kubevirt_client->message_lock);
                lws_write(kubevirt_client->wsi, framebuffer_update_request + LWS_PRE,
                        10, LWS_WRITE_BINARY);
                pthread_mutex_unlock(&kubevirt_client->message_lock);

                /* Buffer any remaining data for next callback */
                if (bytes_to_copy < length) {
                    int remaining = length - bytes_to_copy;
                    if (remaining <= GUAC_KUBEVIRT_VNC_BUFFER_SIZE) {
                        memcpy(kubevirt_client->vnc_buffer, data + bytes_to_copy, remaining);
                        kubevirt_client->vnc_buffer_length = remaining;
                    }
                }
                return 0;  /* Frame complete, wait for next update */
            }
        }

        /* If there's remaining data after the pixel data, buffer it and process it */
        if (bytes_to_copy < length) {
            int remaining = length - bytes_to_copy;
            if (remaining <= GUAC_KUBEVIRT_VNC_BUFFER_SIZE) {
                memcpy(kubevirt_client->vnc_buffer, data + bytes_to_copy, remaining);
                kubevirt_client->vnc_buffer_length = remaining;
                /* Don't return - fall through to process the buffered data */
            } else {
                return 0;  /* Data too large, discard */
            }
        } else {
            return 0;  /* All data consumed, nothing more to process */
        }
    }

    /* Append data to buffer for protocol messages */
    if (kubevirt_client->vnc_buffer_length + length > GUAC_KUBEVIRT_VNC_BUFFER_SIZE) {
        guac_client_log(client, GUAC_LOG_WARNING,
                "VNC buffer overflow, resetting buffer");
        kubevirt_client->vnc_buffer_length = 0;
    }

    memcpy(kubevirt_client->vnc_buffer + kubevirt_client->vnc_buffer_length,
            data, length);
    kubevirt_client->vnc_buffer_length += length;

    /* Process all available data in a loop */
    while (kubevirt_client->vnc_buffer_length > 0) {
        unsigned char* buffer = kubevirt_client->vnc_buffer;
        int buffer_length = kubevirt_client->vnc_buffer_length;
        int bytes_consumed = 0;

        /* Process based on VNC state */
        switch (kubevirt_client->vnc_state) {

        case GUAC_KUBEVIRT_STATE_WAITING_FOR_VERSION:
            /* Need 12 bytes for ProtocolVersion */
            if (buffer_length >= 12) {
                guac_client_log(client, GUAC_LOG_DEBUG,
                        "Received VNC version: %.12s", buffer);

                /* Send our version (RFB 003.008) - requires LWS_PRE padding */
                unsigned char ws_buffer[LWS_PRE + 12];
                memcpy(ws_buffer + LWS_PRE, "RFB 003.008\n", 12);

                guac_client_log(client, GUAC_LOG_DEBUG,
                        "Preparing to send VNC version response (wsi=%p, client_state=%d)",
                        kubevirt_client->wsi, client->state);

                if (kubevirt_client->wsi == NULL) {
                    guac_client_log(client, GUAC_LOG_ERROR,
                            "Cannot send VNC version: WebSocket is NULL");
                    return -1;
                }

                pthread_mutex_lock(&kubevirt_client->message_lock);
                guac_client_log(client, GUAC_LOG_DEBUG, "About to call lws_write()");
                int wrote = lws_write(kubevirt_client->wsi, ws_buffer + LWS_PRE,
                        12, LWS_WRITE_BINARY);
                guac_client_log(client, GUAC_LOG_DEBUG, "lws_write() returned %d", wrote);
                pthread_mutex_unlock(&kubevirt_client->message_lock);

                if (wrote < 0) {
                    guac_client_log(client, GUAC_LOG_ERROR,
                            "Failed to send VNC version response: %d", wrote);
                    return -1;
                }

                guac_client_log(client, GUAC_LOG_INFO,
                        "Sent VNC version response (%d bytes)", wrote);

                kubevirt_client->vnc_state = GUAC_KUBEVIRT_STATE_WAITING_FOR_SECURITY;
                bytes_consumed = 12;
            }
            break;

        case GUAC_KUBEVIRT_STATE_WAITING_FOR_SECURITY:
            /* Need at least 1 byte for security types count */
            if (buffer_length >= 1) {
                int num_types = buffer[0];

                if (num_types == 0) {
                    /* Security handshake failed */
                    guac_client_abort(client, GUAC_PROTOCOL_STATUS_UPSTREAM_ERROR,
                            "VNC security handshake failed");
                    return -1;
                }

                /* Need 1 + num_types bytes total */
                if (buffer_length >= 1 + num_types) {
                    guac_client_log(client, GUAC_LOG_DEBUG,
                            "Received %d security types", num_types);

                    /* Select security type 1 (None) if available */
                    int selected_type = 1;
                    int found = 0;

                    for (int i = 0; i < num_types; i++) {
                        if (buffer[1 + i] == 1) {
                            found = 1;
                            break;
                        }
                    }

                    if (!found) {
                        guac_client_abort(client, GUAC_PROTOCOL_STATUS_UPSTREAM_ERROR,
                                "VNC server does not support security type None");
                        return -1;
                    }

                    /* Send selected security type - requires LWS_PRE padding */
                    unsigned char ws_buffer[LWS_PRE + 1];
                    ws_buffer[LWS_PRE] = selected_type;
                    pthread_mutex_lock(&kubevirt_client->message_lock);
                    lws_write(kubevirt_client->wsi, ws_buffer + LWS_PRE,
                            1, LWS_WRITE_BINARY);
                    pthread_mutex_unlock(&kubevirt_client->message_lock);

                    guac_client_log(client, GUAC_LOG_DEBUG,
                            "Sent security type: None (1)");

                    kubevirt_client->vnc_state = GUAC_KUBEVIRT_STATE_WAITING_FOR_SECURITY_RESULT;
                    bytes_consumed = 1 + num_types;
                }
            }
            break;

        case GUAC_KUBEVIRT_STATE_WAITING_FOR_SECURITY_RESULT:
            /* Need 4 bytes for SecurityResult */
            if (buffer_length >= 4) {
                uint32_t result = (buffer[0] << 24) | (buffer[1] << 16) |
                                  (buffer[2] << 8) | buffer[3];

                guac_client_log(client, GUAC_LOG_DEBUG,
                        "Received security result: %u", result);

                if (result != 0) {
                    guac_client_abort(client, GUAC_PROTOCOL_STATUS_UPSTREAM_ERROR,
                            "VNC authentication failed with result: %u", result);
                    return -1;
                }

                /* Send ClientInit (shared flag = 1) - requires LWS_PRE padding */
                unsigned char ws_buffer[LWS_PRE + 1];
                ws_buffer[LWS_PRE] = 1;
                pthread_mutex_lock(&kubevirt_client->message_lock);
                lws_write(kubevirt_client->wsi, ws_buffer + LWS_PRE,
                        1, LWS_WRITE_BINARY);
                pthread_mutex_unlock(&kubevirt_client->message_lock);

                guac_client_log(client, GUAC_LOG_DEBUG,
                        "Sent ClientInit (shared=1)");

                kubevirt_client->vnc_state = GUAC_KUBEVIRT_STATE_WAITING_FOR_SERVER_INIT;
                bytes_consumed = 4;
            }
            break;

        case GUAC_KUBEVIRT_STATE_WAITING_FOR_SERVER_INIT:
            /* Need at least 24 bytes for ServerInit header */
            if (buffer_length >= 24) {
                uint16_t width = (buffer[0] << 8) | buffer[1];
                uint16_t height = (buffer[2] << 8) | buffer[3];
                uint32_t name_length = (buffer[20] << 24) | (buffer[21] << 16) |
                                       (buffer[22] << 8) | buffer[23];

                /* Need complete ServerInit message */
                if (buffer_length >= 24 + name_length) {
                    kubevirt_client->width = width;
                    kubevirt_client->height = height;

                    /* Parse pixel format */
                    memcpy(&kubevirt_client->pixel_format, buffer + 4,
                            sizeof(rfbPixelFormat));

                    /* Store server name */
                    kubevirt_client->server_name = malloc(name_length + 1);
                    memcpy(kubevirt_client->server_name, buffer + 24, name_length);
                    kubevirt_client->server_name[name_length] = '\0';

                    guac_client_log(client, GUAC_LOG_INFO,
                            "Connected to VNC server: %s (%dx%d)",
                            kubevirt_client->server_name, width, height);

                    /* Initialize display */
                    kubevirt_client->display = guac_display_alloc(client);
                    guac_display_layer_resize(guac_display_default_layer(kubevirt_client->display), width, height);

                    /* Start render thread to send display updates to client */
                    kubevirt_client->render_thread = guac_display_render_thread_create(kubevirt_client->display);

                    guac_client_log(client, GUAC_LOG_INFO,
                            "Display initialized and render thread started");

                    /* Set pixel format (request 24-bit color) - requires LWS_PRE padding */
                    unsigned char set_pixel_format[LWS_PRE + 20];
                    set_pixel_format[LWS_PRE + 0] = rfbSetPixelFormat;
                    set_pixel_format[LWS_PRE + 1] = 0;
                    set_pixel_format[LWS_PRE + 2] = 0;
                    set_pixel_format[LWS_PRE + 3] = 0;
                    set_pixel_format[LWS_PRE + 4] = 32;  /* bits per pixel */
                    set_pixel_format[LWS_PRE + 5] = 24;  /* depth */
                    set_pixel_format[LWS_PRE + 6] = 0;   /* big endian */
                    set_pixel_format[LWS_PRE + 7] = 1;   /* true color */
                    set_pixel_format[LWS_PRE + 8] = 0;   /* red max high */
                    set_pixel_format[LWS_PRE + 9] = 255; /* red max low */
                    set_pixel_format[LWS_PRE + 10] = 0;  /* green max high */
                    set_pixel_format[LWS_PRE + 11] = 255;/* green max low */
                    set_pixel_format[LWS_PRE + 12] = 0;  /* blue max high */
                    set_pixel_format[LWS_PRE + 13] = 255;/* blue max low */
                    set_pixel_format[LWS_PRE + 14] = 16; /* red shift */
                    set_pixel_format[LWS_PRE + 15] = 8;  /* green shift */
                    set_pixel_format[LWS_PRE + 16] = 0;  /* blue shift */
                    set_pixel_format[LWS_PRE + 17] = 0;  /* padding */
                    set_pixel_format[LWS_PRE + 18] = 0;
                    set_pixel_format[LWS_PRE + 19] = 0;

                    /* Set encodings (Raw and CopyRect) - requires LWS_PRE padding */
                    unsigned char set_encodings[LWS_PRE + 12];
                    set_encodings[LWS_PRE + 0] = rfbSetEncodings;
                    set_encodings[LWS_PRE + 1] = 0;
                    set_encodings[LWS_PRE + 2] = 0;  /* number of encodings high */
                    set_encodings[LWS_PRE + 3] = 2;  /* number of encodings low (Raw + CopyRect) */
                    /* Encoding 1: CopyRect (most efficient, try first) */
                    set_encodings[LWS_PRE + 4] = 0;
                    set_encodings[LWS_PRE + 5] = 0;
                    set_encodings[LWS_PRE + 6] = 0;
                    set_encodings[LWS_PRE + 7] = 1;  /* CopyRect */
                    /* Encoding 2: Raw (fallback) */
                    set_encodings[LWS_PRE + 8] = 0;
                    set_encodings[LWS_PRE + 9] = 0;
                    set_encodings[LWS_PRE + 10] = 0;
                    set_encodings[LWS_PRE + 11] = 0;  /* Raw */

                    /* Request first frame update - requires LWS_PRE padding */
                    unsigned char framebuffer_update_request[LWS_PRE + 10];
                    framebuffer_update_request[LWS_PRE + 0] = rfbFramebufferUpdateRequest;
                    framebuffer_update_request[LWS_PRE + 1] = 0;  /* incremental */
                    framebuffer_update_request[LWS_PRE + 2] = 0;  /* x high */
                    framebuffer_update_request[LWS_PRE + 3] = 0;  /* x low */
                    framebuffer_update_request[LWS_PRE + 4] = 0;  /* y high */
                    framebuffer_update_request[LWS_PRE + 5] = 0;  /* y low */
                    framebuffer_update_request[LWS_PRE + 6] = (width >> 8) & 0xFF;
                    framebuffer_update_request[LWS_PRE + 7] = width & 0xFF;
                    framebuffer_update_request[LWS_PRE + 8] = (height >> 8) & 0xFF;
                    framebuffer_update_request[LWS_PRE + 9] = height & 0xFF;

                    pthread_mutex_lock(&kubevirt_client->message_lock);
                    lws_write(kubevirt_client->wsi, set_pixel_format + LWS_PRE,
                            20, LWS_WRITE_BINARY);
                    lws_write(kubevirt_client->wsi, set_encodings + LWS_PRE,
                            12, LWS_WRITE_BINARY);
                    lws_write(kubevirt_client->wsi, framebuffer_update_request + LWS_PRE,
                            10, LWS_WRITE_BINARY);
                    pthread_mutex_unlock(&kubevirt_client->message_lock);

                    kubevirt_client->vnc_state = GUAC_KUBEVIRT_STATE_CONNECTED;
                    bytes_consumed = 24 + name_length;
                }
            }
            break;

        case GUAC_KUBEVIRT_STATE_CONNECTED:
            /* Handle server messages */
            if (buffer_length >= 1) {
                /* If we're expecting rectangles, parse them directly */
                if (kubevirt_client->rectangles_remaining > 0) {
                    /* Need rectangle header (12 bytes: x, y, w, h, encoding) */
                    if (buffer_length < 12)
                        break;

                    /* Parse rectangle header if not already parsing one */
                    if (kubevirt_client->current_rect.bytes_expected == 0) {
                        kubevirt_client->current_rect.x = (buffer[0] << 8) | buffer[1];
                        kubevirt_client->current_rect.y = (buffer[2] << 8) | buffer[3];
                        kubevirt_client->current_rect.width = (buffer[4] << 8) | buffer[5];
                        kubevirt_client->current_rect.height = (buffer[6] << 8) | buffer[7];
                        kubevirt_client->current_rect.encoding =
                            (buffer[8] << 24) | (buffer[9] << 16) |
                            (buffer[10] << 8) | buffer[11];

                        /* Handle different encoding types */
                        if (kubevirt_client->current_rect.encoding == rfbEncodingRaw) {
                            /* Raw encoding: width * height * 4 bytes (BGRA) */
                            kubevirt_client->current_rect.bytes_expected =
                                kubevirt_client->current_rect.width *
                                kubevirt_client->current_rect.height * 4;
                            kubevirt_client->current_rect.bytes_received = 0;

                            /* Allocate buffer if needed */
                            if (kubevirt_client->rect_buffer_size < kubevirt_client->current_rect.bytes_expected) {
                                kubevirt_client->rect_buffer = realloc(kubevirt_client->rect_buffer,
                                        kubevirt_client->current_rect.bytes_expected);
                                kubevirt_client->rect_buffer_size = kubevirt_client->current_rect.bytes_expected;
                            }

                            guac_client_log(client, GUAC_LOG_DEBUG,
                                    "Rectangle: x=%d y=%d w=%d h=%d encoding=Raw bytes=%d",
                                    kubevirt_client->current_rect.x,
                                    kubevirt_client->current_rect.y,
                                    kubevirt_client->current_rect.width,
                                    kubevirt_client->current_rect.height,
                                    kubevirt_client->current_rect.bytes_expected);

                            /* Copy any pixel data that's already in the buffer */
                            bytes_consumed = 12;
                            if (buffer_length > 12) {
                                int pixel_bytes_available = buffer_length - 12;
                                int pixel_bytes_to_copy = (pixel_bytes_available < kubevirt_client->current_rect.bytes_expected) ?
                                    pixel_bytes_available : kubevirt_client->current_rect.bytes_expected;

                                memcpy(kubevirt_client->rect_buffer,
                                       buffer + 12,
                                       pixel_bytes_to_copy);

                                kubevirt_client->current_rect.bytes_received = pixel_bytes_to_copy;
                                bytes_consumed = 12 + pixel_bytes_to_copy;

                                guac_client_log(client, GUAC_LOG_DEBUG,
                                        "Copied %d bytes of pixel data from buffer, total: %d/%d",
                                        pixel_bytes_to_copy,
                                        kubevirt_client->current_rect.bytes_received,
                                        kubevirt_client->current_rect.bytes_expected);

                                /* If rectangle is complete, render it */
                                if (kubevirt_client->current_rect.bytes_received >= kubevirt_client->current_rect.bytes_expected) {
                                    guac_display_layer* default_layer = guac_display_default_layer(kubevirt_client->display);
                                    guac_display_layer_raw_context* context = guac_display_layer_open_raw(default_layer);

                                    /* Copy pixel data in BGRA format */
                                    for (int y = 0; y < kubevirt_client->current_rect.height; y++) {
                                        memcpy(context->buffer +
                                               ((kubevirt_client->current_rect.y + y) * context->stride) +
                                               (kubevirt_client->current_rect.x * 4),
                                               kubevirt_client->rect_buffer + (y * kubevirt_client->current_rect.width * 4),
                                               kubevirt_client->current_rect.width * 4);
                                    }

                                    /* Mark region as modified */
                                    guac_rect dirty_region;
                                    guac_rect_init(&dirty_region,
                                            kubevirt_client->current_rect.x,
                                            kubevirt_client->current_rect.y,
                                            kubevirt_client->current_rect.width,
                                            kubevirt_client->current_rect.height);
                                    guac_rect_extend(&context->dirty, &dirty_region);

                                    guac_display_layer_close_raw(default_layer, context);

                                    guac_client_log(client, GUAC_LOG_DEBUG,
                                            "Rendered Raw rectangle at (%d,%d) %dx%d",
                                            kubevirt_client->current_rect.x,
                                            kubevirt_client->current_rect.y,
                                            kubevirt_client->current_rect.width,
                                            kubevirt_client->current_rect.height);

                                    /* Reset for next rectangle */
                                    kubevirt_client->current_rect.bytes_expected = 0;
                                    kubevirt_client->current_rect.bytes_received = 0;
                                    kubevirt_client->rectangles_remaining--;

                                    /* If all rectangles processed, send frame and request next update */
                                    if (kubevirt_client->rectangles_remaining == 0) {
                                        guac_display_end_frame(kubevirt_client->display);

                                        /* Request incremental update */
                                        unsigned char framebuffer_update_request[LWS_PRE + 10];
                                        framebuffer_update_request[LWS_PRE + 0] = rfbFramebufferUpdateRequest;
                                        framebuffer_update_request[LWS_PRE + 1] = 1;  /* incremental */
                                        framebuffer_update_request[LWS_PRE + 2] = 0;
                                        framebuffer_update_request[LWS_PRE + 3] = 0;
                                        framebuffer_update_request[LWS_PRE + 4] = 0;
                                        framebuffer_update_request[LWS_PRE + 5] = 0;
                                        framebuffer_update_request[LWS_PRE + 6] = (kubevirt_client->width >> 8) & 0xFF;
                                        framebuffer_update_request[LWS_PRE + 7] = kubevirt_client->width & 0xFF;
                                        framebuffer_update_request[LWS_PRE + 8] = (kubevirt_client->height >> 8) & 0xFF;
                                        framebuffer_update_request[LWS_PRE + 9] = kubevirt_client->height & 0xFF;

                                        pthread_mutex_lock(&kubevirt_client->message_lock);
                                        lws_write(kubevirt_client->wsi, framebuffer_update_request + LWS_PRE,
                                                10, LWS_WRITE_BINARY);
                                        pthread_mutex_unlock(&kubevirt_client->message_lock);

                                        /* Frame complete, exit loop to wait for next update */
                                        break;
                                    }
                                }
                            }
                        } else if (kubevirt_client->current_rect.encoding == rfbEncodingCopyRect) {
                            /* CopyRect encoding: 4 bytes (src_x, src_y) */
                            kubevirt_client->current_rect.bytes_expected = 4;
                            kubevirt_client->current_rect.bytes_received = 0;

                            /* Allocate buffer if needed */
                            if (kubevirt_client->rect_buffer_size < 4) {
                                kubevirt_client->rect_buffer = realloc(kubevirt_client->rect_buffer, 4);
                                kubevirt_client->rect_buffer_size = 4;
                            }

                            guac_client_log(client, GUAC_LOG_DEBUG,
                                    "Rectangle: x=%d y=%d w=%d h=%d encoding=CopyRect bytes=%d",
                                    kubevirt_client->current_rect.x,
                                    kubevirt_client->current_rect.y,
                                    kubevirt_client->current_rect.width,
                                    kubevirt_client->current_rect.height,
                                    kubevirt_client->current_rect.bytes_expected);

                            /* Copy any data that's already in the buffer */
                            bytes_consumed = 12;
                            if (buffer_length > 12) {
                                int data_bytes_available = buffer_length - 12;
                                int data_bytes_to_copy = (data_bytes_available < 4) ? data_bytes_available : 4;

                                memcpy(kubevirt_client->rect_buffer, buffer + 12, data_bytes_to_copy);
                                kubevirt_client->current_rect.bytes_received = data_bytes_to_copy;
                                bytes_consumed = 12 + data_bytes_to_copy;

                                /* If complete, render it */
                                if (kubevirt_client->current_rect.bytes_received >= 4) {
                                    uint16_t src_x = (kubevirt_client->rect_buffer[0] << 8) | kubevirt_client->rect_buffer[1];
                                    uint16_t src_y = (kubevirt_client->rect_buffer[2] << 8) | kubevirt_client->rect_buffer[3];

                                    guac_display_layer* default_layer = guac_display_default_layer(kubevirt_client->display);
                                    guac_display_layer_raw_context* context = guac_display_layer_open_raw(default_layer);

                                    /* Copy rectangle from source to destination */
                                    for (int y = 0; y < kubevirt_client->current_rect.height; y++) {
                                        memcpy(context->buffer +
                                               ((kubevirt_client->current_rect.y + y) * context->stride) +
                                               (kubevirt_client->current_rect.x * 4),
                                               context->buffer +
                                               ((src_y + y) * context->stride) +
                                               (src_x * 4),
                                               kubevirt_client->current_rect.width * 4);
                                    }

                                    /* Mark region as modified */
                                    guac_rect dirty_region;
                                    guac_rect_init(&dirty_region,
                                            kubevirt_client->current_rect.x,
                                            kubevirt_client->current_rect.y,
                                            kubevirt_client->current_rect.width,
                                            kubevirt_client->current_rect.height);
                                    guac_rect_extend(&context->dirty, &dirty_region);

                                    guac_display_layer_close_raw(default_layer, context);

                                    guac_client_log(client, GUAC_LOG_DEBUG,
                                            "Rendered CopyRect rectangle from (%d,%d) to (%d,%d) %dx%d",
                                            src_x, src_y,
                                            kubevirt_client->current_rect.x,
                                            kubevirt_client->current_rect.y,
                                            kubevirt_client->current_rect.width,
                                            kubevirt_client->current_rect.height);

                                    /* Reset for next rectangle */
                                    kubevirt_client->current_rect.bytes_expected = 0;
                                    kubevirt_client->current_rect.bytes_received = 0;
                                    kubevirt_client->rectangles_remaining--;

                                    /* If all rectangles processed, send frame and request next update */
                                    if (kubevirt_client->rectangles_remaining == 0) {
                                        guac_display_end_frame(kubevirt_client->display);

                                        /* Request incremental update */
                                        unsigned char framebuffer_update_request[LWS_PRE + 10];
                                        framebuffer_update_request[LWS_PRE + 0] = rfbFramebufferUpdateRequest;
                                        framebuffer_update_request[LWS_PRE + 1] = 1;  /* incremental */
                                        framebuffer_update_request[LWS_PRE + 2] = 0;
                                        framebuffer_update_request[LWS_PRE + 3] = 0;
                                        framebuffer_update_request[LWS_PRE + 4] = 0;
                                        framebuffer_update_request[LWS_PRE + 5] = 0;
                                        framebuffer_update_request[LWS_PRE + 6] = (kubevirt_client->width >> 8) & 0xFF;
                                        framebuffer_update_request[LWS_PRE + 7] = kubevirt_client->width & 0xFF;
                                        framebuffer_update_request[LWS_PRE + 8] = (kubevirt_client->height >> 8) & 0xFF;
                                        framebuffer_update_request[LWS_PRE + 9] = kubevirt_client->height & 0xFF;

                                        pthread_mutex_lock(&kubevirt_client->message_lock);
                                        lws_write(kubevirt_client->wsi, framebuffer_update_request + LWS_PRE,
                                                10, LWS_WRITE_BINARY);
                                        pthread_mutex_unlock(&kubevirt_client->message_lock);

                                        /* Frame complete, exit loop to wait for next update */
                                        break;
                                    }
                                }
                            }
                        } else if (kubevirt_client->current_rect.encoding < 0) {
                            /* Pseudo-encoding (negative values) - typically no pixel data */
                            guac_client_log(client, GUAC_LOG_DEBUG,
                                    "Skipping pseudo-encoding: 0x%08X", kubevirt_client->current_rect.encoding);

                            /* Skip rectangle header, no pixel data for pseudo-encodings */
                            kubevirt_client->rectangles_remaining--;
                            bytes_consumed = 12;

                            /* If all rectangles processed, send frame and request next update */
                            if (kubevirt_client->rectangles_remaining == 0) {
                                guac_display_end_frame(kubevirt_client->display);

                                /* Request incremental update */
                                unsigned char framebuffer_update_request[LWS_PRE + 10];
                                framebuffer_update_request[LWS_PRE + 0] = rfbFramebufferUpdateRequest;
                                framebuffer_update_request[LWS_PRE + 1] = 1;  /* incremental */
                                framebuffer_update_request[LWS_PRE + 2] = 0;
                                framebuffer_update_request[LWS_PRE + 3] = 0;
                                framebuffer_update_request[LWS_PRE + 4] = 0;
                                framebuffer_update_request[LWS_PRE + 5] = 0;
                                framebuffer_update_request[LWS_PRE + 6] = (kubevirt_client->width >> 8) & 0xFF;
                                framebuffer_update_request[LWS_PRE + 7] = kubevirt_client->width & 0xFF;
                                framebuffer_update_request[LWS_PRE + 8] = (kubevirt_client->height >> 8) & 0xFF;
                                framebuffer_update_request[LWS_PRE + 9] = kubevirt_client->height & 0xFF;

                                pthread_mutex_lock(&kubevirt_client->message_lock);
                                lws_write(kubevirt_client->wsi, framebuffer_update_request + LWS_PRE,
                                        10, LWS_WRITE_BINARY);
                                pthread_mutex_unlock(&kubevirt_client->message_lock);

                                /* Frame complete, exit loop to wait for next update */
                                break;
                            }
                        } else {
                            /* Unsupported encoding */
                            guac_client_log(client, GUAC_LOG_DEBUG,
                                    "Unsupported encoding: %d, requesting fresh frame",
                                    kubevirt_client->current_rect.encoding);

                            /* Can't safely skip unknown encoding data, request fresh frame */
                            kubevirt_client->rectangles_remaining = 0;
                            kubevirt_client->current_rect.bytes_expected = 0;
                            kubevirt_client->current_rect.bytes_received = 0;
                            kubevirt_client->vnc_buffer_length = 0;

                            /* Request full (non-incremental) update */
                            unsigned char framebuffer_update_request[LWS_PRE + 10];
                            framebuffer_update_request[LWS_PRE + 0] = rfbFramebufferUpdateRequest;
                            framebuffer_update_request[LWS_PRE + 1] = 0;  /* non-incremental */
                            framebuffer_update_request[LWS_PRE + 2] = 0;
                            framebuffer_update_request[LWS_PRE + 3] = 0;
                            framebuffer_update_request[LWS_PRE + 4] = 0;
                            framebuffer_update_request[LWS_PRE + 5] = 0;
                            framebuffer_update_request[LWS_PRE + 6] = (kubevirt_client->width >> 8) & 0xFF;
                            framebuffer_update_request[LWS_PRE + 7] = kubevirt_client->width & 0xFF;
                            framebuffer_update_request[LWS_PRE + 8] = (kubevirt_client->height >> 8) & 0xFF;
                            framebuffer_update_request[LWS_PRE + 9] = kubevirt_client->height & 0xFF;

                            pthread_mutex_lock(&kubevirt_client->message_lock);
                            lws_write(kubevirt_client->wsi, framebuffer_update_request + LWS_PRE,
                                    10, LWS_WRITE_BINARY);
                            pthread_mutex_unlock(&kubevirt_client->message_lock);

                            return 0;
                        }
                    }
                } else {
                    /* No rectangles pending, look for new message */
                    uint8_t msg_type = buffer[0];

                    if (msg_type == rfbFramebufferUpdate) {
                        /* Need complete FramebufferUpdate header (4 bytes) */
                        if (buffer_length < 4)
                            break;

                        uint16_t num_rects = (buffer[2] << 8) | buffer[3];
                        kubevirt_client->rectangles_remaining = num_rects;

                        guac_client_log(client, GUAC_LOG_DEBUG,
                                "FramebufferUpdate: %d rectangles", num_rects);

                        bytes_consumed = 4;
                    } else if (msg_type == rfbSetColourMapEntries) {
                        /* SetColourMapEntries - skip for now */
                        if (buffer_length < 6)
                            break;
                        uint16_t num_colors = (buffer[4] << 8) | buffer[5];
                        int msg_length = 6 + (num_colors * 6);
                        if (buffer_length < msg_length)
                            break;
                        bytes_consumed = msg_length;
                    } else if (msg_type == rfbBell) {
                        /* Bell - 1 byte message */
                        bytes_consumed = 1;
                    } else if (msg_type == rfbServerCutText) {
                        /* ServerCutText - variable length */
                        if (buffer_length < 8)
                            break;
                        uint32_t text_length = (buffer[4] << 24) | (buffer[5] << 16) |
                                              (buffer[6] << 8) | buffer[7];
                        int msg_length = 8 + text_length;
                        if (buffer_length < msg_length)
                            break;
                        bytes_consumed = msg_length;
                    } else {
                        /* Unknown message type - buffer is likely corrupted */
                        guac_client_log(client, GUAC_LOG_DEBUG,
                                "Unknown message type: %d, clearing buffer and requesting fresh frame",
                                msg_type);

                        /* Clear buffer and request full update to recover */
                        kubevirt_client->vnc_buffer_length = 0;

                        unsigned char framebuffer_update_request[LWS_PRE + 10];
                        framebuffer_update_request[LWS_PRE + 0] = rfbFramebufferUpdateRequest;
                        framebuffer_update_request[LWS_PRE + 1] = 0;  /* non-incremental */
                        framebuffer_update_request[LWS_PRE + 2] = 0;
                        framebuffer_update_request[LWS_PRE + 3] = 0;
                        framebuffer_update_request[LWS_PRE + 4] = 0;
                        framebuffer_update_request[LWS_PRE + 5] = 0;
                        framebuffer_update_request[LWS_PRE + 6] = (kubevirt_client->width >> 8) & 0xFF;
                        framebuffer_update_request[LWS_PRE + 7] = kubevirt_client->width & 0xFF;
                        framebuffer_update_request[LWS_PRE + 8] = (kubevirt_client->height >> 8) & 0xFF;
                        framebuffer_update_request[LWS_PRE + 9] = kubevirt_client->height & 0xFF;

                        pthread_mutex_lock(&kubevirt_client->message_lock);
                        lws_write(kubevirt_client->wsi, framebuffer_update_request + LWS_PRE,
                                10, LWS_WRITE_BINARY);
                        pthread_mutex_unlock(&kubevirt_client->message_lock);

                        return 0;
                    }
                }
            }
            break;
    }

        /* Remove consumed bytes from buffer */
        if (bytes_consumed > 0) {
            kubevirt_client->vnc_buffer_length -= bytes_consumed;
            if (kubevirt_client->vnc_buffer_length > 0) {
                memmove(buffer, buffer + bytes_consumed,
                        kubevirt_client->vnc_buffer_length);
            }
        } else {
            /* No bytes consumed, need more data - exit loop */
            break;
        }
    }

    return 0;
}
#endif /* Disabled manual VNC parsing code */

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
