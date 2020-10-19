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

# Add protobuf files
proto_gen = generator(
	protoc_c,
	output : ['@BASENAME@.pb-c.c', '@BASENAME@.pb-c.h'],
	arguments : [
		'--proto_path=@CURRENT_SOURCE_DIR@',
		'--c_out=@BUILD_DIR@',
		'@INPUT@'
	]
)
proto_files = [ 'proto/network.proto' ]
proto_gen_src = proto_gen.process(
	proto_files,
	preserve_path_from : meson.current_source_dir(),
)

# Library dependencies
libnm_dep = dependency('libnm', version : '>=1.14.6', required : false)

# Program sources
src = [
	'asset.c',
	'discover.c',
	'main.c',
	'settings.c',
	'websocket.c'
]
cflags = []

# Add network
if libnm_dep.found()
src += 'network.c'
cflags += '-DNETWORK_SUPPORT'
endif

# Generate program
executable(
	'melo',
	sources : [ src, proto_gen_src ],
	dependencies : [ libmelo_dep, libprotobuf_c_dep, libnm_dep ],
	c_args : cflags,
	install : true)

# Install additional proto files
install_data(
	proto_files,
	install_dir : proto_path
)

# Generate systemd service
conf = configuration_data()
conf.set('bindir', join_paths(get_option('prefix'), get_option('bindir')))
configure_file(
	input: 'melo.service.in',
	output: 'melo.service',
	configuration: conf,
	install: true,
	install_dir: '/lib/systemd/system'
)
