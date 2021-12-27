project('nethid', 'c')

args = ['-Wall', '-Wextra', '-Wpedantic']
if get_option('buildtype').startswith('debug')
  args += ['-ggdb', '-Og']
endif

incdir = include_directories('include')

executable('nethidserver',
  'src/server/main.c',
  'src/common.c',
  c_args : args,
  include_directories : incdir,
  install : true)

executable('nethidclient',
  'src/client/main.c',
  'src/common.c',
  c_args : args,
  include_directories : incdir,
  install : true)
