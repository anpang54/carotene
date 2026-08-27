
# It's pretty annoying to write a benchmark without arrays, so this'll be in Python for now.

from os import system
from time import perf_counter
from random import shuffle

results = {}

languages = [
    ("Carotene",    "./caro-release",                      "caro"),
    ("clox with %", "./tests/benchmarks/clox-with-modulo", "clox"),
    ("Lua",         "luajit -j off",                       "lua" ),
    ("Node.js",     "node --jitless",                      "js"  ),
    ("PHP",         "php -d opcache.enable_cli=0",         "php" ),
    ("Python",      "python",                              "py"  ),
    ("Wren",        "wren",                                "wren")
]

print("\nThis is a rudimentary benchmark where a few bytecode interpreters start and check whether 0 - 9,999 are prime.\nOf course, you're comparing production-grade super-optimized languages written by experts with my toy language, so this doesn't mean much.\nNode.js, Lua, and PHP are in their non-JIT modes.\n")

print("Trial ", end="", flush=True)

for i in range(6):

    print(f"{i} ", end="", flush=True)
    shuffle(languages)
    for language in languages:

        start = perf_counter()
        system(f"{language[1]} tests/benchmarks/find_primes/find_primes.{language[2]}")
        end = perf_counter() - start

        if i > 0:    # discard first trial
            if not language[0] in results:
                results[language[0]] = []
            results[language[0]].append(end)

print("\n\n                  min      max")

for language, times in sorted(results.items(), key=lambda result: min(result[1])):
    print(f"  {language:<11}  {round((min(times)) * 1000):>4} ms  {round((max(times)) * 1000):>4} ms")

