#include "benchmark.h"
#include "heap_storage.h"
#include <cassert>
#include <cstddef>
#include <lmdb.h>
#include <string>
#include <vector>

std::vector<MDB_val*> Benchmark::block_data;;

void Benchmark::run(std::string filename) {

    size_t n[] = {10000, 100000, 1000000, 2500000, 3000000};
    BenchFile *file;

    if (block_data.empty()) {
        std::string *data = new std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
        for (size_t i = 0; i < 133; i++) {
            block_data.push_back(new MDB_val(data->size(), (void *)data->c_str()));
        }
    }
    printf("n,type,seconds\n");

    for (size_t i = 0; i < 5; i++) {
        file = new BenchFile(filename);
        printf("%lu,write,%f\n", n[i], write_test(*file, n[i]).count());
        delete file;
        file = new BenchFile(filename);
        file->open();
        printf("%lu,read,%f\n", n[i], write_test(*file, n[i]).count());
        file->drop();
        delete file;
    }

    for (auto data : block_data) {
        delete data;
    }
    block_data.clear();
}

TimeSpan Benchmark::write_test(BenchFile &file, size_t n) {
    file.create();
    std::vector<BenchPage*> *pages = init_pages(file, n);

    // Begin benchmark
    TimePoint start_time = steady_clock::now();

    for (BenchPage *page : *pages) {
        for (const MDB_val *data : block_data) {
            page->add(data);
        }
    }
    file.close(); // We have to close inside the benchmark because db cheats

    // End benchmark
    TimePoint end_time = steady_clock::now();

    free_pages(pages);
    TimeSpan span = duration_cast<TimeSpan>(end_time - start_time);
    return span;
}

TimeSpan Benchmark::read_test(BenchFile &file, size_t n) {
    // Begin benchmark
    TimePoint start_time = steady_clock::now();

    for (size_t j = 0; j < n; j++) {
        BenchPage *page = file.get(j);
        RecordIDs *ids = page->ids();
        for (size_t i = 0; i < ids->size(); i++) {
            MDB_val *data = page->get((*ids)[i]);
            // Assert that the data is the same as the data we put in
            if (memcmp(data->mv_data, block_data[i]->mv_data, data->mv_size) != 0) {
              throw "Bad block";
            }
            delete data;
        }
        delete ids;
    }

    // End benchmark
    TimePoint end_time = steady_clock::now();

    TimeSpan span = duration_cast<TimeSpan>(end_time - start_time);
    return span;
}

std::vector<BenchPage*> *Benchmark::init_pages(BenchFile &file, size_t n) {
    std::vector<BenchPage*> *pages = new std::vector<BenchPage*>();
    for (size_t i = 0; i < n; i++) {
        BenchPage *page = file.get_new();
        pages->push_back(page);
    }
    return pages;
}

void Benchmark::free_pages(std::vector<BenchPage*> *pages) {
    for (auto page : *pages) {
        delete page;
    }
    delete pages;
}
