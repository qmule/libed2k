
#ifndef __LIBED2K_ERROR_CODE__
#define __LIBED2K_ERROR_CODE__

namespace libed2k {

    struct libed2k_error_category : boost::system::error_category
    {
        virtual const char* name() const;
        virtual std::string message(int ev) const;
        virtual boost::system::error_condition default_error_condition(int ev) const
            { return boost::system::error_condition(ev, *this); }
    };

    inline boost::system::error_category& get_libed2k_category()
    {
        static libed2k_error_category libed2k_category;
        return libed2k_category;
    }

}

#endif
