import type { Metadata } from "next";
import "./globals.css";

export const metadata: Metadata = {
  title: "Chat App",
  description: "Minimalist Black and White Chat Application",
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="en">
      <body className="antialiased bg-white text-black dark:bg-black dark:text-white">
        {children}
      </body>
    </html>
  );
}
