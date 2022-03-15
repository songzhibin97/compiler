#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    dynArray_init(&tktable,50);
    for (tp = &keywords[0];tp->spelling !=NULL;tp++) {
        tkWord_direct_insert(tp);
    }
}

int main() {
    init_lex();

    return 0;
}


















