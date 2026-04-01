#pragma once

#define ROSCRAFT_BIT(x) (1 << (x))

// Stringify macros
#define ROSCRAFT_STRINGIFY_IMPL(x) #x
#define ROSCRAFT_STRINGIFY(x) ROSCRAFT_STRINGIFY_IMPL(x)

// Concatenation macros
#define ROSCRAFT_CONCAT_IMPL(a, b) a##b
#define ROSCRAFT_CONCAT(a, b) ROSCRAFT_CONCAT_IMPL(a, b)

// Anonymous variable generation
#define ROSCRAFT_ANONYMOUS_VAR(prefix) ROSCRAFT_CONCAT(prefix, __LINE__)
