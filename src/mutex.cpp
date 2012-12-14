
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_FIBERS_SOURCE

#include <boost/fiber/mutex.hpp>

#include <boost/assert.hpp>

#include <boost/fiber/scheduler.hpp>
#include <boost/fiber/operations.hpp>

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

namespace boost {
namespace fibers {

mutex::mutex( bool checked) :
	state_( UNLOCKED),
    owner_(),
    waiting_(),
    checked_( checked)
{}

void
mutex::lock()
{
    while ( UNLOCKED != state_)
    {
        if ( this_fiber::is_fiberized() )
        {
            waiting_.push_back(
                    scheduler::instance().active() );
            scheduler::instance().wait();
        }
        else
            scheduler::instance().run();
    }
    state_ = LOCKED;
    if ( this_fiber::is_fiberized() )
        owner_ = scheduler::instance().active()->get_id();
    else
        owner_ = detail::fiber_base::id();
}

bool
mutex::try_lock()
{
    if ( LOCKED == state_) return false;
    state_ = LOCKED;
    if ( this_fiber::is_fiberized() )
        owner_ = scheduler::instance().active()->get_id();
    else
        owner_ = detail::fiber_base::id();
    return true;
}

void
mutex::unlock()
{
    if ( checked_)
    {
        if ( this_fiber::is_fiberized() )
        {
            if ( scheduler::instance().active()->get_id() != owner_)
                std::abort();
        }
        else if ( detail::fiber_base::id() != owner_)
                std::abort();
    }

	if ( ! waiting_.empty() )
    {
        detail::fiber_base::ptr_t f;
        do
        {
            f.swap( waiting_.front() );
            waiting_.pop_front();
        } while ( f->is_complete() );
        if ( f)
            scheduler::instance().notify( f);
    }
	state_ = UNLOCKED;
    owner_ = detail::fiber_base::id();
}

}}

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif
