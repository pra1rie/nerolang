greeting = "Greetings, "

def greet(name) {
    if !name {
        echo("Thou art but a nameless peasant.")
    } elif name == "uwu" {
        echo("BEGONE!!")
    } else {
        echo(greeting, name)
    }
}

greet(read("What is thy name? "))
