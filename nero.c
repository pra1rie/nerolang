#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <gc.h>

#include "nlist.h"
#include "nstring.h"

#define malloc(X) GC_MALLOC(X)
#define calloc(X, Y) GC_MALLOC(X * Y)
#define realloc(X, Y) GC_REALLOC(X, Y)
#define free(X) GC_FREE(X)

#define SIMPLE_ERROR(...) do { fprintf(stderr, "Error: "__VA_ARGS__); exit(1); } while(0)
#define ERROR(...) SIMPLE_ERROR("%s\n"__VA_ARGS__)

enum {
    // constants
    TK_EOF, TK_NIL, TK_FALSE, TK_TRUE, TK_NUMBER, TK_STRING, TK_WORD,
    // operators
    TK_LPAREN, TK_RPAREN, TK_LBRACK, TK_RBRACK, TK_LSQUARE, TK_RSQUARE, TK_COMMA, TK_EQ,
    TK_NOT, TK_DOT, TK_PLUS, TK_MINUS, TK_MUL, TK_DIV, TK_MOD, TK_BOR, TK_BAND, TK_XOR,
    TK_SHL, TK_SHR, TK_BNOT, TK_EQEQ, TK_NEQ, TK_LT, TK_GT, TK_LEQ, TK_GEQ, TK_AND, TK_OR,
    // keywords
    TK_DEF, TK_RETURN, TK_IF, TK_ELIF, TK_ELSE,
    TK_WHILE, TK_FOR, TK_BREAK, TK_NEXT, TK_IMPORT,
};

// XXX: implement pow '**', first-class functions, proper garbage collector, error type
// XXX: some way to call more foreign functions at runtime
// XXX: basic ranges (eg. str[1, 5] (str from 1..5); str[3, -1] (str from 3..length-1))

#define GLOBAL_SCOPE "<global>"
#define MAX_CONDITIONS 1024

const char *ops[] = {
    "(", ")", "{", "}", "[", "]", ",", "=", "!", ".",
    "+", "-", "*", "/", "%", "|", "&", "^", "<<", ">>",
    "~", "==", "!=", "<", ">", "<=", ">=", "&&", "||",
};

const char *keywords[] = {
    "def", "return", "if", "elif", "else",
    "while", "for", "break", "next", "import",
};

