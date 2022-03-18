#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

//#if _WIN32
//#define CH_EOF '\n\r'
//#elif __linux__
//#define CH_EOF '\n'
//#else
//#define CH_EOF '\r'
//#endif
#define CH_EOF -1

int token;
char ch;
FILE *fin;
int tkvalue;
char *filename = "";
int line_num = 0;

void get_token();

char *get_tkstr(int);

void preprocess();

void parse_identifier();

void parse_num();

void parse_string(char);

void skip_white_space();

void parse_comment();


enum e_ErrorLevel {
    LEVEL_WARNING,
    LEVEL_ERROR,
};

enum e_WorkStage {
    STAGE_COMPILER,
    STAGE_LINK,
};

void handle_exception(int stage, int level, char *fmt, va_list ap) {
    char buf[1024];

    vsprintf(buf, fmt, ap);
    if (stage == STAGE_COMPILER) {
        if (level == LEVEL_WARNING) {
            printf("[WARNING][COMPILER]%s(line:%d): %s!\n", filename, line_num, buf);
        } else {
            printf("[ERROR][COMPILER]%s(line:%d): %s!\n", filename, line_num, buf);
            exit(-1);
        }
    } else {
        printf("LNK: %s!\n", buf);
        exit(-1);
    }
}

void warning(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    handle_exception(STAGE_COMPILER, LEVEL_WARNING, fmt, ap);
    va_end(ap);
}

void error(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    handle_exception(STAGE_COMPILER, LEVEL_ERROR, fmt, ap);
    va_end(ap);
}

void expect(char *msg) {
    error("loss %s", msg);
}

void skip(int c) {
    if (token != c) {
        error("loss '%s'", get_tkstr(c));
    }
    get_token();
}

void link_error(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    handle_exception(STAGE_LINK, LEVEL_ERROR, fmt, ap);
    va_end(ap);
}

// Token code
enum e_TokenCode {
    TK_PLUS,
    TK_MINUS,
    TK_STAR,
    TK_DIVIDE,
    TK_MOD,
    TK_EQ,
    TK_NEQ,
    TK_LT,
    TK_LEQ,
    TK_GT,
    TK_GEQ,
    TK_ASSIGN,
    TK_POINTSTO,
    TK_DOT,
    TK_AND,
    TK_OPENPA,
    TK_CLOSEPA,
    TK_OPENBR,
    TK_CLOSEBR,
    TK_BEGIN,
    TK_END,
    TK_SEMICOLON,
    TK_COMMA,
    TK_ELLIPSIS,
    TK_EOF,

    TK_CINT,
    TK_CCHAR,
    TK_CSTR,

    // key word
    KW_CHAR,
    KW_SHORT,
    KW_INT,
    KW_VOID,
    KW_STRUCT,
    KW_IF,
    KW_ELSE,
    KW_FOR,
    KW_CONTINUE,
    KW_BREAK,
    KW_RETURN,
    KW_SIZEOF,
    KW_CDECL,
    KW_STDCALL,
    KW_ALIGN,

    TK_IDENT,
};

// 动态字符串
typedef struct DynString {
    int count;
    int capacity;
    char *data;
} DynString;

void dynstring_init(DynString *pstr, int initsize) {
    if (pstr != NULL) {
        pstr->data = (char *) malloc(sizeof(char) * initsize);
        pstr->count = 0;
        pstr->capacity = initsize;
    }
}

void dynstring_free(DynString *pstr) {
    if (pstr != NULL) {
        if (pstr->data) {
            free(pstr->data);
        }
        pstr->count = 0;
        pstr->capacity = 0;
    }
}

void dynstring_reset(DynString *pstr) {
    dynstring_free(pstr);
    dynstring_init(pstr, 8);
}

void dynstring_realloc(DynString *pstr, int new_size) {
    int cap;
    char *data;

    cap = pstr->capacity;

    if (new_size <= cap) {
        perror("the new_size need big than capacity");
        return;
    }

    cap = new_size;
    data = realloc(pstr->data, new_size * sizeof(char));
    if (!data) {
        perror("alloc error");
    }
    pstr->capacity = cap;
    pstr->data = data;
}

