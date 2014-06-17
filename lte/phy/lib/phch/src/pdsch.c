/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2014 The libLTE Developers. See the
 * COPYRIGHT file at the top-level directory of this distribution.
 *
 * \section LICENSE
 *
 * This file is part of the libLTE library.
 *
 * libLTE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * libLTE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * A copy of the GNU Lesser General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

#include "prb.h"
#include "liblte/phy/phch/pdsch.h"
#include "liblte/phy/common/base.h"
#include "liblte/phy/utils/bit.h"
#include "liblte/phy/utils/debug.h"
#include "liblte/phy/utils/vector.h"

const enum modem_std modulations[4] =
    { LTE_BPSK, LTE_QPSK, LTE_QAM16, LTE_QAM64 };

#define MAX_PDSCH_RE(cp) (2 * (CP_NSYMB(cp) - 1) * 12 - 6)
#define HAS_REF(l, cp, nof_ports) ((l == 1 && nof_ports == 4) \
							|| l == 0 \
							|| l == CP_NSYMB(cp) - 3)

int pdsch_cp(pdsch_t *q, cf_t *input, cf_t *output, ra_prb_t *prb_alloc,
    int nsubframe, bool put) {
  int s, n, l, lp, lstart, lend, nof_refs;
  bool is_pbch, is_sss;
  cf_t *in_ptr = input, *out_ptr = output;
  int offset;

assert(q->cell_id >= 0);

	  INFO("%s %d RE from %d PRB\n", put ? "Putting" : "Getting",
      prb_alloc->re_sf[nsubframe], prb_alloc->slot[0].nof_prb);

  if (q->nof_ports == 1) {
    nof_refs = 2;
  } else {
    nof_refs = 4;
  }

  for (s = 0; s < 2; s++) {
    if (s == 0) {
      lstart = prb_alloc->lstart;
    } else {
      lstart = 0;
    }

    for (l = lstart; l < CP_NSYMB(q->cp); l++) {
      for (n = 0; n < prb_alloc->slot[s].nof_prb; n++) {
        lend = CP_NSYMB(q->cp);
        is_pbch = is_sss = false;

        // Skip PSS/SSS signals
        if (s == 0 && (nsubframe == 0 || nsubframe == 5)) {
          if (prb_alloc->slot[s].prb_idx[n] >= q->nof_prb / 2 - 3
              && prb_alloc->slot[s].prb_idx[n] <= q->nof_prb / 2 + 3) {
            lend = CP_NSYMB(q->cp) - 2;
            is_sss = true;
          }
        }
        // Skip PBCH
        if (s == 1 && nsubframe == 0) {
          if (prb_alloc->slot[s].prb_idx[n] >= q->nof_prb / 2 - 3
              && prb_alloc->slot[s].prb_idx[n] <= q->nof_prb / 2 + 3) {
            lstart = 4;
            is_pbch = true;
          }
        }
        lp = l + s * CP_NSYMB(q->cp);
        if (put) {
          out_ptr = &output[(lp * q->nof_prb + prb_alloc->slot[s].prb_idx[n])
              * RE_X_RB];
        } else {
          in_ptr = &input[(lp * q->nof_prb + prb_alloc->slot[s].prb_idx[n])
              * RE_X_RB];
        }

        if (is_pbch && (q->nof_prb % 2)
            && (prb_alloc->slot[s].prb_idx[n] == q->nof_prb / 2 - 3
                && prb_alloc->slot[s].prb_idx[n] == q->nof_prb / 2 + 3)) {
          if (l < lstart) {
            prb_cp_half(&in_ptr, &out_ptr, 1);
          }
        }
        if (l >= lstart && l < lend) {
          if (HAS_REF(l, q->cp, q->nof_ports)) {
            if (nof_refs == 2 && l != 0) {
              offset = q->cell_id % 3 + 3;
            } else {
              offset = q->cell_id % 3;
            }
            prb_cp_ref(&in_ptr, &out_ptr, offset, nof_refs, 1, put);
          } else {
            prb_cp(&in_ptr, &out_ptr, 1);
          }
        }
        if (is_sss && (q->nof_prb % 2)
            && (prb_alloc->slot[s].prb_idx[n] == q->nof_prb / 2 - 3
                && prb_alloc->slot[s].prb_idx[n] == q->nof_prb / 2 + 3)) {
          if (l >= lend) {
            prb_cp_half(&in_ptr, &out_ptr, 1);
          }
        }
      }
    }
  }

  if (put) {
    return (int) (input - in_ptr);
  } else {
    return (int) (output - out_ptr);
  }
}