typedef struct {
    int kind, line, file;
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

enum { T_NIL, T_BOOL, T_INT, T_REAL, T_STR, T_LIST, T_DICT };
typedef struct Value {
    uint8_t type;
    union {
        int64_t as_int;
        double as_real;
        String *as_str;
        ValueList *as_list;
        VariableList *as_dict;
    };
} Value;

typedef struct Variable {
    String name;
    Value value;
} Variable;

typedef struct {
    int start, end;
} Block;

typedef struct Function {
    String name;
    VariableList vars;
    Block code;
} Function;

typedef struct {
    int ip, ret;
    TokenList code;
    VariableList vars;
    FunctionList funs;
    ForeignList extn;
    Function *fn;
} Nero;

enum { RET_NO, RET_RETURN, RET_BREAK, RET_NEXT };

static ValueList args_list = {.alloc = 0};
static StringList files = {.alloc = 0};
static int line = 1;

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

static inline int is_operator(char *op, int sz) {
    for (int i = 0; i < LENGTH(ops); ++i) {
        if (sz == strlen(ops[i]) && !strncmp(ops[i], op, sz))
            return 1;
    }
    return 0;
}

static inline int is_word(char c) {
    return !(is_operator(&c, 1) || isspace(c) || c == '#' || c == '\"' || c == '\'');
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

static inline Token next_token(FILE *fp, int file) {
    char c = skip_space(fp);
    Token tok = {.kind = TK_EOF, .line = line, .file = file};
    tok.value.alloc = 0;
    if (c == EOF) return tok;
    if (isdigit(c)) {
        tok.value = STRALLOC();
        tok.kind = TK_NUMBER;
        while (c != EOF && (isdigit(c) || c == '.')) {
            LIST_PUSH(tok.value, c); c = fgetc(fp);
            if (tok.value.ptr[0] == '0' && (c == 'x' || c == 'X')) {
                do {
                    LIST_PUSH(tok.value, c); c = fgetc(fp);
                } while (c != EOF && isxdigit(c));
                break;
            }
        }
        goto end;
    }
    if (c == '\"' || c == '\'') {
        char match = c;
        tok.kind = TK_STRING;
        tok.value = STRALLOC();
        do {
            c = fgetc(fp);
            if (c == '\\') {
                LIST_PUSH(tok.value, escape(fgetc(fp)));
            } else {
                LIST_PUSH(tok.value, c);
            }
        } while (c != EOF && c != match);
        LIST_POP(tok.value); // remove last quote
        return tok; // don't unget last quote
    }
    if (is_operator(&c, 1)) {
        char op[2] = {c, fgetc(fp)};
        int sz = 2;
        if (!is_operator(op, 2)) {
            sz = 1; ungetc(op[1], fp);
        }
        for (int i = 0; i < LENGTH(ops); ++i) {
            if (strlen(ops[i]) == sz && !strncmp(ops[i], op, sz)) {
                tok.kind = i+TK_LPAREN;
                break;
            }
        }
        return tok;
    }

    tok.value = STRALLOC();
    while (c != EOF && is_word(c)) {
        LIST_PUSH(tok.value, c);
        c = fgetc(fp);
    }
    const char *booleans[] = {"nil", "false", "true"};
    for (int i = 0; i < LENGTH(booleans); ++i) {
        if (STRCMPP(tok.value, booleans[i])) {
            tok.kind = i+TK_NIL;
            goto end;
        }
    }
    for (int i = 0; i < LENGTH(keywords); ++i) {
        if (STRCMPP(tok.value, keywords[i])) {
            tok.kind = i+TK_DEF;
            goto end;
        }
    }
    tok.kind = TK_WORD;
end:
    ungetc(c, fp);
    return tok;
}

static inline char *errpos(Nero *nr, Token tk);

void tokenize(Nero *nr, String file) {
    if (!nr->code.alloc) nr->code = (TokenList) LIST_ALLOC(Token);
    if (!files.alloc) files = (StringList) LIST_ALLOC(String);
    int cur_line = line, cur_file = files.sz;
    char str[1024] = {0};

    for (int f = 0; f < files.sz; ++f) {
        if (STRCMPS(files.ptr[f], file))
            return;
    }

    LIST_PUSH(files, file);
    memcpy(str, file.ptr, file.sz);
    FILE *fp = fopen(str, "r");
    if (!fp) goto fail;

    line = 1;
    Token tok;
    do {
        tok = next_token(fp, cur_file);
        if (tok.kind == TK_IMPORT) {
            tok = next_token(fp, cur_file);
            if (tok.kind != TK_STRING) {
                fclose(fp);
                ERROR("Expected str\n", errpos(nr, tok));
            }
            tokenize(nr, tok.value);
            if (nr->code.ptr[nr->code.sz-1].kind == TK_EOF)
                LIST_POP(nr->code);
            continue;
        }
        LIST_PUSH(nr->code, tok);
    } while (tok.kind != TK_EOF);

    nr->ip = 0;
    line = cur_line;
    fclose(fp);
    return;
fail:
    if (fp) fclose(fp);
    SIMPLE_ERROR("Could not open file '%.*s'\n", file.sz, file.ptr);
}

#define PEEK(n) nr->code.ptr[nr->ip+n]
#define ADVANCE(n) nr->ip += n
#define INBOUND() nr->ip < nr->code.sz

Value exec_expr(Nero *nr);
Value exec_block(Nero *nr, Block blk);
static inline void free_vars(VariableList *vars);
void set_var(VariableList *vars, String name, Value val);
Value get_var(VariableList *vars, String name);
Value nero_keys(int argc, Value *argv);

static inline char *errpos(Nero *nr, Token tk) {
    char err[1024];
    String fn = nr->fn->name, fl = files.ptr[tk.file];
    sprintf(err, "(file \"%.*s\", line %d, in \"%.*s\")",
        fl.sz, fl.ptr, tk.line, fn.sz, fn.ptr);
    return strdup(err);
}

#define nero_list_alloc() (nero_list_allocn(_NERO_LIST_ALLOC_SIZE))
static inline ValueList *nero_list_allocn(int sz) {
    ValueList *list = malloc(sizeof(ValueList));
    *list = (ValueList) LIST_ALLOCN(Value, sz);
    return list;
}

#define nero_string_alloc() (nero_string_allocn(_NERO_STRING_ALLOC_SIZE))
static inline String *nero_string_allocn(int sz) {
    String *str = malloc(sizeof(String));
    str->ptr = malloc((str->alloc = sz) * sizeof(char));
    return str;
}

static inline String *nero_string_copy(String s) {
    String *str = nero_string_allocn(s.sz);
    memcpy(str->ptr, s.ptr, (str->sz = s.sz));
    return str;
}

static inline void nero_string_concats(String *str, String *s) {
    if (str->sz + s->sz >= str->alloc)
        str->ptr = realloc(str->ptr, str->alloc = (str->sz+s->sz+_NERO_STRING_ALLOC_SIZE));
    memcpy(str->ptr+str->sz, s->ptr, s->sz);
    str->sz += s->sz;
}

static inline void nero_string_concatp(String *str, char *p) {
    nero_string_concats(str, &(String){.sz = strlen(p), .ptr = p});
}

Value nero_string(Value val, int escape) {
    String *str = nero_string_alloc();
    char num[100];
    switch (val.type) {
    case T_BOOL:
        nero_string_concatp(str, val.as_int? "true" : "false");
        break;
    case T_INT:
        sprintf(num, "%ld", val.as_int);
        nero_string_concatp(str, num);
        break;
    case T_REAL:
        sprintf(num, "%.2lf", val.as_real);
        nero_string_concatp(str, num);
        break;
    case T_STR:
        if (escape) nero_string_concatp(str, "\"");
        nero_string_concats(str, val.as_str);
        if (escape) nero_string_concatp(str, "\"");
        break;
    case T_LIST:
        nero_string_concatp(str, "[");
        for (int i = 0; i < val.as_list->sz; ++i) {
            if (i > 0) nero_string_concatp(str, ", ");
            Value v = nero_string(val.as_list->ptr[i], 1);
            nero_string_concats(str, v.as_str);
        }
        nero_string_concatp(str, "]");
        break;
    case T_DICT:
        nero_string_concatp(str, "{");
        for (int i = 0; i < val.as_dict->sz; ++i) {
            if (i > 0) nero_string_concatp(str, ", ");
            nero_string_concatp(str, "\"");
            nero_string_concats(str, &val.as_dict->ptr[i].name);
            nero_string_concatp(str, "\" = ");
            Value v = nero_string(val.as_dict->ptr[i].value, 1);
            nero_string_concats(str, v.as_str);
        }
        nero_string_concatp(str, "}");
        break;
    default:
        nero_string_concatp(str, "nil");
        break;
    }
    return (Value) {T_STR, .as_str = str};
}

void nero_print(Value val, int escape) {
    Value str = nero_string(val, escape);
    fprintf(stdout, "%.*s", str.as_str->sz, str.as_str->ptr);
}

int nero_true(Value v) {
    switch (v.type) {
    case T_BOOL:
    case T_INT:  return v.as_int;
    case T_REAL: return v.as_real;
    case T_STR:  return v.as_str->sz;
    case T_LIST: return v.as_list->sz;
    case T_DICT: return v.as_dict->sz;
    default: return 0;
    }
}

Value nero_equals(Value a, Value b) {
    Value res = {T_BOOL, .as_int = 0};
    if (a.type != b.type) return res;
    switch (a.type) {
    case T_NIL:  res.as_int = 1; break;
    case T_BOOL:
    case T_INT:  res.as_int = a.as_int == b.as_int; break;
    case T_REAL: res.as_int = a.as_real == b.as_real; break;
    case T_STR:  res.as_int = STRCMPSP(a.as_str, b.as_str); break;
    case T_LIST:
        if (a.as_list->sz != b.as_list->sz) break;
        res.as_int = 1;
        for (int i = 0; i < a.as_list->sz; ++i) {
            if (!nero_equals(a.as_list->ptr[i], b.as_list->ptr[i]).as_int) {
                res.as_int = 0;
                break;
            }
        }
        break;
    case T_DICT:
        if (a.as_dict->sz != b.as_dict->sz) break;
        res.as_int = 1;
        Value keys = nero_keys(1, &a);
        for (int i = 0; i < keys.as_list->sz; ++i) {
            String *key = keys.as_list->ptr[i].as_str;
            if (!nero_equals(get_var(a.as_dict, *key), get_var(b.as_dict, *key)).as_int) {
                res.as_int = 0;
                break;
            }
        }
        break;
    }
    return res;
}

Value nero_compare(Value a, Value b, uint8_t k) {
    Value res = {T_BOOL, .as_int = 0};
    switch (k) {
    case TK_LT:  res.as_int = a.as_int <  b.as_int; break;
    case TK_LEQ: res.as_int = a.as_int <= b.as_int; break;
    case TK_GT:  res.as_int = a.as_int >  b.as_int; break;
    case TK_GEQ: res.as_int = a.as_int >= b.as_int; break;
    default: break;
    }
    return res;
}

Value nero_copy(Value val) {
    Value res = {val.type};
    switch (val.type) {
    case T_BOOL:
    case T_INT:
    case T_REAL: res.as_int = val.as_int; break;
    case T_STR:  res.as_str = nero_string_copy(*val.as_str); break;
    case T_LIST:
        res.as_list = nero_list_allocn(val.as_list->alloc);
        for (int i = 0; i < val.as_list->sz; ++i)
            LIST_PUSHP(res.as_list, nero_copy(val.as_list->ptr[i]));
        break;
    case T_DICT:
        res.as_dict = malloc(sizeof(VariableList));
        *res.as_dict = (VariableList) LIST_ALLOCN(Variable, val.as_dict->alloc);
        for (int i = 0; i < val.as_dict->sz; ++i) {
            String *s = nero_string_copy(val.as_dict->ptr[i].name);
            set_var(res.as_dict, *s, val.as_dict->ptr[i].value);
        }
        break;
    }
    return res;
}

#define EXPECT(N) if (argc != N) { SIMPLE_ERROR("Expected %d argument(s), got %d\n", N, argc); }
#define EXPECT_TYPE(A, T) if (A.type != T) {\
    String _exp = type_to_string(T), _got = type_to_string(A.type); \
    SIMPLE_ERROR("Expected %.*s, got %.*s\n", _exp.sz, _exp.ptr, _got.sz, _got.ptr); }

static inline String type_to_string(uint8_t type) {
    String str = STRALLOC();
    switch (type) {
    case T_BOOL: nero_string_concatp(&str, "bool"); break;
    case T_INT:  nero_string_concatp(&str, "int"); break;
    case T_REAL: nero_string_concatp(&str, "real"); break;
    case T_STR:  nero_string_concatp(&str, "str"); break;
    case T_LIST: nero_string_concatp(&str, "list"); break;
    case T_DICT: nero_string_concatp(&str, "dict"); break;
    default:     nero_string_concatp(&str, "nil"); break;
    }
    return str;
}

Value nero_typeof(int argc, Value *argv) {
    EXPECT(1);
    return (Value) {T_STR, .as_str = nero_string_copy(type_to_string(argv[0].type))};
}

Value nero_dup(int argc, Value *argv) {
    EXPECT(1);
    return nero_copy(argv[0]);
}

Value nero_stringfy(int argc, Value *argv) {
    Value str = {T_STR, .as_str = nero_string_alloc()};
    for (int i = 0; i < argc; ++i) {
        Value s = nero_string(argv[i], 0);
        nero_string_concats(str.as_str, s.as_str);
    }
    return str;
}

Value nero_number(int argc, Value *argv) {
    EXPECT(1);
    EXPECT_TYPE(argv[0], T_STR);
    int found_dot = 0;
    char *str = strndup(argv[0].as_str->ptr, argv[0].as_str->sz);
    for (int i = 0; i < argv[0].as_str->sz; ++i) {
        if (!(isdigit(str[i]) || str[i] == '.') || (str[i] == '.' && found_dot))
            SIMPLE_ERROR("Invalid number '%s'\n", str);
        if (str[i] == '.') found_dot = 1;
    }
    if (found_dot) return (Value) {T_REAL, .as_real = strtod(str, NULL)};
    return (Value) {T_INT, .as_int = strtol(str, NULL, 10)};
}

Value nero_echo(int argc, Value *argv) {
    for (int i = 0; i < argc; ++i) nero_print(argv[i], 0);
    fprintf(stdout, "\n");
    return (Value) {T_NIL};
}

Value nero_read(int argc, Value *argv) {
    for (int i = 0; i < argc; ++i) nero_print(argv[i], 0);
    char *l = NULL; size_t n = 0;
    getline(&l, &n, stdin);
    String *str = nero_string_alloc();
    nero_string_concatp(str, l);
    LIST_POPP(str); // remove last \n
    return (Value) {T_STR, .as_str = str};
}

Value nero_exit(int argc, Value *argv) {
    EXPECT(1);
    EXPECT_TYPE(argv[0], T_INT);
    exit((int)argv[0].as_int);
    return (Value){T_NIL}; // dum dum compiler
}

Value nero_keys(int argc, Value *argv) {
    EXPECT(1);
    EXPECT_TYPE(argv[0], T_DICT);
    Value keys = {T_LIST, .as_list = nero_list_alloc()};
    for (int i = 0; i < argv[0].as_dict->sz; ++i) {
        Value key = nero_copy((Value){T_STR, .as_str = nero_string_copy(argv[0].as_dict->ptr[i].name)});
        LIST_PUSHP(keys.as_list, key);
    }
    return keys;
}

Value nero_push(int argc, Value *argv) {
    EXPECT(2);
    if (argv[0].type == T_STR) {
        Value s = nero_string(argv[1], 0);
        nero_string_concats(argv[0].as_str, s.as_str);
        return argv[0];
    }
    EXPECT_TYPE(argv[0], T_LIST);
    Value val = nero_equals(argv[1], argv[0]).as_int? nero_copy(argv[1]) : argv[1];
    LIST_PUSHP(argv[0].as_list, val);
    return argv[0];
}

Value nero_pop(int argc, Value *argv) {
    EXPECT(1);
    if (argv[0].type == T_STR) {
        if (argv[0].as_str->sz == 0) goto fail;
        LIST_POPP(argv[0].as_str);
        return argv[0];
    }
    EXPECT_TYPE(argv[0], T_LIST);
    if (argv[0].as_list->sz == 0) goto fail;
    ValueList *list = argv[0].as_list;
    LIST_POPP(list);
    return argv[0];
fail:
    SIMPLE_ERROR("List index out of range\n");
    return (Value){T_NIL}; // dum dum compiler
}

Value nero_len(int argc, Value *argv) {
    EXPECT(1);
    if (argv[0].type == T_STR) return (Value) {T_INT, .as_int = argv[0].as_str->sz};
    EXPECT_TYPE(argv[0], T_LIST);
    return (Value) {T_INT, .as_int = argv[0].as_list->sz};
}

Value nero_chr(int argc, Value *argv) {
    EXPECT(1);
    EXPECT_TYPE(argv[0], T_INT);
    String *str = nero_string_alloc();
    LIST_PUSHP(str, argv[0].as_int);
    return (Value){T_STR, .as_str = str};
}

Value nero_ord(int argc, Value *argv) {
    EXPECT(1);
    EXPECT_TYPE(argv[0], T_STR);
    if (argv[0].as_str->sz != 1) SIMPLE_ERROR("Expected str of size 1\n");
    return (Value){T_INT, .as_int = argv[0].as_str->ptr[0]};
}

Value nero_system(int argc, Value *argv) {
    EXPECT(1);
    Value str = nero_string(argv[0], 0);
    char cmd[str.as_str->sz];
    sprintf(cmd, "%.*s", str.as_str->sz, str.as_str->ptr);
    return (Value) {T_INT, .as_int = system(cmd)};
}

Value nero_write_file(int argc, Value *argv) {
    EXPECT(2);
    EXPECT_TYPE(argv[0], T_STR);
    char file[argv[0].as_str->sz];
    sprintf(file, "%.*s", argv[0].as_str->sz, argv[0].as_str->ptr);
    FILE *fp = fopen(file, "w+");
    if (!fp) SIMPLE_ERROR("Could not write file '%s'\n", file);
    Value str = nero_string(argv[1], 0);
    fwrite(str.as_str->ptr, sizeof(char), str.as_str->sz, fp);
    fclose(fp);
    return (Value){T_NIL};
}

Value nero_read_file(int argc, Value *argv) {
    EXPECT(1);
    EXPECT_TYPE(argv[0], T_STR);
    char file[argv[0].as_str->sz];
    sprintf(file, "%.*s", argv[0].as_str->sz, argv[0].as_str->ptr);
    FILE *fp = fopen(file, "r");
    int sz = 0;
    String text;
    if (!fp) SIMPLE_ERROR("Could not read file '%s'\n", file);
    fseek(fp, 0, SEEK_END);
    if ((sz = ftell(fp))) text = STRALLOCN(sz);
    fseek(fp, 0, SEEK_SET);
    if (fread(text.ptr, sizeof(char), sz, fp) != sz) SIMPLE_ERROR("Failed to read file '%s'\n", file);
    text.alloc = text.sz = sz;
    fclose(fp);
    return (Value){T_STR, .as_str = nero_string_copy(text)};
}

Value nero_range(int argc, Value *argv) {
    EXPECT(3);
    EXPECT_TYPE(argv[1], T_INT);
    EXPECT_TYPE(argv[2], T_INT);
    if (argv[0].type != T_STR) EXPECT_TYPE(argv[0], T_LIST);
    int len = argv[0].type == T_STR? argv[0].as_str->sz : argv[0].as_list->sz;
    int64_t start = argv[1].as_int, end = argv[2].as_int;
    if (start < 0 || start >= len || end > len || len+end < 0) SIMPLE_ERROR("List index out of range\n");
    if (argv[0].type == T_STR) {
        String s = {.ptr = argv[0].as_str->ptr+start, .sz = (end <= 0? (len-start)+end : end-start+1)};
        Value res = {T_STR, .as_str = nero_string_copy(s)};
        return res;
    }
    Value res = {T_LIST, .as_list = nero_list_alloc()};
    for (int i = start; i < (end <= 0? len+end : end+1); ++i)
        LIST_PUSHP(res.as_list, argv[0].as_list->ptr[i]);
    return res;
}

Value nero_contains(int argc, Value *argv) {
    EXPECT(2);
    if (argv[0].type == T_STR) {
        EXPECT_TYPE(argv[1], T_STR);
        char *haystack = strndup(argv[0].as_str->ptr, argv[0].as_str->sz);
        char *needle = strndup(argv[1].as_str->ptr, argv[1].as_str->sz);
        int res = (strstr(haystack, needle) != NULL);
        return (Value) {T_BOOL, .as_int = res};
    }
    EXPECT_TYPE(argv[0], T_LIST);
    for (int i = 0; i < argv[0].as_list->sz; ++i) {
        Value v = argv[0].as_list->ptr[i];
        if (nero_equals(v, argv[1]).as_int) return (Value) {T_BOOL, .as_int = 1};
    }
    return (Value) {T_BOOL, .as_int = 0};
}

Value nero_split(int argc, Value *argv) {
    EXPECT(2);
    EXPECT_TYPE(argv[0], T_STR);
    const int is_list = (argv[1].type == T_LIST);
    if (!is_list) EXPECT_TYPE(argv[1], T_STR);
    // split("hello world!", " ") || split("hello world!", [" ", "!"])

    Value list = {T_LIST, .as_list = nero_list_alloc()};
    Value tok = {T_STR, .as_str = nero_string_alloc()};

    for (int i = 0; i < argv[0].as_str->sz; ++i) {
        char ch = argv[0].as_str->ptr[i];
        Value val_ch = {T_STR, .as_str = nero_string_copy((String){.sz = 1, .ptr = &ch})};
        const int list_condition = (is_list && nero_contains(2, (Value[]){argv[1], val_ch}).as_int);
        const int string_condition = (!is_list && nero_equals(argv[1], val_ch).as_int);
        if (list_condition || string_condition) {
            LIST_PUSHP(list.as_list, nero_copy(tok));
            tok.as_str->sz = 0;
        } else {
            LIST_PUSHP(tok.as_str, ch);
        }
    }
    LIST_PUSHP(list.as_list, tok);
    return list;
}

Value nero_arguments(int argc, Value *argv) {
    EXPECT(0);
    Value list = nero_copy((Value){T_LIST, .as_list = &args_list});
    return list;
}

void nero_init_foreign(Nero *nr) {
    nr->extn = (ForeignList) LIST_ALLOC(Foreign);
    LIST_PUSH(nr->extn, ((Foreign) { "echo",       &nero_echo }));
    LIST_PUSH(nr->extn, ((Foreign) { "read",       &nero_read }));
    LIST_PUSH(nr->extn, ((Foreign) { "exit",       &nero_exit }));
    LIST_PUSH(nr->extn, ((Foreign) { "keys",       &nero_keys }));
    LIST_PUSH(nr->extn, ((Foreign) { "push",       &nero_push }));
    LIST_PUSH(nr->extn, ((Foreign) { "pop",        &nero_pop }));
    LIST_PUSH(nr->extn, ((Foreign) { "len",        &nero_len }));
    LIST_PUSH(nr->extn, ((Foreign) { "chr",        &nero_chr }));
    LIST_PUSH(nr->extn, ((Foreign) { "ord",        &nero_ord }));
    LIST_PUSH(nr->extn, ((Foreign) { "dup",        &nero_dup }));
    LIST_PUSH(nr->extn, ((Foreign) { "typeof",     &nero_typeof }));
    LIST_PUSH(nr->extn, ((Foreign) { "string",     &nero_stringfy }));
    LIST_PUSH(nr->extn, ((Foreign) { "number",     &nero_number }));
    LIST_PUSH(nr->extn, ((Foreign) { "system",     &nero_system }));
    LIST_PUSH(nr->extn, ((Foreign) { "write_file", &nero_write_file }));
    LIST_PUSH(nr->extn, ((Foreign) { "read_file",  &nero_read_file }));
    LIST_PUSH(nr->extn, ((Foreign) { "range",      &nero_range }));
    LIST_PUSH(nr->extn, ((Foreign) { "contains",   &nero_contains }));
    LIST_PUSH(nr->extn, ((Foreign) { "split",      &nero_split }));
    LIST_PUSH(nr->extn, ((Foreign) { "arguments",  &nero_arguments }));
}

static inline void free_vars(VariableList *vars) {
    for (int i = 0; i < vars->sz; ++i) vars->ptr[i].value = (Value) {T_NIL};
}

static inline VariableList copy_vars(VariableList *vars) {
    VariableList copy = LIST_ALLOC(Variable);
    for (int i = 0; i < vars->sz; ++i) LIST_PUSH(copy, vars->ptr[i]);
    return copy;
}

void set_var(VariableList *vars, String name, Value val) {
    if (!vars->alloc) *vars = (VariableList) LIST_ALLOC(Variable);
    for (int i = 0; i < vars->sz; ++i) {
        if (STRCMPS(vars->ptr[i].name, name)) {
            vars->ptr[i].value = val;
            return;
        }
    }
    Variable var = {.name = name, .value = val};
    LIST_PUSHP(vars, var);
}

Value get_var(VariableList *vars, String name) {
    if (!vars->alloc) goto fail;
    for (int i = 0; i < vars->sz; ++i) {
        if (STRCMPS(vars->ptr[i].name, name))
            return vars->ptr[i].value;
    }
fail:
    return (Value){T_BOOL, .as_int = -1};
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

Function get_fun(FunctionList *funs, String name) {
    if (!funs->alloc) goto fail;
    for (int i = 0; i < funs->sz; ++i) {
        if (STRCMPS(funs->ptr[i].name, name))
            return funs->ptr[i];
    }
fail:
    return (Function) {.code = {-1}};
}

#define EXPECT_INT(T, A) if ((A).type != T_INT) ERROR("Expected int\n", errpos(nr, (T)));
#define EXPECT_NUMBER(T, A) if ((A).type != T_REAL) EXPECT_INT(T, A)

#define GET_NUMBER(N) (N).type == T_REAL? (N).as_real : (N).as_int
#define OPERATE_ON_NUMBER(OP, A, B) \
    if ((A).type == T_REAL) (A).as_real OP##= GET_NUMBER(B); else (A).as_int OP##= GET_NUMBER(B);

Value exec_number(Nero *nr) {
    Value val;
    if (strchr(PEEK(0).value.ptr, '.'))
        val = (Value) {T_REAL, .as_real = strtod(PEEK(0).value.ptr, NULL)};
    else val = (Value) {T_INT, .as_int = strtol(PEEK(0).value.ptr, NULL, 0)};
    ADVANCE(1);
    return val;
}

Value exec_string(Nero *nr) {
    Value val = nero_copy((Value){T_STR, .as_str = nero_string_copy(PEEK(0).value)});
    ADVANCE(1);
    return val;
}

Value exec_dict(Nero *nr) {
    VariableList *dict = malloc(sizeof(VariableList));
    *dict = (VariableList) LIST_ALLOC(Variable);
    Value val = {T_DICT, .as_dict = dict};
    do {
        ADVANCE(1); // '{' | ','
        if (PEEK(0).kind == TK_RBRACK) break;
        if (PEEK(0).kind != TK_WORD && PEEK(0).kind != TK_STRING)
            ERROR("Expected word\n", errpos(nr, PEEK(0)));
        String *key = nero_string_copy(PEEK(0).value);
        ADVANCE(1);
        if (PEEK(0).kind != TK_EQ) ERROR("Missing '='\n", errpos(nr, PEEK(0)));
        ADVANCE(1);
        Value v = exec_expr(nr);
        set_var(val.as_dict, *key, nero_copy(v));
    } while (PEEK(0).kind == TK_COMMA);
    if (PEEK(0).kind != TK_RBRACK) ERROR("Missing '}'\n", errpos(nr, PEEK(0)));
    ADVANCE(1);
    return val;
}

Value exec_list(Nero *nr) {
    Value val = {T_LIST, .as_list = nero_list_alloc()};
    do {
        ADVANCE(1); // '[' | ','
        if (PEEK(0).kind == TK_RSQUARE) break;
        Value v = exec_expr(nr);
        LIST_PUSHP(val.as_list, v);
    } while (PEEK(0).kind == TK_COMMA);
    if (PEEK(0).kind != TK_RSQUARE) ERROR("Missing ']'\n", errpos(nr, PEEK(0)));
    ADVANCE(1);
    return val;
}

Value exec_assign(Nero *nr) {
    String var = PEEK(0).value;
    ADVANCE(2);
    Value res = exec_expr(nr);
    if (STRCMPP(nr->fn->name, "<global>")) set_var(&nr->vars, var, res);
    else set_var(&nr->fn->vars, var, res);
    return res;
}

static Value call_foreign(Nero *nr, Token tok, Value args) {
    Value res = {T_NIL};
    for (int i = 0; i < nr->extn.sz; ++i) {
        if (STRCMPP(tok.value, nr->extn.ptr[i].name)) {
            res = nr->extn.ptr[i].func(args.as_list->sz, args.as_list->ptr);
            return res;
        }
    }
    ERROR("Undefined function '%.*s'\n", errpos(nr, tok), tok.value.sz, tok.value.ptr);
    return (Value){T_NIL}; // dum dum compiler
}

Value exec_call(Nero *nr) {
    Token tok = PEEK(0);
    ADVANCE(1);
    Value args = {T_LIST, .as_list = nero_list_alloc()};
    do {
        ADVANCE(1); // '(' | ','
        if (PEEK(0).kind == TK_RPAREN) break;
        Value val = exec_expr(nr);
        LIST_PUSHP(args.as_list, val);
    } while (PEEK(0).kind == TK_COMMA);
    if (PEEK(0).kind != TK_RPAREN) ERROR("Missing ')'\n", errpos(nr, PEEK(0)));
    ADVANCE(1);
    Function fn;
    if ((fn = get_fun(&nr->funs, tok.value)).code.start == -1)
        return call_foreign(nr, tok, args);
    if (fn.vars.sz != args.as_list->sz)
        ERROR("Function '%.*s' expects %d argument(s), got %d\n",
            errpos(nr, tok), tok.value.sz, tok.value.ptr, fn.vars.sz, args.as_list->sz);
    VariableList vars = fn.vars, copy = copy_vars(&fn.vars);
    fn.vars = copy;
    for (int i = 0; i < fn.vars.sz; ++i)
        set_var(&fn.vars, fn.vars.ptr[i].name, args.as_list->ptr[i]);
    Value res = nero_call(nr, fn);
    fn.vars = vars;
    return res;
}

Block parse_block(Nero *nr) {
    Block blk;
    int id = 0;
    if (PEEK(0).kind != TK_LBRACK) ERROR("Expected '{'\n", errpos(nr, PEEK(0)));
    ADVANCE(1);
    blk.start = nr->ip;
    while (PEEK(0).kind != TK_EOF) {
        if (PEEK(0).kind == TK_LBRACK) ++id;
        if (PEEK(0).kind == TK_RBRACK) if (--id < 0) break;
        ADVANCE(1);
    }
    if (PEEK(0).kind != TK_RBRACK) ERROR("Missing '}'\n", errpos(nr, PEEK(0)));
    blk.end = nr->ip;
    ADVANCE(1);
    return blk;
}

Value exec_variable(Nero *nr) {
    Token var = PEEK(0);
    if (PEEK(1).kind == TK_EQ) return exec_assign(nr);
    if (PEEK(1).kind == TK_LPAREN) return exec_call(nr);
    ADVANCE(1);
    Value res = get_var(&nr->fn->vars, var.value);
    if (res.type == T_BOOL && res.as_int == -1)
        res = get_var(&nr->vars, var.value);
    if (res.type == T_BOOL && res.as_int == -1)
        ERROR("Undefined variable '%.*s'\n", errpos(nr, var), var.value.sz, var.value.ptr);
    return res;
}

Value exec_primary(Nero *nr) {
    switch (PEEK(0).kind) {
    case TK_WORD:    return exec_variable(nr);
    case TK_NUMBER:  return exec_number(nr);
    case TK_STRING:  return exec_string(nr);
    case TK_LSQUARE: return exec_list(nr);
    case TK_LBRACK:  return exec_dict(nr);
    case TK_NIL:
        ADVANCE(1);
        return (Value) {T_NIL};
    case TK_FALSE:
        ADVANCE(1);
        return (Value) {T_BOOL, .as_int = 0};
    case TK_TRUE:
        ADVANCE(1);
        return (Value) {T_BOOL, .as_int = 1};
    case TK_LPAREN: {
        Token tk = PEEK(0);
        ADVANCE(1);
        Value ret = exec_expr(nr);
        if (PEEK(0).kind != TK_RPAREN) ERROR("Missing ')'\n", errpos(nr, tk));
        ADVANCE(1);
        return ret;
    }
    default: ERROR("Unexpected token\n", errpos(nr, PEEK(0)));
    }
    return (Value){T_NIL}; // dum dum compiler
}

Value exec_dict_key(Nero *nr, Value dict) {
    if (dict.type != T_DICT) ERROR("Expected dict\n", errpos(nr, PEEK(0)));
    Token tok = PEEK(0);
    const int is_bracket = (tok.kind == TK_LSQUARE);
    String *key;

    if (tok.kind != TK_DOT && !is_bracket) ERROR("Missing '.'\n", errpos(nr, PEEK(0)));
    ADVANCE(1);
    if (is_bracket) {
        Value val = exec_expr(nr);
        if (val.type != T_STR) ERROR("Expected str\n", errpos(nr, PEEK(0)));
        key = nero_string_copy(*val.as_str);
        if (PEEK(0).kind != TK_RSQUARE) ERROR("Missing ']'\n", errpos(nr, PEEK(0)));
    } else {
        tok = PEEK(0);
        if (tok.kind != TK_WORD && tok.kind != TK_STRING) ERROR("Expected word\n", errpos(nr, tok));
        key = nero_string_copy(tok.value);
    }
    ADVANCE(1);
    if (PEEK(0).kind == TK_EQ) {
        ADVANCE(1);
        Value val = exec_expr(nr);
        set_var(dict.as_dict, *key, val);
        return val;
    }
    Value val = get_var(dict.as_dict, *key);
    if (val.type == T_BOOL && val.as_int == -1)
        ERROR("Dict has no key '%.*s'\n", errpos(nr, tok), key->sz, key->ptr);
    return val;
}

void nero_check_bounds(Nero *nr, Value list, int idx) {
    if (list.type != T_LIST && list.type != T_STR) ERROR("Expected list\n", errpos(nr, PEEK(0)));
    int size = list.type == T_STR? list.as_str->sz : list.as_list->sz;
    if (idx < 0 || idx >= size) ERROR("List index out of range\n", errpos(nr, PEEK(0)));
}

Value nero_get_index(Nero *nr, Value list, int idx) {
    nero_check_bounds(nr, list, idx);
    Value val;
    if (list.type == T_STR) {
        String s = {.sz = 1, .ptr = &list.as_str->ptr[idx]};
        val = nero_copy((Value){T_STR, .as_str = nero_string_copy(s)});
        return val;
    }
    val = list.as_list->ptr[idx];
    return val;
}

Value exec_list_index(Nero *nr, Value list) {
    if (list.type == T_DICT) return exec_dict_key(nr, list);
    if (list.type != T_LIST && list.type != T_STR) ERROR("Expected list\n", errpos(nr, PEEK(0)));
    Token tk = PEEK(0);
    if (PEEK(0).kind != TK_LSQUARE) ERROR("Missing '['\n", errpos(nr, PEEK(0)));
    ADVANCE(1);
    Value index = exec_expr(nr);
    if (PEEK(0).kind != TK_RSQUARE) ERROR("Missing ']'\n", errpos(nr, PEEK(0)));
    ADVANCE(1);
    EXPECT_INT(tk, index);
    int idx = index.as_int;
    if (PEEK(0).kind == TK_EQ) {
        ADVANCE(1);
        if (list.type != T_LIST) ERROR("Unexpected '='\n", errpos(nr, tk));
        nero_check_bounds(nr, list, idx);
        Value val = exec_expr(nr);
        list.as_list->ptr[idx] = val;
        return val;
    }
    return nero_get_index(nr, list, idx);
}

Value exec_term(Nero *nr) {
    Value val = exec_primary(nr);
    while (1) {
        if (PEEK(0).kind == TK_LSQUARE) val = exec_list_index(nr, val);
        else if (PEEK(0).kind == TK_DOT) val = exec_dict_key(nr, val);
        else break;
    }
    return val;
}

Value exec_factor(Nero *nr) {
    Token tk = PEEK(0);
    switch (tk.kind) {
    case TK_MINUS: {
        ADVANCE(1);
        Value ret = exec_term(nr);
        EXPECT_NUMBER(tk, ret);
        return (Value){ret.type, { ret.type == T_INT? -ret.as_int : -ret.as_real }};
    }
    case TK_NOT: {
        ADVANCE(1);
        return (Value) {T_BOOL, .as_int = !nero_true(exec_term(nr))};
    }
    case TK_BNOT: {
        ADVANCE(1);
        Value ret = exec_term(nr);
        EXPECT_INT(tk, ret);
        return (Value) {T_INT, .as_int = ~ret.as_int};
    }
    default: return exec_term(nr);
    }
}

Value exec_muldiv(Nero *nr) {
    Value val = exec_factor(nr);
    Token tk = PEEK(0);
    while (tk.kind == TK_MUL || tk.kind == TK_DIV || tk.kind == TK_MOD) {
        ADVANCE(1);
        Value other = exec_factor(nr);
        EXPECT_NUMBER(tk, val); EXPECT_NUMBER(tk, other);
        switch (tk.kind) {
        case TK_MUL:
            OPERATE_ON_NUMBER(*, val, other);
            break;
        case TK_DIV:
            if (GET_NUMBER(other) == 0) ERROR("Division by zero\n", errpos(nr, tk));
            OPERATE_ON_NUMBER(/, val, other);
            break;
        case TK_MOD:
            EXPECT_INT(tk, val); EXPECT_INT(tk, other);
            if (other.as_int == 0) ERROR("Division by zero\n", errpos(nr, tk));
            val.as_int %= other.as_int;
            break;
        default: ERROR("Unexpected token\n", errpos(nr, tk));
        }
        tk = PEEK(0);
    }
    return val;
}

Value exec_addsub(Nero *nr) {
    Value val = exec_muldiv(nr);
    Token tk = PEEK(0);
    while (tk.kind == TK_PLUS || tk.kind == TK_MINUS) {
        ADVANCE(1);
        Value other = exec_muldiv(nr);
        EXPECT_NUMBER(tk, val);
        EXPECT_NUMBER(tk, other);
        switch (tk.kind) {
        case TK_PLUS:  OPERATE_ON_NUMBER(+, val, other); break;
        case TK_MINUS: OPERATE_ON_NUMBER(-, val, other); break;
        default: ERROR("Unexpected token\n", errpos(nr, tk));
        }
        tk = PEEK(0);
    }
    return val;
}

Value exec_bitshift(Nero *nr) {
    Value val = exec_addsub(nr);
    Token tk = PEEK(0);
    while (tk.kind == TK_SHL || tk.kind == TK_SHR) {
        ADVANCE(1);
        Value other = exec_addsub(nr);
        EXPECT_INT(tk, val); EXPECT_INT(tk, other);
        switch (tk.kind) {
        case TK_SHL: val.as_int = val.as_int << other.as_int; break;
        case TK_SHR: val.as_int = val.as_int >> other.as_int; break;
        default: ERROR("Unexpected token\n", errpos(nr, tk));
        }
        tk = PEEK(0);
    }
    return val;
}

Value exec_bitwise(Nero *nr) {
    Value val = exec_bitshift(nr);
    Token tk = PEEK(0);
    while (tk.kind == TK_BAND || tk.kind == TK_BOR || tk.kind == TK_XOR) {
        ADVANCE(1);
        Value other = exec_bitshift(nr);
        EXPECT_INT(tk, val); EXPECT_INT(tk, other);
        switch (tk.kind) {
        case TK_BAND: val.as_int = val.as_int & other.as_int; break;
        case TK_BOR:  val.as_int = val.as_int | other.as_int; break;
        case TK_XOR:  val.as_int = val.as_int ^ other.as_int; break;
        default: ERROR("Unexpected token\n", errpos(nr, tk));
        }
        tk = PEEK(0);
    }
    return val;
}

Value exec_compare(Nero *nr) {
    Value val = exec_bitwise(nr);
    Token tk = PEEK(0);
    if (tk.kind >= TK_EQEQ && tk.kind <= TK_GEQ) {
        ADVANCE(1);
        Value other = exec_bitwise(nr);
        switch (tk.kind) {
        case TK_EQEQ: val = nero_equals(val, other); break;
        case TK_NEQ:  val = (Value){T_BOOL, .as_int = !nero_equals(val, other).as_int}; break;
        case TK_LT: case TK_GT: case TK_LEQ: case TK_GEQ:
            EXPECT_NUMBER(tk, val);
            EXPECT_NUMBER(tk, other);
            return nero_compare(val, other, tk.kind);
        default: ERROR("Unexpected token\n", errpos(nr, tk));
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
        case TK_AND: val = (Value) {T_BOOL, .as_int = nero_true(val) && nero_true(other)}; break;
        case TK_OR:  val = (Value) {T_BOOL, .as_int = nero_true(val) || nero_true(other)}; break;
        default: ERROR("Unexpected token\n", errpos(nr, tk));
        }
        tk = PEEK(0);
    }
    return val;
}

Value exec_def(Nero *nr) {
    ADVANCE(1);
    Token name = PEEK(0);
    if (name.kind != TK_WORD) ERROR("Expected function name\n", errpos(nr, name));
    ADVANCE(1);
    if (PEEK(0).kind != TK_LPAREN) ERROR("Missing '('\n", errpos(nr, PEEK(0)));
    VariableList args = LIST_ALLOC(Variable);
    do {
        ADVANCE(1); // '(' | ','
        if (PEEK(0).kind == TK_RPAREN) break;
        if (PEEK(0).kind != TK_WORD) ERROR("Expected word\n", errpos(nr, PEEK(0)));
        String arg = PEEK(0).value;
        ADVANCE(1); // arg
        set_var(&args, arg, (Value){T_NIL});
    } while (PEEK(0).kind == TK_COMMA);

    if (PEEK(0).kind != TK_RPAREN) ERROR("Missing ')'\n", errpos(nr, PEEK(0)));
    ADVANCE(1);

    Block code = parse_block(nr);
    Function fn = {
        .name = name.value,
        .code = code,
        .vars = args,
    };

    if (!nr->funs.alloc) nr->funs = (FunctionList) LIST_ALLOC(Function);
    for (int i = 0; i < nr->funs.sz; ++i) {
        if (STRCMPS(nr->funs.ptr[i].name, fn.name)) {
            nr->funs.ptr[i] = fn;
            goto end;
        }
    }
    LIST_PUSH(nr->funs, fn);
end:
    return (Value) {T_NIL};
}

Value exec_return(Nero *nr) {
    ADVANCE(1);
    Value res = {T_NIL};
    // if token on same line, and not '}' nor ')', then return an expression, otherwise,
    // it's probably an empty return within some poorly written code.
    if (PEEK(0).kind != TK_RBRACK && PEEK(0).kind != TK_RPAREN && PEEK(0).line == PEEK(-1).line)
        res = exec_expr(nr);
    nr->ret = RET_RETURN;
    return res;
}

Value exec_break(Nero *nr, uint8_t brk) {
    ADVANCE(1);
    nr->ret = brk;
    return (Value) {T_NIL};
}

Value exec_if(Nero *nr) {
    struct { int cond; Block body; } conditions[MAX_CONDITIONS];
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

    for (int i = 0; i < sz; ++i)
        if (conditions[i].cond) return exec_block(nr, conditions[i].body);
    if (else_body.start != -1) return exec_block(nr, else_body);
    return (Value) {T_NIL};
}

Value exec_while(Nero *nr) {
    Value res = {T_NIL};
    ADVANCE(1);
    int start = nr->ip;
    Value cond = exec_expr(nr);
    Block body = parse_block(nr);

    while (nero_true(cond)) {
        res = exec_block(nr, body);
        if (nr->ret == RET_RETURN) return res;
        else if (nr->ret == RET_NEXT) nr->ret = RET_NO;
        else if (nr->ret == RET_BREAK) { nr->ret = RET_NO; break; }
        nr->ip = start;
        cond = exec_expr(nr);
    }
    nr->ip = body.end+1;
    return res;
}

Value exec_for(Nero *nr) {
    Value res = {T_NIL};
    ADVANCE(1);
    Token var = PEEK(0);
    Token idx = {0};
    if (var.kind != TK_WORD) ERROR("Expected word\n", errpos(nr, var));
    ADVANCE(1);
    if (PEEK(0).kind == TK_COMMA) {
        ADVANCE(1);
        idx = PEEK(0);
        if (idx.kind != TK_WORD) ERROR("Expected word\n", errpos(nr, idx));
        ADVANCE(1);
    }
    if (PEEK(0).kind != TK_EQ) ERROR("Expected '='\n", errpos(nr, var));
    ADVANCE(1);
    Value iter = exec_expr(nr);
    Block body = parse_block(nr);
    int sz;

    if (iter.type == T_STR || iter.type == T_LIST) {
        sz = iter.type == T_STR? iter.as_str->sz : iter.as_list->sz;
    } else if (iter.type == T_INT) {
        if (idx.kind != TK_EOF) ERROR("Expected list\n", errpos(nr, var));
        sz = iter.as_int;
    } else ERROR("Expected list\n", errpos(nr, var));

    Value val;
    for (int i = 0; i < sz; ++i) {
        Value index = (Value){T_INT, .as_int = i};
        val = (iter.type == T_INT)? index : nero_get_index(nr, iter, i);
        set_var(&nr->fn->vars, var.value, val);
        if (idx.kind != TK_EOF) set_var(&nr->fn->vars, idx.value, index);
        res = exec_block(nr, body);
        if (nr->ret == RET_RETURN) return res;
        else if (nr->ret == RET_NEXT) nr->ret = RET_NO;
        else if (nr->ret == RET_BREAK) { nr->ret = RET_NO; break; }
    }
    nr->ip = body.end+1;
    return res;
}

Value exec_keyword(Nero *nr) {
    switch (PEEK(0).kind) {
    case TK_DEF:    return exec_def(nr);
    case TK_RETURN: return exec_return(nr);
    case TK_BREAK:  return exec_break(nr, RET_BREAK);
    case TK_NEXT:   return exec_break(nr, RET_NEXT);
    case TK_IF:     return exec_if(nr);
    case TK_WHILE:  return exec_while(nr);
    case TK_FOR:    return exec_for(nr);
    default: ERROR("Unexpected token\n", errpos(nr, PEEK(0)));
    }
    return (Value){T_NIL}; // dum dum compiler
}

Value exec_block(Nero *nr, Block blk) {
    int ip = nr->ip;
    nr->ret = RET_NO;
    nr->ip = blk.start;
    Value res = {T_NIL};
    while (nr->ip < blk.end && PEEK(0).kind != TK_EOF) {
        res = exec_expr(nr);
        if (nr->ret != RET_NO) break;
    }
    nr->ip = ip;
    return res;
}

Value exec_expr(Nero *nr) {
    if (PEEK(0).kind >= TK_DEF && PEEK(0).kind <= TK_NEXT) return exec_keyword(nr);
    return exec_andor(nr);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input>\n", argv[0]);
        return 1;
    }

    args_list = (ValueList) LIST_ALLOC(Value);
    for (int i = 1; i < argc; ++i) {
        String s = {.sz = strlen(argv[i]), .ptr = argv[i]};
        Value arg = {T_STR, .as_str = nero_string_copy(s)};
        LIST_PUSH(args_list, nero_copy(arg));
    }

    Nero nero = {0};
    String file = {.sz = strlen(argv[1]), .ptr = argv[1]};
    Function global = { .name = {.alloc = 0, .sz = 8, .ptr = "<global>"} };
    nero.fn = &global;
    tokenize(&nero, file);
    nero_init_foreign(&nero);
    global.code = (Block) {0, nero.code.sz};
    nero_call(&nero, global);
    return 0;
}
