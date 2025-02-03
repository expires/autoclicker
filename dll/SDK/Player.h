#pragma once
#include "Lunar.h"
#include "LivingEntity.h"

class Player : public LivingEntity
{
public:
	Player(jobject instance) : LivingEntity(instance) {};

	jclass GetClass() override;

};
