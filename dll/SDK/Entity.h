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

    // 1.21.11+: position is a Vec3 reference, not three primitive doubles.
    // getX/Y/Z dereference the Vec3 each call; for hot loops fetch the Vec3 once
    // via getPosition() and read x/y/z off it.
    Vec3 getPosition();
    double getX();
    double getY();
    double getZ();

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
