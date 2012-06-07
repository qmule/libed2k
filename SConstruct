
import os, fnmatch
from os.path import join, basename

def globrec(rootdir, wildcard):
    matches = []
    for root, dirnames, filenames in os.walk(rootdir):
        for filename in fnmatch.filter(filenames, wildcard):
            matches.append(join(root, filename))
    return matches

# Constants 
incdir = 'include'
srcdir = 'src'
bindir = 'bin'
libdir = 'lib'
libname = 'ed2k'

# Environment
boostRoot      = os.getenv('BOOST_ROOT')
libtorrentRoot = os.getenv('LIBTORRENT_ROOT')

# Arguments
args = {
    'ENV'     : os.environ,
    'CPPPATH' : [incdir] + [join(p, 'include') for p in [boostRoot, libtorrentRoot]],
    'CXXFLAGS': ['-DBOOST_FILESYSTEM_VERSION=2', '-Wall', '-g'],
    'LIBPATH' : [join(p, 'lib') for p in [boostRoot, libtorrentRoot]],
    'LIBS'    : ['boost_system', 'boost_program_options', 'boost_iostreams',
                 'boost_thread', 'boost_signals', 'boost_filesystem',
                 'torrent-rasterbar', 'cryptopp', 'ssl', 'pthread']
    }

env = Environment(**args)
sources = globrec(srcdir, '*.cpp')
lib = env.StaticLibrary(join(libdir, libname), sources)
env.Program(join(bindir, 'ed2k_client'), [join('test', 'ed2k_client', 'ed2k_client.cpp'), lib])