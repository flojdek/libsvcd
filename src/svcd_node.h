#ifndef SVCD_NODE_H
#define SVCD_NODE_H

#include <string>

namespace svcd 
{

/**
 * Represents a discovered node.
 */
class Node
{
public:
    Node(const char* address);
    Node(const char* address, const char* payload);

    const std::string& Payload() const;
    bool IsPayloadEmpty() const;

    const std::string& Address() const;
    bool IsAddressEmpty() const;

private:
    std::string m_address; /**< Address e.g. IPv4. */
    std::string m_payload; /**< Additional information from node. */
};

bool operator<(const Node& lhs, const Node& rhs);

}

#endif
