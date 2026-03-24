// paani_lexer.hpp - Paani Lexer
// Clean lexer with good error messages

#ifndef PAANI_LEXER_HPP
#define PAANI_LEXER_HPP

#include "paani_ast.hpp"
#include <string>
#include <string_view>
#include <unordered_map>
#include <stdexcept>

namespace paani {

// ============================================================================
// Token Types
// ============================================================================
enum class TokenType : uint8_t {
    End,          // End of file

    // Keywords
    Data,         // data
    Trait,        // trait
    Template,     // template
    Spawn,        // spawn
    Give,         // give
    Tag,          // tag
    Untag,        // untag
    Destroy,      // destroy
    Has,          // has
    Use,          // use
    Extern,       // extern
    Handle,       // handle
    Type,         // type
    As,           // as
    With,         // with
    Query,        // query
    Lock,         // lock
    Unlock,       // unlock
    System,       // system
    Depend,       // depend
    Export,       // export
    Fn,           // fn
    Let,          // let
    Var,          // var
    If,           // if
    Else,         // else
    While,        // while
    For,          // for (Trait System only)
    In,           // in (Trait System only)
    Return,       // return
    Break,        // break
    Continue,     // continue
    Exit,         // exit
    True,         // true
    False,        // false

    // Types
    Void,         // void
    Bool,         // bool
    I8, I16, I32, I64,
    U8, U16, U32, U64,
    F32, F64,
    String,       // string
    Entity,       // entity

    // Literals
    Identifier,
    IntegerLiteral,
    FloatLiteral,
    StringLiteral,
    FString,       // f"..." (template string)

    // Operators
    Assign,       // =
    AddAssign,    // +=
    SubAssign,    // -=
    Eq,           // ==
    Ne,           // !=
    Lt,           // <
    Le,           // <=
    Gt,           // >
    Ge,           // >=
    And,          // &&
    Or,           // ||
    Not,          // !
    NotHas,       // !has
    Add,          // +
    Sub,          // -
    Mul,          // *
    Div,          // /
    Mod,          // %
    Arrow,        // ->
    // Bitwise operators
    BitAnd,       // &
    BitOr,        // |
    BitXor,       // ^
    BitShl,       // <<
    BitShr,       // >>
    Tilde,        // ~

    // Delimiters
    LParen,       // (
    RParen,       // )
    LBrace,       // {
    RBrace,       // }
    LBracket,     // [
    RBracket,     // ]
    Semicolon,    // ;
    Colon,        // :
    Comma,        // ,
    Dot,          // .
    Hash,         // # (for attributes like #batch)
};

// ============================================================================
// Token
// ============================================================================
struct Token {
    TokenType type;
    std::string_view text;  // View into source (no copy)
    Location loc;

    Token(TokenType t = TokenType::End, std::string_view txt = "",
          Location l = Location{})
        : type(t), text(txt), loc(l) {}

    bool is(TokenType t) const { return type == t; }
    bool isKeyword() const;
    bool isType() const;
    bool isLiteral() const;
    bool isOperator() const;
};

// ============================================================================
// Lexer Error
// ============================================================================
struct LexerError : public std::runtime_error {
    Location loc;

    LexerError(Location l, const std::string& msg)
        : std::runtime_error("Lexer error at " + l.toString() + ": " + msg),
          loc(l) {}
};

// ============================================================================
// Lexer
// ============================================================================
class Lexer {
public:
    explicit Lexer(std::string_view source);

    // Get current token without consuming
    const Token& current() const { return current_; }

    // Get next token and advance
    Token next();

    // Check if at end of file
    bool isAtEnd() const { return current_.type == TokenType::End; }

    // Expect a specific token type, consume it and return true if matches
    bool expect(TokenType type);

    // Get current location
    Location location() const { return current_.loc; }

private:
    std::string_view source_;
    size_t pos_;
    Location loc_;
    Token current_;

    // Keyword lookup table
    static const std::unordered_map<std::string_view, TokenType> keywords;

    // Character operations
    char peek(size_t offset = 0) const;
    char advance();
    bool match(char expected);

    // Skip whitespace and comments
    void skipWhitespace();
    void skipComment();

