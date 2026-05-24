#pragma once
#include "Lunar.h"
#include "Entity.h"
#include "ItemStack.h"

class LivingEntity : public Entity
{
public:
    LivingEntity(jobject instance) : Entity(instance) {};

    virtual jclass GetClass() override;

    ItemStack getItemInHand();

    bool isUsingItem();

    // Current and max HP in MC's float units (1 heart = 2 HP). Returns -1
    // if the JNI lookup fails or the entity isn't a LivingEntity — caller
    // treats negative as "unknown" so we never render a fake 0/0.
    float getHealth();
    float getMaxHealth();
};
