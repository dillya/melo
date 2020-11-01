# Melo Meson build

project('melo',
	'c',
	version : '1.0.0',
	license : 'LGPLv2.1')

# Melo module installation path
module_path = join_paths(get_option('prefix'), get_option('libdir'), 'melo')

# Shared data installation path
data_path = join_paths(get_option('prefix'), get_option('datadir'), 'melo')

# Proto files installation path
proto_path = join_paths(data_path, 'proto')

# Include path
libmelo_inc = include_directories('include')

subdir('proto')
subdir('include/melo')
subdir('lib')
subdir('modules')
subdir('src')
subdir('data')
subdir('doc')
