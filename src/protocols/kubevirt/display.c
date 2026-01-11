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

#include "display.h"
#include "kubevirt.h"
#include "settings.h"

#include <guacamole/client.h>
#include <guacamole/display.h>
#include <guacamole/rect.h>
#include <rfb/rfbclient.h>

#include <stdlib.h>
#include <string.h>

rfbBool guac_kubevirt_resize_framebuffer(rfbClient* rfb_client) {

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

void guac_kubevirt_framebuffer_update(rfbClient* rfb_client,
        int x, int y, int w, int h) {

    guac_client* client = rfbClientGetClientData(rfb_client, NULL);
    guac_kubevirt_client* kubevirt_client =
        (guac_kubevirt_client*) client->data;
    guac_kubevirt_settings* settings = kubevirt_client->settings;

    /* Use the current context that was opened in the VNC client thread */
    guac_display_layer_raw_context* context = kubevirt_client->current_context;
    if (context == NULL) {
        guac_client_log(client, GUAC_LOG_WARNING,
                "Framebuffer update received but no context is open");
        return;
    }

    unsigned int vnc_bpp = rfb_client->format.bitsPerPixel / 8;
    size_t vnc_stride = vnc_bpp * rfb_client->width;
    size_t row_bytes = w * 4;

    /* If pixel format matches guac_display (32-bit BGRA/BGRX), use direct copy */
    if (vnc_bpp == 4 && !settings->swap_red_blue) {
        /* Optimized path: if strides match and region spans full width, use single memcpy */
        if (x == 0 && w == rfb_client->width && context->stride == (int)vnc_stride) {
            /* Single bulk copy for full-width updates */
            memcpy(
                context->buffer + y * context->stride,
                rfb_client->frameBuffer + y * vnc_stride,
                (size_t)h * row_bytes
            );
        }
        else {
            /* Row-by-row copy for partial updates */
            unsigned char* dst = context->buffer + y * context->stride + x * 4;
            const unsigned char* src = rfb_client->frameBuffer + y * vnc_stride + x * 4;
            for (int row = 0; row < h; row++) {
                memcpy(dst, src, row_bytes);
                dst += context->stride;
                src += vnc_stride;
            }
        }
    }
    /* Otherwise, convert pixel format row by row */
    else {
        const unsigned char* vnc_row = rfb_client->frameBuffer + y * vnc_stride + x * vnc_bpp;
        unsigned char* layer_row = context->buffer + y * context->stride + x * 4;

        for (int dy = 0; dy < h; dy++) {
            uint32_t* layer_pixel = (uint32_t*) layer_row;
            const unsigned char* vnc_pixel = vnc_row;

            for (int dx = 0; dx < w; dx++) {
                /* Read VNC pixel value */
                uint32_t v;
                switch (vnc_bpp) {
                    case 1:
                        v = *((uint8_t*) vnc_pixel);
                        break;
                    case 2:
                        v = *((uint16_t*) vnc_pixel);
                        break;
                    default:
                        v = *((uint32_t*) vnc_pixel);
                        break;
                }

                /* Extract RGB components */
                uint8_t red   = (v >> rfb_client->format.redShift)   * 0x100 / (rfb_client->format.redMax   + 1);
                uint8_t green = (v >> rfb_client->format.greenShift) * 0x100 / (rfb_client->format.greenMax + 1);
                uint8_t blue  = (v >> rfb_client->format.blueShift)  * 0x100 / (rfb_client->format.blueMax  + 1);

                /* Write BGRA pixel (swap red/blue if requested) */
                if (settings->swap_red_blue)
                    *(layer_pixel++) = 0xFF000000 | (blue << 16) | (green << 8) | red;
                else
                    *(layer_pixel++) = 0xFF000000 | (red << 16) | (green << 8) | blue;

                vnc_pixel += vnc_bpp;
            }

            vnc_row += vnc_stride;
            layer_row += context->stride;
        }
    }

    /* Mark the updated region as dirty */
    guac_rect update_rect;
    guac_rect_init(&update_rect, x, y, w, h);
    guac_rect_extend(&context->dirty, &update_rect);

    /* Notify the render thread that the display has been modified */
    guac_display_render_thread_notify_modified(kubevirt_client->render_thread);
}

void guac_kubevirt_set_pixel_format(rfbClient* rfb_client, int color_depth) {
    rfb_client->format.trueColour = 1;
    rfb_client->format.bigEndian = 0;

    switch(color_depth) {
        case 8:
            rfb_client->format.depth        = 8;
            rfb_client->format.bitsPerPixel = 8;
            rfb_client->format.blueShift    = 6;
            rfb_client->format.redShift     = 0;
            rfb_client->format.greenShift   = 3;
            rfb_client->format.blueMax      = 3;
            rfb_client->format.redMax       = 7;
            rfb_client->format.greenMax     = 7;
            break;

        case 16:
            rfb_client->format.depth        = 16;
            rfb_client->format.bitsPerPixel = 16;
            rfb_client->format.blueShift    = 0;
            rfb_client->format.redShift     = 11;
            rfb_client->format.greenShift   = 5;
            rfb_client->format.blueMax      = 0x1f;
            rfb_client->format.redMax       = 0x1f;
            rfb_client->format.greenMax     = 0x3f;
            break;

        case 24:
        case 32:
        default:
            rfb_client->format.depth        = 24;
            rfb_client->format.bitsPerPixel = 32;
            rfb_client->format.blueShift    = 0;
            rfb_client->format.redShift     = 16;
            rfb_client->format.greenShift   = 8;
            rfb_client->format.blueMax      = 0xff;
            rfb_client->format.redMax       = 0xff;
            rfb_client->format.greenMax     = 0xff;
            break;
    }
}

// Made with Bob
