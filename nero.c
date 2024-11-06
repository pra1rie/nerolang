#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "nlist.h"
#include "nstring.h"

enum {
    // constants
    TK_EOF, TK_NIL, TK_FALSE, TK_TRUE, TK_NUMBER, TK_STRING, TK_WORD,
    // operators
    TK_LPAREN, TK_RPAREN, TK_LBRACK, TK_RBRACK, TK_LSQUARE, TK_RSQUARE, TK_COMMA,
    TK_EQ, TK_NOT, TK_EQEQ, TK_NEQ, TK_LT, TK_GT, TK_LEQ, TK_GEQ, TK_AND, TK_OR,
    TK_PLUS, TK_MINUS, TK_MUL, TK_DIV, TK_MOD, TK_POW,
    // keywords
    TK_DEF, TK_RETURN, TK_IF, TK_ELIF, TK_ELSE, TK_WHILE, TK_BREAK, TK_NEXT,
    // builtin functions
    TK_ECHO, TK_READ, TK_QUIT,
    NUM_TOKENS,
};

// XXX: unkeywordify 'echo', 'read' and 'quit'
// XXX: implement pow '**', 'import'

const char *ops[] = {
    "(", ")", "{", "}", "[", "]", ",", "=", "!",
    "==", "!=", "<", ">", "<=", ">=", "&&", "||",
    "+", "-", "*", "/", "%", "**",
};

const char *keywords[] = {
    "def", "return", "if", "elif", "else", "while", "break", "next",
    "echo", "read", "quit",
};

const char *booleans[] = {"nil", "false", "true"};

typedef struct {
    int kind, line, word;
    String value;
} Token;

typedef struct Foreign {
    const char *name;
    struct Value (*func)(int argc, struct Value *argv);
} Foreign;

typedef LIST(Token) TokenList;
typedef LIST(String) StringList;
typedef LIST(struct Value) ValueList;
typedef LIST(struct Variable) VariableList;
typedef LIST(struct Function) FunctionList;
typedef LIST(Foreign) ForeignList;

enum { V_FREE, V_NOFREE };
enum { T_NIL, T_BOOL, T_NUMBER, T_STRING, T_LIST };
enum { RET_NO, RET_RETURN, RET_BREAK, RET_NEXT };

typedef struct Value {
    uint8_t type, free;
    union {
        double as_num;
        String as_str;
        ValueList as_list;
    };
} Value;

typedef struct Variable {
    int exists;
    Value value;
} Variable;

typedef struct {
    int start, end;
} Block;

#define MAX_ARGS 1024
typedef struct Function {
    int exists, num_args;
    int args[MAX_ARGS];
    VariableList vars;
    Block code;
} Function;

typedef struct {
    char *file;
    int ip, ret;
    TokenList code;
    VariableList vars;
    FunctionList funs;
    ForeignList extn;
    Function *fn;
} Nero;

static int line = 1;
static Value args_list;
static StringList lexer_words = LIST_EMPTY();

static inline int get_word(String s) {
    if (!lexer_words.alloc) lexer_words = (StringList) LIST_ALLOC(String);
    for (int i = 0; i < lexer_words.sz; ++i) {
        if (STRCMPS(lexer_words.ptr[i], s)) return i;
    }
    LIST_PUSH(lexer_words, s);
    return lexer_words.sz-1;
}

static inline char skip_space(FILE *fp) {
    char c = fgetc(fp);
    while (c != EOF && isspace(c)) {
        if (c == '\n') ++line;
        c = fgetc(fp);
    }
    if (c == '#') {
        while (c != EOF && c != '\n') c = fgetc(fp);
        ungetc(c, fp);
        return skip_space(fp);
    }
    return c;
}

static inline int is_operator(char op[2], int sz) {
    for (int i = 0; i < LENGTH(ops); ++i) {
        if (!strncmp(ops[i], op, sz))
            return 1;
    }
    return 0;
}

static inline int is_word(char c) {
    return !(is_operator(&c, 1) || isspace(c) || c == '#' || c == '\"');
}

static inline char escape(char c) {
    switch (c) {
    case '\\': return '\\';
    case '\"': return '\"';
    case '\'': return '\'';
    case 'n': return '\n';
    case 'r': return '\r';
    case 't': return '\t';
    case 'e': return '\033';
    case 's': return ' ';
    default: return c;
    }
}

