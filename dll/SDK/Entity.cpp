#include "Entity.h"
#include "Mappings.h"

Entity::Entity(jobject instance)
{
    this->instance = instance;
}

jclass Entity::GetClass()
{
    static jclass c = nullptr;
    return JClass(c, MC_Entity);
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
    static jmethodID name = nullptr;
    JMethod(name, this->GetClass(), MTD_Entity_getName, DESC_Entity_getName);

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
    static jmethodID getUUIDMethod = nullptr;
    JMethod(getUUIDMethod, this->GetClass(), MTD_Entity_getUUID, DESC_Entity_getUUID);
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
    static jmethodID typeName = nullptr;
    JMethod(typeName, this->GetClass(), MTD_Entity_getTypeName, DESC_Entity_getTypeName);

    jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), typeName);

    return Component(rtn);
}

double Entity::getXo()
{
    static jfieldID f = nullptr;
    if (!JField(f, this->GetClass(), FLD_Entity_xo, "D")) return 0.0;
    return lc->env->GetDoubleField(this->instance, f);
}
double Entity::getYo()
{
    static jfieldID f = nullptr;
    if (!JField(f, this->GetClass(), FLD_Entity_yo, "D")) return 0.0;
    return lc->env->GetDoubleField(this->instance, f);
}
double Entity::getZo()
{
    static jfieldID f = nullptr;
    if (!JField(f, this->GetClass(), FLD_Entity_zo, "D")) return 0.0;
    return lc->env->GetDoubleField(this->instance, f);
}

float Entity::getYRot()
{
    static jfieldID f = nullptr;
    if (!JField(f, this->GetClass(), FLD_Entity_yRot, "F")) return 0.0f;
    return lc->env->GetFloatField(this->instance, f);
}
float Entity::getXRot()
{
    static jfieldID f = nullptr;
    if (!JField(f, this->GetClass(), FLD_Entity_xRot, "F")) return 0.0f;
    return lc->env->GetFloatField(this->instance, f);
}

AABB Entity::getBoundingBox()
{
    static jmethodID m = nullptr;
    if (!JMethod(m, this->GetClass(), MTD_Entity_getBoundingBox, DESC_Entity_getBoundingBox)) return AABB(nullptr);
    jobject b = lc->env->CallObjectMethod(this->instance, m);
    return AABB(b);
}

jobject Entity::getTeamRaw()
{
    static jmethodID m = nullptr;
    JMethod(m, this->GetClass(), MTD_Entity_getTeam, DESC_Entity_getTeam);
    if (!m) { lc->env->ExceptionClear(); return nullptr; }
    jobject t = lc->env->CallObjectMethod(this->instance, m);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return nullptr; }
    return t;
}
