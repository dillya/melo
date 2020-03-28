# Radio module

# Module sources
libmelo_radio_sources = [
	'melo_radio_browser.c',
	'melo_radio_player.c',
	'melo_radio.c'
]

# Generate module
libmelof_radio = shared_library(
	'melo_radio',
	libmelo_radio_sources,
	dependencies : [libmelo_dep, libmelo_proto_dep],
	install : true,
	install_dir : module_path)
