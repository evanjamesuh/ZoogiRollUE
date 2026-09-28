import { pgTable, text, serial, integer, boolean, timestamp, jsonb } from "drizzle-orm/pg-core";
import { createInsertSchema } from "drizzle-zod";
import { z } from "zod";

export const users = pgTable("users", {
  id: serial("id").primaryKey(),
  username: text("username").notNull().unique(),
  password: text("password").notNull(),
  displayName: text("display_name"),
  avatarUrl: text("avatar_url"),
  favoriteZoogi: text("favorite_zoogi"),
  totalWins: integer("total_wins").default(0).notNull(),
  totalLosses: integer("total_losses").default(0).notNull(),
  totalKnockoffs: integer("total_knockoffs").default(0).notNull(),
  highScore: integer("high_score").default(0).notNull(),
  gamesPlayed: integer("games_played").default(0).notNull(),
  coins: integer("coins").default(100).notNull(),
  gems: integer("gems").default(0).notNull(),
  level: integer("level").default(1).notNull(),
  xp: integer("xp").default(0).notNull(),
  lastLoginAt: timestamp("last_login_at"),
  loginStreak: integer("login_streak").default(0).notNull(),
  createdAt: timestamp("created_at").defaultNow().notNull(),
});

export const friendships = pgTable("friendships", {
  id: serial("id").primaryKey(),
  userId: integer("user_id").notNull(),
  friendId: integer("friend_id").notNull(),
  status: text("status").notNull().default("pending"),
  createdAt: timestamp("created_at").defaultNow().notNull(),
});

export const achievements = pgTable("achievements", {
  id: serial("id").primaryKey(),
  userId: integer("user_id").notNull(),
  achievementId: text("achievement_id").notNull(),
  unlockedAt: timestamp("unlocked_at").defaultNow().notNull(),
});

export const chatMessages = pgTable("chat_messages", {
  id: serial("id").primaryKey(),
  senderId: integer("sender_id").notNull(),
  receiverId: integer("receiver_id"),
  channel: text("channel").notNull().default("global"),
  message: text("message").notNull(),
  createdAt: timestamp("created_at").defaultNow().notNull(),
});

export const purchases = pgTable("purchases", {
  id: serial("id").primaryKey(),
  deviceId: text("device_id").notNull(),
  sessionId: text("session_id"),
  characterId: text("character_id").notNull(),
  priceId: text("price_id"),
  status: text("status").notNull().default("pending"),
  createdAt: timestamp("created_at").defaultNow().notNull(),
});

export const leaderboard = pgTable("leaderboard", {
  id: serial("id").primaryKey(),
  playerName: text("player_name").notNull(),
  score: integer("score").notNull(),
  zoogiUsed: text("zoogi_used").notNull(),
  createdAt: timestamp("created_at").defaultNow().notNull(),
});

export const customZoogis = pgTable("custom_zoogis", {
  id: serial("id").primaryKey(),
  deviceId: text("device_id").notNull(),
  userId: integer("user_id"),
  name: text("name").notNull(),
  meshyTaskId: text("meshy_task_id").notNull(),
  thumbnailUrl: text("thumbnail_url"),
  stats: jsonb("stats").notNull(),
  isPublic: boolean("is_public").default(false).notNull(),
  likes: integer("likes").default(0).notNull(),
  createdAt: timestamp("created_at").defaultNow().notNull(),
});

export const zoogiVotes = pgTable("zoogi_votes", {
  id: serial("id").primaryKey(),
  zoogiId: integer("zoogi_id").notNull(),
  userId: integer("user_id").notNull(),
  createdAt: timestamp("created_at").defaultNow().notNull(),
});

export const customArenas = pgTable("custom_arenas", {
  id: serial("id").primaryKey(),
  userId: integer("user_id"),
  deviceId: text("device_id"),
  name: text("name").notNull(),
  description: text("description"),
  arenaData: jsonb("arena_data"),
  meshyTaskId: text("meshy_task_id"),
  modelType: text("model_type").default("config"),
  modelUrl: text("model_url"),
  thumbnailUrl: text("thumbnail_url"),
  isPublic: boolean("is_public").default(false).notNull(),
  likes: integer("likes").default(0).notNull(),
  downloads: integer("downloads").default(0).notNull(),
  createdAt: timestamp("created_at").defaultNow().notNull(),
});

