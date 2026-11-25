#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "detectchip.h"

#ifndef KB
#define KB(n) ((size_t)(n) * 1024u)
#endif


static size_t detect_min_size(size_t a, size_t b) {
    return (a < b) ? a : b;
}

static bool detect_matches_at(const uint8_t *rom, size_t size,
                              size_t offset, const uint8_t *sig, size_t sig_len) {
    if (!rom || !sig || sig_len == 0u || offset + sig_len > size) {
        return false;
    }
    return memcmp(rom + offset, sig, sig_len) == 0;
}

static bool detect_search_count(const uint8_t *rom, size_t size,
                                const uint8_t *sig, size_t sig_len,
                                size_t min_matches) {
    if (!rom || !sig || sig_len == 0u || size < sig_len) {
        return false;
    }
    if (min_matches == 0u) {
        return true;
    }

    size_t found = 0u;
    for (size_t i = 0u; i <= size - sig_len; i++) {
        if (detect_matches_at(rom, size, i, sig, sig_len)) {
            found++;
            if (found >= min_matches) {
                return true;
            }
        }
    }
    return false;
}

static bool detect_search(const uint8_t *rom, size_t size,
                          const uint8_t *sig, size_t sig_len) {
    return detect_search_count(rom, size, sig, sig_len, 1u);
}

static bool detect_search_region_count(const uint8_t *rom, size_t region_size,
                                       const uint8_t *sig, size_t sig_len,
                                       size_t min_matches) {
    if (!rom || region_size == 0u) {
        return false;
    }
    return detect_search_count(rom, region_size, sig, sig_len, min_matches);
}

static bool detect_search_region(const uint8_t *rom, size_t region_size,
                                 const uint8_t *sig, size_t sig_len) {
    return detect_search_region_count(rom, region_size, sig, sig_len, 1u);
}

static bool detect_is_probably_elf(const uint8_t *rom, size_t size) {
    if (!rom || size < 52u) {
        return false;
    }

    static const uint8_t sig[4] = { 0x7Fu, 'E', 'L', 'F' };
    if (!detect_search_region(rom, detect_min_size(size, 2u * sizeof(sig)), sig, sizeof(sig))) {
        return false;
    }

    if (rom[0x05] != 1u) return false;   /* Little endian */
    if (rom[0x10] != 1u) return false;   /* ET_REL */
    if (rom[0x12] != 40u) return false;  /* EM_ARM */

    return true;
}

static bool detect_is_probably_sc(const uint8_t *rom, size_t size) {
    if (!rom || size == 0u || (size % KB(4)) != 0u) {
        return false;
    }

    const uint8_t *ptr = rom;
    size_t rem = size;
    while (rem > 0u) {
        if (rem < 256u || memcmp(ptr, ptr + 128u, 128u) != 0) {
            return false;
        }
        ptr += KB(4);
        rem -= KB(4);
    }
    return true;
}

static bool detect_is_probably_arm(const uint8_t *rom, size_t size) {
    static const uint8_t sigs[2][4] = {
        { 0xA0u, 0xC1u, 0x1Fu, 0xE0u },
        { 0x00u, 0x80u, 0x02u, 0xE0u }
    };
    size_t region = detect_min_size(size, KB(1));
    for (size_t i = 0u; i < 2u; i++) {
        if (detect_search_region(rom, region, sigs[i], 4u)) {
            return true;
        }
    }
    return false;
}

static bool detect_is_probably_03e0(const uint8_t *rom, size_t size) {
    static const uint8_t sigs[2][4] = {
        { 0x0Du, 0xE0u, 0x03u, 0x0Du },
        { 0xADu, 0xE0u, 0x03u, 0xADu }
    };
    for (size_t i = 0u; i < 2u; i++) {
        if (detect_search(rom, size, sigs[i], 4u)) {
            return true;
        }
    }
    return false;
}

