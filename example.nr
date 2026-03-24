# this version has comments! (although only single-line)

def find(list, fn) {
    # it has first-class functions!!!
    for item = list {
        if fn(item) {
            return item
        }
    }
    return nil
}

def new_item(type, name) {
    # this here is a dictionary
    return { type = type, name = name }
}

def item_compare(a, b) {
    return a.type == b.type && a.name == b.name
}

def new_slot(item) {
    return { item = item, amount = 1 }
}

def new_inventory() {
    return { slots = [] }
}

def inventory_add(inv, item) {
    # look at this beautiful anonymous function!!!!!!!!!!!1111!
    slot = find(inv.slots, def(s) { item_compare(s.item, item) })
    if slot != nil {
        slot.amount = slot.amount + 1
    } else {
        push(inv.slots, new_slot(item))
    }
}

inv = new_inventory()
# add some arbitrary items
inventory_add(inv, new_item('food', 'apple'))
inventory_add(inv, new_item('food', 'banana'))
inventory_add(inv, new_item('food', 'pineapple'))
inventory_add(inv, new_item('food', 'apple'))
inventory_add(inv, new_item('food', 'banana'))
inventory_add(inv, new_item('food', 'apple'))
inventory_add(inv, new_item('food', 'apple'))

for slot = inv.slots {
    echo(slot.item.type, ': ', slot.item.name, ' x', slot.amount)
}

