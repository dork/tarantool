add_library(bitmap STATIC
    bitmap.h
    bitmap_p.h
    bitmap.c
    iterator.h
    iterator.c
    index.h
    index.c
    bit.h
    bit.c
    expr.h
    expr_p.h
    expr.c
)

set_target_properties(bitmap
    PROPERTIES COMPILE_FLAGS "-std=c99 -Wall")

