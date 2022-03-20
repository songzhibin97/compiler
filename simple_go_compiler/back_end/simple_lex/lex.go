package simple_lex

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
)

type Matching struct {
	s      string
	offset int
	ln     int
}

var keywordMp = map[string]bool{
	"Int": true,
	// todo 可以填充一些 keyword
}

func (m *Matching) IsDigit() bool {
	return unicode.IsDigit(rune(m.s[m.offset]))
}

func (m *Matching) IsLetter() bool {
	return unicode.IsLetter(rune(m.s[m.offset]))
}

func InitMatching(s string) *Matching {
	return &Matching{
		s:      s + " ",
		offset: 0,
		ln:     len(s) + 1,
	}
}

type Token struct {
	Type    Status
	Content string
}

func Parse(s *Matching) []Token {
	t := Init
	oldState := Init
	var p []byte
	var ret []Token
	for s.offset < s.ln {
		if t == Clear {
			ret = append(ret, Token{
				Type:    oldState,
				Content: string(p),
			})
			p = []byte{}
			t = Init
		}

		switch t {
		case Init:
			v := s.s[s.offset]
			if v == ' ' {
				s.offset++
				continue
			}
			switch {
			case s.IsLetter():
				t = Identifier
			case s.IsDigit():
				t = Digit
			case v == '>':
				t = Gt
			case v == '<':
				t = Lt
			case v == '=':
				t = Eq
			case v == '+' || v == '-' || v == '*' || v == '/':
				t = Op
			default:
				panic("意外字符:" + string(v))
			}
			p = append(p, v)
			s.offset++

		case Identifier:
			for s.offset < s.ln {
				v := s.s[s.offset]
				if s.IsDigit() || s.IsLetter() {
					p = append(p, v)
					s.offset++
				} else {
					break
				}
			}
			if keywordMp[string(p)] {
				oldState = Keyword
			} else {
				oldState = Identifier
			}
			t = Clear
		case Gt, Lt, Eq:
			v := s.s[s.offset]
			if v == '=' {
				p = append(p, v)
				s.offset++
				oldState = t + 1
			} else {
				oldState = t
			}
			t = Clear
		case Digit:
			for s.offset < s.ln {
				v := s.s[s.offset]
				if s.IsDigit() {
					p = append(p, v)
					s.offset++
				} else {
					break
				}
			}
			oldState = Digit
			t = Clear
		case Op:
			oldState = Op
			t = Clear
		}
	}
	return ret
}
