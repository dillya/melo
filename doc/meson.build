# Melo library documentation

if not find_program('hotdoc', required : false).found()
    message('Hotdoc not found, skip documentation build')
    subdir_done()
endif

hotdoc = import('hotdoc')

hotdoc.generate_doc('melo',
	project_version : '1.0.0',
	sitemap : 'sitemap.txt',
	index : 'index.md',
	c_sources: [ '../include/melo/*.h', '../lib/*.c' ],
	c_smart_index: true,
	languages : ['c'],
	dependencies : [libmelo_dep],
	install : false)

