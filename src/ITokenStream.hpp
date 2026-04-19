#pragma once
#include "Token.hpp"

class ITokenStream {
public:
    virtual ~ITokenStream() = default;
    virtual Token next_token() = 0;
};
