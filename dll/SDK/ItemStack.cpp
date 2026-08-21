#include "ItemStack.h"
#include "Mappings.h"

ItemStack::ItemStack(jobject instance)
{
    this->instance = instance;
}

jclass ItemStack::GetClass()
{
    static jclass c = nullptr;
    return JClass(c, MC_ItemStack);
}

void ItemStack::Cleanup()
{
    lc->env->DeleteLocalRef(this->instance);
}

jobject ItemStack::GetInstance()
{
    return this->instance;
}

Item ItemStack::getItem()
{
    if (this->instance == nullptr) return Item(nullptr);
    static jmethodID getItem = nullptr;
    JMethod(getItem, this->GetClass(), MTD_ItemStack_getItem, DESC_ItemStack_getItem);
    if (!getItem) { lc->env->ExceptionClear(); return Item(nullptr); }
    jobject rtn = lc->env->CallObjectMethod(this->instance, getItem);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return Item(nullptr); }
    return Item(rtn);
}

Component ItemStack::getHoverName()
{
    if (this->instance == nullptr) return Component(nullptr);
    static jmethodID m = nullptr;
    JMethod(m, this->GetClass(), MTD_ItemStack_getHoverName, DESC_ItemStack_getHoverName);
    if (!m) { lc->env->ExceptionClear(); return Component(nullptr); }
    jobject rtn = lc->env->CallObjectMethod(this->instance, m);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return Component(nullptr); }
    return Component(rtn);
}

static std::string ReadJavaString(jobject owner, jmethodID m)
{
    jstring js = (jstring)lc->env->CallObjectMethod(owner, m);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return ""; }
    if (js == nullptr) return "";
    const char* chars = lc->env->GetStringUTFChars(js, nullptr);
    std::string out(chars ? chars : "");
    if (chars) lc->env->ReleaseStringUTFChars(js, chars);
    lc->env->DeleteLocalRef(js);
    return out;
}

std::string ItemStack::getDescriptionId()
{
    if (this->instance == nullptr) return "";

    if (MTD_ItemStack_getDescriptionId[0] != '\0') {
        static jmethodID m = nullptr;
        JMethod(m, this->GetClass(), MTD_ItemStack_getDescriptionId, "()Ljava/lang/String;");
        if (m) return ReadJavaString(this->instance, m);
        lc->env->ExceptionClear();
    }

    if (MTD_Item_getDescriptionId[0] != '\0') {
        Item item = this->getItem();
        if (item.GetInstance() == nullptr) return "";
        static jmethodID m = nullptr;
        JMethod(m, item.GetClass(), MTD_Item_getDescriptionId, "()Ljava/lang/String;");
        if (!m) { lc->env->ExceptionClear(); return ""; }
        std::string out = ReadJavaString(item.GetInstance(), m);
        lc->env->DeleteLocalRef(item.GetInstance());
        return out;
    }

    return "";
}
