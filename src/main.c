/* ELang Compiler */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include "ast.h"
#include "config.h"
#include "semantics.h"

/* VERSION from config.h — MAX_PATH_LEN and MAX_MODULES from config.h */

/* --- Link list: stdlib .o files to link --- */
typedef struct {
    char *name;         /* "math", "std", etc. */
    char *path;         /* full path to .asm or .o */
} lib_entry_t;

/* --- Compiler context (replaces global state) --- */
typedef struct {
    /* Library link list */
    lib_entry_t libs[MAX_MODULES];
    int lib_count;
    /* Import resolution state */
    char import_stack[MAX_MODULES][MAX_PATH_LEN];
    int import_depth;
    char resolved[MAX_MODULES][MAX_PATH_LEN];
    int resolved_count;
} compiler_ctx_t;

static compiler_ctx_t g_ctx;

static void add_lib(const char *name, const char *path) {
    if (g_ctx.lib_count >= MAX_MODULES) {
        fprintf(stderr, "Error: too many imported libraries (max %d)\n", MAX_MODULES);
        return;
    }
    g_ctx.libs[g_ctx.lib_count].name = SAFE_STRDUP(name);
    g_ctx.libs[g_ctx.lib_count].path = SAFE_STRDUP(path);
    g_ctx.lib_count++;
}

static void free_libs(void) {
    for (int i = 0; i < g_ctx.lib_count; i++) {
        free(g_ctx.libs[i].name);
        free(g_ctx.libs[i].path);
    }
    g_ctx.lib_count = 0;
}

/* --- Usage --- */
static void usage(const char *p) {
    printf("ELang Compiler v%s\n", ELANG_VERSION);
    printf("Usage: %s [options] <file.el>\n", p);
    printf("  -o <file>    Output file\n");
    printf("  -t           Show tokens\n");
    printf("  -a           Show AST\n");
    printf("  -l           Library mode (no _start)\n");
    printf("  -check       Type check only (no codegen)\n");
    printf("  -fold        Constant fold only\n");
}

/* --- Token printer --- */
static void print_tokens(const char *src) {
    lexer_t l; lexer_init(&l, src);
    token_t t;
    do { t = lexer_next_token(&l);
        printf("%-12s ", token_type_name(t.type));
        if (t.length > 0) printf("'%.*s'", (int)t.length, t.value);
        printf("  %d:%d\n", t.line, t.col);
    } while (t.type != TOKEN_EOF);
}

