#include "tools/ExpressionParser.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kE = 2.71828182845904523536;
}  // namespace

ExpressionParser::ExpressionParser(std::string expression) : text_(std::move(expression)) {}

void ExpressionParser::fail(const std::string& message) const {
    throw std::runtime_error(message);
}

void ExpressionParser::skipSpaces() {
    while (pos_ < text_.size() && std::isspace((unsigned char)text_[pos_])) ++pos_;
}

bool ExpressionParser::eat(char c) {
    skipSpaces();
    if (pos_ < text_.size() && text_[pos_] == c) {
        ++pos_;
        return true;
    }
    return false;
}

double ExpressionParser::evaluate() {
    pos_ = 0;
    double v = parseExpression();
    skipSpaces();
    if (pos_ != text_.size()) fail(std::string("Unexpected '") + text_[pos_] + "'");
    return v;
}

// expression := term (('+' | '-') term)*
double ExpressionParser::parseExpression() {
    double v = parseTerm();
    for (;;) {
        if (eat('+')) v += parseTerm();
        else if (eat('-')) v -= parseTerm();
        else return v;
    }
}

// term := unary (('*' | '/' | '%') unary)*     ("**" is power, so we stop at it)
double ExpressionParser::parseTerm() {
    double v = parseUnary();
    for (;;) {
        skipSpaces();
        if (pos_ + 1 < text_.size() && text_[pos_] == '*' && text_[pos_ + 1] == '*') return v;
        if (eat('*')) {
            v *= parseUnary();
        } else if (eat('/')) {
            double d = parseUnary();
            if (d == 0) fail("Division by zero");
            v /= d;
        } else if (eat('%')) {
            double d = parseUnary();
            if (d == 0) fail("Modulo by zero");
            v = std::fmod(v, d);
        } else {
            return v;
        }
    }
}

double ExpressionParser::parseUnary() {
    if (eat('-')) return -parseUnary();
    if (eat('+')) return parseUnary();
    return parsePower();
}

// power := postfix (('^' | '**') unary)?      (right-associative: 2^3^2 = 2^(3^2))
double ExpressionParser::parsePower() {
    double base = parsePostfix();
    skipSpaces();
    bool star2 = pos_ + 1 < text_.size() && text_[pos_] == '*' && text_[pos_ + 1] == '*';
    if (star2) pos_ += 2;
    if (star2 || eat('^')) return std::pow(base, parseUnary());
    return base;
}

// postfix := primary '!'*
double ExpressionParser::parsePostfix() {
    double v = parsePrimary();
    while (eat('!')) {
        if (v < 0 || v != std::floor(v) || v > 170) fail("Factorial needs an integer from 0 to 170");
        v = std::tgamma(v + 1);
    }
    return v;
}

// primary := number | '(' expression ')' | name | name '(' args ')'
double ExpressionParser::parsePrimary() {
    skipSpaces();
    if (pos_ >= text_.size()) fail("Unexpected end of expression");
    char c = text_[pos_];

    if (c == '(') {
        ++pos_;
        double v = parseExpression();
        if (!eat(')')) fail("Missing ')'");
        return v;
    }

    if (std::isdigit((unsigned char)c) || c == '.') {
        const char* start = text_.c_str() + pos_;
        char* end = nullptr;
        double v = std::strtod(start, &end);
        if (end == start) fail("Bad number");
        pos_ += (std::size_t)(end - start);
        return v;
    }

    if (std::isalpha((unsigned char)c)) {
        std::string name;
        while (pos_ < text_.size() && (std::isalnum((unsigned char)text_[pos_]) || text_[pos_] == '_'))
            name += (char)std::tolower((unsigned char)text_[pos_++]);
        if (name == "pi") return kPi;
        if (name == "e") return kE;
        if (name == "tau") return 2 * kPi;
        if (!eat('(')) fail("Unknown name '" + name + "'");

        std::vector<double> args;
        if (!eat(')')) {
            do args.push_back(parseExpression());
            while (eat(','));
            if (!eat(')')) fail("Missing ')' after function arguments");
        }
        return callFunction(name, args);
    }

    fail(std::string("Unexpected '") + c + "'");
}

double ExpressionParser::callFunction(const std::string& n, const std::vector<double>& a) {
    auto need = [&](std::size_t k) {
        if (a.size() != k) fail("'" + n + "' takes " + std::to_string(k) + " argument(s)");
    };
    if (n == "sqrt") { need(1); if (a[0] < 0) fail("sqrt of a negative number"); return std::sqrt(a[0]); }
    if (n == "cbrt") { need(1); return std::cbrt(a[0]); }
    if (n == "sin")  { need(1); return std::sin(a[0]); }
    if (n == "cos")  { need(1); return std::cos(a[0]); }
    if (n == "tan")  { need(1); return std::tan(a[0]); }
    if (n == "asin") { need(1); return std::asin(a[0]); }
    if (n == "acos") { need(1); return std::acos(a[0]); }
    if (n == "atan") { need(1); return std::atan(a[0]); }
    if (n == "sinh") { need(1); return std::sinh(a[0]); }
    if (n == "cosh") { need(1); return std::cosh(a[0]); }
    if (n == "tanh") { need(1); return std::tanh(a[0]); }
    if (n == "deg")  { need(1); return a[0] * 180.0 / kPi; }
    if (n == "rad")  { need(1); return a[0] * kPi / 180.0; }
    if (n == "ln")   { need(1); if (a[0] <= 0) fail("ln needs a positive number"); return std::log(a[0]); }
    if (n == "log" || n == "log10") { need(1); if (a[0] <= 0) fail("log needs a positive number"); return std::log10(a[0]); }
    if (n == "log2") { need(1); if (a[0] <= 0) fail("log2 needs a positive number"); return std::log2(a[0]); }
    if (n == "exp")  { need(1); return std::exp(a[0]); }
    if (n == "abs")  { need(1); return std::fabs(a[0]); }
    if (n == "floor"){ need(1); return std::floor(a[0]); }
    if (n == "ceil") { need(1); return std::ceil(a[0]); }
    if (n == "round"){ need(1); return std::round(a[0]); }
    if (n == "pow")  { need(2); return std::pow(a[0], a[1]); }
    if (n == "min" && !a.empty()) return *std::min_element(a.begin(), a.end());
    if (n == "max" && !a.empty()) return *std::max_element(a.begin(), a.end());
    fail("Unknown function '" + n + "'");
}

std::string ExpressionParser::evaluateToString(const std::string& expression) {
    double v = ExpressionParser(expression).evaluate();
    if (std::isnan(v) || std::isinf(v)) throw std::runtime_error("The result is not a finite number");
    char buf[64];
    if (v == std::floor(v) && std::fabs(v) < 1e15) std::snprintf(buf, sizeof buf, "%.0f", v);
    else std::snprintf(buf, sizeof buf, "%.12g", v);
    return buf;
}
