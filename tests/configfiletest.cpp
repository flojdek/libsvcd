#include <svcd_nodesfinder.h>

#include <cassert>

int main()
{
    svcd::NodesFinder finder;

    // Try load config file that doesn't exist.

    bool was_exception = false;
    try
    {
        finder.ReadConfigFile("./data/some/not/existing/path");
    }
    catch (std::runtime_error& ex)
    {
        was_exception = true;
    }
    assert(was_exception);

    // Load config file that exists, should end with success.

    try
    {
        finder.ReadConfigFile("./data/configfile");
    }
    catch (std::runtime_error& ex)
    {
        assert(false);
    }

    assert(finder.GetPort() == 8000);
    assert(finder.GetMaxRetries() == 4);
    assert(finder.GetRetryTimeout() == 6);
    assert(finder.GetMinRetryInterval() == 1);
    assert(finder.GetMaxRetryInterval() == 2);

    return 0;
}
