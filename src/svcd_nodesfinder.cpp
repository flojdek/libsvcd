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
    template <typename T>
    void zero(T* x)
    {
        memset(x, 0, sizeof(T));
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
        //NotifyAllOnSearchError();
        throw ex;
    }
}

std::string NodesFinder::BuildError(const std::string& prefix)
{
    std::ostringstream err;
    err << prefix << " error: " << strerror(errno);
    return err.str();
}

void NodesFinder::SearchAux()
{
    NotifyAllOnSearchStarted();

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        /*
        throw std::runtime_error(NotifyAllOnSearchError(
                                                       BuildError("socket()")));
        */
    }

    int opt = 1;
    int ret = setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
    if (ret < 0)
    {
        /*
        std::string err = BuildError("setsockopt(SOL_SOCKET, SO_BROADCAST)");
        NotifyAllOnSearchError(err);
        throw std::runtime_error(err);
        */
    }

    opt = 1;
    ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (ret < 0)
    {
        std::string err = BuildError("setsockopt(SOL_SOCKET, SO_REUSEADDR)");
        NotifyAllOnSearchError(err);
        throw std::runtime_error(err);
    }

    sockaddr_in broadcast_addr;
    zero(&broadcast_addr);

    broadcast_addr.sin_family      = AF_INET;
    broadcast_addr.sin_port        = htons(m_port);
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

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

        timeval tv;
        zero(&tv);

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
            if (ret == 0)
            {
                break;
            }
            else if (ret < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                else
                {
                    throw std::runtime_error(BuildError("select()"));
                }
            }

            if (FD_ISSET(sockfd, &errorfds))
            {
                throw std::runtime_error("error condition on socket");
            }

            if (FD_ISSET(sockfd, &readfds))
            {
                boost::scoped_array<char> msg_buffer(
                                                new char [m_max_message_size]);
                zero(msg_buffer.get());

                sockaddr_in node_addr;
                zero(&node_addr);

                socklen_t length = sizeof(node_addr);
                ret = recvfrom(sockfd,
                               msg_buffer.get(),
                               m_max_message_size - 1,
                               0,
                               reinterpret_cast<sockaddr*>(&node_addr),
                               &length);
                if (ret < 0)
                {
                    // FIXME: Log warning.
                }
                else
                {
                    char addr_buffer[MAX_ADDRESS_SIZE];
                    zero(addr_buffer);

                    const char* ret = inet_ntop(AF_INET,
                                                &node_addr.sin_addr,
                                                addr_buffer,
                                                MAX_ADDRESS_SIZE);
                    if (ret != 0)
                    {
                        nodes_just_found.insert(Node(addr_buffer,
                                                     msg_buffer.get()));
                    }
                }
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
    return rand() % (m_max_retry_interval - m_min_retry_interval + 1) +
        m_min_retry_interval;
}

void NodesFinder::NotifyAllOnSearchStarted()
{
    std::for_each(m_listeners.begin(),
                  m_listeners.end(),
                  std::mem_fun(&Listener::OnSearchStarted));
}

void NodesFinder::NotifyAllOnSearchFinished()
{
    std::for_each(m_listeners.begin(),
                  m_listeners.end(),
                  std::mem_fun(&Listener::OnSearchFinished));
}

void NodesFinder::NotifyAllOnSearchPushNodes(const std::vector<Node>& nodes)
{
    std::for_each(m_listeners.begin(),
                  m_listeners.end(),
                  std::bind2nd(std::mem_fun(&Listener::OnSearchPushNodes),
                               nodes));
}

void NodesFinder::NotifyAllOnSearchError(const std::string& what)
{
    std::for_each(m_listeners.begin(),
                  m_listeners.end(),
                  std::bind2nd(std::mem_fun(&Listener::OnSearchError),
                               what));
}

}
