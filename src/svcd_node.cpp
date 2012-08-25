#include <svcd_node.h>

namespace svcd
{

Node::Node(const char* address)
: m_address(address)
{
}

Node::Node(const char* address, const char* payload)
: m_address(address)
, m_payload(payload)
{
}

const std::string& Node::Address() const
{
    return m_address;
}

bool Node::IsAddressEmpty() const
{
    return m_address.empty();
}

const std::string& Node::Payload() const
{
    return m_payload;
}

bool Node::IsPayloadEmpty() const
{
    return m_payload.empty();
}

bool operator<(const Node& lhs, const Node& rhs)
{
    return lhs.Address() < rhs.Address();
}

}
