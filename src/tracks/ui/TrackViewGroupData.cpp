/**********************************************************************

Audacity: A Digital Audio Editor

TrackViewGroupData.cpp

Paul Licameli split from class Track

**********************************************************************/

#include "TrackViewGroupData.h"
#include "../../Track.h"

// Supply the default implementation of ViewGroupData
template<> auto CreateViewGroupData::Implementation() -> Function {
   return []( TrackGroupData &host ) {
      return std::make_unique< TrackViewGroupData >( host );
   };
}
static CreateViewGroupData registerMe;

static TrackGroupData::Extensions::RegisteredFactory sKey {
   []( TrackGroupData &groupData ){
      // dispatches on runtime type of groupData to make appropriate
      // subclass of TrackViewGroupData
      return CreateViewGroupData::Call( groupData );
   }
};

TrackViewGroupData::TrackViewGroupData( TrackGroupData &host )
   : Base( &host )
{
}

TrackViewGroupData::~TrackViewGroupData()
{
}

auto TrackViewGroupData::Clone() const -> PointerType
{
   return std::make_unique< TrackViewGroupData >( *this );
}

TrackViewGroupData &TrackViewGroupData::Get( Track &track )
{
   auto &data = track.GetGroupData();
   return data.Extensions::Get< TrackViewGroupData >( sKey );
}

const TrackViewGroupData
&TrackViewGroupData::Get( const Track &track )
{
   // May create the data structure on demend but not change it if present
   return Get( const_cast< Track& >( track ) );
}