static inline Token next_token(FILE *fp) {
    char c = skip_space(fp);
    Token tok = {.kind = TK_EOF, .line = line};
    tok.value.alloc = 0;
    if (c == EOF) return tok;
    if (isdigit(c)) {
        tok.kind = TK_NUMBER;
        tok.value = STRALLOC();
        while (c != EOF && (isdigit(c) || c == '.')) {
            LIST_PUSH(tok.value, c); c = fgetc(fp);
        }
        goto end;
    }
    if (c == '\"') {
        tok.kind = TK_STRING;
        tok.value = STRALLOC();
        do {
            c = fgetc(fp);
            if (c == '\\') {
                LIST_PUSH(tok.value, escape(fgetc(fp)));
            } else {
                LIST_PUSH(tok.value, c);
            }
        } while (c != EOF && c != '\"');
        LIST_POP(tok.value); // remove last '\"'
        return tok; // don't unget last '\"'
    }
    if (is_operator(&c, 1)) {
        char op[2] = {c, fgetc(fp)};
        int sz = 2;
        if (!is_operator(op, 2)) {
            sz = 1; ungetc(op[1], fp);
        }
        for (int i = 0; i < LENGTH(ops); ++i) {
            if (!strncmp(ops[i], op, sz)) {
                tok.kind = i+TK_LPAREN;
                break;
            }
        }
        return tok;
    }

    tok.value = STRALLOC();
    tok.kind = TK_WORD;
    while (c != EOF && is_word(c)) {
        LIST_PUSH(tok.value, c);
        c = fgetc(fp);
    }
    for (int i = 0; i < LENGTH(booleans); ++i) {
        if (STRCMPP(tok.value, booleans[i])) {
            STRFREE(tok.value);
            tok.kind = i+TK_NIL;
            goto end;
        }
    }
    for (int i = 0; i < LENGTH(keywords); ++i) {
        if (STRCMPP(tok.value, keywords[i])) {
            STRFREE(tok.value);
            tok.kind = i+TK_DEF;
            goto end;
        }
    }
    tok.word = get_word(tok.value);
    // STRFREE(tok.value);
end:
    ungetc(c, fp);
    return tok;
}

void tokenize(Nero *nr, char *file) {
    FILE *fp = fopen(file, "r");
    if (!fp) goto fail;
    if (!nr->file) nr->file = file;
    if (!nr->code.alloc) nr->code = (TokenList) LIST_ALLOC(Token);
    Token tok;
    do {
        tok = next_token(fp);
        LIST_PUSH(nr->code, tok);
    } while (tok.kind != TK_EOF);
    nr->ip = 0;
    fclose(fp);
    return;
fail:
    if (fp) fclose(fp);
    fprintf(stderr, "Error: Could not open file '%s'\n", file);
    exit(1);
}

#define PEEK(n) nr->code.ptr[nr->ip+n]
#define ADVANCE(n) nr->ip += n
#define INBOUND() nr->ip < nr->code.sz

Value exec_expr(Nero *nr);
Value exec_block(Nero *nr, Block blk);

static inline char *errpos(Nero *nr, int ln) {
    char err[1024];
    sprintf(err, "(file \"%s\", line %d)", nr->file, ln);
/*
    sprintf(err, "(file \"%s\", line %d, in \"%.*s\")",
        nr->file, ln, nr->fn->name.sz, nr->fn->name.ptr);
*/
    return err;
}

static inline void nero_free(Value val) {
    if (val.free == V_NOFREE) return;
    if (val.type == T_STRING && val.as_str.alloc) {
        STRFREE(val.as_str);
    }
    else if (val.type == T_LIST && val.as_list.alloc) {
        for (int i = 0; i < val.as_list.sz; ++i) {
            val.as_list.ptr[i].free = V_FREE;
            nero_free(val.as_list.ptr[i]);
        }
        LIST_FREE(val.as_list);
    }
}

Value nero_string(Value val) {
    String str = STRALLOC();
    char num[100];
    switch (val.type) {
    case T_BOOL:
        strcatp(&str, val.as_num? "true" : "false");
        break;
    case T_NUMBER:
        sprintf(num, "%.2lf", val.as_num);
        strcatp(&str, num);
        break;
    case T_STRING:
        strcats(&str, &val.as_str);
        break;
    case T_LIST:
        strcatp(&str, "[");
        for (int i = 0; i < val.as_list.sz; ++i) {
            if (i > 0) strcatp(&str, ", ");
            Value v = nero_string(val.as_list.ptr[i]);
            strcats(&str, &v.as_str);
            nero_free(v);
        }
        strcatp(&str, "]");
        break;
    default:
        strcatp(&str, "nil");
        break;
    }
    return (Value) {T_STRING, .free = V_FREE, .as_str = str};
}

void nero_echo(Value val) {
    Value str = nero_string(val);
    fprintf(stdout, "%.*s", str.as_str.sz, str.as_str.ptr);
    nero_free(str);
    nero_free(val);
}

Value nero_read() {
    char *l = NULL; size_t n = 0;
    getline(&l, &n, stdin);
    String str = STRALLOC();
    strcatp(&str, l);
    LIST_POP(str); // remove last \n
    free(l);
    return (Value) {T_STRING, .free = V_FREE, .as_str = str};
}

int nero_true(Value v) {
    switch (v.type) {
    case T_BOOL:
    case T_NUMBER:
        return v.as_num;
    case T_STRING:
        return v.as_str.sz;
    case T_LIST:
        return v.as_list.sz;
    default:
        return 0;
    }
}

Value nero_equals(Value a, Value b) {
    Value res = {T_BOOL, .as_num = 0};
    if (a.type != b.type) return res;
    switch (a.type) {
    case T_NIL:
        res.as_num = 1; break;
    case T_BOOL:
    case T_NUMBER:
        res.as_num = a.as_num == b.as_num;
        break;
    case T_STRING:
        res.as_num = STRCMPS(a.as_str, b.as_str);
        break;
    case T_LIST: {
        if (a.as_list.sz != b.as_list.sz) break;
        res.as_num = 1;
        for (int i = 0; i < a.as_list.sz; ++i) {
            if (!nero_equals(a.as_list.ptr[i], b.as_list.ptr[i]).as_num) {
                res.as_num = 0;
                break;
            }
        }
    } break;
    default:
        break;
    }
    return res;
}