void dynstring_chcat(DynString *pstr, char ch) {
    int count = pstr->count + 1;
    if (count >= (pstr->capacity * 0.75)) {
        dynstring_realloc(pstr, pstr->capacity * 2);
    }
    ((char *) pstr->data)[count - 1] = ch;
    pstr->count = count;
}

// 动态数组
typedef struct DynArray {
    int count;
    int capacity;
    void **data;
} DynArray;

void dynArray_init(DynArray *parr, int initsize) {
    if (parr != NULL) {
        parr->data = (void **) malloc(sizeof(void *) * initsize);
        parr->count = 0;
        parr->capacity = initsize;
    }
}

void dynArray_free(DynArray *parr) {
    if (parr != NULL) {
        if (parr->data) {
            void **p;
            for (p = parr->data; parr->count; ++p, --parr->count) {
                if (*p) {
                    free(*p);
                }
            }
            free(parr->data);
            parr->data = NULL;
        }
        parr->count = 0;
        parr->capacity = 0;
    }
}

void dynArray_realloc(DynArray *parr, int new_size) {
    int cap;
    void *data;

    cap = parr->capacity;

    if (new_size <= cap) {
        perror("the new_size need big than capacity");
        return;
    }

    cap = new_size;
    data = realloc(parr->data, sizeof(void *) * new_size);
    if (!data) {
        perror("alloc error");
    }
    parr->capacity = cap;
    parr->data = data;
}

void dynArray_add(DynArray *parr, void *data) {
    int count = parr->count + 1;
    if (count >= (parr->capacity * 0.75)) {
        dynArray_realloc(parr, parr->capacity * 2);
    }
    ((void **) parr->data)[count - 1] = data;
    parr->count = count;
}

int dynArray_search(DynArray *parr, int key) {
    int **p = (int **) parr->data;
    for (int i = 0; i < parr->count; ++i, p++) {
        if (key == **p) {
            return i;
        }
    }
    return -1;
}


// 计算hash
#define MAXKEY 1024

int elf_hash(char *key) {
    int h = 0, g;
    while (*key) {
        h = (h << 4) + *key++;
        g = h & 0xf0000000;
        if (g) {
            h ^= g >> 24;
        }
        h &= ~g;
    }
    return h % MAXKEY;
}

struct Symbol {
};

typedef struct TkWord {
    int tkcode;
    struct TkWord *next;
    char *spelling;
    struct Symbol *sym_struct;
    struct Symbol *sym_identifier;
} TkWord;

TkWord *tk_hashtable[MAXKEY];
DynArray tktable;
int token;
DynString tkstr, sourcestr;

TkWord *tkWord_direct_insert(TkWord *tp) {
    int keyno;
    dynArray_add(&tktable, tp);
    keyno = elf_hash(tp->spelling);
    tp->next = tk_hashtable[keyno];
    tk_hashtable[keyno] = tp;
    return tp;
}

TkWord *tkWord_find(char *p) {
    int keyno = elf_hash(p);
    TkWord *tp = NULL, *tpl;
    for (tpl = tk_hashtable[keyno]; tpl; tpl = tpl->next) {
        if (!strcmp(p, tpl->spelling)) {
            token = tpl->tkcode;
            tp = tpl;
            break;
        }
    }
    return tp;
}

TkWord *tkWord_insert(char *p) {
    TkWord *tp;
    int keyno = elf_hash(p);
    char *s;
    char *end;
    int length;

    tp = tkWord_find(p);
    if (tp == NULL) {
        length = strlen(p);
        tp = (TkWord *) malloc(sizeof(TkWord) + length + 1);
        tp->next = tk_hashtable[keyno];
        tk_hashtable[keyno] = tp;

        dynArray_add(&tktable, tp);
        tp->tkcode = tktable.count - 1;
        s = (char *) tp + sizeof(TkWord);
        tp->spelling = (char *) s;
        for (end = p + length; p < end;) {
            *s++ = *p++;
        }
        *s = (char) '\0';
    }
    return tp;
}

void *mallocz(int size) {
    if (size <= 0) {
        perror("size must > 0");
        return NULL;
    }
    void *ptr;
    ptr = malloc(size);
    if (!ptr && size) {
        perror("size must > 0");
        return NULL;
    }
    memset(ptr, 0, size);
    return ptr;
}

