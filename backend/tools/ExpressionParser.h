// ExpressionParser: evaluates maths text like  "1200*(1+0.07)^5"  or  "sqrt(16)+3!".
//
// How it works ("recursive descent"): each grammar rule is one small function, and the
// functions call each other in order of operator priority (lowest to highest):
//
//     parseExpression  ->  + and -
//     parseTerm        ->  * / %
//     parseUnary       ->  unary minus/plus
//     parsePower       ->  ^ and **
//     parsePostfix     ->  !   (factorial)
//     parsePrimary     ->  numbers, ( brackets ), pi, e, sqrt(...)
//
// OOP ideas used here:
//   * Encapsulation - the text and the current position are PRIVATE state; the object
//     keeps track of "where am I in the text" so callers never have to.
//   * Class vs. namespace - this needs state (pos_), so it is a class.
#ifndef VELORA_TOOLS_EXPRESSIONPARSER_H
#define VELORA_TOOLS_EXPRESSIONPARSER_H

#include <string>
#include <vector>

class ExpressionParser {
public:
    explicit ExpressionParser(std::string expression);

    // Calculates the value. Throws std::runtime_error with a clear message on bad input.
    double evaluate();

    // Convenience: parse + evaluate + format ("7" instead of "7.000000").
    static std::string evaluateToString(const std::string& expression);

private:
    double parseExpression();
    double parseTerm();
    double parseUnary();
    double parsePower();
    double parsePostfix();
    double parsePrimary();
    double callFunction(const std::string& name, const std::vector<double>& args);

    void skipSpaces();
    bool eat(char c);  // consume c if it is next
    [[noreturn]] void fail(const std::string& message) const;

    std::string text_;
    std::size_t pos_ = 0;
};

#endif  // VELORA_TOOLS_EXPRESSIONPARSER_H
