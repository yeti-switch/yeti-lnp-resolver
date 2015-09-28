#include "resolver_driver_mhash_csv.h"
#include "log.h"

#include <iostream>
#include <fstream>
#include <sstream>

resolver_driver_mhash_csv::resolver_driver_mhash_csv(const resolver_driver::driver_cfg &dcfg)
	: resolver_driver(dcfg)
{
    load_csv();
}

resolver_driver_mhash_csv::~resolver_driver_mhash_csv()
{}

void resolver_driver_mhash_csv::load_csv()
{
    std::ifstream f(_cfg.data_path,std::ifstream::in);
    if(!f.is_open()) {
        throw string("can't open file: "+_cfg.data_path);
    }

    size_t i = 0;
    string l;
    while(std::getline(f,l)&&++i){
        string number,tag,lrn;
        //dbg("line%ld: '%s'",i,l.c_str());
        std::stringstream ls(l);

        if(!std::getline(ls,number,',')){
            err("can't read number at line %ld",i);
            throw string("unexpected format");
        }
        if(!std::getline(ls,tag,',')){
            err("can't read tag at line %ld",i);
            throw string("unexpected format");
        }
        if(!std::getline(ls,lrn,',')){
            err("can't read lrn at line %ld",i);
            throw string("unexpected format");
        }

        /*dbg("%ld: number: '%s', tag: '%s', lrn: '%s'",
            i,number.c_str(),tag.c_str(),lrn.c_str());*/

        hash.emplace(number,lrn_entry(lrn,tag));
    }
}

void resolver_driver_mhash_csv::show_info(int map_id)
{
    info("id: %d, driver: %s, hash_size: %ld, csv_file: '%s'",
        map_id,driver_id2name(_cfg.driver_id),
        hash.size(),_cfg.data_path.c_str());
}

void resolver_driver_mhash_csv::resolve(const string &in, resolver_driver::result &out)
{
    out.raw_data.clear();
    entries_hash::const_iterator i = hash.find(in);
    if(i == hash.end()){
        dbg("number '%s' not found in hash. set out to in. empty tag",in.c_str());
        out.lrn = in;
        out.tag.clear();
        return;
    }
    out.lrn = i->second.lrn;
    out.tag = i->second.tag;
}
