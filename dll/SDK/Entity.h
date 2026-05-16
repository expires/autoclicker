#pragma once
#include "Lunar.h"
#include "Component.h"
#include "AABB.h"
#include "Vec3.h"

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

    // Returns the team-formatted display string (e.g. "[Clan] manu") the same
    // way MC's renderer composes it: pulls the bare name via getName(), looks
    // up the player's scoreboard team via getTeam(), and if non-null pipes
    // through PlayerTeam.formatNameForTeam to prepend the prefix and append
    // the suffix. Falls back to bare name on any JNI failure or null team.
    std::string getFormattedName();

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

    // Client-side only: flips Entity.hasGlowingTag. isCurrentlyGlowing() reads
    // this and the engine renders the outline. Not synced to server.
    // Returns true if the JNI lookup found the method and the call dispatched.
    bool setGlowingTag(bool glowing);

private:
    jobject instance;
};
