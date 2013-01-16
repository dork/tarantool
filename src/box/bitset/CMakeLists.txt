add_library(bitset STATIC
    bitset.h
    bitset_p.h
    bitset.c
    iterator.h
    iterator.c
    index.h
    index.c
    expr.h
    expr_p.h
    expr.c
)

set_target_properties(bitset
    PROPERTIES COMPILE_FLAGS "-std=c99 -Wall")

target_link_libraries(bitset bit)