char *get_tkstr(int v) {
    if (v >= tktable.count) {
        return NULL;
    } else if (v >= TK_CINT && v <= TK_CSTR) {
        return "";
    } else {
        return ((TkWord *) tktable.data[v])->spelling;
    }
}

void getch() {
    int i = getc(fin);
    ch = i;
}

void get_token() {
    preprocess();
    switch (ch) {
        case 'a' :
        case 'b' :
        case 'c' :
        case 'd' :
        case 'e' :
        case 'f' :
        case 'g' :
        case 'h' :
        case 'i' :
        case 'j' :
        case 'k' :
        case 'l' :
        case 'm' :
        case 'n' :
        case 'o' :
        case 'p' :
        case 'q' :
        case 'r' :
        case 's' :
        case 't' :
        case 'u' :
        case 'v' :
        case 'w' :
        case 'x' :
        case 'y' :
        case 'z' :
        case 'A' :
        case 'B' :
        case 'C' :
        case 'D' :
        case 'E' :
        case 'F' :
        case 'G' :
        case 'H' :
        case 'I' :
        case 'J' :
        case 'K' :
        case 'L' :
        case 'M' :
        case 'N' :
        case 'O' :
        case 'P' :
        case 'Q' :
        case 'R' :
        case 'S' :
        case 'T' :
        case 'U' :
        case 'V' :
        case 'W' :
        case 'X' :
        case 'Y' :
        case 'Z' :
        case '_': {
            TkWord *tp;
            parse_identifier();
            tp = tkWord_insert(tkstr.data);
            token = tp->tkcode;
            break;
        }
        case '0' :
        case '1' :
        case '2' :
        case '3' :
        case '4' :
        case '5' :
        case '6' :
        case '7' :
        case '8' :
        case '9': {
            parse_num();
            token = TK_CINT;
            break;
        }
        case '+':
            getch();
            token = TK_PLUS;
            break;
        case '-':
            getch();
            if (ch == '>') {
                token = TK_POINTSTO;
                getch();
            } else {
                token = TK_MINUS;
            }
            break;
        case '/':
            token = TK_DIVIDE;
            getch();
            break;
        case '%':
            token = TK_MOD;
            getch();
            break;
        case '=':
            getch();
            if (ch == '=') {
                token = TK_EQ;
                getch();
            } else {
                token = TK_ASSIGN;
            }
            break;
        case '!':
            getch();
            if (ch == '=') {
                token = TK_NEQ;
                getch();
            } else {
                error("now we can't support '!'");
            }
            break;
        case '<':
            getch();
            if (ch == '=') {
                token = TK_LEQ;
                getch();
            } else {
                token = TK_LT;
            }
            break;
        case '>':
            getch();
            if (ch == '=') {
                token = TK_GEQ;
                getch();
            } else {
                token = TK_GT;
            }
            break;
        case '.':
            getch();
            if (ch == '.') {
                getch();
                if (ch != '.') {
                    error("is '...' you want to write");
                } else {
                    token = TK_ELLIPSIS;
                    getch();
                }
                getch();
            } else {
                token = TK_DOT;
            }
            break;
        case '&':
            token = TK_AND;
            getch();
            break;
        case ';':
            token = TK_SEMICOLON;
            getch();
            break;
        case ']':
            token = TK_CLOSEBR;
            getch();
            break;
        case '}':
            token = TK_END;
            getch();
            break;
        case ')':
            token = TK_CLOSEPA;
            getch();
            break;
        case '[':
            token = TK_OPENBR;
            getch();
            break;
        case '{':
            token = TK_BEGIN;
            getch();
            break;
        case '(':
            token = TK_OPENPA;
            getch();
            break;
        case ',':
            token = TK_COMMA;
            getch();
            break;
        case '*':
            token = TK_STAR;
            getch();
            break;
        case '\'':
            parse_string(ch);
            token = TK_CCHAR;
            tkvalue = *(char *) tkstr.data;
            break;
        case '\"':
            parse_string(ch);
            token = TK_CSTR;
            break;
        case EOF:
            token = TK_EOF;
            break;
        default:
            error("understand this char: \\x%02x", ch);
            getch();
            break;
    }
}

