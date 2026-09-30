import { create } from "zustand";
import { getOnlineMessage, getServerEnv } from "@/lib/serverStatus";
import { voiceChatWebSocketUrl } from "@/lib/serverUrl";

interface Peer {
  id: string;
  connection: RTCPeerConnection;
  audioElement?: HTMLAudioElement;
  isMuted: boolean;
}

interface VoiceChatState {
  isConnected: boolean;
  isConnecting: boolean;
  isMuted: boolean;
  roomId: string | null;
  peerId: string | null;
  peers: Map<string, Peer>;
  localStream: MediaStream | null;
  websocket: WebSocket | null;
  error: string | null;
  
  connect: (roomId: string) => Promise<void>;
  disconnect: () => void;
  toggleMute: () => void;
  setError: (error: string | null) => void;
}

const ICE_SERVERS: RTCConfiguration = {
  iceServers: [
    { urls: "stun:stun.l.google.com:19302" },
    { urls: "stun:stun1.l.google.com:19302" },
  ],
};

export const useVoiceChat = create<VoiceChatState>((set, get) => ({
  isConnected: false,
  isConnecting: false,
  isMuted: false,
  roomId: null,
  peerId: null,
  peers: new Map(),
  localStream: null,
  websocket: null,
  error: null,

  connect: async (roomId: string) => {
    const state = get();
    if (state.isConnected || state.isConnecting) return;

    const offline = getOnlineMessage();
    if (offline) {
      set({ isConnecting: false, error: offline });
      return;
    }

    set({ isConnecting: true, error: null });

    try {
      const stream = await navigator.mediaDevices.getUserMedia({
        audio: {
          echoCancellation: true,
          noiseSuppression: true,
          autoGainControl: true,
        },
      });

      const peerId = `peer_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
      
      const wsUrl = voiceChatWebSocketUrl(getServerEnv(), window.location);
      if (!wsUrl) {
        stream.getTracks().forEach((track) => track.stop());
        set({ isConnecting: false, error: getOnlineMessage() });
        return;
      }
      const ws = new WebSocket(wsUrl);

      ws.onopen = () => {
        ws.send(JSON.stringify({
          type: "join",
          roomId,
          peerId,
        }));

        set({
          isConnected: true,
          isConnecting: false,
          localStream: stream,
          websocket: ws,
          roomId,
          peerId,
        });

        console.log("Voice chat connected to room:", roomId);
      };

      ws.onmessage = async (event) => {
        const message = JSON.parse(event.data);
        await handleSignalingMessage(message, get, set);
      };

      ws.onerror = (error) => {
        console.error("Voice chat WebSocket error:", error);
        set({ error: "Connection error", isConnecting: false });
      };

      ws.onclose = () => {
        console.log("Voice chat disconnected");
        cleanup(get, set);
      };

    } catch (error: any) {
      console.error("Failed to connect voice chat:", error);
      set({
        error: error.message || "Failed to access microphone",
        isConnecting: false,
      });
    }
  },

  disconnect: () => {
    const { websocket, roomId, peerId, localStream, peers } = get();

    if (websocket && websocket.readyState === WebSocket.OPEN) {
      websocket.send(JSON.stringify({
        type: "leave",
        roomId,
        peerId,
      }));
      websocket.close();
    }

    cleanup(get, set);
  },

  toggleMute: () => {
    const { localStream, isMuted, websocket, roomId, peerId, peers } = get();

    if (localStream) {
      localStream.getAudioTracks().forEach((track) => {
        track.enabled = isMuted;
      });

      const newMutedState = !isMuted;
      set({ isMuted: newMutedState });

      if (websocket && websocket.readyState === WebSocket.OPEN) {
        peers.forEach((peer) => {
          websocket.send(JSON.stringify({
            type: "mute-status",
            roomId,
            peerId,
            targetPeerId: peer.id,
            payload: { isMuted: newMutedState },
          }));
        });
      }
    }
  },

  setError: (error) => set({ error }),
}));

async function handleSignalingMessage(
  message: any,
  get: () => VoiceChatState,
  set: (state: Partial<VoiceChatState>) => void
) {
  const { localStream, websocket, roomId, peerId, peers } = get();

  switch (message.type) {
    case "room-joined":
      for (const existingPeerId of message.existingPeers) {
        await createPeerConnection(existingPeerId, true, get, set);
      }
      break;

    case "peer-joined":
      await createPeerConnection(message.peerId, false, get, set);
      break;

    case "peer-left":
      const peerToRemove = peers.get(message.peerId);
      if (peerToRemove) {
        peerToRemove.connection.close();
        peerToRemove.audioElement?.remove();
        const newPeers = new Map(peers);
        newPeers.delete(message.peerId);
        set({ peers: newPeers });
      }
      break;

    case "offer":
      await handleOffer(message, get, set);
      break;

    case "answer":
      await handleAnswer(message, get, set);
      break;

    case "ice-candidate":
      await handleIceCandidate(message, get, set);
      break;

    case "mute-status":
      const peer = peers.get(message.peerId);
      if (peer) {
        const newPeers = new Map(peers);
        newPeers.set(message.peerId, { ...peer, isMuted: message.payload.isMuted });
        set({ peers: newPeers });
      }
      break;
  }
}

async function createPeerConnection(
  remotePeerId: string,
  isInitiator: boolean,
  get: () => VoiceChatState,
  set: (state: Partial<VoiceChatState>) => void
) {
  const { localStream, websocket, roomId, peerId, peers } = get();
  if (!localStream || !websocket) return;

  const pc = new RTCPeerConnection(ICE_SERVERS);

  localStream.getTracks().forEach((track) => {
    pc.addTrack(track, localStream);
  });

  pc.onicecandidate = (event) => {
    if (event.candidate) {
      websocket.send(JSON.stringify({
        type: "ice-candidate",
        roomId,
        peerId,
        targetPeerId: remotePeerId,
        payload: { candidate: event.candidate },
      }));
    }
  };

  pc.ontrack = (event) => {
    const audio = document.createElement("audio");
    audio.srcObject = event.streams[0];
    audio.autoplay = true;
    document.body.appendChild(audio);

    const currentPeers = get().peers;
    const existingPeer = currentPeers.get(remotePeerId);
    if (existingPeer) {
      const newPeers = new Map(currentPeers);
      newPeers.set(remotePeerId, { ...existingPeer, audioElement: audio });
      set({ peers: newPeers });
    }
  };

  const newPeer: Peer = {
    id: remotePeerId,
    connection: pc,
    isMuted: false,
  };

  const newPeers = new Map(peers);
  newPeers.set(remotePeerId, newPeer);
  set({ peers: newPeers });

  if (isInitiator) {
    const offer = await pc.createOffer();
    await pc.setLocalDescription(offer);
    websocket.send(JSON.stringify({
      type: "offer",
      roomId,
      peerId,
      targetPeerId: remotePeerId,
      payload: { sdp: pc.localDescription },
    }));
  }
}

async function handleOffer(
  message: any,
  get: () => VoiceChatState,
  set: (state: Partial<VoiceChatState>) => void
) {
  const { peers, websocket, roomId, peerId, localStream } = get();
  let peer = peers.get(message.peerId);

  if (!peer) {
    await createPeerConnection(message.peerId, false, get, set);
    peer = get().peers.get(message.peerId);
  }

  if (!peer || !websocket) return;

  await peer.connection.setRemoteDescription(new RTCSessionDescription(message.payload.sdp));
  const answer = await peer.connection.createAnswer();
  await peer.connection.setLocalDescription(answer);

  websocket.send(JSON.stringify({
    type: "answer",
    roomId,
    peerId,
    targetPeerId: message.peerId,
    payload: { sdp: peer.connection.localDescription },
  }));
}

async function handleAnswer(
  message: any,
  get: () => VoiceChatState,
  set: (state: Partial<VoiceChatState>) => void
) {
  const { peers } = get();
  const peer = peers.get(message.peerId);

  if (peer) {
    await peer.connection.setRemoteDescription(new RTCSessionDescription(message.payload.sdp));
  }
}

async function handleIceCandidate(
  message: any,
  get: () => VoiceChatState,
  set: (state: Partial<VoiceChatState>) => void
) {
  const { peers } = get();
  const peer = peers.get(message.peerId);

  if (peer && message.payload.candidate) {
    try {
      await peer.connection.addIceCandidate(new RTCIceCandidate(message.payload.candidate));
    } catch (error) {
      console.error("Error adding ICE candidate:", error);
    }
  }
}

function cleanup(
  get: () => VoiceChatState,
  set: (state: Partial<VoiceChatState>) => void
) {
  const { localStream, peers } = get();

  if (localStream) {
    localStream.getTracks().forEach((track) => {
      track.enabled = false;
      track.stop();
    });
  }

  peers.forEach((peer) => {
    peer.connection.close();
    peer.audioElement?.remove();
  });

  set({
    isConnected: false,
    isConnecting: false,
    isMuted: false,
    localStream: null,
    websocket: null,
    roomId: null,
    peerId: null,
    peers: new Map(),
  });
}
