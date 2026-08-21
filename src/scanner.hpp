
#pragma once


// includes

#include "common.hpp"


// scanner

class Scanner{

    public:

        string start;
        string current;
        int line;

        Scanner(string source) {
            this->start = source;
            this->current = source;
            this->line = 1;
        }

};