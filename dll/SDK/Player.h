#pragma once
#include "Lunar.h"
#include "LivingEntity.h"
#include "Inventory.h"

class Player : public LivingEntity
{
public:
	Player(jobject instance) : LivingEntity(instance) {};

	jclass GetClass() override;

	// Player.getInventory() — needed by macros to walk hotbar slots by display
	// name. Returns Inventory(nullptr) if the JNI lookup fails.
	Inventory getInventory();
};
