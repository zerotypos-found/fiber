
//          Copyright Oliver Kowalke 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "boost/fiber/detail/waiting_queue.hpp"

#include <algorithm>
#include <cstddef>
#include <chrono>

#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/intrusive_ptr.hpp>

#include "boost/fiber/algorithm.hpp"
#include "boost/fiber/detail/config.hpp"
#include "boost/fiber/fiber_context.hpp"

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

namespace boost {
namespace fibers {
namespace detail {

void
waiting_queue::push( fiber_context * item) noexcept {
    BOOST_ASSERT( nullptr != item);
    BOOST_ASSERT( nullptr == item->nxt);

    // Skip past any fiber_contexts in the queue whose time_point() is less
    // than item->time_point(), looking for the first fiber_context in the
    // queue whose time_point() is at least item->time_point(). Insert
    // item before that. In other words, insert item so as to preserve
    // ascending order of time_point() values. (Recall that a fiber_context
    // waiting with no timeout uses the maximum time_point value.)

    // We do this by walking the linked list of nxt fields with a
    // fiber_context**. In other words, first we point to &head_, then to
    // &head_->nxt, then to &head_->nxt->nxt and so forth. When we find
    // the item with the right time_point(), we're already pointing to the
    // fiber_context* that links it into the list. Insert item right there.

    fiber_context ** f( & head_);
    for ( ; nullptr != * f; f = & ( * f)->nxt) {
        if ( item->time_point() <= ( * f)->time_point() ) {
            break;
        }
    }

    // Here, either we reached the end of the list (! *f) or we found a
    // (*f) before which to insert 'item'. Break the link at *f and insert
    // item.
    item->nxt = * f;
    * f = item;
}

void
waiting_queue::move_to( sched_algorithm * sched_algo) {
    BOOST_ASSERT( nullptr != sched_algo);

    std::chrono::high_resolution_clock::time_point now(
        std::chrono::high_resolution_clock::now() );

    // Search the queue for every fiber_context 'f' which is_ready(). Each
    // time we find a ready fiber_context, unlink it from the queue and pass
    // it to sched_algo->awakened().

    // Search using a fiber_context**, starting at &head_.
    for ( fiber_context ** fp( & head_); nullptr != * fp;) {
        fiber_context * f( * fp);
        BOOST_ASSERT( ! f->is_running() );
        BOOST_ASSERT( ! f->is_terminated() );

        // set fiber to state_ready if deadline was reached
        // set fiber to state_ready if interruption was requested
        if ( f->time_point() <= now || f->interruption_requested() ) {
            f->set_ready();
        }

        if ( ! f->is_ready() ) {
            // If f is NOT ready, skip fp past it.
            fp = & ( * fp)->nxt;
        } else {
            // Here f is ready. Unlink it from the list.
            * fp = ( * fp)->nxt;
            f->nxt = nullptr;
            // Pass the newly-unlinked fiber_context* to sched_algo.
            f->time_point_reset();
            sched_algo->awakened( f);
        }
    }
}

}}}

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif
