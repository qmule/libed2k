
import os, fnmatch
from os.path import join, basename

def globrec(rootdir, wildcard):
    matches = []
    for root, dirnames, filenames in os.walk(rootdir):
        for filename in fnmatch.filter(filenames, wildcard):
            matches.append(join(root, filename))
    return matches

def unionArgs(*args):
    res = args[0].copy()
    for f in args[1:] :
        for k,v in f.iteritems():
            prev = res.get(k)
            res[k] = prev and prev + v or v  # for old pythons
    return res

# Environment
boostRoot      = os.getenv('BOOST_ROOT')
libtorrentRoot = os.getenv('LIBTORRENT_ROOT')

# Arguments
args = {
    'ENV'     : os.environ,
    'CPPPATH' : ['include'] + [join(p, 'include') for p in [boostRoot, libtorrentRoot]],
    'CXXFLAGS': ['-DLIBED2K_DEBUG', '-Wall', '-g', '-D_FILE_OFFSET_BITS=64',
                 '-DBOOST_FILESYSTEM_VERSION=2'],
    'LIBPATH' : [join(p, 'lib') for p in [boostRoot, libtorrentRoot]],
    'LIBS'    : ['boost_system', 'boost_program_options', 'boost_iostreams',
                 'boost_thread', 'boost_signals', 'boost_filesystem',
                 'cryptopp', 'ssl', 'pthread', 'rt']
    }

env = Environment(**args)
sources = globrec('src', '*.cpp')
lib = env.StaticLibrary(join('lib', 'ed2k'), sources)
env.Program(join('bin', 'ed2k_client'), [join('test', 'ed2k_client', 'ed2k_client.cpp'), lib])

uenv = Environment(**unionArgs(args, {'CXXFLAGS': ['-Wno-sign-compare'], 'LIBS': ['boost_unit_test_framework']}))
utests = globrec('unit', '*.cpp')
uenv.Program(join('unit', 'run_tests'), utests + [lib])