Value nero_compare(Value a, Value b, uint8_t k) {
    Value res = {T_BOOL, .as_num = 0};
    switch (k) {
    case TK_LT:
        res.as_num = a.as_num < b.as_num;
        break;
    case TK_LEQ:
        res.as_num = a.as_num <= b.as_num;
        break;
    case TK_GT:
        res.as_num = a.as_num > b.as_num;
        break;
    case TK_GEQ:
        res.as_num = a.as_num >= b.as_num;
        break;
    default:
        break;
    }
    return res;
}

Value nero_copy(Value val) {
    Value res = {val.type, .free = V_FREE};
    switch (val.type) {
    case T_BOOL:
    case T_NUMBER:
        res.as_num = val.as_num;
        break;
    case T_STRING:
        res.as_str = STRALLOC();
        strcats(&res.as_str, &val.as_str);
        break;
    case T_LIST:
        res.as_list = (ValueList) LIST_ALLOCN(Value, val.as_list.alloc);
        for (int i = 0; i < val.as_list.sz; ++i)
            LIST_PUSH(res.as_list, nero_copy(val.as_list.ptr[i]));
        break;
    }
    return res;
}

#define EXPECT(exp) if (argc != exp) { fprintf(stderr, "Error: expected %d argument(s), got %d\n", exp, argc); exit(1); }

Value nero_typeof(int argc, Value *argv) {
    EXPECT(1);
    Value typ = {T_STRING, .free = V_FREE, .as_str = STRALLOC()};
    switch (argv[0].type) {
    case T_BOOL:
        strcatp(&typ.as_str, "bool"); break;
    case T_NUMBER:
        strcatp(&typ.as_str, "number"); break;
    case T_STRING:
        strcatp(&typ.as_str, "string"); break;
    case T_LIST:
        strcatp(&typ.as_str, "list"); break;
    default:
        strcatp(&typ.as_str, "nil"); break;
    }
    return typ;
}

Value nero_stringfy(int argc, Value *argv) {
    Value str = {T_STRING, .free = V_FREE, .as_str = STRALLOC()};
    for (int i = 0; i < argc; ++i) {
        Value s = nero_string(argv[i]);
        strcats(&str.as_str, &s.as_str);
        nero_free(s);
    }
    return str;
}

Value nero_push(int argc, Value *argv) {
    EXPECT(2);
    if (argv[0].type == T_STRING) {
        Value s = nero_string(argv[1]);
        strcats(&argv[0].as_str, &s.as_str);
        nero_free(s);
        return nero_copy(argv[0]);
    }
    if (argv[0].type != T_LIST) {
        fprintf(stderr, "Error: expected list\n"); exit(1);
    }
    LIST_PUSH(argv[0].as_list, nero_copy(argv[1]));
    return nero_copy(argv[0]);
}

Value nero_pop(int argc, Value *argv) {
    EXPECT(1);
    if (argv[0].type == T_STRING) {
        if (argv[0].as_str.sz == 0) goto fail;
        LIST_POP(argv[0].as_str);
        return nero_copy(argv[0]);
    }
    if (argv[0].type != T_LIST) {
        fprintf(stderr, "Error: expected list\n"); exit(1);
    }
    if (argv[0].as_list.sz == 0) goto fail;
    ValueList *list = &argv[0].as_list;
    nero_free(list->ptr[list->sz-1]);
    LIST_POPP(list);
    return nero_copy(argv[0]);
fail:
    fprintf(stderr, "Error: list index out of range\n");
    exit(1);
}

Value nero_len(int argc, Value *argv) {
    EXPECT(1);
    if (argv[0].type == T_STRING)
        return (Value) {T_NUMBER, .as_num = argv[0].as_str.sz};
    if (argv[0].type != T_LIST) {
        fprintf(stderr, "Error: expected list\n"); exit(1);
    }
    return (Value) {T_NUMBER, .as_num = argv[0].as_list.sz};
}

Value nero_chr(int argc, Value *argv) {
    EXPECT(1);
    if (argv[0].type != T_NUMBER) {
        fprintf(stderr, "Error: expected number\n"); exit(1);
    }
    String str = STRALLOC();
    LIST_PUSH(str, argv[0].as_num);
    return (Value){T_STRING, .free = V_FREE, .as_str = str};
}

Value nero_ord(int argc, Value *argv) {
    EXPECT(1);
    if (argv[0].type != T_STRING) {
        fprintf(stderr, "Error: expected string\n"); exit(1);
    }
    if (argv[0].as_str.sz != 1) {
        fprintf(stderr, "Error: expected string of size 1\n"); exit(1);
    }
    return (Value){T_NUMBER, .as_num = argv[0].as_str.ptr[0]};
}

Value nero_system(int argc, Value *argv) {
    EXPECT(1);
    Value str = nero_string(argv[0]);
    char cmd[str.as_str.sz];
    sprintf(cmd, "%.*s", str.as_str.sz, str.as_str.ptr);
    nero_free(str);
    return (Value) {T_NUMBER, .as_num = system(cmd)};
}