/* --- AST printer --- */
static void print_ast(ast_node_t *n, int indent) {
    if (!n) return;
    for (int i = 0; i < indent; i++) printf("  ");
    switch (n->type) {
        case AST_INT_LIT: printf("IntLit(%ld)\n", n->as.int_val); break;
        case AST_STRING_LIT: printf("StringLit(\"%.*s\")\n", (int)n->as.string_val.length, n->as.string_val.value); break;
        case AST_IDENT: printf("Ident(%.*s)\n", (int)n->as.ident.name_len, n->as.ident.name); break;
        case AST_BINARY_OP:
            printf("BinaryOp(%s)\n", token_type_name(n->as.binary.op));
            print_ast(n->as.binary.left, indent+1);
            print_ast(n->as.binary.right, indent+1);
            break;
        case AST_RANGE:
            printf("Range\n");
            print_ast(n->as.range.left, indent+1);
            print_ast(n->as.range.right, indent+1);
            break;
        case AST_FN_DECL:
            printf("FnDecl(%.*s)\n", (int)n->as.fn_decl.name_len, n->as.fn_decl.name);
            print_ast(n->as.fn_decl.body, indent+1);
            break;
        case AST_LET:
            printf("Let(%s)\n", n->as.let.name);
            if (n->as.let.value) print_ast(n->as.let.value, indent+1);
            break;
        case AST_ASSIGN:
            printf("Assign\n");
            print_ast(n->as.assign.target, indent+1);
            print_ast(n->as.assign.value, indent+1);
            break;
        case AST_IF:
            printf("If\n");
            print_ast(n->as.if_stmt.condition, indent+1);
            print_ast(n->as.if_stmt.then_block, indent+1);
            break;
        case AST_WHILE:
            printf("While\n");
            print_ast(n->as.while_stmt.condition, indent+1);
            print_ast(n->as.while_stmt.body, indent+1);
            break;
        case AST_FOR:
            printf("For(");
            for (int fi = 0; fi < n->as.for_stmt.var_count; fi++) {
                if (fi > 0) printf(", ");
                printf("%.*s", (int)n->as.for_stmt.var_lens[fi], n->as.for_stmt.vars[fi]);
            }
            printf(")\n");
            print_ast(n->as.for_stmt.body, indent+1);
            break;
        case AST_BLOCK:
            printf("Block(%d)\n", n->as.block.count);
            for (int i = 0; i < n->as.block.count; i++)
                print_ast(n->as.block.stmts[i], indent+1);
            break;
        case AST_RETURN:
            printf("Return\n");
            if (n->as.ret.value) print_ast(n->as.ret.value, indent+1);
            break;
        case AST_CALL:
            printf("Call\n");
            print_ast(n->as.call.callee, indent+1);
            break;
        case AST_MATCH:
            printf("Match\n");
            print_ast(n->as.match_expr.value, indent+1);
            break;
        case AST_STRUCT_DECL: printf("Struct(%.*s)\n", (int)n->as.struct_decl.name_len, n->as.struct_decl.name); break;
        case AST_ENUM_DECL: printf("Enum(%.*s)\n", (int)n->as.enum_decl.name_len, n->as.enum_decl.name); break;
        case AST_OK_EXPR:
            printf("Ok\n");
            if (n->as.ok_expr.value) print_ast(n->as.ok_expr.value, indent+1);
            break;
        case AST_ERR_EXPR:
            printf("Err\n");
            if (n->as.err_expr.value) print_ast(n->as.err_expr.value, indent+1);
            break;
        case AST_PIPE:
            printf("Pipe\n");
            print_ast(n->as.pipe.left, indent+1);
            print_ast(n->as.pipe.right, indent+1);
            break;
        case AST_DEFER: printf("Defer\n"); print_ast(n->as.defer_stmt.expr, indent+1); break;
        case AST_TUPLE:
            printf("Tuple(%d)\n", n->as.tuple.count);
            for (int i = 0; i < n->as.tuple.count; i++)
                print_ast(n->as.tuple.elements[i], indent+1);
            break;
        case AST_ARRAY_LITERAL:
            printf("ArrayLiteral(%d)\n", n->as.array_literal.count);
            for (int i = 0; i < n->as.array_literal.count; i++)
                print_ast(n->as.array_literal.elements[i], indent+1);
            break;
        case AST_INDEX:
            printf("Index\n");
            print_ast(n->as.binary.left, indent+1);
            print_ast(n->as.binary.right, indent+1);
            break;
        case AST_LEN_EXPR: printf("Len\n"); print_ast(n->as.len_expr.operand, indent+1); break;
        case AST_RESULT_TYPE:
            printf("ResultType\n");
            print_ast(n->as.result_type.ok_type, indent+1);
            print_ast(n->as.result_type.err_type, indent+1);
            break;
        case AST_IMPORT_DECL: {
            const char *kind = n->as.import.is_stdlib ? "stdlib" : "project";
            if (n->as.import.alias)
                printf("Import(%s \"%.*s\" as %.*s)\n", kind,
                    (int)n->as.import.path_len, n->as.import.path,
                    (int)n->as.import.alias_len, n->as.import.alias);
            else
                printf("Import(%s \"%.*s\")\n", kind,
                    (int)n->as.import.path_len, n->as.import.path);
            break;
        }
        case AST_PROGRAM:
            printf("Program(%d)\n", n->as.program.count);
            for (int i = 0; i < n->as.program.count; i++)
                print_ast(n->as.program.declarations[i], indent+1);
            break;
        default: printf("Node(%d)\n", n->type); break;
    }
}

/* --- Parse a file and return AST --- */
static ast_node_t *parse_file(const char *path) {
    char *src = read_file(path);
    if (!src) return NULL;
    parser_t p; parser_init(&p, src);
    ast_node_t *ast = parser_parse(&p);
    if (!ast || p.has_error) { fprintf(stderr, "Parse error in %s\n", path); free(src); return NULL; }
    free(src);
    return ast;
}

