env = Environment(CCFLAGS=['-W', '-Wextra', '-Wall', '-Wno-unused-parameter',
	'-Wno-missing-field-initializers'])

opts = Variables('plastichunt.conf')
opts.Add('config', 'Either "debug" or "release"', 'release')
opts.Add('prefix', 'Install prefix (used only for release builds)',
	'/usr/local')

opts.Update(env)
opts.Save('plastichunt.conf', env)

if env['config'] == 'debug':
	env.Append(CCFLAGS=['-g'], LDFLAGS=['-g'])
elif env['config'] == 'release':
	data_dir = env['prefix'] + '/share/plastichunt'
	env.Append(CCFLAGS=['-O3'],
		CPPDEFINES={'PH_DATA_DIRECTORY': '\\"' + data_dir + '\\"'})

env.ParseConfig('pkg-config --cflags --libs gtk+-2.0')
env.ParseConfig('pkg-config --cflags --libs gdk-pixbuf-2.0')
env.ParseConfig('pkg-config --cflags --libs sqlite3')
env.ParseConfig('pkg-config --cflags --libs libxml-2.0')
env.ParseConfig('pkg-config --cflags --libs libsoup-2.4')
env.ParseConfig('pkg-config --cflags --libs librsvg-2.0')
env.ParseConfig('pkg-config --cflags --libs webkit-1.0')

plastichunt = env.Program('plastichunt', Glob('src/*.c'))
env.Install('$prefix/bin', plastichunt)
env.Install('$prefix/share/plastichunt/sprites', Glob('data/sprites/*'))
env.Install('$prefix/share/plastichunt/ui', Glob('data/ui/*'))
env.Alias('install', env['prefix'])

# vim:tw=80:ft=python:
