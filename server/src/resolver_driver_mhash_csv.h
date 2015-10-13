#pragma once

#include "resolver_driver.h"
#include <unordered_map>

using std::unordered_map;

class resolver_driver_mhash_csv: public resolver_driver {
    struct lrn_entry {
        string lrn, tag;
        size_t csv_line;
        lrn_entry(string lrn, string tag, size_t line):
            lrn(lrn),
            tag(tag),
            csv_line(line)
        {}
    };

    typedef unordered_map<string,lrn_entry> entries_hash;
    entries_hash hash;

    void load_csv();
  public:
    resolver_driver_mhash_csv(const resolver_driver::driver_cfg &dcfg);
    ~resolver_driver_mhash_csv();

    void show_info(int map_id);
    void resolve(const string &in, resolver_driver::result &out);
};
