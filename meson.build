# Melo Meson build

project('melo',
	'c',
	version : '1.0.0',
	license : 'LGPLv2.1')

# Melo module installation path
module_path = join_paths(get_option('prefix'), get_option('libdir'), 'melo')

libmelo_inc = include_directories('include')

subdir('proto')
subdir('include/melo')
subdir('lib')
subdir('src')
