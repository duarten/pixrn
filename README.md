# Compiling

Ensure libexif is installed (e.g., by doing `brew install libexif`).

Then, use `cmake` and `make` to build the executable:

```
cmake -D CMAKE_C_COMPILER=gcc-7 -D CMAKE_CXX_COMPILER=g++-7 .
make
```

# Executing

`./pixrn <path-to-directory-with-photos-to-rename>`
