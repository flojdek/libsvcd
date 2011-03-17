#include "libservdisc.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include <fstream>
#include <iostream>
#include <algorithm>
#include <set>

namespace
{
	int pipefd[2];
	void AlarmHandler(int signo) { write(pipefd[1], "", 1); } 
}

NodesSearch::NodesSearch() :
		m_payload(""),
		m_port(DEFAULT_SEARCH_PORT),
		m_max_retries(DEFAULT_MAX_RETRIES),
		m_retry_timeout(DEFAULT_RETRY_TIMEOUT),
		m_min_retry_interval(DEFAULT_MIN_RETRY_INTERVAL),
		m_max_retry_interval(DEFAULT_MAX_RETRY_INTERVAL),
		m_max_message_size(DEFAULT_MAX_MESSAGE_SIZE)
{
	m_opts_desc.add_options()
		("Discovery.Port", po::value<unsigned>(&m_port))
		("Discovery.MaxRetries", po::value<unsigned>(&m_max_retries))
		("Discovery.RetryTimeout", po::value<unsigned>(&m_retry_timeout))
		("Discovery.MinRetryInterval", po::value<unsigned>(&m_min_retry_interval))
		("Discovery.MaxRetryInterval", po::value<unsigned>(&m_max_retry_interval));
}

void NodesSearch::Search()
{
	NotifyAllOnSearchStarted();

	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	THROW_IF_ERROR(sockfd, "Can't create socket");

	const int value = 1; 
	THROW_IF_ERROR(setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &value, sizeof(value)),
			"Can't set option SO_BROADCAST on socket");
	THROW_IF_ERROR(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)),
			"Can't set option SO_REUSEADDR on socket");

	struct sockaddr_in broadcast_addr;
	memset(&broadcast_addr, 0, sizeof(broadcast_addr));
	broadcast_addr.sin_family = AF_INET;
	broadcast_addr.sin_port = htons(m_port);
	broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

	THROW_IF_ERROR(pipe(pipefd), "Can't create pipe");

	void (*pf_ret)(int);
	pf_ret = signal(SIGALRM, AlarmHandler);
	THROW_IF_TRUE(pf_ret == SIG_ERR, "Can't set SIGALRM handler");

	// We'll be pushing difference of nodes_just_found and nodes_all
	// to the listeners via OnSearchPushNodes.

	std::set<Node> nodes_all;
	std::set<Node> nodes_just_found;

	for (int i = 0; i < m_max_retries; ++i)
	{
		if (i > 0)
			sleep(rand() % (m_max_retry_interval - m_min_retry_interval + 1) + m_min_retry_interval);

		THROW_IF_ERROR(
				sendto(sockfd, m_payload.data(), m_payload.size(), 0, 
					reinterpret_cast<struct sockaddr*>(&broadcast_addr), sizeof(struct sockaddr_in)),
					"Can't send message");

		alarm(m_retry_timeout);

		fd_set readfds, errorfds;
		for (;;)
		{
			FD_ZERO(&readfds);
			FD_ZERO(&errorfds);
			FD_SET(sockfd, &readfds);
			FD_SET(sockfd, &errorfds);
			FD_SET(pipefd[0], &readfds);
			FD_SET(pipefd[0], &errorfds);

			int ret = select(std::max(sockfd, pipefd[0]) + 1, &readfds, 0, &errorfds, 0);
			if (ret < 0)
			{
				if (errno == EINTR)
					continue;

				THROW_IF_TRUE(errno != EINTR, "select error");
			}

			THROW_IF_TRUE(FD_ISSET(sockfd, &errorfds) || FD_ISSET(pipefd[0], &errorfds),
					"Error condition occured on socket or pipe");

			if (FD_ISSET(sockfd, &readfds))
			{
				struct sockaddr_in node_addr;
				socklen_t length = sizeof(node_addr);

				char* msg_buff = new char [m_max_message_size];
				memset(msg_buff, 0, m_max_message_size);

				// Receive message from peer and remember its address.

				int ret = recvfrom(sockfd, msg_buff, m_max_message_size - 1, 0, 
						reinterpret_cast<struct sockaddr*>(&node_addr), &length);
				if (ret >= 0)
				{
					char addr_buff[MAX_ADDRESS_SIZE] = {0};
					if (inet_ntop(AF_INET, &node_addr.sin_addr, addr_buff, sizeof(addr_buff)) != 0)
					{
						Node node(addr_buff, msg_buff);
						nodes_just_found.insert(node);
					}
				}

				delete [] msg_buff;
			}

			if (FD_ISSET(pipefd[0], &readfds))
			{
				THROW_IF_ERROR(read(pipefd[0], &ret, 1), "Can't read from pipe");
				break;
			}
		}

		// Remove duplicates, push to listeners if new nodes were found.

		std::vector<Node> nodes_diff;
		std::set_difference(nodes_just_found.begin(), nodes_just_found.end(),
				nodes_all.begin(), nodes_all.end(), std::inserter(nodes_diff, nodes_diff.end()));
		for (int i = 0; i < nodes_diff.size(); ++i)
			nodes_all.insert(nodes_diff[i]);

		NotifyAllOnSearchPushNodes(nodes_diff);

		nodes_just_found.clear();
		nodes_diff.clear();
	}

	NotifyAllOnSearchFinished();
}

void NodesSearch::ReadConfigFile(const char* path)
{
	std::ifstream cfg_file_path(path);
	THROW_IF_TRUE(cfg_file_path.fail(), "In/Out problem with config file");
	po::variables_map vm;
	po::store(po::parse_config_file(cfg_file_path, m_opts_desc), vm);
	po::notify(vm);
}

void NodesSearch::NotifyAllOnSearchStarted()
{
	std::list<Listener*>::iterator it;
	for (it = m_listeners.begin(); it != m_listeners.end(); ++it)
		(*it)->OnSearchStarted();
}

void NodesSearch::NotifyAllOnSearchFinished()
{
	std::list<Listener*>::iterator it;
	for (it = m_listeners.begin(); it != m_listeners.end(); ++it)
		(*it)->OnSearchFinished();
}

void NodesSearch::NotifyAllOnSearchPushNodes(const std::vector<Node>& nodes)
{
	std::list<Listener*>::iterator it;
	for (it = m_listeners.begin(); it != m_listeners.end(); ++it)
		(*it)->OnSearchPushNodes(nodes);
}

void NodesSearch::NotifyAllOnSearchError()
{
	std::list<Listener*>::iterator it;
	for (it = m_listeners.begin(); it != m_listeners.end(); ++it)
		(*it)->OnSearchError();
}