export const arenaVotes = pgTable("arena_votes", {
  id: serial("id").primaryKey(),
  arenaId: integer("arena_id").notNull(),
  userId: integer("user_id").notNull(),
  createdAt: timestamp("created_at").defaultNow().notNull(),
});

export const mapDecorations = pgTable("map_decorations", {
  id: serial("id").primaryKey(),
  deviceId: text("device_id").notNull(),
  mapId: text("map_id").notNull(),
  placedModels: jsonb("placed_models").notNull(),
  backgroundSettings: jsonb("background_settings"),
  wallSettings: jsonb("wall_settings"),
  wallSegmentConfigs: jsonb("wall_segment_configs"),
  innerWallSegmentConfigs: jsonb("inner_wall_segment_configs"),
  zoneSettings: jsonb("zone_settings"),
  zoneEditorConfigs: jsonb("zone_editor_configs"),
  elementTransforms: jsonb("element_transforms"),
  createdAt: timestamp("created_at").defaultNow().notNull(),
  updatedAt: timestamp("updated_at").defaultNow().notNull(),
});

export const clans = pgTable("clans", {
  id: serial("id").primaryKey(),
  name: text("name").notNull().unique(),
  tag: text("tag").notNull().unique(),
  description: text("description"),
  leaderId: integer("leader_id").notNull(),
  iconUrl: text("icon_url"),
  totalWins: integer("total_wins").default(0).notNull(),
  totalScore: integer("total_score").default(0).notNull(),
  memberCount: integer("member_count").default(1).notNull(),
  createdAt: timestamp("created_at").defaultNow().notNull(),
});

export const clanMembers = pgTable("clan_members", {
  id: serial("id").primaryKey(),
  clanId: integer("clan_id").notNull(),
  userId: integer("user_id").notNull(),
  role: text("role").notNull().default("member"),
  joinedAt: timestamp("joined_at").defaultNow().notNull(),
});

export const seasons = pgTable("seasons", {
  id: serial("id").primaryKey(),
  name: text("name").notNull(),
  startDate: timestamp("start_date").notNull(),
  endDate: timestamp("end_date").notNull(),
  rewards: jsonb("rewards"),
  isActive: boolean("is_active").default(false).notNull(),
});

export const seasonalLeaderboard = pgTable("seasonal_leaderboard", {
  id: serial("id").primaryKey(),
  seasonId: integer("season_id").notNull(),
  userId: integer("user_id").notNull(),
  score: integer("score").default(0).notNull(),
  wins: integer("wins").default(0).notNull(),
  rank: integer("rank"),
  updatedAt: timestamp("updated_at").defaultNow().notNull(),
});

export const tournaments = pgTable("tournaments", {
  id: serial("id").primaryKey(),
  name: text("name").notNull(),
  description: text("description"),
  startTime: timestamp("start_time").notNull(),
  endTime: timestamp("end_time").notNull(),
  entryFee: integer("entry_fee").default(0).notNull(),
  prizePool: jsonb("prize_pool"),
  maxParticipants: integer("max_participants"),
  status: text("status").notNull().default("upcoming"),
  createdAt: timestamp("created_at").defaultNow().notNull(),
});

export const tournamentParticipants = pgTable("tournament_participants", {
  id: serial("id").primaryKey(),
  tournamentId: integer("tournament_id").notNull(),
  userId: integer("user_id").notNull(),
  score: integer("score").default(0).notNull(),
  placement: integer("placement"),
  joinedAt: timestamp("joined_at").defaultNow().notNull(),
});

export const dailyBonuses = pgTable("daily_bonuses", {
  id: serial("id").primaryKey(),
  userId: integer("user_id").notNull(),
  day: integer("day").notNull(),
  reward: jsonb("reward").notNull(),
  claimedAt: timestamp("claimed_at").defaultNow().notNull(),
});

export const referrals = pgTable("referrals", {
  id: serial("id").primaryKey(),
  referrerId: integer("referrer_id").notNull(),
  referredId: integer("referred_id").notNull(),
  rewardClaimed: boolean("reward_claimed").default(false).notNull(),
  createdAt: timestamp("created_at").defaultNow().notNull(),
});

export const challenges = pgTable("challenges", {
  id: serial("id").primaryKey(),
  name: text("name").notNull(),
  description: text("description").notNull(),
  type: text("type").notNull(),
  target: integer("target").notNull(),
  reward: jsonb("reward").notNull(),
  startDate: timestamp("start_date").notNull(),
  endDate: timestamp("end_date").notNull(),
  isActive: boolean("is_active").default(true).notNull(),
});

export const challengeProgress = pgTable("challenge_progress", {
  id: serial("id").primaryKey(),
  challengeId: integer("challenge_id").notNull(),
  userId: integer("user_id").notNull(),
  progress: integer("progress").default(0).notNull(),
  completed: boolean("completed").default(false).notNull(),
  rewardClaimed: boolean("reward_claimed").default(false).notNull(),
  updatedAt: timestamp("updated_at").defaultNow().notNull(),
});

export const replays = pgTable("replays", {
  id: serial("id").primaryKey(),
  userId: integer("user_id").notNull(),
  title: text("title"),
  replayData: jsonb("replay_data").notNull(),
  duration: integer("duration"),
  isPublic: boolean("is_public").default(false).notNull(),
  likes: integer("likes").default(0).notNull(),
  views: integer("views").default(0).notNull(),
  createdAt: timestamp("created_at").defaultNow().notNull(),
});

export const insertUserSchema = createInsertSchema(users).pick({
  username: true,
  password: true,
});

export const registerUserSchema = z.object({
  username: z.string().min(3).max(20),
  password: z.string().min(6),
  displayName: z.string().min(1).max(30).optional(),
});

export const loginUserSchema = z.object({
  username: z.string(),
  password: z.string(),
});

export const insertFriendshipSchema = createInsertSchema(friendships).pick({
  userId: true,
  friendId: true,
  status: true,
});

export const insertChatMessageSchema = createInsertSchema(chatMessages).pick({
  senderId: true,
  receiverId: true,
  channel: true,
  message: true,
});

export const insertLeaderboardSchema = createInsertSchema(leaderboard).pick({
  playerName: true,
  score: true,
  zoogiUsed: true,
});

export const insertCustomZoogiSchema = createInsertSchema(customZoogis).pick({
  deviceId: true,
  name: true,
  meshyTaskId: true,
  thumbnailUrl: true,
  stats: true,
});

export const insertCustomArenaSchema = createInsertSchema(customArenas).pick({
  deviceId: true,
  name: true,
  description: true,
  meshyTaskId: true,
  modelType: true,
  thumbnailUrl: true,
});

export type InsertUser = z.infer<typeof insertUserSchema>;
export type User = typeof users.$inferSelect;
export type Friendship = typeof friendships.$inferSelect;
export type Achievement = typeof achievements.$inferSelect;
export type ChatMessage = typeof chatMessages.$inferSelect;
export type InsertLeaderboard = z.infer<typeof insertLeaderboardSchema>;
export type Leaderboard = typeof leaderboard.$inferSelect;
export type InsertCustomZoogi = z.infer<typeof insertCustomZoogiSchema>;
export type CustomZoogi = typeof customZoogis.$inferSelect;
export type ZoogiVote = typeof zoogiVotes.$inferSelect;
export type CustomArena = typeof customArenas.$inferSelect;
export type ArenaVote = typeof arenaVotes.$inferSelect;
export type Clan = typeof clans.$inferSelect;
export type ClanMember = typeof clanMembers.$inferSelect;
export type Season = typeof seasons.$inferSelect;
export type SeasonalLeaderboard = typeof seasonalLeaderboard.$inferSelect;
export type Tournament = typeof tournaments.$inferSelect;
export type TournamentParticipant = typeof tournamentParticipants.$inferSelect;
export type DailyBonus = typeof dailyBonuses.$inferSelect;
export type Referral = typeof referrals.$inferSelect;
export type Challenge = typeof challenges.$inferSelect;
export type ChallengeProgress = typeof challengeProgress.$inferSelect;
export type Replay = typeof replays.$inferSelect;
