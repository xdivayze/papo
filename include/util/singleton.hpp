#pragma once
template <typename T>
class Singleton
{
public:
    static T &get()
    {
        static T inst;
        return inst;
    }

    Singleton(const Singleton &) = delete;
    Singleton &operator=(const Singleton &) = delete;

protected:
    Singleton() = default;
};