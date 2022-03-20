package simple_lexical

import "github.com/songzhibin97/compiler/simple_go_compiler/front_end"

func Parse(s *front_end.Matching) []front_end.Token {
	t := front_end.Init
	oldState := front_end.Init
	var p []byte
	var ret []front_end.Token
	for s.Offset() < s.Len() {
		if t == front_end.Clear {
			ret = append(ret, front_end.Token{
				Type:    oldState,
				Content: string(p),
			})
			p = []byte{}
			t = front_end.Init
		}

		switch t {
		case front_end.Init:
			v := s.Point()
			if v == ' ' {
				s.Next()
				continue
			}
			switch {
			case s.IsLetter():
				t = front_end.Identifier
			case s.IsDigit():
				t = front_end.Digit
			case v == '>':
				t = front_end.Gt
			case v == '<':
				t = front_end.Lt
			case v == '=':
				t = front_end.Eq
			case v == '+' || v == '-' || v == '*' || v == '/':
				t = front_end.Op
			default:
				panic("意外字符:" + string(v))
			}
			p = append(p, v)
			s.Next()

		case front_end.Identifier:
			for s.Offset() < s.Len() {
				v := s.Point()
				if s.IsDigit() || s.IsLetter() {
					p = append(p, v)
					s.Next()
				} else {
					break
				}
			}
			if front_end.KeywordMp[string(p)] {
				oldState = front_end.Keyword
			} else {
				oldState = front_end.Identifier
			}
			t = front_end.Clear
		case front_end.Gt, front_end.Lt, front_end.Eq:
			v := s.Point()
			if v == '=' {
				p = append(p, v)
				s.Next()
				oldState = t + 1
			} else {
				oldState = t
			}
			t = front_end.Clear
		case front_end.Digit:
			for s.Offset() < s.Len() {
				v := s.Point()
				if s.IsDigit() {
					p = append(p, v)
					s.Next()
				} else {
					break
				}
			}
			oldState = front_end.Digit
			t = front_end.Clear
		case front_end.Op:
			oldState = front_end.Op
			t = front_end.Clear
		}
	}
	return ret
}