Value nero_write_file(int argc, Value *argv) {
    EXPECT(2);
    if (argv[0].type != T_STRING) {
        fprintf(stderr, "Error: expected string\n"); exit(1);
    }
    char file[argv[0].as_str.sz];
    sprintf(file, "%.*s", argv[0].as_str.sz, argv[0].as_str.ptr);
    FILE *fp = fopen(file, "w+");
    if (!fp) {
        fprintf(stderr, "Error: could not write file \"%s\"\n", file); exit(1);
    }
    Value str = nero_string(argv[1]);
    fwrite(str.as_str.ptr, sizeof(char), str.as_str.sz, fp);
    nero_free(str);
    fclose(fp);
    return (Value){T_NIL};
}

Value nero_read_file(int argc, Value *argv) {
    EXPECT(1);
    if (argv[0].type != T_STRING) {
        fprintf(stderr, "Error: expected string\n"); exit(1);
    }
    char file[argv[0].as_str.sz];
    sprintf(file, "%.*s", argv[0].as_str.sz, argv[0].as_str.ptr);
    FILE *fp = fopen(file, "r");
    int sz = 0;
    String text;
    if (!fp) {
        fprintf(stderr, "Error: could not read file \"%s\"\n", file); exit(1);
    }
    fseek(fp, 0, SEEK_END);
    if (sz = ftell(fp)) text = STRALLOCN(sz);
    fseek(fp, 0, SEEK_SET);
    if (fread(text.ptr, sizeof(char), sz, fp) != sz) {
        fprintf(stderr, "Error: failed to read file \"%s\"\n", file); exit(1);
    }
    text.sz = sz;
    fclose(fp);
    return (Value){T_STRING, .free = V_FREE, .as_str = text};
}

Value nero_contains(int argc, Value *argv) {
    EXPECT(2);
    if (argv[0].type == T_STRING) {
        if (argv[1].type != T_STRING) {
            fprintf(stderr, "Error: expected string\n"); exit(1);
        }
        char *str = strstr(argv[0].as_str.ptr, argv[1].as_str.ptr);
        return (Value) {T_BOOL, .as_num = (str != NULL)};
    }
    if (argv[0].type != T_LIST) {
        fprintf(stderr, "Error: expected list\n"); exit(1);
    }
    for (int i = 0; i < argv[0].as_list.sz; ++i) {
        Value v = argv[0].as_list.ptr[i];
        if (nero_equals(v, argv[1]).as_num) return (Value) {T_BOOL, .as_num = 1};
    }
    return (Value) {T_BOOL, .as_num = 0};
}

Value nero_split(int argc, Value *argv) {
    EXPECT(2);
    if (argv[0].type != T_STRING) {
        fprintf(stderr, "Error: expected string\n"); exit(1);
    }
    if (argv[1].type != T_LIST) {
        fprintf(stderr, "Error: expected list\n"); exit(1);
    }

    Value list = {T_LIST, .free = V_FREE, .as_list = LIST_ALLOC(Value)};
    Value tok = {T_STRING, .free = V_FREE, .as_str = STRALLOC()};
    for (int i = 0; i < argv[0].as_str.sz; ++i) {
        char ch = argv[0].as_str.ptr[i];
        Value str_i = {T_STRING, .free = V_NOFREE, .as_str = {.sz = 1, .ptr = &ch}};
        Value args[2] = {argv[1], str_i};
        if (nero_contains(2, args).as_num) {
            LIST_PUSH(list.as_list, nero_copy(tok));
            tok.as_str.sz = 0;
        } else {
            LIST_PUSH(tok.as_str, ch);
        }
    }
    LIST_PUSH(list.as_list, nero_copy(tok));
    nero_free(tok);
    return list;
}

Value nero_trim(int argc, Value *argv) {
    EXPECT(2);
    if (argv[1].type != T_LIST) {
        fprintf(stderr, "Error: expected list\n"); exit(1);
    }
    if (argv[0].type == T_STRING) {
        Value str = {T_STRING, .free = V_FREE, .as_str = STRALLOC()};
        for (int i = 0; i < argv[0].as_str.sz; ++i) {
            Value str_i = {T_STRING, .as_str = {.sz = 1, .ptr = &argv[0].as_str.ptr[i]}};
            Value args[2] = {argv[1], str_i};
            if (!nero_contains(2, args).as_num)
                strcats(&str.as_str, &str_i.as_str);
        }
        return str;
    }
    if (argv[0].type != T_LIST) {
        fprintf(stderr, "Error: expected list\n"); exit(1);
    }
    Value list = {T_LIST, .free = V_FREE, .as_list = LIST_ALLOC(Value)};
    for (int i = 0; i < argv[0].as_list.sz; ++i) {
        Value args[2] = {argv[1], argv[0].as_list.ptr[i]};
        if (!nero_contains(2, args).as_num)
            LIST_PUSH(list.as_list, nero_copy(argv[0].as_list.ptr[i]));
    }
    return list;
}

Value nero_arguments(int argc, Value *argv) {
    EXPECT(0);
    return nero_copy(args_list);
}

