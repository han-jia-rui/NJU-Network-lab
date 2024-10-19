#include "tcp_receiver.hh"
#include "byte_stream.hh"

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST ) {
    reader().set_error();
    return;
  }

  if ( ( !SYN_received_ && !message.SYN ) || writer().has_error() )
    return;

  if ( message.SYN ) {
    SYN_received_ = true;
    zero_point = Wrap32 { message.seqno };
  }

  uint64_t stream_index
    = message.SYN ? 0 : message.seqno.unwrap( zero_point, abs_seqno_ ) - 1;

  reassembler_.insert( stream_index, message.payload, message.FIN );
  abs_seqno_ = 1 + writer().bytes_pushed() + writer().is_closed();
}

TCPReceiverMessage TCPReceiver::send() const
{
  return TCPReceiverMessage {
    .ackno = SYN_received_ ? std::make_optional( Wrap32::wrap( abs_seqno_, zero_point ) )
                           : std::nullopt,
    .window_size = static_cast<uint16_t>(
      std::min( static_cast<uint64_t>( 0xffff ), writer().available_capacity() ) ),
    .RST = writer().has_error(),
  };
}
