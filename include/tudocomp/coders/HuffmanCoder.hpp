#pragma once

#include <bitset>
#include <numeric>

#include <tudocomp/Env.hpp>
#include <tudocomp/Coder.hpp>
#include <tudocomp/util.hpp>
#include <tudocomp/Range.hpp>
#include <tudocomp/def.hpp>

namespace tdc {

namespace huff {

    /**
     * Counts the number of different elements in a sequence
     * @param input a sequence of integer values whose maximum number should be low (at most 1<<16).
     * @return storing for each character of the full alphabet whether it exists in a given input text (value > 0 -> existing, value = 0 -> non-existing)
     */
    template<class T>
    len_t* count_alphabet(const T& input) {
        typedef typename std::make_unsigned<typename T::value_type>::type value_type;
        constexpr size_t max_literal = std::numeric_limits<value_type>::max();
        len_t* C { new len_t[max_literal+1] };
        std::memset(C, 0, sizeof(len_t)*(max_literal+1));

        for(const auto& c : input) {
            DCHECK_LT(static_cast<value_type>(c), max_literal+1);
            DCHECK_LT(C[static_cast<value_type>(c)], std::numeric_limits<len_t>::max());
            ++C[static_cast<value_type>(c)];
        }
        return C;
    }
    template<class T>
    len_t* count_alphabet_literals(T&& input) {
        len_t* C { new len_t[ULITERAL_MAX+1] };
        std::memset(C, 0, sizeof(len_t)*(ULITERAL_MAX+1));

        while(input.has_next()) {
            literal_t c = input.next().c;
            DCHECK_LT(static_cast<uliteral_t>(c), ULITERAL_MAX+1);
            DCHECK_LT(C[static_cast<uliteral_t>(c)], std::numeric_limits<len_t>::max());
            ++C[static_cast<uliteral_t>(c)];
        }
        return C;
    }
    /** Computes an array that maps from the effective alphabet to the full alphabet.
     *  @param C @see count_alphabet
     */
    inline len_t effective_alphabet_size(const len_t* C) {
        return std::count_if(C, C+ULITERAL_MAX+1, [] (const len_t& i) { return i != 0; }); // size of the effective alphabet
    }

    /**
     * Computes an array that maps from the effective alphabet to the full alphabet
     * The full alphabet is determined by the non-zero entries of the array C
     * @param C storing for each character of the full alphabet whether it exists in a given input text (value > 0 -> existing, value = 0 -> non-existing)
     * @param C @see count_alphabet
     */
    inline uliteral_t* gen_effective_alphabet(const len_t*const C, const size_t alphabet_size) {
        uliteral_t* map_from_effective { new uliteral_t[alphabet_size] };
        size_t j = 0;
        for(size_t i = 0; i <= ULITERAL_MAX; ++i) {
            if(C[i] == 0) continue;
            DCHECK_LT(j, alphabet_size);
            map_from_effective[j++] = i;
        }
        DCHECK_EQ(j, alphabet_size);
        for(size_t i = 0; i < alphabet_size; ++i) {
//          DCHECK_NE(map_from_effective[i],0);
            DCHECK_LE(map_from_effective[i], ULITERAL_MAX);
            DCHECK_NE(C[map_from_effective[i]],0);
        }
        return map_from_effective;
    }

    /**
     * Returns an array storing for each character of the effective alphabet the length of its codeword.
     * The array is sorted with respect to the rank of the alphabet character.
     * This is an implementation of Managing Gigabytes, Chapter 2.3 Huffman Coding, Second Edition, 1999
     * @param C @see count_alphabet
     * @param map_from_effective maps from the effective alphabet to the full alphabet
     * @param alphabet_size the size of the effective alphabet
     *
     **/
    inline uint8_t* gen_codelengths(const len_t*const C, const uliteral_t*const map_from_effective, const size_t alphabet_size) {
        size_t A[2*alphabet_size];
        for(size_t i=0; i < alphabet_size; i++) {
            DVLOG(2) << "Char " << map_from_effective[i] << " : " << size_t(C[map_from_effective[i]]);
            A[alphabet_size+i] = C[map_from_effective[i]];
            A[i] = alphabet_size+i;
        }
        // build a minHeap of A[0..size)
        auto comp = [&A] (const size_t a, const size_t b) -> bool { return A[a] > A[b];};
        std::make_heap(&A[0], &A[alphabet_size], comp);
        DCHECK_LE(A[A[0]], A[A[1]]);
        assert_permutation_offset<size_t*>(A,alphabet_size,alphabet_size);

        DVLOG(2) << "A: "<<arr_to_debug_string(A,2*alphabet_size);


        size_t h = alphabet_size-1;
        //builds the Huffman tree
        while(h > 0) {
            std::pop_heap(A, A+h+1, comp);
            const size_t m1 = A[h];  // first element from the heap (already popped)
            --h;

            std::pop_heap(A, A+h+1, comp);
            const size_t m2 = A[h]; //second min count
            DCHECK_GT(m1,h); DCHECK_GT(m2,h);

            A[h+1] = A[m1] + A[m2]; // create a new parent node
            A[h] = h + 1;
            A[m1] = A[m2] = h + 1; //parent pointer
            std::push_heap(A, A+h+1, comp);
        }

        A[1] = 0;
        for(size_t i = 2; i < 2*alphabet_size; ++i) {
            A[i] = A[A[i]]+ 1;
        }
        // variant:
        // for(size_t i = size; i < 2*size; ++i) {
        //  size_t d = 0;
        //  size_t r = i;
        //  while(r > 1) {
        //      ++d;
        //      r = A[r];
        //  }
        //  A[i] = d;
        // }

        uint8_t* codelengths { new uint8_t[alphabet_size] };
        for (size_t i=0; i < alphabet_size; i++) {
            DCHECK_LE(A[alphabet_size+i], 64); // the latter representation allows only codewords of length at most 64 bits
            codelengths[i] = A[alphabet_size+i];
            DVLOG(2) << "Char " << map_from_effective[i] << " : " << codelengths[i];
        }

        IF_PARANOID({
            // invariants

            // check that more frequent keywords get shorter codelengthss
            for(size_t i=0; i < alphabet_size; ++i) {
                for(size_t j=i+1; j < alphabet_size; ++j) {
                    if(codelengths[i] > codelengths[j]) {
                        DCHECK_LE(C[map_from_effective[i]], C[map_from_effective[j]]);
                    }
                    else if(codelengths[i] < codelengths[j]) {
                        DCHECK_GE(C[map_from_effective[i]], C[map_from_effective[j]]);
                    }
                }}
            { // check Kraft's equality
                const size_t& max_el = *std::max_element(A+alphabet_size,A+2*alphabet_size);
                DCHECK_LT(max_el,63);
                size_t sum = 0;
                for (size_t i = 0; i < alphabet_size; ++i)
                {
                    sum += 2ULL<<(max_el - A[alphabet_size+i]);
                }
                DCHECK_EQ(sum, 2ULL<<max_el);
            }
        })

        return codelengths;
    }

    /** Generates the numl array (@see huffmantable). This function is called before decoding Huffman-encoded text.
     */
    inline uliteral_t* gen_numl(const uint8_t*const ordered_codelengths, const size_t alphabet_size, const uint8_t longest) {
        DCHECK_EQ(longest, *std::max_element(ordered_codelengths, ordered_codelengths+alphabet_size));
        DCHECK_GT(longest,0);

        // numl : length l -> #codewords of length l
        uliteral_t* numl = new uliteral_t[longest];
        std::memset(numl,0,sizeof(uliteral_t)*longest);

        for (size_t i = 0; i < alphabet_size; ++i) {
            DCHECK_LE(ordered_codelengths[i], longest);
            DCHECK_GT(ordered_codelengths[i], 0);
            ++numl[ordered_codelengths[i]-1];
        }
        return numl;
    }

    /**
     * The returned array stores for each codeword length the smallest codeword of the respective length.
     */
    inline size_t* gen_first_codes(const uliteral_t*const numl, const size_t longest) {
        size_t* firstcode = new size_t[longest];
        firstcode[longest-1] = 0;
        for(size_t i = longest-1; i > 0; --i)
            firstcode[i-1] = (firstcode[i] + numl[i])/2;
        return firstcode;
    }