static bool detect_is_probably_0840(const uint8_t *rom, size_t size) {
    static const uint8_t sigs1[3][3] = {
        { 0xADu, 0x00u, 0x08u },
        { 0xADu, 0x40u, 0x08u },
        { 0x2Cu, 0x00u, 0x08u }
    };
    for (size_t i = 0u; i < 3u; i++) {
        if (detect_search_count(rom, size, sigs1[i], 3u, 2u)) {
            return true;
        }
    }

    static const uint8_t sigs2[2][4] = {
        { 0x0Cu, 0x00u, 0x08u, 0x4Cu },
        { 0x0Cu, 0xFFu, 0x0Fu, 0x4Cu }
    };
    for (size_t i = 0u; i < 2u; i++) {
        if (detect_search_count(rom, size, sigs2[i], 4u, 2u)) {
            return true;
        }
    }
    return false;
}

static bool detect_is_probably_0fa0(const uint8_t *rom, size_t size) {
    static const uint8_t sigs[4][3] = {
        { 0x2Cu, 0xC0u, 0x0Fu },
        { 0x8Du, 0xC0u, 0x0Fu },
        { 0xADu, 0xC0u, 0x0Fu },
        { 0x2Cu, 0xC0u, 0xEFu }
    };
    for (size_t i = 0u; i < 4u; i++) {
        if (detect_search(rom, size, sigs[i], 3u)) {
            return true;
        }
    }
    return false;
}

static bool detect_is_probably_3e(const uint8_t *rom, size_t size) {
    static const uint8_t sig1[2] = { 0x85u, 0x3Eu };
    static const uint8_t sig2[2] = { 0x85u, 0x3Fu };
    return detect_search(rom, size, sig1, 2u) &&
           detect_search_count(rom, size, sig2, 2u, 2u);
}

static bool detect_is_probably_3ex(const uint8_t *rom, size_t size) {
    static const uint8_t sig[3] = { '3', 'E', 'X' };
    return detect_search_count(rom, size, sig, 3u, 2u);
}

static bool detect_is_probably_3e_plus(const uint8_t *rom, size_t size) {
    static const uint8_t sig[4] = { 'T', 'J', '3', 'E' };
    return detect_search(rom, size, sig, 4u);
}

static bool detect_is_probably_3f(const uint8_t *rom, size_t size) {
    static const uint8_t sig[2] = { 0x85u, 0x3Fu };
    return detect_search_count(rom, size, sig, 2u, 2u);
}

static bool detect_is_probably_4a50(const uint8_t *rom, size_t size) {
    if (!rom || size < 6u) {
        return false;
    }
    if (rom[size - 6u] == 0x50u && rom[size - 5u] == 0x4Au) {
        return true;
    }
    if (size > 0xFFFDu) {
        const size_t target = (size_t)rom[0xFFFDu] * 256u + (size_t)rom[0xFFFCu];
        if (((rom[0xFFFDu] & 0x1Fu) == 0x1Fu) &&
            (target + 2u < size) &&
            (rom[target] == 0x0Cu) &&
            ((rom[target + 2u] & 0xFEu) == 0x6Eu)) {
            return true;
        }
    }
    return false;
}

static bool detect_is_probably_4ksc(const uint8_t *rom, size_t size) {
    if (!rom || size < 256u || size < 6u) {
        return false;
    }
    const uint8_t first = rom[0];
    for (size_t i = 1u; i < 256u; i++) {
        if (rom[i] != first) {
            return false;
        }
    }
    return (rom[size - 6u] == 'S') && (rom[size - 5u] == 'C');
}

static bool detect_is_probably_bf(const uint8_t *rom, size_t size, CartType *out_type) {
    if (!rom || size < 8u || !out_type) {
        return false;
    }
    static const uint8_t bf[4]   = { 'B', 'F', 'B', 'F' };
    static const uint8_t bfsc[4] = { 'B', 'F', 'S', 'C' };
    const uint8_t *tail = rom + (size - 8u);

    if (detect_search(tail, 8u, bf, 4u)) {
        *out_type = CART_TYPE_BF;
        return true;
    }
    if (detect_search(tail, 8u, bfsc, 4u)) {
        *out_type = CART_TYPE_BFSC;
        return true;
    }
    return false;
}

