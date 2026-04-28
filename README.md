## liblua.a 빌드 방법
### Windows
*`Emscripten 필요`*
```sh
$ emmake make liblua.a ^
    CC=emcc.bat ^
    AR="emar.bat rcs" ^
    RANLIB=emranlib.bat ^ 
    -sMEMORY64=1
```

## Build
### Windows
```sh
$ build.bat
```