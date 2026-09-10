project(
    'Lumi',
    'c',
    version: 'alpha-0.1.0',
    default_options: [
        'c_std=c23',
        'optimization=3'
    ]
)

include_dir = include_directories('include')

sources = [
    'src/lumi.c',
    'src/vm.c',
    'src/as.c'
]

args = [
    '-Wall',
    '-Wextra',
    '-Wpedantic',
    '-Wno-gnu-designator',
    '-Wno-gnu-label-as-value',
    '-Wno-override-init',
    '-Wno-extra-semi'
]

executable(
    'Lumi',
    sources,
    include_directories: include_dir,
    c_args: args
)
