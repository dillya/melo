# Melo protocol-buffers library

# Proto generated headers install
install_dir = join_paths(get_option('includedir'), 'melo/proto')

# Get protobuf-c library
libprotobuf_c_dep = dependency('libprotobuf-c')

# Proto file source
proto_files = [
	'browser.proto',
	'player.proto',
	'playlist.proto',
	'tags.proto'
]

# Add protoc-c compiler to generate C files
protoc_c = find_program('protoc-c')

# Generate C files from proto files
proto_gen_src = custom_target('proto-src',
	input : proto_files,
	output : [
		'browser.pb-c.h',
		'player.pb-c.h',
		'playlist.pb-c.h',
		'tags.pb-c.h',
		'browser.pb-c.c',
		'player.pb-c.c',
		'playlist.pb-c.c',
		'tags.pb-c.c',
	],
	command : [
		protoc_c,
		'--proto_path=@CURRENT_SOURCE_DIR@',
		'--c_out=@OUTDIR@',
		'@INPUT@'
	],
	install : true,
	install_dir : [
		install_dir, install_dir, install_dir, install_dir,
		false, false, false, false
	]
)

# Compile library
libmelo_proto = shared_library(
	'melo_proto',
	proto_gen_src,
	dependencies : [libprotobuf_c_dep],
	install : true)

# Export library
libmelo_proto_dep = declare_dependency(
	sources : proto_gen_src,
	link_with : libmelo_proto,
	dependencies : [libprotobuf_c_dep])

# Create pkg-config
pkgg = import('pkgconfig')
pkgg.generate(
	libmelo_proto,
	requires: 'libprotobuf-c'
)
