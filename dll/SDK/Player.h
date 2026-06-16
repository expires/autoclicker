#pragma once
#include "Lunar.h"
#include "LivingEntity.h"
#include "Inventory.h"

class Player : public LivingEntity
{
public:
	Player(jobject instance) : LivingEntity(instance) {};

	jclass GetClass() override;

	Inventory getInventory();
};