void nero_init_foreign(Nero *nr) {
    nr->extn = (ForeignList) LIST_ALLOC(Foreign);
    LIST_PUSH(nr->extn, ((Foreign) { "push", &nero_push }));
    LIST_PUSH(nr->extn, ((Foreign) { "pop", &nero_pop }));
    LIST_PUSH(nr->extn, ((Foreign) { "len", &nero_len }));
    LIST_PUSH(nr->extn, ((Foreign) { "chr", &nero_chr }));
    LIST_PUSH(nr->extn, ((Foreign) { "ord", &nero_ord }));
    LIST_PUSH(nr->extn, ((Foreign) { "typeof", &nero_typeof }));
    LIST_PUSH(nr->extn, ((Foreign) { "string", &nero_stringfy }));
    LIST_PUSH(nr->extn, ((Foreign) { "system", &nero_system }));
    LIST_PUSH(nr->extn, ((Foreign) { "write_file", &nero_write_file }));
    LIST_PUSH(nr->extn, ((Foreign) { "read_file", &nero_read_file }));
    LIST_PUSH(nr->extn, ((Foreign) { "contains", &nero_contains }));
    LIST_PUSH(nr->extn, ((Foreign) { "split", &nero_split }));
    LIST_PUSH(nr->extn, ((Foreign) { "trim", &nero_trim }));
    LIST_PUSH(nr->extn, ((Foreign) { "arguments", &nero_arguments }));
}

static inline void free_vars(VariableList *vars) {
    for (int i = 0; i < vars->alloc; ++i) {
        vars->ptr[i].value.free = V_FREE;
        nero_free(vars->ptr[i].value);
        vars->ptr[i].value = (Value) {T_NIL};
    }
}

static inline VariableList copy_vars(VariableList *vars) {
    VariableList copy = LIST_ALLOCN(Variable, lexer_words.sz);
    for (int i = 0; i < vars->alloc; ++i)
        copy.ptr[i] = vars->ptr[i];
    return copy;
}

Value nero_call(Nero *nr, Function fn) {
    Function *fp = nr->fn;
    nr->fn = &fn;
    Value res = exec_block(nr, fn.code);
    free_vars(&fn.vars);
    nr->ret = RET_NO;
    nr->fn = fp;
    return res;
}

void set_var(VariableList *vars, int name, Value val) {
    if (!vars->alloc) {
        *vars = (VariableList) LIST_ALLOCN(Variable, lexer_words.sz);
        memset(vars->ptr, 0, vars->alloc*sizeof(Variable));
    }
    Value cpy = nero_copy(val);
    cpy.free = V_NOFREE;
    vars->ptr[name] = (Variable) {.exists = 1, .value = cpy};
}

Value get_var(VariableList *vars, int name) {
    if (!vars->alloc || !vars->ptr[name].exists) return (Value){T_BOOL, .as_num = -1};
    return vars->ptr[name].value;
}

Function get_fun(FunctionList *funs, int name) {
    if (!funs->alloc || !funs->ptr[name].exists) return (Function) {.exists = 0};
    return funs->ptr[name];
}

Value exec_number(Nero *nr) {
    Value val = {T_NUMBER, .as_num = strtod(PEEK(0).value.ptr, NULL)};
    ADVANCE(1);
    return val;
}

Value exec_string(Nero *nr) {
    Value val = nero_copy((Value){T_STRING, .free = V_FREE, .as_str = PEEK(0).value});
    ADVANCE(1);
    return val;
}

Value exec_list(Nero *nr) {
    Value val = {T_LIST, .free = V_FREE, .as_list = (ValueList) LIST_ALLOC(Value)};
    do {
        ADVANCE(1); // '[' | ','
        if (PEEK(0).kind == TK_RSQUARE) break;
        Value v = exec_expr(nr);
        LIST_PUSH(val.as_list, nero_copy(v));
        nero_free(v);
    } while (PEEK(0).kind == TK_COMMA);
    if (PEEK(0).kind != TK_RSQUARE) {
        fprintf(stderr, "Error: %s\nMissing ']'\n", errpos(nr, PEEK(0).line));
        exit(1);
    }
    ADVANCE(1);
    return val;
}

Value exec_assign(Nero *nr) {
    int var = PEEK(0).word;
    ADVANCE(2);
    Value res = exec_expr(nr);
    if (!nr->fn->exists)
        set_var(&nr->vars, var, res);
    else set_var(&nr->fn->vars, var, res);
    return res;
}

static Value call_foreign(Nero *nr, Token tok, Value args) {
    Value res = {T_NIL};
    for (int i = 0; i < nr->extn.sz; ++i) {
        if (STRCMPP(tok.value, nr->extn.ptr[i].name)) {
            res = nr->extn.ptr[i].func(args.as_list.sz, args.as_list.ptr);
            nero_free(args);
            return res;
        }
    }
    fprintf(stderr, "Error: %s\nUndefined function '%.*s'\n",
        errpos(nr, tok.line), tok.value.sz, tok.value.ptr);
    exit(1);
}

Value exec_call(Nero *nr) {
    Token tok = PEEK(0);
    ADVANCE(1);
    Value args = {T_LIST, .free = V_FREE, .as_list = LIST_ALLOC(Value)};
    do {
        ADVANCE(1); // '(' | ','
        if (PEEK(0).kind == TK_RPAREN) break;
        Value val = exec_expr(nr);
        LIST_PUSH(args.as_list, nero_copy(val));
        nero_free(val);
    } while (PEEK(0).kind == TK_COMMA);

    if (PEEK(0).kind != TK_RPAREN) {
        fprintf(stderr, "Error: %s\nMissing ')'\n", errpos(nr, PEEK(0).line));
        exit(1);
    }
    ADVANCE(1);
    Function fn;
    if (!(fn = get_fun(&nr->funs, tok.word)).exists)
        return call_foreign(nr, tok, args);
    if (fn.num_args != args.as_list.sz) {
        fprintf(stderr, "Error: %s\nFunction '%.*s' expects %d argument(s), got %d\n",
            errpos(nr, tok.line), tok.value.sz, tok.value.ptr, fn.num_args, args.as_list.sz);
        exit(1);
    }

    VariableList vars = fn.vars;
    fn.vars = copy_vars(&fn.vars);
    for (int i = 0; i < fn.num_args; ++i)
        set_var(&fn.vars, fn.args[i], args.as_list.ptr[i]);
    nero_free(args);
    Value res = nero_call(nr, fn);
    LIST_FREE(fn.vars);
    fn.vars = vars;
    return res;
}

