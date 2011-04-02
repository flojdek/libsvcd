#include "libservdisc.h"

#include <cassert>

int main()
{
	NodesSearch nodes_finder;

	// Too big payload.

	char* payload = new (std::nothrow) char [nodes_finder.GetMaxMessageSize() + 1];
	assert(payload);
	memset(payload, 1, nodes_finder.GetMaxMessageSize() + 1);

	bool was_exception = false;
	try
	{
		nodes_finder.SetPayload(payload);
	}
	catch (std::length_error& ex)
	{
		was_exception = true;
	}
	assert(was_exception);

	assert(std::string(nodes_finder.GetPayload()) == "");

	// Exact payload.

	payload[nodes_finder.GetMaxMessageSize()] = 0;

	try
	{
		nodes_finder.SetPayload(payload);
	}
	catch (std::length_error& ex)
	{
		assert(false);
	}

	assert(std::string(nodes_finder.GetPayload()) != "");

	return 0;
}
