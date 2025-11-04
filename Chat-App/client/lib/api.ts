import axios from 'axios';

const API_BASE_URL = process.env.NEXT_PUBLIC_API_URL || 'http://localhost:3005/api';
const WS_URL = process.env.NEXT_PUBLIC_WS_URL || 'ws://localhost:7070';

const apiRequest = async (endpoint: string, method: string = 'GET', data?: any) => {
  const url = `${API_BASE_URL}${endpoint}`;
  
  const options: RequestInit = {
    method,
    headers: {
      'Content-Type': 'application/json',
    },
  };

  if (data && (method === 'POST' || method === 'PUT')) {
    options.body = JSON.stringify(data);
    console.log(`[API] ${method} ${endpoint}`, {
      url: url,
      headers: options.headers,
      body: options.body,
      data: data,
    });
  }

  try {
    const response = await fetch(url, options);
    const responseText = await response.text();
    
    let result;
    try {
      result = JSON.parse(responseText);
    } catch (e) {
      result = { message: responseText };
    }

    console.log(`[API] Response [${response.status}]:`, {
      status: response.status,
      headers: Object.fromEntries(response.headers.entries()),
      body: result,
      rawText: responseText.substring(0, 200),
    });

    if (!response.ok) {
      const errorMessage = result.message || `HTTP ${response.status}: ${responseText}`;
      console.error(`API Error [${response.status}]:`, result, errorMessage);
      throw new Error(errorMessage);
    }

    return result;
  } catch (error) {
    console.error(`[API] Request failed for ${method} ${endpoint}:`, error);
    throw error;
  }
};

export const authApi = {
  register: async (username: string, password: string, role: string = 'user') => {
    return apiRequest('/register', 'POST', { username, password, role });
  },

  login: async (username: string, password: string) => {
    return apiRequest('/login', 'POST', { username, password });
  },
};

export const roomApi = {
  getRooms: async () => {
    const result = await apiRequest('/rooms', 'GET');
    return result.rooms || [];
  },

  createRoom: async (roomName: string, userId: number) => {
    return apiRequest('/rooms/create', 'POST', {
      room_name: roomName,
      user_id: userId,
    });
  },

  joinRoom: async (roomId: number, userId: number) => {
    try {
      return await apiRequest(`/rooms/${roomId}/join`, 'POST', {
        user_id: userId,
      });
    } catch (error) {
      const errorMsg = error instanceof Error ? error.message : String(error);
      
      // Parse backend error codes for user-friendly messages
      if (errorMsg.includes('error_code') || errorMsg.includes('-1')) {
        throw new Error('This room is full - please try another room');
      } else if (errorMsg.includes('error_code') || errorMsg.includes('-2')) {
        throw new Error('You are already a member of this room');
      } else if (errorMsg.includes('error_code') || errorMsg.includes('-3')) {
        throw new Error('This room no longer exists - please refresh to see available rooms');
      } else if (errorMsg.includes('Invalid room ID')) {
        throw new Error('Invalid room ID - please try again');
      } else if (errorMsg.includes('Missing user_id')) {
        throw new Error('Invalid request - missing user information');
      }
      
      // Re-throw original error if not matched
      throw error;
    }
  },

  getRoomUsers: async (roomId: number) => {
    const result = await apiRequest(`/rooms/${roomId}/users`, 'GET');
    return result.users || [];
  },
};

export const getWebSocketURL = () => WS_URL;
