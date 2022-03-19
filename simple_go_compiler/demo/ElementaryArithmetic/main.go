// 简单的四则运算编译器-输入正整数 demo版
// 使用方法 go run main.go '1+(2+3)*4-10/5' > tmp.s 获取汇编代码
// cc -o tmp tmp.s 生成可执行程序
// ./tmp 运行可执行程序
// echo $? 输出结果
package main

import (
	"fmt"
	"os"
	"strings"
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

type NodeKind int

const (
	ND_ADD NodeKind = iota
	ND_SUB
	ND_MUL
	ND_DIV
	ND_NUM
)

// AST抽象语法树节点
type Node struct {
	kind     NodeKind
	lhs, rhs *Node
	val      int
}

// 全局(当前)
var (
	token     *Token
	userInput string
)

func my_error(fmts string, a ...interface{}) {
	if len(a) == 0 {
		fmt.Printf(fmts, a)
		os.Exit(1)
	}
	fmt.Printf(fmts, a)
	os.Exit(1)
}

// 带位置的错误处理
func err_at(strP int, fmts string, a ...interface{}) {
	fmt.Printf("%s\n", userInput)
	fmt.Printf("%s ^ ", userInput[:strP])
	if len(a) == 0 {
		fmt.Printf(fmts, a)
		os.Exit(1)
	}
	fmt.Printf(fmts+"\n", a)
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

func newNode(kind NodeKind) *Node {
	return &Node{kind: kind}
}

func newBinary(kind NodeKind, lhs, rhs *Node) *Node {
	node := newNode(kind)
	node.lhs = lhs
	node.rhs = rhs
	return node
}

func newNum(val int) *Node {
	node := newNode(ND_NUM)
	node.val = val
	return node
}

func expr() *Node {
	node := mul()
	for {
		if consume('+') {
			node = newBinary(ND_ADD, node, mul())
		} else if consume('-') {
			node = newBinary(ND_SUB, node, mul())
		} else {
			return node
		}
	}
}

func mul() *Node {
	node := primary()
	for {
		if consume('*') {
			node = newBinary(ND_MUL, node, primary())
		} else if consume('/') {
			node = newBinary(ND_DIV, node, primary())
		} else {
			return node
		}
	}
}

func primary() *Node {
	if consume('(') {
		node := expr()
		expect(')')
		return node
	}
	return newNum(expectNumber())
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

func gen(node *Node) {
	if node.kind == ND_NUM {
		fmt.Printf("\tpush %d\n", node.val)
		return
	}
	gen(node.lhs)
	gen(node.rhs)

	fmt.Printf("\tpop rdi\n")
	fmt.Printf("\tpop rax\n")

	switch node.kind {
	case ND_ADD:
		fmt.Printf("\tadd rax, rdi\n")
		break
	case ND_SUB:
		fmt.Printf("\tsub rax, rdi\n")
		break
	case ND_MUL:
		fmt.Printf("\timul rax, rdi\n")
		break
	case ND_DIV:
		// 数值操作的汇编可以看: https://www.shuzhiduo.com/A/1O5EPwq4J7/
		fmt.Printf("\tcqo\n") // rax -> rdx:rax
		fmt.Printf("\tidiv rdi\n")
		break
	default:
		break
	}
	fmt.Printf("\tpush rax\n")
}

// token序列化
func tokenize() *Token {
	p := userInput
	var head Token
	head.next = nil
	cur := &head
	i := 0
	for ; i < len(p); i++ {
		if p[i] == ' ' {
			continue
		}
		if strings.ContainsRune("+-*/()", rune(p[i])) {
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
	userInput = os.Args[1]
	token = tokenize()
	node := expr()

	fmt.Printf(".intel_syntax noprefix\n")
	fmt.Printf(".globl _main\n")
	fmt.Printf("_main:\n")

	gen(node)

	fmt.Printf("\tpop rax\n")
	fmt.Printf("\tret\n")
}

// expr = mul ('+' mul|'-' mul)*
// mul = primary ('*' primary | '/' primary)*
// primary = num | '('expr')'
// 1*(2+3)     expr
//              mul
//   primary     *      primary
//     num           (    expr    )
//     1             mul   +     mul
//                   primary    primary
//                   num        num
//                    2          3
