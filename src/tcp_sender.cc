#include "tcp_sender.hh"
#include "tcp_config.hh"

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return seq_current_ - ack_base_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retransmissions_;
}

void TCPSender::transmit_wrapper( Segment& seg, const TransmitFunction& transmit, bool track )
{
  transmit( TCPSenderMessage {
    .seqno { Wrap32::wrap( seg.seqno, isn_ ) },
    .SYN = seg.SYN,
    .payload { seg.data },
    .FIN = seg.FIN,
    .RST = seg.RST,
  } );

  if ( track ) {
    seq_current_ = std::max( seq_current_, seg.seqno + seg.length() );
    buffer_.push( std::move( seg ) );
    if ( !timer.started() )
      timer.restart();
  }
}

void TCPSender::push( const TransmitFunction& transmit )
{
  Segment seg { .seqno = seq_current_ };

  if ( input_.has_error() ) {
    seg.RST = true;
    transmit_wrapper( seg, transmit, false );
    return;
  }

  Reader& reader = input_.reader();
  uint64_t seq_window = ack_base_ + ( window_size_ ? window_size_ : 1 );
  if ( seq_window < seq_current_ )
    return;
  uint64_t max_seq_size = seq_window - seq_current_;

  if ( seg.length() < max_seq_size )
    seg.SYN = ( seq_current_ == 0 );

  while ( reader.bytes_buffered() != 0 && max_seq_size > 0 ) {
    uint64_t max_data_size = std::min( TCPConfig::MAX_PAYLOAD_SIZE, max_seq_size - seg.length() );
    seg.data = reader.peek().substr( 0, max_data_size );
    reader.pop( seg.data.size() );

    if ( seg.length() < max_seq_size )
      seg.FIN = reader.is_finished();

    transmit_wrapper( seg, transmit );
    max_seq_size = seq_window - seq_current_;
    seg = { .seqno = seq_current_ };
  }

  if ( seq_current_ <= reader.bytes_popped() + 1 && seg.length() < max_seq_size )
    seg.FIN = reader.is_finished();

  if ( seg.length() > 0 )
    transmit_wrapper( seg, transmit );
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  return TCPSenderMessage {
    .seqno = Wrap32::wrap( seq_current_, isn_ ),
    .RST = input_.has_error(),
  };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( msg.RST )
    input_.set_error();

  window_size_ = msg.window_size;

  if ( msg.ackno.has_value() ) {
    uint64_t ack_no = msg.ackno.value().unwrap( isn_, ack_base_ );
    if ( ack_no > ack_base_ && ack_no <= seq_current_ ) {
      if ( RTO_ratio_ != 1 ) {
        RTO_ratio_ = 1;
        consecutive_retransmissions_ = 0;
        timer.restart();
      }

      while ( !buffer_.empty() ) {
        Segment& seg = buffer_.front();
        if ( seg.seqno + seg.length() > ack_no )
          break;
        ack_base_ = seg.seqno + seg.length();
        buffer_.pop();
        timer.restart();
      }

      if ( buffer_.empty() )
        timer.stop();
    }
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  timer.tick( ms_since_last_tick );
  if ( timer.expired( RTO_ratio_ * initial_RTO_ms_ ) ) {
    if ( window_size_ != 0 ) {
      consecutive_retransmissions_++;
      RTO_ratio_ *= 2;
    }
    timer.restart();
    transmit_wrapper( buffer_.front(), transmit, false );
  }
}
