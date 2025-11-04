'use client';

import { useState, useEffect } from 'react';
import { useChatStore } from '@/lib/store';
import { roomApi } from '@/lib/api';
import { Button } from '@/components/ui/button';
import RoomsList from './RoomsList';
import ChatWindow from './ChatWindow';

export default function ChatPage() {
  const user = useChatStore((state) => state.user);
  const currentRoom = useChatStore((state) => state.currentRoom);
  const rooms = useChatStore((state) => state.rooms);
  const setRooms = useChatStore((state) => state.setRooms);
  const setCurrentRoom = useChatStore((state) => state.setCurrentRoom);
  const setUser = useChatStore((state) => state.setUser);
  const [isLoadingRooms, setIsLoadingRooms] = useState(true);
  const [showNewRoom, setShowNewRoom] = useState(false);
  const [newRoomName, setNewRoomName] = useState('');
  const [createRoomError, setCreateRoomError] = useState('');

  useEffect(() => {
    loadRooms();
  }, []);

  const loadRooms = async () => {
    try {
      setIsLoadingRooms(true);
      const roomsList = await roomApi.getRooms();
      setRooms(roomsList);
    } catch (error) {
      console.error('Error loading rooms:', error);
    } finally {
      setIsLoadingRooms(false);
    }
  };

  const handleCreateRoom = async () => {
    if (!newRoomName.trim() || !user) return;

    try {
      setCreateRoomError('');
      const response = await roomApi.createRoom(newRoomName, user.user_id);
      if (response.status === 'success') {
        await loadRooms();
        setNewRoomName('');
        setShowNewRoom(false);
      } else {
        setCreateRoomError(response.message || 'Failed to create room');
      }
    } catch (error: any) {
      console.error('Error creating room:', error);
      setCreateRoomError(error.response?.data?.message || error.message || 'Failed to create room');
    }
  };

  const handleLogout = () => {
    setUser(null);
    setCurrentRoom(null);
    localStorage.removeItem('user');
  };

  return (
    <div className="flex h-screen bg-white dark:bg-black">
      {/* Sidebar */}
      <div className="w-64 border-r border-gray-300 dark:border-zinc-800 flex flex-col bg-white dark:bg-zinc-950">
        {/* Header */}
        <div className="p-4 border-b border-gray-300 dark:border-zinc-800">
          <h1 className="text-xl font-bold text-black dark:text-white mb-4">Chat Room</h1>
          <p className="text-sm text-gray-600 dark:text-gray-400 mb-4">
            Welcome, <span className="font-semibold">{user?.username}</span>
          </p>
          <Button
            onClick={handleLogout}
            size="sm"
            className="w-full bg-black dark:bg-white text-white dark:text-black hover:bg-gray-800 dark:hover:bg-gray-200"
          >
            Logout
          </Button>
        </div>

        {/* Rooms Section */}
        <div className="flex-1 overflow-y-auto p-4">
          <div className="flex items-center justify-between mb-4">
            <h2 className="font-semibold text-black dark:text-white text-sm">Rooms</h2>
            <Button
              size="sm"
              onClick={() => setShowNewRoom(!showNewRoom)}
              variant="outline"
              className="border-black dark:border-white text-black dark:text-white h-8 w-8 p-0"
            >
              +
            </Button>
          </div>

          {showNewRoom && (
            <div className="mb-4 p-3 border border-gray-300 dark:border-zinc-700 rounded bg-gray-50 dark:bg-zinc-900">
              <input
                type="text"
                placeholder="Room name..."
                value={newRoomName}
                onChange={(e) => setNewRoomName(e.target.value)}
                className="w-full px-2 py-1 mb-2 text-sm bg-white dark:bg-zinc-800 border border-gray-300 dark:border-zinc-600 text-black dark:text-white rounded"
              />
              {createRoomError && (
                <div className="mb-2 p-2 bg-red-50 dark:bg-red-950 border border-red-200 dark:border-red-800 rounded text-red-700 dark:text-red-300 text-xs">
                  {createRoomError}
                </div>
              )}
              <div className="flex gap-2">
                <Button
                  size="sm"
                  onClick={handleCreateRoom}
                  className="flex-1 bg-black dark:bg-white text-white dark:text-black hover:bg-gray-800 dark:hover:bg-gray-200 text-xs"
                >
                  Create
                </Button>
                <Button
                  size="sm"
                  onClick={() => setShowNewRoom(false)}
                  variant="outline"
                  className="flex-1 border-black dark:border-white text-black dark:text-white text-xs"
                >
                  Cancel
                </Button>
              </div>
            </div>
          )}

          {isLoadingRooms ? (
            <p className="text-xs text-gray-500 dark:text-gray-400">Loading rooms...</p>
          ) : (
            <RoomsList rooms={rooms} currentRoom={currentRoom} />
          )}
        </div>
      </div>

      {/* Main Chat Area */}
      <div className="flex-1 flex flex-col">
        {currentRoom ? (
          <ChatWindow />
        ) : (
          <div className="flex-1 flex items-center justify-center bg-gray-50 dark:bg-zinc-900">
            <div className="text-center">
              <h2 className="text-2xl font-bold text-black dark:text-white mb-2">
                Welcome to Chat Room
              </h2>
              <p className="text-gray-600 dark:text-gray-400">
                Select a room to start chatting or create a new one
              </p>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