static bool detect_is_probably_bus(const uint8_t *rom, size_t size) {
    static const uint8_t bus[3] = { 'B', 'U', 'S' };
    return detect_search_count(rom, size, bus, 3u, 2u);
}

static bool detect_is_probably_cdf(const uint8_t *rom, size_t size) {
    static const uint8_t cdf[3]      = { 'C', 'D', 'F' };
    static const uint8_t cdfjplus[8] = { 'P', 'L', 'U', 'S', 'C', 'D', 'F', 'J' };
    return detect_search_count(rom, size, cdf, 3u, 3u) ||
           detect_search(rom, size, cdfjplus, 8u);
}

static bool detect_is_probably_cty(const uint8_t *rom, size_t size) {
    static const uint8_t lenin[5] = { 'L', 'E', 'N', 'I', 'N' };
    return detect_search(rom, size, lenin, 5u);
}

static bool detect_is_probably_cv(const uint8_t *rom, size_t size) {
    static const uint8_t sigs[2][3] = {
        { 0x9Du, 0xFFu, 0xF3u },
        { 0x99u, 0x00u, 0xF4u }
    };
    for (size_t i = 0u; i < 2u; i++) {
        if (detect_search(rom, size, sigs[i], 3u)) {
            return true;
        }
    }
    return false;
}

static bool detect_is_probably_devc(const uint8_t *rom, size_t size) {
    static const uint8_t lock[4] = { 0xA9u, 0xFDu, 0x85u, 0x08u };
    return detect_search(rom, size, lock, 4u);
}

static bool detect_is_probably_df(const uint8_t *rom, size_t size, CartType *out_type) {
    if (!rom || size < 8u || !out_type) {
        return false;
    }
    static const uint8_t df[4]   = { 'D', 'F', 'D', 'F' };
    static const uint8_t dfsc[4] = { 'D', 'F', 'S', 'C' };
    const uint8_t *tail = rom + (size - 8u);

    if (detect_search(tail, 8u, df, 4u)) {
        *out_type = CART_TYPE_DF;
        return true;
    }
    if (detect_search(tail, 8u, dfsc, 4u)) {
        *out_type = CART_TYPE_DFSC;
        return true;
    }
    return false;
}

static bool detect_is_probably_dpc_plus(const uint8_t *rom, size_t size) {
    static const uint8_t dpcp[4] = { 'D', 'P', 'C', '+' };
    return detect_search_count(rom, size, dpcp, 4u, 2u);
}

static bool detect_is_probably_e0(const uint8_t *rom, size_t size) {
    static const uint8_t sigs[8][3] = {
        { 0x8Du, 0xE0u, 0x1Fu },
        { 0x8Du, 0xE0u, 0x5Fu },
        { 0x8Du, 0xE9u, 0xFFu },
        { 0x0Cu, 0xE0u, 0x1Fu },
        { 0xADu, 0xE0u, 0x1Fu },
        { 0xADu, 0xE9u, 0xFFu },
        { 0xADu, 0xEDu, 0xFFu },
        { 0xADu, 0xF3u, 0xBFu }
    };
    for (size_t i = 0u; i < 8u; i++) {
        if (detect_search(rom, size, sigs[i], 3u)) {
            return true;
        }
    }
    return false;
}

static bool detect_is_probably_e7(const uint8_t *rom, size_t size) {
    static const uint8_t sigs[7][3] = {
        { 0xADu, 0xE2u, 0xFFu },
        { 0xADu, 0xE5u, 0xFFu },
        { 0xADu, 0xE5u, 0x1Fu },
        { 0xADu, 0xE7u, 0x1Fu },
        { 0x0Cu, 0xE7u, 0x1Fu },
        { 0x8Du, 0xE7u, 0xFFu },
        { 0x8Du, 0xE7u, 0x1Fu }
    };
    for (size_t i = 0u; i < 7u; i++) {
        if (detect_search(rom, size, sigs[i], 3u)) {
            return true;
        }
    }
    return false;
}

