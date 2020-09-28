# File module

# Module sources
libmelo_file_sources = [
	'melo_file_browser.c',
	'melo_file_player.c',
	'melo_file.c'
]

gio_dep = dependency('gio-2.0', version : '>=2.58.3')
gst_pbutils_dep = dependency('gstreamer-pbutils-1.0', version : '>=1.14.4')

# Generate module
libmelof_file = shared_library(
	'melo_file',
	libmelo_file_sources,
	dependencies : [
		libmelo_dep,
		libmelo_proto_dep,
		gio_dep,
		gst_pbutils_dep
	],
	version : meson.project_version(),
	install : true,
	install_dir : module_path)
