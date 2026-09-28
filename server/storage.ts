import { 
  users, leaderboard, purchases, customZoogis, friendships, chatMessages, achievements,
  zoogiVotes, customArenas, arenaVotes, clans, clanMembers, seasons, seasonalLeaderboard,
  tournaments, tournamentParticipants, dailyBonuses, referrals, challenges, challengeProgress, replays, mapDecorations,
  type User, type InsertUser, type Leaderboard, type InsertLeaderboard, type CustomZoogi, type InsertCustomZoogi, 
  type Friendship, type ChatMessage, type Achievement, type ZoogiVote, type CustomArena, type ArenaVote,
  type Clan, type ClanMember, type Season, type SeasonalLeaderboard, type Tournament, type TournamentParticipant,
  type DailyBonus, type Referral, type Challenge, type ChallengeProgress, type Replay
} from "@shared/schema";
import { db } from "./db";
import { desc, eq, and, or, sql, gte, lte, asc } from "drizzle-orm";

export interface IStorage {
  getUser(id: number): Promise<User | undefined>;
  getUserByUsername(username: string): Promise<User | undefined>;
  createUser(user: InsertUser): Promise<User>;
  updateUser(id: number, updates: Partial<User>): Promise<User | undefined>;
  getLeaderboard(limit?: number): Promise<Leaderboard[]>;
  addLeaderboardEntry(entry: InsertLeaderboard): Promise<Leaderboard>;
  getCustomZoogisByDevice(deviceId: string): Promise<CustomZoogi[]>;
  createCustomZoogi(zoogi: InsertCustomZoogi): Promise<CustomZoogi>;
  deleteCustomZoogi(id: number, deviceId: string): Promise<boolean>;
  getFriendships(userId: number): Promise<Friendship[]>;
  createFriendRequest(userId: number, friendId: number): Promise<Friendship>;
  updateFriendshipStatus(id: number, status: string): Promise<Friendship | undefined>;
  getChatMessages(channel: string, limit?: number): Promise<ChatMessage[]>;
  getDirectMessages(userId: number, friendId: number, limit?: number): Promise<ChatMessage[]>;
  createChatMessage(senderId: number, message: string, channel?: string, receiverId?: number): Promise<ChatMessage>;
  getUserAchievements(userId: number): Promise<Achievement[]>;
  unlockAchievement(userId: number, achievementId: string): Promise<Achievement>;
  searchUsers(query: string, limit?: number): Promise<User[]>;
}

export class DatabaseStorage implements IStorage {
  async getUser(id: number): Promise<User | undefined> {
    const [user] = await db.select().from(users).where(eq(users.id, id));
    return user;
  }

  async getUserByUsername(username: string): Promise<User | undefined> {
    const [user] = await db.select().from(users).where(eq(users.username, username));
    return user;
  }

  async createUser(insertUser: InsertUser): Promise<User> {
    const [user] = await db.insert(users).values(insertUser).returning();
    return user;
  }
  
  async getLeaderboard(limit: number = 10): Promise<Leaderboard[]> {
    return await db.select().from(leaderboard).orderBy(desc(leaderboard.score)).limit(limit);
  }
  
  async addLeaderboardEntry(entry: InsertLeaderboard): Promise<Leaderboard> {
    const [newEntry] = await db.insert(leaderboard).values(entry).returning();
    return newEntry;
  }

  async createPurchase(data: { deviceId: string; sessionId: string; characterId: string; priceId: string }) {
    const [purchase] = await db.insert(purchases).values({
      deviceId: data.deviceId,
      sessionId: data.sessionId,
      characterId: data.characterId,
      priceId: data.priceId,
      status: "pending"
    }).returning();
    return purchase;
  }

  async getCompletedPurchasesByDevice(deviceId: string) {
    return await db.select().from(purchases)
      .where(and(eq(purchases.deviceId, deviceId), eq(purchases.status, "completed")));
  }

  async updatePurchaseStatus(sessionId: string, status: string) {
    const [purchase] = await db.update(purchases)
      .set({ status })
      .where(eq(purchases.sessionId, sessionId))
      .returning();
    return purchase;
  }

  async getCustomZoogisByDevice(deviceId: string): Promise<CustomZoogi[]> {
    return await db.select().from(customZoogis)
      .where(eq(customZoogis.deviceId, deviceId))
      .orderBy(desc(customZoogis.createdAt));
  }

