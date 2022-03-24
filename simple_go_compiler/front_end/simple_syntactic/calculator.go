package simple_syntactic

import "github.com/songzhibin97/compiler/simple_go_compiler/front_end"

// Calculator
// E = E +|- E
// E = E *|/ E
// E = (E)
// E = num
// 1 + 2 * 3

func ParseCalculator(t *front_end.TokenList) *front_end.AstNode {
	return add(t)
}

func add(t *front_end.TokenList) *front_end.AstNode {
	node := mul(t)
	if t.Offset() >= t.Len() {
		return node
	}

	switch t.Point().GetContent() {
	case "+":
		nNode := new(front_end.AstNode)
		nNode.Type = front_end.ADD
		nNode.Left = node
		t.Next()
		node.Right = mul(t)
		return nNode
	case "-":
		nNode := new(front_end.AstNode)
		nNode.Type = front_end.SUB
		nNode.Left = node
		t.Next()
		node.Right = mul(t)
		return nNode
	}
	return node
}

func bracket(t *front_end.TokenList) *front_end.AstNode {
	if t.Offset() >= t.Len() {
		return nil
	}
	if t.Point().GetType() == front_end.LeftParenthesis {
		t.Next()
		node := add(t)
		if t.Offset() >= t.Len() || t.Point().GetType() != front_end.RightParenthesis {
			panic("expire )")
		}
		t.Next()
		return node
	}
	return digit(t)
}

func digit(t *front_end.TokenList) *front_end.AstNode {
	if t.Offset() >= t.Len() || t.Point().GetType() != front_end.Digit {
		panic("expire digit")
	}
	r := &front_end.AstNode{
		Left:  nil,
		Right: nil,
		Value: t.Point(),
		Type:  front_end.Base,
	}
	t.Next()
	return r
}

func mul(t *front_end.TokenList) *front_end.AstNode {
	node := bracket(t)
	if t.Offset() >= t.Len() {
		return node
	}

	if t.Point().GetType() != front_end.Op {
		panic("expire op")
	}
	switch t.Point().GetContent() {
	case "*":
		nNode := new(front_end.AstNode)
		nNode.Type = front_end.MUL
		nNode.Left = node
		t.Next()
		node.Right = bracket(t)
		return nNode
	case "/":
		nNode := new(front_end.AstNode)
		nNode.Type = front_end.QUO
		nNode.Left = node
		t.Next()
		node.Right = bracket(t)
		return nNode
	}
	return node
}