    /** Generates all codewords. Called before encoding a text.
     */
    inline size_t* gen_codewords(const uint8_t*const ordered_codelengths, const size_t alphabet_size, const uliteral_t*const numl, const uint8_t longest) {
        DCHECK_EQ(longest, *std::max_element(ordered_codelengths, ordered_codelengths+alphabet_size));
        DCHECK_GT(longest,0);

        //firstcode stores the code of the first character with the specific length. It is then incremented for the later characters of the same length
        size_t*const firstcode = gen_first_codes(numl, longest);

        size_t*const codewords = new size_t[alphabet_size];
        for(size_t i = 0; i < alphabet_size; ++i) {
            DCHECK_LE(ordered_codelengths[i], longest);
            DCHECK_GT(ordered_codelengths[i], 0);
            codewords[i] = firstcode[ordered_codelengths[i]-1]++;
            DVLOG(2) << "codeword " << i << " : " << std::bitset<64>(codewords[i]) << ", length " << ordered_codelengths[i];
        }
        delete [] firstcode;
        return codewords;
    }

    struct huffmantable {
        const uliteral_t* ordered_map_from_effective; //! stores a map from the effective alphabet to the full alphabet, sorted by the length of the codewords
        const size_t alphabet_size; //! stores the size of the effective alphabet

        /** Given a codelength l, nums returns the number of codewords with the given length.
         * numl starts with index 0, i.e., numl[l] returns the codewords with length l+1 !
         */
        const uliteral_t*const numl;
        const uint8_t longest; //! how long is the longest codeword?

        ~huffmantable() { //! all members of the huffmantable are created dynamically
            if(ordered_map_from_effective != nullptr) delete [] ordered_map_from_effective;
            if(numl != nullptr) delete [] numl;
        }
    };

    /** Stores more data than huffmantable.
     * The additional data can be inferred with the data already stored in huffmantable.
     * During the generation of the Huffman code, these additional data is computed as a by-product,
     * and needed for encoding the text.
     */
    struct extended_huffmantable : public huffmantable {
        extended_huffmantable(
                const uliteral_t*const _ordered_map_from_effective,
                const size_t*const _codewords,
                const uint8_t*const _ordered_codelengths,
                const size_t _alphabet_size,
                const uliteral_t*const _numl,
                const uint8_t _longest)
            : huffmantable{_ordered_map_from_effective,_alphabet_size, _numl, _longest},
            codewords(_codewords),
            ordered_codelengths(_ordered_codelengths)
            {}
        const size_t*const codewords; //! the codeword of each character of the effective alphabet
        const uint8_t*const ordered_codelengths; //! stores the codelength of the codeword of each character of the effective alphabet, sorted by the length of the codewords
        ~extended_huffmantable() { //! all members of the huffmantable are created dynamically
            if(codewords != nullptr) delete [] codewords;
            if(ordered_codelengths != nullptr) delete [] ordered_codelengths;
        }
    };

    /**
     * Encodes the Huffman table needed to decode Huffman-encoded text.
     */
    inline void huffmantable_encode(tdc::io::BitOStream& os, const huffmantable& table) {
        os.write_compressed_int(table.longest);
        for(size_t i = 0; i < table.longest; ++i) {
            os.write_compressed_int(table.numl[i]);
        }
        os.write_compressed_int(table.alphabet_size);
        for(size_t i = 0; i < table.alphabet_size; ++i) {
            os.write_int<uliteral_t>(table.ordered_map_from_effective[i]);
        }
    }

    /**
     * Decodes the Huffman table needed to decode Huffman-encoded text.
     */
    inline huffmantable huffmantable_decode(tdc::io::BitIStream& in) {
        const uint8_t longest = in.read_compressed_int<uint8_t>();
        uliteral_t*const numl { new uliteral_t[longest] };
        for(size_t i = 0; i < longest; ++i) {
            numl[i] = in.read_compressed_int<uliteral_t>();
        }
        const size_t alphabet_size = in.read_compressed_int<size_t>();
        uint8_t*const ordered_map_from_effective { new uint8_t[alphabet_size] };
        for(size_t i = 0; i < alphabet_size; ++i) {
            ordered_map_from_effective[i] = in.read_int<uliteral_t>();
        }
        return { ordered_map_from_effective, alphabet_size, numl, longest };
    }

    /** maps from the full alphabet to the effective alphabet
     */
    inline uint8_t* gen_ordered_map_to_effective(const uint8_t*const ordered_map_from_effective, const size_t alphabet_size) {
            uint8_t* map_to_effective = new uint8_t[ULITERAL_MAX];
            std::memset(map_to_effective, 0xff, sizeof(map_to_effective)*sizeof(uint8_t));
            for(size_t i = 0; i < alphabet_size; ++i) {
                map_to_effective[ordered_map_from_effective[i]] = i;
            }
            DVLOG(2) << "ordered_map_from_effective : " << arr_to_debug_string(ordered_map_from_effective, alphabet_size);
            DVLOG(2) << "map_to_effective : " << arr_to_debug_string(map_to_effective, ULITERAL_MAX);
            return map_to_effective;
    }


