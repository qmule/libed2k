
// ed2k session

namespace libed2k
{
    // Once it's created, the session object will spawn the main thread
    // that will do all the work. The main thread will be idle as long 
    // it doesn't have any transfers to participate in.
    class session : public session_base
    {
    public:
        session(const std::string& logpath = ".")
        {
        }
    };
}