    // Token parsing
    Token parseIdentifier();
    Token parseNumber();
    Token parseString();
    Token parseFString();
    Token parseOperator();
};

// ============================================================================
// Implementation
// ============================================================================
inline bool Token::isKeyword() const {
    return type >= TokenType::Data && type <= TokenType::False;
}

inline bool Token::isType() const {
    return type >= TokenType::Void && type <= TokenType::Entity;
}

inline bool Token::isLiteral() const {
    return type >= TokenType::Identifier && type <= TokenType::StringLiteral;
}

inline bool Token::isOperator() const {
    return (type >= TokenType::Assign && type <= TokenType::Mod) ||
           (type >= TokenType::BitAnd && type <= TokenType::BitShr);
}

// Keyword table
inline const std::unordered_map<std::string_view, TokenType> Lexer::keywords = {
    // Keywords
    {"data", TokenType::Data},
    {"trait", TokenType::Trait},
    {"template", TokenType::Template},
    {"spawn", TokenType::Spawn},
    {"give", TokenType::Give},
    {"tag", TokenType::Tag},
    {"untag", TokenType::Untag},
    {"destroy", TokenType::Destroy},
    {"has", TokenType::Has},
    {"use", TokenType::Use},
    {"extern", TokenType::Extern},
    {"handle", TokenType::Handle},
{"type", TokenType::Type},
    {"as", TokenType::As},
    {"with", TokenType::With},
    {"query", TokenType::Query},
    {"lock", TokenType::Lock},
    {"unlock", TokenType::Unlock},
    {"system", TokenType::System},
    {"depend", TokenType::Depend},
    {"export", TokenType::Export},
    {"fn", TokenType::Fn},
    {"let", TokenType::Let},
    {"var", TokenType::Var},
    {"if", TokenType::If},
    {"else", TokenType::Else},
    {"while", TokenType::While},
    {"for", TokenType::For},
    {"in", TokenType::In},
    {"return", TokenType::Return},
    {"break", TokenType::Break},
    {"continue", TokenType::Continue},
    {"exit", TokenType::Exit},
    {"true", TokenType::True},
    {"false", TokenType::False},

    // Types
    {"void", TokenType::Void},
    {"bool", TokenType::Bool},
    {"i8", TokenType::I8},
    {"i16", TokenType::I16},
    {"i32", TokenType::I32},
    {"i64", TokenType::I64},
    {"u8", TokenType::U8},
    {"u16", TokenType::U16},
    {"u32", TokenType::U32},
    {"u64", TokenType::U64},
    {"f32", TokenType::F32},
    {"f64", TokenType::F64},
    {"string", TokenType::String},
    {"entity", TokenType::Entity},
};

inline Lexer::Lexer(std::string_view source)
    : source_(source), pos_(0), loc_(1, 1, 0) {
    next(); // Prime the pump
}

inline char Lexer::peek(size_t offset) const {
    if (pos_ + offset < source_.size()) {
        return source_[pos_ + offset];
    }
    return '\0';
}

inline char Lexer::advance() {
    char c = peek();
    if (c == '\n') {
        loc_.line++;
        loc_.column = 1;
    } else {
        loc_.column++;
    }
    loc_.offset = pos_;
    pos_++;
    return c;
}

inline bool Lexer::match(char expected) {
    if (peek() == expected) {
        advance();
        return true;
    }
    return false;
}

inline void Lexer::skipWhitespace() {
    while (true) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '/' && peek(1) == '/') {
            skipComment();
        } else {
            break;
        }
    }
}

inline void Lexer::skipComment() {
    // Skip // comment
    while (peek() != '\n' && peek() != '\0') {
        advance();
    }
}

inline Token Lexer::parseIdentifier() {
    Location startLoc = loc_;
    size_t start = pos_;

    while (std::isalnum(peek()) || peek() == '_') {
        advance();
    }

    std::string_view text = source_.substr(start, pos_ - start);

    // Check if keyword
    auto it = keywords.find(text);
    if (it != keywords.end()) {
        return Token(it->second, text, startLoc);
    }

    return Token(TokenType::Identifier, text, startLoc);
}

inline Token Lexer::parseNumber() {
    Location startLoc = loc_;
    size_t start = pos_;
    bool isFloat = false;

    // Integer part
    while (std::isdigit(peek())) {
        advance();
    }

    // Decimal part
    if (peek() == '.' && std::isdigit(peek(1))) {
        isFloat = true;
        advance(); // .
        while (std::isdigit(peek())) {
            advance();
        }
    }

    std::string_view text = source_.substr(start, pos_ - start);
    return Token(isFloat ? TokenType::FloatLiteral : TokenType::IntegerLiteral,
                 text, startLoc);
}

inline Token Lexer::parseString() {
    Location startLoc = loc_;
    size_t start = pos_;

    char quote = advance(); // opening " or '

    while (peek() != quote && peek() != '\0') {
        if (peek() == '\\') {
            advance(); // skip \
            if (peek() != '\0') advance(); // skip escaped char
        } else {
            advance();
        }
    }

    if (peek() == quote) {
        advance(); // closing quote
    } else {
        throw LexerError(loc_, "Unterminated string literal");
    }

    std::string_view text = source_.substr(start, pos_ - start);
    return Token(TokenType::StringLiteral, text, startLoc);
}