    /**
     * Encodes a single literal
     */
    inline void huffman_encode(
            uliteral_t input,
            tdc::io::BitOStream& os,
            const uint8_t*const ordered_codelengths,
            const uint8_t*const ordered_map_to_effective,
            const size_t alphabet_size,
            const size_t*const codewords
            ) {
        const size_t char_value = static_cast<uliteral_t>(input);
        const uint8_t& effective_char = ordered_map_to_effective[char_value];
        DCHECK_LT(effective_char, alphabet_size);
        os.write_int(codewords[effective_char], ordered_codelengths[effective_char]);
        DVLOG(2) << " codeword " << codewords[effective_char] << " length " << int(ordered_codelengths[effective_char]);
    }


    /**
     * Encodes a stream storing input_length characters
     */
    inline void huffman_encode(
            std::basic_istream<literal_t>& input,
            tdc::io::BitOStream& os,
            const size_t input_length,
            const uint8_t*const ordered_map_from_effective,
            const uint8_t*const ordered_codelengths,
            const size_t alphabet_size,
            const size_t*const codewords
            ) {
        DCHECK_GT(input_length, 0);

            const uint8_t*const ordered_map_to_effective { gen_ordered_map_to_effective(ordered_map_from_effective, alphabet_size) };

            {//now writing
                os.write_compressed_int<size_t>(input_length);
                literal_t c;
                while(input.get(c)) {
                    huffman_encode(c, os, ordered_codelengths, ordered_map_to_effective, alphabet_size, codewords);
                }
            }
            delete [] ordered_map_to_effective;
    }


    /**
     * prefix_sum_lengths stores for each different codeword length the first entry of the codeword with this length in codewords
     * Needed for decoding Huffman code
     */
    inline size_t* gen_prefix_sum_lengths(
            const uint8_t*const ordered_codelengths,
            const size_t alphabet_size,
            const uint8_t longest) {
            size_t*const prefix_sum_lengths = new size_t[longest];
#ifndef NDDEBUG
            std::fill(prefix_sum_lengths,prefix_sum_lengths+longest,std::numeric_limits<size_t>::max());
#endif
            prefix_sum_lengths[ordered_codelengths[0]-1] = 0;
            for(size_t i = 1; i < alphabet_size; ++i) {
                if(ordered_codelengths[i-1] < ordered_codelengths[i])
                    prefix_sum_lengths[ordered_codelengths[i]-1] = i;
            }
            DVLOG(2) << "ordered_codelengths : " << arr_to_debug_string(ordered_codelengths, alphabet_size);
            DVLOG(2) << "prefix_sum_lengths : " << arr_to_debug_string(prefix_sum_lengths, longest);
            return prefix_sum_lengths;
    }
    inline literal_t huffman_decode(
            tdc::io::BitIStream& is,
            const uliteral_t*const ordered_map_from_effective,
            const size_t*const prefix_sum_lengths,
            const size_t*const firstcodes
            ) {
        DCHECK(!is.eof());
        size_t value = 0;
        uint8_t length = 0;
        do {
            DCHECK(!is.eof());
            value = (value<<1) + is.read_bit();
            ++length;
        } while(value < firstcodes[length-1]);
        DVLOG(2) << " codeword " << value << " length " << length;
        --length;
//      DCHECK_LT(prefix_sum_lengths[length]+ (value - firstcodes[length]), alphabet_size);
        return ordered_map_from_effective[prefix_sum_lengths[length]+ (value - firstcodes[length]) ];


    }


    inline void huffman_decode(
            tdc::io::BitIStream& is,
            std::basic_ostream<literal_t>& output,
            const uliteral_t*const ordered_map_from_effective,
            const uint8_t*const ordered_codelengths,
            const size_t alphabet_size,
            const uliteral_t*const numl,
            const uint8_t longest) {

            const size_t*const prefix_sum_lengths { gen_prefix_sum_lengths(ordered_codelengths, alphabet_size, longest) };

            const size_t text_length = is.read_compressed_int<size_t>();
            DCHECK_GT(text_length, 0);
            const size_t*const firstcodes = gen_first_codes(numl, longest);
            DVLOG(2) << "firstcodes : " << arr_to_debug_string(firstcodes, longest);
            size_t num_chars_read = 0;
            while(true) {
                output << huffman_decode(is, ordered_map_from_effective, prefix_sum_lengths, firstcodes);
                ++num_chars_read;
                if(num_chars_read == text_length) break;
            }
            delete [] firstcodes;
    }

