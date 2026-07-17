/* ELang Compiler */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include "ast.h"
#include "semantics.h"

#define VERSION "0.1.0"

static void usage(const char *p) {
    printf("ELang Compiler v%s\n", VERSION);
    printf("Usage: %s [options] <file.el>\n", p);
    printf("  -o <file>    Output file\n");
    printf("  -t           Show tokens\n");
    printf("  -a           Show AST\n");
    printf("  -l           Library mode (no _start)\n");
    printf("  -check       Type check only (no codegen)\n");
    printf("  -fold        Constant fold only\n");
}

static void print_tokens(const char *src) {
    lexer_t l; lexer_init(&l, src);
    token_t t;
    do { t = lexer_next_token(&l);
        printf("%-12s ", token_type_name(t.type));
        if (t.length > 0) printf("'%.*s'", (int)t.length, t.value);
        printf("  %d:%d\n", t.line, t.col);
    } while (t.type != TOKEN_EOF);
}

static void print_ast(ast_node_t *n, int indent) {
    if (!n) return;
    for (int i = 0; i < indent; i++) printf("  ");
    switch (n->type) {
        case AST_INT_LIT: printf("IntLit(%ld)\n", n->as.int_val); break;
        case AST_STRING_LIT: printf("StringLit(\"%.*s\")\n", (int)n->as.string_val.length, n->as.string_val.value); break;
        case AST_IDENT: printf("Ident(%.*s)\n", (int)n->as.ident.name_len, n->as.ident.name); break;
        case AST_BINARY_OP: printf("BinaryOp(%s)\n", token_type_name(n->as.binary.op));
            print_ast(n->as.binary.left, indent+1); print_ast(n->as.binary.right, indent+1); break;
        case AST_RANGE: printf("Range\n");
            print_ast(n->as.range.left, indent+1); print_ast(n->as.range.right, indent+1); break;
        case AST_FN_DECL: printf("FnDecl(%.*s)\n", (int)n->as.fn_decl.name_len, n->as.fn_decl.name);
            print_ast(n->as.fn_decl.body, indent+1); break;
        case AST_LET: printf("Let(%s)\n", n->as.let.name);
            if (n->as.let.value) print_ast(n->as.let.value, indent+1); break;
        case AST_ASSIGN: printf("Assign\n");
            print_ast(n->as.assign.target, indent+1); print_ast(n->as.assign.value, indent+1); break;
        case AST_IF: printf("If\n");
            print_ast(n->as.if_stmt.condition, indent+1); print_ast(n->as.if_stmt.then_block, indent+1); break;
        case AST_WHILE: printf("While\n");
            print_ast(n->as.while_stmt.condition, indent+1); print_ast(n->as.while_stmt.body, indent+1); break;
        case AST_FOR: printf("For(%.*s)\n", (int)n->as.for_stmt.var_len, n->as.for_stmt.var);
            print_ast(n->as.for_stmt.body, indent+1); break;
        case AST_BLOCK: printf("Block(%d)\n", n->as.block.count);
            for (int i = 0; i < n->as.block.count; i++) print_ast(n->as.block.stmts[i], indent+1); break;
        case AST_RETURN: printf("Return\n"); if (n->as.ret.value) print_ast(n->as.ret.value, indent+1); break;
        case AST_CALL: printf("Call\n"); print_ast(n->as.call.callee, indent+1); break;
        case AST_MATCH: printf("Match\n"); print_ast(n->as.match_expr.value, indent+1); break;
        case AST_STRUCT_DECL: printf("Struct(%.*s)\n", (int)n->as.struct_decl.name_len, n->as.struct_decl.name); break;
        case AST_ENUM_DECL: printf("Enum(%.*s)\n", (int)n->as.enum_decl.name_len, n->as.enum_decl.name); break;
        case AST_OK_EXPR: printf("Ok\n"); if (n->as.ok_expr.value) print_ast(n->as.ok_expr.value, indent+1); break;
        case AST_ERR_EXPR: printf("Err\n"); if (n->as.err_expr.value) print_ast(n->as.err_expr.value, indent+1); break;
        case AST_PIPE: printf("Pipe\n");
            print_ast(n->as.pipe.left, indent+1); print_ast(n->as.pipe.right, indent+1); break;
        case AST_DEFER: printf("Defer\n"); print_ast(n->as.defer_stmt.expr, indent+1); break;
        case AST_WHEN: printf("When\n");
            print_ast(n->as.when_expr.condition, indent+1); print_ast(n->as.when_expr.then_block, indent+1); break;
        case AST_TUPLE: printf("Tuple(%d)\n", n->as.tuple.count);
            for (int i = 0; i < n->as.tuple.count; i++) print_ast(n->as.tuple.elements[i], indent+1); break;
        case AST_ARRAY_LITERAL: printf("ArrayLiteral(%d)\n", n->as.array_literal.count);
            for (int i = 0; i < n->as.array_literal.count; i++) print_ast(n->as.array_literal.elements[i], indent+1); break;
        case AST_INDEX: printf("Index\n");
            print_ast(n->as.binary.left, indent+1); print_ast(n->as.binary.right, indent+1); break;
        case AST_LEN_EXPR: printf("Len\n"); print_ast(n->as.len_expr.operand, indent+1); break;
        case AST_USING: printf("Using(\"%.*s\")\n", (int)n->as.using_decl.path_len, n->as.using_decl.path); break;
        case AST_PROGRAM: printf("Program(%d)\n", n->as.program.count);
            for (int i = 0; i < n->as.program.count; i++) print_ast(n->as.program.declarations[i], indent+1); break;
        default: printf("Node(%d)\n", n->type); break;
    }
}