void init_lex() {
    TkWord *tp;
    static TkWord keywords[] = {
            {TK_PLUS,      NULL, "+",           NULL, NULL},
            {TK_MINUS,     NULL, "-",           NULL, NULL},
            {TK_STAR,      NULL, "*",           NULL, NULL},
            {TK_DIVIDE,    NULL, "/",           NULL, NULL},
            {TK_MOD,       NULL, "%",           NULL, NULL},
            {TK_EQ,        NULL, "==",          NULL, NULL},
            {TK_NEQ,       NULL, "!=",          NULL, NULL},
            {TK_LT,        NULL, "<",           NULL, NULL},
            {TK_LEQ,       NULL, "<=",          NULL, NULL},
            {TK_GT,        NULL, ">",           NULL, NULL},
            {TK_GEQ,       NULL, ">=",          NULL, NULL},
            {TK_ASSIGN,    NULL, "=",           NULL, NULL},
            {TK_POINTSTO,  NULL, "->",          NULL, NULL},
            {TK_DOT,       NULL, ".",           NULL, NULL},
            {TK_AND,       NULL, "&",           NULL, NULL},
            {TK_OPENPA,    NULL, "(",           NULL, NULL},
            {TK_CLOSEPA,   NULL, ")",           NULL, NULL},
            {TK_OPENBR,    NULL, "[",           NULL, NULL},
            {TK_CLOSEBR,   NULL, "]",           NULL, NULL},
            {TK_BEGIN,     NULL, "{",           NULL, NULL},
            {TK_END,       NULL, "}",           NULL, NULL},
            {TK_SEMICOLON, NULL, ";",           NULL, NULL},
            {TK_COMMA,     NULL, ",",           NULL, NULL},
            {TK_ELLIPSIS,  NULL, "...",         NULL, NULL},
            {TK_EOF,       NULL, "End_Of_File", NULL, NULL},

            {TK_CINT,      NULL, "int",         NULL, NULL},
            {TK_CCHAR,     NULL, "char",        NULL, NULL},
            {TK_CSTR,      NULL, "string",      NULL, NULL},


            {KW_CHAR,      NULL, "char",        NULL, NULL},
            {KW_SHORT,     NULL, "short",       NULL, NULL},
            {KW_INT,       NULL, "int",         NULL, NULL},
            {KW_VOID,      NULL, "void",        NULL, NULL},
            {KW_STRUCT,    NULL, "struct",      NULL, NULL},
            {KW_IF,        NULL, "if",          NULL, NULL},
            {KW_ELSE,      NULL, "else",        NULL, NULL},
            {KW_FOR,       NULL, "for",         NULL, NULL},
            {KW_CONTINUE,  NULL, "continue",    NULL, NULL},
            {KW_BREAK,     NULL, "break",       NULL, NULL},
            {KW_RETURN,    NULL, "return",      NULL, NULL},
            {KW_SIZEOF,    NULL, "sizeof",      NULL, NULL},
            {KW_CDECL,     NULL, "__cdecl",     NULL, NULL},
            {KW_STDCALL,   NULL, "__stdcall",   NULL, NULL},
            {KW_ALIGN,     NULL, "__align",     NULL, NULL},

            {0,            NULL, NULL,          NULL, NULL},
    };

    dynArray_init(&tktable, 50);
    for (tp = &keywords[0]; tp->spelling != NULL; tp++) {
        tkWord_direct_insert(tp);
    }
}

void preprocess() {
    while (1) {
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
            skip_white_space();
        } else if (ch == '/') {
            getch();
            if (ch == '*' || ch == '/') {
                parse_comment();
            } else {
                ungetc(ch, fin);
                ch = '/';
                break;
            }
        } else {
            break;
        }
    }
}

void parse_comment() {
    if (ch == '/') {
        while (1) {
            getch();
            if (ch == '\n') {
                line_num++;
                getch();
                return;
            } else if (ch == CH_EOF) {
                error("loss the right end-sign of '*/'");
                return;
            }
        }
    }
    getch();
    do {
        do {
            if (ch == '\n' || ch == '*' || (ch == CH_EOF)) {
                break;
            } else {
                getch();
            }
        } while (1);
        if (ch == '\n') {
            line_num++;
            getch();
        } else if (ch == '*') {
            getch();
            if (ch == '/') {
                getch();
                return;
            }
        } else {
            error("loss the right end-sign of '*/'");
            return;
        }
    } while (1);
}

