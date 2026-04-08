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

#include "keyboard.h"
#include "keymap.h"
#include "scancode.h"

#include <guacamole/client.h>
#include <guacamole/mem.h>
#include <rfb/rfbclient.h>
#include <rfb/rfbproto.h>

#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

/* QEMU Extended Key Event message type */
#define QEMU_EXTENDED_KEY_EVENT 255

/**
 * Sends a QEMU Extended Key Event message to the VNC server.
 * This sends hardware scancodes instead of X11 keysyms.
 *
 * @param rfb_client
 *     The VNC client connection.
 *
 * @param down
 *     Non-zero if the key is being pressed, zero if released.
 *
 * @param keysym
 *     The X11 keysym (for fallback/logging purposes).
 *
 * @param scancode
 *     The XT scancode to send.
 *
 * @return
 *     TRUE if successful, FALSE otherwise.
 */
static rfbBool send_qemu_extended_key_event(rfbClient* rfb_client,
        rfbBool down, uint32_t keysym, uint32_t scancode) {
    
    /* QEMU Extended Key Event message format:
     * 1 byte: message type (255)
     * 1 byte: submessage type (0 for key event)
     * 2 bytes: down flag (network byte order)
     * 4 bytes: keysym (network byte order, for compatibility)
     * 4 bytes: scancode (network byte order)
     */
    uint8_t msg[12];
    
    msg[0] = QEMU_EXTENDED_KEY_EVENT;  /* Message type */
    msg[1] = 0;  /* Submessage type: key event */
    
    /* Down flag (16-bit, network byte order) */
    uint16_t down_flag = down ? 1 : 0;
    uint16_t down_net = htons(down_flag);
    memcpy(&msg[2], &down_net, 2);
    
    /* Keysym (32-bit, network byte order) */
    uint32_t keysym_net = htonl(keysym);
    memcpy(&msg[4], &keysym_net, 4);
    
    /* Scancode (32-bit, network byte order) */
    uint32_t scancode_net = htonl(scancode);
    memcpy(&msg[8], &scancode_net, 4);
    
    /* Send the message */
    if (WriteToRFBServer(rfb_client, (char*)msg, sizeof(msg))) {
        return TRUE;
    }
    
    return FALSE;
}

guac_kubevirt_keyboard* guac_kubevirt_keyboard_alloc(guac_client* client,
        rfbClient* rfb_client, const guac_kubevirt_keymap* keymap) {

    /* Allocate keyboard structure */
    guac_kubevirt_keyboard* keyboard = guac_mem_zalloc(sizeof(guac_kubevirt_keyboard));

    /* Store client and VNC client references */
    keyboard->client = client;
    keyboard->rfb_client = rfb_client;
    keyboard->keymap = keymap;
    
    /* Enable QEMU Extended Key Event by default - will be used if server supports it */
    keyboard->qemu_extended_key_event = 1;

    guac_client_log(client, GUAC_LOG_DEBUG,
            "Keyboard initialized with layout: %s, QEMU Extended Key Events: %s",
            keymap ? keymap->name : "default",
            keyboard->qemu_extended_key_event ? "enabled" : "disabled");

    return keyboard;
}

void guac_kubevirt_keyboard_free(guac_kubevirt_keyboard* keyboard) {
    guac_mem_free(keyboard);
}

