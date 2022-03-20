package simple_syntactic

import (
	"errors"

	"github.com/songzhibin97/compiler/simple_go_compiler/front_end"
)

// expr = var Identifier int ( = (Identifier | Digit) )

func IntDeclareExec(t *front_end.TokenList) (*front_end.AstNode, error) {
	f := t.Point()
	var r *front_end.AstNode
	if t.Len() != 0 && f.GetType() == front_end.Keyword && f.GetContent() == "var" {
		r = front_end.CreateAstNode(f, front_end.IntDeclare)
		nr := r
		t.Next()
		if t.Offset() < t.Len() && t.Point().GetType() != front_end.Identifier {
			// 不匹配标识符
			return nil, errors.New("int declare fault, Identifier")
		}
		nr.Right = front_end.CreateAstNode(t.Point(), front_end.Child)
		nr = nr.Right
		t.Next()
		if t.Offset() < t.Len() && ((t.Point().GetType() == front_end.Keyword && t.Point().GetContent() == "int") || (t.Point().GetType() == front_end.Eq)) {
			nr.Right = front_end.CreateAstNode(t.Point(), front_end.Child)
			nr = nr.Right

			if t.Point().GetType() == front_end.Keyword {
				t.Next()
				if (t.Offset() < t.Len() && t.Point().GetType() != front_end.Eq) || (t.Offset() >= t.Len()) {
					// 不是等号退出
					return r, nil
				}
				nr.Right = front_end.CreateAstNode(t.Point(), front_end.Child)
				nr = nr.Right
				t.Next()
			} else {
				t.Next()
			}

			if t.Offset() < t.Len() && t.Point().GetType() != front_end.Digit {
				return nil, errors.New("int declare fault, not int value")
			} else if t.Offset() >= t.Len() {
				return nil, errors.New("int declare fault, offset >= len")
			}
			nr.Right = front_end.CreateAstNode(t.Point(), front_end.Child)
			nr = nr.Right
			t.Next()
			return r, nil
		} else if t.Offset() >= t.Len() {
			return nil, errors.New("int declare fault, offset >= len")
		} else {
			return nil, errors.New("int declare fault, keyword int")
		}
	} else {
		return nil, errors.New("int declare fault")
	}
}
