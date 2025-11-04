# WebSocket Server Functions - Complete Explanation

## Overview
This file implements a **WebSocket server** that allows real-time, two-way communication between clients and the server. WebSockets are like a special phone line - once connected, you can send and receive messages instantly without having to keep asking "is there a message for me?"

---

## Core Functions Explained

### 🔑 **Helper Functions (Low-level)**

#### 1. `base64_encode(input, length, output)`
- **What it does**: Converts binary data into text format (Base64)
- **Why it's needed**: WebSocket handshake requires the security key to be Base64 encoded
- **Simple example**: Binary `\x12\x34` → Text string `EjQ=`

#### 2. `generate_accept_header(key, accept)`
- **What it does**: Creates a secure response key for the WebSocket handshake
- **Why it's needed**: When a client connects, it sends a key. The server must create a specific response using that key + a special string + SHA1 hashing
- **Used by**: Called when accepting new WebSocket connections

#### 3. `parse_websocket_handshake(request, key)`
- **What it does**: Reads the client's initial connection request and extracts the security key
- **Input**: Raw HTTP request text from client
- **Output**: The security key (stored in `key` variable)
- **Returns**: 1 if successful, 0 if failed

---

### 📦 **Frame Handling Functions**

#### 4. `parse_ws_frame(data, data_len, frame)`
- **What it does**: Breaks down WebSocket messages into readable pieces
- **Handles**:
  - ✅ Text messages from clients
  - ✅ Close signals (when client disconnects)
  - ✅ Ping/Pong signals (keep-alive checks)
  - ✅ Masked/Unmasked payloads
  - ✅ Large messages (even 1GB+ if needed)
- **Returns**: Size of the complete frame if successful, -1 if parsing failed

**Example**: Raw bytes → Extracts message content + checks if it's complete

#### 5. `create_ws_frame(data, data_len, frame, frame_size)`
- **What it does**: Packages a message into WebSocket frame format to send to clients
- **Input**: Message text and its length
- **Output**: Binary frame data ready to send
- **Returns**: Size of the frame created, -1 if failed
- **Opposite of**: `parse_ws_frame()` - it creates frames instead of parsing them

---

### 👥 **Client Connection Management**

#### 6. `websocket_client_handler(arg)` ⭐ **Main Function**
- **What it does**: Handles communication with ONE connected client
- **Runs as**: A separate thread (one thread per client)
- **Steps**:
  1. Read initial WebSocket handshake from client
  2. Parse and validate the security key
  3. Send handshake response (confirming connection)
  4. Add client to the server's client list
  5. Enter infinite loop waiting for messages
  6. For each message:
     - Parse it to extract the content
     - Broadcast it to all other connected clients
  7. Handle special opcodes:
     - **Text (0x1)**: Regular message
     - **Close (0x8)**: Client disconnecting
     - **Ping (0x9)**: Keep-alive request (respond with Pong)
  8. Remove client when done

**Key detail**: Runs in its own thread - doesn't block the main server

#### 7. `websocket_accept_connections(arg)` ⭐ **Main Function**
- **What it does**: Listens for new clients trying to connect
- **Runs as**: A separate thread (runs constantly)
- **What it does every time**:
  1. Wait for a new connection request
  2. Accept the connection
  3. Create a new thread to handle that client
  4. Use `pthread_detach()` so the thread cleans up automatically
  5. Loop back to step 1

**Like a receptionist**: Takes calls, assigns them to a person, then takes the next call

---

### 🚀 **Server Control Functions**

#### 8. `websocket_init(port)`
- **What it does**: Sets up the WebSocket server on a specific port
- **Input**: Port number (e.g., 7070)
- **Steps**:
  1. Create a listening socket
  2. Set socket options (allow port reuse)
  3. Bind to the specified port
  4. Listen for incoming connections
  5. Initialize the client list
- **Returns**: 0 if successful, -1 if failed
- **When to call**: Once, at the start of your program

#### 9. `websocket_start()`
- **What it does**: Actually starts the server
- **Steps**:
  1. Set `g_running = 1` (flag to keep server active)
  2. Create the accept thread (starts listening)
  3. Wait for that thread (blocks until server stops)
- **Returns**: 0 if successful, -1 if failed
- **When to call**: After `websocket_init()`

