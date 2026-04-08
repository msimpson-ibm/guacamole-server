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

#ifndef GUAC_KUBEVIRT_KEYBOARD_H
#define GUAC_KUBEVIRT_KEYBOARD_H

#include "keymap.h"

#include <guacamole/client.h>
#include <rfb/rfbclient.h>

/**
 * The current keyboard state of a KubeVirt VNC session.
 */
typedef struct guac_kubevirt_keyboard {

    /**
     * The guac_client associated with the KubeVirt session whose keyboard
     * state is being managed by this guac_kubevirt_keyboard.
     */
    guac_client* client;

    /**
     * The VNC client instance associated with the current session.
     */
    rfbClient* rfb_client;

    /**
     * The keymap being used to translate keysyms for the remote server's
     * keyboard layout.
     */
    const guac_kubevirt_keymap* keymap;

    /**
     * Whether the QEMU Extended Key Event extension is supported by the server.
     * When enabled, hardware scancodes are sent instead of X11 keysyms,
     * providing better keyboard handling for virtualized guests.
     */
    int qemu_extended_key_event;

} guac_kubevirt_keyboard;

/**
 * Allocates a new guac_kubevirt_keyboard which manages the keyboard state of
 * the KubeVirt VNC session associated with the given guac_client. The returned
 * guac_kubevirt_keyboard must eventually be freed with
 * guac_kubevirt_keyboard_free().
 *
 * @param client
 *     The guac_client associated with the KubeVirt session whose keyboard
 *     state is to be managed by the newly-allocated guac_kubevirt_keyboard.
 *
 * @param rfb_client
 *     The VNC client instance for sending key events.
 *
 * @param keymap
 *     The keymap which should be used to translate keyboard events.
 *
 * @return
 *     A newly-allocated guac_kubevirt_keyboard which manages the keyboard
 *     state for the KubeVirt session associated with the given guac_client.
 */
guac_kubevirt_keyboard* guac_kubevirt_keyboard_alloc(guac_client* client,
        rfbClient* rfb_client, const guac_kubevirt_keymap* keymap);

/**
 * Frees all memory allocated for the given guac_kubevirt_keyboard.
 *
 * @param keyboard
 *     The guac_kubevirt_keyboard instance which should be freed.
 */
void guac_kubevirt_keyboard_free(guac_kubevirt_keyboard* keyboard);

/**
 * Sends the appropriate key event to the VNC server for the given keysym.
 * This function handles keymap-aware translation when necessary.
 *
 * @param keyboard
 *     The guac_kubevirt_keyboard associated with the current session.
 *
 * @param keysym
 *     The X11 keysym being pressed or released.
 *
 * @param pressed
 *     Non-zero if the key is being pressed, zero if being released.
 *
 * @return
 *     Zero if the key event was successfully sent, non-zero otherwise.
 */
int guac_kubevirt_keyboard_send_key(guac_kubevirt_keyboard* keyboard,
        int keysym, int pressed);

#endif

// Made with Bob
