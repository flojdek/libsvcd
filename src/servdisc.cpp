#include "libservdisc.h"

#include <iostream>

class NodesListener : public NodesSearch::Listener
{
	virtual void OnSearchStarted()
	{
		std::cout << "Nodes search in progress.\n";
	}

	virtual void OnSearchFinished()
	{
		std::cout << "Nodes search finished.\n";
	}

	virtual void OnSearchPushNodes(const std::vector<NodesSearch::Node>& nodes)
	{
		for (int i = 0; i < nodes.size(); ++i)
			std::cout << "Found " << nodes[i].m_ip_addr << ".\n";
	}

	virtual void OnSearchError()
	{
		std::cout << "Error!\n";
	}
};

int main()
{
	NodesListener listener;
	NodesSearch nodes_finder;

	nodes_finder.AddListener(&listener);
	nodes_finder.Search();
	nodes_finder.RemoveListener(&listener);

	return 0;
}
