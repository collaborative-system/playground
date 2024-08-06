# Building
```bash
$ git clone https://github.com/collaborative-system/playground
$ cd playground
$ mkdir -p buildDir
$ meson setup buildDir
$ meson compile -C buildDir
```

# Running
```bash
$ ./buildDir/playground <mount-path>
```