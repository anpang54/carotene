

#pragma once


// INCLUDES

#include <array>

#include "../util/natives.hpp"


// HELPERS

size_t damerauLevenshtein(const string& source, const string& target) {

    size_t m = source.size();
    size_t n = target.size();
    if(m == 0) return n;
    if(n == 0) return m;

    size_t infinity = m + n;
    size_t width = n + 2;
    vector<size_t> score((m + 2) * width, 0);
    auto at = [&](size_t i, size_t j) -> size_t& { return score[i * width + j]; };

    at(0, 0) = infinity;
    for(size_t i = 0; i <= m; ++i) {
        at(i + 1, 1) = i;
        at(i + 1, 0) = infinity;
    }
    for(size_t j = 0; j <= n; ++j) {
        at(1, j + 1) = j;
        at(0, j + 1) = infinity;
    }

    std::array<size_t, 256> lastRow{};

    for(size_t i = 1; i <= m; ++i) {

        size_t lastMatch = 0;

        for(size_t j = 1; j <= n; ++j) {

            unsigned char sourceChar = source[i - 1];
            unsigned char targetChar = target[j - 1];

            size_t i1 = lastRow[targetChar];
            size_t j1 = lastMatch;

            if(sourceChar == targetChar) {
                at(i + 1, j + 1) = at(i, j);
                lastMatch = j;
            } else {
                at(i + 1, j + 1) = std::min({at(i, j), at(i + 1, j), at(i, j + 1)}) + 1;
            }

            at(i + 1, j + 1) = std::min(at(i + 1, j + 1), at(i1, j1) + (i - i1 - 1) + 1 + (j - j1 - 1));

        }

        lastRow[(unsigned char)source[i - 1]] = i;

    }

    return at(m + 1, n + 1);

}
    // based on https://gist.github.com/IceCreamYou/8396172 and https://gist.github.com/croneter/19d9ffb8e41dd4584465128b49485dcf
    // initially used in acrylic


// FUNCTIONS

nFunc(text_levenshtein_distance, "text", "levenshtein_distance", {
    params({
        {{OBJ_STRING}, true},
        {{OBJ_STRING}, true}
    });
    return CaroUlong(damerauLevenshtein(asString(args[0])->str, asString(args[1])->str));
});

nFunc(text_levenshtein_ratio, "text", "levenshtein_ratio", {
    params({
        {{OBJ_STRING}, true},
        {{OBJ_STRING}, true}
    });

    const string& source = asString(args[0])->str;
    const string& target = asString(args[1])->str;

    size_t total = source.size() + target.size();
    if(total == 0) return CaroDouble(1.0);

    return CaroDouble((double)(total - damerauLevenshtein(source, target)) / (double)total);

});
