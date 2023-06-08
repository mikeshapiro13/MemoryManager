//
// Created by mikes on 3/22/2022.
//

#pragma once


#include <functional>
#include <vector>
#include <cstdint>

#ifndef BEST_FIT
#define BEST_FIT
int bestFit(int sizeInWords, void *list);
#endif

#ifndef WORST_FIT
#define WORST_FIT
int worstFit(int sizeInWords, void *list);
#endif


class MemoryManager {
    struct block
    {
        bool free;
        unsigned int offset;
        unsigned int length;
        block* next;
        block* prev{};
        block(unsigned int offset, unsigned int length, block* next, block* prev, bool free);
        uint64_t* begin;
    };
private:
    unsigned int wordSize;
    unsigned int memSize;
    std::function<int(int, void*)> allocator;
    block* head;
    block* tail;
    uint64_t* mem;
public:
    MemoryManager(unsigned wordSize, std::function<int(int, void *)> allocator);
    ~MemoryManager();
    void initialize(size_t sizeInWords);
    void shutdown();
    void *allocate(size_t sizeInBytes);
    void free(void *address);
    void setAllocator(std::function<int(int, void *)> allocator);
    int dumpMemoryMap(char *filename);
    void *getList();
    void *getBitmap();
    unsigned getWordSize();
    void *getMemoryStart();
    unsigned getMemoryLimit();
};