/* --- Extract namespace from import path --- */
static void get_namespace(ast_node_t *import_node, char *ns, size_t ns_size) {
    if (import_node->as.import.alias) {
        snprintf(ns, ns_size, "%.*s",
            (int)import_node->as.import.alias_len, import_node->as.import.alias);
    } else {
        /* use last path component */
        const char *path = import_node->as.import.path;
        size_t len = import_node->as.import.path_len;
        const char *slash = memrchr(path, '/', len);
        if (slash) {
            snprintf(ns, ns_size, "%.*s", (int)(len - (slash - path + 1)), slash + 1);
        } else {
            snprintf(ns, ns_size, "%.*s", (int)len, path);
        }
    }
}

/* --- Merge a project module's AST into the main program --- */
static int merge_module_project(ast_node_t *main_prog, ast_node_t *module, const char *ns) {
    if (!module || module->type != AST_PROGRAM) return -1;

    int ns_len = strlen(ns);
    for (int i = 0; i < module->as.program.count; i++) {
        ast_node_t *decl = module->as.program.declarations[i];
        if (!decl) continue;
        if (decl->type == AST_FN_DECL && decl->as.fn_decl.is_export) {
            /* rename: func → ns_func */
            size_t old_len = decl->as.fn_decl.name_len;
            char *new_name = SAFE_MALLOC(ns_len + 1 + old_len + 1);
            memcpy(new_name, ns, ns_len);
            new_name[ns_len] = '_';
            memcpy(new_name + ns_len + 1, decl->as.fn_decl.name, old_len);
            new_name[ns_len + 1 + old_len] = '\0';
            free(decl->as.fn_decl.name);
            decl->as.fn_decl.name = new_name;
            decl->as.fn_decl.name_len = ns_len + 1 + old_len;
        }
        /* transfer declaration to main program */
        main_prog->as.program.count++;
        main_prog->as.program.declarations = SAFE_REALLOC(main_prog->as.program.declarations,
            sizeof(void*) * main_prog->as.program.count);
        main_prog->as.program.declarations[main_prog->as.program.count - 1] = decl;
        module->as.program.declarations[i] = NULL;
    }
    free(module->as.program.declarations);
    free(module);
    return 0;
}

/* --- Find stdlib file (.asm or .o) --- */
static char *find_library(const char *name, const char *base_dir) {
    static char path[MAX_PATH_LEN];

    /* Prefer .o (pre-built) over .asm (source) at each location */

    /* 1. Check ELANG_LIB_PATH */
    const char *env = getenv("ELANG_LIB_PATH");
    if (env) {
        snprintf(path, sizeof(path), "%s/%s.o", env, name);
        if (access(path, R_OK) == 0) return path;
        snprintf(path, sizeof(path), "%s/%s.asm", env, name);
        if (access(path, R_OK) == 0) return path;
    }

    /* 2. Check ./lib/ relative to source file */
    if (base_dir) {
        snprintf(path, sizeof(path), "%slib/%s.o", base_dir, name);
        if (access(path, R_OK) == 0) return path;
        snprintf(path, sizeof(path), "%slib/%s.asm", base_dir, name);
        if (access(path, R_OK) == 0) return path;
    }

    /* 3. Check ./lib/ relative to cwd */
    snprintf(path, sizeof(path), "lib/%s.o", name);
    if (access(path, R_OK) == 0) return path;
    snprintf(path, sizeof(path), "lib/%s.asm", name);
    if (access(path, R_OK) == 0) return path;

    /* 4. Check /usr/local/share/elang/lib/ */
    snprintf(path, sizeof(path), "/usr/local/share/elang/lib/%s.o", name);
    if (access(path, R_OK) == 0) return path;
    snprintf(path, sizeof(path), "/usr/local/share/elang/lib/%s.asm", name);
    if (access(path, R_OK) == 0) return path;

    return NULL;
}

/* --- Circular import guard --- */
/* --- Resolved modules (dedup) --- */
/* All state is in g_ctx */

static int import_in_stack(const char *path) {
    for (int i = 0; i < g_ctx.import_depth; i++)
        if (strcmp(g_ctx.import_stack[i], path) == 0) return 1;
    return 0;
}

static int module_resolved(const char *path) {
    for (int i = 0; i < g_ctx.resolved_count; i++)
        if (strcmp(g_ctx.resolved[i], path) == 0) return 1;
    return 0;
}

