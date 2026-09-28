import { WebSocketServer, WebSocket } from "ws";
import type { Server } from "http";

interface VoiceChatRoom {
  id: string;
  participants: Map<string, WebSocket>;
}

interface SignalingMessage {
  type: "join" | "leave" | "offer" | "answer" | "ice-candidate" | "mute-status";
  roomId: string;
  peerId: string;
  targetPeerId?: string;
  payload?: any;
}

const rooms: Map<string, VoiceChatRoom> = new Map();

export function setupVoiceChatSignaling(server: Server) {
  const wss = new WebSocketServer({ server, path: "/voice-chat" });

  wss.on("connection", (ws: WebSocket) => {
    let currentRoomId: string | null = null;
    let currentPeerId: string | null = null;

    ws.on("message", (data: Buffer) => {
      try {
        const message: SignalingMessage = JSON.parse(data.toString());

        switch (message.type) {
          case "join":
            handleJoin(ws, message);
            currentRoomId = message.roomId;
            currentPeerId = message.peerId;
            break;

          case "leave":
            handleLeave(message);
            currentRoomId = null;
            currentPeerId = null;
            break;

          case "offer":
          case "answer":
          case "ice-candidate":
          case "mute-status":
            relayMessage(message);
            break;
        }
      } catch (error) {
        console.error("Voice chat signaling error:", error);
      }
    });

    ws.on("close", () => {
      if (currentRoomId && currentPeerId) {
        handleLeave({ type: "leave", roomId: currentRoomId, peerId: currentPeerId });
      }
    });

    ws.on("error", (error) => {
      console.error("WebSocket error:", error);
    });
  });

  console.log("Voice chat signaling server initialized");
}

function handleJoin(ws: WebSocket, message: SignalingMessage) {
  const { roomId, peerId } = message;

  if (!rooms.has(roomId)) {
    rooms.set(roomId, { id: roomId, participants: new Map() });
  }

  const room = rooms.get(roomId)!;
  
  const existingPeerIds = Array.from(room.participants.keys());
  
  room.participants.set(peerId, ws);

  existingPeerIds.forEach((existingPeerId) => {
    const existingPeerWs = room.participants.get(existingPeerId);
    if (existingPeerWs && existingPeerWs.readyState === WebSocket.OPEN) {
      existingPeerWs.send(JSON.stringify({
        type: "peer-joined",
        peerId: peerId,
        roomId: roomId
      }));
    }
  });

  ws.send(JSON.stringify({
    type: "room-joined",
    roomId: roomId,
    peerId: peerId,
    existingPeers: existingPeerIds
  }));

  console.log(`Peer ${peerId} joined room ${roomId}. Total: ${room.participants.size}`);
}

function handleLeave(message: SignalingMessage) {
  const { roomId, peerId } = message;
  const room = rooms.get(roomId);

  if (!room) return;

  room.participants.delete(peerId);

  room.participants.forEach((ws, existingPeerId) => {
    if (ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({
        type: "peer-left",
        peerId: peerId,
        roomId: roomId
      }));
    }
  });

  if (room.participants.size === 0) {
    rooms.delete(roomId);
  }

  console.log(`Peer ${peerId} left room ${roomId}. Remaining: ${room.participants.size}`);
}

function relayMessage(message: SignalingMessage) {
  const { roomId, targetPeerId } = message;
  const room = rooms.get(roomId);

  if (!room || !targetPeerId) return;

  const targetWs = room.participants.get(targetPeerId);
  if (targetWs && targetWs.readyState === WebSocket.OPEN) {
    targetWs.send(JSON.stringify(message));
  }
}
