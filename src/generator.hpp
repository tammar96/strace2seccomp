#ifndef SRC_GENERATOR_HPP
#define SRC_GENERATOR_HPP

#include "ids.hpp"
#include "outputCPP.hpp"

namespace st2se {

    enum class language {
        CPP,
        GO
    };

    class Generator {
        Output *gnrt {nullptr};
      public:
        Generator() = default;
        Generator(Output *_out);
        bool initialize(Output *_out);
        bool generate(Ids &ids);
        bool isInitialized();
        void removeOutput();
        void threadSupport(bool b);
        void genFuncProlog(bool b);
        void setOutput(std::string o);


    };
} // namespace st2se


#endif