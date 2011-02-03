import glob
import os.path

env = Environment()

shet_sources = glob.glob("lib/*.c")

env.ParseConfig("pkg-config json --cflags --libs")

env.Append(CCFLAGS = ['-g', '-Wall', '-O3',
                      '--std=c99',
                      '-D_POSIX_C_SOURCE=200112L'])

shet_lib = env.Library("shet", shet_sources)
shet_sharedlib = env.SharedLibrary("shet", shet_sources)


env["PREFIX"] = ARGUMENTS.get("installdir", "/usr")

inst_lib = env.Install("$PREFIX/lib", [shet_lib, shet_sharedlib])
inst_inc = env.Install("$PREFIX/include", "lib/shet.h")

def make_pkgconfig(target, source, env):
	open(str(target[0]), 'w').write(open(str(source[0])).read().replace("!PREFIX!", env["PREFIX"]))

pc = env.Command("shet.pc", "shet.pc.in", make_pkgconfig)

inst_pc = env.Install("$PREFIX/lib/pkgconfig", pc)

_install_lib = env.Alias("install-lib", inst_lib)
_install_inc = env.Alias("install-inc", inst_inc)
_install_pc = env.Alias("install-inc", inst_pc)
env.Alias("install", [_install_lib, _install_inc, _install_pc])


