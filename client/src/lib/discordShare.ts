export interface DiscordShareData {
  title: string;
  description: string;
  url?: string;
  imageUrl?: string;
  fields?: { name: string; value: string; inline?: boolean }[];
}

export async function shareToDiscord(webhookUrl: string, data: DiscordShareData): Promise<boolean> {
  try {
    const embed = {
      title: data.title,
      description: data.description,
      url: data.url,
      color: 0x5865F2,
      timestamp: new Date().toISOString(),
      footer: {
        text: "Zoogi Roll Arena"
      },
      thumbnail: data.imageUrl ? { url: data.imageUrl } : undefined,
      fields: data.fields || []
    };

    const response = await fetch(webhookUrl, {
      method: "POST",
      headers: {
        "Content-Type": "application/json"
      },
      body: JSON.stringify({
        embeds: [embed]
      })
    });

    return response.ok;
  } catch (error) {
    console.error("Error sharing to Discord:", error);
    return false;
  }
}

export function createScoreShareData(playerName: string, score: number, zoogiUsed: string): DiscordShareData {
  return {
    title: `New High Score!`,
    description: `**${playerName}** just scored **${score.toLocaleString()}** points!`,
    fields: [
      { name: "Zoogi Used", value: zoogiUsed, inline: true },
      { name: "Score", value: score.toLocaleString(), inline: true }
    ]
  };
}

export function createReplayShareData(title: string, playerName: string, duration: number, replayUrl: string): DiscordShareData {
  const mins = Math.floor(duration / 60);
  const secs = duration % 60;
  const durationStr = `${mins}:${secs.toString().padStart(2, "0")}`;
  
  return {
    title: `Replay Shared: ${title}`,
    description: `**${playerName}** shared an epic battle replay!`,
    url: replayUrl,
    fields: [
      { name: "Duration", value: durationStr, inline: true },
      { name: "Watch Now", value: `[Click here](${replayUrl})`, inline: true }
    ]
  };
}

export function createTournamentResultData(
  tournamentName: string, 
  playerName: string, 
  rank: number, 
  score: number
): DiscordShareData {
  const rankEmoji = rank === 1 ? "" : rank === 2 ? "" : rank === 3 ? "" : `#${rank}`;
  
  return {
    title: `Tournament Result: ${tournamentName}`,
    description: `**${playerName}** finished ${rankEmoji} with **${score.toLocaleString()}** points!`,
    fields: [
      { name: "Final Rank", value: `${rankEmoji} #${rank}`, inline: true },
      { name: "Score", value: score.toLocaleString(), inline: true }
    ]
  };
}

export function generateDiscordInvite(guildId?: string): string {
  if (guildId) {
    return `https://discord.gg/${guildId}`;
  }
  return "";
}
