// Copyright (c) 2018 Red Hat <mtamasko@redhat.com>
// Author: Marek Tamaskovic <tamaskovic.marek@gmail.com>
//
// This file is part of strace2seccomp tool.
//
// strace2seccomp tool is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// strace2seccomp tool is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with strace2seccomp tool.  If not, see <https://www.gnu.org/licenses/>.

#include "algorithm_advanced.hpp"

#if __cplusplus < 201703L // C++14 version and bellow
    #include "cpp11_support.hpp"
    namespace variant_ns = mpark;
#else // C++17
    namespace variant_ns = std;
#endif

namespace st2se {
    bool Algo_advanced::optimize(Ids &in, Ids &out) {
        std::cout << "Algo_advanced optimize emitted." << std::endl;
        int no {0};

        for (const auto &item : in.data) {
            no++;
            #ifndef NDEBUG
            std::cout << "\n\n\n\n";
            std::cout << "=======================================" << std::endl;
            std::cout << "\tProcessing: (";
            std::cout << no;
            std::cout << "/";
            std::cout << in.data.size();
            std::cout << ") ";
            std::cout << item.first;
            std::cout << std::endl;
            std::cout << "=======================================" << std::endl;
            #endif
            this->processSyscall(item.second, out);
        }

        // std::cout << in.data.count("brk") << std::endl;

        return true;
    }

    void Algo_advanced::processSyscall(const Syscall &sc, Ids &out) {
        unsigned depth {0};

        #ifndef NDEBUG
        std::cout << "max depth of `" << sc.name << "` is";
        #endif

        // FIXME
        // make static function of this vvv
        for (const auto &arg : sc.next) {
            unsigned tmp;

            if ((tmp = getDepth(arg)) > depth) {
                #ifndef NDEBUG

                if (depth != 0) {
                    std::cout << " " << depth;
                }

                #endif
                depth = tmp;
            }
        }

        #ifndef NDEBUG
        std::cout << " " << depth << std::endl;
        #endif
        // ------------------------------------
        // Algorithm:

        out.data[sc.name].name = sc.name;
        out.data[sc.name].return_code = sc.return_code;
        out.data[sc.name].arg_num = sc.arg_num;

        for (unsigned arg_pos = 0; arg_pos < depth; arg_pos++) {
            std::vector<Argument> v;
            // std::cout << "arg_pos: " << arg_pos << std::endl;
            v = getArguemntsFromPos(sc.next, arg_pos);

            if (!v.empty()) {
                // std::cout << "v.size()" << v.size() << std::endl;

                // for (auto &item : v) {
                //     std::cout << arg2str(item) << " ";
                // }

                // std::cout << std::endl;

                // *INDENT-OFF*
                std::sort(v.begin(), v.end(),
                    [](Argument a, Argument b) {
                        return a.value > b.value;
                    }
                );

                auto end =
                    std::unique(v.begin(), v.end(),
                        [](Argument l, Argument r) {
                            return l.value == r.value;
                        }
                    );
                // *INDENT-ON*

                v.erase(end, v.end());
            }


            // Clustering:
            std::vector<Argument> clustered_v {};


            #ifndef NDEBUG
            unsigned clusters = this->cluster(v, clustered_v);
            std::cout << "arg_pos: " << arg_pos << " v:" << std::endl;
            std::cout << "\targ no." << arg_pos << " is ";
            std::cout << clustered_v.size() << " items long" << std::endl;
            std::cout << "\t\tclusters: " << clusters << std::endl;
            #else
            this->cluster(v, clustered_v);
            #endif

            out.data[sc.name].next.emplace_back(ValueFormat::EMPTY,
                ValueType::CLUSTERS,
                clustered_v
            );

            out.data[sc.name].clustered = true;

            #ifndef NDEBUG
            std::cout << "out...next.size()" << out.data[sc.name].next.size() << std::endl;
            std::cout << "\n" << std::endl;
            #endif
        }

        #ifndef NDEBUG
        std::cout << "\n\n" << std::endl;
        #endif

    }

