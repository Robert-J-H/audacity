/**********************************************************************

Audacity: A Digital Audio Editor

TrackViewGroupData.h

Paul Licameli split from class Track

**********************************************************************/

#ifndef __AUDACITY_TRACK_VIEW_GROUP_DATA__
#define __AUDACITY_TRACK_VIEW_GROUP_DATA__

#include <memory>
#include "../../AttachedVirtualFunction.h"
#include "../../ClientData.h"

class Track;
class TrackGroupData;

class TrackViewGroupData /* not final */
   : public ClientData::BackPointingCloneable< TrackGroupData >
{
public:
   TrackViewGroupData( TrackGroupData & );
   ~TrackViewGroupData() override;
   PointerType Clone() const override;

   static const TrackViewGroupData &Get( const Track& );
   static TrackViewGroupData &Get( Track& );
};

struct CreateViewGroupDataTag{};
using CreateViewGroupData = AttachedVirtualFunction< CreateViewGroupDataTag,
   std::unique_ptr< TrackViewGroupData >,
   TrackGroupData
>;

#endif
