#include "Component.h"
#include "Mappings.h"

Component::Component(jobject instance)
{
    this->instance = instance;
}

jclass Component::GetClass()
{
    static jclass c = nullptr;
    return JClass(c, MC_Component);
}

void Component::Cleanup()
{
    lc->env->DeleteLocalRef(this->instance);
}

jobject Component::GetInstance()
{
    return this->instance;
}
