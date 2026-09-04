#pragma once

#include <cstddef>

namespace Meridian::Common
{
    /// Moves one sink to the front without changing the relative order of
    /// any other sink. The container only needs size() and operator[].
    template <class Container, class Sink>
    bool PromoteInputSink(Container& a_sinks, const Sink& a_sink)
    {
        using Size = typename Container::size_type;
        Size index = 0;
        for (; index < a_sinks.size(); ++index)
        {
            if (a_sinks[index] == a_sink)
            {
                break;
            }
        }

        if (index == a_sinks.size())
        {
            return false;
        }

        for (; index > 0; --index)
        {
            a_sinks[index] = a_sinks[index - 1];
        }
        a_sinks[0] = a_sink;
        return true;
    }

    /// Gives Meridian's language switch sink first refusal, followed by the
    /// browser router. External sinks retain their original relative order.
    template <class Container, class Sink>
    void PromoteInputSinks(Container& a_sinks,
                           const Sink& a_router,
                           const Sink& a_languageSwitch)
    {
        PromoteInputSink(a_sinks, a_router);
        PromoteInputSink(a_sinks, a_languageSwitch);
    }
}
