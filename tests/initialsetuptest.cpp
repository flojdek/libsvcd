#include "libservdisc.h"

#include <cassert>

int main()
{
	NodesSearch nodes_finder;

	assert(nodes_finder.GetPort() == 6666);
	assert(nodes_finder.GetMaxRetries() == 3);
	assert(nodes_finder.GetRetryTimeout() == 5);
	assert(nodes_finder.GetMinRetryInterval() == 1);
	assert(nodes_finder.GetMaxRetryInterval() == 5);
	assert(nodes_finder.GetMaxMessageSize() == 1000);

	return 0;
}
