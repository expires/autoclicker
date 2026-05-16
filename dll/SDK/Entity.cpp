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

    if (!name || lc->env->ExceptionCheck())
    {
        lc->env->ExceptionClear();
        return Component(nullptr);
    }

    jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), name);

    if (lc->env->ExceptionCheck())
    {
        lc->env->ExceptionClear();
        return Component(nullptr);
    }

    return Component(rtn);
}

std::string Entity::getUUID()
{
    jmethodID getUUIDMethod = lc->env->GetMethodID(this->GetClass(), MTD_Entity_getUUID, DESC_Entity_getUUID);
    if (!getUUIDMethod || lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return ""; }

    jobject uuidObj = lc->env->CallObjectMethod(this->GetInstance(), getUUIDMethod);
    if (!uuidObj || lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return ""; }

    jclass uuidClass = lc->env->GetObjectClass(uuidObj);
    jmethodID toStringMethod = lc->env->GetMethodID(uuidClass, "toString", "()Ljava/lang/String;");
    if (!toStringMethod) { lc->env->ExceptionClear(); return ""; }

    jstring javaString = (jstring)lc->env->CallObjectMethod(uuidObj, toStringMethod);
    if (!javaString || lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return ""; }

    const char* chars = lc->env->GetStringUTFChars(javaString, nullptr);
    std::string result(chars ? chars : "");
    lc->env->ReleaseStringUTFChars(javaString, chars);
    return result;
}

Component Entity::getTypeName()
{
    jmethodID typeName = lc->env->GetMethodID(this->GetClass(),
        MTD_Entity_getTypeName, DESC_Entity_getTypeName);

    jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), typeName);

    return Component(rtn);
}

// 1.21.11+: Entity.x/y/z no longer exist as primitive fields; position is a
// Vec3 reference on Entity. Each accessor reads the Vec3 and pulls a coord.
Vec3 Entity::getPosition()
{
    jfieldID f = lc->env->GetFieldID(this->GetClass(), FLD_Entity_position, DESC_Entity_position);
    jobject v = lc->env->GetObjectField(this->instance, f);
    return Vec3(v);
}
double Entity::getX() { return getPosition().getX(); }
double Entity::getY() { return getPosition().getY(); }
double Entity::getZ() { return getPosition().getZ(); }

float Entity::getYRot()
{
    jfieldID f = lc->env->GetFieldID(this->GetClass(), FLD_Entity_yRot, "F");
    return lc->env->GetFloatField(this->instance, f);
}
float Entity::getXRot()
{
    jfieldID f = lc->env->GetFieldID(this->GetClass(), FLD_Entity_xRot, "F");
    return lc->env->GetFloatField(this->instance, f);
}

AABB Entity::getBoundingBox()
{
    jmethodID m = lc->env->GetMethodID(this->GetClass(),
        MTD_Entity_getBoundingBox, DESC_Entity_getBoundingBox);
    jobject b = lc->env->CallObjectMethod(this->instance, m);
    return AABB(b);
}

bool Entity::setGlowingTag(bool glowing)
{
    // Direct field write to Entity.hasGlowingTag (a local-only boolean). Avoids
    // the setter method entirely so anything that method does beyond the field
    // assignment (e.g. setting the synced flag bit) is bypassed. isCurrentlyGlowing
    // ORs hasGlowingTag with the synced flag, so writing this field alone is
    // sufficient on the client side.
    jfieldID f = lc->env->GetFieldID(this->GetClass(), FLD_Entity_hasGlowingTag, "Z");
    if (!f) { lc->env->ExceptionClear(); return false; }
    lc->env->SetBooleanField(this->instance, f, (jboolean)glowing);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return false; }
    return true;
}