static bool detect_is_probably_e7_8k(const uint8_t *rom, size_t size) {
    static const uint8_t sigs[3][3] = {
        { 0xADu, 0xE4u, 0xFFu },
        { 0xADu, 0xE5u, 0xFFu },
        { 0xADu, 0xE6u, 0xFFu }
    };
    for (size_t i = 0u; i < 3u; i++) {
        if (detect_search(rom, size, sigs[i], 3u)) {
            return true;
        }
    }
    return false;
}

static bool detect_is_probably_ef(const uint8_t *rom, size_t size, CartType *out_type) {
    if (!rom || !out_type) {
        return false;
    }
    if (size >= 8u) {
        static const uint8_t efef[4] = { 'E', 'F', 'E', 'F' };
        static const uint8_t efsc[4] = { 'E', 'F', 'S', 'C' };
        const uint8_t *tail = rom + (size - 8u);

        if (detect_search(tail, 8u, efef, 4u)) {
            *out_type = CART_TYPE_EF;
            return true;
        }
        if (detect_search(tail, 8u, efsc, 4u)) {
            *out_type = CART_TYPE_EFSC;
            return true;
        }
    }

    static const uint8_t sigs[4][3] = {
        { 0x0Cu, 0xE0u, 0xFFu },
        { 0xADu, 0xE0u, 0xFFu },
        { 0x0Cu, 0xE0u, 0x1Fu },
        { 0xADu, 0xE0u, 0x1Fu }
    };
    bool is_ef = false;
    for (size_t i = 0u; i < 4u; i++) {
        if (detect_search(rom, size, sigs[i], 3u)) {
            is_ef = true;
            break;
        }
    }

    if (is_ef) {
        *out_type = detect_is_probably_sc(rom, size) ? CART_TYPE_EFSC : CART_TYPE_EF;
        return true;
    }
    return false;
}

static bool detect_is_probably_eff(const uint8_t *rom, size_t size) {
    if (!rom) {
        return false;
    }
    if (size >= 8u) {
        static const uint8_t effb[4] = { 'E', 'F', 'F', 'B' };
        if (detect_search(rom + (size - 8u), 8u, effb, 4u)) {
            return true;
        }
    }

    static const uint8_t sigs[3][3] = {
        { 0x0Cu, 0xE0u, 0xFFu },
        { 0x0Cu, 0xF0u, 0x1Fu },
        { 0xADu, 0xF4u, 0x1Fu }
    };
    for (size_t i = 0u; i < 3u; i++) {
        if (!detect_search(rom, size, sigs[i], 3u)) {
            return false;
        }
    }
    return true;
}

static bool detect_is_probably_fa2(const uint8_t *rom, size_t size) {
    if (!rom || size < KB(32)) {
        return false;
    }
    for (size_t i = KB(29); i < KB(32); i++) {
        if (rom[i] != 0u) {
            return false;
        }
    }
    return true;
}

static bool detect_is_probably_fc(const uint8_t *rom, size_t size) {
    static const uint8_t sigs[3][6] = {
        { 0x8Du, 0xF8u, 0x1Fu, 0x4Au, 0x4Au, 0x8Du },
        { 0x8Du, 0xF8u, 0xFFu, 0x8Du, 0xFCu, 0xFFu },
        { 0x8Cu, 0xF9u, 0xFFu, 0xADu, 0xFCu, 0xFFu }
    };
    for (size_t i = 0u; i < 3u; i++) {
        if (detect_search(rom, size, sigs[i], 6u)) {
            return true;
        }
    }
    return false;
}

static bool detect_is_probably_fe(const uint8_t *rom, size_t size) {
    static const uint8_t sigs[5][5] = {
        { 0x20u, 0x00u, 0xD0u, 0xC6u, 0xC5u }, /* Decathlon */
        { 0x20u, 0xC3u, 0xF8u, 0xA5u, 0x82u }, /* Robot Tank */
        { 0xD0u, 0xFBu, 0x20u, 0x73u, 0xFEu }, /* Space Shuttle (NTSC/PAL) */
        { 0xD0u, 0xFBu, 0x20u, 0x68u, 0xFEu }, /* Space Shuttle (SECAM) */
        { 0x20u, 0x00u, 0xF0u, 0x84u, 0xD6u }  /* Thwocker */
    };
    for (size_t i = 0u; i < 5u; i++) {
        if (detect_search(rom, size, sigs[i], 5u)) {
            return true;
        }
    }
    return false;
}

