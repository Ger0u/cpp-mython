#include "lexer.h"

#include <cassert>
#include <algorithm>
#include <charconv>
#include <unordered_map>

using namespace std;

namespace parse {

bool operator==(const Token& lhs, const Token& rhs) {
    using namespace token_type;

    if (lhs.index() != rhs.index()) {
        return false;
    }
    if (lhs.Is<Char>()) {
        return lhs.As<Char>().value == rhs.As<Char>().value;
    }
    if (lhs.Is<Number>()) {
        return lhs.As<Number>().value == rhs.As<Number>().value;
    }
    if (lhs.Is<String>()) {
        return lhs.As<String>().value == rhs.As<String>().value;
    }
    if (lhs.Is<Id>()) {
        return lhs.As<Id>().value == rhs.As<Id>().value;
    }
    return true;
}

bool operator!=(const Token& lhs, const Token& rhs) {
    return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, const Token& rhs) {
    using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

    VALUED_OUTPUT(Number);
    VALUED_OUTPUT(Id);
    VALUED_OUTPUT(String);
    VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

    UNVALUED_OUTPUT(Class);
    UNVALUED_OUTPUT(Return);
    UNVALUED_OUTPUT(If);
    UNVALUED_OUTPUT(Else);
    UNVALUED_OUTPUT(Def);
    UNVALUED_OUTPUT(Newline);
    UNVALUED_OUTPUT(Print);
    UNVALUED_OUTPUT(Indent);
    UNVALUED_OUTPUT(Dedent);
    UNVALUED_OUTPUT(And);
    UNVALUED_OUTPUT(Or);
    UNVALUED_OUTPUT(Not);
    UNVALUED_OUTPUT(Eq);
    UNVALUED_OUTPUT(NotEq);
    UNVALUED_OUTPUT(LessOrEq);
    UNVALUED_OUTPUT(GreaterOrEq);
    UNVALUED_OUTPUT(None);
    UNVALUED_OUTPUT(True);
    UNVALUED_OUTPUT(False);
    UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

    return os << "Unknown token :("sv;
}

Lexer::Lexer(std::istream& input)
: input_(input) {
    NextToken();
}

const Token& Lexer::CurrentToken() const {
    return token_;
}
    
int GetNumber(istream& input) {
    int result;
    input >> result;
    return result;
}
    
string GetStr(istream& input) {
    string result;
    
    char k = input.get();
    char pred_c = ' ';
    for (char c = input.get(); c != k || pred_c == '\\'; c = input.get()) {
        if (pred_c == '\\') {
            switch (c) {
                case '\'':
                case '"':
                    result += c;
                    break;
                case 'n':
                    result += '\n';
                    break;
                case 't':
                    result += '\t';
                    break;
                default:
                    result += '\\';
            }
        } else if (c != '\\') {
            result += c;
        }
        pred_c = c;
    }
    if (pred_c == '\\') {
        result += '\\';
    }

    return result;
}
    
string GetId(istream& input) {
    string result{(char)input.get()};
    for (char c = input.get();
         (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
         c = input.get())
    {
        result += c;
    }
    input.unget();
    return result;
}

Token Lexer::NextToken() {
    token_ = [this]() -> Token {
        if (token_.Is<token_type::Eof>()) {
            return token_;
        } else if (dedent_ > 0) {
            --dedent_;
            return token_type::Dedent{};
        }
        char c = input_.get();
        if (token_.Is<token_type::Newline>()) {
            size_t i = 0;
            for (; c == ' '; c = input_.get()) {
                assert(input_.get() == ' ');
                ++i;
            }
            while (c == '\n' || c == '#') {
                for (; c == '\n' || c == '#'; c = input_.get()) {
                    if (c == '#') {
                        string line;
                        getline(input_, line);
                    }
                }
                i = 0;
                for (; c == ' '; c = input_.get()) {
                    assert(input_.get() == ' ');
                    ++i;
                }
            }
            if (i > indent_) {
                input_.unget();
                ++indent_;
                return token_type::Indent{};
            } else if (i < indent_) {
                input_.unget();
                dedent_ = indent_ - i - 1;
                indent_ = i;
                return token_type::Dedent{};
            }
        } else {
            for (; c == ' '; c = input_.get()) {}
        }
        if (c == char_traits<char>::eof()) {
            if (!token_.Is<token_type::Newline>() && !token_.Is<token_type::Dedent>()) {
                dedent_ = indent_;
                indent_ = 0;
                return token_type::Newline();
            }
            return token_type::Eof();
        } else if (c >= '0' && c <= '9') {
            input_.unget();
            return token_type::Number{GetNumber(input_)};
        } else if (c == '\'' || c == '"') {
            input_.unget();
            return token_type::String{GetStr(input_)};
        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
            input_.unget();
            string str = GetId(input_);
            if (str == "class"sv) {
                return token_type::Class{};
            } else if (str == "return"sv) {
                return token_type::Return{};
            } else if (str == "if"sv) {
                return token_type::If{};
            } else if (str == "else"sv) {
                return token_type::Else{};
            } else if (str == "def"sv) {
                return token_type::Def{};
            } else if (str == "print"sv) {
                return token_type::Print{};
            } else if (str == "and"sv) {
                return token_type::And{};
            } else if (str == "or"sv) {
                return token_type::Or{};
            } else if (str == "not"sv) {
                return token_type::Not{};
            } else if (str == "None"sv) {
                return token_type::None{};
            } else if (str == "True"sv) {
                return token_type::True{};
            } else if (str == "False"sv) {
                return token_type::False{};
            } else {
                return token_type::Id{move(str)};
            }
        } else if (c == '\n') {
            return token_type::Newline{};
        } else if (c == '#') {
            string line;
            getline(input_, line);
            return token_type::Newline{};
        } else if (c == '=' && input_.peek() == '=') {
            input_.get();
            return token_type::Eq{};
        } else if (c == '!' && input_.peek() == '=') {
            assert(input_.get() == '=');
            return token_type::NotEq{};
        } else if (c == '<' && input_.peek() == '=') {
            assert(input_.get() == '=');
            return token_type::LessOrEq{};
        } else if (c == '>' && input_.peek() == '=') {
            assert(input_.get() == '=');
            return token_type::GreaterOrEq{};
        } else if (c == '=' || c == '.' || c == ',' || c == '(' || c == '+' || c == '<' || c == '>' || c == '-' || c == ')' || c == '*' || c == '/' || c == ':') {
            return token_type::Char{c};
        } else {
            assert(false);
        }
    }();
    
    return token_;
}

}  // namespace parse