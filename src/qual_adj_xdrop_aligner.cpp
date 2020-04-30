/**
 * \file qual_adj_xdrop_aliigner.cpp: contains implementation of QualAdjXdropAligner
 */

#include "dozeu_interface.hpp"

// Configure dozeu:
// We want the full length bonus included
#ifndef DZ_FULL_LENGTH_BONUS
#define DZ_FULL_LENGTH_BONUS
#endif
// We want the quality adjusted versions of functions
#ifndef DZ_QUAL_ADJ
#define DZ_QUAL_ADJ
#endif
// We require these particular values for this enum because we index arrays with it.
enum { MISMATCH = 1, MATCH = 2, INS = 3, DEL = 4 };
// Set dozeu's CIGAR codes to match our enum
#ifndef DZ_CIGAR_OP
#define DZ_CIGAR_OP 0x04030201
#endif

#include <dozeu/dozeu.h>

using namespace vg;

QualAdjXdropAligner::QualAdjXdropAligner(const QualAdjXdropAligner& other)
{
    *this = other;
}

QualAdjXdropAligner& QualAdjXdropAligner::operator=(const QualAdjXdropAligner& other)
{
	if (this != &other) {

        if (dz) {
            dz_destroy(dz);
        }
        
        // TODO: a bit of an arcane step
        // we need to pull out the 0-padded quality adjusted matrices from dz into a contiguous array
        int8_t* qual_adj_matrix = (int8_t*) malloc(DZ_QUAL_MATRIX_SIZE * sizeof(int8_t));
        for (size_t i = 0; i < DZ_QUAL_MATRIX_SIZE; ++i) {
            qual_adj_matrix[i] = dz_qual_matrix(other.dz)[(i / 16) * 32 + (i % 16)];
        }
        
        dz = dz_qual_adj_init(other.dz->matrix,
                              qual_adj_matrix,
                              *((const uint16_t*) &other.dz->giv),
                              *((const uint16_t*) &other.dz->gev),
                              other.dz->bonus);
        
        free(qual_adj_matrix);
    }

	return *this;
}

QualAdjXdropAligner::QualAdjXdropAligner(QualAdjXdropAligner&& other)
{
    *this = other;
}

QualAdjXdropAligner& QualAdjXdropAligner::operator=(QualAdjXdropAligner&& other)
{
	if (this != &other) {
        if (dz) {
            dz_destroy(dz);
        }
        dz = other.dz;
        other.dz = nullptr;
    }

	return *this;
}

QualAdjXdropAligner::QualAdjXdropAligner(const int8_t* _score_matrix,
                                         const int8_t* _qual_adj_score_matrix,
                                         int8_t _gap_open, int8_t _gap_extension,
                                         int32_t _full_length_bonus)
{
    // xdrop aligner uses the parameterization where both gap open and gap extend
    // are added when opening a gap
    assert(_gap_open - _gap_extension >= 0);
    assert(_gap_extension > 0);
    assert(_full_length_bonus >= 0);
    dz = dz_qual_adj_init(_score_matrix, _qual_adj_score_matrix, _gap_open - _gap_extension,
                          _gap_extension, _full_length_bonus);
}

QualAdjXdropAligner::~QualAdjXdropAligner(void)
{
    dz_destroy(dz);
}

dz_query_s* QualAdjXdropAligner::pack_query_forward(const char* seq, const uint8_t* qual, size_t len) {
    return dz_qual_adj_pack_query_forward(dz, seq, qual, len);
}

dz_query_s* QualAdjXdropAligner::pack_query_reverse(const char* seq, const uint8_t* qual, size_t len) {
    return dz_qual_adj_pack_query_reverse(dz, seq, qual, len);
}

const dz_forefront_s* QualAdjXdropAligner::scan(const dz_query_s* query, const dz_forefront_s** forefronts,
                                         size_t n_forefronts, const char* ref, int32_t rlen,
                                         uint32_t rid, uint16_t xt) {
    return dz_qual_adj_scan(dz, query, forefronts, n_forefronts, ref, rlen, rid, xt);
}

const dz_forefront_s* QualAdjXdropAligner::extend(const dz_query_s* query, const dz_forefront_s** forefronts,
                                           size_t n_forefronts, const char* ref, int32_t rlen,
                                           uint32_t rid, uint16_t xt) {
    return dz_qual_adj_extend(dz, query, forefronts, n_forefronts, ref, rlen, rid, xt);
}

dz_alignment_s* QualAdjXdropAligner::trace(const dz_forefront_s* forefront) {
    return dz_qual_adj_trace(dz, forefront);
}

void QualAdjXdropAligner::flush() {
    dz_qual_adj_flush(dz);
}

/**
 * end of xdrop_aligner.cpp
 */
