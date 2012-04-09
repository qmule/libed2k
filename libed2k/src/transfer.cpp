
#include "session.hpp"
#include "session_impl.hpp"
#include "transfer.hpp"


using namespace libed2k;

transfer::transfer(aux::session_impl& ses, tcp::endpoint const& net_interface, 
                   int seq, add_transfer_params const& p):
    m_ses(ses),
    m_paused(false),
    m_sequential_download(false),
    m_sequence_number(seq),
    m_save_path(p.save_path),
    m_storage_mode(p.storage_mode),
    m_seed_mode(p.seed_mode)
{
    // TODO: init here
}

transfer::~transfer()
{
}

void transfer::start()
{
    if (!m_seed_mode)
    {
        m_picker.reset(new libtorrent::piece_picker());
        // TODO: resume data, file progress
    }

    init();

}

bool transfer::connect_to_peer(peer* peerinfo)
{

}

bool transfer::is_paused() const
{
    return m_paused || m_ses.is_paused();
}

void transfer::init()
{
    // the shared_from_this() will create an intentional
    // cycle of ownership, see the hpp file for description.
    m_owning_storage = new libtorrent::piece_manager(
        shared_from_this(), m_info, m_save_path, m_ses.m_filepool,
        m_ses.m_disk_thread, libtorrent::default_storage_constructor, 
        static_cast<libtorrent::storage_mode_t>(m_storage_mode));
    m_storage = m_owning_storage.get();

    if (has_picker())
    {
        int blocks_per_piece;      // TODO: need define
        int blocks_in_last_piece;  // TODO: need define
        m_picker->init(blocks_per_piece, blocks_in_last_piece, m_info->num_pieces());
    }

    // TODO: checking resume data

}
