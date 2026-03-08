#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
using namespace std;

inline string trim(const string &s) {
    auto a = s.find_first_not_of(" \t\r\n");
    if (a==string::npos) return "";
    auto b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b-a+1);
}

inline vector<string> split_ws(const string &s) {
    istringstream iss(s);
    vector<string> out;
    string t;
    while (iss >> t) out.push_back(t);
    return out;
}

#endif