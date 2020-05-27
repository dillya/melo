# Melo program

# Set default web UI path
web_ui_path = get_option('web_ui_path')
if web_ui_path == ''
	web_ui_path = join_paths(
		get_option('prefix'), get_option('datadir'), 'melo/ui')
endif

# Program configuration
cdata = configuration_data()
cdata.set_quoted(
	'MELO_WEB_UI_PATH',
	web_ui_path,
	description : 'Web UI path for HTTP server')
configure_file(output : 'config.h', configuration : cdata)

# Program sources
src = [
	'asset.c',
	'discover.c',
	'main.c',
	'websocket.c'
]

# Generate program
executable(
	'melo',
	sources : src,
	dependencies : libmelo_dep,
	install : true)