int guac_kubevirt_keyboard_send_key(guac_kubevirt_keyboard* keyboard,
        int keysym, int pressed) {

    /* Skip if VNC client is not initialized */
    if (keyboard->rfb_client == NULL)
        return 1;

    /* Try to use QEMU Extended Key Event if enabled */
    if (keyboard->qemu_extended_key_event) {
        guac_kubevirt_scancode_mapping mapping;
        
        /* Get layout name from keymap if available */
        const char* layout_name = keyboard->keymap ? keyboard->keymap->name : NULL;
        
        if (guac_kubevirt_keysym_to_scancode(keysym, &mapping, layout_name)) {
            /* For extended keys, we need to send the 0xE0 prefix as a separate event
             * before sending the actual scancode. This is how QEMU expects it. */
            if (mapping.extended && pressed) {
                /* Send 0xE0 prefix key down */
                guac_client_log(keyboard->client, GUAC_LOG_TRACE,
                        "Sending 0xE0 prefix for extended key");
                send_qemu_extended_key_event(keyboard->rfb_client, 1, 0, 0xE0);
            }
            
            /* Send the actual scancode (without extended flag) */
            uint32_t scancode = mapping.scancode & 0xFF;
            
            /* Handle modifier keys if needed */
            if (pressed && mapping.modifiers) {
                /* Press modifier keys first */
                if (mapping.modifiers & GUAC_KUBEVIRT_MODIFIER_SHIFT) {
                    guac_client_log(keyboard->client, GUAC_LOG_TRACE,
                            "Pressing Shift modifier for keysym=0x%X", keysym);
                    send_qemu_extended_key_event(keyboard->rfb_client, 1, XK_Shift_L, 0x2A);
                }
                if (mapping.modifiers & GUAC_KUBEVIRT_MODIFIER_ALTGR) {
                    guac_client_log(keyboard->client, GUAC_LOG_TRACE,
                            "Pressing AltGr modifier for keysym=0x%X", keysym);
                    /* AltGr is Right Alt, scancode 0x38 with E0 prefix */
                    send_qemu_extended_key_event(keyboard->rfb_client, 1, XK_ISO_Level3_Shift, 0xE038);
                }
            }
            
            guac_client_log(keyboard->client, GUAC_LOG_TRACE,
                    "Sending QEMU Extended Key Event: keysym=0x%X, scancode=0x%X, down=%d, modifiers=0x%X, extended=%d",
                    keysym, scancode, pressed, mapping.modifiers, mapping.extended);
            
            if (send_qemu_extended_key_event(keyboard->rfb_client,
                    pressed, keysym, scancode)) {
                
                /* For extended keys, send the 0xE0 prefix release after key release */
                if (mapping.extended && !pressed) {
                    guac_client_log(keyboard->client, GUAC_LOG_TRACE,
                            "Sending 0xE0 prefix release for extended key");
                    send_qemu_extended_key_event(keyboard->rfb_client, 0, 0, 0xE0);
                }
                
                /* Release modifier keys after key release */
                if (!pressed && mapping.modifiers) {
                    if (mapping.modifiers & GUAC_KUBEVIRT_MODIFIER_ALTGR) {
                        guac_client_log(keyboard->client, GUAC_LOG_TRACE,
                                "Releasing AltGr modifier for keysym=0x%X", keysym);
                        send_qemu_extended_key_event(keyboard->rfb_client, 0, XK_ISO_Level3_Shift, 0xE038);
                    }
                    if (mapping.modifiers & GUAC_KUBEVIRT_MODIFIER_SHIFT) {
                        guac_client_log(keyboard->client, GUAC_LOG_TRACE,
                                "Releasing Shift modifier for keysym=0x%X", keysym);
                        send_qemu_extended_key_event(keyboard->rfb_client, 0, XK_Shift_L, 0x2A);
                    }
                }
                
                return 0;  /* Success */
            }
            
            /* If sending failed, fall back to standard key event */
            guac_client_log(keyboard->client, GUAC_LOG_DEBUG,
                    "QEMU Extended Key Event failed, falling back to standard key event");
            keyboard->qemu_extended_key_event = 0;  /* Disable for future keys */
        }
        else {
            /* No scancode mapping available, use standard key event */
            guac_client_log(keyboard->client, GUAC_LOG_TRACE,
                    "No scancode mapping for keysym 0x%X, using standard key event",
                    keysym);
        }
    }

    /* Fall back to standard VNC key event (X11 keysyms) */
    guac_client_log(keyboard->client, GUAC_LOG_TRACE,
            "Sending standard key event: keysym=0x%X, down=%d", keysym, pressed);
    SendKeyEvent(keyboard->rfb_client, keysym, pressed);

    return 0;
}

// Made with Bob
