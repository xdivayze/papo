#pragma once
#include "../collections/robin_hood.hpp"

#include "string_id.hpp"

namespace Service
{
    class StringManager
    {
    public:
        const char *getStrFromID(StringId::stringid_t id);
        void insertStringID(StringId &sid);
        void removeStringID(StringId &sid);

    private:
        robin_hood::unordered_flat_map<StringId::stringid_t, const char *> stringIDTable_;
    };
}