#### 10. `websocket_stop()`
- **What it does**: Gracefully stops the server
- **Steps**:
  1. Set `g_running = 0` (tells all threads to stop)
  2. Close the listening socket
- **When to call**: When you want to shut down

#### 11. `websocket_cleanup()`
- **What it does**: Frees up memory used by the server
- **Destroys**: Mutex locks and other resources
- **When to call**: After `websocket_stop()` - right before program ends

---

### 👤 **Client List Management**

#### 12. `websocket_add_client(fd, user_id, room_id)`
- **What it does**: Registers a new client in the server's list
- **Input**:
  - `fd`: File descriptor (connection ID)
  - `user_id`: Which user this is
  - `room_id`: Which chat room they're in
- **Returns**: Client index if successful, -1 if list is full
- **Protection**: Uses a mutex lock to prevent conflicts

#### 13. `websocket_remove_client(fd)`
- **What it does**: Unregisters a client when they disconnect
- **Input**: `fd` (connection ID)
- **Steps**:
  1. Find the client in the list
  2. Mark as disconnected
  3. Shift remaining clients up to fill the gap
  4. Decrease client count
- **Returns**: Client index if found, -1 if not found
- **Protection**: Uses mutex lock

#### 14. `websocket_get_client_index(fd)`
- **What it does**: Finds a client in the list by file descriptor
- **Input**: `fd` (connection ID)
- **Returns**: Index of client if found, -1 if not found
- **Speed**: O(n) - searches through all clients

---

### 📢 **Broadcasting Functions**

#### 15. `websocket_broadcast_to_room(room_id, message)` ⭐ **Important**
- **What it does**: Sends a message to all clients in a specific chat room
- **Input**:
  - `room_id`: Which room to broadcast to
  - `message`: The text to send
- **Steps**:
  1. Convert message into WebSocket frame
  2. Lock the client list
  3. Loop through all clients
  4. Send to clients who are:
     - Connected ✅
     - In the specified room ✅
  5. Count how many received it
- **Returns**: Number of clients who got the message
- **Use case**: Chat room gets a new message → broadcast to all users in that room

#### 16. `websocket_send_to_client(fd, message)` ⭐ **Important**
- **What it does**: Sends a private message to one specific client
- **Input**:
  - `fd`: Connection ID of target client
  - `message`: The text to send
- **Steps**:
  1. Convert message into WebSocket frame
  2. Send it to the specific client
- **Returns**: 0 if successful, -1 if failed
- **Use case**: Send a private message or notification to one user

---

## Quick Flow Diagram

```
┌─────────────────────────────────────────────────────────────┐
│  1. websocket_init() - Set up server                       │
│  2. websocket_start() - Start listening for clients        │
└─────────────────────────────────────────────────────────────┘
                          ↓
         ┌────────────────────────────────────┐
         │ websocket_accept_connections()     │
         │ (Waits for client connections)    │
         └────────────────────────────────────┘
                          ↓
              (New client connects)
                          ↓
         ┌────────────────────────────────────┐
         │ websocket_client_handler()         │
         │ (One thread per client)            │
         │                                    │
         │ - Parse handshake                 │
         │ - Add client to list              │
         │ - Loop: Receive messages          │
         │ - Broadcast to other clients      │
         │ - Remove when disconnected        │
         └────────────────────────────────────┘
```

---

## Usage Example

```c
// Start the server
websocket_init(7070);        // Port 7070
websocket_start();           // Blocks until stopped

// In another thread, send a message to room 5
websocket_broadcast_to_room(5, "Hello everyone!");

// Send private message to client with fd=10
websocket_send_to_client(10, "Hello you!");

// Stop the server
websocket_stop();
websocket_cleanup();
```

---

## Key Concepts to Remember

1. **Threads**: Each client gets its own thread so multiple clients can connect simultaneously
2. **Mutexes**: Locks protect the client list from corruption when multiple threads access it
3. **WebSocket Protocol**: Converts regular TCP connections into WebSocket connections during handshake
4. **Frames**: Messages are split into frames (header + data) following WebSocket protocol rules
5. **Broadcasting**: Sends the same message to multiple clients in a room

This is the **real-time communication backbone** of your chat application! 🎯
