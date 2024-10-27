#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <cstdint>
#include <functional>
#include <queue>

class Timer
{
public:
  Timer() = default;
  bool started() const { return started_; }
  void stop() { started_ = false; }
  void restart()
  {
    time_ = 0;
    started_ = true;
  }
  void tick( uint64_t ms )
  {
    if ( started_ )
      time_ += ms;
  }
  bool expired( uint64_t ms ) const { return started_ && time_ >= ms; }

private:
  bool started_ { false };
  uint64_t time_ { 0 };
};

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), initial_RTO_ms_( initial_RTO_ms )
  {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send
   * messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method
   * was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight()
    const; // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions()
    const; // How many consecutive *re*transmissions have happened?
  Writer& writer() { return input_.writer(); }
  const Writer& writer() const { return input_.writer(); }

  // Access input stream reader, but const-only (can't read from outside)
  const Reader& reader() const { return input_.reader(); }

private:
  // Variables initialized in constructor
  ByteStream input_;
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;

  // Helper functions and variables
  struct Segment
  {
    bool SYN { false };
    bool FIN { false };
    bool RST { false };
    uint64_t seqno { 0 };
    std::string data {};
    size_t length() const { return SYN + data.size() + FIN; }
  };
  void transmit_wrapper( Segment& seg,
                         const TransmitFunction& transmit,
                         bool track = true );
  std::queue<Segment> buffer_ {};
  Timer timer {};
  uint64_t RTO_ratio_ { 1 };
  uint64_t ack_base_ { 0 };
  uint64_t seq_current_ { 0 };
  uint16_t window_size_ { 1 }; // Assume window size is 1 before SYN
  uint64_t consecutive_retransmissions_ { 0 };
};
