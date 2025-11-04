'use client';

import { useEffect, useRef, useCallback, useState } from 'react';
import { useChatStore, Message, User } from '@/lib/store';
import { getWebSocketURL, roomApi } from '@/lib/api';

interface WebSocketMessage {
  type: 'message' | 'user_joined' | 'user_left' | 'message_history' | 'error' | 'connected' | 'pong' | 'ping';
  room_id?: number;
  user_id?: number;
  username?: string;
  content?: string;
  timestamp?: number;
  message_id?: string;
  messages?: any[];
  data?: any;
}

interface UseWebSocketOptions {
  enabled?: boolean;
  autoConnect?: boolean;
  reconnectAttempts?: number;
  reconnectDelay?: number;
}


export const useWebSocket = (
  roomIdOrOptions?: number | null | UseWebSocketOptions,
  userIdParam?: number | null,
  usernameParam?: string | null,
  optionsParam?: UseWebSocketOptions
) => {
  // Handle backward compatibility - if first param is a number, it's the old signature
  let options: UseWebSocketOptions = {};
  
  if (typeof roomIdOrOptions === 'object' && roomIdOrOptions !== null) {
    options = roomIdOrOptions;
  } else if (typeof roomIdOrOptions === 'number' || roomIdOrOptions === null) {
    // Old signature - ignore these params, use new store-based approach
    options = optionsParam || {};
  }

  const {
    enabled = true,
    autoConnect = true,
    reconnectAttempts = 5,
    reconnectDelay = 3000,
  } = options;

  const wsRef = useRef<WebSocket | null>(null);
  const reconnectAttemptsRef = useRef(0);
  const reconnectTimeoutRef = useRef<NodeJS.Timeout | null>(null);
  const pingIntervalRef = useRef<NodeJS.Timeout | null>(null);
  const messageQueueRef = useRef<WebSocketMessage[]>([]);

  const [isConnected, setIsConnected] = useState(false);
  const [connectionError, setConnectionError] = useState<string | null>(null);

  // Store actions
  const user = useChatStore((state) => state.user);
  const currentRoom = useChatStore((state) => state.currentRoom);
  const addMessage = useChatStore((state) => state.addMessage);
  const setMessages = useChatStore((state) => state.setMessages);
  const setError = useChatStore((state) => state.setError);
  const addUserRoomMembership = useChatStore((state) => state.addUserRoomMembership);

  /**
   * Connect to WebSocket server
   */
  const connect = useCallback(() => {
    if (!enabled || wsRef.current?.readyState === WebSocket.OPEN) {
      return;
    }

    try {
      const wsUrl = getWebSocketURL();
      console.log('[WebSocket] Connecting to:', wsUrl);

      wsRef.current = new WebSocket(wsUrl);

      wsRef.current.onopen = () => {
        console.log('[WebSocket] Connected');
        setIsConnected(true);
        setConnectionError(null);
        reconnectAttemptsRef.current = 0;

        // Clear message queue and resend any pending messages
        const queuedMessages = [...messageQueueRef.current];
        messageQueueRef.current = [];
        queuedMessages.forEach((msg) => send(msg));

        // Set up ping/pong to keep connection alive
        setPingInterval();

        // If user is in a room, rejoin and fetch messages
        if (user && currentRoom) {
          rejoinRoom();
        }
      };

      wsRef.current.onmessage = (event) => {
        try {
          const message: WebSocketMessage = JSON.parse(event.data);
          handleWebSocketMessage(message);
        } catch (error) {
          console.error('[WebSocket] Failed to parse message:', error, event.data);
        }
      };

      wsRef.current.onerror = (error) => {
        console.error('[WebSocket] Error:', error);
        setConnectionError('WebSocket connection error');
        setError('Connection error. Please try refreshing the page.');
      };

      wsRef.current.onclose = () => {
        console.log('[WebSocket] Disconnected');
        setIsConnected(false);
        clearPingInterval();

        // Attempt to reconnect
        if (enabled && reconnectAttemptsRef.current < reconnectAttempts) {
          reconnectAttemptsRef.current += 1;
          console.log(
            `[WebSocket] Reconnecting... Attempt ${reconnectAttemptsRef.current}/${reconnectAttempts}`
          );

          reconnectTimeoutRef.current = setTimeout(() => {
            connect();
          }, reconnectDelay);
        } else if (reconnectAttemptsRef.current >= reconnectAttempts) {
          setConnectionError('Failed to connect after multiple attempts');
          setError('Unable to establish WebSocket connection. Please refresh the page.');
        }
      };
    } catch (error) {
      console.error('[WebSocket] Connection failed:', error);
      setConnectionError('Failed to create WebSocket connection');
      setError('Failed to connect to chat server');
    }
  }, [enabled, reconnectAttempts, reconnectDelay, user, currentRoom, setError, addUserRoomMembership]);

  /**
   * Disconnect from WebSocket server
   */
  const disconnect = useCallback(() => {
    console.log('[WebSocket] Disconnecting');
    clearPingInterval();

    if (reconnectTimeoutRef.current) {
      clearTimeout(reconnectTimeoutRef.current);
      reconnectTimeoutRef.current = null;
    }

    if (wsRef.current) {
      wsRef.current.close();
      wsRef.current = null;
    }

    setIsConnected(false);
  }, []);

  /**
   * Send a message through WebSocket
   */
  const send = useCallback((message: WebSocketMessage) => {
    if (!wsRef.current || wsRef.current.readyState !== WebSocket.OPEN) {
      console.warn('[WebSocket] Not connected, queueing message:', message);
      messageQueueRef.current.push(message);
      return false;
    }

    try {
      wsRef.current.send(JSON.stringify(message));
      console.log('[WebSocket] Message sent:', message);
      return true;
    } catch (error) {
      console.error('[WebSocket] Failed to send message:', error);
      messageQueueRef.current.push(message);
      return false;
    }
  }, []);

  /**
   * Join a room via WebSocket
   */
  const joinRoom = useCallback(
    async (roomId: number) => {
      if (!user) {
        console.error('[WebSocket] Cannot join room: user not authenticated');
        return false;
      }

      try {
        // First, join via HTTP API to register membership
        await roomApi.joinRoom(roomId, user.user_id);
        addUserRoomMembership(roomId);

        // Then join via WebSocket
        const message: WebSocketMessage = {
          type: 'user_joined',
          room_id: roomId,
          user_id: user.user_id,
          username: user.username,
        };

        return send(message);
      } catch (error) {
        console.error('[WebSocket] Failed to join room:', error);
        const errorMsg = error instanceof Error ? error.message : 'Failed to join room';
        setError(errorMsg);
        return false;
      }
    },
    [user, send, addUserRoomMembership, setError]
  );

  /**
   * Rejoin current room (used on reconnection)
   */
  const rejoinRoom = useCallback(() => {
    if (currentRoom && user) {
      console.log('[WebSocket] Rejoining room:', currentRoom.room_id);
      joinRoom(currentRoom.room_id);
    }
  }, [currentRoom, user, joinRoom]);

  /**
   * Send a chat message
   */
  const sendMessage = useCallback(
    (content: string) => {
      if (!user || !currentRoom) {
        console.error('[WebSocket] Cannot send message: user or room not set');
        return false;
      }

      if (!content.trim()) {
        console.warn('[WebSocket] Ignoring empty message');
        return false;
      }

      const messageId = `${user.user_id}-${Date.now()}`;
      const timestamp = Date.now();

      // Create the message object
      const message: WebSocketMessage = {
        type: 'message',
        room_id: currentRoom.room_id,
        user_id: user.user_id,
        username: user.username,
        content: content.trim(),
        timestamp: timestamp,
        message_id: messageId,
      };

      // Immediately add message to store (optimistic update)
      const chatMessage: Message = {
        id: messageId,
        sender_id: user.user_id,
        sender_name: user.username,
        content: content.trim(),
        timestamp: timestamp,
        room_id: currentRoom.room_id,
      };

      addMessage(chatMessage);
      console.log('[WebSocket] Message added to store (optimistic update):', chatMessage);

      // Send via WebSocket
      return send(message);
    },
    [user, currentRoom, send, addMessage]
  );

  /**
   * Fetch chat history for current room
   */
  const fetchChatHistory = useCallback(async () => {
    if (!currentRoom) {
      console.warn('[WebSocket] Cannot fetch history: no current room');
      return;
    }

    if (!isConnected) {
      console.warn('[WebSocket] Cannot fetch history: not connected');
      return;
    }

    try {
      console.log('[WebSocket] Fetching chat history for room:', currentRoom.room_id);
      
      const message: WebSocketMessage = {
        type: 'message_history',
        room_id: currentRoom.room_id,
      };

      send(message);
    } catch (error) {
      console.error('[WebSocket] Failed to fetch chat history:', error);
      setError('Failed to load chat history');
    }
  }, [currentRoom, isConnected, send, setError]);

  /**
   * Leave current room
   */
  const leaveRoom = useCallback(() => {
    if (!currentRoom || !user) {
      return;
    }

    const message: WebSocketMessage = {
      type: 'user_left',
      room_id: currentRoom.room_id,
      user_id: user.user_id,
      username: user.username,
    };

    send(message);
  }, [currentRoom, user, send]);

  /**
   * Handle incoming WebSocket messages
   */
  const handleWebSocketMessage = useCallback(
    (message: WebSocketMessage) => {
      console.log('[WebSocket] Message received:', message.type, message);

      switch (message.type) {
        case 'message':
          if (message.room_id === currentRoom?.room_id) {
            const chatMessage: Message = {
              id: message.message_id || `${message.user_id}-${message.timestamp}`,
              sender_id: message.user_id || 0,
              sender_name: message.username || 'Unknown',
              content: message.content || '',
              timestamp: message.timestamp || Date.now(),
              room_id: message.room_id || 0,
            };

            addMessage(chatMessage);
          }
          break;

        case 'message_history':
          if (message.messages && Array.isArray(message.messages)) {
            const historyMessages: Message[] = message.messages.map((msg: any) => ({
              id: msg.message_id || msg.id || `${msg.sender_id}-${msg.timestamp}`,
              sender_id: msg.sender_id || msg.user_id || 0,
              sender_name: msg.sender_name || msg.username || 'Unknown',
              content: msg.content || '',
              timestamp: msg.timestamp || 0,
              room_id: msg.room_id || currentRoom?.room_id || 0,
            }));

            // Sort by timestamp
            historyMessages.sort((a, b) => a.timestamp - b.timestamp);
            setMessages(historyMessages);
            console.log('[WebSocket] Chat history loaded:', historyMessages.length, 'messages');
          }
          break;

        case 'user_joined':
          console.log('[WebSocket] User joined room:', message.username);
          // Optional: Show notification or update user count
          break;

        case 'user_left':
          console.log('[WebSocket] User left room:', message.username);
          // Optional: Show notification or update user count
          break;

        case 'error':
          console.error('[WebSocket] Server error:', message.data);
          setError(message.data?.message || 'An error occurred');
          break;

        case 'pong':
          console.log('[WebSocket] Pong received - connection alive');
          break;

        case 'connected':
          console.log('[WebSocket] Connection confirmed by server');
          break;

        default:
          console.warn('[WebSocket] Unknown message type:', message.type);
      }
    },
    [currentRoom, addMessage, setMessages, setError]
  );

  /**
   * Set up ping interval to keep connection alive
   */
  const setPingInterval = useCallback(() => {
    if (pingIntervalRef.current) {
      clearInterval(pingIntervalRef.current);
    }

    pingIntervalRef.current = setInterval(() => {
      if (wsRef.current?.readyState === WebSocket.OPEN) {
        send({ type: 'ping' });
      }
    }, 30000); // Ping every 30 seconds
  }, [send]);

  /**
   * Clear ping interval
   */
  const clearPingInterval = useCallback(() => {
    if (pingIntervalRef.current) {
      clearInterval(pingIntervalRef.current);
      pingIntervalRef.current = null;
    }
  }, []);

  /**
   * Auto-connect on mount and cleanup on unmount
   */
  useEffect(() => {
    if (autoConnect && enabled) {
      connect();
    }

    return () => {
      disconnect();
    };
  }, [autoConnect, enabled, connect, disconnect]);

  /**
   * Fetch chat history when current room changes
   */
  useEffect(() => {
    if (currentRoom && isConnected) {
      // Add a small delay to ensure we're fully joined
      const timer = setTimeout(() => {
        fetchChatHistory();
      }, 100);

      return () => clearTimeout(timer);
    }
  }, [currentRoom, isConnected, fetchChatHistory]);

  return {
    // State
    isConnected,
    connectionError,

    // Methods
    connect,
    disconnect,
    send,
    sendMessage,
    joinRoom,
    leaveRoom,
    fetchChatHistory,
    rejoinRoom,
  };
};
