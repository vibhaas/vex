//
// Created by accord.
//

#ifndef VEXC_ERROR_CODE_H
#define VEXC_ERROR_CODE_H

// see docs/error_codes.md

enum class ErrorCode {
    OK = 0,
    INCORRECT_USAGE = 64,
    DATA_ERROR = 65,
    INPUT_ERROR = 66,
    INTERNAL_ERROR = 70,
    IO_ERROR = 74
};

constexpr int get_error_code(const ErrorCode code) {
    return static_cast<int>(code);
}

#endif  // VEXC_ERROR_CODE_H