void skip_white_space() {
    while (1) {
        if (ch == ' ' || ch == '\t') {
            getch();
            continue;
        }
#if __APPLE__
        if (ch == '\n') {
            line_num++;
            getch();
            continue;
        }
#elif __linux__
        if (ch == '\n') {
            line_num++;
            getch();
            continue;
        }
#else
        if (ch == '\r') {
            getch();
            if (ch != '\n') {
                return;
            }
            line_num++;
            getch();
            continue;
        }
#endif
        break;
    }
}

int is_nodigit(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_');
}

int is_digit(char c) {
    return c >= '0' && c <= '9';
}

void parse_identifier() {
    dynstring_reset(&tkstr);
    dynstring_chcat(&tkstr, ch);
    getch();
    while (is_nodigit(ch) || is_digit(ch)) {
        dynstring_chcat(&tkstr, ch);
        getch();
    }
    dynstring_chcat(&tkstr, '\0');
}

// todo: atof
void parse_num() {
    dynstring_reset(&tkstr);
    dynstring_reset(&sourcestr);
    do {
        dynstring_chcat(&tkstr, ch);
        dynstring_chcat(&sourcestr, ch);
        getch();
    } while (is_digit(ch));
    if (ch == '.') {
        do {
            dynstring_chcat(&tkstr, ch);
            dynstring_chcat(&sourcestr, ch);
            getch();
        } while (is_digit(ch));
    }
    dynstring_chcat(&tkstr, '\0');
    dynstring_chcat(&sourcestr, '\0');
    tkvalue = atoi(tkstr.data);
};

void parse_string(char sep) {
    char c;
    dynstring_reset(&tkstr);
    dynstring_reset(&sourcestr);
    dynstring_chcat(&sourcestr, sep);
    getch();
    while (1) {
        if (ch == sep) {
            break;
        } else if (ch == '\\') {
            // change mean
            dynstring_chcat(&sourcestr, ch);
            getch();
            switch (ch) {
                case '0':
                    c = '\0';
                    break;
                case 'a':
                    c = '\a';
                    break;
                case 'b':
                    c = '\b';
                    break;
                case 't':
                    c = '\t';
                    break;
                case 'n':
                    c = '\n';
                    break;
                case 'v':
                    c = '\v';
                    break;
                case 'f':
                    c = '\f';
                    break;
                case 'r':
                    c = '\r';
                    break;
                case '\"':
                    c = '\"';
                    break;
                case '\'':
                    c = '\'';
                    break;
                case '\\':
                    c = '\\';
                    break;
                default:
                    c = ch;
                    if (c >= '!' && c <= '~') {
                        warning("it's illegal change mean: \'\\%c\'", c);
                    } else {
                        warning("it's illegal change mean: \'\\0x%x\'", c);
                    }
                    break;
            }
            dynstring_chcat(&tkstr, c);
            dynstring_chcat(&sourcestr, ch);
            getch();
        } else {
            dynstring_chcat(&tkstr, ch);
            dynstring_chcat(&sourcestr, ch);
            getch();
        }
    }
    dynstring_chcat(&tkstr, '\0');
    dynstring_chcat(&sourcestr, sep);
    dynstring_chcat(&sourcestr, '\0');
    getch();
}

void init() {
    line_num = 1;
    init_lex();
}

void cleanup() {
    int i;
    printf("\n tktable.count=%d\n", tktable.count);
    for (i = TK_IDENT; i < tktable.count; ++i) {
        free(tktable.data[i]);
    }
    free(tktable.data);
}

// 句(语)法分析
int SC_GLOBAL = 0, SC_LOCAL = 1,SC_MEMBER;

// 翻译单元 --> {外部声明}文件结束符
void translation_unit();

// <外部声明> --> <函数定义>|<声明>
void external_declaration(int);

// <函数定义> --> <类型区分符><声明符><函数体>
// <函数体> --> <复合语句>
// <声明> --> <类型区分符>[<初值声明符表>]<分号>
// <初值声明符表> --> <初值声明符>{<逗号><初值声明符>}
// <初值声明符> --> <声明符>|<声明符><赋值运算符'='><初值符>
// 变换文法定义到LL(1) ==>> 提取共有左部<类型区分符>
// <外部声明> --> <类型区分符>(<声明符><函数体>|[<初值声明符表>]<分号>)
// 扩展 <外部声明> --> <类型区分符>(<声明符><函数体>|<分号>|
// <声明符>[<赋值运算符'='><初值符>]{<逗号><声明符>[<赋值运算符'='><初值符>]}<分号>)

