'use client';

import { useState, useEffect } from 'react';
import { useChatStore } from '@/lib/store';
import { authApi, roomApi } from '@/lib/api';
import ChatPage from '@/components/ChatPage';
import AuthPage from '@/components/AuthPage';

export default function Home() {
  const user = useChatStore((state) => state.user);
  const [isLoading, setIsLoading] = useState(true);

  useEffect(() => {
    // Check if user is already logged in (from localStorage)
    const savedUser = localStorage.getItem('user');
    if (savedUser) {
      try {
        useChatStore.setState({ user: JSON.parse(savedUser) });
      } catch (error) {
        console.error('Error loading saved user:', error);
      }
    }
    setIsLoading(false);
  }, []);

  if (isLoading) {
    return (
      <div className="flex items-center justify-center min-h-screen bg-white dark:bg-black">
        <p className="text-black dark:text-white">Loading...</p>
      </div>
    );
  }

  return (
    <main className="min-h-screen bg-white dark:bg-black text-black dark:text-white">
      {!user ? <AuthPage /> : <ChatPage />}
    </main>
  );
}
