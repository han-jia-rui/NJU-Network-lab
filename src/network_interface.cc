#include <cstdint>
#include <iostream>

#include "arp_message.hh"
#include "ethernet_header.hh"
#include "exception.hh"
#include "network_interface.hh"

using namespace std;

NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address )
       << " and IP address " << ip_address.ip() << "\n";
}

EthernetFrame NetworkInterface::encapsulate( const std::vector<std::string>& payload,
                                             const EthernetAddress& dest_address,
                                             const uint16_t type ) const
{
  EthernetHeader header {
    .dst = dest_address,
    .src = ethernet_address_,
    .type = type,
  };

  EthernetFrame frame {
    .header = std::move( header ),
    .payload = payload,
  };

  return frame;
}

void NetworkInterface::send_arp_message( const IPv4Address& dest_ip,
                                         const EthernetAddress& dest_ethernet,
                                         const uint16_t opcode ) const
{
  ARPMessage arp_message {
    .opcode = opcode,
    .sender_ethernet_address = ethernet_address_,
    .sender_ip_address = ip_address_.ipv4_numeric(),
    .target_ethernet_address
    = opcode == ARPMessage::OPCODE_REQUEST ? ETHERNET_DEFAULT : dest_ethernet,
    .target_ip_address = dest_ip,
  };
  EthernetFrame arp_frame
    = encapsulate( serialize( arp_message ), dest_ethernet, EthernetHeader::TYPE_ARP );
  transmit( arp_frame );
}

void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  IPv4Address dst_ip = next_hop.ipv4_numeric();
  if ( arp_table_.contains( dst_ip ) ) {
    EthernetFrame ipv4_frame
      = encapsulate( serialize( dgram ), arp_table_[dst_ip].get(), EthernetHeader::TYPE_IPv4 );
    transmit( ipv4_frame );
  } else {
    data_queued_.insert( std::make_pair( dst_ip, dgram ) );
    if ( not arp_waited_.contains( dst_ip ) ) {
      send_arp_message( dst_ip, ETHERNET_BROADCAST, ARPMessage::OPCODE_REQUEST );
      arp_waited_[dst_ip] = 0;
    }
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  if ( frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST )
    return;

  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram datagram {};
    if ( parse( datagram, frame.payload ) ) {
      datagrams_received_.push( std::move( datagram ) );
    }
  }

  if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    ARPMessage arp_message {};
    if ( parse( arp_message, frame.payload ) ) {
      IPv4Address ipv4_address = arp_message.sender_ip_address;
      arp_table_[ipv4_address] = arp_message.sender_ethernet_address;

      if ( data_queued_.contains( ipv4_address ) ) {
        auto range = data_queued_.equal_range( ipv4_address );
        for ( auto it = range.first; it != range.second; it++ )
          send_datagram( it->second, Address::from_ipv4_numeric( it->first ) );
        data_queued_.erase( ipv4_address );
      }

      if ( arp_message.opcode == ARPMessage::OPCODE_REQUEST
           && arp_message.target_ip_address == ip_address_.ipv4_numeric() ) {
        send_arp_message( arp_message.sender_ip_address,
                          arp_message.sender_ethernet_address,
                          ARPMessage::OPCODE_REPLY );
      }
    }
  }
}

/**
 * @brief [Update the time]
 * @param ms_since_last_tick [the time since the last tick]
 */
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  for ( auto it = arp_waited_.begin(); it != arp_waited_.end(); ) {
    it->second += ms_since_last_tick;
    if ( it->second >= max_waited_time_ms )
      it = arp_waited_.erase( it );
    else
      ++it;
  }

  for ( auto it = arp_table_.begin(); it != arp_table_.end(); ) {
    it->second.add( ms_since_last_tick );
    if ( it->second.expired( max_cached_time_ms ) )
      it = arp_table_.erase( it );
    else
      ++it;
  }
}
