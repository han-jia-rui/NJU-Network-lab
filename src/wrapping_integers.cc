#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32 { zero_point + static_cast<uint32_t>( n ) };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  Wrap32 check_point = Wrap32::wrap( checkpoint, zero_point );

  uint32_t diff = check_point.raw_value_ - raw_value_;
  if ( diff <= 0x7FFFFFFF && checkpoint >= diff )
    return checkpoint - diff;

  return checkpoint + ( raw_value_ - check_point.raw_value_ );
}