    // clustering algorithm which clusters the arguments
    // from vector 'in' and stores them in vector 'out'
    unsigned Algo_advanced::cluster(std::vector<Argument> &in, std::vector<Argument> &out) {

        if (in.empty()) {
            return 0;
        }

        if (in.size() == 1) {
            out.push_back(in.front());
            return 1;
        }

        if (in.size() == 2) {
            out.push_back(in.front());
            out.push_back(in.back());
            return 2;
        }



        // DEBUGprint(arg2str(in.front()) << std::endl);
        // DEBUGprint("in.size() is " << in.size() << std::endl);
        // DEBUGprint("in is:");
        // DEBUGprintArgumentSet(in);

        std::vector<Argument> reducing_input;

        // DEBUGprint("reducing_input.size() is " << reducing_input.size() << std::endl);

        // duplicate in to reducing_input
        std::copy(in.begin(), in.end(),
            std::back_inserter(reducing_input));

        // DEBUGprint("reducing_input.size() is " << reducing_input.size() << std::endl);
        // DEBUGprint("reducing_input is:");
        // DEBUGprintArgumentSet(reducing_input);

        // helping vectors
        std::vector<Argument> to_check {};
        std::vector<Argument> cluster {};

        auto smallest_pair = smallestDst(in);
        double eps = 2.0 * distance(smallest_pair.first, smallest_pair.second);
        // DEBUGprint("eps: " << eps << std::endl);

        to_check.push_back(smallest_pair.first);
        // to_check.push_back(smallest_pair.second);

        // DEBUGprint(arg2str(smallest_pair.first) << std::endl);
        // DEBUGprint(arg2str(smallest_pair.second) << std::endl);

        // DEBUGprint("to_check is:");
        // DEBUGprintArgumentSet(to_check);

        // std::cout << "---------------------------------------\n\n" << std::endl;

        while (true) {
            // Debug info:
            //------------------------------------------------------------------
            // DEBUGprint("step 1: removeItem  to_check.front() from reducing_input" << std::endl);
            //------------------------------------------------------------------
            removeItem(to_check.front(), reducing_input);

            // Debug info:
            //------------------------------------------------------------------
            // DEBUGprint("to_check is:");
            // DEBUGprintArgumentSet(to_check);
            // DEBUGprint("cluster is:");
            // DEBUGprintArgumentSet(cluster);
            // DEBUGprint("reducing_input is:");
            // DEBUGprintArgumentSet(reducing_input);
            //------------------------------------------------------------------

            // Debug info:
            //------------------------------------------------------------------
            // DEBUGprint("step 2: find closest items to `" << arg2str(to_check.front()) << "`" << std::endl);
            //------------------------------------------------------------------

            auto vec_tmp = closestItemsTo(to_check.front(), reducing_input, eps);

            // DEBUGprint("vec_tmp" << vec_tmp.size() << " is:");
            // DEBUGprintArgumentSet(vec_tmp);

            // DEBUGprint("step 3" << std::endl);

            // DEBUGprint("step 3.1: insert clossest items to `to_check` set" << std::endl);
            for (auto item : vec_tmp) {
                to_check.push_back(item);
                cluster.push_back(item);
            }

            // DEBUGprint("to_check is:");
            // DEBUGprintArgumentSet(to_check);

            // DEBUGprint("step 3.2: remove clossest item from `reducing_input` set" << std::endl);
            for (auto item : vec_tmp) {
                removeItem(item, reducing_input);
            }

            // DEBUGprint("reducing_input is:");
            // DEBUGprintArgumentSet(reducing_input);

            // // move first `to_check` element to cluster
            // DEBUGprint("step 4: move first `to_check` element to cluster" << std::endl);

            moveFirstItem(/*where*/cluster, /*from*/ to_check);

            // DEBUGprint("to_check is:");
            // DEBUGprintArgumentSet(to_check);
            // DEBUGprint("cluster is:");
            // DEBUGprintArgumentSet(cluster);


            // DEBUGprint("step 5: check if is `to_check` set empty" << std::endl);

            if (to_check.empty()) {
                moveCluster(cluster, out);
                cluster.clear();

                if (!reducing_input.empty()) {
                    // DEBUGprint("step 5.1: `reducing_input` is not empty adding first item of it to to_check set" << std::endl);
                    to_check.push_back(reducing_input.front());
                }
                else {
                    // DEBUGprint("step 5.2: `reducing_input` is empty() breaking out from loop" << std::endl);
                    break;
                }
            }

            // DEBUGprint("\n\n\n");
        }

        // DEBUGprint("step 6: returning from cluster() with ret_val: " << out.size() << std::endl);

        return out.size();
    }

    bool Algo_advanced::moveFirstItem(std::vector<Argument> &to, std::vector<Argument> &from) {

        Argument element = from.front();

        for (auto item : to) {
            if (item == element) {
                from.erase(from.begin());
                return false;
            }
        }

        to.push_back(element);
        from.erase(from.begin());

        return true;
    }


    // asdf func()
    bool Algo_advanced::removeItem(Argument &arg, std::vector<Argument> &vec) {

        // DEBUGprint("arg is " << arg2str(arg) << std::endl);
        // DEBUGprint("vec is:" << std::endl);

        // for (auto item : vec) {
        //     std::cout << "\t" << arg2str(item) << std::endl;
        // }

        for (auto item = vec.begin(); item != vec.end(); item++) {
            // DEBUGprint("comparing:" << std::endl);
            // std::cout << "\t" << arg2str(arg) << std::endl;
            // std::cout << "\t" << arg2str(*item) << std::endl;

            if (*item == arg) {
                vec.erase(item);
                break;
            }
        }

        vec.shrink_to_fit();
        return true;
    }

    // asdf func()
    std::vector<Argument> Algo_advanced::closestItemsTo(Argument &arg, std::vector<Argument> &vec, double eps) {

        std::vector<Argument> ret_val {};

        if (vec.empty()) {
            return ret_val;
        }

        // DEBUGprint("computing distance:" << std::endl);
        // std::cout << "\t" << arg2str(vec.front()) << std::endl;
        // std::cout << "\t" << arg2str(vec.back()) << std::endl;

        for (auto item : vec) {
            if (distance(item, arg) <= eps) {
                ret_val.push_back(item);
            }
        }

        return ret_val;
    }

    // asdf func()
    bool Algo_advanced::moveCluster(std::vector<Argument> &cluster, std::vector<Argument> &out) {

        if (cluster.empty()) {
            return false;
        }

        out.push_back(cluster.back());
        cluster.pop_back();

        std::vector<Argument> &place = out.back().next;

        for (auto item : cluster) {
            place.push_back(item);
        }

        cluster.clear();

        return true;
    }


    // find the closest pair of values
    std::pair<Argument, Argument> Algo_advanced::smallestDst(std::vector<Argument> &in) {
        std::pair<Argument, Argument> p;
        double mem_dst = distance(in.front(), in.back());
        double tmp {0.0};

        // DEBUGprint("distance:" << std::endl);

        // std::cout << "\tin.size(): " << in.size() << std::endl;
        // std::cout << "\tmem_dst: " << mem_dst << std::endl;
        // std::cout << "\ti\tl\ttmp" << std::endl;

        for (unsigned i = 0; i < in.size(); i++) {
            for (unsigned l = (i + 1); l < in.size(); l++) {
                tmp = distance(in[i], in[l]);

                #ifndef NDEBUG
                // std::cout << "\t" << i << "\t" << l << "\t" << tmp << std::endl;
                #endif

                if (tmp == (-1.0)) {
                    continue;
                }

                if (tmp != 0.0 && tmp <= mem_dst) {
                    mem_dst = tmp;
                    // std::cout << "\tassigning: " << arg2str(in.at(i)) << std::endl;
                    // std::cout << "\tassigning: " << arg2str(in.at(l)) << std::endl;
                    p.first = in.at(i);
                    p.second = in.at(l);
                }
            }
        }

        // // DEBUGprint(arg2str(in.front()) << std::endl);
        // DEBUGprint(arg2str(p.first) << std::endl);
        // // DEBUGprint(arg2str(p.second) << std::endl);

        return p;
    }

