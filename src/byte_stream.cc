#include "byte_stream.hh"

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

bool Writer::is_closed() const
{
  return closed_;
}

void Writer::push( std::string data )
{
  uint64_t to_push = std::min( data.size(), available_capacity() );
  if ( to_push < data.size() )
    data.erase( to_push ); // Keep only the part to be moved
  buffer_.append( std::move( data ) );
  pushed_ += to_push;
}

void Writer::close()
{
  closed_ = true;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - buffer_.size();
}

uint64_t Writer::bytes_pushed() const
{
  return pushed_;
}

bool Reader::is_finished() const
{
  return buffer_.empty() && closed_;
}

uint64_t Reader::bytes_popped() const
{
  return popped_;
}

std::string_view Reader::peek() const
{
  return std::string_view( buffer_ );
}

void Reader::pop( uint64_t len )
{
  uint64_t to_pop = std::min( len, buffer_.size() );
  buffer_.erase( 0, to_pop );
  popped_ += to_pop;
}

uint64_t Reader::bytes_buffered() const
{
  return buffer_.size();
}