Block parse_block(Nero *nr) {
    Block blk;
    int id = 0;
    if (PEEK(0).kind != TK_LBRACK) {
        fprintf(stderr, "Error: %s\nExpected '{'\n", errpos(nr, PEEK(0).line));
        exit(1);
    }
    ADVANCE(1);
    blk.start = nr->ip;
    while (PEEK(0).kind != TK_EOF) {
        if (PEEK(0).kind == TK_LBRACK) ++id;
        if (PEEK(0).kind == TK_RBRACK) {
            if (--id < 0) break;
        }
        ADVANCE(1);
    }

    if (PEEK(0).kind != TK_RBRACK) {
        fprintf(stderr, "Error: %s\nMissing '}'\n", errpos(nr, PEEK(0).line));
        exit(1);
    }
    blk.end = nr->ip;
    ADVANCE(1);
    return blk;
}

Value exec_variable(Nero *nr) {
    Token var = PEEK(0);
    if (PEEK(1).kind == TK_EQ)
        return exec_assign(nr);
    if (PEEK(1).kind == TK_LPAREN)
        return exec_call(nr);

    ADVANCE(1);
    Value res = get_var(&nr->fn->vars, var.word);
    if (res.type == T_BOOL && res.as_num == -1)
        res = get_var(&nr->vars, var.word);
    if (res.type == T_BOOL && res.as_num == -1) {
        fprintf(stderr, "Error: %s\nUndefined variable '%.*s'\n",
            errpos(nr, var.line), var.value.sz, var.value.ptr);
        exit(1);
    }
    return res;
}

Value exec_term(Nero *nr) {
    switch (PEEK(0).kind) {
    case TK_WORD:
        return exec_variable(nr);
    case TK_NUMBER:
        return exec_number(nr);
    case TK_STRING:
        return exec_string(nr);
    case TK_NIL:
        ADVANCE(1);
        return (Value) {T_NIL};
    case TK_LSQUARE:
        return exec_list(nr);
    case TK_FALSE:
        ADVANCE(1);
        return (Value) {T_BOOL, .as_num = 0};
    case TK_TRUE:
        ADVANCE(1);
        return (Value) {T_BOOL, .as_num = 1};
    case TK_MINUS: {
        int line = PEEK(0).line;
        ADVANCE(1);
        Value ret = exec_expr(nr);
        if (ret.type != T_NUMBER) {
            fprintf(stderr, "Error: %s\nExpected number\n", errpos(nr, line));
            exit(1);
        }
        return (Value) {T_NUMBER, .as_num = -ret.as_num};
    }
    case TK_NOT: {
        ADVANCE(1);
        return (Value) {T_BOOL, .as_num = !nero_true(exec_expr(nr))};
    }
    case TK_LPAREN: {
        int line = PEEK(0).line;
        ADVANCE(1);
        Value ret = exec_expr(nr);
        if (PEEK(0).kind != TK_RPAREN) {
            fprintf(stderr, "Error: %s\nMissing ')'\n", errpos(nr, line));
            exit(1);
        }
        ADVANCE(1);
        return ret;
    }
    default:
        fprintf(stderr, "Error: %s\nUnexpected token\n", errpos(nr, PEEK(0).line));
        exit(1);
    }
}

Value exec_list_index_assign(Nero *nr, Value list, int64_t idx) {
    ADVANCE(1);
    Value val = exec_expr(nr);
    nero_free(list.as_list.ptr[idx]);
    list.as_list.ptr[idx] = nero_copy(val);
    return val;
}

Value exec_list_index(Nero *nr, Value list) {
    if (list.type != T_LIST && list.type != T_STRING) {
        fprintf(stderr, "Error: %s\nExpected list\n", errpos(nr, PEEK(0).line));
        exit(1);
    }
    if (PEEK(0).kind != TK_LSQUARE) {
        fprintf(stderr, "Error: %s\nMissing '['\n", errpos(nr, PEEK(0).line));
        exit(1);
    }
    Token tk = PEEK(0);
    ADVANCE(1);
    Value idx = exec_expr(nr);
    if (PEEK(0).kind != TK_RSQUARE) {
        fprintf(stderr, "Error: %s\nMissing ']'\n", errpos(nr, PEEK(0).line));
        exit(1);
    }
    ADVANCE(1);
    if (idx.type != T_NUMBER) {
        fprintf(stderr, "Error: %s\nExpected number\n", errpos(nr, tk.line));
        exit(1);
    }
    int64_t i = (int64_t)idx.as_num;
    if (list.type == T_STRING) {
        if (i < 0 || i > list.as_str.sz) {
            fprintf(stderr, "Error: %s\nList index out of range\n", errpos(nr, tk.line));
            exit(1);
        }
        Value val = nero_copy((Value){T_STRING, .as_str = {.sz = 1, .ptr = &list.as_str.ptr[i]}});
        nero_free(list);
        return val;
    }
    if (i < 0 || i > list.as_list.sz) {
        fprintf(stderr, "Error: %s\nList index out of range\n", errpos(nr, tk.line));
        exit(1);
    }
    if (PEEK(0).kind == TK_EQ)
        return exec_list_index_assign(nr, list, i);

    if (list.free == V_NOFREE) {
        Value val = list.as_list.ptr[i];
        val.free = V_NOFREE;
        return val;
    }
    Value val = nero_copy(list.as_list.ptr[i]);
    nero_free(list);
    return val;
}

