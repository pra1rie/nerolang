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

def new_item(type, name) {
    return { type = type, name = name }
}

def new_slot(item) {
    return { item = item, amount = 1 }
}

def new_inventory() {
    return { slots = [] }
}

def inventory_add(inv, item) {
    slot = list_find(inv.slots, def(s) { s.item == item })
    if slot != nil {
        slot.amount = slot.amount + 1
    } else {
        push(inv.slots, new_slot(item))
    }
}

def inventory_del(inv, item) {
    slot = list_find(inv.slots, def(s) { s.item == item })
    if slot != nil {
        if (slot.amount = slot.amount - 1) == 0 {
            inv.slots = list_remove(inv.slots, slot)
        }
    }
}

inv = new_inventory()
echo('epic inventory system')
while true {
    text = read('> ')
    if text == nil { break }
    line = split(text, ' ')
    command = line[0]
    if command == 'help' || command == 'h' {
        echo('commands:')
        echo('  (a)dd <item>')
        echo('  (d)el <item>')
        echo('  (l)ist')
        echo('  (h)elp')
    } elif command == 'add' || command == 'a' {
        inventory_add(inv, str_join(' ', range(line, 1, 0)))
    } elif command == 'del' || command == 'd' {
        inventory_del(inv, str_join(' ', range(line, 1, 0)))
    } elif command == 'list' || command == 'l' {
        for slot = inv.slots {
            echo(slot.item, ' x', slot.amount)
        }
    } elif command == 'quit' || command == 'q' {
        break
    }
}

