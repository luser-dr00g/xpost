
# libxpost

libxpost_src = files([
  'xpost_array.c',
  'xpost_compat.c',
  'xpost_context.c',
  'xpost_dev_bgr.c',
  'xpost_dev_generic.c',
  'xpost_dev_jpeg.c',
  'xpost_dev_png.c',
  'xpost_dev_raster.c',
  'xpost_dict.c',
  'xpost_dsc_file.c',
  'xpost_dsc_parse.c',
  'xpost_error.c',
  'xpost_file.c',
  'xpost_font.c',
  'xpost_free.c',
  'xpost_garbage.c',
  'xpost_interpreter.c',
  'xpost_log.c',
  'xpost_main.c',
  'xpost_matrix.c',
  'xpost_memory.c',
  'xpost_name.c',
  'xpost_object.c',
  'xpost_op_array.c',
  'xpost_op_boolean.c',
  'xpost_op_context.c',
  'xpost_op_control.c',
  'xpost_op_dict.c',
  'xpost_op_file.c',
  'xpost_op_font.c',
  'xpost_op_math.c',
  'xpost_op_matrix.c',
  'xpost_op_misc.c',
  'xpost_op_packedarray.c',
  'xpost_op_param.c',
  'xpost_op_path.c',
  'xpost_op_save.c',
  'xpost_op_stack.c',
  'xpost_op_string.c',
  'xpost_op_token.c',
  'xpost_op_type.c',
  'xpost_operator.c',
  'xpost_oplib.c',
  'xpost_save.c',
  'xpost_stack.c',
  'xpost_string.c'
])

if xcb_dep.found()
  libxpost_src += files([
    'xpost_dev_xcb.c'
  ])
endif

if sys_windows
  libxpost_src += files([
    'xpost_compat_win32.c',
    'xpost_dev_win32.c'
  ])
else
  libxpost_src += files([
    'xpost_compat_posix.c'
  ])
endif

libxpost_deps = [
  fontconfig_dep,
  freetype_dep,
  jpeg_dep,
  libpng_dep,
  xcb_dep,
  m_dep
]

if sys_windows
  libxpost_deps += [
    bcrypt_dep,
    gdi_dep,
    gl_dep,
    winsock2_dep
  ]
endif

libxpost_lib = library('xpost', libxpost_src,
  c_args: [
    '-DHAVE_CONFIG_H',
    '-DXPOST_BUILD',
    '-D_POSIX_C_SOURCE=200809L',
    '-DPACKAGE_DATA_DIR="' + dir_pkgdata + '"',
    '-DPACKAGE_INSTALL_DIR="' + dir_prefix + '"'
  ],
  dependencies: libxpost_deps,
  include_directories: [ config_dir, include_directories('.') ],
  install: true,
  version: meson.project_version()
)

libxpost_dep = declare_dependency(
  link_with: libxpost_lib
)

libxpost_headers_src = files([
  'xpost.h',
  'xpost_memory.h',
  'xpost_object.h',
  'xpost_context.h',
  'xpost_interpreter.h'
])

install_headers(libxpost_headers_src,
  install_dir : dir_pkginclude + '-' + v_maj
)

# libxpost_dsc

libxpost_dsc_src = files([
  'xpost_compat.c',
  'xpost_log.c',
  'xpost_dsc_file.c',
  'xpost_dsc_parse.c'
])

if sys_windows
  libxpost_dsc_src += files([
    'xpost_compat_win32.c',
  ])
else
  libxpost_dsc_src += files([
    'xpost_compat_posix.c'
  ])
endif

libxpost_dsc_lib = library('xpost_dsc', libxpost_dsc_src,
  c_args: [
    '-DHAVE_CONFIG_H',
    '-DXPOST_BUILD',
    '-D_POSIX_C_SOURCE=200809L',
  ],
  dependencies: libxpost_deps,
  include_directories: [ config_dir, include_directories('.') ],
  install: true,
  version: meson.project_version()
)

libxpost_dsc_dep = declare_dependency(
  link_with: libxpost_dsc_lib
)
