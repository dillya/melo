# Library module

# Module sources
libmelo_library_sources = [
	'melo_library_browser.c',
	'melo_library.c'
]

# Generate module
libmelof_library = shared_library(
	'melo_library',
	libmelo_library_sources,
	dependencies : [libmelo_dep],
	version : meson.project_version(),
	install : true,
	install_dir : module_path)
