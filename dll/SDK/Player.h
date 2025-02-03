#pragma once
#include "Lunar.h"
#include "LivingEntity.h"
#include "ItemStack.h"

class Player : public LivingEntity
{
public:
	Player(jobject instance) : LivingEntity(instance), instance(instance) {};

	jclass GetClass() override;

private:
	jobject instance;
};