static bool detect_is_probably_jane(const uint8_t *rom, size_t size) {
    static const uint8_t sig[4] = { 0xADu, 0xF1u, 0xFFu, 0x60u };
    return detect_search(rom, size, sig, 4u);
}

static bool detect_is_probably_gl(const uint8_t *rom, size_t size) {
    static const uint8_t sig[3] = { 0xADu, 0xB8u, 0x0Cu };
    return detect_search(rom, size, sig, 3u);
}

static bool detect_is_probably_mdm(const uint8_t *rom, size_t size) {
    static const uint8_t mdmc[4] = { 'M', 'D', 'M', 'C' };
    return detect_search_region(rom, detect_min_size(size, KB(8)), mdmc, 4u);
}

static bool detect_is_probably_mvc(const uint8_t *rom, size_t size) {
    static const uint8_t sig[4] = { 'M', 'V', 'C', 0u };
    return detect_search_region(rom, detect_min_size(size, 5u), sig, 4u);
}

static bool detect_is_probably_tvboy(const uint8_t *rom, size_t size) {
    static const uint8_t sig[5] = { 0x91u, 0x82u, 0x6Cu, 0xFCu, 0xFFu };
    return detect_search(rom, size, sig, 5u);
}

static bool detect_is_probably_ua(const uint8_t *rom, size_t size) {
    static const uint8_t sigs[6][3] = {
        { 0x8Du, 0x40u, 0x02u },
        { 0xADu, 0x40u, 0x02u },
        { 0xBDu, 0x1Fu, 0x02u },
        { 0x2Cu, 0xC0u, 0x02u },
        { 0x8Du, 0xC0u, 0x02u },
        { 0xADu, 0xC0u, 0x02u }
    };
    static const uint8_t bit_fb0[3] = { 0x2Cu, 0xB0u, 0x0Fu };

    for (size_t i = 0u; i < 6u; i++) {
        if (detect_search(rom, size, sigs[i], 3u)) {
            return true;
        }
    }
    return detect_search_count(rom, size, bit_fb0, 3u, 2u);
}

static bool detect_is_probably_wd(const uint8_t *rom, size_t size) {
    static const uint8_t sig[3] = { 0xA5u, 0x39u, 0x4Cu };
    return detect_search(rom, size, sig, 3u);
}

static bool detect_is_probably_x07(const uint8_t *rom, size_t size) {
    static const uint8_t sigs[6][3] = {
        { 0xADu, 0x0Du, 0x08u },
        { 0xADu, 0x1Du, 0x08u },
        { 0xADu, 0x2Du, 0x08u },
        { 0x0Cu, 0x0Du, 0x08u },
        { 0x0Cu, 0x1Du, 0x08u },
        { 0x0Cu, 0x2Du, 0x08u }
    };
    for (size_t i = 0u; i < 6u; i++) {
        if (detect_search(rom, size, sigs[i], 3u)) {
            return true;
        }
    }
    return false;
}

