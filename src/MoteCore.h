#ifndef MOTE_CORE_H
#define MOTE_CORE_H

#include <stdbool.h>

#include "Diagnostic.h"

#define MOTE_MAX_IDENTIFIER_LENGTH 128
#define MOTE_MAX_PATH_LENGTH 4096
#define MOTE_MAX_PACKAGES 128
#define MOTE_MAX_LINK_ARGS 256
#define MOTE_MAX_LINKER_ARGS 256
#define MOTE_MAX_EXTRA_C_SOURCES 32

#if defined(_WIN32)
#define MOTE_CORE_EXPORT __declspec(dllexport)
#else
#define MOTE_CORE_EXPORT
#endif

typedef struct MotePackage {
    char name[MOTE_MAX_IDENTIFIER_LENGTH];
    char root_path[MOTE_MAX_PATH_LENGTH];
    bool is_search_root;
    bool is_collection;
} MotePackage;

typedef struct MoteCompileOptions {
    bool emit_llvm;
    bool emit_exe;
    bool dump_ast;
    bool dump_mir;
    bool emit_debug_info;
    bool keep_llvm_output;
    const char *input_path;
    const char *requested_output_path;
    const char *llvm_output_path;
    const char *exe_output_path;
} MoteCompileOptions;

typedef struct MoteCompileArtifacts {
    char llvm_output_path[MOTE_MAX_PATH_LENGTH];
    char exe_output_path[MOTE_MAX_PATH_LENGTH];
} MoteCompileArtifacts;

typedef struct MoteCompileResult {
    bool ok;
    bool emitted_llvm;
    bool emitted_exe;
    Diagnostic diagnostic;
    MoteCompileArtifacts artifacts;
} MoteCompileResult;

MOTE_CORE_EXPORT int moteCliMain(int argn, char **argv);

MOTE_CORE_EXPORT MoteCompileResult moteCompile(const MoteCompileOptions *options,
                                               MotePackage *packages, int package_count,
                                               const char **driver_args, int driver_arg_count,
                                               const char **extra_c_sources, int extra_c_source_count,
                                               const char **linker_args, int linker_arg_count,
                                               const char *runtime_source_path);

MOTE_CORE_EXPORT int mote_core_compile_simple(const char *input_path,
                                              const char *output_path,
                                              bool emit_exe,
                                              bool emit_llvm,
                                              bool emit_debug_info);

MOTE_CORE_EXPORT void mote_core_set_host_argv0(const char *argv0);

#endif /* MOTE_CORE_H */
