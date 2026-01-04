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

#include "input.h"
#include "kubevirt.h"

#include <guacamole/client.h>
#include <guacamole/recording.h>
#include <guacamole/user.h>

#include <rfb/rfbclient.h>
#include <stdlib.h>

int guac_kubevirt_user_mouse_handler(guac_user* user, int x, int y, int mask) {

    guac_client* client = user->client;
    guac_kubevirt_client* kubevirt_client =
        (guac_kubevirt_client*) client->data;

    /* Record mouse event if recording */
    if (kubevirt_client->recording != NULL)
        guac_recording_report_mouse(kubevirt_client->recording, x, y, mask);

    /* Skip if VNC client is not initialized */
    if (kubevirt_client->rfb_client == NULL)
        return 0;

    /* Convert Guacamole mouse button mask to VNC button mask */
    int vnc_mask = 0;
    if (mask & 0x01) vnc_mask |= 0x01;  /* Left button */
    if (mask & 0x02) vnc_mask |= 0x04;  /* Middle button */
    if (mask & 0x04) vnc_mask |= 0x02;  /* Right button */
    if (mask & 0x08) vnc_mask |= 0x08;  /* Scroll up */
    if (mask & 0x10) vnc_mask |= 0x10;  /* Scroll down */

    /* Send mouse event using libvncclient */
    SendPointerEvent(kubevirt_client->rfb_client, x, y, vnc_mask);

    return 0;
}

int guac_kubevirt_user_key_handler(guac_user* user, int keysym, int pressed) {

    guac_client* client = user->client;
    guac_kubevirt_client* kubevirt_client =
        (guac_kubevirt_client*) client->data;

    /* Record key event if recording */
    if (kubevirt_client->recording != NULL)
        guac_recording_report_key(kubevirt_client->recording,
                keysym, pressed);

    /* Skip if VNC client is not initialized */
    if (kubevirt_client->rfb_client == NULL)
        return 0;

    /* Send key event using libvncclient */
    SendKeyEvent(kubevirt_client->rfb_client, keysym, pressed);

    return 0;
}
