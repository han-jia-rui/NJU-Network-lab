#pragma once

#include <cstdint>
#include <map>
#include <queue>

#include "address.hh"
#include "ethernet_frame.hh"
#include "ipv4_datagram.hh"

template<typename T>
class TimerWrapper
{
public:
  TimerWrapper( T pass_in ) : item( pass_in ), time_count( 0 ) {}

  TimerWrapper() = default;

  void add( const size_t ms ) { time_count += ms; }

  bool expired( const size_t max_ms ) { return time_count >= max_ms; }

  T get() { return item; }

private:
  T item;
  size_t time_count;
};

// A "network interface" that connects IP (the internet layer, or network layer)
// with Ethernet (the network access layer, or link layer).

// This module is the lowest layer of a TCP/IP stack
// (connecting IP with the lower-layer network protocol,
// e.g. Ethernet). But the same module is also used repeatedly
// as part of a router: a router generally has many network
// interfaces, and the router's job is to route Internet datagrams
// between the different interfaces.

// The network interface translates datagrams (coming from the
// "customer," e.g. a TCP/IP stack or router) into Ethernet
// frames. To fill in the Ethernet destination address, it looks up
// the Ethernet address of the next IP hop of each datagram, making
// requests with the [Address Resolution Protocol](\ref rfc::rfc826).
// In the opposite direction, the network interface accepts Ethernet
// frames, checks if they are intended for it, and if so, processes
// the the payload depending on its type. If it's an IPv4 datagram,
// the network interface passes it up the stack. If it's an ARP
// request or reply, the network interface processes the frame
// and learns or replies as necessary.
class NetworkInterface
{
  using IPv4Address = uint32_t;

public:
  // Anabstraction for the physical output port where the NetworkInterface sends Ethernet
  // frames
  class OutputPort
  {
  public:
    virtual void transmit( const NetworkInterface& sender, const EthernetFrame& frame ) = 0;
    virtual ~OutputPort() = default;
  };

  // Construct a network interface with given Ethernet (network-access-layer) and IP
  // (internet-layer) addresses
  NetworkInterface( std::string_view name,
                    std::shared_ptr<OutputPort> port,
                    const EthernetAddress& ethernet_address,
                    const Address& ip_address );

  // Sends an Internet datagram, encapsulated in an Ethernet frame (if it knows the
  // Ethernet destination address). Will need to use [ARP](\ref rfc::rfc826) to look up
  // the Ethernet destination address for the next hop. Sending is accomplished by calling
  // `transmit()` (a member variable) on the frame.
  void send_datagram( const InternetDatagram& dgram, const Address& next_hop );

  // Receives an Ethernet frame and responds appropriately.
  // If type is IPv4, pushes the datagram to the datagrams_in queue.
  // If type is ARP request, learn a mapping from the "sender" fields, and send an ARP
  // reply. If type is ARP reply, learn a mapping from the "sender" fields.
  void recv_frame( const EthernetFrame& frame );

  // Called periodically when time elapses
  void tick( size_t ms_since_last_tick );

  // Construct a Ethernet frame from
  EthernetFrame encapsulate( const std::vector<std::string>& payload,
                             const EthernetAddress& dest_address,
                             const uint16_t type ) const;

  // Query the Ethernet address for a specific IPv4 address
  void send_arp_message( const IPv4Address& dest_ip,
                         const EthernetAddress& dest_ethernet,
                         const uint16_t opcode ) const;

  // Accessors
  const std::string& name() const { return name_; }
  const OutputPort& output() const { return *port_; }
  OutputPort& output() { return *port_; }
  std::queue<InternetDatagram>& datagrams_received() { return datagrams_received_; }

private:
  // Human-readable name of the interface
  std::string name_;

  // The physical output port (+ a helper function `transmit` that uses it to send an
  // Ethernet frame)
  std::shared_ptr<OutputPort> port_;
  void transmit( const EthernetFrame& frame ) const { port_->transmit( *this, frame ); }

  // Ethernet (known as hardware, network-access-layer, or link-layer) address of the
  // interface
  EthernetAddress ethernet_address_;

  // IP (known as internet-layer or network-layer) address of the interface
  Address ip_address_;

  // Datagrams that have been received
  std::queue<InternetDatagram> datagrams_received_ {};

  // Datagrams with unknown Ethernet address
  std::multimap<IPv4Address, InternetDatagram> data_queued_ {};

  static constexpr size_t max_waited_time_ms = 5000;
  // IPv4 addresses that have been queried with ARP
  std::map<IPv4Address, size_t> arp_waited_ {};

  static constexpr size_t max_cached_time_ms = 30000;
  // Map from IPv4 address to Ethernet address
  std::map<IPv4Address, TimerWrapper<EthernetAddress>> arp_table_ {};
};
