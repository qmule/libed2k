#include "log.hpp"
#include <boost/logging/format.hpp>
#include <boost/logging/writer/ts_write.hpp>

using namespace boost::logging;

// Step 6: Define the filters and loggers you'll use
BOOST_DEFINE_LOG(g_l, log_type)
BOOST_DEFINE_LOG_FILTER(g_l_filter, level::holder)

void init_logs()
{
    // Add formatters and destinations
    // That is, how the message is to be formatted...
    g_l()->writer().add_formatter( formatter::idx() );
    g_l()->writer().add_formatter( formatter::time("$hh:$mm.$ss ") );
    g_l()->writer().add_formatter( formatter::append_newline() );

    //        ... and where should it be written to
    g_l()->writer().add_destination( destination::cout() );
    //g_l()->writer().add_destination( destination::dbg_window() );
    g_l()->writer().add_destination( destination::file("out.txt") );
    g_l()->turn_cache_off();
}