  async createCustomZoogi(zoogi: InsertCustomZoogi): Promise<CustomZoogi> {
    const [newZoogi] = await db.insert(customZoogis).values(zoogi).returning();
    return newZoogi;
  }

  async deleteCustomZoogi(id: number, deviceId: string): Promise<boolean> {
    const result = await db.delete(customZoogis)
      .where(and(eq(customZoogis.id, id), eq(customZoogis.deviceId, deviceId)))
      .returning();
    return result.length > 0;
  }

  async getCustomArenasByDevice(deviceId: string): Promise<CustomArena[]> {
    return await db.select().from(customArenas)
      .where(eq(customArenas.deviceId, deviceId))
      .orderBy(desc(customArenas.createdAt));
  }

  async createCustomArena(arena: {
    deviceId?: string | null;
    name: string;
    description?: string | null;
    meshyTaskId?: string | null;
    modelType: string;
    modelUrl?: string | null;
    thumbnailUrl?: string | null;
    isPublic?: boolean;
  }): Promise<CustomArena> {
    const [newArena] = await db.insert(customArenas).values(arena).returning();
    return newArena;
  }

  async deleteCustomArena(id: number, deviceId: string): Promise<boolean> {
    const result = await db.delete(customArenas)
      .where(and(eq(customArenas.id, id), eq(customArenas.deviceId, deviceId)))
      .returning();
    return result.length > 0;
  }

  async updateUser(id: number, updates: Partial<User>): Promise<User | undefined> {
    const [user] = await db.update(users)
      .set(updates)
      .where(eq(users.id, id))
      .returning();
    return user;
  }

  async getFriendships(userId: number): Promise<Friendship[]> {
    return await db.select().from(friendships)
      .where(or(eq(friendships.userId, userId), eq(friendships.friendId, userId)))
      .orderBy(desc(friendships.createdAt));
  }

  async createFriendRequest(userId: number, friendId: number): Promise<Friendship> {
    const [friendship] = await db.insert(friendships)
      .values({ userId, friendId, status: "pending" })
      .returning();
    return friendship;
  }

  async updateFriendshipStatus(id: number, status: string): Promise<Friendship | undefined> {
    const [friendship] = await db.update(friendships)
      .set({ status })
      .where(eq(friendships.id, id))
      .returning();
    return friendship;
  }

  async getChatMessages(channel: string, limit: number = 50): Promise<ChatMessage[]> {
    return await db.select().from(chatMessages)
      .where(eq(chatMessages.channel, channel))
      .orderBy(desc(chatMessages.createdAt))
      .limit(limit);
  }

  async getDirectMessages(userId: number, friendId: number, limit: number = 50): Promise<ChatMessage[]> {
    return await db.select().from(chatMessages)
      .where(
        and(
          eq(chatMessages.channel, "direct"),
          or(
            and(eq(chatMessages.senderId, userId), eq(chatMessages.receiverId, friendId)),
            and(eq(chatMessages.senderId, friendId), eq(chatMessages.receiverId, userId))
          )
        )
      )
      .orderBy(desc(chatMessages.createdAt))
      .limit(limit);
  }

  async createChatMessage(senderId: number, message: string, channel: string = "global", receiverId?: number): Promise<ChatMessage> {
    const [msg] = await db.insert(chatMessages)
      .values({ senderId, message, channel, receiverId: receiverId ?? null })
      .returning();
    return msg;
  }

  async getUserAchievements(userId: number): Promise<Achievement[]> {
    return await db.select().from(achievements)
      .where(eq(achievements.userId, userId))
      .orderBy(desc(achievements.unlockedAt));
  }

  async unlockAchievement(userId: number, achievementId: string): Promise<Achievement> {
    const [achievement] = await db.insert(achievements)
      .values({ userId, achievementId })
      .returning();
    return achievement;
  }

  async searchUsers(query: string, limit: number = 20): Promise<User[]> {
    return await db.select().from(users)
      .where(sql`${users.username} ILIKE ${'%' + query + '%'} OR ${users.displayName} ILIKE ${'%' + query + '%'}`)
      .limit(limit);
  }

  // Gallery - Public Zoogis
  async getPublicZoogis(limit: number = 50): Promise<CustomZoogi[]> {
    return await db.select().from(customZoogis)
      .where(eq(customZoogis.isPublic, true))
      .orderBy(desc(customZoogis.likes), desc(customZoogis.createdAt))
      .limit(limit);
  }

  async updateZoogiPublic(id: number, userId: number, isPublic: boolean): Promise<CustomZoogi | undefined> {
    const [zoogi] = await db.update(customZoogis)
      .set({ isPublic, userId })
      .where(eq(customZoogis.id, id))
      .returning();
    return zoogi;
  }

  async voteZoogi(zoogiId: number, usrId: number): Promise<boolean> {
    const existing = await db.select().from(zoogiVotes)
      .where(and(eq(zoogiVotes.zoogiId, zoogiId), eq(zoogiVotes.userId, usrId)));
    if (existing.length > 0) return false;
    
    await db.insert(zoogiVotes).values({ zoogiId, userId: usrId });
    await db.update(customZoogis)
      .set({ likes: sql`${customZoogis.likes} + 1` })
      .where(eq(customZoogis.id, zoogiId));
    return true;
  }

  async unvoteZoogi(zoogiId: number, usrId: number): Promise<boolean> {
    const result = await db.delete(zoogiVotes)
      .where(and(eq(zoogiVotes.zoogiId, zoogiId), eq(zoogiVotes.userId, usrId)))
      .returning();
    if (result.length > 0) {
      await db.update(customZoogis)
        .set({ likes: sql`${customZoogis.likes} - 1` })
        .where(eq(customZoogis.id, zoogiId));
      return true;
    }
    return false;
  }

  async getUserZoogiVotes(usrId: number): Promise<number[]> {
    const votes = await db.select({ zoogiId: zoogiVotes.zoogiId }).from(zoogiVotes)
      .where(eq(zoogiVotes.userId, usrId));
    return votes.map(v => v.zoogiId);
  }

  // Arena Showcase
  async getPublicArenas(limit: number = 50): Promise<CustomArena[]> {
    return await db.select().from(customArenas)
      .where(eq(customArenas.isPublic, true))
      .orderBy(desc(customArenas.likes), desc(customArenas.createdAt))
      .limit(limit);
  }

  async getUserArenas(usrId: number): Promise<CustomArena[]> {
    return await db.select().from(customArenas)
      .where(eq(customArenas.userId, usrId))
      .orderBy(desc(customArenas.createdAt));
  }

  async createArena(data: { userId: number; name: string; description?: string; arenaData: any; thumbnailUrl?: string; isPublic?: boolean }): Promise<CustomArena> {
    const [arena] = await db.insert(customArenas).values({
      userId: data.userId,
      name: data.name,
      description: data.description || null,
      arenaData: data.arenaData,
      thumbnailUrl: data.thumbnailUrl || null,
      isPublic: data.isPublic || false,
    }).returning();
    return arena;
  }

  async updateArena(id: number, usrId: number, updates: Partial<CustomArena>): Promise<CustomArena | undefined> {
    const [arena] = await db.update(customArenas)
      .set(updates)
      .where(and(eq(customArenas.id, id), eq(customArenas.userId, usrId)))
      .returning();
    return arena;
  }

  async deleteArena(id: number, usrId: number): Promise<boolean> {
    const result = await db.delete(customArenas)
      .where(and(eq(customArenas.id, id), eq(customArenas.userId, usrId)))
      .returning();
    return result.length > 0;
  }

  async voteArena(arenaId: number, usrId: number): Promise<boolean> {
    const existing = await db.select().from(arenaVotes)
      .where(and(eq(arenaVotes.arenaId, arenaId), eq(arenaVotes.userId, usrId)));
    if (existing.length > 0) return false;
    
    await db.insert(arenaVotes).values({ arenaId, userId: usrId });
    await db.update(customArenas)
      .set({ likes: sql`${customArenas.likes} + 1` })
      .where(eq(customArenas.id, arenaId));
    return true;
  }

  async downloadArena(arenaId: number): Promise<CustomArena | undefined> {
    const [arena] = await db.update(customArenas)
      .set({ downloads: sql`${customArenas.downloads} + 1` })
      .where(eq(customArenas.id, arenaId))
      .returning();
    return arena;
  }

