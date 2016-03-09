#ifndef _INCLUDED_ONLINE_COMPRESSOR_HPP
#define _INCLUDED_ONLINE_COMPRESSOR_HPP

#include <tudocomp/Compressor.hpp>

namespace tudocomp {

template<typename F, typename A, typename C>
class OnlineCompressor : public Compressor {

private:

public:
    /// Class needs to be constructed with an `Env&` argument.
    inline OnlineCompressor() = delete;

    /// Construct the class with an environment.
    inline OnlineCompressor(Env& env): Compressor(env) {}
    
    /// Compress `inp` into `out`.
    ///
    /// \param inp The input stream.
    /// \param out The output stream.
    inline virtual void compress(Input& input, Output& output) override final {
        
        //Read input into SDSL int vector buffer
        size_t len = input.size(); //TODO check has_size first
        
        DLOG(INFO) << "Init (n = " << len << ")...";
        
        //Encode
        DLOG(INFO) << "Init...";
        
        auto out_guard = output.as_stream();
        BitOStream out_bits(*out_guard);
        
        //TODO: write magic (ID for A and C)
        //Write input text length
        out_bits.write_compressed_int(len);
        
        {
            //Init coders
            A alphabet_coder(*m_env, out_bits);
            C factor_coder(*m_env, out_bits, len);
            
            //Factorize and encode
            DLOG(INFO) << "Factorize / encode...";
            F::factorize(*m_env, input, alphabet_coder, factor_coder);
        }
        
        //Done
        out_bits.flush();
        DLOG(INFO) << "Done.";
    }

    /// Decompress `inp` into `out`.
    ///
    /// \param inp The input stream.
    /// \param out The output stream.
    virtual void decompress(Input& input, Output& output) override final {
        // TODO
    }

};

}

#endif
