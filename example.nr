def string_to_number(str) {
    num = i = 0
    while (i = i + 1) <= len(str) {
        num = num * 10 + (ord(str[i-1]) - ord("0"))
    }
    return num
}

def inc(x) {
    return x + 1
}

a = read("input a number: ")
echo("inc(a) = ", inc(string_to_number(a)))

