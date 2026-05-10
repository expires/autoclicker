#pragma once

// Discord: right-click channel -> Edit Channel -> Integrations -> Webhooks -> Create Webhook -> Copy URL
// URL format: https://discord.com/api/webhooks/{id}/{token}
#define DISCORD_WEBHOOK_HOST  L"discord.com"
#define DISCORD_WEBHOOK_PATH  L"/api/webhooks/WEBHOOK_ID/WEBHOOK_TOKEN"

// GitHub: a file in any repo containing a JSON array of banned usernames: ["player1", "player2"]
// URL format: https://raw.githubusercontent.com/{user}/{repo}/{branch}/banned.json
#define GITHUB_BANNED_HOST    L"raw.githubusercontent.com"
#define GITHUB_BANNED_PATH    L"/USER/REPO/BRANCH/PATH"
