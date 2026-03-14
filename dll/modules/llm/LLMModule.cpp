#include "LLMModule.h"
#include "LLMClient.h"
#include "../../Settings.h"
#include <sstream>
#include <cstring>

// Extract first string value for key from a JSON object
static std::string extractJsonStr(const std::string& json, const std::string& key)
{
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return {};
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return {};
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return {};
    ++pos;
    std::string result;
    while (pos < json.size() && json[pos] != '"')
    {
        if (json[pos] == '\\' && pos + 1 < json.size())
        {
            ++pos;
            switch (json[pos])
            {
            case '"':  result += '"';  break;
            case '\\': result += '\\'; break;
            case 'n':  result += '\n'; break;
            default:   result += json[pos];
            }
        }
        else
        {
            result += json[pos];
        }
        ++pos;
    }
    return result;
}

static constexpr const char* SYSTEM_PROMPT =
    "You are a Minecraft combat bot used for anticheat research. "
    "Given the game state, choose one action. "
    "Respond with valid JSON ONLY, no other text. "
    "Format: {\"action\":\"attack\",\"reason\":\"brief\"} "
    "or {\"action\":\"use_item\",\"reason\":\"brief\"} "
    "or {\"action\":\"idle\",\"reason\":\"brief\"}. "
    "Attack hostile mobs when they are in crosshair. "
    "Use item if a food/potion is held and health is low. "
    "Otherwise idle.";

namespace LLMModule
{

std::atomic<bool> running{false};

DWORD WINAPI init(LPVOID)
{
    running = true;

    while (running && !g_settings.selfDestruct)
    {
        if (!g_settings.llmEnabled)
        {
            g_botAction = BotAction::IDLE;
            strncpy_s(g_settings.llmStatus, "disabled", sizeof(g_settings.llmStatus) - 1);
            Sleep(500);
            continue;
        }

        // Snapshot current game state (written by autoclicker thread which owns JNI)
        GameState gs;
        {
            std::lock_guard<std::mutex> lock(g_gameStateMutex);
            gs = g_gameState;
        }

        // Build natural-language game state description for the LLM
        std::ostringstream prompt;
        prompt << "Health: " << gs.health << "/" << gs.maxHealth << "\n";
        prompt << "Entity in crosshair: "
               << (gs.entityInCrosshair ? gs.entityType : "none") << "\n";
        prompt << "Held item: "
               << (gs.heldItem[0] ? gs.heldItem : "empty hand") << "\n";
        prompt << "Using item: " << (gs.usingItem ? "true" : "false") << "\n";

        if (gs.nearbyCount > 0)
        {
            prompt << "Nearby players:";
            for (int i = 0; i < gs.nearbyCount; ++i)
                prompt << " " << gs.nearby[i].name
                       << " (" << gs.nearby[i].distanceXZ << "m)";
            prompt << "\n";
        }
        else
        {
            prompt << "Nearby players: none\n";
        }

        strncpy_s(g_settings.llmStatus, "querying...", sizeof(g_settings.llmStatus) - 1);

        std::string content = LLMClient::Chat(
            g_settings.llmModel,
            SYSTEM_PROMPT,
            prompt.str());

        if (content.empty())
        {
            strncpy_s(g_settings.llmStatus, "no response", sizeof(g_settings.llmStatus) - 1);
            g_botAction = BotAction::IDLE;
        }
        else
        {
            std::string action = extractJsonStr(content, "action");
            std::string reason = extractJsonStr(content, "reason");

            if (!reason.empty())
                strncpy_s(g_settings.llmLastReason, reason.c_str(),
                          sizeof(g_settings.llmLastReason) - 1);

            if (action == "attack")
                g_botAction = BotAction::ATTACK;
            else if (action == "use_item")
                g_botAction = BotAction::USE_ITEM;
            else
                g_botAction = BotAction::IDLE;

            strncpy_s(g_settings.llmStatus,
                      action.empty() ? "parse error" : action.c_str(),
                      sizeof(g_settings.llmStatus) - 1);
        }

        Sleep(static_cast<DWORD>(g_settings.llmIntervalMs));
    }

    running = false;
    return 0;
}

} // namespace LLMModule