  async getUserArenaVotes(usrId: number): Promise<number[]> {
    const votes = await db.select({ arenaId: arenaVotes.arenaId }).from(arenaVotes)
      .where(eq(arenaVotes.userId, usrId));
    return votes.map(v => v.arenaId);
  }

  // Clans
  async getClan(id: number): Promise<Clan | undefined> {
    const [clan] = await db.select().from(clans).where(eq(clans.id, id));
    return clan;
  }

  async getClanByName(name: string): Promise<Clan | undefined> {
    const [clan] = await db.select().from(clans).where(eq(clans.name, name));
    return clan;
  }

  async getTopClans(limit: number = 20): Promise<Clan[]> {
    return await db.select().from(clans)
      .orderBy(desc(clans.totalScore))
      .limit(limit);
  }

  async createClan(data: { name: string; tag: string; description?: string; leaderId: number; iconUrl?: string }): Promise<Clan> {
    const [clan] = await db.insert(clans).values({
      name: data.name,
      tag: data.tag,
      description: data.description || null,
      leaderId: data.leaderId,
      iconUrl: data.iconUrl || null,
    }).returning();
    
    await db.insert(clanMembers).values({ clanId: clan.id, userId: data.leaderId, role: "leader" });
    return clan;
  }

  async getClanMembers(clanId: number): Promise<(ClanMember & { user?: User })[]> {
    const members = await db.select().from(clanMembers).where(eq(clanMembers.clanId, clanId));
    return members;
  }

  async getUserClan(usrId: number): Promise<{ clan: Clan; membership: ClanMember } | undefined> {
    const [membership] = await db.select().from(clanMembers).where(eq(clanMembers.userId, usrId));
    if (!membership) return undefined;
    const clan = await this.getClan(membership.clanId);
    if (!clan) return undefined;
    return { clan, membership };
  }

  async joinClan(clanId: number, usrId: number): Promise<ClanMember | undefined> {
    const existing = await db.select().from(clanMembers).where(eq(clanMembers.userId, usrId));
    if (existing.length > 0) return undefined;
    
    const [member] = await db.insert(clanMembers).values({ clanId, userId: usrId, role: "member" }).returning();
    await db.update(clans).set({ memberCount: sql`${clans.memberCount} + 1` }).where(eq(clans.id, clanId));
    return member;
  }

  async leaveClan(usrId: number): Promise<boolean> {
    const [member] = await db.select().from(clanMembers).where(eq(clanMembers.userId, usrId));
    if (!member || member.role === "leader") return false;
    
    await db.delete(clanMembers).where(eq(clanMembers.userId, usrId));
    await db.update(clans).set({ memberCount: sql`${clans.memberCount} - 1` }).where(eq(clans.id, member.clanId));
    return true;
  }

  // Seasons & Leaderboards
  async getActiveSeason(): Promise<Season | undefined> {
    const [season] = await db.select().from(seasons).where(eq(seasons.isActive, true));
    return season;
  }

  async getSeasonalLeaderboard(seasonId: number, limit: number = 100): Promise<SeasonalLeaderboard[]> {
    return await db.select().from(seasonalLeaderboard)
      .where(eq(seasonalLeaderboard.seasonId, seasonId))
      .orderBy(desc(seasonalLeaderboard.score))
      .limit(limit);
  }

  async updateSeasonalScore(seasonId: number, usrId: number, scoreToAdd: number, won: boolean): Promise<void> {
    const existing = await db.select().from(seasonalLeaderboard)
      .where(and(eq(seasonalLeaderboard.seasonId, seasonId), eq(seasonalLeaderboard.userId, usrId)));
    
    if (existing.length === 0) {
      await db.insert(seasonalLeaderboard).values({
        seasonId, userId: usrId, score: scoreToAdd, wins: won ? 1 : 0
      });
    } else {
      await db.update(seasonalLeaderboard)
        .set({ 
          score: sql`${seasonalLeaderboard.score} + ${scoreToAdd}`,
          wins: won ? sql`${seasonalLeaderboard.wins} + 1` : seasonalLeaderboard.wins,
          updatedAt: new Date()
        })
        .where(and(eq(seasonalLeaderboard.seasonId, seasonId), eq(seasonalLeaderboard.userId, usrId)));
    }
  }

