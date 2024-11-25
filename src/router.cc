#include "router.hh"
#include "ipv4_datagram.hh"

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address
// against prefix_length: For this route to be applicable, how many high-order (most-significant)
// bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination
//    address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to
// the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  Entry entry = std::make_pair( next_hop, interface_num );
  table.add_route( route_prefix, prefix_length, entry );
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing
// interface.
void Router::route()
{
  for ( auto& interface : _interfaces ) {
    auto& received_datagrams = interface->datagrams_received();
    while ( !received_datagrams.empty() ) {
      InternetDatagram datagram = received_datagrams.front();
      received_datagrams.pop();
      if ( datagram.header.ttl <= 1 )
        continue;
      std::optional<Entry> result = table.route( datagram.header.dst );
      if ( result.has_value() ) {
        datagram.header.ttl--;
        datagram.header.compute_checksum();
        Entry entry = result.value();
        Address next_hop
          = entry.first.value_or( Address::from_ipv4_numeric( datagram.header.dst ) );
        _interfaces[entry.second]->send_datagram( datagram, next_hop );
      }
    }
  }
}

void ip_table::add_route( uint32_t route_prefix, uint8_t prefix_length, Entry& entry )
{
  node* cur = root.get();

  for ( uint8_t i = 0; i < prefix_length; ++i ) {
    uint8_t bit = ( route_prefix >> ( 31 - i ) ) & 1;

    if ( !cur->next[bit] ) {
      cur->next[bit] = std::make_unique<node>();
      cur->next[bit]->depth = cur->depth + 1;
    }

    cur = cur->next[bit].get();
  }

  cur->hold = true;
  cur->entry = std::move( entry );
}

std::optional<Entry> ip_table::route( uint32_t ip_address )
{
  node* cur = root.get();
  uint8_t depth = 0;
  auto ret = std::make_optional<Entry>();

  while ( depth < 32 ) {
    uint8_t bit = ( ip_address >> ( 31 - depth ) ) & 1;

    if ( cur->hold )
      ret = std::make_optional<Entry>( cur->entry );

    if ( !cur->next[bit] )
      break;

    cur = cur->next[bit].get();
    depth++;
  }

  return ret;
}
