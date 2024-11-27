import "lib.nr"

def inc(x) {
    return x + 1
}

a = read("input a number: ")
echo("inc(a) = ", inc(string_to_number(a)))