Value exec_factor(Nero *nr) {
    Value val = exec_term(nr);
    while (PEEK(0).kind == TK_LSQUARE)
        val = exec_list_index(nr, val);
    return val;
}

Value exec_muldiv(Nero *nr) {
    Value val = exec_factor(nr);
    Token tk = PEEK(0);
    while (tk.kind == TK_MUL || tk.kind == TK_DIV || tk.kind == TK_MOD) {
        ADVANCE(1);
        Value other = exec_factor(nr);
        if (other.type != T_NUMBER || val.type != T_NUMBER) {
            fprintf(stderr, "Error: %s\nExpected number\n", errpos(nr, tk.line));
            exit(1);
        }
        switch (tk.kind) {
        case TK_MUL:
            val.as_num *= other.as_num;
            break;
        case TK_DIV:
            if ((int)other.as_num == 0) goto divbyzero;
            val.as_num /= other.as_num;
            break;
        case TK_MOD:
            if ((int)other.as_num == 0) goto divbyzero;
            val.as_num = ((int)val.as_num) % ((int)other.as_num);
            break;
        default:
            fprintf(stderr, "Error: %s\nUnexpected token\n", errpos(nr, tk.line));
            exit(1);
        }
        tk = PEEK(0);
    }
    return val;
divbyzero:
    fprintf(stderr, "Error: %s\nDivision by zero\n", errpos(nr, tk.line));
    exit(1);
}

Value exec_addsub(Nero *nr) {
    Value val = exec_muldiv(nr);
    Token tk = PEEK(0);
    while (tk.kind == TK_PLUS || tk.kind == TK_MINUS) {
        ADVANCE(1);
        Value other = exec_muldiv(nr);
        if (other.type != T_NUMBER || val.type != T_NUMBER) {
            fprintf(stderr, "Error: %s\nExpected number\n", errpos(nr, tk.line));
            exit(1);
        }
        switch (tk.kind) {
        case TK_PLUS:
            val.as_num += other.as_num;
            break;
        case TK_MINUS:
            val.as_num -= other.as_num;
            break;
        default:
            fprintf(stderr, "Error: %s\nUnexpected token\n", errpos(nr, tk.line));
            exit(1);
        }
        tk = PEEK(0);
    }
    return val;
}

Value exec_compare(Nero *nr) {
    Value val = exec_addsub(nr);
    Token tk = PEEK(0);
    if (tk.kind >= TK_EQEQ && tk.kind <= TK_GEQ) {
        ADVANCE(1);
        Value other = exec_addsub(nr);
        switch (tk.kind) {
        case TK_EQEQ:
            val = nero_equals(val, other);
            break;
        case TK_NEQ:
            val = (Value) {T_BOOL, .as_num = !nero_equals(val, other).as_num};
            break;
        case TK_LT:
        case TK_GT:
        case TK_LEQ:
        case TK_GEQ:
            if (val.type != T_NUMBER || other.type != T_NUMBER) {
                fprintf(stderr, "Error: %s\nExpected number\n", errpos(nr, tk.line));
                exit(1);
            }
            return nero_compare(val, other, tk.kind);
        default:
            fprintf(stderr, "Error: %s\nUnexpected token\n", errpos(nr, tk.line));
            exit(1);
        }
    }
    return val;
}

Value exec_andor(Nero *nr) {
    Value val = exec_compare(nr);
    Token tk = PEEK(0);
    while (tk.kind == TK_AND || tk.kind == TK_OR) {
        ADVANCE(1);
        Value other = exec_compare(nr);
        switch (tk.kind) {
        case TK_AND:
            val = (Value) {T_BOOL, .as_num = nero_true(val) && nero_true(other)};
            break;
        case TK_OR:
            val = (Value) {T_BOOL, .as_num = nero_true(val) || nero_true(other)};
            break;
        default:
            fprintf(stderr, "Error: %s\nUnexpected token\n", errpos(nr, tk.line));
            exit(1);
        }
        tk = PEEK(0);
    }
    return val;
}

