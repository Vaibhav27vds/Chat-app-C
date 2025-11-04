'use client';

import { useState } from 'react';
import { useChatStore } from '@/lib/store';
import { authApi } from '@/lib/api';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';

export default function AuthPage() {
  const [isLogin, setIsLogin] = useState(true);
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [error, setError] = useState('');
  const [isLoading, setIsLoading] = useState(false);
  const setUser = useChatStore((state) => state.setUser);

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setError('');
    setIsLoading(true);

    try {
      let response;

      if (isLogin) {
        response = await authApi.login(username, password);
      } else {
        response = await authApi.register(username, password, 'user');
      }

      if (response.status === 'success') {
        const userData = {
          user_id: response.user_id,
          username: response.username,
          role: response.role as 'user' | 'admin',
          token: response.token || '',
        };
        
        setUser(userData);
        localStorage.setItem('user', JSON.stringify(userData));
      } else {
        setError(response.message || 'Authentication failed');
      }
    } catch (err: any) {
      setError(err.response?.data?.message || err.message || 'An error occurred');
    } finally {
      setIsLoading(false);
    }
  };

  return (
    <div className="flex items-center justify-center min-h-screen bg-white dark:bg-black p-4">
      <Card className="w-full max-w-md bg-white dark:bg-zinc-950 border-black dark:border-zinc-800">
        <CardHeader className="text-center">
          <CardTitle className="text-2xl font-bold text-black dark:text-white">
            Chat Room
          </CardTitle>
          <CardDescription className="text-gray-600 dark:text-gray-400">
            {isLogin ? 'Login to continue' : 'Create your account'}
          </CardDescription>
        </CardHeader>
        <CardContent>
          <form onSubmit={handleSubmit} className="space-y-4">
            <div className="space-y-2">
              <label className="text-sm font-medium text-black dark:text-white">
                Username
              </label>
              <Input
                type="text"
                placeholder="Enter your username"
                value={username}
                onChange={(e) => setUsername(e.target.value)}
                required
                className="bg-white dark:bg-zinc-900 border-black dark:border-zinc-700 text-black dark:text-white placeholder-gray-500 dark:placeholder-gray-400"
              />
            </div>

            <div className="space-y-2">
              <label className="text-sm font-medium text-black dark:text-white">
                Password
              </label>
              <Input
                type="password"
                placeholder="Enter your password"
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                required
                className="bg-white dark:bg-zinc-900 border-black dark:border-zinc-700 text-black dark:text-white placeholder-gray-500 dark:placeholder-gray-400"
              />
            </div>

            {error && (
              <div className="p-3 bg-red-50 dark:bg-red-950 border border-red-200 dark:border-red-800 rounded text-red-700 dark:text-red-300 text-sm">
                {error}
              </div>
            )}

            <Button
              type="submit"
              disabled={isLoading}
              className="w-full bg-black dark:bg-white text-white dark:text-black hover:bg-gray-800 dark:hover:bg-gray-200"
            >
              {isLoading ? 'Loading...' : isLogin ? 'Login' : 'Register'}
            </Button>
          </form>

          <div className="mt-4 text-center">
            <button
              type="button"
              onClick={() => {
                setIsLogin(!isLogin);
                setError('');
              }}
              className="text-sm text-gray-600 dark:text-gray-400 hover:text-black dark:hover:text-white"
            >
              {isLogin ? "Don't have an account? Register" : 'Already have an account? Login'}
            </button>
          </div>
        </CardContent>
      </Card>
    </div>
  );
}
