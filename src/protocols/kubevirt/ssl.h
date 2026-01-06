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

#ifndef GUAC_KUBEVIRT_SSL_H
#define GUAC_KUBEVIRT_SSL_H

#include <guacamole/client.h>
#include <openssl/ssl.h>

/**
 * Initializes SSL for the given client, configuring hostname checks and
 * certificate verification as specified by the associated settings. This
 * function should be invoked by the libwebsockets callback handling
 * LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS.
 *
 * @param client
 *     The guac_client associated with the Kubernetes connection.
 *
 * @param context
 *     The SSL_CTX structure associated with the connection over which
 *     Kubernetes is being run.
 */
void guac_kubevirt_init_ssl(guac_client* client, SSL_CTX* context);

#endif /* GUAC_KUBEVIRT_SSL_H */
