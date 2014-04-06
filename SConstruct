
import os, fnmatch
from os.path import join, basename

# Options
AddOption('--boost-mt', dest='boostmt', nargs=0, help='Using boost libraries with -mt suffix')
AddOption('--dbg', dest='dbg', nargs=0, help='Build library with debug options')

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

def libboost(name):
    suffix = '' if GetOption('boostmt') == None else '-mt'
    return 'boost_' + name + suffix

# Environment
boostRoot = os.getenv('BOOST_ROOT')

# Arguments
commonArgs = {
    'ENV'     : os.environ,
    'CPPPATH' : ['include'] + [join(p, 'include') for p in [boostRoot]],
    'CXXFLAGS': ['-Wall', '-Werror=return-type', '-Wno-unused-local-typedefs',
                 '-D_FILE_OFFSET_BITS=64', '-DLIBED2K_USE_BOOST_DATE_TIME'],
    'LIBPATH' : [join(p, 'lib') for p in [boostRoot]],
    'LIBS'    : [libboost('system'), libboost('thread'), 'pthread']
    }

debugArgs = {
    'CXXFLAGS': ['-g', '-O0', '-rdynamic', '-DLIBED2K_DEBUG']
    }

releaseArgs = {
    'CXXFLAGS': ['-O2']
    }

args = unionArgs(commonArgs, releaseArgs if GetOption('dbg') == None else debugArgs)

env = Environment(**args)
sources = globrec('src', '*.cpp')
lib = env.StaticLibrary(join('lib', 'ed2k'), sources)

binenv = Environment(**unionArgs(args, {'LIBS' : ['ed2k'], 'LIBPATH' : ['lib']}))
conn = binenv.Program(join('bin', 'conn'), [join('test', 'conn', 'conn.cpp'), lib])

uenv = Environment(**unionArgs(args,
                               {'CXXFLAGS': ['-Wno-sign-compare'],
                                'LIBS': [libboost('unit_test_framework')]}))
utests = globrec('unit', '*.cpp')
uenv.Program(join('unit', 'run_tests'), utests + [lib])
