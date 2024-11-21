def rec(str, list) {
    if len(list) == 0 { echo(str) }
    else {
        last = list[len(list)-1]
        str = string(str, last, " ")
        rec(str, pop(list))
    }
}

rec("", ["cookies", "your", "ate", "I"])