    /** Computes the lengths of all codewords of the Huffman code. Needed to decode a Huffman-encoded text.
     */
    inline uint8_t* gen_ordered_codelength(const size_t alphabet_size, const uliteral_t*const numl, const size_t longest) {
        uint8_t* ordered_codelengths { new uint8_t[alphabet_size] };
        for(size_t i = 0,k=0; i < longest; ++i) {
            for(size_t j = 0; j < numl[i]; ++j) {
                DCHECK_LT(k, alphabet_size);
                ordered_codelengths[k++] = i+1;
            }
        }
        return ordered_codelengths;
    }

    /** Generates the Huffman table based on some input text
     * @param C @see count_alphabet
     * @attention Deletes the input array C!
     * @attention C must contain at least two non-zero values
     */
    inline extended_huffmantable gen_huffmantable(const len_t*const C) {
        const size_t alphabet_size = effective_alphabet_size(C);
        DCHECK_GT(alphabet_size,0);

        // mapFromEffective : rank of an effective alphabet character -> input alphabet (char-range)
        const uliteral_t*const mapFromEffective = gen_effective_alphabet(C, alphabet_size);

        const uint8_t*const codelengths = gen_codelengths(C, mapFromEffective, alphabet_size);
        delete [] C;

        // codeword_order is a permutation (like suffix array) sorting (code_length, mapFromEffective) by code_length ascendingly (instead of mapFromEffective values)
        size_t*const codeword_order = new size_t[alphabet_size];
        std::iota(codeword_order,codeword_order+alphabet_size,0);
        std::sort(codeword_order,codeword_order+alphabet_size, [&] (const uliteral_t& i, const uliteral_t& j) { return codelengths[i] < codelengths[j]; });

        const uint8_t longest = *std::max_element(codelengths, codelengths+alphabet_size);

        // the ordered variants are all sorted by the codelengths, (instead of by character values)
        uint8_t* ordered_codelengths { new uint8_t[alphabet_size] };
        uliteral_t*const ordered_map_from_effective { new uliteral_t[alphabet_size] };
        for(size_t i = 0; i < alphabet_size; ++i) {
            ordered_codelengths[i] = codelengths[codeword_order[i]];
            ordered_map_from_effective[i] = mapFromEffective[codeword_order[i]];
        }
        delete [] codelengths;
        delete [] mapFromEffective;
        delete [] codeword_order;

        const uliteral_t*const numl = gen_numl(ordered_codelengths, alphabet_size, longest);
        const size_t*const codewords = gen_codewords(ordered_codelengths, alphabet_size, numl, longest);

        return { ordered_map_from_effective, codewords, ordered_codelengths, alphabet_size, numl, longest };
    }

    inline extended_huffmantable gen_huffmantable(const std::string& text) {
        const len_t*const C { count_alphabet(text) };
        return gen_huffmantable(C);
    }

    inline void encode(tdc::io::Input& input, tdc::io::Output& output) {
        tdc::io::BitOStream bit_os{output};
        View iview = input.as_view();
        const len_t*const C { count_alphabet(iview) };
        extended_huffmantable table = gen_huffmantable(C);
        huffmantable_encode(bit_os, table);
        io::ViewStream view_stream(iview);
        auto& is = view_stream.stream();
        huffman_encode(is, bit_os, iview.size(), table.ordered_map_from_effective, table.ordered_codelengths, table.alphabet_size, table.codewords);
    }

    inline void decode(tdc::io::Input& input, tdc::io::Output& output) {
        tdc::io::BitIStream bit_is{input};
        huffmantable table = huffmantable_decode(bit_is);
        const uint8_t*const ordered_codelengths = gen_ordered_codelength(table.alphabet_size, table.numl, table.longest);
        auto os = output.as_stream();
        huffman_decode(
                bit_is,
                os,
                table.ordered_map_from_effective,
                ordered_codelengths,
                table.alphabet_size,
                table.numl,
                table.longest);

        delete [] ordered_codelengths;
    }

}//ns


class HuffmanCoder : public Algorithm {
public:
    inline static Meta meta() {
        Meta m("coder", "huff", "Canonical Huffman Coder");
        return m;
    }

