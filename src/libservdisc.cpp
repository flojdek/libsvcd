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
#include <stdexcept>

#define QUOTEME(x) #x
#define QUOTEME2(x) QUOTEME(x)
#define THROW(ex, msg) (throw ex(__FILE__ ":" QUOTEME2(__LINE__) ": " msg))
#define THROW_IF_ERROR(ret, ex, msg) if ((ret) < 0) { THROW(ex, msg); }
#define THROW_IF_TRUE(ret, ex, msg) if (ret) { THROW(ex, msg); }

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
	try
	{
		SearchAux();
	}
	catch (std::exception& ex)
	{
		NotifyAllOnSearchError();
		throw ex;
	}
}

void NodesSearch::SearchAux()
{
	NotifyAllOnSearchStarted();

	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	THROW_IF_ERROR(sockfd, std::runtime_error, "can't create socket");

	const int value = 1; 
	int ret = setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &value, sizeof(value));
	THROW_IF_ERROR(ret, std::runtime_error, "can't set option SO_BROADCAST on socket");
	ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
	THROW_IF_ERROR(ret, std::runtime_error, "can't set option SO_REUSEADDR on socket");

	SA_IN broadcast_addr;
	memset(&broadcast_addr, 0, sizeof(broadcast_addr));
	broadcast_addr.sin_family = AF_INET;
	broadcast_addr.sin_port = htons(m_port);
	broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

	ret = pipe(pipefd); 
	THROW_IF_ERROR(ret, std::runtime_error, "can't create pipe");

	void (*pf_ret)(int);
	pf_ret = signal(SIGALRM, AlarmHandler);
	THROW_IF_TRUE(pf_ret == SIG_ERR, std::runtime_error, "can't set SIGALRM handler");

	// We'll be pushing difference of nodes_just_found and nodes_all
	// to the listeners via OnSearchPushNodes.

	std::set<Node> nodes_all;
	std::set<Node> nodes_just_found;
	std::vector<Node> nodes_diff;

	for (int i = 0; i < m_max_retries; ++i)
	{
		nodes_just_found.clear();
		nodes_diff.clear();

		if (i > 0)
			sleep(GetNextRetrySleepInterval());

		// If sendto will return an error we try again after some time interval.

		int ret = sendto(sockfd, m_payload.data(), m_payload.size(), 0, 
			reinterpret_cast<SA*>(&broadcast_addr), sizeof(broadcast_addr));
		if (ret < 0)
			continue;

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

				THROW_IF_TRUE(errno != EINTR, std::runtime_error, "select error");
			}

			THROW_IF_TRUE(FD_ISSET(sockfd, &errorfds) || FD_ISSET(pipefd[0], &errorfds),
				std::runtime_error, "error condition occured on socket or pipe");

			if (FD_ISSET(sockfd, &readfds))
			{
				SA_IN node_addr;
				socklen_t length = sizeof(node_addr);

				char* msg_buff = new char [m_max_message_size];
				memset(msg_buff, 0, m_max_message_size);

				int ret = recvfrom(sockfd, msg_buff, m_max_message_size - 1, 0, 
					reinterpret_cast<SA*>(&node_addr), &length);
				if (ret >= 0)
				{
					char addr_buff[MAX_ADDRESS_SIZE] = {0};
					if (inet_ntop(AF_INET, &node_addr.sin_addr, addr_buff, sizeof(addr_buff)) != 0)
						nodes_just_found.insert(Node(addr_buff, msg_buff));
				}

				delete [] msg_buff;
			}

			if (FD_ISSET(pipefd[0], &readfds))
			{
				ret = read(pipefd[0], &ret, 1); 
				THROW_IF_ERROR(ret, std::runtime_error, "can't read from pipe");
				break;
			}
		}

		// Remove duplicates, push to listeners if new nodes were found.

		std::set_difference(nodes_just_found.begin(), nodes_just_found.end(),
			nodes_all.begin(), nodes_all.end(), std::inserter(nodes_diff, nodes_diff.end()));
		for (int i = 0; i < nodes_diff.size(); ++i)
			nodes_all.insert(nodes_diff[i]);

		NotifyAllOnSearchPushNodes(nodes_diff);
	}

	NotifyAllOnSearchFinished();
}

void NodesSearch::ReadConfigFile(const char* path)
{
	std::ifstream cfg_file_path(path);
	THROW_IF_TRUE(cfg_file_path.fail(), std::runtime_error, "i/o error with config file");
	po::variables_map vm;
	po::store(po::parse_config_file(cfg_file_path, m_opts_desc), vm);
	po::notify(vm);
}

void NodesSearch::SetPayload(const char* payload)
{
	if (strlen(payload) > m_max_message_size)
		THROW(std::length_error, "payload length exceeds maximum allowable size");	
	
	m_payload = payload;
}

unsigned NodesSearch::GetNextRetrySleepInterval() const
{
	return rand() % (m_max_retry_interval - m_min_retry_interval + 1) + 
		m_min_retry_interval;
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
