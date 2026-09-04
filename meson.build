project(
    'Lumi',
    'c',
    version: 'alpha-0.1.0',
    default_options: [ 'c_std=c23' ]
)

include_dir = include_directories('include')

sources = [
    'src/lumi.c',
    'src/vm.c'
]

executable(
    'Lumi',
    sources,
    include_directories: include_dir
)
