#include <svcd_nodesfinder.h>

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
#include <functional>

#include <boost/scoped_array.hpp>

namespace
{
    template <typename L, typename N>
    void Notify(L listeners, N notification)
    {
        std::for_each(listeners.begin(), listeners.end(), notification);
    }

    template <typename N>
    std::vector<N> Difference(const std::set<N>& lhs, const std::set<N>& rhs)
    {
        std::vector<N> diff;
        std::set_difference(lhs.begin(), lhs.end(),
                            rhs.begin(), rhs.end(),
                            std::inserter(diff, diff.end()));
        return diff;
    }
}

namespace svcd
{

NodesFinder::NodesFinder()
: m_payload("")
, m_port(DEFAULT_SEARCH_PORT)
, m_max_retries(DEFAULT_MAX_RETRIES)
, m_retry_timeout(DEFAULT_RETRY_TIMEOUT)
, m_min_retry_interval(DEFAULT_MIN_RETRY_INTERVAL)
, m_max_retry_interval(DEFAULT_MAX_RETRY_INTERVAL)
, m_max_message_size(DEFAULT_MAX_MESSAGE_SIZE)
{
    m_opts_desc.add_options()
        ("Discovery.Port", po::value<unsigned>(&m_port))
        ("Discovery.MaxRetries", po::value<unsigned>(&m_max_retries))
        ("Discovery.RetryTimeout", po::value<unsigned>(&m_retry_timeout))
        ("Discovery.MinRetryInterval", po::value<unsigned>(&m_min_retry_interval))
        ("Discovery.MaxRetryInterval", po::value<unsigned>(&m_max_retry_interval));
}

void NodesFinder::Search()
{
    try
    {
        SearchAux();
    }
    catch (std::exception& ex)
    {
        NotifyAllOnSearchError(ex.what());
        throw ex;
    }
}

std::string NodesFinder::ErrorMsg(const std::string& prefix)
{
    std::ostringstream err;
    err << prefix << " error: " << strerror(errno);
    return err.str();
}

void NodesFinder::SearchAux()
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        throw std::runtime_error(ErrorMsg("socket()"));
    }

    int opt = 1;
    int ret = setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
    if (ret < 0)
    {
        throw std::runtime_error(ErrorMsg("setsockopt(SO_BROADCAST)"));
    }

    opt = 1;
    ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (ret < 0)
    {
        throw std::runtime_error(ErrorMsg("setsockopt(SO_REUSEADDR)"));
    }

    sockaddr_in broadcast_addr     = {0};
    broadcast_addr.sin_family      = AF_INET;
    broadcast_addr.sin_port        = htons(m_port);
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    std::set<Node> nodes_all;
    std::set<Node> nodes_fnd;

    NotifyAllOnSearchStarted();

    for (int i = 0; i < m_max_retries; ++i)
    {
        nodes_fnd.clear();

        if (i > 0)
        {
            sleep(GetNextRetrySleepInterval());
        }

        int ret = sendto(sockfd,
                         m_payload.data(),
                         m_payload.size(),
                         0,
                         reinterpret_cast<sockaddr*>(&broadcast_addr),
                         sizeof(broadcast_addr));
        if (ret < 0)
        {
            continue;
        }

        timeval tv = {0};
        tv.tv_sec  = m_retry_timeout;
        tv.tv_usec = 0;

        fd_set readfds, errorfds;
        for (;;)
        {
            FD_ZERO(&readfds);
            FD_ZERO(&errorfds);
            FD_SET(sockfd, &readfds);
            FD_SET(sockfd, &errorfds);

            ret = select(sockfd + 1, &readfds, 0, &errorfds, &tv);
            if (ret < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                else
                {
                    throw std::runtime_error(ErrorMsg("select()"));
                }
            }
            else if (ret == 0)
            {
                break;
            }

            if (FD_ISSET(sockfd, &errorfds))
            {
                throw std::runtime_error("error condition on socket");
            }
            else if (FD_ISSET(sockfd, &readfds))
            {
                boost::scoped_array<char> msg_buffer(
                                                new char [m_max_message_size]);
                memset(msg_buffer.get(), 0, m_max_message_size);

                sockaddr_in node_addr = {0};
                socklen_t length      = sizeof(node_addr);

                ret = recvfrom(sockfd,
                               msg_buffer.get(),
                               m_max_message_size - 1,
                               0,
                               reinterpret_cast<sockaddr*>(&node_addr),
                               &length);
                if (ret < 0)
                {
                    NotifyAllOnSearchError(ErrorMsg("recvfrom()"));
                }
                else
                {
                    char addr_buffer[MAX_ADDRESS_SIZE] = {0};
                    const char* ret = inet_ntop(AF_INET,
                                                &node_addr.sin_addr,
                                                addr_buffer,
                                                MAX_ADDRESS_SIZE);
                    if (ret != 0)
                    {
                        nodes_fnd.insert(Node(addr_buffer, msg_buffer.get()));
                    }
                }
            }
        }

        std::vector<Node> diff = NotifyAllOnSearchPushNodes(
                                             Difference(nodes_fnd, nodes_all));
        nodes_all.insert(diff.begin(), diff.end());
    }

    NotifyAllOnSearchFinished();
}

void NodesFinder::ReadConfigFile(const char* path)
{
    std::ifstream cfg_file_path(path);
    if (cfg_file_path.fail())
    {
        throw std::runtime_error("error reading config file");
    }

    po::variables_map vm;
    po::store(po::parse_config_file(cfg_file_path, m_opts_desc), vm);
    po::notify(vm);
}

void NodesFinder::SetPayload(const char* payload)
{
    if (strlen(payload) > m_max_message_size)
    {
        throw std::length_error("payload length exceeds maximum allowable size");
    }
    m_payload = payload;
}

unsigned NodesFinder::GetNextRetrySleepInterval() const
{
    unsigned diff = m_max_retry_interval - m_min_retry_interval + 1;
    return rand() % diff + m_min_retry_interval;
}

void NodesFinder::NotifyAllOnSearchStarted()
{
    Notify(m_listeners, std::mem_fun(&Listener::OnSearchStarted));
}

void NodesFinder::NotifyAllOnSearchFinished()
{
    Notify(m_listeners, std::mem_fun(&Listener::OnSearchFinished));
}

const std::vector<Node>& NodesFinder::NotifyAllOnSearchPushNodes(
                                                const std::vector<Node>& nodes)
{
    Notify(m_listeners,
           std::bind2nd(std::mem_fun(&Listener::OnSearchPushNodes), nodes));
    return nodes;
}

std::string NodesFinder::NotifyAllOnSearchError(const std::string& what)
{
    Notify(m_listeners,
           std::bind2nd(std::mem_fun(&Listener::OnSearchError), what));
    return what;
}

}