inline Token Lexer::parseFString() {
    Location startLoc = loc_;
    size_t start = pos_;

    // Consume 'f'
    advance();
    // Now at '"'
    advance(); // opening "

    while (peek() != '"' && peek() != '\0') {
        if (peek() == '\\') {
            advance(); // skip \
            if (peek() != '\0') advance(); // skip escaped char
        } else if (peek() == '{') {
            // Check for '{{' (escaped brace)
            if (peek(1) == '{') {
                advance(); // skip first {
                advance(); // skip second {
            } else {
                // Start of interpolation - but we include it in the token
                // Parser will handle the actual parsing
                advance();
            }
        } else if (peek() == '}') {
            // Check for '}}' (escaped brace)
            if (peek(1) == '}') {
                advance(); // skip first }
                advance(); // skip second }
            } else {
                advance();
            }
        } else {
            advance();
        }
    }

    if (peek() == '"') {
        advance(); // closing quote
    } else {
        throw LexerError(loc_, "Unterminated template string");
    }

    std::string_view text = source_.substr(start, pos_ - start);
    return Token(TokenType::FString, text, startLoc);
}

inline Token Lexer::parseOperator() {
    Location startLoc = loc_;
    char c = advance();

    switch (c) {
        case '=':
            if (match('=')) return Token(TokenType::Eq, "==", startLoc);
            return Token(TokenType::Assign, "=", startLoc);
        case '!':
            if (match('=')) return Token(TokenType::Ne, "!=", startLoc);
            return Token(TokenType::Not, "!", startLoc);
        case '<':
            if (match('=')) return Token(TokenType::Le, "<=", startLoc);
            if (match('<')) return Token(TokenType::BitShl, "<<", startLoc);
            return Token(TokenType::Lt, "<", startLoc);
        case '>':
            if (match('=')) return Token(TokenType::Ge, ">=", startLoc);
            if (match('>')) return Token(TokenType::BitShr, ">>", startLoc);
            return Token(TokenType::Gt, ">", startLoc);
        case '&':
            if (match('&')) return Token(TokenType::And, "&&", startLoc);
            return Token(TokenType::BitAnd, "&", startLoc);
        case '|':
            if (match('|')) return Token(TokenType::Or, "||", startLoc);
            return Token(TokenType::BitOr, "|", startLoc);
                    case '^':
                        return Token(TokenType::BitXor, "^", startLoc);
                    case '~':
                        return Token(TokenType::Tilde, "~", startLoc);        case '+':
            if (match('=')) return Token(TokenType::AddAssign, "+=", startLoc);
            return Token(TokenType::Add, "+", startLoc);
        case '-':
            if (match('>')) return Token(TokenType::Arrow, "->", startLoc);
            if (match('=')) return Token(TokenType::SubAssign, "-=", startLoc);
            return Token(TokenType::Sub, "-", startLoc);
        case '*':
            return Token(TokenType::Mul, "*", startLoc);
        case '/':
            return Token(TokenType::Div, "/", startLoc);
        case '%':
            return Token(TokenType::Mod, "%", startLoc);
        case '(':
            return Token(TokenType::LParen, "(", startLoc);
        case ')':
            return Token(TokenType::RParen, ")", startLoc);
        case '{':
            return Token(TokenType::LBrace, "{", startLoc);
        case '}':
            return Token(TokenType::RBrace, "}", startLoc);
        case '[':
            return Token(TokenType::LBracket, "[", startLoc);
        case ']':
            return Token(TokenType::RBracket, "]", startLoc);
        case ';':
            return Token(TokenType::Semicolon, ";", startLoc);
        case ':':
            return Token(TokenType::Colon, ":", startLoc);
        case ',':
            return Token(TokenType::Comma, ",", startLoc);
        case '.':
            return Token(TokenType::Dot, ".", startLoc);
        case '#':
            return Token(TokenType::Hash, "#", startLoc);
        default:
            throw LexerError(startLoc, std::string("Unexpected character: '") + c + "'");
    }
}

inline Token Lexer::next() {
    skipWhitespace();

    Token tok = current_;

    if (pos_ >= source_.size()) {
        current_ = Token(TokenType::End, "", loc_);
        return tok;
    }

    char c = peek();

    // Template string (f"...") - must check before identifier
    if (c == 'f' && peek(1) == '"') {
        current_ = parseFString();
        return tok;
    }

    // Identifier or keyword
    if (std::isalpha(c) || c == '_') {
        current_ = parseIdentifier();
        return tok;
    }

    // Number
    if (std::isdigit(c)) {
        current_ = parseNumber();
        return tok;
    }
    if (c == '"' || c == '\'') {
        current_ = parseString();
        return tok;
    }

    // Operator or delimiter
    current_ = parseOperator();
    return tok;
}

inline bool Lexer::expect(TokenType type) {
    if (current_.type == type) {
        next();
        return true;
    }
    return false;
}

} // namespace paani

#endif // PAANI_LEXER_HPP