/**
 * Puts PDSCH in slot number 1
 *
 * Returns the number of symbols written to sf_symbols
 *
 * 36.211 10.3 section 6.3.5
 */
int pdsch_put(pdsch_t *q, cf_t *pdsch_symbols, cf_t *sf_symbols,
    ra_prb_t *prb_alloc, int nsubframe) {
  return pdsch_cp(q, pdsch_symbols, sf_symbols, prb_alloc, nsubframe, true);
}

/**
 * Extracts PDSCH from slot number 1
 *
 * Returns the number of symbols written to PDSCH
 *
 * 36.211 10.3 section 6.3.5
 */
int pdsch_get(pdsch_t *q, cf_t *sf_symbols, cf_t *pdsch_symbols,
    ra_prb_t *prb_alloc, int nsubframe) {
  return pdsch_cp(q, sf_symbols, pdsch_symbols, prb_alloc, nsubframe, false);
}

/** Initializes the PDCCH transmitter and receiver */
int pdsch_init(pdsch_t *q, unsigned short user_rnti, int nof_prb, int nof_ports,
    int cell_id, lte_cp_t cp) {
  int ret = -1;
  int i;

  if (cell_id < 0) {
    return -1;
  }

  if (nof_ports > MAX_PORTS) {
    fprintf(stderr, "Invalid number of ports %d\n", nof_ports);
    return -1;
  }

  bzero(q, sizeof(pdsch_t));
  q->cell_id = cell_id;
  q->cp = cp;
  q->nof_ports = nof_ports;
  q->nof_prb = nof_prb;
  q->rnti = user_rnti;

  q->max_symbols = nof_prb * MAX_PDSCH_RE(cp);

  INFO("Init PDSCH: %d ports %d PRBs, max_symbols: %d\n", q->nof_ports,
      q->nof_prb, q->max_symbols);

  for (i = 0; i < 4; i++) {
    if (modem_table_std(&q->mod[i], modulations[i], true)) {
      goto clean;
    }
  }
  if (crc_init(&q->crc_tb, LTE_CRC24A, 24)) {
    goto clean;
  }
  if (crc_init(&q->crc_cb, LTE_CRC24B, 24)) {
    goto clean;
  }

  demod_soft_init(&q->demod);
  demod_soft_alg_set(&q->demod, APPROX);

  for (i = 0; i < NSUBFRAMES_X_FRAME; i++) {
    if (sequence_pdsch(&q->seq_pdsch[i], q->rnti, 0, 2 * i, q->cell_id,
        q->max_symbols * q->mod[3].nbits_x_symbol)) {
      goto clean;
    }
  }

  if (tcod_init(&q->encoder, MAX_LONG_CB)) {
    goto clean;
  }
  if (tdec_init(&q->decoder, MAX_LONG_CB)) {
    goto clean;
  }
  if (rm_turbo_init(&q->rm_turbo, 3 * MAX_LONG_CB)) {
    goto clean;
  }

  q->cb_in_b = malloc(sizeof(char) * MAX_LONG_CB);
  if (!q->cb_in_b) {
    goto clean;
  }
  q->cb_out_b = malloc(sizeof(char) * (3 * MAX_LONG_CB + 12));
  if (!q->cb_out_b) {
    goto clean;
  }

  q->pdsch_rm_f = malloc(sizeof(float) * (3 * MAX_LONG_CB + 12));
  if (!q->pdsch_rm_f) {
    goto clean;
  }

  q->pdsch_e_bits = malloc(
      sizeof(char) * q->max_symbols * q->mod[3].nbits_x_symbol);
  if (!q->pdsch_e_bits) {
    goto clean;
  }

  q->pdsch_llr = malloc(
      sizeof(float) * q->max_symbols * q->mod[3].nbits_x_symbol);
  if (!q->pdsch_llr) {
    goto clean;
  }

  q->pdsch_d = malloc(sizeof(cf_t) * q->max_symbols);
  if (!q->pdsch_d) {
    goto clean;
  }

  for (i = 0; i < nof_ports; i++) {
    q->ce[i] = malloc(sizeof(cf_t) * q->max_symbols);
    if (!q->ce[i]) {
      goto clean;
    }
    q->pdsch_x[i] = malloc(sizeof(cf_t) * q->max_symbols);
    if (!q->pdsch_x[i]) {
      goto clean;
    }
    q->pdsch_symbols[i] = malloc(sizeof(cf_t) * q->max_symbols);
    if (!q->pdsch_symbols[i]) {
      goto clean;
    }
  }

  ret = 0;
  clean: if (ret == -1) {
    pdsch_free(q);
  }
  return ret;
}

