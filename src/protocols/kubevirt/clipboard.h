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

#ifndef GUAC_KUBEVIRT_CLIPBOARD_H
#define GUAC_KUBEVIRT_CLIPBOARD_H

#include <guacamole/stream.h>
#include <guacamole/user.h>

/**
 * Handler for inbound clipboard data from the user.
 */
int guac_kubevirt_clipboard_receive_handler(guac_user* user, guac_stream* stream, char* mimetype);

/**
 * Handler for blob data within an inbound clipboard stream.
 */
int guac_kubevirt_clipboard_blob_handler(guac_user* user, guac_stream* stream,
        void* data, int length);

/**
 * Handler for end of inbound clipboard stream.
 */
int guac_kubevirt_clipboard_end_handler(guac_user* user, guac_stream* stream);

/**
 * Sends clipboard data to all connected users.
 *
 * @param client
 *     The guac_client associated with the KubeVirt connection.
 *
 * @param data
 *     The clipboard data to send.
 *
 * @param length
 *     The length of the clipboard data.
 */
void guac_kubevirt_clipboard_send(guac_client* client,
        const char* data, int length);

#endif /* GUAC_KUBEVIRT_CLIPBOARD_H */
