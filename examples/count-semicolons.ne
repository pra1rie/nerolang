# count all semicolons from specified files

total = 0
for arg, i = arguments() {
    if i == 0 { next }
    semi = 0
    path = arguments()[i]
    file = read_file(path)
    for char = file {
        if char == ';' {
            semi = semi + 1
        }
    }
    total = total + semi
    echo('file \'', path, '\' contains ', semi, ' semicolons.')
}
echo('counted ', total, ' semicolons in total.')

