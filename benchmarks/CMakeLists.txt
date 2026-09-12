add_executable(iptv_benchmarks 
    bench_m3u8_parser.cpp
)

target_include_directories(iptv_benchmarks PRIVATE ../include)
target_link_libraries(iptv_benchmarks PRIVATE 
    iptv_core 
    benchmark::benchmark_main
)

# Benchmarks must run extremely fast, ensure optimizations
target_compile_options(iptv_benchmarks PRIVATE -O3 -march=native -flto)
target_link_options(iptv_benchmarks PRIVATE -O3 -flto)
