project('playground', 'c',
        version : '0.1',
        default_options : {
            'buildtype' : 'debug',
            'warning_level' : 3,
            'c_std' : 'c2x',
            'b_lto' : true,
            'default_library' : 'static'
        })

current_build_type = get_option('buildtype')

# Compiler setup
cc = meson.get_compiler('c')
add_project_arguments('-lm', language : 'c')
add_project_arguments('-g3',
                      '-Wshadow', '-Wno-unused-parameter', '-Wno-unknown-pragmas',
                      '-fvisibility=hidden', # If something breaks, it's mostly likely this
                      '-fno-strict-overflow', '-Wno-strict-overflow',
                      '-funwind-tables', '-fasynchronous-unwind-tables', '-rdynamic', '-fno-dwarf2-cfi-asm', '-fvar-tracking-assignments',
                      '-Wstrict-prototypes', '-Wmissing-prototypes', '-Wduplicated-branches', '-Wduplicated-cond',
                      '-Wmissing-declarations', '-Wformat=2', '-Wlogical-op', '-Wmissing-include-dirs',
                      '-Wnull-dereference', '-Wpointer-arith', '-Wredundant-decls', '-Wswitch-enum', '-Wundef', '-Wuninitialized',
                      '-Wwrite-strings', '-fno-common',
                      language : 'c')

if current_build_type == 'debug'
    add_project_arguments('-fanalyzer', '-fanalyzer-transitivity', language : 'c')

    # Enable ASAN
    asan_args = ['-fsanitize=address,leak,undefined', '-fsanitize-trap=undefined', '-fno-sanitize-recover=all']
    add_project_arguments(asan_args, language : 'c')
    add_project_link_arguments(asan_args, language : 'c')
endif

# Dependencies
fuse_dep = dependency('fuse3')

# Define sources
incdir = include_directories('include')

src = []
# Use glob() to find all .c files in the src/ directory and its subdirectories
src_files = run_command('find', 'src/', '-name', '*.c', check : true).stdout().strip().split('\n')
foreach file : src_files
    src += files(file)
endforeach

# Define the executable
executable('playground',
           sources : src,
           dependencies : [fuse_dep],
           include_directories : incdir,
           install : true
)