//<类型区分符> --> <数据类型>|<结构区分符>
int type_specifier();

//<声明符> --> {<指针>}[<调用约定>][<结构成员对齐>]<直接声明符>
void declarator();

//<调用约定> --> <__cdecl>|<__stdcall>
void function_calling_convention(int *);

//<结构成员对齐> --> <__align>'('<整数常量>')'
void struct_member_alignment();

//<直接声明符> --> <标识符><直接声明符后缀>
void direct_declarator();

//<直接声明符后缀> --> {'['']'|'['<整数常量>']'|'('')'|'('<形参表>')'}
void direct_declarator_postfix();

//<形参表> --> <参数表>|<参数表>',''...'
//<参数表> --> <参数声明>{','<参数声明>}
//<参数声明> --> <类型区分符>{<声明符>}
void parameter_type_list();

//<函数体> --> <复合语句>
void funcbody();

//<复合语句> --> '{' {<声明>}{<语句>} '}'
void compound_statement();

//<初值符> --> <赋值表达式>
void initializer();

void assignment_expression();

//<结构区分符> --> <struct关键字><标识符>'{'<结构声明表>'}'|<struct关键字><标识符>
void struct_specifier();

//<结构声明表> --> <结构声明>{<结构声明>}
void struct_declaration_list();

//<结构声明> --> <类型区分符>{<结构声明符表>}';'
//<结构声明符表> --> <声明符>{','<声明符>}
void struct_declaration(int *, int *);

// ......
void translation_unit() {
    while (token != TK_EOF) {
        external_declaration(SC_GLOBAL);
    }
}

void external_declaration(int l) {
    if (!type_specifier()) {
        expect("<类型区分符>");
    }

    if (token == TK_SEMICOLON) {
        get_token();
        return;
    }
    while (1) {
        declarator();
        if (token == TK_BEGIN) {
            if (l == SC_LOCAL) {
                error("不支持嵌套定义");
            }
            funcbody();
            break;
        } else {
            if (token == TK_ASSIGN) {
                get_token();
                initializer();
            }
            if (token == TK_COMMA) {
                get_token();
            } else {
                skip(TK_SEMICOLON);
                break;
            }
        }
    }
}

int type_specifier() {
    int type_found = 0;
    switch (token) {
        case KW_CHAR:
        case KW_SHORT:
        case KW_VOID:
        case KW_INT:
            type_found = 1;
            get_token();
            break;
        case KW_STRUCT:
            struct_specifier();
            type_found = 1;
            break;
        default:
            break;
    }
    return type_found;
}

void struct_specifier() {
    int v;
    get_token();
    v = token;
    get_token();
    if (v < TK_IDENT) {
        expect("结构体名字不能是关键字");
    }
    if (token == TK_BEGIN) {
        struct_declaration_list();
    }
}

void struct_declaration_list() {
    int maxalign, offset;
    get_token();
    while (token != TK_END) {
        struct_declaration(&maxalign, &offset);
    }
    skip(TK_END);
}

void struct_declaration(int *a, int *b) {
    type_specifier();
    while (1) {
        declarator();
        if (token == TK_SEMICOLON) {
            break;
        }
        skip(TK_COMMA);
    }
    skip(TK_SEMICOLON);
}

void function_calling_convention(int *fc) {
    *fc = KW_CDECL;
    if (token == KW_CDECL || token == KW_STDCALL) {
        *fc = token;
        get_token();
    }
}

void struct_member_alignment() {
    if (token == KW_ALIGN) {
        get_token();
        skip(TK_OPENPA);
        if (token == TK_CINT) {
            get_token();
        } else {
            expect("常数整亮");
        }
        skip(TK_CLOSEPA);
    }
}

void declarator() {
    int fc;
    while (token == TK_STAR) {
        get_token();
    }
    function_calling_convention(&fc);
    struct_member_alignment();
    direct_declarator();
}

void direct_declarator() {
    if (token >= TK_IDENT) {
        get_token();
    } else {
        expect("标识符");
    }
    direct_declarator_postfix();
}

