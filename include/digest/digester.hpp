#ifndef DIGESTER_HPP
#define DIGESTER_HPP

#include <cstdint>
#include <deque>
#include <nthash/kmer.hpp>
#include <nthash/nthash.hpp>

/**
 * @brief digest code.
 */
namespace digest {

/**
 * @brief Exception thrown when initializing a Digister with k (kmer size) < 4
 * and start (starting index) < len (length of sequence).
 *
 *
 */
class BadConstructionException : public std::exception {
	const char *what() const throw() {
		return "k must be greater than 3, start must be less than len";
	}
};

/**
 * @brief Exception thrown when append_seq() is called before all kmers/large
 * windows in the current sequence have been iterated over
 *
 */
class NotRolledTillEndException : public std::exception {
	const char *what() const throw() {
		return "Iterator must be at the end of the current sequence before "
			   "appending a new one.";
	}
};

/**
 * @brief Enum values for the type of hash to minimize
 */
enum class MinimizedHashType {
	/** minimize the canonical hash */
	CANON,
	/** minimize the forward hash */
	FORWARD,
	/** minimize the reverse hash */
	REVERSE
};

/**
 * @brief Specifies behavior with non-ACTG characters.
 */
enum class BadCharPolicy {
	/** The WRITEOVER policy specifies that any non-ACTG character is simply
	   replaced with an A. */
	WRITEOVER,
	/** The SKIPOVER policy skips over any kmers with a non-ACTG character.
	 *
	 * For example, if you have k = 4 and your sequence is ACTGNNACTGAC, then
	 * the only kmers that would be considered would be the ACTG starting at
	 * index 0, the ACTG starting at index 6, CTGA at index 7, and TGAC at
	 * index 8. Then if you had a large window of 4 (kmers), then the smallest
	 * would be picked from one of those 4.
	 */
	SKIPOVER
};

/**
 * @brief an abstract class for Digester objects.
 *
 * @tparam a BadCharPolicy enum value. The policy to adopt when handling
 * non-ACTG characters.
 *
 */
template <BadCharPolicy P> class Digester {
  public:
	/**
	 * @param seq const char pointer pointing to the c-string of DNA sequence to
	 * be hashed.
	 * @param len length of seq.
	 * @param k kmer size.
	 * @param start 0-indexed position in seq to start hashing from.
	 * @param minimized_h whether we are minimizing the canonical, forward, or
	 * reverse hash
	 *
	 * @throws BadConstructionException Thrown if k is less than 4,
	 * or if the starting position is after the end of the string
	 */
	Digester(const char *seq, size_t len, unsigned k, size_t start = 0,
			 MinimizedHashType minimized_h = MinimizedHashType::CANON)
		: seq(seq), len(len), offset(0), start(start), end(start + k), chash(0),
		  fhash(0), rhash(0), k(k), minimized_h(minimized_h) {
		if (k < 4 or start >= len or (int) minimized_h > 2) {
			throw BadConstructionException();
		}
		init_hash();
	}

	/**
	 * @param seq const string of the DNA sequence to be hashed.
	 * @param k kmer size.
	 * @param start 0-indexed position in seq to start hashing from.
	 * @param minimized_h whether we are minimizing the canonical, forward, or
	 * reverse hash
	 *
	 * @throws BadConstructionException Thrown if k is less than 4,
	 * or if the starting position is after the end of the string
	 */
	Digester(const std::string &seq, unsigned k, size_t start = 0,
			 MinimizedHashType minimized_h = MinimizedHashType::CANON)
		: Digester(seq.c_str(), seq.size(), k, start, minimized_h) {}

	virtual ~Digester() = default;

	/**
	 * @return bool, true if values of the 3 hashes are meaningful, false
	 * otherwise, i.e. the object wasn't able to initialize with a valid hash or
	 * roll_one() was called when already at end of sequence
	 */
	bool get_is_valid_hash() { return is_valid_hash; }

	/**
	 * @return unsigned, the value of k (kmer size)
	 */
	unsigned get_k() { return k; }

	/**
	 * @return size_t, the length of the sequence
	 */
	size_t get_len() { return len; }

	/**
	 * @brief moves the internal pointer to the next valid k-mer. <br>
	 * Time Complexity: O(1)
	 *
	 * @return bool, true if we were able generate a valid hash, false otherwise
	 */
	bool roll_one() {
		if (P == BadCharPolicy::SKIPOVER) {
			return roll_one_skip_over();
		} else {
			return roll_one_write_over();
		}
	};

	/**
	 * @brief gets the positions, as defined by get_pos(), of minimizers up to
	 * the amount specified
	 *
	 * @param amount number of minimizers you want to generate
	 * @param vec a reference to a vector of uint32_t, the positions returned
	 * will go there
	 */
	virtual void roll_minimizer(unsigned amount,
								std::vector<uint32_t> &vec) = 0;

	/**
	 * @brief gets the positions (pair.first), as defined by get_pos(), and the
	 * hashes (pair.second) of minimizers up to the amount specified
	 *
	 * @param amount number of minimizers you want to generate
	 * @param vec a reference to a vector of a pair of uint32_t, the positions
	 * and hashes returned will go there
	 */
	virtual void
	roll_minimizer(unsigned amount,
				   std::vector<std::pair<uint32_t, uint32_t>> &vec) = 0;

	/**
	 * @return current index of the first character of the current kmer that has
	 * been hashed. Sequences that have been appended onto each other count as 1
	 * big sequence, i.e. if you first had a sequence of length 10 and then
	 * appended another sequence of length 20, and the index of the first
	 * character of the current k-mer is at index 4, 0-indexed, in the second
	 * sequence, then it will return 14
	 */
	size_t get_pos() { return offset + start - c_outs.size(); }

	/**
	 * @return uint64_t, the canonical hash of the kmer that was rolled over
	 * when roll_one was last called (roll_minimizer() calls roll_one()
	 * internally).
	 */
	uint64_t get_chash() { return chash; }

	/**
	 * @return uint64_t, the forward hash of the kmer that was rolled over when
	 * roll_one was last called (roll_minimizer() calls roll_one() internally).
	 */
	uint64_t get_fhash() { return fhash; }

	/**
	 * @return uint64_t, the reverse hash of the kmer that was rolled over when
	 * roll_one was last called (roll_minimizer() calls roll_one() internally).
	 */
	uint64_t get_rhash() { return rhash; }

	/**
	 * @brief replaces the current sequence with the new one. It's like starting
	 * over with a completely new seqeunce
	 *
	 * @param seq const char pointer to new sequence to be hashed
	 * @param len length of the new sequence
	 * @param start position in new sequence to start from
	 *
	 * @throws BadConstructionException thrown if the starting position is
	 * greater than the length of the string
	 */
	virtual void new_seq(const char *seq, size_t len, size_t start) {
		this->seq = seq;
		this->len = len;
		this->offset = 0;
		this->start = start;
		this->end = start + this->k;
		is_valid_hash = false;
		if (start >= len) {
			throw BadConstructionException();
		}
		init_hash();
	}

	/**
	 * @brief replaces the current sequence with the new one. It's like starting
	 * over with a completely new sequence
	 *
	 * @param seq const std string reference to the new sequence to be hashed
	 * @param start position in new sequence to start from
	 *
	 * @throws BadConstructionException thrown if the starting position is
	 * greater than the length of the string
	 */
	virtual void new_seq(const std::string &seq, size_t pos) {
		new_seq(seq.c_str(), seq.size(), pos);
	}

	/**
	 * @brief simulates the appending of a new sequence to the end of the old
	 * sequence. The old sequence will no longer be stored, but the rolling
	 * hashes will be able to preceed as if the sequences were appended. Can
	 * only be called when you've reached the end of the current sequence i.e.
	 * if you're current sequence is ACTGAC, and you have reached the end of
	 * this sequence, and you call append_seq with the sequence CCGGCCGG, then
	 * the minimizers you will get after calling append_seq plus the minimizers
	 * you got from going through ACTGAC, will be equivalent to the minimizers
	 * you would have gotten from rolling across ACTGACCCGGCCGG
	 *
	 * @param seq const C string of DNA sequence to be appended
	 * @param len length of the sequence
	 *
	 * @throws NotRolledTillEndException Thrown when the internal iterator is
	 * not at the end of the current sequence
	 */
	void append_seq(const char *seq, size_t len) {
		if (P == BadCharPolicy::SKIPOVER) {
			append_seq_skip_over(seq, len);
		} else {
			append_seq_write_over(seq, len);
		}
	}

	/**
	 * @brief simulates the appending of a new sequence to the end of the old
	 * sequence. The old sequence will no longer be stored, but the rolling
	 * hashes will be able to preceed as if the sequences were appended. Can
	 * only be called when you've reached the end of the current sequence i.e.
	 * if you're current sequence is ACTGAC, and you have reached the end of
	 * this sequence, and you call append_seq with the sequence CCGGCCGG, then
	 * the minimizers you will get after calling append_seq plus the minimizers
	 * you got from going through ACTGAC, will be equivalent to the minimizers
	 * you would have gotten from rolling across ACTGACCCGGCCGG
	 *
	 * @param seq const std string of DNA sequence to be appended
	 *
	 * @throws NotRolledTillEndException Thrown when the internal iterator is
	 * not at the end of the current sequence
	 */
	void append_seq(const std::string &seq) {
		if (P == BadCharPolicy::SKIPOVER) {
			append_seq_skip_over(seq.c_str(), seq.size());
		} else {
			append_seq_write_over(seq.c_str(), seq.size());
		}
	}

	/**
	 * @return unsigned, a number representing the hash you are minimizing, 0
	 * for canoncial, 1 for forward, 2 for reverse
	 */
	MinimizedHashType get_minimized_h() { return minimized_h; }

	/**
	 * @return const char* representation of the sequence
	 */
	const char *get_sequence() { return seq; }

  protected:
	// 0x41 = 'A', 0x43 = 'C', 0x47 = 'G' 0x54 = 'T'
	// 0x61 = 'a', 0x63 = 'c', 0x67 = 'g' 0x74 = 't'
	std::array<bool, 256> actg{
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // NOLINT
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // NOLINT
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // NOLINT
		0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, // NOLINT
		0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // NOLINT
		0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, // NOLINT
		0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // NOLINT
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // NOLINT
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // NOLINT
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // NOLINT
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // NOLINT
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // NOLINT
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // NOLINT
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // NOLINT
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // NOLINT
	};

	/**
	 * @internal
	 * Helper function
	 *
	 * @param in char to be checked
	 *
	 * @return bool, true if in is an upper or lowercase ACTG character, false
	 * otherwise
	 */
	bool is_ACTG(char in) { return actg[in]; }

	/**
	 * @internal
	 *
	 * @brief Helper function that initializes the hash values at the first
	 * valid k-mer at or after start Sets is_valid_hash to be equal to its
	 * return value
	 *
	 * @return bool, true on success, a valid hash is initialized, false
	 * otherwise
	 */
	bool init_hash() {
		if (P == BadCharPolicy::SKIPOVER) {
			return init_hash_skip_over();
		} else {
			return init_hash_write_over();
		}
	};

	void append_seq_skip_over(const char *seq, size_t len) {
		if (end < this->len) {
			throw NotRolledTillEndException();
		}
		offset += this->len;
		size_t ind = this->len - 1;

		/*
			this is for the case where we call append_seq after having
		   previously called append_seq and not having gotten through the deque
			In such a case, since append_seq initializes a hash, we need to get
		   rid of the first character in the deque since if we just initialized
		   the hash without doing this, it would be identical the the current
		   hash held by the object

			However, there is also the case that a hash was never previously
		   initialized, such as when the length of the string used in the
		   previous append_seq call, plus the the amount of ACTG characters
		   after the last non-ACTG character in the original string summed to be
		   less than k In this case, it would not be correct to remove the first
		   character in the deque
		*/
		if ((start != end || c_outs.size() == k) && c_outs.size() > 0) {
			c_outs.pop_front();
		}

		// the following copies in characters from the end of the old sequence
		// into the deque
		std::vector<char> temp_vec;
		while (temp_vec.size() + c_outs.size() < k - 1 && ind >= start) {
			if (!is_ACTG(this->seq[ind]))
				break;

			temp_vec.push_back(this->seq[ind]);
			if (ind == 0)
				break;

			ind--;
		}
		for (std::vector<char>::reverse_iterator rit = temp_vec.rbegin();
			 rit != temp_vec.rend(); rit++) {
			c_outs.push_back(*rit);
		}

		// the following copies in characters from the front of the new sequence
		// if there weren't enough non-ACTG characters at the end of the old
		// sequence
		ind = 0;
		start = 0;
		end = 0;
		while (c_outs.size() < k && ind < len) {
			if (!is_ACTG(seq[ind])) {
				start = ind + 1;
				end = start + k;
				this->seq = seq;
				this->len = len;
				c_outs.clear();
				init_hash();
				break;
			}
			c_outs.push_back(seq[ind]);
			ind++;
			start++;
			end++;
		}

		// the following initializes a hash if we managed to fill the deque
		if (c_outs.size() == k) {
			std::string temp(c_outs.begin(), c_outs.end());
			// nthash::ntc64(temp.c_str(), k, fhash, rhash, chash,
			// locn_useless);
			fhash = base_forward_hash(temp.c_str(), k);
			rhash = base_reverse_hash(temp.c_str(), k);
			chash = nthash::canonical(fhash, rhash);
			is_valid_hash = true;
		}
		this->seq = seq;
		this->len = len;
	}

	void append_seq_write_over(const char *seq, size_t len) {
		if (end < this->len) {
			throw NotRolledTillEndException();
		}
		offset += this->len;
		size_t ind = this->len - 1;

		if ((start != end || c_outs.size() == k) && c_outs.size() > 0) {
			c_outs.pop_front();
		}

		// the following copies in characters from the end of the old sequence
		// into the deque
		std::vector<char> temp_vec;
		while (temp_vec.size() + c_outs.size() < k - 1 && ind >= start) {
			if (!is_ACTG(this->seq[ind])) {
				temp_vec.push_back('A');
			} else {
				temp_vec.push_back(this->seq[ind]);
			}
			if (ind == 0)
				break;

			ind--;
		}
		for (std::vector<char>::reverse_iterator rit = temp_vec.rbegin();
			 rit != temp_vec.rend(); rit++) {
			c_outs.push_back(*rit);
		}

		// the following copies in characters from the front of the new sequence
		// if there weren't enough non-ACTG characters at the end of the old
		// sequence
		ind = 0;
		start = 0;
		end = 0;
		while (c_outs.size() < k && ind < len) {
			if (!is_ACTG(seq[ind])) {
				c_outs.push_back('A');
			} else {
				c_outs.push_back(seq[ind]);
			}

			ind++;
			start++;
			end++;
		}

		// the following initializes a hash if we managed to fill the deque
		if (c_outs.size() == k) {
			std::string temp(c_outs.begin(), c_outs.end());
			// nthash::ntc64(temp.c_str(), k, fhash, rhash, chash,
			// locn_useless);
			fhash = base_forward_hash(temp.c_str(), k);
			rhash = base_reverse_hash(temp.c_str(), k);
			chash = nthash::canonical(fhash, rhash);
			is_valid_hash = true;
		}
		this->seq = seq;
		this->len = len;
	}

	bool init_hash_skip_over() {
		c_outs.clear();
		while (end - 1 < len) {
			bool works = true;
			for (size_t i = start; i < end; i++) {
				if (!is_ACTG(seq[i])) {
					start = i + 1;
					end = start + k;
					works = false;
					break;
				}
			}
			if (!works) {
				continue;
			}
			// nthash::ntc64(seq + start, k, fhash, rhash, chash, locn_useless);
			fhash = base_forward_hash(seq + start, k);
			rhash = base_reverse_hash(seq + start, k);
			chash = nthash::canonical(fhash, rhash);
			is_valid_hash = true;
			return true;
		}
		is_valid_hash = false;
		return false;
	}

	// need to do a good bit of rewriting
	// not performance critical so it's kinda whatever
	bool init_hash_write_over() {
		c_outs.clear();
		while (end - 1 < len) {
			std::string init_str;
			for (size_t i = start; i < end; i++) {
				if (!is_ACTG(seq[i])) {
					init_str.push_back('A');
				} else {
					init_str.push_back(seq[i]);
				}
			}

			// nthash::ntc64(seq + start, k, fhash, rhash, chash, locn_useless);
			fhash = base_forward_hash(init_str.c_str(), k);
			rhash = base_reverse_hash(init_str.c_str(), k);
			chash = nthash::canonical(fhash, rhash);
			is_valid_hash = true;
			return true;
		}
		is_valid_hash = false;
		return false;
	}

	bool roll_one_skip_over() {
		if (!is_valid_hash) {
			return false;
		}
		if (end >= len) {
			is_valid_hash = false;
			return false;
		}
		if (c_outs.size() > 0) {
			if (is_ACTG(seq[end])) {
				fhash = next_forward_hash(fhash, k, c_outs.front(), seq[end]);
				rhash = next_reverse_hash(rhash, k, c_outs.front(), seq[end]);
				c_outs.pop_front();
				end++;
				chash = nthash::canonical(fhash, rhash);
				return true;
			} else {
				// c_outs will contain at most k-1 characters, so if we jump to
				// end + 1, we won't consider anything else in deque so we
				// should clear it
				c_outs.clear();
				start = end + 1;
				end = start + k;
				return init_hash();
			}
		} else {
			if (is_ACTG(seq[end])) {
				fhash = next_forward_hash(fhash, k, seq[start], seq[end]);
				rhash = next_reverse_hash(rhash, k, seq[start], seq[end]);
				start++;
				end++;
				chash = nthash::canonical(fhash, rhash);
				return true;
			} else {
				start = end + 1;
				end = start + k;
				return init_hash();
			}
		}
	}

	bool roll_one_write_over() {
		if (!is_valid_hash) {
			return false;
		}
		if (end >= len) {
			is_valid_hash = false;
			return false;
		}
		char next_char = is_ACTG(seq[end]) ? seq[end] : 'A';
		if (c_outs.size() > 0) {
			fhash = next_forward_hash(fhash, k, c_outs.front(), next_char);
			rhash = next_reverse_hash(rhash, k, c_outs.front(), next_char);
			c_outs.pop_front();
			end++;

		} else {
			char out_char = is_ACTG(seq[start]) ? seq[start] : 'A';
			fhash = next_forward_hash(fhash, k, out_char, next_char);
			rhash = next_reverse_hash(rhash, k, out_char, next_char);
			start++;
			end++;
		}
		chash = nthash::canonical(fhash, rhash);
		return true;
	}

	// sequence to be digested, memory is owned by the user
	const char *seq;

	// length of seq
	size_t len;

	// the combined length of all the previous strings that have been appended
	// together, not counting the current string
	size_t offset;

	// internal index of the next character to be thrown out, junk if c_outs is
	// not empty
	size_t start;

	// internal index of next character to be added
	size_t end;

	// canonical hash of current k-mer
	uint64_t chash;

	// forward hash of current k-mer
	uint64_t fhash;

	// reverse hash of current k-mer
	uint64_t rhash;

	// length of kmer
	unsigned k;

	// deque of characters to be rolled out in the rolling hash from left to
	// right
	std::deque<char> c_outs;

	// Hash value to be minimized, 0 for canonical, 1 for forward, 2 for reverse
	MinimizedHashType minimized_h;

	// bool representing whether the current hash is meaningful, i.e.
	// corresponds to the k-mer at get_pos()
	bool is_valid_hash = false;
};

} // namespace digest

#endif // DIGESTER_HPP