CartType detect_cart_type(const uint8_t *rom, size_t size) {
    if (!rom || size == 0u) {
        return CART_TYPE_4K;
    }

    CartType type = CART_TYPE_UNKNOWN;

    if (detect_is_probably_elf(rom, size)) {
        type = CART_TYPE_ELF;
    } else if ((size % 8448u) == 0u || size == KB(6)) {
        if (size == KB(6) && detect_is_probably_gl(rom, size)) {
            type = CART_TYPE_GL;
        } else {
            type = CART_TYPE_AR;
        }
    } else if ((size <= KB(2)) ||
               (size == KB(4) && memcmp(rom, rom + KB(2), KB(2)) == 0)) {
        type = detect_is_probably_cv(rom, size) ? CART_TYPE_CV : CART_TYPE_2K;
    } else if (size == KB(4)) {
        if (detect_is_probably_cv(rom, size))         type = CART_TYPE_CV;
        else if (detect_is_probably_4ksc(rom, size))  type = CART_TYPE_4KSC;
        else if (detect_is_probably_fc(rom, size))    type = CART_TYPE_FC;
        else if (detect_is_probably_gl(rom, size))    type = CART_TYPE_GL;
        else                                          type = CART_TYPE_4K;
    } else if (size == KB(8)) {
        static const uint8_t f8sig[2][3] = {
            { 0x8Du, 0xF9u, 0x1Fu }, /* STA $1FF9 */
            { 0x8Du, 0xF9u, 0xFFu }  /* STA $FFF9 */
        };
        bool f8 = detect_search_count(rom, size, f8sig[0], 3u, 2u) ||
                  detect_search_count(rom, size, f8sig[1], 3u, 2u);

        if (detect_is_probably_sc(rom, size))                         type = CART_TYPE_F8SC;
        else if (size >= KB(8) && memcmp(rom, rom + KB(4), KB(4)) == 0) type = CART_TYPE_4K;
        else if (detect_is_probably_e0(rom, size))                    type = CART_TYPE_E0;
        else if (detect_is_probably_3ex(rom, size))                   type = CART_TYPE_3EX;
        else if (detect_is_probably_3e(rom, size))                    type = CART_TYPE_3E;
        else if (detect_is_probably_3f(rom, size))                    type = CART_TYPE_3F;
        else if (detect_is_probably_ua(rom, size))                    type = CART_TYPE_UA;
        else if (detect_is_probably_0fa0(rom, size))                  type = CART_TYPE_0FA0;
        else if (detect_is_probably_fe(rom, size) && !f8)            type = CART_TYPE_FE;
        else if (detect_is_probably_0840(rom, size))                  type = CART_TYPE_0840;
        else if (detect_is_probably_e7_8k(rom, size))                 type = CART_TYPE_E7;
        else if (detect_is_probably_wd(rom, size))                    type = CART_TYPE_WD;
        else if (detect_is_probably_fc(rom, size))                    type = CART_TYPE_FC;
        else if (detect_is_probably_03e0(rom, size))                  type = CART_TYPE_03E0;
        else                                                          type = CART_TYPE_F8;
    } else if (size == KB(8) + 3u) {
        type = CART_TYPE_WDSW;
    } else if (size >= KB(10) && size <= KB(10) + 256u) {
        type = CART_TYPE_DPC;
    } else if (size == KB(12)) {
        if (detect_is_probably_e7(rom, size)) type = CART_TYPE_E7;
        else                                  type = CART_TYPE_FA;
    } else if (size == KB(16)) {
        if (detect_is_probably_sc(rom, size))        type = CART_TYPE_F6SC;
        else if (detect_is_probably_e7(rom, size))   type = CART_TYPE_E7;
        else if (detect_is_probably_fc(rom, size))   type = CART_TYPE_FC;
        else if (detect_is_probably_3ex(rom, size))  type = CART_TYPE_3EX;
        else if (detect_is_probably_3e(rom, size))   type = CART_TYPE_3E;
        else if (detect_is_probably_jane(rom, size)) type = CART_TYPE_JANE;
        else                                         type = CART_TYPE_F6;
    } else if (size == KB(24) || size == KB(28)) {
        if (detect_is_probably_devc(rom, size)) type = CART_TYPE_DEVC;
        else                                    type = CART_TYPE_FA2;
    } else if (size == KB(29)) {
        if (detect_is_probably_arm(rom, size)) type = CART_TYPE_FA2;
        else                                   type = CART_TYPE_DPCP;
    } else if (size == KB(32)) {
        if (detect_is_probably_cty(rom, size))           type = CART_TYPE_CTY;
        else if (detect_is_probably_cdf(rom, size))      type = CART_TYPE_CDF;
        else if (detect_is_probably_dpc_plus(rom, size)) type = CART_TYPE_DPCP;
        else if (detect_is_probably_sc(rom, size))       type = CART_TYPE_F4SC;
        else if (detect_is_probably_3ex(rom, size))      type = CART_TYPE_3EX;
        else if (detect_is_probably_3e(rom, size))       type = CART_TYPE_3E;
        else if (detect_is_probably_3f(rom, size))       type = CART_TYPE_3F;
        else if (detect_is_probably_bus(rom, size))      type = CART_TYPE_BUS;
        else if (detect_is_probably_fa2(rom, size))      type = CART_TYPE_FA2;
        else if (detect_is_probably_fc(rom, size))       type = CART_TYPE_FC;
        else                                             type = CART_TYPE_F4;
    } else if (size == KB(60)) {
        if (detect_is_probably_cty(rom, size)) type = CART_TYPE_CTY;
        else                                   type = CART_TYPE_F4;
    } else if (size == KB(64)) {
        if (detect_is_probably_eff(rom, size))             type = CART_TYPE_EFF;
        else if (detect_is_probably_cdf(rom, size))        type = CART_TYPE_CDF;
        else if (detect_is_probably_3ex(rom, size))        type = CART_TYPE_3EX;
        else if (detect_is_probably_3e(rom, size))         type = CART_TYPE_3E;
        else if (detect_is_probably_3f(rom, size))         type = CART_TYPE_3F;
        else if (detect_is_probably_4a50(rom, size))       type = CART_TYPE_4A50;
        else if (detect_is_probably_ef(rom, size, &type))  ;
        else if (detect_is_probably_x07(rom, size))        type = CART_TYPE_X07;
        else                                               type = CART_TYPE_F0;
    } else if (size == KB(128)) {
        if (detect_is_probably_cdf(rom, size))             type = CART_TYPE_CDF;
        else if (detect_is_probably_3ex(rom, size))        type = CART_TYPE_3EX;
        else if (detect_is_probably_3e(rom, size))         type = CART_TYPE_3E;
        else if (detect_is_probably_df(rom, size, &type))  ;
        else if (detect_is_probably_3f(rom, size))         type = CART_TYPE_3F;
        else if (detect_is_probably_4a50(rom, size))       type = CART_TYPE_4A50;
        else                                               type = CART_TYPE_SB;
    } else if (size == KB(256)) {
        if (detect_is_probably_cdf(rom, size))             type = CART_TYPE_CDF;
        else if (detect_is_probably_3ex(rom, size))        type = CART_TYPE_3EX;
        else if (detect_is_probably_3e(rom, size))         type = CART_TYPE_3E;
        else if (detect_is_probably_bf(rom, size, &type))  ;
        else if (detect_is_probably_3f(rom, size))         type = CART_TYPE_3F;
        else                                               type = CART_TYPE_SB;
    } else if (size == KB(512)) {
        if (detect_is_probably_tvboy(rom, size))      type = CART_TYPE_TVBOY;
        else if (detect_is_probably_cdf(rom, size))   type = CART_TYPE_CDF;
        else if (detect_is_probably_3ex(rom, size))  type = CART_TYPE_3EX;
        else if (detect_is_probably_3e(rom, size))   type = CART_TYPE_3E;
        else if (detect_is_probably_3f(rom, size))   type = CART_TYPE_3F;
    } else {
        if (detect_is_probably_3ex(rom, size))      type = CART_TYPE_3EX;
        else if (detect_is_probably_3e(rom, size))  type = CART_TYPE_3E;
        else if (detect_is_probably_3f(rom, size))  type = CART_TYPE_3F;
    }

    if (detect_is_probably_3e_plus(rom, size))     type = CART_TYPE_3EP;
    else if (detect_is_probably_mdm(rom, size))    type = CART_TYPE_MDM;
    else if (detect_is_probably_mvc(rom, size))    type = CART_TYPE_MVC;

    if (type == CART_TYPE_UNKNOWN) {
        type = CART_TYPE_4K;
    }

    return type;
}