/* --- Resolve all imports in AST --- */
static int resolve_imports(ast_node_t *prog, const char *base_dir) {
    if (!prog || prog->type != AST_PROGRAM) return 0;

    /* Check for "core" import (forbidden) */
    for (int i = 0; i < prog->as.program.count; i++) {
        ast_node_t *d = prog->as.program.declarations[i];
        if (d && d->type == AST_IMPORT_DECL &&
            d->as.import.path_len == 4 &&
            memcmp(d->as.import.path, "core", 4) == 0) {
            fprintf(stderr, "Error at %d:%d: 'core' is auto-linked, do not import it\n", d->line, d->col);
            return -1;
        }
    }

    /* Collect import nodes */
    int imp_count = 0;
    for (int i = 0; i < prog->as.program.count; i++) {
        if (prog->as.program.declarations[i] &&
            prog->as.program.declarations[i]->type == AST_IMPORT_DECL)
            imp_count++;
    }
    if (imp_count == 0) return 0;

    /* Check for namespace conflicts (same namespace from different sources) */
    for (int i = 0; i < prog->as.program.count; i++) {
        ast_node_t *a = prog->as.program.declarations[i];
        if (!a || a->type != AST_IMPORT_DECL) continue;
        char ns_a[256];
        get_namespace(a, ns_a, sizeof(ns_a));
        for (int j = i + 1; j < prog->as.program.count; j++) {
            ast_node_t *b = prog->as.program.declarations[j];
            if (!b || b->type != AST_IMPORT_DECL) continue;
            char ns_b[256];
            get_namespace(b, ns_b, sizeof(ns_b));
            if (strcmp(ns_a, ns_b) == 0) {
                fprintf(stderr, "Error at %d:%d: namespace conflict — '%s' already imported\n",
                    b->line, b->col, ns_a);
                return -1;
            }
        }
    }

    /* Process each import */
    for (int i = 0; i < prog->as.program.count; i++) {
        ast_node_t *d = prog->as.program.declarations[i];
        if (!d || d->type != AST_IMPORT_DECL) continue;

        char ns[256];
        get_namespace(d, ns, sizeof(ns));

        if (!d->as.import.is_stdlib) {
            /* --- Project module --- */
            char file_path[MAX_PATH_LEN];
            snprintf(file_path, sizeof(file_path), "%s%.*s.el", base_dir,
                (int)d->as.import.path_len, d->as.import.path);

            /* Check for circular import */
            if (import_in_stack(file_path)) {
                fprintf(stderr, "Error at %d:%d: circular import of '%.*s'\n",
                    d->line, d->col,
                    (int)d->as.import.path_len, d->as.import.path);
                return -1;
            }
            /* Skip if already resolved */
            if (module_resolved(file_path)) continue;

            ast_node_t *mod_ast = parse_file(file_path);
            if (!mod_ast) {
                fprintf(stderr, "Error at %d:%d: cannot find module '%.*s' (%s)\n",
                    d->line, d->col,
                    (int)d->as.import.path_len, d->as.import.path, file_path);
                return -1;
            }
            /* Push onto import stack */
            snprintf(g_ctx.import_stack[g_ctx.import_depth], MAX_PATH_LEN, "%s", file_path);
            g_ctx.import_depth++;
            /* recursively resolve imports in the module */
            int rc = resolve_imports(mod_ast, base_dir);
            g_ctx.import_depth--;
            if (rc != 0) return -1;
            /* merge into main program */
            merge_module_project(prog, mod_ast, ns);
            /* mark as resolved */
            if (g_ctx.resolved_count < MAX_MODULES) {
                snprintf(g_ctx.resolved[g_ctx.resolved_count], MAX_PATH_LEN, "%s", file_path);
                g_ctx.resolved_count++;
            }
        } else {
            /* --- Standard library --- */
            char *lib_path = find_library(d->as.import.path, base_dir);
            if (!lib_path) {
                fprintf(stderr, "Error at %d:%d: cannot find library '%.*s'\n",
                    d->line, d->col,
                    (int)d->as.import.path_len, d->as.import.path);
                return -1;
            }
            add_lib(d->as.import.path, lib_path);
        }
    }

    /* NOTE: import nodes are NOT removed from AST.
     * Codegen needs them to register modules in its module table. */

    return 0;
}

