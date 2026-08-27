
# It's pretty annoying to write a benchmark without arrays, so this'll be in Python for now.

from os import system
from time import perf_counter
from random import shuffle

results = {}

languages = [
    ("Carotene", "./caro-release", "caro"),
    ("Lua",      "lua",            "lua" ),
    ("Node.js",  "node",           "js"  ),
    ("PHP",      "php",            "php" ),
    ("Python",   "python",         "py"  )
]

print("\nThis is a rudimentary benchmark where 5 bytecode or JIT languages start and check whether 0 - 9,999 are prime.\nOf course, you're comparing production-grade super-optimized languages written by experts with my toy language, so this means nothing.\n")

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

print("\n\n               min      max")

for language, times in sorted(results.items(), key=lambda result: result[0].lower()):
    print(f"  {language:<8}  {round((min(times)) * 1000):>4} ms  {round((max(times)) * 1000):>4} ms")