void pdsch_free(pdsch_t *q) {
  int i;

  if (q->cb_in_b) {
    free(q->cb_in_b);
  }
  if (q->cb_out_b) {
    free(q->cb_out_b);
  }
  if (q->pdsch_e_bits) {
    free(q->pdsch_e_bits);
  }
  if (q->pdsch_rm_f) {
    free(q->pdsch_rm_f);
  }
  if (q->pdsch_llr) {
    free(q->pdsch_llr);
  }
  if (q->pdsch_d) {
    free(q->pdsch_d);
  }
  for (i = 0; i < q->nof_ports; i++) {
    if (q->ce[i]) {
      free(q->ce[i]);
    }
    if (q->pdsch_x[i]) {
      free(q->pdsch_x[i]);
    }
    if (q->pdsch_symbols[i]) {
      free(q->pdsch_symbols[i]);
    }
  }

  for (i = 0; i < NSUBFRAMES_X_FRAME; i++) {
    sequence_free(&q->seq_pdsch[i]);
  }

  for (i = 0; i < 4; i++) {
    modem_table_free(&q->mod[i]);
  }
  tdec_free(&q->decoder);
  tcod_free(&q->encoder);
  rm_turbo_free(&q->rm_turbo);

}

struct cb_segm {
  int F;
  int C;
  int K1;
  int K2;
  int C1;
  int C2;
};

/* Calculate Codeblock Segmentation as in Section 5.1.2 of 36.212 */
void codeblock_segmentation(struct cb_segm *s, int tbs) {
  int Bp, B, idx1;

  B = tbs + 24;

  /* Calculate CB sizes */
  if (B < 6114) {
    s->C = 1;
    Bp = B;
  } else {
    s->C = (int) ceilf((float) B / (6114 - 24));
    Bp = B + 24 * s->C;
  }
  idx1 = lte_find_cb_index(Bp / s->C);
  s->K1 = lte_cb_size(idx1);
  if (s->C == 1) {
    s->K2 = 0;
    s->C2 = 0;
    s->C1 = 1;
  } else {
    s->K2 = lte_cb_size(idx1 - 1);
    s->C2 = (s->C * s->K1 - Bp) / (s->K1 - s->K2);
    s->C1 = s->C - s->C2;
  }
  s->F = s->C1 * s->K1 + s->C2 * s->K2 - Bp;
  INFO(
      "CB Segmentation: TBS: %d, C=%d, C+=%d K+=%d, C-=%d, K-=%d, F=%d, Bp=%d\n",
      tbs, s->C, s->C1, s->K1, s->C2, s->K2, s->F, Bp);
}

/* Decode a transport block according to 36.212 5.3.2
 *
 */
