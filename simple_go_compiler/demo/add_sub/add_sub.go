// 简单的加减法编译器-输入正整数 demo版
// 使用方法 go run add_sub.go '1+2+3+4-10' > tmp.s 获取汇编代码
// cc -o tmp tmp.s 生成可执行程序
// ./tmp 运行可执行程序
// echo $? 输出结果
package main

import (
	"fmt"
	"os"
)

type TokenKind int

// token
const (
	TK_RESERVED TokenKind = iota // '+' '-'
	TK_NUM                       // 数字
	TK_EOF
)

type Token struct {
	kind TokenKind
	next *Token // token链
	val  int    // 如果是数字token 存它的值
	str  string // 当前及后面的字符
}

// 全局(当前)token
var (
	token *Token
)

func my_error(fmts string, a ...interface{}) {
	if len(a) == 0 {
		fmt.Printf(fmts, a)
		os.Exit(1)
	}
	fmt.Printf(fmts, a)
	os.Exit(1)
}

// 消费一个操作符
func consume(op byte) bool {
	if token.kind != TK_RESERVED || token.str[0] != op {
		return false
	}
	token = token.next
	return true
}

// 非操作符则异常
func expect(op byte) {
	if token.kind != TK_RESERVED || token.str[0] != op {
		my_error("op error:'%c'", op)
	}
	token = token.next
}

// 非数字则异常
func expectNumber() int {
	if token.kind != TK_NUM {
		my_error("not number token")
	}
	val := token.val
	token = token.next
	return val
}

func atEof() bool {
	return token.kind == TK_EOF
}

func newToken(kind TokenKind, cur *Token, str string) *Token {
	tok := new(Token)
	tok.kind = kind
	tok.str = str
	cur.next = tok
	return tok
}

// 字符串转数字
func s2i(p string, i *int) int {
	v := 0
	//  *i < len(p)是为了防止range out
	for ; *i < len(p) && p[*i] >= '0' && p[*i] <= '9'; *i++ {
		v = v*10 + (int(p[*i]) - 48)
	}
	// for循环会多加一
	*i--
	return v
}

// token序列化
func tokenize(p string) *Token {
	var head Token
	head.next = nil
	cur := &head
	i := 0
	for ; i < len(p); i++ {
		if p[i] == ' ' {
			continue
		}
		if p[i] == '+' || p[i] == '-' {
			cur = newToken(TK_RESERVED, cur, p[i:])
			continue
		}
		if p[i] >= '0' && p[i] <= '9' {
			cur = newToken(TK_NUM, cur, p[i:])
			cur.val = s2i(p, &i)
			continue
		}
		my_error("illegal char")
	}
	newToken(TK_EOF, cur, p[i:])
	return head.next
}

func main() {
	if len(os.Args) != 2 {
		fmt.Println("number of arguments err")
		os.Exit(1)
	}
	token = tokenize(os.Args[1])

	fmt.Printf(".intel_syntax noprefix\n")
	fmt.Printf(".globl _main\n")
	fmt.Printf("_main:\n")

	fmt.Printf("\tmov rax, %d\n", expectNumber())
	for !atEof() {
		if consume('+') {
			fmt.Printf("\tadd rax, %d\n", expectNumber())
			continue
		}
		expect('-')
		fmt.Printf("\tsub rax, %d\n", expectNumber())
	}
	fmt.Printf("\tret\n")
}