/* --- Run shell command, return 0 on success --- */
/* --- Run a command safely without shell interpretation --- */
static int run_cmd(const char *cmd) {
    /* Parse command into argv for execvp (split on spaces) */
    char *argv[64];
    int argc = 0;
    char buf[MAX_PATH_LEN * 3];
    snprintf(buf, sizeof(buf), "%s", cmd);
    char *tok = strtok(buf, " ");
    while (tok && argc < 63) {
        argv[argc++] = tok;
        tok = strtok(NULL, " ");
    }
    argv[argc] = NULL;

    pid_t pid = fork();
    if (pid == 0) {
        execvp(argv[0], argv);
        fprintf(stderr, "Error: cannot exec '%s'\n", argv[0]);
        _exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) return WEXITSTATUS(status);
        return -1;
    }
    return -1;
}

/* Run a shell command (for build scripts with && and pipes) */
static int run_shell(const char *cmd) {
    fprintf(stderr, "  %s\n", cmd);
    pid_t pid = fork();
    if (pid == 0) {
        execlp("sh", "sh", "-c", cmd, NULL);
        _exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) return WEXITSTATUS(status);
        return -1;
    }
    return -1;
}

/* --- Assemble and link --- */
static int build_and_link(const char *output, const char *asm_file, int is_lib) {
    char cmd[MAX_PATH_LEN * 3];
    int rc;

    /* Step 1: assemble main .asm */
    snprintf(cmd, sizeof(cmd), "nasm -w-implicit-abs-deprecated -f elf64 %s -o %s.o.tmp", asm_file, output);
    rc = run_cmd(cmd);
    if (rc != 0) { fprintf(stderr, "Error: nasm failed on %s\n", asm_file); return rc; }

    /* Step 2: resolve core runtime — prefer pre-built .o, fallback to .asm */
    char core_lib[MAX_PATH_LEN] = {0};
    char *found = find_library("core", NULL);
    if (found) {
        strncpy(core_lib, found, sizeof(core_lib) - 1);
    }
    size_t core_len = strlen(core_lib);

    if (core_len > 2 && strcmp(core_lib + core_len - 2, ".o") == 0) {
        /* Pre-built .o — use directly, link against it */
    } else {
        /* .asm source — assemble it */
        run_cmd("mkdir -p lib/build");
        snprintf(cmd, sizeof(cmd), "nasm -w-implicit-abs-deprecated -f elf64 %s -o lib/build/core.o", core_lib);
        rc = run_cmd(cmd);
        if (rc != 0) { fprintf(stderr, "Error: nasm failed on core.asm\n"); return rc; }
        strncpy(core_lib, "lib/build/core.o", sizeof(core_lib) - 1);
    }

    /* Step 3: resolve stdlib .asm files — prefer pre-built .o */
    char lib_paths[MAX_MODULES][MAX_PATH_LEN];
    for (int i = 0; i < g_ctx.lib_count; i++) {
        const char *name = g_ctx.libs[i].name;
        found = find_library(name, NULL);
        if (found) {
            strncpy(lib_paths[i], found, sizeof(lib_paths[i]) - 1);
        } else {
            strncpy(lib_paths[i], g_ctx.libs[i].path, sizeof(lib_paths[i]) - 1);
        }

        size_t len = strlen(lib_paths[i]);
        if (len > 4 && strcmp(lib_paths[i] + len - 4, ".asm") == 0) {
            /* .asm source — assemble it */
            snprintf(cmd, sizeof(cmd), "nasm -w-implicit-abs-deprecated -f elf64 %s -o lib/build/%s.o",
                lib_paths[i], name);
            rc = run_cmd(cmd);
            if (rc != 0) { fprintf(stderr, "Error: nasm failed on %s\n", lib_paths[i]); return rc; }
            strncpy(lib_paths[i], "lib/build/", sizeof(lib_paths[i]) - 1);
            strncat(lib_paths[i], name, sizeof(lib_paths[i]) - strlen(lib_paths[i]) - 1);
            strncat(lib_paths[i], ".o", sizeof(lib_paths[i]) - strlen(lib_paths[i]) - 1);
        }
        /* else: already a .o path, use directly */
    }

    /* Step 4: link */
    if (is_lib) {
        snprintf(cmd, sizeof(cmd), "mv %s.o.tmp %s.o", output, output);
    } else {
        int pos = snprintf(cmd, sizeof(cmd), "ld %s.o.tmp %s", output, core_lib);
        for (int i = 0; i < g_ctx.lib_count; i++) {
            int written = snprintf(cmd + pos, sizeof(cmd) - pos, " %s", lib_paths[i]);
            if (written > 0 && pos + written < (int)sizeof(cmd)) pos += written;
        }
        snprintf(cmd + pos, sizeof(cmd) - pos, " -o %s", output);
    }
    rc = run_cmd(cmd);
    if (rc != 0) { fprintf(stderr, "Error: linker failed\n"); return rc; }

    /* Cleanup .o.tmp */
    snprintf(cmd, sizeof(cmd), "rm -f %s.o.tmp", output);
    run_cmd(cmd);

    return 0;
}