int pdsch_decode_tb(pdsch_t *q, char *data, int tbs, int nb_e, int rv_idx) {
  char parity[24];
  char *p_parity = parity;
  unsigned int par_rx, par_tx;
  int i;
  int cb_len, rp, wp, rlen, F, n_e;
  struct cb_segm cbs;

  /* Compute CB segmentation for this TBS */
  codeblock_segmentation(&cbs, tbs);

  rp = 0;
  rp = 0;
  wp = 0;
  for (i = 0; i < cbs.C; i++) {

    /* Get read/write lengths */
    if (i < cbs.C - cbs.C2) {
      cb_len = cbs.K1;
    } else {
      cb_len = cbs.K2;
    }
    if (cbs.C == 1) {
      rlen = cb_len;
    } else {
      rlen = cb_len - 24;
    }
    if (i == 0) {
      F = cbs.F;
    } else {
      F = 0;
    }

    if (i < cbs.C - 1) {
      n_e = nb_e / cbs.C;
    } else {
      n_e = nb_e - rp;
    }

    INFO("CB#%d: cb_len: %d, rlen: %d, wp: %d, rp: %d, F: %d, E: %d\n", i,
        cb_len, rlen - F, wp, rp, F, n_e);

    /* Rate Unmatching */
    rm_turbo_rx(&q->rm_turbo, &q->pdsch_llr[rp], n_e, q->pdsch_rm_f,
        3 * cb_len + 12, rv_idx);

    /* Turbo Decoding */
    tdec_run_all(&q->decoder, q->pdsch_rm_f, q->cb_in_b, TDEC_ITERATIONS,
        cb_len);

    if (cbs.C > 1) {
      /* Check Codeblock CRC */
      //crc_attach(&q->crc_cb, q->pdsch_b[wp], cb_len);
    }

    if (VERBOSE_ISDEBUG()) {
      DEBUG("CB#%d Len=%d: ", i, cb_len);
      vec_fprint_b(stdout, q->cb_in_b, cb_len);
    }

    /* Copy data to another buffer, removing the Codeblock CRC */
    if (i < cbs.C - 1) {
      memcpy(&data[wp], &q->cb_in_b[F], (rlen - F) * sizeof(char));
    } else {
      INFO("Last CB, appending parity: %d to %d from %d and 24 from %d\n",
          rlen - F - 24, wp, F, rlen - 24);
      /* Append Transport Block parity bits to the last CB */
      memcpy(&data[wp], &q->cb_in_b[F], (rlen - F - 24) * sizeof(char));
      memcpy(parity, &q->cb_in_b[rlen - 24], 24 * sizeof(char));
    }

    /* Set read/write pointers */
    wp += (rlen - F);
    rp += n_e;
  }

  INFO("END CB#%d: wp: %d, rp: %d\n", i, wp, rp);

  // Compute transport block CRC
  par_rx = crc_checksum(&q->crc_tb, data, tbs);

  // check parity bits
  par_tx = bit_unpack(&p_parity, 24);

  if (VERBOSE_ISDEBUG()) {
    DEBUG("DATA: ", 0);
    vec_fprint_b(stdout, data, tbs);
    DEBUG("PARITY: ", 0);
    vec_fprint_b(stdout, parity, 24);
  }

  if (!par_rx) {
    printf("\n\tCAUTION!! Received all-zero transport block\n\n");
  }

  return (par_rx != par_tx);
}

/** Decodes the PDSCH from the received symbols
 */
