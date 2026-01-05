# Nero
Implementation of my teeny tiny little toy language in ~1.5k lines of C.

It was one of my very first interpreters, written in [D](https://dlang.org)
to learn how recursive-descent parsers worked. I then accidentally deleted
the source code and was left with just the binary to play around with,
and then my computer fucking died and I don't even have that anymore.

Thankfully the language was simple enough, so I used it as a """specification"""
as an exercise for writing different implementations and extending upon them.

# Original language

The original implementation only had the following keywords:
```
def      echo     elif       else       exit     if
import   let      read       return     system   while
```

These operators:
```
(   )   [   ]   {   }   ,
+   -   *   /   !   =   <
>   <=  >=  !=  ==
```

I actually don't remember if it had '&&' and '||'.

And the following types:
* nil
* bool
* number
* string
* list

It was dynamically-typed, and everything was considered an expression.

## Quirks

It had 3 ways of assigning to variables:
```
let name value
let name = value
name = value
```

`let name value` was the first assignment expression I added, which i then turned into
`let name = value`, and then allowed for assigning without the `let` keyword, making
it redundant, but I still left it there for no reason in particular.

The builtins `echo`, `exit`, `read` and `system` were first implemented as keywords
instead of builtin functions (because there weren't any) and needed no parentheses.

There were no comments, no for loops, and no way to break out of while loops.

The interpreter did not evaluate code until it was reached, and that could be used as comments.
The interpreter would not complain as long as they got properly tokenized.

```
def __() { this is a stupid comment, dont call this function }
if 0 {
    this here is a multi-line comment
    it wont get executed
}
```

Everything was an expression, but not everything returned something valid. Anything that wasn't supposed
to return a value would return `nil` (namely, `def`, `import`, `echo`, `system`, `exit`).
There were also no closures or first-class functions.

Lists were one of the last things I added before losing the source code, and there was no way
to get the length, append or remove things from them. They merely existed.

## Example code

There isn't much to show:

```
def min(a, b) {
    if 0 { the last expression always gets returned }
    if a < b { a } else { b }
}

echo "Hello, World!"
let a = read "Input a number: "
if 0 { this syntax looks so goofy and i hate it }
let b read "Input another number: "

c = min(a, b) if 0 { this looks so much cleaner, wow }

echo "min(", a, ", ", b, ") = ", min(a, b)

list = [a, b, c]
if 0 { you can modify existing values, but cant append anything to it lmfao }
list[0] = false
echo list

i = 0
while (i = i + 1) < 6 {
    echo i
}

a = system "echo 'This is an external command'"
if a == nil {
    exit

    system always returns nil, and exit always exits (supposedly)
    so the interpreter shouldnt even try executing this
}
```

# C Implementation

Although a very naïve, slow, buggy implementation, the version in this repository
improves on the base language quite a bit.

It has a minimal set of keywords:
```
break   def      elif    else     if
for     import   next    return   while
```

These operators:
```
(   )   [   ]   {   }   ,   .
+   -   *   /   %   !   =   &&
||  <   >   <=  >=  !=  ==
```

6 types:
* nil
* bool
* number
* string
* list
* dict

And these builtin functions:
```
arguments   chr      contains   echo     exit         keys
len         ord      pop        push     read         read_file
split       string   system     typeof   write_file
```

## Example code

There still isn't much to show:
```
# this version has comments!

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

# dictionary keys can only be strings and may be accessed by dict.key, dict."key" or dict["key"]
for value, i = dict["list"] {
    echo("list[", i, "] = ", value)
}

echo(dict)
```

## Known issues

The most annoying ones i can remember from the top of my head are:
* It's too goddamn slow (i implemented it using the same method as the original version, by tokenizing the input text and executing it token by token, which is pretty dumb).
* Memory management is too naive (there is no GC, and the way i handle it is so bizarre, it's almost a ref counter, but not really. i honestly don't even know what that's supposed to be).
* Modifying dicts/lists passed functions doesn't work properly because the interpreter always copies the values and NEVER passes by reference.
