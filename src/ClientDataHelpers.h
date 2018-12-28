/**********************************************************************

Audacity: A Digital Audio Editor

ClientDataHelpers.h

Paul Licameli

**********************************************************************/

#ifndef __AUDACITY_CLIENT_DATA_HELPERS__
#define __AUDACITY_CLIENT_DATA_HELPERS__

#include <memory>
#include <mutex>
#include <type_traits>

namespace ClientData {
   // Helpers to define ClientData::Site class template

// To specify (separately for the table of factories, and for the per-Site
// container of client data objects) whether to ensure mutual exclusion.
enum LockingPolicy {
   NoLocking,
   NonrecursiveLocking, // using std::mutex
   RecursiveLocking,    // using std::recursive_mutex
};

// To specify how the Site implements its copy constructor and assignment.
// (Move construction and assignment always work.)
enum CopyingPolicy {
   SkipCopying,     // copy ignores the argument and constructs empty
   ShallowCopying,  // just copy smart pointers; won't compile for unique_ptr
   DeepCopying,     // requires ClientData to define a Clone() member;
                    // reparent the clones if they are back-pointing;
                    //    won't compile for weak_ptr (and wouldn't work)
   CopyOnWrite,     // requires ClientData to define a Clone() member;
                    //    won't compile for weak_ptr (and wouldn't work)
};

// forward declarations
struct Base;
template<
   template<typename> class Owner
> struct Cloneable;
template<
   typename Host
> struct BackPointing;
template<
   typename Host,
   template<typename> class Owner
> struct BackPointingCloneable;

// A metafunction to choose the best ClientData parameter for Site
template< CopyingPolicy copyingPolicy, bool BackPointing >
struct ChooseClientData;
template< CopyingPolicy copyingPolicy >
struct ChooseClientData< copyingPolicy, false >
{
   // Covers most cases of non-back-pointing
   template< typename Host, template<typename> class Pointer >
   using type = Base;
};
template< CopyingPolicy copyingPolicy >
struct ChooseClientData< copyingPolicy, true >
{
   // Covers most cases of back-pointing
   template< typename Host, template<typename> class Pointer >
   using type = BackPointing< Host >;
};
template<>
struct ChooseClientData< ShallowCopying, true >
{
   // Intentionally not defined for this case
};
template<> struct ChooseClientData< DeepCopying, false >
{
   template< typename Host, template<typename> class Pointer >
   using type = Cloneable< Pointer >;
};
template<> struct ChooseClientData< DeepCopying, true >
{
   template< typename Host, template<typename> class Pointer >
   using type = BackPointingCloneable< Host, Pointer >;
};

// A conversion so we can use operator * in all the likely cases for the
// template parameter Pointer.  (Return value should be bound only to const
// lvalue references)
template< typename Ptr > static inline
   const Ptr &Dereferenceable( Ptr &p )
      { return p; } // returns an lvalue
template< typename Obj > static inline
   std::shared_ptr<Obj> Dereferenceable( std::weak_ptr<Obj> &p )
      { return p.lock(); } // overload returns a prvalue

// Declared but undefined helper function for defining trait class HasReparent
auto HasReparentHelper(...) -> std::false_type;
template< typename Child, typename Parent > auto HasReparentHelper(
   Child *pChild, Parent *pParent,
   decltype( (void)pChild->Reparent(pParent), nullptr ) // use SFINAE
)  -> std::true_type;

// A trait class detecting whether class Child has a member function Reparent
// taking a pointer to Parent
template< typename Child, typename Parent>
struct HasReparent
   : decltype( HasReparentHelper((Child*)nullptr, (Parent*)nullptr, nullptr) )
{};

// Decorator template to implement locking policies
template< typename Object, LockingPolicy > struct Lockable;
template< typename Object > struct Lockable< Object, NoLocking >
: Object {
   // implement trivial non-locking policy
   struct Lock{};
   Lock lock() { return {}; }
};
template< typename Object > struct Lockable< Object, NonrecursiveLocking >
: Object, std::mutex {
   // implement real locking
   using Lock = std::unique_lock< std::mutex >;
   Lock lock() { return Lock{ *this }; }
};
template< typename Object > struct Lockable< Object, RecursiveLocking >
: Object, std::recursive_mutex {
   // implement real locking
   using Lock = std::unique_lock< std::recursive_mutex >;
   Lock lock() { return Lock{ *this }; }
};

// Pairing of a reference to a Lockable and a lock on it
template< typename Lockable > struct Locked
   // inherit, maybe empty base class optimization applies:
   : private Lockable::Lock
{
   explicit Locked( Lockable &object )
      : Lockable::Lock( object.lock() )
      , mObject( object )
   {}
   Lockable &mObject;
};

// Decorator template implements the copying policy
template< typename Container, CopyingPolicy > struct Copyable;
template< typename Container > struct Copyable< Container, SkipCopying >
: Container {
   Copyable() = default;
   Copyable( const Copyable & ) {}
   Copyable &operator=( const Copyable & ) { return *this; }
   Copyable( Copyable && ) = default;
   Copyable &operator=( Copyable&& ) = default;
   bool NeedCopyOnWrite() { return false; }
   void DoCopyOnWrite() {}
};
template< typename Container > struct Copyable< Container, ShallowCopying >
: Container {
   Copyable() = default;
   Copyable( const Copyable &other )
   { *this = other;  }
   Copyable &operator=( const Copyable &other )
   {
      if (this != &other) {
         // Build then swap for strong exception guarantee
         Copyable temp;
         for ( auto &&ptr : other )
            temp.push_back( ptr );
         this->swap( temp );
      }
      return *this;
   }
   Copyable( Copyable && ) = default;
   Copyable &operator=( Copyable&& ) = default;
   bool NeedCopyOnWrite() { return false; }
   void DoCopyOnWrite() {}
};
template< typename Container > struct Copyable< Container, DeepCopying >
: Container {
   Copyable() = default;
   Copyable( const Copyable &other )
   { *this = other;  }
   Copyable &operator=( const Copyable &other )
   {
      if (this != &other) {
         // Build then swap for strong exception guarantee
         Copyable temp;
         for ( auto &&p : other ) {
            temp.push_back( p ? p->Clone() : (decltype( p->Clone() )){} );
         }
         this->swap( temp );
      }
      return *this;
   }
   Copyable( Copyable && ) = default;
   Copyable &operator=( Copyable&& ) = default;
   bool NeedCopyOnWrite() { return false; }
   void DoCopyOnWrite() {}
};
template< typename Container > struct Copyable< Container, CopyOnWrite >
: private std::shared_ptr<Container> {
   using Base = std::shared_ptr<Container>;
   using iterator = typename Container::iterator;
   using size_type = typename Container::size_type;
   
   iterator begin() const { return (*this)->begin(); }
   iterator end() const { return (*this)->end(); }
   size_type size() const { return (*this)->size(); }
   void resize( size_type newSize ) { (*this)->resize( newSize ); }

   Copyable() : Base{ std::make_shared< Container >() } {}
   Copyable( const Copyable &other ) : Base( other ) {}
   Copyable &operator=( const Copyable &other )
      // Nothing special needed:
      // If this is not self-assignment, then old data are abandoned,
      // and other's data become shared (if not already shared)
      // and copy-on-write of either *this or other may happen later
      = default;
   Copyable( Copyable && ) = default;
   Copyable &operator=( Copyable&& ) = default;
   bool NeedCopyOnWrite()
   {
      return Base::use_count() > 1;
   }
   void DoCopyOnWrite()
   {
      if ( NeedCopyOnWrite() )
         DoCopy( *this );
   }
   void DoCopy( const Copyable &other )
   {
      // Build first then share for strong exception guarantee
      Copyable temp;
      for ( auto &&p : other ) {
         (*temp).push_back( p ? p->Clone() : ( decltype( p->Clone() ) ){} );
      }
      ( Base& )( *this ) = temp;
   }
};

}

#endif
