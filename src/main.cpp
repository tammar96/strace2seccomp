#include <iostream>

#include "tao/pegtl.hpp"

#include "argparse.hpp"
#include "ids.hpp"
#include "optimizer.hpp"
#include "states.hpp"
#include "straceparser.hpp"
#include "algorithm_weak.hpp"
#include "algorithm_strict.hpp"
#include "algorithm_advanced.hpp"
#include "generator.hpp"

int main(int argc, char *argv[]) {

    int ret_val {0};

    Params params(argc, argv);

    if (params.debug) {
        std::cout << params << std::endl;
    }

    st2se::Ids in {};
    st2se::Ids out {};
    st2se::States states {};
    st2se::StraceParser o(in);

    // run grammar analysis
    if (params.analysis != 0) {

        std::cout << "Analyzing grammar ..." << std::endl;

        const std::size_t issues = o.AnalyzeGrammar();

        std::cout << "Found " << issues << " issues." << std::endl;

        return ret_val;
    }

    // parse files
    for (const auto &fn : params.file_names) {
        o.parse(fn, in, params, states);
    }

    // optimize IDS
    st2se::Algorithm *algo {nullptr};

    if (params.weak != 0) {
        algo = new st2se::Algo_weak();
    }
    else if (params.strict != 0) {
        algo = new st2se::Algo_strict();
    }
    else {
        algo = new st2se::Algo_advanced();
    }

    st2se::Optimizer opti;

    opti.useAlgorithm(algo);

    try {
        opti.optimize(in, out);
    }
    catch (std::runtime_error &e) {
        std::cerr << e.what() << std::endl
            << "Exiting with error ..." << std::endl;
        ret_val = 1;
    }

    st2se::Generator gen;
    st2se::Output *cpp = new st2se::outputCPP();

    // initialize and configure generator
    gen.initialize(cpp);
    gen.configure(params);

    gen.generate(out);

    // remove and dealloc algorithm
    opti.useAlgorithm(nullptr);
    delete algo;

    // remove and dealloc output generator
    gen.removeOutput();
    delete cpp;

    return ret_val;

}
