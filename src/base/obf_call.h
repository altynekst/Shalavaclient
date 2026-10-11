#pragma once

template<typename Ret, typename... Args>
struct SObfCall
{
    using FnType = Ret(*)(Args...);

    SObfCall(FnType fn) : m_Fn(fn) {}

    __declspec(noinline) Ret operator()(Args... args) const
    {
        volatile FnType f = m_Fn;
        return f(args...);
    }

private:
    volatile FnType m_Fn;
};

#define OBF_CALL(fn) ([&]() { \
    static volatile auto _fp = &fn; \
    return _fp; \
}())