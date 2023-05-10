//
// Created by Nathan Tormaschy on 4/19/23.
//

#ifndef ARGUS_UTILS_STRING_H
#define ARGUS_UTILS_STRING_H
#include <cstddef>
#include <algorithm>
#include <iterator>
#include <cstring>
#include <iostream>
#include <vector>
#include <tuple>

using namespace std;

bool case_ins_str_compare(const std::string& str1, const std::string& str2) {
    return strcasecmp(str1.c_str(), str2.c_str()) == 0;
}

size_t case_ins_str_index(const std::vector<std::string>& columns, const std::string& column) {
    auto it = std::find_if(columns.begin(), columns.end(), [&column](const std::string& s) {
        return case_ins_str_compare(s, column);
    });

    if (it != columns.end()) {
        size_t open_index = std::distance(columns.begin(), it);
        return open_index;
    } else {
        throw std::runtime_error("Failed to find column: " + column);
    }
}

std::tuple<size_t, size_t> parse_headers(const std::vector<std::string>& columns) {
    size_t open_index = case_ins_str_index(columns, "open");
    size_t close_index = case_ins_str_index(columns, "close");
    return std::make_tuple(open_index, close_index);
}

#endif //ARGUS_UTILS_STRING_H

/*
INSIDE PARSE HEADERS
INSIDE PARSE HEADERS case_ins_str_index
INSIDE case_ins_str_compate
id
*/