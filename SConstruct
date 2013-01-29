
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

# Arguments
args = {
    'ENV'     : os.environ,
    'CPPPATH' : ['include'] + [join(p, 'include') for p in [boostRoot]],
    'CXXFLAGS': ['-DLIBED2K_DEBUG', '-Wall', '-g', '-D_FILE_OFFSET_BITS=64',
                 '-DLIBED2K_USE_BOOST_DATE_TIME'],
    'LIBPATH' : [join(p, 'lib') for p in [boostRoot]],
    'LIBS'    : ['boost_system-mt', 'boost_thread-mt', 'pthread']
    }

env = Environment(**args)
sources = globrec('src', '*.cpp')
lib = env.StaticLibrary(join('lib', 'ed2k'), sources)

uenv = Environment(**unionArgs(args,
                               {'CXXFLAGS': ['-Wno-sign-compare'],
                                'LIBS': ['boost_unit_test_framework-mt', 'boost_filesystem-mt']}))
utests = globrec('unit', '*.cpp')
uenv.Program(join('unit', 'run_tests'), utests + [lib])
