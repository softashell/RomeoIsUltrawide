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
fs = import('fs')

libdir = join_paths(meson.project_source_root(), 'RIADMW_SomeUIFixes', 'lib')
# Archive is named libMinHook.x64.lib (nonstandard suffix); link by full path.
minhook_lib = libdir / 'libMinHook.x64.lib'
minhook_dep = declare_dependency(link_args: [minhook_lib])

# The Dumper-7 SDK needs MSVC-compatible semantics.
add_project_arguments(
  cxx.get_supported_arguments([
    '-fdeclspec',
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
  link_args: ['-Wl,--subsystem,windows', '-static'],
  dependencies: minhook_dep,
  name_prefix: '',
  name_suffix: 'asi',
  install: false)

# Minimal: only the SDK modules dllmain.cpp references.
minimal_mods = [
  'Basic', 'CoreUObject', 'Engine', 'UMG', 'Paper2D', 'SevGame',
  'WB_CinematicPauseMenu', 'WB_SimplePauseMenu', 'WB_PauseMenu_Option2',
]
minimal_sources = []
foreach m : minimal_mods
  f = meson.project_source_root() / 'RIADMW_SomeUIFixes' / 'SDK' / (m + '_functions.cpp')
  if not fs.is_file(f)
    f = meson.project_source_root() / 'RIADMW_SomeUIFixes' / 'SDK' / (m + '.cpp')
  endif
  if fs.is_file(f)
    minimal_sources += f
  else
    warning('minimal module missing: @0@'.format(m))
  endif
endforeach

shared_module('RomeoIsUltrawide_minimal',
  ['RIADMW_SomeUIFixes/dllmain.cpp'] + minimal_sources,
  include_directories: include_directories('RIADMW_SomeUIFixes', 'RIADMW_SomeUIFixes/include'),
  link_args: ['-Wl,--subsystem,windows', '-static'],
  dependencies: minhook_dep,
  name_prefix: '',
  name_suffix: 'asi',
  install: false)
