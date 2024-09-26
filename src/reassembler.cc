#include "reassembler.hh"

void Reassembler::insert( uint64_t first_index, std::string data, bool is_last_substring )
{
  scope avaliable_scope = std::make_pair( index_, index_ + output_.writer().available_capacity() );
  scope data_scope = std::make_pair( first_index, first_index + data.size() );

  if ( is_last_substring && !received_last_ ) {
    last_index_ = data_scope.second;
    received_last_ = true;
  }

  if ( data_scope.first >= avaliable_scope.second || data_scope.second <= avaliable_scope.first )
    goto check_close;

  // truncate data to fit in available scope
  if ( data_scope.second > avaliable_scope.second ) {
    data.erase( avaliable_scope.second - data_scope.first );
    data_scope.second = avaliable_scope.second;
  }
  if ( data_scope.first < avaliable_scope.first ) {
    data.erase( 0, avaliable_scope.first - data_scope.first );
    data_scope.first = avaliable_scope.first;
  }

  for ( auto it = buffer_.begin(); it != buffer_.end(); ) {
    const auto& item = *it;
    const auto& item_scope = item.first;
    const auto& item_data = item.second;
    // check if data and item are disjoint
    if ( data_scope.first > item.first.second || data_scope.second < item.first.first ) {
      ++it;
      continue;
    }
    // merge data with item
    std::string merged_data {};
    if ( item_scope.first < data_scope.first ) {
      merged_data = std::move( item_data );
      if ( item_scope.second < data_scope.second ) {
        merged_data.append( data, item_scope.second - data_scope.first, data_scope.second - item_scope.second );
      }
    } else {
      merged_data = std::move( data );
      if ( item_scope.second > data_scope.second ) {
        merged_data.append(
          item_data, data_scope.second - item_scope.first, item_scope.second - data_scope.second );
      }
    }
    data = std::move( merged_data );
    data_scope.first = std::min( item_scope.first, data_scope.first );
    data_scope.second = std::max( item_scope.second, data_scope.second );

    pending_ -= item_scope.second - item_scope.first;
    it = buffer_.erase( it ); // Erase and move to the next element
  }

  // write data
  if ( data_scope.first == index_ ) {
    output_.writer().push( data );
    index_ = data_scope.second;
  } else {
    buffer_.insert( std::make_pair( data_scope, data ) );
    pending_ += data.size();
  }

check_close:
  if ( received_last_ && index_ == last_index_ ) {
    output_.writer().close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  return pending_;
}