int pdsch_decode(pdsch_t *q, cf_t *sf_symbols, cf_t *ce[MAX_PORTS], char *data,
    int nsubframe, ra_mcs_t mcs, ra_prb_t *prb_alloc) {

  /* Set pointers for layermapping & precoding */
  int i;
  cf_t *x[MAX_LAYERS];
  int nof_symbols, nof_bits, nof_bits_e;

  nof_bits = mcs.tbs;
  nof_symbols = prb_alloc->re_sf[nsubframe];
  nof_bits_e = nof_symbols * q->mod[mcs.mod - 1].nbits_x_symbol;

  if (nof_bits > nof_bits_e) {
    fprintf(stderr, "Invalid code rate %.2f\n", (float) nof_bits / nof_bits_e);
    return -1;
  }

  if (nof_symbols > q->max_symbols) {
    fprintf(stderr,
        "Error too many RE per subframe (%d). PDSCH configured for %d RE (%d PRB)\n",
        nof_symbols, q->max_symbols, q->nof_prb);
    return -1;
  }

  INFO(
      "Decoding PDSCH SF: %d, Mod %d, NofBits: %d, NofSymbols: %d, NofBitsE: %d\n",
      nsubframe, mcs.mod, nof_bits, nof_symbols, nof_bits_e);

  if (nsubframe < 0 || nsubframe > NSUBFRAMES_X_FRAME) {
    fprintf(stderr, "Invalid subframe %d\n", nsubframe);
    return -1;
  }

  /* number of layers equals number of ports */
  for (i = 0; i < q->nof_ports; i++) {
    x[i] = q->pdsch_x[i];
  }
  memset(&x[q->nof_ports], 0, sizeof(cf_t*) * (MAX_LAYERS - q->nof_ports));

  /* extract symbols */
  pdsch_get(q, sf_symbols, q->pdsch_symbols[0], prb_alloc, nsubframe);

  /* extract channel estimates */
  for (i = 0; i < q->nof_ports; i++) {
    pdsch_get(q, ce[i], q->ce[i], prb_alloc, nsubframe);
  }

  /* TODO: only diversity is supported */
  if (q->nof_ports == 1) {
    /* no need for layer demapping */
    predecoding_single_zf(q->pdsch_symbols[0], q->ce[0], q->pdsch_d,
        nof_symbols);
  } else {
    predecoding_diversity_zf(q->pdsch_symbols[0], q->ce, x, q->nof_ports,
        nof_symbols);
    layerdemap_diversity(x, q->pdsch_d, q->nof_ports,
        nof_symbols / q->nof_ports);
  }

  /* demodulate symbols */
  demod_soft_sigma_set(&q->demod, 2.0 / q->mod[mcs.mod - 1].nbits_x_symbol);
  demod_soft_table_set(&q->demod, &q->mod[mcs.mod - 1]);
  demod_soft_demodulate(&q->demod, q->pdsch_d, q->pdsch_llr, nof_symbols);

  /* descramble */
  scrambling_f_offset(&q->seq_pdsch[nsubframe], q->pdsch_llr, 0, nof_bits_e);

  return pdsch_decode_tb(q, data, nof_bits, nof_bits_e, 0);
}

/* Encode a transport block according to 36.212 5.3.2
 *
 */
void pdsch_encode_tb(pdsch_t *q, char *data, int tbs, int nb_e, int rv_idx) {
  char parity[24];
  char *p_parity = parity;
  unsigned int par;
  int i;
  int cb_len, rp, wp, rlen, F, n_e;
  struct cb_segm cbs;

  /* Compute CB segmentation */
  codeblock_segmentation(&cbs, tbs);

  /* Compute transport block CRC */
  par = crc_checksum(&q->crc_tb, data, tbs);

  /* parity bits will be appended later */
  bit_pack(par, &p_parity, 24);

  if (VERBOSE_ISDEBUG()) {
    DEBUG("DATA: ", 0);
    vec_fprint_b(stdout, data, tbs);
    DEBUG("PARITY: ", 0);
    vec_fprint_b(stdout, parity, 24);
  }

  /* Add filler bits to the new data buffer */
  for (i = 0; i < cbs.F; i++) {
    q->cb_in_b[i] = LTE_NULL_BIT;
  }

  wp = 0;
  rp = 0;
  for (i = 0; i < cbs.C; i++) {

    /* Get read lengths */
    if (i < cbs.C - cbs.C2) {
      cb_len = cbs.K1;
    } else {
      cb_len = cbs.K2;
    }
    if (cbs.C > 1) {
      rlen = cb_len - 24;
    } else {
      rlen = cb_len;
    }
    if (i == 0) {
      F = cbs.F;
    } else {
      F = 0;
    }

    if (i < cbs.C - 1) {
      n_e = nb_e / cbs.C;
    } else {
      n_e = nb_e - wp;
    }

    INFO("CB#%d: cb_len: %d, rlen: %d, wp: %d, rp: %d, F: %d, E: %d\n", i,
        cb_len, rlen - F, wp, rp, F, n_e);

    /* Copy data to another buffer, making space for the Codeblock CRC */
    if (i < cbs.C - 1) {
      memcpy(&q->cb_in_b[F], &data[rp], (rlen - F) * sizeof(char));
    } else {
      INFO("Last CB, appending parity: %d from %d and 24 to %d\n",
          rlen - F - 24, rp, rlen - 24);
      /* Append Transport Block parity bits to the last CB */
      memcpy(&q->cb_in_b[F], &data[rp], (rlen - F - 24) * sizeof(char));
      memcpy(&q->cb_in_b[rlen - 24], parity, 24 * sizeof(char));
    }

    if (cbs.C > 1) {
      /* Attach Codeblock CRC */
      crc_attach(&q->crc_cb, q->cb_in_b, rlen);
    }

    if (VERBOSE_ISDEBUG()) {
      DEBUG("CB#%d Len=%d: ", i, cb_len);
      vec_fprint_b(stdout, q->cb_in_b, cb_len);
    }

    /* Turbo Encoding */
    tcod_encode(&q->encoder, q->cb_in_b, q->cb_out_b, cb_len);

    /* Rate matching */
    rm_turbo_tx(&q->rm_turbo, q->cb_out_b, 3 * cb_len + 12,
        &q->pdsch_e_bits[wp], n_e, rv_idx);

    /* Set read/write pointers */
    rp += (rlen - F);
    wp += n_e;
  }

  INFO("END CB#%d: wp: %d, rp: %d\n", i, wp, rp);
}