  async getWeeklyLeaderboard(limit: number = 50): Promise<Leaderboard[]> {
    const oneWeekAgo = new Date();
    oneWeekAgo.setDate(oneWeekAgo.getDate() - 7);
    return await db.select().from(leaderboard)
      .where(gte(leaderboard.createdAt, oneWeekAgo))
      .orderBy(desc(leaderboard.score))
      .limit(limit);
  }

  // Tournaments
  async getActiveTournaments(): Promise<Tournament[]> {
    return await db.select().from(tournaments)
      .where(eq(tournaments.status, "active"))
      .orderBy(asc(tournaments.endTime));
  }

  async getUpcomingTournaments(): Promise<Tournament[]> {
    return await db.select().from(tournaments)
      .where(eq(tournaments.status, "upcoming"))
      .orderBy(asc(tournaments.startTime));
  }

  async getTournament(id: number): Promise<Tournament | undefined> {
    const [tournament] = await db.select().from(tournaments).where(eq(tournaments.id, id));
    return tournament;
  }

  async joinTournament(tournamentId: number, usrId: number): Promise<TournamentParticipant | undefined> {
    const existing = await db.select().from(tournamentParticipants)
      .where(and(eq(tournamentParticipants.tournamentId, tournamentId), eq(tournamentParticipants.userId, usrId)));
    if (existing.length > 0) return undefined;
    
    const [participant] = await db.insert(tournamentParticipants)
      .values({ tournamentId, userId: usrId })
      .returning();
    return participant;
  }

  async getTournamentLeaderboard(tournamentId: number): Promise<TournamentParticipant[]> {
    return await db.select().from(tournamentParticipants)
      .where(eq(tournamentParticipants.tournamentId, tournamentId))
      .orderBy(desc(tournamentParticipants.score));
  }

  async updateTournamentScore(tournamentId: number, usrId: number, scoreToAdd: number): Promise<void> {
    await db.update(tournamentParticipants)
      .set({ score: sql`${tournamentParticipants.score} + ${scoreToAdd}` })
      .where(and(eq(tournamentParticipants.tournamentId, tournamentId), eq(tournamentParticipants.userId, usrId)));
  }

  // Daily Bonuses
  async getLastDailyBonus(usrId: number): Promise<DailyBonus | undefined> {
    const [bonus] = await db.select().from(dailyBonuses)
      .where(eq(dailyBonuses.userId, usrId))
      .orderBy(desc(dailyBonuses.claimedAt))
      .limit(1);
    return bonus;
  }

  async claimDailyBonus(usrId: number, day: number, reward: any): Promise<DailyBonus> {
    const [bonus] = await db.insert(dailyBonuses)
      .values({ userId: usrId, day, reward })
      .returning();
    return bonus;
  }

  // Referrals
  async createReferral(referrerId: number, referredId: number): Promise<Referral> {
    const [referral] = await db.insert(referrals)
      .values({ referrerId, referredId })
      .returning();
    return referral;
  }

  async getReferrals(referrerId: number): Promise<Referral[]> {
    return await db.select().from(referrals)
      .where(eq(referrals.referrerId, referrerId))
      .orderBy(desc(referrals.createdAt));
  }

  async claimReferralReward(referralId: number): Promise<boolean> {
    const [referral] = await db.update(referrals)
      .set({ rewardClaimed: true })
      .where(and(eq(referrals.id, referralId), eq(referrals.rewardClaimed, false)))
      .returning();
    return !!referral;
  }

  // Challenges
  async getActiveChallenges(): Promise<Challenge[]> {
    const now = new Date();
    return await db.select().from(challenges)
      .where(and(eq(challenges.isActive, true), lte(challenges.startDate, now), gte(challenges.endDate, now)));
  }

  async getChallengeProgress(challengeId: number, usrId: number): Promise<ChallengeProgress | undefined> {
    const [progress] = await db.select().from(challengeProgress)
      .where(and(eq(challengeProgress.challengeId, challengeId), eq(challengeProgress.userId, usrId)));
    return progress;
  }

