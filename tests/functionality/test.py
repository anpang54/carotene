

# impotrs

from pathlib import Path
from re import findall
from subprocess import run


# test

passes = 0
fails = 0

def test(name):
    global passes, fails

    with open(f"tests/functionality/{name}", "r") as file:
        source = file.read()

    expected = f"{"\n".join(result.strip() for result in findall(r"//(?!!)(.*)", source))}\n"

    output = run(["./caro", f"tests/functionality/{name}"], capture_output=True, text=True).stdout

    if output == expected:
        print(f"  ✅  {name}")
        passes += 1
    else:
        print(f"  ❌  {name}\n      Intended: {" ".join(expected.split())}\n      Output:   {" ".join(output.split())}")
        fails += 1


# do the tests

print("")

for file in Path("tests/functionality").rglob("*.caro"):
    test(f"{file.parts[-2]}/{file.parts[-1]}")

print(f"\n{passes} passed, {fails} failed")


