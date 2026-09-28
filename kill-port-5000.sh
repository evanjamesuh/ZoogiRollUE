#!/bin/sh
# Kill whatever process is holding port 5000 using /proc/net/tcp
PORT_HEX="1388"
INODE=$(awk -v port=":$PORT_HEX" '$2 ~ port {print $10}' /proc/net/tcp 2>/dev/null | head -1)
if [ -n "$INODE" ]; then
  PID=$(grep -r "socket:\[$INODE\]" /proc/[0-9]*/fd 2>/dev/null | grep -oP '/proc/\K[0-9]+' | head -1)
  if [ -n "$PID" ]; then
    echo "Killing PID $PID holding port 5000"
    kill -9 $PID 2>/dev/null
    sleep 2
  fi
else
  echo "Port 5000 is free"
fi
