#pragma once

#include <cstdint>
#include <cstddef>

namespace Service
{
    constexpr uint32_t fnv1a(const char *str, size_t len)
    {
        uint32_t hash = 2166136261u;
        for (size_t i = 0; i < len; ++i)
        {
            hash ^= static_cast<uint8_t>(str[i]);
            hash *= 16777619u;
        }
        return hash;
    }
    class StringId
    {
    public:
        typedef std::uint32_t stringid_t;
        constexpr bool operator==(const StringId &o) const { return id_ == o.id_; }
        constexpr bool operator!=(const StringId &o) const { return id_ != o.id_; }

        constexpr uint32_t id() const { return id_; }
        constexpr const char *str() const { return str_; }

        constexpr StringId() : id_(0), str_(nullptr) {}
        constexpr StringId(uint32_t id, const char *str) : id_(id), str_(str) {}

    private:
        std::uint32_t id_;
        const char *str_;
    };
}

using namespace Service;

/*
    the following are a mirror from NaughtyDog's string id access mechanism
*/
constexpr StringId operator""_sid(const char *str, size_t len)
{
    return StringId(fnv1a(str, len), str);
}

#define SID(s) (s##_sid)