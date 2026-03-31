#include "services/string_manager.hpp"
#include "utils/exception.hpp"
constexpr const char *TAG = "String Manager";

namespace Service
{

    //    returns the string that matches the key "id" in the lookup table
    const char *StringManager::getStrFromID(StringId::stringid_t id)
    {
        auto it = stringIDTable_.find(id);
        if (it == stringIDTable_.end())
            throw Util::LogicException(TAG, "string id not found in the lookup table");

        return it->second;
    }

    void StringManager::insertStringID(StringId &sid)
    {
        stringIDTable_[sid.id()] = sid.str();
    }

    void StringManager::removeStringID(StringId &sid)
    {
        auto it = stringIDTable_.find(sid.id());
        if (it == stringIDTable_.end())
            throw Util::LogicException(TAG, "string id not found in the lookup table");

        stringIDTable_.erase(it);
    }

}