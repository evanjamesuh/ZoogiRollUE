import type { Express, Request, Response, NextFunction } from "express";
import { createServer, type Server } from "http";
import { storage } from "./storage";
import { setupVoiceChatSignaling } from "./voiceChatSignaling";
import * as meshyService from "./meshyService";
import bcrypt from "bcryptjs";
import { registerUserSchema, loginUserSchema } from "@shared/schema";

declare module "express-session" {
  interface SessionData {
    userId?: number;
  }
}

function requireAuth(req: Request, res: Response, next: NextFunction) {
  if (!req.session?.userId) {
    return res.status(401).json({ error: "Not authenticated" });
  }
  next();
}

export async function registerRoutes(
  httpServer: Server,
  app: Express
): Promise<Server> {
  setupVoiceChatSignaling(httpServer);

  app.post("/api/auth/register", async (req, res) => {
    try {
      const validation = registerUserSchema.safeParse(req.body);
      if (!validation.success) {
        return res.status(400).json({ error: validation.error.errors[0].message });
      }
      
      const { username, password, displayName } = validation.data;
      
      const existingUser = await storage.getUserByUsername(username);
      if (existingUser) {
        return res.status(400).json({ error: "Username already taken" });
      }

      const hashedPassword = await bcrypt.hash(password, 10);
      const newUser = await storage.createUser({
        username,
        password: hashedPassword,
      });
      
      let finalUser = newUser;
      if (displayName) {
        const updated = await storage.updateUser(newUser.id, { displayName });
        if (updated) finalUser = updated;
      }

      req.session.userId = finalUser.id;
      
      const { password: _pw, ...safeUser } = finalUser;
      res.json({ user: safeUser });
    } catch (error) {
      console.error("Registration error:", error);
      res.status(500).json({ error: "Failed to register" });
    }
  });

  app.post("/api/auth/login", async (req, res) => {
    try {
      const validation = loginUserSchema.safeParse(req.body);
      if (!validation.success) {
        return res.status(400).json({ error: "Invalid credentials" });
      }
      
      const { username, password } = validation.data;
      const user = await storage.getUserByUsername(username);
      
      if (!user || !(await bcrypt.compare(password, user.password))) {
        return res.status(401).json({ error: "Invalid username or password" });
      }

      const now = new Date();
      const lastLogin = user.lastLoginAt;
      let loginStreak = user.loginStreak;
      
      if (lastLogin) {
        const daysSinceLastLogin = Math.floor((now.getTime() - lastLogin.getTime()) / (1000 * 60 * 60 * 24));
        if (daysSinceLastLogin === 1) {
          loginStreak += 1;
        } else if (daysSinceLastLogin > 1) {
          loginStreak = 1;
        }
      } else {
        loginStreak = 1;
      }

      await storage.updateUser(user.id, { 
        lastLoginAt: now,
        loginStreak 
      });

      req.session.userId = user.id;
      
      const { password: _, ...safeUser } = user;
      res.json({ user: { ...safeUser, loginStreak } });
    } catch (error) {
      console.error("Login error:", error);
      res.status(500).json({ error: "Failed to login" });
    }
  });

  app.post("/api/auth/logout", (req, res) => {
    req.session.destroy((err) => {
      if (err) {
        return res.status(500).json({ error: "Failed to logout" });
      }
      res.json({ success: true });
    });
  });

  app.get("/api/auth/me", async (req, res) => {
    try {
      if (!req.session?.userId) {
        return res.json({ user: null, sessionInfo: null });
      }
      
      const user = await storage.getUser(req.session.userId);
      if (!user) {
        return res.json({ user: null, sessionInfo: null });
      }
      
      const { password: _, ...safeUser } = user;
      
      const maxAgeMs = req.session.cookie.maxAge || 30 * 24 * 60 * 60 * 1000;
      const expiresAt = new Date(Date.now() + maxAgeMs);
      
      const sessionInfo = {
        expiresAt: expiresAt.toISOString(),
        maxAge: maxAgeMs,
        daysRemaining: Math.ceil(maxAgeMs / (1000 * 60 * 60 * 24)),
      };
      
      res.json({ user: safeUser, sessionInfo });
    } catch (error) {
      console.error("Get user error:", error);
      res.status(500).json({ error: "Failed to get user" });
    }
  });

  app.get("/api/users/search", requireAuth, async (req, res) => {
    try {
      const query = req.query.q as string;
      if (!query || query.length < 2) {
        return res.json({ users: [] });
      }
      
      const users = await storage.searchUsers(query);
      const safeUsers = users.map(({ password: _, ...u }) => u);
      res.json({ users: safeUsers });
    } catch (error) {
      console.error("Search users error:", error);
      res.status(500).json({ error: "Failed to search users" });
    }
  });

  app.get("/api/users/:id", async (req, res) => {
    try {
      const userId = parseInt(req.params.id);
      const user = await storage.getUser(userId);
      if (!user) {
        return res.status(404).json({ error: "User not found" });
      }
      const { password: _, ...safeUser } = user;
      res.json({ user: safeUser });
    } catch (error) {
      console.error("Get user error:", error);
      res.status(500).json({ error: "Failed to get user" });
    }
  });

  app.patch("/api/users/profile", requireAuth, async (req, res) => {
    try {
      const { displayName, avatarUrl, favoriteZoogi } = req.body;
      const updates: any = {};
      if (displayName !== undefined) updates.displayName = displayName;
      if (avatarUrl !== undefined) updates.avatarUrl = avatarUrl;
      if (favoriteZoogi !== undefined) updates.favoriteZoogi = favoriteZoogi;
      
      const user = await storage.updateUser(req.session.userId!, updates);
      if (!user) {
        return res.status(404).json({ error: "User not found" });
      }
      
      const { password: _, ...safeUser } = user;
      res.json({ user: safeUser });
    } catch (error) {
      console.error("Update profile error:", error);
      res.status(500).json({ error: "Failed to update profile" });
    }
  });

  app.get("/api/friends", requireAuth, async (req, res) => {
    try {
      const friendships = await storage.getFriendships(req.session.userId!);
      res.json({ friendships });
    } catch (error) {
      console.error("Get friends error:", error);
      res.status(500).json({ error: "Failed to get friends" });
    }
  });

  app.post("/api/friends/request", requireAuth, async (req, res) => {
    try {
      const { friendId } = req.body;
      if (!friendId || friendId === req.session.userId) {
        return res.status(400).json({ error: "Invalid friend request" });
      }
      
      const existingFriendships = await storage.getFriendships(req.session.userId!);
      const alreadyExists = existingFriendships.some(f => 
        (f.userId === req.session.userId && f.friendId === friendId) ||
        (f.userId === friendId && f.friendId === req.session.userId)
      );
      
      if (alreadyExists) {
        return res.status(400).json({ error: "Friend request already exists" });
      }
      
      const friendship = await storage.createFriendRequest(req.session.userId!, friendId);
      res.json({ friendship });
    } catch (error) {
      console.error("Friend request error:", error);
      res.status(500).json({ error: "Failed to send friend request" });
    }
  });

  app.patch("/api/friends/:id", requireAuth, async (req, res) => {
    try {
      const { status } = req.body;
      if (!["accepted", "rejected"].includes(status)) {
        return res.status(400).json({ error: "Invalid status" });
      }
      
      const friendship = await storage.updateFriendshipStatus(parseInt(req.params.id), status);
      res.json({ friendship });
    } catch (error) {
      console.error("Update friendship error:", error);
      res.status(500).json({ error: "Failed to update friendship" });
    }
  });

  app.get("/api/chat/:channel", requireAuth, async (req, res) => {
    try {
      const messages = await storage.getChatMessages(req.params.channel);
      res.json({ messages: messages.reverse() });
    } catch (error) {
      console.error("Get chat error:", error);
      res.status(500).json({ error: "Failed to get messages" });
    }
  });

  app.post("/api/chat", requireAuth, async (req, res) => {
    try {
      const { message, channel, receiverId } = req.body;
      if (!message) {
        return res.status(400).json({ error: "Message required" });
      }
      
      const msg = await storage.createChatMessage(
        req.session.userId!,
        message,
        channel || "global",
        receiverId
      );
      res.json({ message: msg });
    } catch (error) {
      console.error("Send message error:", error);
      res.status(500).json({ error: "Failed to send message" });
    }
  });

  app.get("/api/chat/direct/:friendId", requireAuth, async (req, res) => {
    try {
      const messages = await storage.getDirectMessages(
        req.session.userId!,
        parseInt(req.params.friendId)
      );
      res.json({ messages: messages.reverse() });
    } catch (error) {
      console.error("Get DMs error:", error);
      res.status(500).json({ error: "Failed to get messages" });
    }
  });

  app.get("/api/achievements", requireAuth, async (req, res) => {
    try {
      const achievements = await storage.getUserAchievements(req.session.userId!);
      res.json({ achievements });
    } catch (error) {
      console.error("Get achievements error:", error);
      res.status(500).json({ error: "Failed to get achievements" });
    }
  });

  app.get("/api/leaderboard", async (req, res) => {
    try {
      const limit = parseInt(req.query.limit as string) || 10;
      const entries = await storage.getLeaderboard(limit);
      res.json(entries);
    } catch (error) {
      console.error("Error fetching leaderboard:", error);
      res.status(500).json({ error: "Failed to fetch leaderboard" });
    }
  });
  
  app.post("/api/leaderboard", async (req, res) => {
    try {
      const { playerName, score, zoogiUsed } = req.body;
      if (!playerName || score === undefined || !zoogiUsed) {
        return res.status(400).json({ error: "Missing required fields" });
      }
      const entry = await storage.addLeaderboardEntry({ playerName, score, zoogiUsed });
      res.json(entry);
    } catch (error) {
      console.error("Error adding leaderboard entry:", error);
      res.status(500).json({ error: "Failed to add leaderboard entry" });
    }
  });

  app.get("/api/purchases", async (req, res) => {
    try {
      const deviceId = req.query.deviceId as string;
      if (!deviceId) {
        return res.status(400).json({ error: "Missing deviceId" });
      }
      const purchases = await storage.getCompletedPurchasesByDevice(deviceId);
      res.json({ data: purchases });
    } catch (error) {
      console.error("Error fetching purchases:", error);
      res.status(500).json({ error: "Failed to fetch purchases" });
    }
  });

  app.post("/api/meshy/image-to-3d", async (req, res) => {
    try {
      const { imageDataUrl, imageUrl, surfaceMode } = req.body;
      
      console.log("Meshy API request - imageDataUrl length:", imageDataUrl?.length || 0);
      
      if (!imageDataUrl && !imageUrl) {
        return res.status(400).json({ error: "Either imageDataUrl or imageUrl is required" });
      }

      const result = await meshyService.createImageTo3DTask({
        image_data_url: imageDataUrl,
        image_url: imageUrl,
        surface_mode: surfaceMode || "organic",
        enable_pbr: true,
        should_remesh: true,
        topology: "triangle",
        target_polycount: 30000,
      });

      console.log("Meshy API response:", JSON.stringify(result));
      res.json({ taskId: result.result });
    } catch (error: any) {
      console.error("Error creating Meshy task:", error.message);
      if (error.message?.includes("MESHY_API_KEY")) {
        return res.status(503).json({ error: "Meshy API not configured" });
      }
      res.status(500).json({ error: error.message || "Failed to create 3D model task" });
    }
  });

  app.get("/api/meshy/task/:taskId", async (req, res) => {
    try {
      const { taskId } = req.params;
      const task = await meshyService.getImageTo3DTask(taskId);
      res.json(task);
    } catch (error: any) {
      console.error("Error fetching Meshy task:", error);
      if (error.message?.includes("MESHY_API_KEY")) {
        return res.status(503).json({ error: "Meshy API not configured" });
      }
      res.status(500).json({ error: "Failed to fetch task status" });
    }
  });

  app.get("/api/meshy/tasks", async (req, res) => {
    try {
      const pageNum = parseInt(req.query.pageNum as string) || 1;
      const pageSize = parseInt(req.query.pageSize as string) || 10;
      const tasks = await meshyService.listImageTo3DTasks(pageNum, pageSize);
      res.json({ data: tasks });
    } catch (error: any) {
      console.error("Error listing Meshy tasks:", error);
      if (error.message?.includes("MESHY_API_KEY")) {
        return res.status(503).json({ error: "Meshy API not configured" });
      }
      res.status(500).json({ error: "Failed to list tasks" });
    }
  });

  app.get("/api/meshy/download/:taskId", async (req, res) => {
    try {
      const { taskId } = req.params;
      const task = await meshyService.getImageTo3DTask(taskId);
      
      if (task.status !== "SUCCEEDED" || !task.model_urls?.glb) {
        return res.status(400).json({ error: "Model not ready for download" });
      }

      const modelBuffer = await meshyService.downloadModel(task.model_urls.glb);
      
      res.setHeader("Content-Type", "model/gltf-binary");
      res.setHeader("Access-Control-Allow-Origin", "*");
      res.send(Buffer.from(modelBuffer));
    } catch (error: any) {
      console.error("Error downloading model:", error);
      res.status(500).json({ error: "Failed to download model" });
    }
  });

  app.get("/api/custom-zoogis", async (req, res) => {
    try {
      const deviceId = req.query.deviceId as string;
      if (!deviceId) {
        return res.status(400).json({ error: "Missing deviceId" });
      }
      const zoogis = await storage.getCustomZoogisByDevice(deviceId);
      res.json({ data: zoogis });
    } catch (error) {
      console.error("Error fetching custom zoogis:", error);
      res.status(500).json({ error: "Failed to fetch custom zoogis" });
    }
  });

  app.post("/api/custom-zoogis", async (req, res) => {
    try {
      const { deviceId, name, meshyTaskId, thumbnailUrl, stats } = req.body;
      if (!deviceId || !name || !meshyTaskId || !stats) {
        return res.status(400).json({ error: "Missing required fields" });
      }
      const zoogi = await storage.createCustomZoogi({
        deviceId,
        name,
        meshyTaskId,
        thumbnailUrl: thumbnailUrl || null,
        stats,
      });
      res.json(zoogi);
    } catch (error) {
      console.error("Error creating custom zoogi:", error);
      res.status(500).json({ error: "Failed to save custom zoogi" });
    }
  });

  app.delete("/api/custom-zoogis/:id", async (req, res) => {
    try {
      const id = parseInt(req.params.id);
      const deviceId = req.query.deviceId as string;
      if (!deviceId) {
        return res.status(400).json({ error: "Missing deviceId" });
      }
      const deleted = await storage.deleteCustomZoogi(id, deviceId);
      if (deleted) {
        res.json({ success: true });
      } else {
        res.status(404).json({ error: "Zoogi not found" });
      }
    } catch (error) {
      console.error("Error deleting custom zoogi:", error);
      res.status(500).json({ error: "Failed to delete custom zoogi" });
    }
  });

  // Custom Arenas (Meshy-generated 3D arenas)
  app.get("/api/custom-arenas", async (req, res) => {
    try {
      const deviceId = req.query.deviceId as string;
      if (!deviceId) {
        return res.status(400).json({ error: "Missing deviceId" });
      }
      const arenas = await storage.getCustomArenasByDevice(deviceId);
      res.json({ data: arenas });
    } catch (error) {
      console.error("Error fetching custom arenas:", error);
      res.status(500).json({ error: "Failed to fetch custom arenas" });
    }
  });

  app.post("/api/custom-arenas", async (req, res) => {
    try {
      const { deviceId, name, description, meshyTaskId, thumbnailUrl } = req.body;
      if (!deviceId || !name || !meshyTaskId) {
        return res.status(400).json({ error: "Missing required fields" });
      }
      const arena = await storage.createCustomArena({
        deviceId,
        name,
        description: description || null,
        meshyTaskId,
        modelType: "meshy",
        thumbnailUrl: thumbnailUrl || null,
      });
      res.json(arena);
    } catch (error) {
      console.error("Error creating custom arena:", error);
      res.status(500).json({ error: "Failed to save custom arena" });
    }
  });

  app.delete("/api/custom-arenas/:id", async (req, res) => {
    try {
      const id = parseInt(req.params.id);
      const deviceId = req.query.deviceId as string;
      if (!deviceId) {
        return res.status(400).json({ error: "Missing deviceId" });
      }
      const deleted = await storage.deleteCustomArena(id, deviceId);
      if (deleted) {
        res.json({ success: true });
      } else {
        res.status(404).json({ error: "Arena not found" });
      }
    } catch (error) {
      console.error("Error deleting custom arena:", error);
      res.status(500).json({ error: "Failed to delete custom arena" });
    }
  });

  // Gallery - Public Zoogis
  app.get("/api/gallery/zoogis", async (req, res) => {
    try {
      const limit = parseInt(req.query.limit as string) || 50;
      const zoogis = await storage.getPublicZoogis(limit);
      res.json({ data: zoogis });
    } catch (error) {
      console.error("Error fetching gallery:", error);
      res.status(500).json({ error: "Failed to fetch gallery" });
    }
  });

  app.post("/api/gallery/zoogis/:id/share", requireAuth, async (req, res) => {
    try {
      const id = parseInt(req.params.id);
      const zoogi = await storage.updateZoogiPublic(id, req.session.userId!, true);
      res.json({ zoogi });
    } catch (error) {
      console.error("Error sharing zoogi:", error);
      res.status(500).json({ error: "Failed to share zoogi" });
    }
  });

  app.post("/api/gallery/zoogis/:id/vote", requireAuth, async (req, res) => {
    try {
      const zoogiId = parseInt(req.params.id);
      const voted = await storage.voteZoogi(zoogiId, req.session.userId!);
      res.json({ success: voted });
    } catch (error) {
      console.error("Error voting zoogi:", error);
      res.status(500).json({ error: "Failed to vote" });
    }
  });

  app.delete("/api/gallery/zoogis/:id/vote", requireAuth, async (req, res) => {
    try {
      const zoogiId = parseInt(req.params.id);
      const unvoted = await storage.unvoteZoogi(zoogiId, req.session.userId!);
      res.json({ success: unvoted });
    } catch (error) {
      console.error("Error unvoting zoogi:", error);
      res.status(500).json({ error: "Failed to unvote" });
    }
  });

  app.get("/api/gallery/votes", requireAuth, async (req, res) => {
    try {
      const votes = await storage.getUserZoogiVotes(req.session.userId!);
      res.json({ votes });
    } catch (error) {
      console.error("Error getting votes:", error);
      res.status(500).json({ error: "Failed to get votes" });
    }
  });

  // Arena Showcase
  app.get("/api/arenas/public", async (req, res) => {
    try {
      const limit = parseInt(req.query.limit as string) || 50;
      const arenas = await storage.getPublicArenas(limit);
      res.json({ data: arenas });
    } catch (error) {
      console.error("Error fetching arenas:", error);
      res.status(500).json({ error: "Failed to fetch arenas" });
    }
  });

  app.get("/api/arenas/mine", requireAuth, async (req, res) => {
    try {
      const arenas = await storage.getUserArenas(req.session.userId!);
      res.json({ data: arenas });
    } catch (error) {
      console.error("Error fetching user arenas:", error);
      res.status(500).json({ error: "Failed to fetch arenas" });
    }
  });

  app.post("/api/arenas", requireAuth, async (req, res) => {
    try {
      const { name, description, arenaData, thumbnailUrl, isPublic } = req.body;
      if (!name || !arenaData) {
        return res.status(400).json({ error: "Missing required fields" });
      }
      const arena = await storage.createArena({
        userId: req.session.userId!,
        name,
        description,
        arenaData,
        thumbnailUrl,
        isPublic,
      });
      res.json({ arena });
    } catch (error) {
      console.error("Error creating arena:", error);
      res.status(500).json({ error: "Failed to create arena" });
    }
  });

  app.patch("/api/arenas/:id", requireAuth, async (req, res) => {
    try {
      const id = parseInt(req.params.id);
      const arena = await storage.updateArena(id, req.session.userId!, req.body);
      res.json({ arena });
    } catch (error) {
      console.error("Error updating arena:", error);
      res.status(500).json({ error: "Failed to update arena" });
    }
  });

  app.delete("/api/arenas/:id", requireAuth, async (req, res) => {
    try {
      const id = parseInt(req.params.id);
      const deleted = await storage.deleteArena(id, req.session.userId!);
      res.json({ success: deleted });
    } catch (error) {
      console.error("Error deleting arena:", error);
      res.status(500).json({ error: "Failed to delete arena" });
    }
  });

  app.post("/api/arenas/:id/vote", requireAuth, async (req, res) => {
    try {
      const arenaId = parseInt(req.params.id);
      const voted = await storage.voteArena(arenaId, req.session.userId!);
      res.json({ success: voted });
    } catch (error) {
      console.error("Error voting arena:", error);
      res.status(500).json({ error: "Failed to vote" });
    }
  });

  app.post("/api/arenas/:id/download", async (req, res) => {
    try {
      const arenaId = parseInt(req.params.id);
      const arena = await storage.downloadArena(arenaId);
      res.json({ arena });
    } catch (error) {
      console.error("Error downloading arena:", error);
      res.status(500).json({ error: "Failed to download arena" });
    }
  });

  app.get("/api/arenas/votes", requireAuth, async (req, res) => {
    try {
      const votes = await storage.getUserArenaVotes(req.session.userId!);
      res.json({ votes });
    } catch (error) {
      console.error("Error getting arena votes:", error);
      res.status(500).json({ error: "Failed to get votes" });
    }
  });

  // Clans
  app.get("/api/clans", async (req, res) => {
    try {
      const limit = parseInt(req.query.limit as string) || 20;
      const clansList = await storage.getTopClans(limit);
      res.json({ data: clansList });
    } catch (error) {
      console.error("Error fetching clans:", error);
      res.status(500).json({ error: "Failed to fetch clans" });
    }
  });

  app.get("/api/clans/:id", async (req, res) => {
    try {
      const id = parseInt(req.params.id);
      const clan = await storage.getClan(id);
      if (!clan) {
        return res.status(404).json({ error: "Clan not found" });
      }
      const members = await storage.getClanMembers(id);
      res.json({ clan, members });
    } catch (error) {
      console.error("Error fetching clan:", error);
      res.status(500).json({ error: "Failed to fetch clan" });
    }
  });

  app.post("/api/clans", requireAuth, async (req, res) => {
    try {
      const { name, tag, description, iconUrl } = req.body;
      if (!name || !tag) {
        return res.status(400).json({ error: "Name and tag required" });
      }
      
      const existingClan = await storage.getUserClan(req.session.userId!);
      if (existingClan) {
        return res.status(400).json({ error: "Already in a clan" });
      }
      
      const clan = await storage.createClan({
        name,
        tag,
        description,
        leaderId: req.session.userId!,
        iconUrl,
      });
      res.json({ clan });
    } catch (error: any) {
      if (error.code === "23505") {
        return res.status(400).json({ error: "Clan name or tag already taken" });
      }
      console.error("Error creating clan:", error);
      res.status(500).json({ error: "Failed to create clan" });
    }
  });

  app.get("/api/clans/mine", requireAuth, async (req, res) => {
    try {
      const result = await storage.getUserClan(req.session.userId!);
      if (!result) {
        return res.json({ clan: null, membership: null });
      }
      const members = await storage.getClanMembers(result.clan.id);
      res.json({ ...result, members });
    } catch (error) {
      console.error("Error fetching user clan:", error);
      res.status(500).json({ error: "Failed to fetch clan" });
    }
  });

  app.post("/api/clans/:id/join", requireAuth, async (req, res) => {
    try {
      const clanId = parseInt(req.params.id);
      const member = await storage.joinClan(clanId, req.session.userId!);
      if (!member) {
        return res.status(400).json({ error: "Already in a clan" });
      }
      res.json({ member });
    } catch (error) {
      console.error("Error joining clan:", error);
      res.status(500).json({ error: "Failed to join clan" });
    }
  });

  app.post("/api/clans/leave", requireAuth, async (req, res) => {
    try {
      const left = await storage.leaveClan(req.session.userId!);
      if (!left) {
        return res.status(400).json({ error: "Cannot leave clan (leader or not in clan)" });
      }
      res.json({ success: true });
    } catch (error) {
      console.error("Error leaving clan:", error);
      res.status(500).json({ error: "Failed to leave clan" });
    }
  });

  // Seasons & Leaderboards
  app.get("/api/seasons/current", async (req, res) => {
    try {
      const season = await storage.getActiveSeason();
      res.json({ season });
    } catch (error) {
      console.error("Error fetching season:", error);
      res.status(500).json({ error: "Failed to fetch season" });
    }
  });

  app.get("/api/leaderboard/seasonal/:seasonId", async (req, res) => {
    try {
      const seasonId = parseInt(req.params.seasonId);
      const limit = parseInt(req.query.limit as string) || 100;
      const entries = await storage.getSeasonalLeaderboard(seasonId, limit);
      res.json({ data: entries });
    } catch (error) {
      console.error("Error fetching seasonal leaderboard:", error);
      res.status(500).json({ error: "Failed to fetch leaderboard" });
    }
  });

  app.get("/api/leaderboard/weekly", async (req, res) => {
    try {
      const limit = parseInt(req.query.limit as string) || 50;
      const entries = await storage.getWeeklyLeaderboard(limit);
      res.json({ data: entries });
    } catch (error) {
      console.error("Error fetching weekly leaderboard:", error);
      res.status(500).json({ error: "Failed to fetch leaderboard" });
    }
  });

  // Tournaments
  app.get("/api/tournaments", async (req, res) => {
    try {
      const active = await storage.getActiveTournaments();
      const upcoming = await storage.getUpcomingTournaments();
      res.json({ active, upcoming });
    } catch (error) {
      console.error("Error fetching tournaments:", error);
      res.status(500).json({ error: "Failed to fetch tournaments" });
    }
  });

  app.get("/api/tournaments/:id", async (req, res) => {
    try {
      const id = parseInt(req.params.id);
      const tournament = await storage.getTournament(id);
      if (!tournament) {
        return res.status(404).json({ error: "Tournament not found" });
      }
      const leaderboardEntries = await storage.getTournamentLeaderboard(id);
      res.json({ tournament, leaderboard: leaderboardEntries });
    } catch (error) {
      console.error("Error fetching tournament:", error);
      res.status(500).json({ error: "Failed to fetch tournament" });
    }
  });

  app.post("/api/tournaments/:id/join", requireAuth, async (req, res) => {
    try {
      const tournamentId = parseInt(req.params.id);
      const participant = await storage.joinTournament(tournamentId, req.session.userId!);
      if (!participant) {
        return res.status(400).json({ error: "Already joined" });
      }
      res.json({ participant });
    } catch (error) {
      console.error("Error joining tournament:", error);
      res.status(500).json({ error: "Failed to join tournament" });
    }
  });

  // Daily Bonuses
  app.get("/api/daily-bonus", requireAuth, async (req, res) => {
    try {
      const lastBonus = await storage.getLastDailyBonus(req.session.userId!);
      const user = await storage.getUser(req.session.userId!);
      
      let canClaim = true;
      let nextDay = 1;
      
      if (lastBonus) {
        const lastClaimDate = new Date(lastBonus.claimedAt);
        const today = new Date();
        const isSameDay = lastClaimDate.toDateString() === today.toDateString();
        canClaim = !isSameDay;
        
        const daysSinceLastClaim = Math.floor((today.getTime() - lastClaimDate.getTime()) / (1000 * 60 * 60 * 24));
        if (daysSinceLastClaim <= 1) {
          nextDay = (lastBonus.day % 7) + 1;
        } else {
          nextDay = 1;
        }
      }
      
      res.json({ canClaim, nextDay, loginStreak: user?.loginStreak || 0 });
    } catch (error) {
      console.error("Error checking daily bonus:", error);
      res.status(500).json({ error: "Failed to check daily bonus" });
    }
  });

  app.post("/api/daily-bonus/claim", requireAuth, async (req, res) => {
    try {
      const lastBonus = await storage.getLastDailyBonus(req.session.userId!);
      
      if (lastBonus) {
        const lastClaimDate = new Date(lastBonus.claimedAt);
        const today = new Date();
        if (lastClaimDate.toDateString() === today.toDateString()) {
          return res.status(400).json({ error: "Already claimed today" });
        }
      }
      
      let day = 1;
      if (lastBonus) {
        const daysSinceLastClaim = Math.floor((Date.now() - lastBonus.claimedAt.getTime()) / (1000 * 60 * 60 * 24));
        if (daysSinceLastClaim <= 1) {
          day = (lastBonus.day % 7) + 1;
        }
      }
      
      const dailyRewards = [
        { coins: 50 },
        { coins: 75 },
        { coins: 100, gems: 1 },
        { coins: 125 },
        { coins: 150, gems: 2 },
        { coins: 200 },
        { coins: 300, gems: 5 },
      ];
      
      const reward = dailyRewards[day - 1];
      const bonus = await storage.claimDailyBonus(req.session.userId!, day, reward);
      
      const user = await storage.getUser(req.session.userId!);
      if (user) {
        await storage.updateUser(req.session.userId!, {
          coins: user.coins + (reward.coins || 0),
          gems: user.gems + (reward.gems || 0),
        });
      }
      
      res.json({ bonus, reward, day });
    } catch (error) {
      console.error("Error claiming daily bonus:", error);
      res.status(500).json({ error: "Failed to claim bonus" });
    }
  });

  // Referrals
  app.get("/api/referrals", requireAuth, async (req, res) => {
    try {
      const referralsList = await storage.getReferrals(req.session.userId!);
      res.json({ data: referralsList });
    } catch (error) {
      console.error("Error fetching referrals:", error);
      res.status(500).json({ error: "Failed to fetch referrals" });
    }
  });

  app.post("/api/referrals/:id/claim", requireAuth, async (req, res) => {
    try {
      const referralId = parseInt(req.params.id);
      const claimed = await storage.claimReferralReward(referralId);
      if (!claimed) {
        return res.status(400).json({ error: "Already claimed or not found" });
      }
      
      const user = await storage.getUser(req.session.userId!);
      if (user) {
        await storage.updateUser(req.session.userId!, {
          coins: user.coins + 200,
          gems: user.gems + 5,
        });
      }
      
      res.json({ success: true, reward: { coins: 200, gems: 5 } });
    } catch (error) {
      console.error("Error claiming referral:", error);
      res.status(500).json({ error: "Failed to claim referral" });
    }
  });

  // Challenges
  app.get("/api/challenges", requireAuth, async (req, res) => {
    try {
      const activeChallenges = await storage.getActiveChallenges();
      const userProgress = await storage.getUserChallengeProgress(req.session.userId!);
      
      const challengesWithProgress = activeChallenges.map(challenge => {
        const progress = userProgress.find(p => p.challengeId === challenge.id);
        return {
          ...challenge,
          progress: progress?.progress || 0,
          completed: progress?.completed || false,
          rewardClaimed: progress?.rewardClaimed || false,
        };
      });
      
      res.json({ data: challengesWithProgress });
    } catch (error) {
      console.error("Error fetching challenges:", error);
      res.status(500).json({ error: "Failed to fetch challenges" });
    }
  });

  app.post("/api/challenges/:id/claim", requireAuth, async (req, res) => {
    try {
      const challengeId = parseInt(req.params.id);
      const claimed = await storage.claimChallengeReward(challengeId, req.session.userId!);
      if (!claimed) {
        return res.status(400).json({ error: "Cannot claim reward" });
      }
      res.json({ success: true });
    } catch (error) {
      console.error("Error claiming challenge reward:", error);
      res.status(500).json({ error: "Failed to claim reward" });
    }
  });

  // Replays
  app.get("/api/replays/public", async (req, res) => {
    try {
      const limit = parseInt(req.query.limit as string) || 50;
      const replaysList = await storage.getPublicReplays(limit);
      res.json({ data: replaysList });
    } catch (error) {
      console.error("Error fetching replays:", error);
      res.status(500).json({ error: "Failed to fetch replays" });
    }
  });

  app.get("/api/replays/mine", requireAuth, async (req, res) => {
    try {
      const replaysList = await storage.getUserReplays(req.session.userId!);
      res.json({ data: replaysList });
    } catch (error) {
      console.error("Error fetching user replays:", error);
      res.status(500).json({ error: "Failed to fetch replays" });
    }
  });

  app.get("/api/replays/:id", async (req, res) => {
    try {
      const id = parseInt(req.params.id);
      const replay = await storage.getReplay(id);
      if (!replay) {
        return res.status(404).json({ error: "Replay not found" });
      }
      res.json({ replay });
    } catch (error) {
      console.error("Error fetching replay:", error);
      res.status(500).json({ error: "Failed to fetch replay" });
    }
  });

  app.post("/api/replays", requireAuth, async (req, res) => {
    try {
      const { title, replayData, duration, isPublic } = req.body;
      if (!replayData) {
        return res.status(400).json({ error: "Replay data required" });
      }
      const replay = await storage.saveReplay({
        userId: req.session.userId!,
        title,
        replayData,
        duration,
        isPublic,
      });
      res.json({ replay });
    } catch (error) {
      console.error("Error saving replay:", error);
      res.status(500).json({ error: "Failed to save replay" });
    }
  });

  app.delete("/api/replays/:id", requireAuth, async (req, res) => {
    try {
      const id = parseInt(req.params.id);
      const deleted = await storage.deleteReplay(id, req.session.userId!);
      res.json({ success: deleted });
    } catch (error) {
      console.error("Error deleting replay:", error);
      res.status(500).json({ error: "Failed to delete replay" });
    }
  });

  // Map Decorations - Save and load map editor configurations
  app.get("/api/map-decorations/:mapId", async (req, res) => {
    try {
      const { mapId } = req.params;
      const deviceId = req.query.deviceId as string;
      if (!deviceId) {
        return res.status(400).json({ error: "deviceId required" });
      }
      const decoration = await storage.getMapDecoration(deviceId, mapId);
      res.json({ decoration });
    } catch (error) {
      console.error("Error fetching map decoration:", error);
      res.status(500).json({ error: "Failed to fetch map decoration" });
    }
  });

  app.post("/api/map-decorations/:mapId", async (req, res) => {
    console.log("POST /api/map-decorations/:mapId received:", { mapId: req.params.mapId, bodyKeys: Object.keys(req.body || {}) });
    try {
      const { mapId } = req.params;
      const { 
        deviceId, 
        placedModels, 
        backgroundSettings,
        wallSettings,
        wallSegmentConfigs,
        innerWallSegmentConfigs,
        zoneSettings,
        zoneEditorConfigs,
        elementTransforms
      } = req.body;
      console.log("Map decoration save request:", { 
        mapId, 
        deviceId, 
        modelsCount: placedModels?.length,
        wallSegmentCount: wallSegmentConfigs?.length,
        innerWallSegmentCount: innerWallSegmentConfigs?.length
      });
      if (!deviceId) {
        console.log("Missing deviceId in request");
        return res.status(400).json({ error: "deviceId required" });
      }
      const decoration = await storage.saveMapDecoration(
        deviceId, 
        mapId, 
        placedModels || [], 
        backgroundSettings || null,
        wallSettings || null,
        wallSegmentConfigs || null,
        innerWallSegmentConfigs || null,
        zoneSettings || null,
        zoneEditorConfigs || null,
        elementTransforms || null
      );
      console.log("Map decoration saved successfully:", { id: decoration?.id });
      res.json({ decoration, success: true });
    } catch (error) {
      console.error("Error saving map decoration:", error);
      res.status(500).json({ error: "Failed to save map decoration", details: error instanceof Error ? error.message : String(error) });
    }
  });

  app.get("/api/map-decorations", async (req, res) => {
    try {
      const deviceId = req.query.deviceId as string;
      if (!deviceId) {
        return res.status(400).json({ error: "deviceId required" });
      }
      const decorations = await storage.getMapDecorationsByDevice(deviceId);
      res.json({ decorations });
    } catch (error) {
      console.error("Error fetching map decorations:", error);
      res.status(500).json({ error: "Failed to fetch map decorations" });
    }
  });

  return httpServer;
}
