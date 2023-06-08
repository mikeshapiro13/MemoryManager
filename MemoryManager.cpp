//
// Created by mikes on 3/22/2022.
//

#include "MemoryManager.h"

#include <utility>
#include <vector>
#include <cmath>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <sstream>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <climits>


int bestFit(int sizeInWords, void *list) {
//    int offset = -1;
//    int smallest = INT_MAX;
//    int *copyList = (int*) list;
//    int numHoles = copyList[0];
//    copyList += sizeof(int);
//    while (numHoles > 0)
//    {
//        if (sizeInWords < *(copyList + sizeof(int))) {
//            if (smallest > *(copyList + sizeof(int))) {
//                offset = *copyList;
//                copyList += sizeof(int);
//                smallest = *copyList;
//            }
//        }
//        --numHoles;
//    }
//    return offset;

    uint16_t offset = -1;
    uint16_t smallest = INT_MAX;
    auto* copy = (uint16_t*)list;
    uint16_t numHoles = copy[0];
    for (uint16_t i = 1; i <= numHoles; i++)
    {
        if (sizeInWords <= copy[2*i])
            if (smallest > copy[2*i])
            {
                offset = copy[2*i-1];
                smallest = copy[2*i];
            }
    }
    //delete[] copy;
    return offset;
}

int worstFit(int sizeInWords, void *list) {
//    int offset = -1;
//    int largest = INT_MIN;
//    int *copyList = (int*) list;
//    int numHoles = copyList[0];
//    copyList += sizeof(int);
//    while (numHoles > 0)
//    {
//        if (sizeInWords < *(copyList + sizeof(int))) {
//            if (largest < *(copyList + sizeof(int))) {
//                offset = *copyList;
//                copyList += sizeof(int);
//                largest = *copyList;
//            }
//        }
//        copyList += sizeof(int);
//        --numHoles;
//    }
//    return offset;
    uint16_t offset = 0;
    uint16_t largest = 0;
    auto* copy = (uint16_t*) list;
    if (!list)
        return -1;
    int numHoles = copy[0];
    for (uint16_t i = 1; i <= numHoles; i++)
    {
        if (sizeInWords <= copy[2*i])
            if (largest < copy[2*i])
            {
                offset = copy[2*i-1];
                largest = copy[2*i];
            }
    }
    //delete[] copy;
    return offset;
}

MemoryManager::MemoryManager(unsigned int wordSize, std::function<int(int, void *)> allocator) {
    this->wordSize = wordSize;
    this->allocator = std::move(allocator);
    head = nullptr;
    tail = nullptr;
    memSize = 0;
}

MemoryManager::~MemoryManager() {
    this->shutdown();
}

void MemoryManager::initialize(size_t sizeInWords) {
    if (sizeInWords <= 65536) {
        memSize = sizeInWords;
        auto* first = new block(0, memSize, nullptr, nullptr, true);
        head = first;
        mem = new uint64_t[sizeInWords];
        for (int i = 0; i < sizeInWords; ++i)
            mem[i] = 0;
    }
}

void MemoryManager::shutdown() {
    block* curr = head;
    while (curr != nullptr)
    {
        block* next = curr->next;
        delete curr;
        curr = next;
    }
    head = nullptr;
    if (mem != nullptr) {
        delete[] mem;
        mem = nullptr;
    }
}

void *MemoryManager::allocate(size_t sizeInBytes) {
    int size = (long)(sizeInBytes / wordSize);
    auto* tmpList = static_cast<uint16_t *>(getList());
    int offset = allocator(size, tmpList);
    block *curr = head;
    uint64_t* test = mem;
    delete[] tmpList;
    if (offset == -1)
        return nullptr;
    else {
        auto* toAdd = new block(offset, size, nullptr, nullptr, false);
        while (curr->offset < offset) {
            curr = curr->next;
        }
        for (int i = 0; i < offset; ++i)
            ++test;
        for (int i = 0; i < size; ++i)
            test[i] = '1';

        curr->length = curr->length - toAdd->length;
        if (curr->length == 0)
        {
            curr->free = false;
            curr->offset = offset;
            curr->length = size;
            delete toAdd;
            return test;
        }
        else {
            curr->offset = curr->offset + toAdd->length;
            if (curr->prev)
                curr->prev->next = toAdd;
            else
                head = toAdd;
            toAdd->next = curr;
            toAdd->prev = curr->prev;
            curr->prev = toAdd;
        }
        toAdd->begin = test;
        return test;
    }
}


