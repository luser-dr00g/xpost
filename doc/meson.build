
doxygen = find_program('doxygen', required: false)
if doxygen.found()
  custom_target('doxygen',
    input: 'Doxyfile',
    output: '.',
    command: [ doxygen, meson.current_source_dir() / 'Doxyfile' ],
    install: false,
    build_by_default : true
  )
endif
