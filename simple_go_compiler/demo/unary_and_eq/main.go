// 简单的(四则运算+判读大小)编译器-输入正整数 demo版
// 在四则运算的基础上增加了 一元符(+ -)和二元符(== != > < >= <=)
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

// 全局(当前)
var (
	token     *Token
	userInput string
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
	len  int    // 符号长度
}

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
func consume(op string) bool {
	if token.kind != TK_RESERVED ||
		len(op) != token.len ||
		token.str[:len(op)] != op {
		return false
	}
	token = token.next
	return true
}

// 非操作符则异常
func expect(op string) {
	if token.kind != TK_RESERVED ||
		len(op) == token.len ||
		token.str[:len(op)] != op {
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

func newToken(kind TokenKind, cur *Token, str string, len int) *Token {
	tok := new(Token)
	tok.kind = kind
	tok.str = str
	tok.len = len
	cur.next = tok
	return tok
}

type NodeKind int

const (
	ND_ADD NodeKind = iota // +
	ND_SUB                 // -
	ND_MUL                 // *
	ND_DIV                 // /
	ND_EQ                  // ==
	ND_NE                  // !=
	ND_LT                  // <
	ND_LE                  // <=
	ND_NUM                 // 整数
)

// AST抽象语法树节点
type Node struct {
	kind     NodeKind
	lhs, rhs *Node
	val      int
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

// expr       = equality
// equality   = relational ("==" relational | "!=" relational)*
// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
// add        = mul ("+" mul | "-" mul)*
// mul        = unary ("*" unary | "/" unary)*
// unary      = ("+" | "-")? unary | primary
// primary    = num | "(" expr ")"
func expr() *Node {
	return equality()
}

func equality() *Node {
	node := relational()
	for {
		if consume("==") {
			node = newBinary(ND_EQ, node, relational())
		} else if consume("!=") {
			node = newBinary(ND_NE, node, relational())
		} else {
			return node
		}
	}
}

func relational() *Node {
	node := add()
	for {
		if consume("<") {
			node = newBinary(ND_LT, node, add())
		} else if consume("<=") {
			node = newBinary(ND_LE, node, add())
		} else if consume(">") {
			node = newBinary(ND_LT, add(), node)
		} else if consume(">=") {
			node = newBinary(ND_LE, add(), node)
		} else {
			return node
		}
	}
}

func add() *Node {
	node := mul()
	for {
		if consume("+") {
			node = newBinary(ND_ADD, node, mul())
		} else if consume("-") {
			node = newBinary(ND_SUB, node, mul())
		} else {
			return node
		}
	}
}

func mul() *Node {
	node := unary()
	for {
		if consume("*") {
			node = newBinary(ND_MUL, node, unary())
		} else if consume("/") {
			node = newBinary(ND_DIV, node, unary())
		} else {
			return node
		}
	}
}

func unary() *Node {
	if consume("-") {
		return newBinary(ND_SUB, newNum(0), unary())
	}
	if consume("+") {
		return unary()
	}
	return primary()
}

func primary() *Node {
	if consume("(") {
		node := expr()
		expect(")")
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
	return v
}

// 判断p的头部是否有q
func startsWith(p, q string) bool {
	if len(p) < len(q) {
		return false
	}
	return p[:len(q)] == q
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
	case ND_EQ:
		fmt.Printf("\tcmp rax, rdi\n")
		fmt.Printf("\tsete al\n")
		fmt.Printf("\tmovzx rax, al")
		break
	case ND_NE:
		fmt.Printf("\tcmp rax, rdi\n")
		fmt.Printf("\tsetne al\n")
		fmt.Printf("\tmovzx rax, al")
		break
	case ND_LT:
		fmt.Printf("\tcmp rax, rdi\n")
		fmt.Printf("\tsetl al\n")
		fmt.Printf("\tmovzx rax, al")
		break
	case ND_LE:
		fmt.Printf("\tcmp rax, rdi\n")
		fmt.Printf("\tsetle al\n")
		fmt.Printf("\tmovzx rax, al")
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
	for i < len(p) {
		if p[i] == ' ' {
			i++
			continue
		}
		if startsWith(p[i:], "==") || startsWith(p[i:], "!=") ||
			startsWith(p[i:], "<=") || startsWith(p[i:], ">=") {
			cur = newToken(TK_RESERVED, cur, p[i:], 2)
			i += 2
			continue
		}
		if strings.ContainsRune("+-*/()<>", rune(p[i])) {
			cur = newToken(TK_RESERVED, cur, p[i:], 1)
			i++
			continue
		}
		if p[i] >= '0' && p[i] <= '9' {
			cur = newToken(TK_NUM, cur, p[i:], 0)
			j := i
			cur.val = s2i(p, &i)
			cur.len = i - j
			continue
		}
		my_error("illegal char")
	}
	newToken(TK_EOF, cur, p[i:], 0)
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