/* Parse a file and return AST */
static ast_node_t *parse_file(const char *path) {
    char *src = read_file(path);
    if (!src) return NULL;
    parser_t p; parser_init(&p, src);
    ast_node_t *ast = parser_parse(&p);
    if (!ast) { fprintf(stderr, "Parse error in %s\n", path); free(src); return NULL; }
    /* source is no longer needed — AST has copied all string values */
    free(src);
    return ast;
}

/* Merge source modules into the main program AST, renaming exported functions */
static void merge_modules(ast_node_t *main_prog, ast_node_t *module, const char *module_name) {
    if (!module || module->type != AST_PROGRAM) return;
    int name_len = strlen(module_name);
    for (int i = 0; i < module->as.program.count; i++) {
        ast_node_t *decl = module->as.program.declarations[i];
        if (decl && decl->type == AST_FN_DECL && decl->as.fn_decl.is_export) {
            /* rename: add → module_add */
            char *new_name = malloc(name_len + 1 + decl->as.fn_decl.name_len + 1);
            memcpy(new_name, module_name, name_len);
            new_name[name_len] = '_';
            memcpy(new_name + name_len + 1, decl->as.fn_decl.name, decl->as.fn_decl.name_len);
            new_name[name_len + 1 + decl->as.fn_decl.name_len] = '\0';
            free(decl->as.fn_decl.name);
            decl->as.fn_decl.name = new_name;
            decl->as.fn_decl.name_len = name_len + 1 + decl->as.fn_decl.name_len;
            /* add to main program */
            main_prog->as.program.count++;
            main_prog->as.program.declarations = realloc(main_prog->as.program.declarations,
                sizeof(void*) * main_prog->as.program.count);
            main_prog->as.program.declarations[main_prog->as.program.count - 1] = decl;
            module->as.program.declarations[i] = NULL;
        }
    }
}

/* Resolve using directives: find and parse imported modules */
static void resolve_modules(ast_node_t *prog, const char *base_dir) {
    if (!prog || prog->type != AST_PROGRAM) return;

    /* Phase 1: collect all using paths */
    int using_count = 0;
    for (int i = 0; i < prog->as.program.count; i++) {
        if (prog->as.program.declarations[i] && prog->as.program.declarations[i]->type == AST_USING)
            using_count++;
    }
    if (using_count == 0) return;

    char **paths = malloc(sizeof(char*) * using_count);
    int pi = 0;
    for (int i = 0; i < prog->as.program.count; i++) {
        ast_node_t *decl = prog->as.program.declarations[i];
        if (decl && decl->type == AST_USING) {
            char *p = malloc(512);
            snprintf(p, 512, "%s%.*s.el", base_dir,
                (int)decl->as.using_decl.path_len, decl->as.using_decl.path);
            paths[pi++] = p;
        }
    }

    /* Phase 2: parse and merge each module */
    for (int i = 0; i < using_count; i++) {
        ast_node_t *mod_ast = parse_file(paths[i]);
        if (mod_ast) {
            resolve_modules(mod_ast, base_dir);
            /* extract module name from path: "math.el" → "math" */
            const char *slash = strrchr(paths[i], '/');
            const char *fname = slash ? slash + 1 : paths[i];
            char mod_name[256];
            snprintf(mod_name, sizeof(mod_name), "%s", fname);
            char *dot = strrchr(mod_name, '.');
            if (dot) *dot = '\0';
            merge_modules(prog, mod_ast, mod_name);
            /* leak mod_ast shell — declarations owned by main_prog */
        } else {
            fprintf(stderr, "Warning: could not import '%s'\n", paths[i]);
        }
        free(paths[i]);
    }
    free(paths);

    /* Phase 3: compact — remove USING entries */
    int dst = 0;
    for (int src = 0; src < prog->as.program.count; src++) {
        ast_node_t *d = prog->as.program.declarations[src];
        if (d && d->type != AST_USING)
            prog->as.program.declarations[dst++] = d;
    }
    prog->as.program.count = dst;
}

int main(int argc, char **argv) {
    const char *input = NULL, *output = "output.asm";
    int show_tokens = 0, show_ast = 0, is_lib = 0, check_only = 0, fold_only = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) { usage(argv[0]); return 0; }
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) { printf("elc v%s\n", VERSION); return 0; }
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
    char base_dir[512] = "./";
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
    if (!ast) { fprintf(stderr, "Parse error\n"); free(src); return 1; }
    if (show_ast) { print_ast(ast, 0); ast_free(ast); free(src); return 0; }

    /* Resolve module imports */
    resolve_modules(ast, base_dir);

    /* Semantic analysis */
    sem_ctx_t sem;
    sem_init(&sem);
    sem_analyze(&sem, ast);
    if (sem.has_errors) {
        fprintf(stderr, "%d type error(s) found.\n", sem.error_count);
        sem_free(&sem);
        ast_free(ast); free(src);
        return 1;
    }
    sem_free(&sem);

    if (check_only) {
        printf("elc: %s — OK (%d decls, no type errors)\n", input, ast->as.program.count);
        ast_free(ast); free(src); return 0;
    }

    if (fold_only) {
        printf("elc: %s — folded (%d decls)\n", input, ast->as.program.count);
        ast_free(ast); free(src); return 0;
    }

    FILE *out = fopen(output, "w");
    if (!out) { fprintf(stderr, "Cannot open '%s'\n", output); ast_free(ast); free(src); return 1; }
    codegen_t cg; codegen_init(&cg, out); cg.is_main = !is_lib;
    codegen_program(&cg, ast); codegen_free(&cg); fclose(out);
    printf("elc: %s -> %s (%d decls)\n", input, output, ast->as.program.count);
    ast_free(ast); free(src); return 0;
}
