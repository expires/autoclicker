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

    std::vector<std::pair<std::string, uint32_t>> getFormattedNameChunks();

    Vec3 getPosition();
    double getX();
    double getY();
    double getZ();

    double getXo();
    double getYo();
    double getZo();

    float getYRot();
    float getXRot();

    AABB getBoundingBox();

    jobject getTeamRaw();

private:
    jobject instance;
};
