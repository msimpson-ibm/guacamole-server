# KubeVirt Protocol for Apache Guacamole

This protocol implementation adds support for connecting to KubeVirt VirtualMachineInstance VNC consoles via WebSocket.

## Overview

The KubeVirt protocol enables Apache Guacamole to connect directly to KubeVirt VM consoles using the Kubernetes API's WebSocket VNC endpoint. This provides a seamless web-based console experience for KubeVirt virtual machines.

## Features

- **VNC over WebSocket**: Connects to KubeVirt VNC console via the Kubernetes API
- **Kubernetes Authentication**: Uses bearer token authentication
- **SSL/TLS Support**: Supports secure connections to Kubernetes API
- **Display Management**: Full graphical display with configurable color depth
- **Input Handling**: Mouse and keyboard input support
- **Clipboard**: Bidirectional clipboard support (copy/paste)
- **Recording**: Session recording capability
- **Read-only Mode**: Optional read-only access

## Dependencies

- libwebsockets (WebSocket client library)
- libvncserver (VNC protocol definitions)
- OpenSSL (for TLS connections)

## Connection Parameters

### Required Parameters

- **hostname**: Kubernetes API server hostname or IP address
- **namespace**: Kubernetes namespace where the VirtualMachineInstance exists
- **vm-name**: Name of the VirtualMachineInstance
- **token**: Kubernetes bearer token for authentication

### Optional Parameters

- **port**: Kubernetes API port (default: 6443)
- **use-ssl**: Use SSL/TLS for connection (default: true)
- **ignore-cert**: Ignore certificate validation errors (default: false)
- **ca-cert**: CA certificate in PEM format for server verification
- **read-only**: Enable read-only mode (default: false)
- **color-depth**: Color depth in bits - 8, 16, 24, or 32 (default: 24)
- **remote-cursor**: Render cursor remotely vs locally (default: false)
- **swap-red-blue**: Swap red and blue color components (default: false)
- **clipboard-buffer-size**: Maximum clipboard size in bytes (default: 262144)
- **disable-copy**: Disable copying from remote to client (default: false)
- **disable-paste**: Disable pasting from client to remote (default: false)
- **retries**: Connection retry attempts (default: 5)

### Recording Parameters

- **recording-path**: Directory path for session recordings
- **recording-name**: Filename for the recording (default: "recording")
- **create-recording-path**: Auto-create recording directory (default: false)
- **recording-exclude-output**: Exclude graphical output from recording (default: false)
- **recording-exclude-mouse**: Exclude mouse events from recording (default: false)
- **recording-include-keys**: Include key events in recording (default: false)
- **recording-write-existing**: Append to existing recording files (default: false)

## Usage Example

### Guacamole Connection Configuration

```xml
<connection>
    <protocol>kubevirt</protocol>
    <param name="hostname">kubernetes.example.com</param>
    <param name="port">6443</param>
    <param name="namespace">default</param>
    <param name="vm-name">my-vm</param>
    <param name="token">eyJhbGciOiJSUzI1NiIsImtpZCI6...</param>
    <param name="use-ssl">true</param>
    <param name="ignore-cert">false</param>
</connection>
```

### Getting a Kubernetes Token

To obtain a bearer token for authentication:

```bash
# Create a service account
kubectl create serviceaccount guacamole-viewer

# Create role with VNC access
kubectl create role vnc-viewer \
  --verb=get \
  --resource=virtualmachineinstances/vnc

# Bind the role
kubectl create rolebinding vnc-viewer-binding \
  --role=vnc-viewer \
  --serviceaccount=default:guacamole-viewer

# Get the token
kubectl create token guacamole-viewer
```

## Building

The KubeVirt protocol is automatically built if the required dependencies are available:

```bash
./configure --enable-kubevirt
make
sudo make install
```

To explicitly disable KubeVirt support:

```bash
./configure --disable-kubevirt
```

## How It Works

1. **WebSocket Connection**: Establishes a WebSocket connection to the Kubernetes API endpoint:
   ```
   wss://<hostname>:<port>/apis/subresources.kubevirt.io/v1/namespaces/<namespace>/virtualmachineinstances/<vm-name>/vnc
   ```

2. **Authentication**: Uses HTTP header authentication with bearer token:
   ```
   Authorization: Bearer <token>
   ```

3. **VNC Protocol**: Implements the RFB (Remote Framebuffer) protocol over the WebSocket connection:
   - Protocol version negotiation
   - Security handshake (None type for now)
   - Client/Server initialization
   - Framebuffer updates
   - User input events

4. **Display Rendering**: Uses Guacamole's display layer system for efficient rendering

## Limitations

- Currently supports only VNC security type "None" (authentication handled by Kubernetes)
- Implements basic VNC encoding (Raw encoding)
- Advanced encodings (Tight, ZRLE, etc.) not yet implemented
- Audio redirection not supported

## Future Enhancements

- Support for additional VNC encodings (Tight, ZRLE, Hextile)
- Improved framebuffer update handling
- Copy rectangle optimization
- Desktop resize support
- Better error handling and reconnection logic

## Security Considerations

1. **Token Security**: Bearer tokens provide full access permissions - store them securely
2. **Certificate Validation**: Do not use `ignore-cert=true` in production
3. **Network Security**: Always use SSL/TLS (`use-ssl=true`) in production
4. **RBAC**: Limit service account permissions to minimum required (get VNC subresource)
5. **Token Expiration**: Use short-lived tokens when possible

## Troubleshooting

### Connection Failed

- Verify Kubernetes API is reachable
- Check token has correct permissions
- Ensure VirtualMachineInstance exists and is running
- Verify namespace and VM name are correct

### Certificate Errors

- Provide CA certificate via `ca-cert` parameter
- Verify hostname matches certificate
- Check certificate expiration

### Display Issues

- Try different color depths
- Enable/disable `swap-red-blue` if colors are incorrect
- Check VNC console is active on the VM

## License

Licensed under the Apache License, Version 2.0. See the LICENSE file for details.

## Contributing

Contributions are welcome! Please follow the Apache Guacamole contribution guidelines.