Value exec_def(Nero *nr) {
    ADVANCE(1);
    Token name = PEEK(0);
    if (name.kind != TK_WORD) {
        fprintf(stderr, "Error: %s\nExpected function name\n", errpos(nr, name.line));
        exit(1);
    }
    ADVANCE(1);
    if (PEEK(0).kind != TK_LPAREN) {
        fprintf(stderr, "Error: %s\nMissing '('\n", errpos(nr, PEEK(0).line));
        exit(1);
    }

    Function fn = {.exists = 1, .num_args = 0, .vars = LIST_EMPTY()};
    do {
        ADVANCE(1); // '(' | ','
        if (PEEK(0).kind == TK_RPAREN) break;
        if (PEEK(0).kind != TK_WORD) {
            fprintf(stderr, "Error: %s\nExpected word\n", errpos(nr, PEEK(0).line));
            exit(1);
        }
        int arg = PEEK(0).word;
        ADVANCE(1); // arg
        set_var(&fn.vars, arg, (Value){T_NIL});
        fn.args[fn.num_args++] = arg;
    } while (PEEK(0).kind == TK_COMMA);

    if (PEEK(0).kind != TK_RPAREN) {
        fprintf(stderr, "Error: %s\nMissing ')'\n", errpos(nr, PEEK(0).line));
        exit(1);
    }
    ADVANCE(1);

    fn.code = parse_block(nr);
    if (!nr->funs.alloc) {
        nr->funs = (FunctionList) LIST_ALLOCN(Function, lexer_words.sz);
        memset(nr->funs.ptr, 0, nr->funs.alloc*sizeof(Function));
    }
    nr->funs.ptr[name.word] = fn;
    return (Value) {T_NIL};
}

Value exec_return(Nero *nr) {
    ADVANCE(1);
    Value res = exec_expr(nr);
    Value cpy = nero_copy(res);
    nero_free(res);
    nr->ret = RET_RETURN;
    return cpy;
}

Value exec_break(Nero *nr, uint8_t brk) {
    ADVANCE(1);
    nr->ret = brk;
    return (Value) {T_NIL};
}

Value exec_if(Nero *nr) {
    struct { int cond; Block body; } conditions[1024];
    int sz = 0;
    do {
        ADVANCE(1);
        conditions[sz].cond = nero_true(exec_expr(nr));
        conditions[sz].body = parse_block(nr);
        ++sz;
    } while (PEEK(0).kind == TK_ELIF);

    Block else_body = {-1};
    if (PEEK(0).kind == TK_ELSE) {
        ADVANCE(1);
        else_body = parse_block(nr);
    }

    for (int i = 0; i < sz; ++i) {
        if (conditions[i].cond)
            return exec_block(nr, conditions[i].body);
    }
    if (else_body.start != -1)
        return exec_block(nr, else_body);
    return (Value) {T_NIL};
}

Value exec_while(Nero *nr) {
    Value res = {T_NIL};
    ADVANCE(1);
    int start = nr->ip;
    Value cond = exec_expr(nr);
    Block body = parse_block(nr);

    while (nero_true(cond)) {
        nero_free(res);
        res = exec_block(nr, body);
        if (nr->ret == RET_RETURN) return res;
        else if (nr->ret == RET_NEXT) nr->ret = RET_NO;
        else if (nr->ret == RET_BREAK) {
            nr->ret = RET_NO; break;
        }
        nr->ip = start;
        cond = exec_expr(nr);
    }
    nr->ip = body.end+1;
    return res;
}

Value exec_keyword(Nero *nr) {
    switch (PEEK(0).kind) {
    case TK_DEF:
        return exec_def(nr);
    case TK_RETURN:
        return exec_return(nr);
    case TK_BREAK:
        return exec_break(nr, RET_BREAK);
    case TK_NEXT:
        return exec_break(nr, RET_NEXT);
    case TK_IF:
        return exec_if(nr);
    case TK_WHILE:
        return exec_while(nr);
    case TK_ECHO: {
        do {
            ADVANCE(1);
            nero_echo(exec_expr(nr));
        } while (PEEK(0).kind == TK_COMMA);
        nero_echo((Value) {T_STRING, .as_str = {.sz = 1, .ptr = "\n"}});
        return (Value) {T_NIL};
    }
    case TK_READ: {
        do {
            ADVANCE(1);
            nero_echo(exec_expr(nr));
        } while (PEEK(0).kind == TK_COMMA);
        return nero_read();
    }
    case TK_QUIT:
        exit(0);
    default:
        fprintf(stderr, "Error: %s\nUnexpected token\n", errpos(nr, PEEK(0).line));
        exit(1);
    }
}

Value exec_block(Nero *nr, Block blk) {
    int ip = nr->ip;
    nr->ret = RET_NO;
    nr->ip = blk.start;
    Value res = {T_NIL};
    while (nr->ip < blk.end && PEEK(0).kind != TK_EOF) {
        nero_free(res);
        res = exec_expr(nr);
        if (nr->ret != RET_NO)
            break;
    }
    nr->ip = ip;
    return res;
}

Value exec_expr(Nero *nr) {
    if (PEEK(0).kind >= TK_DEF && PEEK(0).kind <= TK_QUIT)
        return exec_keyword(nr);
    return exec_andor(nr);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input>\n", argv[0]);
        return 1;
    }

    args_list = (Value) {T_LIST, .free = V_FREE, .as_list = (ValueList) LIST_ALLOC(Value)};
    for (int i = 1; i < argc; ++i) {
        Value arg = {T_STRING, .as_str = {.sz = strlen(argv[i]), .ptr = argv[i]}};
        LIST_PUSH(args_list.as_list, nero_copy(arg));
    }

    Nero nero = {0};
    tokenize(&nero, argv[1]);
    nero_init_foreign(&nero);
    Function global = {
        .exists = 0,
        .code = {0, nero.code.sz},
    };
    nero_call(&nero, global);
    nero_free(args_list);
    return 0;
}
