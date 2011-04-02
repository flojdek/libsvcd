#include "libservdisc.h"

#include <cassert>

int main()
{
	NodesSearch nodes_finder;

	// Try load config file that doesn't exist.

	bool was_exception = false;
	try
	{
		nodes_finder.ReadConfigFile("./data/some/not/existing/path");
	}
	catch (std::runtime_error& ex) 
	{
		was_exception = true;	
	}
	assert(was_exception);

	// Load config file that exists, should end with success.

	try
	{
		nodes_finder.ReadConfigFile("./data/configfile");
	}
	catch (std::runtime_error& ex)
	{
		assert(false);
	}

	assert(nodes_finder.GetPort() == 8000);
	assert(nodes_finder.GetMaxRetries() == 4);
	assert(nodes_finder.GetRetryTimeout() == 6);
	assert(nodes_finder.GetMinRetryInterval() == 1);
	assert(nodes_finder.GetMaxRetryInterval() == 2);

	return 0;
}
