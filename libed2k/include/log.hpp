#ifndef __LOG__
#define __LOG__

#include <boost/logging/format_fwd.hpp>

// Step 1: Optimize : use a cache string, to make formatting the message faster
/*
namespace bl = boost::logging;
typedef bl::tag::holder<
    // string class
    bl::optimize::cache_string_one_str<>,
    // tags
    bl::tag::thread_id, bl::tag::time> log_string_type;
// note: if you don't use tags, you can simply use a string class:
// typedef bl::optimize::cache_string_one_str<> log_string_type;
BOOST_LOG_FORMAT_MSG( log_string_type )
*/
BOOST_LOG_FORMAT_MSG( optimize::cache_string_one_str<> )

#ifndef BOOST_LOG_COMPILE_FAST
#include <boost/logging/format.hpp>
#include <boost/logging/writer/ts_write.hpp>
#endif

// Step 3 : Specify your logging class(es)
typedef boost::logging::logger_format_write< > log_type;

// Step 4: declare which filters and loggers you'll use
BOOST_DECLARE_LOG_FILTER(g_l_filter, boost::logging::level::holder)
BOOST_DECLARE_LOG(g_l, log_type)

// Step 5: define the macros through which you'll log
#define LDBG_ BOOST_LOG_USE_LOG_IF_LEVEL(g_l(), g_l_filter(), debug ) << "[dbg] "
#define LERR_ BOOST_LOG_USE_LOG_IF_LEVEL(g_l(), g_l_filter(), error ) << "[ERR] "
#define LAPP_ BOOST_LOG_USE_LOG_IF_LEVEL(g_l(), g_l_filter(), info )

void init_logs();

#endif //__LOG__