  async updateChallengeProgress(challengeId: number, usrId: number, progressToAdd: number, target: number): Promise<ChallengeProgress> {
    const existing = await this.getChallengeProgress(challengeId, usrId);
    
    if (!existing) {
      const newProgress = Math.min(progressToAdd, target);
      const [progress] = await db.insert(challengeProgress)
        .values({ challengeId, userId: usrId, progress: newProgress, completed: newProgress >= target })
        .returning();
      return progress;
    }
    
    const newProgress = Math.min(existing.progress + progressToAdd, target);
    const [progress] = await db.update(challengeProgress)
      .set({ progress: newProgress, completed: newProgress >= target, updatedAt: new Date() })
      .where(and(eq(challengeProgress.challengeId, challengeId), eq(challengeProgress.userId, usrId)))
      .returning();
    return progress;
  }

  async claimChallengeReward(challengeId: number, usrId: number): Promise<boolean> {
    const [progress] = await db.update(challengeProgress)
      .set({ rewardClaimed: true })
      .where(and(
        eq(challengeProgress.challengeId, challengeId),
        eq(challengeProgress.userId, usrId),
        eq(challengeProgress.completed, true),
        eq(challengeProgress.rewardClaimed, false)
      ))
      .returning();
    return !!progress;
  }

  async getUserChallengeProgress(usrId: number): Promise<ChallengeProgress[]> {
    return await db.select().from(challengeProgress)
      .where(eq(challengeProgress.userId, usrId));
  }

  // Replays
  async saveReplay(data: { userId: number; title?: string; replayData: any; duration?: number; isPublic?: boolean }): Promise<Replay> {
    const [replay] = await db.insert(replays).values({
      userId: data.userId,
      title: data.title || null,
      replayData: data.replayData,
      duration: data.duration || null,
      isPublic: data.isPublic || false,
    }).returning();
    return replay;
  }

  async getUserReplays(usrId: number): Promise<Replay[]> {
    return await db.select().from(replays)
      .where(eq(replays.userId, usrId))
      .orderBy(desc(replays.createdAt));
  }

  async getPublicReplays(limit: number = 50): Promise<Replay[]> {
    return await db.select().from(replays)
      .where(eq(replays.isPublic, true))
      .orderBy(desc(replays.likes), desc(replays.createdAt))
      .limit(limit);
  }

  async getReplay(id: number): Promise<Replay | undefined> {
    const [replay] = await db.update(replays)
      .set({ views: sql`${replays.views} + 1` })
      .where(eq(replays.id, id))
      .returning();
    return replay;
  }

  async deleteReplay(id: number, usrId: number): Promise<boolean> {
    const result = await db.delete(replays)
      .where(and(eq(replays.id, id), eq(replays.userId, usrId)))
      .returning();
    return result.length > 0;
  }

  // Map Decorations
  async getMapDecoration(deviceId: string, mapId: string) {
    const [decoration] = await db.select().from(mapDecorations)
      .where(and(eq(mapDecorations.deviceId, deviceId), eq(mapDecorations.mapId, mapId)));
    return decoration;
  }

  async saveMapDecoration(
    deviceId: string, 
    mapId: string, 
    placedModels: any, 
    backgroundSettings: any,
    wallSettings?: any,
    wallSegmentConfigs?: any,
    innerWallSegmentConfigs?: any,
    zoneSettings?: any,
    zoneEditorConfigs?: any,
    elementTransforms?: any
  ) {
    const existing = await this.getMapDecoration(deviceId, mapId);
    if (existing) {
      const [updated] = await db.update(mapDecorations)
        .set({ 
          placedModels, 
          backgroundSettings,
          wallSettings,
          wallSegmentConfigs,
          innerWallSegmentConfigs,
          zoneSettings,
          zoneEditorConfigs,
          elementTransforms,
          updatedAt: new Date() 
        })
        .where(and(eq(mapDecorations.deviceId, deviceId), eq(mapDecorations.mapId, mapId)))
        .returning();
      return updated;
    } else {
      const [created] = await db.insert(mapDecorations).values({
        deviceId,
        mapId,
        placedModels,
        backgroundSettings,
        wallSettings,
        wallSegmentConfigs,
        innerWallSegmentConfigs,
        zoneSettings,
        zoneEditorConfigs,
        elementTransforms,
      }).returning();
      return created;
    }
  }

  async getMapDecorationsByDevice(deviceId: string) {
    return await db.select().from(mapDecorations)
      .where(eq(mapDecorations.deviceId, deviceId));
  }
}

export const storage = new DatabaseStorage();