    // computes distance for two arguments
    double Algo_advanced::distance(Argument &left, Argument &right) {
        double ret_val {-1.0};

        // std::cout << "\n\n\n";
        // DEBUGprint("valueType:" << left.valueType << std::endl);
        // DEBUGprint("valueType:" << right.valueType << std::endl);
        // DEBUGprint("left:" << arg2str(left) << "\n");
        // DEBUGprint("right:" << arg2str(right) << "\n");

        // constant is the same as bitfield but constant has no `|` operator
        if (
            (
                (left.valueType == ValueType::BITFIELD) ||
                (left.valueType == ValueType::CONSTANT)
            )
            ||
            (
                (right.valueType == ValueType::BITFIELD) ||
                (right.valueType == ValueType::CONSTANT)
            )
        ) {
            // compute distance between bitfields
            // DEBUGprint("computing bitfield distance\n");
            auto a = convert2bitfield(left);
            auto b = convert2bitfield(right);
            ret_val = bitfieldDistance(a, b);
        }
        else if (
            left.valueType == ValueType::POINTER ||
            left.valueType == ValueType::INTEGER
        ) {
            // written this way for null pointer check 'cus get_if is
            // non-throwing function
            auto _a = variant_ns::get_if<unsigned long>(&(left.value));
            auto _b = variant_ns::get_if<unsigned long>(&(right.value));
            unsigned long a {0};
            unsigned long b {0};

            if (_a != nullptr && _b != nullptr) {
                a = *_a;
                b = *_b;
            }
            else {
                return ret_val;
            }

            if (a > b) {
                ret_val = a - b;
            }
            else {
                ret_val = b - a;
            }

            // DEBUGprint("assigning:" << ((a > b) ? a - b : b - a) << std::endl);

        }


        // DEBUGprint("ret_val:" << ret_val << std::endl);
        // std::cout << "\n\n\n";


        return ret_val;
    }

    // FIXME maybe to `ids` module?
    // convert argument string value to Bitfield
    Bitfield convert2bitfield(const Argument &in) {

        std::string s;

        #if __cplusplus < 201703L
        if (auto _s = mpark::get_if<std::string>(&(in.value)))
        #else
        if (auto _s = std::get_if<std::string>(&(in.value)))
        #endif
        {
            s = *_s;
        }
        else {
            return Bitfield {};
        }

        std::string delim = "|";
        Bitfield bitfields;

        auto start = 0U;
        auto end = s.find(delim);

        while (end != std::string::npos) {
            bitfields.push_back(s.substr(start, end - start));
            start = end + delim.length();
            end = s.find(delim, start);
        }

        bitfields.push_back(s.substr(start, end));
        std::sort(bitfields.begin(), bitfields.end());

        return bitfields;
    }

    int bitfieldDistance(Bitfield &a, Bitfield &b) {
        int ret_val {0};

        if (a.empty() && b.empty()) {
            return 0;
        }

        if (a.empty()) {
            return b.size();
        }

        if (b.empty()) {
            return a.size();
        }

        if (a.size() < b.size()) {
            const Bitfield tmp = a;
            a = b;
            b = tmp;
        }

        int diff {0};

        // create uset of bitfields and initialize them with appropriate vectors
        std::unordered_set<std::string> uset_a {};
        std::unordered_set<std::string> uset_b {};

        for (const auto &item : a) {
            uset_a.insert(item);
        }

        for (const auto &item : b) {
            uset_b.insert(item);
        }

        // check if item from vector a is in uset_b
        // if not increment difference
        // else continue
        for (auto &item : a) {
            auto search = uset_b.find(item);

            if (search != uset_b.end()) {
                continue;
            }

            diff++;
        }

        // the same as above but for item from vector b in uset_a
        for (auto &item : b) {
            auto search = uset_a.find(item);

            if (search != uset_a.end()) {
                continue;
            }

            diff++;
        }

        // set ret_val with number of differences between bitfields
        ret_val = diff;

        return ret_val;
    }

} // namespace st2se
