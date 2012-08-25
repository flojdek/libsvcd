#include "svcd_nodesfinder.h"

#include <cassert>

int main()
{
    svcd::NodesFinder finder;

    // Too big payload.

    char* payload = new (std::nothrow) char [finder.GetMaxMessageSize() + 1];
    assert(payload);
    memset(payload, 1, finder.GetMaxMessageSize() + 1);

    bool was_exception = false;
    try
    {
        finder.SetPayload(payload);
    }
    catch (std::length_error& ex)
    {
        was_exception = true;
    }
    assert(was_exception);

    assert(std::string(finder.GetPayload()) == "");

    // Exact payload.

    payload[finder.GetMaxMessageSize()] = 0;

    try
    {
        finder.SetPayload(payload);
    }
    catch (std::length_error& ex)
    {
        assert(false);
    }

    return 0;
}
