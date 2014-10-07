// Copyright (C) 2001-2003 William E. Kempf
// Copyright (C) 2006 Roland Schwarz
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying 
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Boost Logging library
//
// Author: John Torjo, www.torjo.com
//
// Copyright (C) 2007 John Torjo (see www.torjo.com for email)
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org for updates, documentation, and revision history.
// See http://www.torjo.com/log2/ for more details

#ifndef JT28092007_BOOST_TSS_IMPL_WIN32
#define JT28092007_BOOST_TSS_IMPL_WIN32

#include <vector>
#include <memory>
#include <boost/assert.hpp>

#   include <windef.h>

#ifndef WINBASEAPI 
#define WINBASEAPI 
#endif

#ifndef TLS_OUT_OF_INDEXES 
#define TLS_OUT_OF_INDEXES ((DWORD)0xFFFFFFFF)
#endif

WINBASEAPI DWORD WINAPI TlsAlloc( VOID);
WINBASEAPI LPVOID WINAPI TlsGetValue(DWORD dwTlsIndex);
WINBASEAPI BOOL WINAPI TlsSetValue(DWORD dwTlsIndex, LPVOID lpTlsValue);


namespace boost { namespace logging {

namespace detail {


typedef std::vector<void*> tss_libed2k_pods;

DWORD& tss_data_native_key () ;
void init_tss_data();
unsigned long libed2k_pod_idx() ;
tss_libed2k_pods* get_libed2k_pods();


inline DWORD& tss_data_native_key () {
    static DWORD key = TLS_OUT_OF_INDEXES;
    return key;
}

// note: this should be called ONLY ONCE
inline void init_tss_data() {
    //Allocate tls libed2k_pod

    // if you get an assertion here, this function was called twice - should never happen!
    BOOST_ASSERT( tss_data_native_key() == TLS_OUT_OF_INDEXES);
    // now, allocate it
    tss_data_native_key() = TlsAlloc();

    // make sure the static gets created
    object_deleter();
}

inline unsigned long libed2k_pod_idx() {
    typedef boost::logging::threading::mutex mutex;
    static mutex cs;
    static unsigned int idx = 0;
    
    mutex::scoped_lock lk(cs);

    // note: if the Logging Lib is used with TLS, I'm guaranteed this will be called before main(),
    //       and that this will work
    if ( !idx)
        init_tss_data();

    ++idx;
    return idx;
}

inline tss_libed2k_pods* get_libed2k_pods()
{
    tss_libed2k_pods* libed2k_pods = 0;
    libed2k_pods = static_cast<tss_libed2k_pods*>( TlsGetValue( tss_data_native_key() ));

    if (libed2k_pods == 0)
    {
        std::auto_ptr<tss_libed2k_pods> temp( new_object_ensure_delete<tss_libed2k_pods>() );
        // pre-allocate a few elems, so that we'll be fast
        temp->resize(BOOST_LOG_TSS_SLOTS_SIZE);

        if (!TlsSetValue(tss_data_native_key(), temp.get()))
            return 0;

        libed2k_pods = temp.release();
    }

    return libed2k_pods;
}

inline tss::tss() : m_libed2k_pod( libed2k_pod_idx() )
{
}


inline tss::~tss()
{
}

inline void* tss::get() const
{
    tss_libed2k_pods* libed2k_pods = get_libed2k_pods();

    if (m_libed2k_pod >= libed2k_pods->size())
        return 0;

    return (*libed2k_pods)[m_libed2k_pod];
}

inline void tss::set(void* value)
{
    tss_libed2k_pods* libed2k_pods = get_libed2k_pods();

    if (m_libed2k_pod >= libed2k_pods->size())
    {
        libed2k_pods->resize(m_libed2k_pod + 1);
    }

    (*libed2k_pods)[m_libed2k_pod] = value;
}


}}}


#endif
