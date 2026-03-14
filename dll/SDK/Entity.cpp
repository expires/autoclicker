#include "Entity.h"
#include "Mappings.h"

Entity::Entity(jobject instance)
{
    this->instance = instance;
}

jclass Entity::GetClass()
{
    return lc->GetClass(MC_Entity);
}

void Entity::Cleanup()
{
    lc->env->DeleteLocalRef(this->instance);
}

jobject Entity::GetInstance()
{
    return this->instance;
}

Component Entity::getName()
{
    jmethodID name = lc->env->GetMethodID(this->GetClass(),
        MTD_Entity_getName, DESC_Entity_getName);

    jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), name);

    return Component(rtn);
}

Component Entity::getTypeName()
{
    jmethodID typeName = lc->env->GetMethodID(this->GetClass(),
        MTD_Entity_getTypeName, DESC_Entity_getTypeName);

    jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), typeName);

    return Component(rtn);
}

double Entity::getX()
{
    jclass cls = this->GetClass();
    if (cls == nullptr) return 0.0;
    jmethodID m = lc->env->GetMethodID(cls, MTD_Entity_getX, "()D");
    if (m == nullptr) { lc->env->ExceptionClear(); return 0.0; }
    return lc->env->CallDoubleMethod(this->GetInstance(), m);
}

double Entity::getY()
{
    jclass cls = this->GetClass();
    if (cls == nullptr) return 0.0;
    jmethodID m = lc->env->GetMethodID(cls, MTD_Entity_getY, "()D");
    if (m == nullptr) { lc->env->ExceptionClear(); return 0.0; }
    return lc->env->CallDoubleMethod(this->GetInstance(), m);
}

double Entity::getZ()
{
    jclass cls = this->GetClass();
    if (cls == nullptr) return 0.0;
    jmethodID m = lc->env->GetMethodID(cls, MTD_Entity_getZ, "()D");
    if (m == nullptr) { lc->env->ExceptionClear(); return 0.0; }
    return lc->env->CallDoubleMethod(this->GetInstance(), m);
}
