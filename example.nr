
# this is just the example from the README.

def string_to_number(str) {
    num = i = 0
    while (i = i + 1) <= len(str) {
        num = num * 10 + (ord(str[i-1]) - ord("0"))
    }
    return num
}

val = string_to_number(read("input a number: "))

if val == 69 || val == 420 {
    echo("nice")
} elif val == 67 {
    echo("bruh")
} else {
    echo("boring")
}

list = []

# this line shouldn't be executed
if val && list != [] { echo("UwU") }

list = push(list, val)
list = push(list, list)
list[0] = 420
echo("len(", list, ") = ", len(list))
list[1] = pop(list[1])

dict = {
    list = list,
    value = val,
}

dict.list = push(dict.list, "some text")

# dictionary keys can only be strings and may be accessed by dict.key or dict."key"
for value, i = dict."list" {
    echo("list[", i, "] = ", value)
}

echo(dict)
