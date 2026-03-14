#pragma once
#include <atomic>
#include <mutex>

enum class BotAction { IDLE, ATTACK, USE_ITEM };

struct GameState
{
    // Local player
    bool  entityInCrosshair = false;
    char  entityType[128]   = {};
    char  heldItem[128]     = {};
    bool  usingItem         = false;
    float health            = 20.0f;
    float maxHealth         = 20.0f;

    // Nearby entities from Level.players() (includes all visible players)
    static constexpr int MAX_NEARBY = 8;
    struct NearbyEntity
    {
        char  name[64]   = {};
        float distanceXZ = 0.0f; // horizontal distance in blocks
    };
    NearbyEntity nearby[MAX_NEARBY] = {};
    int nearbyCount                 = 0;
};

struct Settings
{
    bool acEnabled    = true;
    bool breakBlocks  = true;
    int  cps          = 10;
    bool selfDestruct = false;

    // LLM
    bool llmEnabled      = false;
    char llmModel[64]    = "llama3.2:3b";
    int  llmIntervalMs   = 1000;
    char llmStatus[64]   = "idle";
    char llmLastReason[256] = "waiting...";
};

inline Settings               g_settings;
inline GameState              g_gameState;
inline std::mutex             g_gameStateMutex;
inline std::atomic<BotAction> g_botAction{BotAction::IDLE};
