# Melo library

# Module prefix
module_prefix = 'libmelo_'

# Library configuration
cdata = configuration_data()
cdata.set_quoted(
	'MELO_MODULE_PATH',
	module_path,
	description : 'Module installation path')
cdata.set_quoted(
	'MELO_MODULE_PREFIX',
	module_prefix,
	description : 'Module file name prefix')
configure_file(output : 'config.h', configuration : cdata)

# Library sources
libmelo_sources = [
	'melo.c',
	'melo_browser.c',
	'melo_cover.c',
	'melo_events.c',
	'melo_log.c',
	'melo_message.c',
	'melo_module.c',
	'melo_player.c',
	'melo_playlist.c',
	'melo_request.c',
	'melo_requests.c',
	'melo_tags.c'
]

# Library dependencies
glib_dep = dependency('glib-2.0', version : '>=2.50.3')
gobject_dep = dependency('gobject-2.0')
gmodule_dep = dependency('gmodule-2.0')
gio_dep = dependency('gio-2.0')
gstreamer_dep = dependency('gstreamer-1.0', version: '>=1.14.4')
gstreamer_tag_dep = dependency('gstreamer-tag-1.0')

# Generate shared library
libmelo = shared_library(
	'melo',
	libmelo_sources,
	include_directories : libmelo_inc,
	dependencies : [
		glib_dep,
		gobject_dep,
		gmodule_dep,
		gio_dep,
		gstreamer_dep,
		gstreamer_tag_dep
	],
	install : true)

# Declare melo library dependency
libmelo_dep = declare_dependency(
	link_with : libmelo,
	include_directories : libmelo_inc,
	dependencies : [ glib_dep, gobject_dep, gmodule_dep, gstreamer_dep ])

# Create pkg-config
pkgg = import('pkgconfig')
pkgg.generate(
	libmelo,
	variables: [
		'moduledir=' + module_path,
		'moduleprefix=' + module_prefix
	],
	requires: [
		'glib-2.0',
		'gobject-2.0',
		'gmodule-2.0',
		'gstreamer-1.0'
	]
)
