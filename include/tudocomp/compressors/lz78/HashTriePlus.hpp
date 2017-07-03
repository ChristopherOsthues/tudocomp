#pragma once

#include <tudocomp/Algorithm.hpp>
#include <tudocomp/util/Hash.hpp>
#include <tudocomp/compressors/lz78/LZ78Trie.hpp>
#include <tudocomp/compressors/lz78/squeeze_node.hpp>

namespace tdc {
namespace lz78 {


template<class HashFunction = MixHasher, class HashManager = SizeManagerDirect>
class HashTriePlus : public Algorithm, public LZ78Trie<factorid_t> {
	HashMap<squeeze_node_t,factorid_t,undef_id,HashFunction,std::equal_to<squeeze_node_t>,LinearProber,SizeManagerPow2> m_table;
	HashMap<squeeze_node_t,factorid_t,undef_id,HashFunction,std::equal_to<squeeze_node_t>,LinearProber,HashManager> m_table2;

public:
    inline static Meta meta() {
        Meta m("lz78trie", "hash_plus", "Hash Trie+");
		m.option("hash_function").templated<HashFunction, MixHasher>("hash_function");
		m.option("hash_manager").templated<HashManager, SizeManagerDirect>("hash_manager");
        m.option("load_factor").dynamic(30);
		return m;
	}

    HashTriePlus(Env&& env, const size_t n, const size_t& remaining_characters, factorid_t reserve = 0)
		: Algorithm(std::move(env))
		, LZ78Trie(n,remaining_characters)
        , m_table(this->env(),n,remaining_characters)
        , m_table2(this->env(),n,remaining_characters)
	{
        m_table.max_load_factor(this->env().option("load_factor").as_integer()/100.0f );
        m_table2.max_load_factor(0.95);
		if(reserve > 0) {
			m_table.reserve(reserve);
		}
    }

    IF_STATS(
        MoveGuard m_guard;
        ~HashTriePlus() {
            if (m_guard) {
                if(m_table2.empty()) {
                    m_table.collect_stats(env());
                } else {
                    m_table2.collect_stats(env());
                }
            }
        }
    )
    HashTriePlus(HashTriePlus&& other) = default;
    HashTriePlus& operator=(HashTriePlus&& other) = default;

	node_t add_rootnode(uliteral_t c) override {
		DCHECK(m_table2.empty());
		m_table.insert(std::make_pair<squeeze_node_t,factorid_t>(create_node(0, c), size()));
		return size() - 1;
	}

    node_t get_rootnode(uliteral_t c) override {
        return c;
    }

	void clear() override {
//		table.clear();

	}

    node_t find_or_insert(const node_t& parent_w, uliteral_t c) override {
        auto parent = parent_w.id();
        const factorid_t newleaf_id = size(); //! if we add a new node, its index will be equal to the current size of the dictionary
		if(!m_table2.empty()) { // already using the second hash table
			auto ret = m_table2.insert(std::make_pair(create_node(parent,c), newleaf_id));
			if(ret.second) {
				return undef_id; // added a new node
			}
			return ret.first.value();
		}
		// using still the first hash table
		auto ret = m_table.insert(std::make_pair(create_node(parent,c), newleaf_id));
		if(ret.second) {
			if(tdc_unlikely(m_table.table_size()*m_table.max_load_factor() < m_table.m_entries+1)) {
				const size_t expected_size = (m_table.m_entries + 1 + lz78_expected_number_of_remaining_elements(m_table.entries(),m_table.m_n,m_table.m_remaining_characters))/0.95;
				if(expected_size < m_table.table_size()*2.0*0.95) {
					m_table2.incorporate(m_table, expected_size);
				}

			}
			return undef_id; // added a new node
		}
		return ret.first.value();
    }

    factorid_t size() const override {
        return m_table2.empty() ? m_table.entries() : m_table2.entries();
    }
};

}} //ns

