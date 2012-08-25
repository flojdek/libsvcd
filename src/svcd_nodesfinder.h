#ifndef SVCD_NODESFINDER_H
#define SVCD_NODESFINDER_H

#include <svcd_node.h>

#include <list>
#include <vector>
#include <string>

#include <boost/program_options.hpp>

#include <netinet/in.h>
#include <sys/socket.h>

namespace svcd
{

namespace po = boost::program_options;

/**
 * @brief Nodes discovery in local network.
 *
 * NodesFinder does UDP broadcasting to discover nodes on local network.
 * Users of this class can be notified about nodes that have been just
 * discovered via listeners - as soon as new nodes have been found they
 * are pushed to listeners with OnSearchPushNodes call. Duplicates are removed,
 * so that means only new nodes are pushed. Class isn't re-entrant for now.
 */
class NodesFinder
{
public:

    typedef sockaddr SA;
    typedef sockaddr_in SA_IN;

    struct Listener
    {
        /**
         * Called after search has started.
         */
        virtual void OnSearchStarted() = 0;

        /**
         * Called when search is finished.
         */
        virtual void OnSearchFinished() = 0;

        /**
         * Called as soon as new nodes have been found.
         *
         * @param nodes Nodes that have been just found.
         */
        virtual void OnSearchPushNodes(const std::vector<Node>& nodes) = 0;

        /**
         * Called when there's a for example system call error,
         * out of memory error etc.
         */
        virtual void OnSearchError() = 0;
    };

    NodesFinder();

    void AddListener(Listener* listener) { m_listeners.push_back(listener); }
    void RemoveListener(Listener* listener) { m_listeners.remove(listener); }

    /**
     * Start searching nodes. Notifications will be send
     * to listeners during the call.
     *
     * @throws std::runtime_error
     */
    void Search();

    /**
     * Most of search parameters can be set in configuration
     * file, this function reads such file and when invoked
     * options from configuration file override defaults.
     *
     * @param path Path to configuration file.
     * @throws std::runtime_error
     */
    void ReadConfigFile(const char* path);

    /**
     * @throws std::length_error
     */
    void SetPayload(const char* payload);
    const char* GetPayload() const { return m_payload.c_str(); }

    void SetPort(unsigned port) { m_port = port; }
    unsigned GetPort() const { return m_port; }

    void SetMaxRetries(unsigned max) { m_max_retries = max; }
    unsigned GetMaxRetries() const { return m_max_retries; }

    void SetRetryTimeout(unsigned sec) { m_retry_timeout = sec; }
    unsigned GetRetryTimeout() const { return m_retry_timeout; }

    void SetMaxRetryInterval(unsigned sec) { m_max_retry_interval = sec; }
    unsigned GetMaxRetryInterval() const { return m_max_retry_interval; }

    void SetMinRetryInterval(unsigned sec) { m_min_retry_interval = sec; }
    unsigned GetMinRetryInterval() const { return m_min_retry_interval; }

    void SetMaxMessageSize(unsigned size) { m_max_message_size = size; }
    unsigned GetMaxMessageSize() const { return m_max_message_size; }

private:

    static const unsigned DEFAULT_MAX_MESSAGE_SIZE = 1000;
    static const unsigned DEFAULT_SEARCH_PORT = 6666;
    static const unsigned DEFAULT_MAX_RETRIES = 3;
    static const unsigned DEFAULT_MAX_RETRY_INTERVAL = 2;
    static const unsigned DEFAULT_MIN_RETRY_INTERVAL = 1;
    static const unsigned DEFAULT_RETRY_TIMEOUT = 3;
    static const unsigned MAX_ADDRESS_SIZE = 128;

    std::list<Listener*> m_listeners;
    std::string m_payload;
    po::options_description m_opts_desc;

    unsigned m_port;
    unsigned m_max_retries;
    unsigned m_retry_timeout;
    unsigned m_min_retry_interval;
    unsigned m_max_retry_interval;
    unsigned m_max_message_size;

    /**
     * Auxiliary call for NodesFinder::Search.
     *
     * @throws std::runtime_error
     * @see NodesFinder::Search
     */
    void SearchAux();

    unsigned GetNextRetrySleepInterval() const;
    void NotifyAllOnSearchStarted();
    void NotifyAllOnSearchFinished();
    void NotifyAllOnSearchPushNodes(const std::vector<Node>& nodes);
    void NotifyAllOnSearchError();
};

}

#endif
