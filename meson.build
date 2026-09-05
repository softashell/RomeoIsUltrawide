project('RIADMW_SomeUIFixes', 'cpp',
  version: '0.1.0',
  default_options: [
    'cpp_std=c++20',
    'default_library=shared',
    'b_vscrt=md',
    'warning_level=0',
    'buildtype=release',
  ])

cxx = meson.get_compiler('cpp')

libdir = join_paths(meson.project_source_root(), 'RIADMW_SomeUIFixes', 'lib')
# Archive is named libMinHook.x64.lib (nonstandard suffix); link by full path.
minhook_lib = libdir / 'libMinHook.x64.lib'
minhook_dep = declare_dependency(link_args: [minhook_lib])

# The Dumper-7 SDK needs MSVC-compatible semantics.
#
# Size notes (9.8 MB -> 239 KB for the full build):
# - The single biggest factor is -Wl,--exclude-all-symbols. The PE linker
#   auto-exports every global symbol when building a DLL with no explicit
#   exports, which marks ALL ~thousands of SDK wrappers as live and makes
#   --gc-sections a no-op. Excluding exports (the ASI loader only needs
#   DllMain) lets gc-sections discard everything the mod never references.
# - -ffunction-sections/-fdata-sections put every function in its own
#   section so gc-sections has something to discard; -Os (placed after
#   meson's -O3, so it wins) and -flto shrink the survivors further.
# - -s strips the ~1.7 MB symbol table; -static keeps the runtime embedded
#   (costs ~200 KB over llvm-mingw's default, buys a single-file mod with
#   no libstdc++/libc++/unwind DLL dependencies on the user's machine).
# - -fno-rtti is safe (no dynamic_cast/typeid in mod+SDK). Exceptions must
#   stay on: UnrealContainers/UtfN use throw for bounds checks.
add_project_arguments(
  cxx.get_supported_arguments([
    '-fdeclspec',
    '-flto',
    '-Os',
    '-ffunction-sections',
    '-fdata-sections',
    '-fno-rtti',
    '-Wno-c++11-narrowing',
    '-Wno-elaborated-enum-class',
    '-Wno-microsoft-cast',
    '-Wno-microsoft-include',
    '-Wno-pragma-pack',
  ]),
  language: 'cpp')

# Full SDK: every SDK/*_functions.cpp + Basic.cpp
sdk_sources = run_command(
  'sh', '-c', 'ls "$1"/*_functions.cpp "$1"/Basic.cpp | sort',
  'meson-get-sdk-sources', meson.project_source_root() / 'RIADMW_SomeUIFixes' / 'SDK',
  check: true).stdout().strip().split('\n')

shared_module('RomeoIsUltrawide',
  ['RIADMW_SomeUIFixes/dllmain.cpp'] + sdk_sources,
  include_directories: include_directories('RIADMW_SomeUIFixes', 'RIADMW_SomeUIFixes/include'),
  link_args: ['-Wl,--subsystem,windows', '-static', '-s', '-Wl,--gc-sections', '-Wl,--exclude-all-symbols', '-flto'],
  dependencies: minhook_dep,
  name_prefix: '',
  name_suffix: 'asi',
  install: false)