void direct_declarator_postfix() {
    int n;
    if (token == TK_OPENPA) {
        parameter_type_list();
    } else if (token == TK_OPENBR) {
        get_token();
        if (token == TK_CINT) {
            get_token();
            n = tkvalue;
        }
        skip(TK_CLOSEBR);
        direct_declarator_postfix();
    }
}

void parameter_type_list() {
    get_token();
    while (token != TK_CLOSEPA) {
        if (!type_specifier()) {
            error("无效类型标识符");
        }
        declarator();
        if (token == TK_CLOSEPA) {
            break;
        }
        skip(TK_COMMA);
        if (token == TK_ELLIPSIS) {
            get_token();
            break;
        }
    }
    skip(TK_CLOSEPA);
}

void funcbody() {
    compound_statement();
}

void initializer() {
    assignment_expression();
}

//<语句> --> {<复合语句>|<if>|<for>|
// <break>|<continue>|<return>|<表达式语句>}
void statement();

//<复合语句> --> '{' {<声明>}{<语句>} '}'
void compound_statement();

//<if> --> 'if''('<表达式>')'<语句>['else'<语句>]
void if_statement();

//<for> --> 'for''('<表达式语句><表达式语句><表达式语句>')'<语句>
void for_statement();

//<break> --> 'break' ';'
void break_statement();

//<continue> --> 'continue' ';'
void continue_statement();

//<return> --> 'return' <expression> ';'
void return_statement();

//<表达式语句> --> [<expression>]';'
void expression_statement();

int is_type_specifier(int);

//<表达式> --> <赋值表达式>{','<赋值表达式>}
void expression();

//<赋值表达式> --> <相等类表达式>|<一元表达式>'='<赋值表达式>
//<相等类表达式> ==>>(n) <一元表达式> ......
// 非等价变换后 <赋值表达式> --> <相等类表达式>|<一元表达式>'='<赋值表达式>
// 有隐患 但在语义分析阶段处理
void assignment_expression();

//<相等类表达式> --> <关系表达式>{'=='<关系表达式>|'!='<关系表达式>}
void equality_expression();

//<关系表达式> --> <加减类表达式>{'<'<加减类表达式>|'>'<加减类表达式>|
// '<='<加减类表达式>|'>='<加减类表达式>}
void relational_expression();

//<加减类表达式> --> <乘除类表达式>{'+'<乘除类表达式>|'-'<乘除类表达式>}
void additive_expression();

//<乘除表达式> --> <一元表达式>{'*'<一元表达式>|'/'<一元表达式>|'%'<一元表达式>}
void multiplicative_expression();

//<一元表达式> --> <后缀表达式>|'&'<一元表达式>|'*'<一元表达式>|'+'<一元表达式>|'-'<一元表达式>|<sizeof表达式>
void unary_expression();

//<后缀表达式> --> <初等表达式>{'['<expression>']'|'('')'|'('<实参表达式>')'
//|'.'IDENTIFIER|'->'IDENTIFIER}
void postfix_expression();
// <初等表达式> --> <标识符>|<整数常量>|<字符串常量>|<字符常量>|(<表达式>)
void primary_expression();
// <实参表达式> --> <赋值表达式>{','<赋值表达式>}
void argument_expression_list();
//<sizeof表达式> ==>> 'struct''('<类型区别符>')'
void sizeof_expression();


void statement() {
    switch (token) {
        case TK_BEGIN:
            compound_statement();
            break;
        case KW_IF:
            if_statement();
            break;
        case KW_FOR:
            for_statement();
            break;
        case KW_BREAK:
            break_statement();
            break;
        case KW_CONTINUE:
            continue_statement();
            break;
        case KW_RETURN:
            return_statement();
            break;
        default:
            expression_statement();
            break;
    }
}

void compound_statement() {
    get_token();
    while (is_type_specifier(token)) {
        external_declaration(SC_LOCAL);
    }
    while (token != TK_END) {
        statement();
    }
    get_token();
}

int is_type_specifier(int v) {
    switch (v) {
        case KW_CHAR:
        case KW_SHORT:
        case KW_INT:
        case KW_VOID:
        case KW_STRUCT:
            return 1;
        default:
            break;
    }
    return 0;
}

