
doxygen = find_program('doxygen', required: false)
if doxygen.found()
  custom_target('doc',
    input: 'Doxyfile',
    output: '.',
    command: [ doxygen, meson.current_source_dir() / 'Doxyfile' ],
    install: false
  )
endif
