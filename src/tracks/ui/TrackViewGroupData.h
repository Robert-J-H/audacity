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

   bool GetMinimized() const { return mMinimized; }
   void SetMinimized( bool minimized );

   static const TrackViewGroupData &Get( const Track& );
   static TrackViewGroupData &Get( Track& );

   static const TrackViewGroupData &Get( const TrackGroupData& );
   static TrackViewGroupData &Get( TrackGroupData& );

protected:
   virtual void DoSetMinimized( bool minimized );

private:
   bool           mMinimized{ false };
};

struct CreateViewGroupDataTag{};
using CreateViewGroupData = AttachedVirtualFunction< CreateViewGroupDataTag,
   std::unique_ptr< TrackViewGroupData >,
   TrackGroupData
>;

#endif
