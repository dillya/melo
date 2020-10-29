# UPnP module

# Icon path
icon_path = join_paths(data_path, 'icons/128x128/')

# Module configuration
cdata = configuration_data()
cdata.set_quoted(
	'MELO_UPNP_PLAYER_ICON_JPEG',
	join_paths(icon_path, 'melo.jpg')
)
cdata.set_quoted(
	'MELO_UPNP_PLAYER_ICON_PNG',
	join_paths(icon_path, 'melo.png')
)
configure_file(output : 'config.h', configuration : cdata)

# Module sources
libmelo_upnp_sources = [
	'melo_upnp_player.c',
	'melo_upnp.c'
]

# Module dependencies
gssdp_dep = dependency('gssdp-1.0', version : '>=1.0.5', required : false)
gupnp_dep = dependency('gupnp-1.0', version : '>=1.0.5', required : false)
rygel_renderer_dep = dependency(
	'rygel-renderer-2.6',
	version : '>=0.36.2',
	required : false
)
rygel_renderer_gst_dep = dependency(
	'rygel-renderer-gst-2.6',
	version : '>=0.36.2',
	required : false
)

# Disable UPnP module
if not (gssdp_dep.found() and gupnp_dep.found() and
		rygel_renderer_dep.found() and rygel_renderer_gst_dep.found())
	subdir_done()
endif

# Generate module
libmelof_upnp = shared_library(
	'melo_upnp',
	libmelo_upnp_sources,
	dependencies : [
		libmelo_dep,
		libmelo_proto_dep,
		gssdp_dep,
		gupnp_dep,
		rygel_renderer_dep,
		rygel_renderer_gst_dep
	],
	version : meson.project_version(),
	install : true,
	install_dir : module_path
)

