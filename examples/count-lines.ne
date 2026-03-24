# count all lines from specified files

total = 0
for arg, i = arguments() {
    if i == 0 { next }
    path = arguments()[i]
    lines = len(split(read_file(path), '\n'))
    total = total + lines
    echo('file \'', path, '\' contains ', lines, ' lines.')
}
echo('counted ', total, ' lines in total.')

