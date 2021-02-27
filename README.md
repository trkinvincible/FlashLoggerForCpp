# FlashLoggerForCpp

Logger for ultra low latency in C++ (https://github.com/trkinvincible/InMemoryCacheForCpp):

Open Sourced under MIT license. This project implements ultra fast logging for CPP projects.
Once service started there will be zero heap memory allocations by the logging service. Unlike
commercially available loggers this logger guarantees both latency to microseconds and high availability. Latency is
improved with always aligned and continuous memory access, instruction and data cache intact in same
processor(s), ProtoBuf for zero copy and zero allocation to move data from in-memory to external
IO(demonstrated for Linux file system).
