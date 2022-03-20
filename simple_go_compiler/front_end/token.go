package front_end

import (
	"unicode"
)

type Status int

const (
	Init Status = iota
	Keyword
	Identifier
	Gt
	Gte
	Lt
	Lte
	Eq
	Eqe
	Digit
	Op
	Clear
	LeftParenthesis
	RightParenthesis
)

type AstType int

const (
	IntDeclare AstType = iota
	Child
)

type Matching struct {
	source string
	offset int
	ln     int
}

var KeywordMp = map[string]bool{
	"int": true,
	"var": true,
	// todo 可以填充一些 keyword
}

func (m *Matching) Offset() int {
	return m.offset
}

func (m *Matching) Point() byte {
	return m.source[m.offset]
}

func (m *Matching) Len() int {
	return m.ln
}

func (m *Matching) Next() {
	m.offset++
}

func (m *Matching) IsDigit() bool {
	return unicode.IsDigit(rune(m.source[m.offset]))
}

func (m *Matching) IsLetter() bool {
	return unicode.IsLetter(rune(m.source[m.offset]))
}

func InitMatching(s string) *Matching {
	return &Matching{
		source: s + " ",
		offset: 0,
		ln:     len(s) + 1,
	}
}

type Token struct {
	Type    Status
	Content string
}

func (t Token) GetType() Status {
	return t.Type
}

func (t Token) GetContent() string {
	return t.Content
}

type TokenList struct {
	list   []Token
	ln     int
	offset int
}

func (m *TokenList) Offset() int {
	return m.offset
}

func (m *TokenList) Point() Token {
	return m.list[m.offset]
}

func (m *TokenList) Len() int {
	return m.ln
}

func (m *TokenList) Next() {
	m.offset++
}

func InitTokenList(list []Token) *TokenList {
	return &TokenList{
		list:   list,
		ln:     len(list),
		offset: 0,
	}
}

type AstNode struct {
	Left  *AstNode
	Right *AstNode
	Value Token
	Type  AstType
}

func CreateAstNode(t Token, ty AstType) *AstNode {
	return &AstNode{Value: t, Type: ty}
}
