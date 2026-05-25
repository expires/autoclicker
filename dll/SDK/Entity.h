#pragma once
#include "Lunar.h"
#include "Component.h"
#include "AABB.h"
#include "Vec3.h"
#include <cstdint>
#include <utility>
#include <vector>

class Entity
{
public:
    Entity(jobject instance);

    virtual jclass GetClass();

    void Cleanup();

    jobject GetInstance();

    Component getName();

    std::string getUUID();

    Component getTypeName();

    // Returns the team-formatted display name decomposed into colored chunks
    // (e.g. {"[Clan] ", 0xFF5555FF}, {"manu", 0xFFFFFFFF}). Walks the formatted
    // Component's siblings; each sibling becomes one chunk with its Style's
    // color. Colors are full ARGB (alpha forced to 0xFF). Empty vector means
    // we couldn't read the name at all.
    std::vector<std::pair<std::string, uint32_t>> getFormattedNameChunks();

    // 1.21.11+: position is a Vec3 reference, not three primitive doubles.
    // getX/Y/Z dereference the Vec3 each call; for hot loops fetch the Vec3 once
    // via getPosition() and read x/y/z off it.
    Vec3 getPosition();
    double getX();
    double getY();
    double getZ();

    // Previous-tick position fields, used for render-time interpolation.
    // MC renders entities at lerp(partialTick, [xyz]o, [xyz]).
    double getXo();
    double getYo();
    double getZo();

    // Yaw / pitch fields directly on Entity.
    float getYRot();
    float getXRot();

    AABB getBoundingBox();

    // Raw Entity.getTeam() jobject (or nullptr). PlayerTeams are scoreboard
    // singletons per name, so JNIEnv::IsSameObject is enough to test "same
    // team" — no need to read the team's name out. Caller owns the local ref;
    // a PushLocalFrame around the loop is the simplest cleanup.
    jobject getTeamRaw();

private:
    jobject instance;
};
