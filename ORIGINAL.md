# Original language

This document contains everything that I can remember from the unfortunate nameless language.

The original implementation only had the following keywords:
```
def      echo     elif       else       exit     if
import   let      read       return     system   while
```

> Unfortunately I don't remember if it was 'quit' or 'exit'.

These operators:
```
(   )   [   ]   {   }   ,
+   -   *   /   !   =   <
>   <=  >=  !=  ==
```

> I actually don't remember if it had '&&' and '||'.

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

`let name value` was the first assignment expression I added, which I then turned into
`let name = value`, and then allowed for assigning without the `let` keyword, making
it redundant, but I still left it there for no reason in particular.

The builtins `echo`, `exit`, `read` and `system` were implemented and treated as keywords
and needed no parentheses.

There were no comments, no for loops, and no way to break out of while loops.

The interpreter did not evaluate code until it was reached, and that could be used as comments.
It would not complain as long as the text got properly tokenized.

```
def __() { this is a stupid comment, dont call this function }
if 0 {
    this here is a multi-line comment
    it wont get executed either
}
```

Everything was an expression, but not everything returned a valid value. Anything that wasn't supposed
to return a value would return `nil` (namely, `def`, `import`, `echo`, `system`, `exit`).
There were also no closures or first-class functions.

Values like `true`, `false` and `nil` weren't keywords, but rather special variables for whatever reason.

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
if 0 { omitting the equal sign on assignment is valid and i hate it }
let b read "Input another number: "

if 0 { this type of assignment looks so much cleaner, wow }
c = min(a, b)

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
    so the interpreter shouldnt even try executing this part
}
```
