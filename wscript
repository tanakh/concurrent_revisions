APPNAME = 'concurrent_revisions'
VERSION = '0.1.0'

top = '.'
out = 'build'

def options(opt):
  opt.load('compiler_cxx')
  opt.load('unittest_gtest')

def configure(conf):
  conf.load('compiler_cxx')
  conf.load('unittest_gtest')

  if conf.env.CXX == ['clang++']:
    conf.env.append_unique(
      'CXXFLAGS',
      ['-std=c++0x', '-stdlib=libc++']
      )
    # conf.env.LINK_CXX = ['llvm-ld']
    conf.env.append_unique(
      'LINKFLAGS',
      ['-lc++', '-O2']
      )
  else:
    conf.env.append_unique(
      'CXXFLAGS',
      ['-std=c++0x', '-Wall', '-O0', '-g']
      )
    conf.env.append_unique(
      'LINKFLAGS',
      []
      )

def build(bld):
  bld.shlib(
    source = 'concurrent_revisions.cpp',
    includes = '.',
    target = 'concurrent_revisions'
    )

  bld.program(
    features = 'gtest',
    source = 'test.cpp',
    includes = '.',
    target = 'test',
    use = 'concurrent_revisions'
    )
