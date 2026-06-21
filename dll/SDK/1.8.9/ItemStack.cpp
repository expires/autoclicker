#include "ItemStack.h"
#include "Mappings.h"

bool ItemStack::isEmpty()
{
    if (this->instance == nullptr) return true;
    return false;
}
