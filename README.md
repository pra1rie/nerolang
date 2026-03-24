# Nero

Implementation of my teeny tiny little toy language in ~1.5k lines of C.

It was one of my very first interpreters, written in [D](https://dlang.org)
to learn how recursive-descent parsers worked. I then accidentally deleted
the source code and was left with just the binary to play around with,
and then my computer fucking died and I don't even have that anymore.

Thankfully the language was simple enough, so I used it as a reference
as an exercise for writing different implementations and extending upon them.

See [the original language](./ORIGINAL.md).

## C Implementation

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
||  <   >   <=  >=  !=  ==  ~
&   ^   |   <<  >>
```

8 types:
* nil
* bool
* int
* real
* str
* list
* dict
* func

And these builtin functions:
```
arguments   chr      contains   dup      echo     exit    keys
len         number   ord        pop      push     range   read
read_file   split    string     system   typeof   write_file
```

## Basic Syntax

### Variables and types

```
# this is a comment
int_var = 69
real_var = 420.69
str_var = "Hello, world!"
bool_var = true || false && !nil
list_var = [0, 1, 2, 3, nil, 'some text', false, 0.5]
dict_var = { key = 'value', "other key" = 80085 }
```

### Conditions and loops

```
# falsey values are 'nil', 'false', empty list, empty string, empty dict
# everything else is true
val = nil
if !val {
    echo('value is falsey')
} else {
    echo('value is truthy')
}

# count from 1 to 10, skip 3 and break on 8
i = 0
while i < 10 {
    i = i + 1
    if i == 3 {
        next
    }
    if i == 8 {
        break
    }
    echo(i)
}

list = [1, 2, 3, 4, 5, 6]
for value, index = list {
    list[index] = value * 2
    echo('list[', index, '] = ', value)
}

for i = len(list) {
    echo(list[i])
}

```

### Functions

```
def sum_all(list) {
    result = 0
    for value = list {
        result = result + value
    }
    return result
}

echo(sum_all([1, 2, 3, 4, 5, 6, 7, 8, 9]))

double = def(x) { return x * 2 }
echo(double(34.5))

def find(list, fn) {
    for item = list {
        if fn(item) { return item }
    }
    return nil
}

def make_item(name, amount) {
    return { name = name, amount = amount }
}

grocery_list = [
    make_item('apples', 50403),
    make_item('tomatoes', 33),
    make_item('rubber tire', 5),
]

apples = find(grocery_list, def(itm) { itm.name == 'apples' })
if apples != nil {
    echo('must buy ', apples.amount, ' ', apples.name)
}
```

## Operator Precedence

Nero's operators are evaluated in this order (highest precedence first):
```
-- parentheses as expression separators:
    ()
-- field accessors and function calls:
    . [] ()
-- unary operators:
    ! ~ -
-- binary operators:
    * / %
    + -
    << >>
    & | ^
    == != < > <= >=
    && ||
```

## Memory Management

This version of nero depends on `libgc` for its memory management.
It literally lets everything leak, and ***just trusts the GC***.

Under the hood, all its non-numeric types are pointers that get allocated and copied around and are never ever free'd.
Needless to say, that is extremely stupid and very very slow.

Since nero passes (mostly) everything as a reference, if you need to copy a value, use the builtin `dup` function.

## Known issues

The most annoying ones i can remember from the top of my head are:
* It's too goddamn slow (i implemented it using the same method as the original version, by tokenizing the input text and executing it token by token, which is pretty dumb).
* Memory management is genuinely retarded.
* The language overall is kinda shit (its fun to use tho).
