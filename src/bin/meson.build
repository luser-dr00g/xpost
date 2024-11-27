
# xpost

xpost_bin_src = files([
  'xpost_main.c'
])

if sys_windows
  windows = import('windows')
  xpost_rc = windows.compile_resources(files('xpost.rc'))
  xpost_bin_src += xpost_rc
endif

executable('xpost', xpost_bin_src,
  c_args: '-DHAVE_CONFIG_H',
  dependencies: libxpost_dep,
  include_directories: [
    config_dir,
    include_directories('../lib') ],
  install : true
)

# xpost_client

executable('xpost_client', 'xpost_client.c',
  c_args: '-DHAVE_CONFIG_H',
  dependencies: libxpost_dep,
  include_directories: [
    config_dir,
    include_directories('../lib') ],
  install : true
)

# xpost_dsc

executable('xpost_dsc', 'xpost_dsc.c',
  c_args: '-DHAVE_CONFIG_H',
  dependencies: [ libxpost_dep, libxpost_dsc_dep ],
  include_directories: [
    config_dir,
    include_directories('../lib') ],
  install : true
)

# xpost_view

if sys_windows or xcb_dep.found()
  xpost_view_src = files([
    'xpost_view.c'
  ])
  if sys_windows
    xpost_view_src += files([
      'xpost_view_win32.c'
    ])
  else
    xpost_view_src += files([
      'xpost_view_xcb.c'
    ])
  endif

  executable('xpost_view', xpost_view_src,
    c_args: '-DHAVE_CONFIG_H',
    dependencies: [ libxpost_dep, libxpost_dsc_dep ],
    include_directories: [
      config_dir,
      include_directories('../lib') ],
    install : true
  )
endif