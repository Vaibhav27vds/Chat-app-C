'use client';

import { useState, useEffect, useRef } from 'react';
import { useChatStore, Message } from '@/lib/store';
import { useWebSocket } from '@/lib/useWebSocket';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { Send } from 'lucide-react';

export default function ChatWindow() {
  const user = useChatStore((state) => state.user);
  const currentRoom = useChatStore((state) => state.currentRoom);
  const messages = useChatStore((state) => state.messages);
  const [messageText, setMessageText] = useState('');
  const [isLoading, setIsLoading] = useState(true);
  const messagesEndRef = useRef<HTMLDivElement>(null);

  const { sendMessage, isConnected } = useWebSocket(
    currentRoom?.room_id || null,
    user?.user_id || null,
    user?.username || null
  );

  const scrollToBottom = () => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  };

  // Auto-scroll to bottom when new messages arrive
  useEffect(() => {
    scrollToBottom();
  }, [messages]);

  // Update loading state when connected and have messages
  useEffect(() => {
    if (isConnected) {
      setIsLoading(false);
    }
  }, [isConnected]);

  const handleSendMessage = () => {
    if (!messageText.trim() || !isConnected) return;
    sendMessage(messageText);
    setMessageText('');
  };

  const formatTime = (timestamp: number) => {
    const date = new Date(timestamp * 1000 || timestamp);
    return date.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit' });
  };

  if (isLoading) {
    return (
      <div className="flex flex-col h-full bg-white dark:bg-zinc-900 items-center justify-center">
        <p className="text-gray-500 dark:text-gray-400">Connecting to room...</p>
      </div>
    );
  }

  return (
    <div className="flex flex-col h-full bg-white dark:bg-zinc-900">
      {/* Header */}
      <div className="p-4 border-b border-gray-300 dark:border-zinc-800">
        <div className="flex items-center justify-between">
          <div>
            <h2 className="text-xl font-bold text-black dark:text-white">
              {currentRoom?.room_name}
            </h2>
            <p className="text-sm text-gray-600 dark:text-gray-400">
              {currentRoom?.user_count} members
            </p>
          </div>
          <div className="flex items-center gap-2">
            <div
              className={`w-3 h-3 rounded-full ${
                isConnected ? 'bg-green-500' : 'bg-red-500'
              }`}
            />
            <span className="text-xs text-gray-600 dark:text-gray-400">
              {isConnected ? 'Connected' : 'Disconnected'}
            </span>
          </div>
        </div>
      </div>

      {/* Messages */}
      <div className="flex-1 overflow-y-auto p-4 space-y-3">
        {messages.length === 0 ? (
          <div className="flex items-center justify-center h-full">
            <p className="text-gray-500 dark:text-gray-400">No messages yet. Start the conversation!</p>
          </div>
        ) : (
          messages.map((message) => (
            <div
              key={message.id}
              className={`flex ${
                message.sender_id === user?.user_id ? 'justify-end' : 'justify-start'
              }`}
            >
              <div
                className={`max-w-xs lg:max-w-md px-4 py-2 rounded-lg ${
                  message.sender_id === user?.user_id
                    ? 'bg-black dark:bg-white text-white dark:text-black'
                    : 'bg-gray-200 dark:bg-zinc-800 text-black dark:text-white'
                }`}
              >
                <p className="text-xs font-semibold mb-1 opacity-80">
                  {message.sender_name}
                </p>
                <p className="word-wrap">{message.content}</p>
                <p className="text-xs opacity-60 mt-1">
                  {formatTime(message.timestamp)}
                </p>
              </div>
            </div>
          ))
        )}
        <div ref={messagesEndRef} />
      </div>

      {/* Input */}
      <div className="p-4 border-t border-gray-300 dark:border-zinc-800">
        <div className="flex gap-2">
          <Input
            type="text"
            placeholder="Type a message..."
            value={messageText}
            onChange={(e) => setMessageText(e.target.value)}
            onKeyPress={(e) => {
              if (e.key === 'Enter' && !e.shiftKey) {
                e.preventDefault();
                handleSendMessage();
              }
            }}
            disabled={!isConnected}
            className="bg-white dark:bg-zinc-800 border-gray-300 dark:border-zinc-700 text-black dark:text-white placeholder-gray-500 dark:placeholder-gray-400"
          />
          <Button
            onClick={handleSendMessage}
            disabled={!isConnected || !messageText.trim()}
            className="bg-black dark:bg-white text-white dark:text-black hover:bg-gray-800 dark:hover:bg-gray-200"
          >
            <Send className="w-4 h-4" />
          </Button>
        </div>
      </div>
    </div>
  );
}
