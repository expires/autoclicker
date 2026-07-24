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

    ItemStack getArmorItem(int index);

    bool isUsingItem();

    float getHealth();
    float getMaxHealth();
};
