// Our own exception classes.
//
// OOP ideas used here:
//   * Inheritance  - every error "is a" VeloraError, and a VeloraError "is a"
//                    std::runtime_error. So  catch (const VeloraError&)  catches
//                    all of ours in one go.
//   * Constructor inheritance (using Base::Base;) - saves us retyping constructors.
//   * Encapsulation - ApiError keeps the HTTP status private; read it with status().
#ifndef VELORA_CORE_EXCEPTIONS_H
#define VELORA_CORE_EXCEPTIONS_H

#include <stdexcept>
#include <string>

// Base class of all VeloraAI errors. what() returns a message that is safe to show to the user.
class VeloraError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Bad settings (for example an invalid port number).
class ConfigError : public VeloraError {
public:
    using VeloraError::VeloraError;
};

// The internet / AI service could not be reached.
class NetworkError : public VeloraError {
public:
    using VeloraError::VeloraError;
};

// A tool refused to do its job (blocked address, missing argument, ...).
class ToolError : public VeloraError {
public:
    using VeloraError::VeloraError;
};

// The AI service answered, but with an error (wrong key, quota, ...).
class ApiError : public VeloraError {
public:
    ApiError(int status, const std::string& message) : VeloraError(message), status_(status) {}

    int status() const noexcept { return status_; }

    // Turns "HTTP 401 + raw JSON body" into a friendly sentence.
    static ApiError fromResponse(int status, const std::string& rawBody);

private:
    int status_;
};

#endif  // VELORA_CORE_EXCEPTIONS_H
