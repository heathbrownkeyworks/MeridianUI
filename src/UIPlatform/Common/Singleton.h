#pragma once

namespace Meridian::Common
{
    template<class T>
    class Singleton
    {
    protected:
        Singleton() = default;
        virtual ~Singleton() = default;

    public:
        static T& GetSingleton()
        {
            static T instance;
            return instance;
        }

        Singleton(Singleton const&) = delete;
        Singleton& operator=(Singleton const&) = delete;
    };
}