    HuffmanCoder() = delete;

    class Encoder : public tdc::Encoder {
    const huff::extended_huffmantable m_table;
    const uint8_t*const ordered_map_to_effective;
    public:
        template<typename literals_t>
        inline Encoder(Env&& env, std::shared_ptr<BitOStream> out, literals_t&& literals)
            : tdc::Encoder(std::move(env), out, literals),
            m_table{ [&] () {
                if(tdc_likely(!literals.has_next())) return huff::extended_huffmantable { nullptr, nullptr, nullptr, 0, nullptr, 0 };
                const len_t*const C = huff::count_alphabet_literals(std::move(literals));
                const len_t alphabet_size = huff::effective_alphabet_size(C);
                if(tdc_unlikely(alphabet_size == 1)) {
                    delete [] C;
                    return huff::extended_huffmantable { nullptr, nullptr, nullptr, 1, nullptr, 0 };
                }
                return huff::gen_huffmantable(C);
            }() }
            , ordered_map_to_effective { m_table.codewords == nullptr ? nullptr : huff::gen_ordered_map_to_effective(m_table.ordered_map_from_effective, m_table.alphabet_size) }
        {
            if(tdc_unlikely(m_table.alphabet_size <= 1)) {
                m_out->write_bit(0);
            }
            else {
                m_out->write_bit(1);
                huff::huffmantable_encode(*m_out, m_table);
            }
        }

        ~Encoder() {
            if(tdc_likely(ordered_map_to_effective != nullptr)) {
                delete [] ordered_map_to_effective;
            }
        }

        template<typename literals_t>
        inline Encoder(Env&& env, Output& out, literals_t&& literals)
            : Encoder(std::move(env), std::make_shared<BitOStream>(out), literals) {
        }

        using tdc::Encoder::encode; // default encoding as fallback

        template<typename value_t>
        inline void encode(value_t v, const LiteralRange&) {
            DCHECK_NE(m_table.alphabet_size,0);
            if(tdc_unlikely(m_table.alphabet_size == 1))
                m_out->write_int(static_cast<uliteral_t>(v),8*sizeof(uliteral_t));
            else
                huff::huffman_encode(v, *m_out, m_table.ordered_codelengths, ordered_map_to_effective, m_table.alphabet_size, m_table.codewords);
        }
    };

    class Decoder : public tdc::Decoder {
        const uliteral_t* ordered_map_from_effective;
        const size_t* prefix_sum_lengths;
        const size_t* firstcodes;
    public:
        ~Decoder() {
            if(tdc_likely(ordered_map_from_effective != nullptr)) {
                delete [] ordered_map_from_effective;
                delete [] prefix_sum_lengths;
                delete [] firstcodes;
            }
        }

        inline Decoder(Env&& env, std::shared_ptr<BitIStream> in)
            : tdc::Decoder(std::move(env), in) {

            if(tdc_unlikely(!m_in->read_bit())) {
                ordered_map_from_effective = nullptr;
                return;
            }
            huff::huffmantable table(huff::huffmantable_decode(*m_in) );
            ordered_map_from_effective = table.ordered_map_from_effective;
            table.ordered_map_from_effective = nullptr;
            const uint8_t*const ordered_codelengths { huff::gen_ordered_codelength(table.alphabet_size, table.numl, table.longest) };
            prefix_sum_lengths = huff::gen_prefix_sum_lengths(ordered_codelengths, table.alphabet_size, table.longest);
            delete [] ordered_codelengths;
            firstcodes = huff::gen_first_codes(table.numl, table.longest);
        }

        inline Decoder(Env&& env, Input& in)
            : Decoder(std::move(env), std::make_shared<BitIStream>(in)) {
        }

        using tdc::Decoder::decode; // default decoding as fallback

        template<typename value_t>
        inline value_t decode(const LiteralRange&) {
            if(tdc_unlikely(ordered_map_from_effective == nullptr))
                return m_in->read_int<uliteral_t>();
            return huff::huffman_decode(*m_in, ordered_map_from_effective, prefix_sum_lengths, firstcodes);
        }
    };
};


}//ns

