APPNAME = 'concurrent-revisions'
VERSION = '0.1.0'

top = '.'
out = 'build'

def options(opt):
  opt.load('compiler_cxx')

def configure(conf):
  conf.load('compiler_cxx')

  if conf.env.CXX == ['clang++']:
    conf.env.append_unique(
      'CXXFLAGS',
      ['-std=c++0x', '-stdlib=libc++', '-D_GLIBCXX_PARALLEL']
      )
    # conf.env.LINK_CXX = ['llvm-ld']
    conf.env.append_unique(
      'LINKFLAGS',
      ['-lc++', '-O4']
      )
  else:
    conf.env.append_unique(
      'CXXFLAGS',
      ['-std=c++0x', '-Wall', '-D_GLIBCXX_PARALLEL']
      )
    conf.env.append_unique(
      'LINKFLAGS',
      ['-lgomp']
      )

def build(bld):
  bld.program(
    source = 'main.cpp concurrent-revisions.cpp',
    includes = '.',
    target = 'test'
    )
