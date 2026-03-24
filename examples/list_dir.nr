# list all files within a directory using the system's `ls` command

def read_dir(path) {
    if system(string('ls -AF ', path, '/ 2> /dev/null > /tmp/cur-dir')) != 0 { return nil }
    if system(string('cd ', path, ' 2> /dev/null && pwd 2> /dev/null > /tmp/cur-dirname')) == 0 {
        fullpath = range(read_file('/tmp/cur-dirname'), 0, -1)
    } else {
        fullpath = path
    }
    names = range(split(read_file('/tmp/cur-dir'), '\n'), 0, -1)
    files = []
    for name = names {
        if !len(name) { next }
        file = {
            fullpath = fullpath,
            path = path,
            name = name,
            is_dir = false,
            is_exec = false,
            is_link = false,
        }
        if name[len(name)-1] == '/' {
            file.is_dir = true
            file.name = range(name, 0, -1)
        }
        if name[len(name)-1] == '@' {
            file.is_link = true
            file.name = range(name, 0, -1)
        }
        if name[len(name)-1] == '*' {
            file.is_exec = true
            file.name = range(name, 0, -1)
        }
        push(files, file)
    }
    return files
}

path = if len(arguments()) > 1 { arguments()[1] } else { '.' }
files = read_dir(path)
if files == nil {
    echo('failed to read directory: ', path)
    exit(1)
}
for file = files {
    stat = if file.is_dir    { ' <dir>  ' }
           elif file.is_exec { ' <exec> ' }
           elif file.is_link { ' <link> ' }
           else              { ' <file> ' }
    echo(stat, file.fullpath, '/', file.name)
}

