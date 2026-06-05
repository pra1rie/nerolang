# extremely basic, stupid inventory system

def str_join(delim, list) {
    str = ''
    for tok, i = list {
        if i > 0 { push(str, delim) }
        push(str, string(tok))
    }
    return str
}

def list_find(list, fn) {
    for item = list {
        if fn(item) {
            return item
        }
    }
    return nil
}

def list_remove(list, val) {
    l = []
    for value, i = list {
        if value != val {
            push(l, value)
        }
    }
    return l
}

def is_number(str) {
    if !str { return false }
    for ch = str {
        if !contains('0123456789', ch) {
            return false
        }
    }
    return true
}

def new_inventory() {
    return { slots = [] }
}

def inventory_add(inv, item, amnt) {
    slot = list_find(inv.slots, def(s) { s.item == item })
    if slot != nil {
        slot.amount = slot.amount + amnt
    } else {
        push(inv.slots, { item = item, amount = amnt })
    }
}

def inventory_del(inv, item, amnt) {
    slot = list_find(inv.slots, def(s) { s.item == item })
    if slot != nil {
        if (slot.amount = slot.amount - amnt) <= 0 {
            inv.slots = list_remove(inv.slots, slot)
        }
    }
}

inv = new_inventory()
echo('epic inventory system (type `help` to see commands)')
while true {
    text = read('> ')
    if text == nil { break }
    line = split(text, ' ')
    command = line[0]
    if command == 'help' || command == 'h' {
        echo('commands:')
        echo('  (a)dd [amount] <item>')
        echo('  (d)el [amount] <item>')
        echo('  (l)ist')
        echo('  (h)elp')
        echo('  (q)uit')
    } elif command == 'add' || command == 'a' {
        if is_number(line[1]) {
            num = number(line[1])
            range_start = 2
        } else {
            range_start = num = 1
        }
        inventory_add(inv, str_join(' ', range(line, range_start, 0)), num)
    } elif command == 'del' || command == 'd' {
        if is_number(line[1]) {
            num = number(line[1])
            range_start = 2
        } else {
            range_start = num = 1
        }
        inventory_del(inv, str_join(' ', range(line, range_start, 0)), num)
    } elif command == 'list' || command == 'l' {
        for slot = inv.slots {
            echo(slot.item, ' x', slot.amount)
        }
    } elif command == 'quit' || command == 'q' {
        break
    }
}