void expression_statement() {
    if (token != TK_SEMICOLON) {
        expression();
    }
    skip(TK_SEMICOLON);
}

void if_statement() {
    get_token();
    skip(TK_OPENPA);
    expression();
    skip(TK_CLOSEPA);
    statement();
    if (token == KW_ELSE) {
        get_token();
        statement();
    }
}

void for_statement() {
    get_token();
    skip(TK_OPENPA);
    if (token != TK_SEMICOLON) {
        expression();
    }
    skip(TK_SEMICOLON);
    if (token != TK_SEMICOLON) {
        expression();
    }
    skip(TK_SEMICOLON);
    if (token != TK_SEMICOLON) {
        expression();
    }
    skip(TK_CLOSEPA);
    statement();
}

void continue_statement() {
    get_token();
    skip(TK_SEMICOLON);
}

void break_statement() {
    get_token();
    skip(TK_SEMICOLON);
}

void return_statement() {
    get_token();
    if (token != TK_SEMICOLON) {
        expression();
    }
    skip(TK_SEMICOLON);
}

void expression() {
    while (1) {
        assignment_expression();
        if (token != TK_COMMA) {
            break;
        }
        get_token();
    }
}

void assignment_expression() {
    equality_expression();
    if (token == TK_ASSIGN) {
        get_token();
        assignment_expression();
    }
}

void equality_expression() {
    relational_expression();
    while (token == TK_EQ || token == TK_NEQ) {
        get_token();
        relational_expression();
    }
}

void relational_expression() {
    additive_expression();
    while (token == TK_LT || token == TK_LEQ ||
           token == TK_GT || token == TK_GEQ) {
        get_token();
        additive_expression();
    }
}

void additive_expression() {
    multiplicative_expression();
    while (token == TK_PLUS || token == TK_MINUS) {
        get_token();
        multiplicative_expression();
    }
}

void multiplicative_expression() {
    unary_expression();
    while (token == TK_STAR || token == TK_DIVIDE || token == TK_MOD) {
        get_token();
        unary_expression();
    }
}

void unary_expression() {
    switch (token) {
        case TK_AND:
        case TK_STAR:
        case TK_PLUS:
        case TK_MINUS:
            get_token();
            unary_expression();
            break;
        case KW_SIZEOF:
            sizeof_expression();
            break;
        default:
            postfix_expression();
            break;
    }
}

void sizeof_expression() {
    get_token();
    skip(TK_OPENPA);
    type_specifier();
    skip(TK_CLOSEPA);
}

void postfix_expression() {
    primary_expression();
    while(1){
        if (token == TK_DOT || token ==TK_POINTSTO){
            get_token();
            token |= SC_MEMBER;
            get_token();
        }else if (token == TK_OPENBR){
            get_token();
            expression();
            skip(TK_CLOSEBR);
        }else if (token == TK_OPENPA){
            argument_expression_list();
        }else{
            break;
        }
    }
}

void primary_expression(){
    int t;
    switch (token) {
        case TK_CINT:
        case TK_CCHAR:
        case TK_CSTR:
            get_token();
            break;
        case TK_OPENPA:
            get_token();
            expression();
            skip(TK_CLOSEPA);
        default:
            t = token;
            get_token();
            if (t<TK_IDENT){
                expect("标识符或常量");
            }
            break;
    }
}

void argument_expression_list(){
    get_token();
    if (token != TK_CLOSEPA){
        while(1){
            assignment_expression();
            if (token == TK_CLOSEPA){
                break;
            }
            skip(TK_COMMA);
        }
    }
    skip(TK_CLOSEPA);
}




int main(int argc, char **argv) {
    fin = fopen(argv[1], "rb");
    if (!fin) {
        printf("不能打开sc源文件!\n");
        return 0;
    }
    init();
    getch();
    get_token();
    translation_unit();

    cleanup();
    fclose(fin);
    printf("%s 语法分析成功！", argv[1]);
    return 0;
}



// 'A':case'B':case'C':case'D':case'E':case'F':case'G':case'H':case'I':case'J':case'K':case'L':case'M':case'N':case'O':case'P':case'Q':case'R':case'S':case'T':case'U':case'V':case'W':case'X':case'Y':case'Z'
// '0':case'1':case'2':case'3':case'4':case'5':case'6':case'7':case'8':case'9'













