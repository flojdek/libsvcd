#include "svcd_nodesfinder.h"

#include <cassert>

int main()
{
    svcd::NodesFinder finder;

    assert(finder.GetPort() == 6666);
    assert(finder.GetMaxRetries() == 3);
    assert(finder.GetRetryTimeout() == 5);
    assert(finder.GetMinRetryInterval() == 1);
    assert(finder.GetMaxRetryInterval() == 5);
    assert(finder.GetMaxMessageSize() == 1000);

    return 0;
}