/* --- Main --- */
int main(int argc, char **argv) {
    const char *input = NULL, *output = "output";
    int show_tokens = 0, show_ast = 0, is_lib = 0, check_only = 0, fold_only = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) { usage(argv[0]); return 0; }
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) { printf("elc v%s\n", ELANG_VERSION); return 0; }
        if (strcmp(argv[i], "-t") == 0) show_tokens = 1;
        else if (strcmp(argv[i], "-a") == 0) show_ast = 1;
        else if (strcmp(argv[i], "-l") == 0) is_lib = 1;
        else if (strcmp(argv[i], "-check") == 0) check_only = 1;
        else if (strcmp(argv[i], "-fold") == 0) fold_only = 1;
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) output = argv[++i];
        else input = argv[i];
    }
    if (!input) { usage(argv[0]); return 1; }

    /* Extract base directory for module resolution */
    char base_dir[MAX_PATH_LEN] = "./";
    const char *last_slash = strrchr(input, '/');
    if (last_slash) {
        size_t dir_len = last_slash - input + 1;
        if (dir_len < sizeof(base_dir)) {
            memcpy(base_dir, input, dir_len);
            base_dir[dir_len] = '\0';
        }
    }

    char *src = read_file(input);
    if (!src) return 1;
    if (show_tokens) { print_tokens(src); free(src); return 0; }

    parser_t p; parser_init(&p, src);
    ast_node_t *ast = parser_parse(&p);
    if (!ast || p.has_error) { fprintf(stderr, "Parse error\n"); ast_free(ast); free(src); return 1; }
    if (show_ast) { print_ast(ast, 0); ast_free(ast); free(src); return 0; }

    /* Resolve imports */
    if (resolve_imports(ast, base_dir) != 0) {
        ast_free(ast); free(src); free_libs(); return 1;
    }

    /* Semantic analysis */
    sem_ctx_t sem;
    sem_init(&sem);
    sem_analyze(&sem, ast);
    if (sem.has_errors) {
        fprintf(stderr, "%d type error(s) found.\n", sem.error_count);
        sem_free(&sem); ast_free(ast); free(src); free_libs();
        return 1;
    }
    sem_free(&sem);

    if (check_only) {
        printf("elc: %s — OK (%d decls, no type errors)\n", input, ast->as.program.count);
        ast_free(ast); free(src); free_libs(); return 0;
    }

    if (fold_only) {
        sem_fold_constants(ast);
        printf("elc: %s — folded (%d decls)\n", input, ast->as.program.count);
        ast_free(ast); free(src); free_libs(); return 0;
    }

    /* Generate assembly */
    char asm_file[MAX_PATH_LEN];
    snprintf(asm_file, sizeof(asm_file), "%s.asm", output);
    FILE *out = fopen(asm_file, "w");
    if (!out) { fprintf(stderr, "Cannot open '%s'\n", asm_file); ast_free(ast); free(src); free_libs(); return 1; }
    codegen_t cg; codegen_init(&cg, out); cg.is_main = !is_lib; cg.source_file = input;
    int cg_err = codegen_program(&cg, ast);
    codegen_free(&cg); fclose(out);
    if (cg_err) { fprintf(stderr, "elc: codegen failed for %s\n", input); ast_free(ast); free(src); free_libs(); return 1; }

    /* Assemble and link */
    int build_err = build_and_link(output, asm_file, is_lib);
    ast_free(ast); free(src); free_libs();

    if (build_err) {
        fprintf(stderr, "elc: build failed for %s\n", input);
        return 1;
    }
    printf("elc: %s -> %s\n", input, output);
    return 0;
}