/** Converts the PDSCH data bits to symbols mapped to the slot ready for transmission
 */
int pdsch_encode(pdsch_t *q, char *data, cf_t *sf_symbols[MAX_PORTS],
    int nsubframe, ra_mcs_t mcs, ra_prb_t *prb_alloc) {
  int i;
  int nof_symbols, nof_bits, nof_bits_e;
  /* Set pointers for layermapping & precoding */
  cf_t *x[MAX_LAYERS];

  if (nsubframe < 0 || nsubframe > NSUBFRAMES_X_FRAME) {
    fprintf(stderr, "Invalid subframe %d\n", nsubframe);
    return -1;
  }

  nof_bits = mcs.tbs;
  nof_symbols = prb_alloc->re_sf[nsubframe];
  nof_bits_e = nof_symbols * q->mod[mcs.mod - 1].nbits_x_symbol;

  if (nof_bits > nof_bits_e) {
    fprintf(stderr, "Invalid code rate %.2f\n", (float) nof_bits / nof_bits_e);
    return -1;
  }

  if (nof_symbols > q->max_symbols) {
    fprintf(stderr,
        "Error too many RE per subframe (%d). PDSCH configured for %d RE (%d PRB)\n",
        nof_symbols, q->max_symbols, q->nof_prb);
    return -1;
  }

  INFO(
      "Encoding PDSCH SF: %d, Mod %d, NofBits: %d, NofSymbols: %d, NofBitsE: %d\n",
      nsubframe, mcs.mod, nof_bits, nof_symbols, nof_bits_e);

  /* number of layers equals number of ports */
  for (i = 0; i < q->nof_ports; i++) {
    x[i] = q->pdsch_x[i];
  }
  memset(&x[q->nof_ports], 0, sizeof(cf_t*) * (MAX_LAYERS - q->nof_ports));

  pdsch_encode_tb(q, data, nof_bits, nof_bits_e, 0);

  scrambling_b_offset(&q->seq_pdsch[nsubframe], q->pdsch_e_bits, 0, nof_bits_e);

  mod_modulate(&q->mod[mcs.mod - 1], q->pdsch_e_bits, q->pdsch_d, nof_bits_e);

  /* TODO: only diversity supported */
  if (q->nof_ports > 1) {
    layermap_diversity(q->pdsch_d, x, q->nof_ports, nof_symbols);
    precoding_diversity(x, q->pdsch_symbols, q->nof_ports,
        nof_symbols / q->nof_ports);
  } else {
    memcpy(q->pdsch_symbols[0], q->pdsch_d, nof_symbols * sizeof(cf_t));
  }

  /* mapping to resource elements */
  for (i = 0; i < q->nof_ports; i++) {
    pdsch_put(q, q->pdsch_symbols[i], sf_symbols[i], prb_alloc, nsubframe);
  }
  return 0;
}
