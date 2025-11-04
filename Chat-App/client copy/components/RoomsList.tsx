'use client';

import { useState } from 'react';
import { useChatStore, ChatRoom } from '@/lib/store';
import { roomApi } from '@/lib/api';
import { Button } from '@/components/ui/button';

interface RoomsListProps {
  rooms: ChatRoom[];
  currentRoom: ChatRoom | null;
}

export default function RoomsList({ rooms, currentRoom }: RoomsListProps) {
  const user = useChatStore((state) => state.user);
  const setCurrentRoom = useChatStore((state) => state.setCurrentRoom);
  const clearMessages = useChatStore((state) => state.clearMessages);
  const addUserRoomMembership = useChatStore((state) => state.addUserRoomMembership);
  const [error, setError] = useState<string | null>(null);
  const [isJoining, setIsJoining] = useState<number | null>(null);

  const handleSelectRoom = async (room: ChatRoom) => {
    if (!user) return;

    try {
      setError(null);
      setIsJoining(room.room_id);
      
      console.log('[RoomsList] Attempting to join room:', {
        roomId: room.room_id,
        userId: user.user_id,
        username: user.username,
      });

      // Join room
      const response = await roomApi.joinRoom(room.room_id, user.user_id);
      
      console.log('[RoomsList] Join room response:', response);
      
      // Track that user is now a member of this room
      addUserRoomMembership(room.room_id);
      
      // Clear old messages and set new room
      clearMessages();
      setCurrentRoom(room);
      setIsJoining(null);
    } catch (error: any) {
      console.error('[RoomsList] Error joining room:', error);
      setError(error.message || 'Failed to join room');
      setIsJoining(null);
    }
  };

  return (
    <div className="space-y-2">
      {error && (
        <div className="p-2 bg-red-50 dark:bg-red-950 border border-red-200 dark:border-red-800 rounded text-red-700 dark:text-red-300 text-xs">
          {error}
          <button
            onClick={() => setError(null)}
            className="ml-2 font-semibold hover:underline"
          >
            ✕
          </button>
        </div>
      )}
      {rooms.length === 0 ? (
        <p className="text-xs text-gray-500 dark:text-gray-400">No rooms available</p>
      ) : (
        rooms.map((room) => (
          <Button
            key={room.room_id}
            onClick={() => handleSelectRoom(room)}
            disabled={isJoining === room.room_id}
            variant={currentRoom?.room_id === room.room_id ? 'default' : 'outline'}
            className={`w-full justify-start text-left h-auto py-2 px-3 text-sm truncate ${
              currentRoom?.room_id === room.room_id
                ? 'bg-black dark:bg-white text-white dark:text-black'
                : 'border-gray-300 dark:border-zinc-700 text-black dark:text-white hover:bg-gray-100 dark:hover:bg-zinc-800'
            }`}
          >
            <div className="flex-1 min-w-0">
              <p className="font-medium truncate">
                {isJoining === room.room_id ? '...' : room.room_name}
              </p>
              <p className="text-xs opacity-70">{room.user_count} members</p>
            </div>
          </Button>
        ))
      )}
    </div>
  );
}
