#pragma once

#include <memory>
#include <optional>

#include "exception.hh"
#include "network_interface.hh"

using Entry = std::pair<std::optional<Address>, size_t>;

class ip_table
{
public:
  void add_route( uint32_t route_prefix, uint8_t prefix_length, Entry& entry );

  std::optional<Entry> route( uint32_t ip_address );

private:
  struct node
  {
    uint32_t depth = 0;
    std::unique_ptr<node> next[2];
    bool hold = false;
    Entry entry {};
  };
  std::unique_ptr<node> root = std::make_unique<node>();
};

// \brief A router that has multiple network interfaces and
// performs longest-prefix-match routing between them.
class Router
{
public:
  // Add an interface to the router
  // \param[in] interface an already-constructed network interface
  // \returns The index of the interface after it has been added to the router
  size_t add_interface( std::shared_ptr<NetworkInterface> interface )
  {
    _interfaces.push_back( notnull( "add_interface", std::move( interface ) ) );
    return _interfaces.size() - 1;
  }

  // Access an interface by index
  std::shared_ptr<NetworkInterface> interface( const size_t N ) { return _interfaces.at( N ); }

  // Add a route (a forwarding rule)
  void add_route( uint32_t route_prefix,
                  uint8_t prefix_length,
                  std::optional<Address> next_hop,
                  size_t interface_num );

  // Route packets between the interfaces
  void route();

private:
  // The router's collection of network interfaces
  std::vector<std::shared_ptr<NetworkInterface>> _interfaces {};
  ip_table table {};
};
