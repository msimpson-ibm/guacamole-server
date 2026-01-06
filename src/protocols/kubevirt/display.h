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

#ifndef GUAC_KUBEVIRT_DISPLAY_H
#define GUAC_KUBEVIRT_DISPLAY_H

#include <rfb/rfbclient.h>

/**
 * Callback invoked by libVNCServer when the framebuffer size changes.
 * This allocates the framebuffer and updates the display size.
 *
 * @param rfb_client
 *     The VNC client associated with the framebuffer being resized.
 *
 * @return
 *     TRUE if the framebuffer was successfully allocated, FALSE otherwise.
 */
rfbBool guac_kubevirt_resize_framebuffer(rfbClient* rfb_client);

/**
 * Callback invoked by libVNCServer when a framebuffer update is received.
 * This copies the updated region to the guac_display layer.
 *
 * @param rfb_client
 *     The VNC client associated with the framebuffer update.
 *
 * @param x
 *     The X coordinate of the upper-left corner of the updated region.
 *
 * @param y
 *     The Y coordinate of the upper-left corner of the updated region.
 *
 * @param w
 *     The width of the updated region.
 *
 * @param h
 *     The height of the updated region.
 */
void guac_kubevirt_framebuffer_update(rfbClient* rfb_client,
        int x, int y, int w, int h);

/**
 * Sets the pixel format to request from the VNC server. Note that the VNC
 * server is not required to honor this request.
 *
 * @param rfb_client
 *     The rfbClient associated with the VNC connection.
 *
 * @param color_depth
 *     The desired color depth in bits per pixel. Valid values are 8, 16, 24, and 32.
 */
void guac_kubevirt_set_pixel_format(rfbClient* rfb_client, int color_depth);

#endif /* GUAC_KUBEVIRT_DISPLAY_H */

// Made with Bob
