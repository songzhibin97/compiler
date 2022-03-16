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
                    if (c>='!' && c<='~'){
                        warning("it's illegal change mean: \'\\%c\'",c);
                    }else{
                        warning("it's illegal change mean: \'\\0x%x\'",c);
                    }
                    break;
            }
            dynstring_chcat(&tkstr,c);
            dynstring_chcat(&sourcestr,ch);
            getch();
        }else{
            dynstring_chcat(&tkstr,ch);
            dynstring_chcat(&sourcestr,ch);
            getch();
        }
    }
    dynstring_chcat(&tkstr,'\0');
    dynstring_chcat(&sourcestr,sep);
    dynstring_chcat(&sourcestr,'\0');
    getch();
}

void init(){
    line_num = 1;
    init_lex();
}

void cleanup(){
    int i;
    printf("\n tktable.count=%d\n",tktable.count);
    for (i = TK_IDENT; i<tktable.count ; ++i) {
        free(tktable.data[i]);
    }
    free(tktable.data);
}

int main(int argc,char **argv) {
    fin = fopen(argv[1],"rb");
    if (!fin){
        printf("不能打开sc源文件!\n");
        return 0;
    }
    init();
    getch();
    do {
        get_token();
    }while(token!=TK_EOF);
    printf("lines of code: %d line\n",line_num);

    cleanup();
    fclose(fin);
    printf("%s 词法分析成功！",argv[1]);
    return 0;
}



// 'A':case'B':case'C':case'D':case'E':case'F':case'G':case'H':case'I':case'J':case'K':case'L':case'M':case'N':case'O':case'P':case'Q':case'R':case'S':case'T':case'U':case'V':case'W':case'X':case'Y':case'Z'
// '0':case'1':case'2':case'3':case'4':case'5':case'6':case'7':case'8':case'9'