void MemoryManager::free(void *address) {
    auto* test = (uint64_t *)address;
    uint64_t* garb = test;
    block* toRemove = head;
    while (toRemove->begin != test)
        toRemove = toRemove->next;
    toRemove->free = true;
    for (int i = 0; i < toRemove->length; ++i) {
        *garb = '0';
        garb += sizeof(unsigned char);
    }
    if (toRemove->prev && toRemove->next && toRemove->prev->free && toRemove->next->free)
    {
        block* currNext = toRemove->next;
        block* currPrev = toRemove->prev;
        auto* newGuy = new block(currPrev->offset, currPrev->length + toRemove->length + currNext->length, currNext->next, currPrev->prev, true);
        newGuy->begin = currPrev->begin;
        if (newGuy->offset == 0)
            head = newGuy;
        if (currPrev->prev != nullptr)
            currPrev->prev->next = newGuy;
        if (currNext->next != nullptr)
            currNext->next->prev = newGuy;
        delete currPrev;
        delete currNext;
        delete toRemove;
    }
    else if (toRemove->prev && toRemove->prev->free)
    {
        block* curr = toRemove->prev;
        auto* newGuy = new block(curr->offset, curr->length + toRemove->length, toRemove->next, curr->prev, true);
        newGuy->begin = curr->begin;
        curr->prev->next = newGuy;
        toRemove->next->prev = newGuy;
        delete curr;
        delete toRemove;
    }
    else if (toRemove->next && toRemove->next->free)
    {
        block* curr = toRemove->next;
        auto* newGuy = new block(toRemove->offset, curr->length + toRemove->length, curr->next, toRemove->prev, true);
        newGuy->begin = toRemove->begin;
        if (!toRemove->prev)
            head = newGuy;
//        toRemove->prev->next = newGuy;
        curr->next->prev = newGuy;
        delete curr;
        delete toRemove;
    }
}

void MemoryManager::setAllocator(std::function<int(int, void *)> allocator) {
    this->allocator = std::move(allocator);
}

int MemoryManager::dumpMemoryMap(char *filename) {
    int op = creat(filename, S_IRWXU);
    if (op == -1) {
        std::cout << "Error can't open file" << std::endl;
        return -1;
    }
    block* curr = head;
    std::string buff;
    while (curr != nullptr)
    {
        if (curr->free)
        {
            buff += "[";
            buff += std::to_string(curr->offset);
            buff += ", ";
            buff += std::to_string(curr->length);
            buff += "]";
            if (curr->next != nullptr) {
                buff += " - ";
            }
        }
        curr = curr->next;
        int wr = write(op, buff.c_str(), buff.size());
        if (wr == -1) {
            std::cout << "Error can't write" << std::endl;
            return -1;
        }
        buff.clear();
    }
    int cl = close(op);
    if (cl == -1) {
        std::cout << "Error can't close file" << std::endl;
        return -1;
    }
    return 0;
}

void *MemoryManager::getList() {
    auto size = 0;
    block* curr = head;
    while(curr != nullptr) {
        if (curr->free)
            ++size;
        curr = curr->next;
    }
    auto* mylist = new uint16_t[size*2+1];
    mylist[0] = 0;
    curr = head;
    int i = 1;
    while (curr != nullptr)
    {
        if (curr->free)
        {
            mylist[0]++;
            mylist[i] = curr->offset;
            mylist[i+1] = curr->length;
            i += 2;
        }
        curr = curr->next;
    }
    if (size < 1) {
        delete[] mylist;
        return nullptr;
    }

    return mylist;
}

void *MemoryManager::getBitmap() {
    std::string bitmap;
    std::vector<uint8_t> rtn;
    block* curr = head;
    while (curr != nullptr)
    {
        if (curr->free)
            for(int i = 0; i < curr->length; ++i)
                bitmap.insert(bitmap.begin(), '0');
        else
            for(int i = 0; i < curr->length; ++i)
                bitmap.insert(bitmap.begin(), '1');
        curr = curr->next;
    }

    for(int i = bitmap.size(); i > 0; i -= 8)
    {

        std::string tmp = "";
        if (i >= 8)
            tmp = bitmap.substr(i - 8, 8);
        else
        {
            tmp = bitmap.substr(0, i);
//            for (int j = 0; j < 8 - i; ++i)
//                tmp.insert(tmp.begin(), '0');
        }
        int num = 0;
        num = std::stoi(tmp, 0, 2);
        rtn.push_back(num);
    }
//    std::stringstream ss;
//    ss<< std::hex << bitmap.size();
//    std::string size(ss.str());
    unsigned char low, high = '0';
//    short int intSize = stoi(size, 0, 16);
    short int intSize = ceil(((float) bitmap.size() / 8));
    low = intSize & 0xFF;
    high = (intSize>>8) & 0xFF;
    rtn.insert(rtn.begin(), high);
    rtn.insert(rtn.begin(), low);


    uint8_t* arr = new uint8_t[rtn.size()];
    for (int i = 0; i < rtn.size(); ++i)
        arr[i] = rtn[i];
    return arr;
}

unsigned MemoryManager::getWordSize() {
    return wordSize;
}

void *MemoryManager::getMemoryStart() {
    return mem;
}

unsigned MemoryManager::getMemoryLimit() {
    return memSize * wordSize;
}

MemoryManager::block::block(unsigned int offset, unsigned int length, MemoryManager::block *next, block* prev, bool free) {
        this->offset = offset;
        this->length = length;
        this->next = next;
        this->prev = prev;
        this->free = free;
}

