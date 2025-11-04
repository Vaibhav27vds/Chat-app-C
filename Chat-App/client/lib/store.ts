import { create } from 'zustand';

export interface User {
  user_id: number;
  username: string;
  role: 'user' | 'admin';
  token: string;
}

export interface ChatRoom {
  room_id: number;
  room_name: string;
  user_count: number;
  created_by?: number;
}

export interface Message {
  id: string;
  sender_id: number;
  sender_name: string;
  content: string;
  timestamp: number;
  room_id: number;
}

interface ChatStore {
  user: User | null;
  rooms: ChatRoom[];
  currentRoom: ChatRoom | null;
  messages: Message[];
  isLoading: boolean;
  error: string | null;
  userRoomMemberships: Set<number>; // Track which rooms the user is a member of
  
  // User actions
  setUser: (user: User | null) => void;
  setError: (error: string | null) => void;
  setLoading: (loading: boolean) => void;
  
  // Room actions
  setRooms: (rooms: ChatRoom[]) => void;
  setCurrentRoom: (room: ChatRoom | null) => void;
  addRoom: (room: ChatRoom) => void;
  addUserRoomMembership: (roomId: number) => void;
  isUserInRoom: (roomId: number) => boolean;
  
  // Message actions
  setMessages: (messages: Message[]) => void;
  addMessage: (message: Message) => void;
  clearMessages: () => void;
}

export const useChatStore = create<ChatStore>((set, get) => ({
  user: null,
  rooms: [],
  currentRoom: null,
  messages: [],
  isLoading: false,
  error: null,
  userRoomMemberships: new Set(),
  
  setUser: (user) => set({ user }),
  setError: (error) => set({ error }),
  setLoading: (loading) => set({ isLoading: loading }),
  
  setRooms: (rooms) => set({ rooms }),
  setCurrentRoom: (room) => set({ currentRoom: room }),
  addRoom: (room) => set((state) => ({ rooms: [...state.rooms, room] })),
  
  addUserRoomMembership: (roomId) => {
    set((state) => {
      const newMemberships = new Set(state.userRoomMemberships);
      newMemberships.add(roomId);
      return { userRoomMemberships: newMemberships };
    });
  },
  
  isUserInRoom: (roomId) => {
    return get().userRoomMemberships.has(roomId);
  },
  
  setMessages: (messages) => set({ messages }),
  addMessage: (message) => set((state) => ({
    messages: [
      ...state.messages.filter(m => m.id !== message.id),
      message,
    ].sort((a, b) => a.timestamp - b.timestamp),
  })),
  clearMessages: () => set({ messages: [] }),
}));
