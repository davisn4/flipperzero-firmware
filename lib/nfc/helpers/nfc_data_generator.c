#include "nfc_data_generator.h"

#include <furi/furi.h>
#include <furi_hal_random.h>

#define NXP_MANUFACTURER_ID (0x04)

typedef void (*NfcDataGeneratorHandler)(NfcDevData* data);

typedef struct {
    const char* name;
    NfcDataGeneratorHandler handler;
} NfcDataGenerator;

static const uint8_t version_bytes_mf0ulx1[] = {0x00, 0x04, 0x03, 0x00, 0x01, 0x00, 0x00, 0x03};
static const uint8_t version_bytes_ntag21x[] = {0x00, 0x04, 0x04, 0x02, 0x01, 0x00, 0x00, 0x03};
static const uint8_t version_bytes_ntag_i2c[] = {0x00, 0x04, 0x04, 0x05, 0x02, 0x00, 0x00, 0x03};
static const uint8_t default_data_ntag203[] =
    {0xE1, 0x10, 0x12, 0x00, 0x01, 0x03, 0xA0, 0x10, 0x44, 0x03, 0x00, 0xFE};
static const uint8_t default_data_ntag213[] = {0x01, 0x03, 0xA0, 0x0C, 0x34, 0x03, 0x00, 0xFE};
static const uint8_t default_data_ntag215_216[] = {0x03, 0x00, 0xFE};
static const uint8_t default_data_ntag_i2c[] = {0xE1, 0x10, 0x00, 0x00, 0x03, 0x00, 0xFE};
static const uint8_t default_config_ntag_i2c[] = {0x01, 0x00, 0xF8, 0x48, 0x08, 0x01, 0x00, 0x00};

static void nfc_generate_common_start(NfcDevData* data) {
    memset(data, 0, sizeof(NfcDevData));
}

static void nfc_generate_mf_ul_uid(uint8_t* uid) {
    uid[0] = NXP_MANUFACTURER_ID;
    furi_hal_random_fill_buf(&uid[1], 6);
    // I'm not sure how this is generated, but the upper nybble always seems to be 8
    uid[6] &= 0x0F;
    uid[6] |= 0x80;
}

static void nfc_generate_mf_ul_common(NfcDevData* data) {
    MfUltralightData* mfu_data = &data->mf_ul_data;
    mfu_data->nfca_data.uid_len = 7;
    nfc_generate_mf_ul_uid(mfu_data->nfca_data.uid);
    mfu_data->nfca_data.atqa[0] = 0x44;
    mfu_data->nfca_data.atqa[1] = 0x00;
    mfu_data->nfca_data.sak = 0x00;
    data->protocol = NfcDevProtocolMfUltralight;
}

static void nfc_generate_calc_bcc(uint8_t* uid, uint8_t* bcc0, uint8_t* bcc1) {
    *bcc0 = 0x88 ^ uid[0] ^ uid[1] ^ uid[2];
    *bcc1 = uid[3] ^ uid[4] ^ uid[5] ^ uid[6];
}

static void nfc_generate_mf_ul_copy_uid_with_bcc(NfcDevData* data) {
    MfUltralightData* mfu_data = &data->mf_ul_data;
    memcpy(mfu_data->page[0].data, mfu_data->nfca_data.uid, 3);
    memcpy(mfu_data->page[1].data, &mfu_data->nfca_data.uid[3], 4);

    nfc_generate_calc_bcc(
        mfu_data->nfca_data.uid, &mfu_data->page[0].data[3], &mfu_data->page[2].data[0]);
}

static void nfc_generate_mf_ul_orig(NfcDevData* data) {
    nfc_generate_common_start(data);
    nfc_generate_mf_ul_common(data);

    MfUltralightData* mfu_data = &data->mf_ul_data;
    mfu_data->type = MfUltralightTypeUnknown;
    mfu_data->pages_total = 16;
    mfu_data->pages_read = 16;
    nfc_generate_mf_ul_copy_uid_with_bcc(data);
    // TODO: what's internal byte on page 2?
    memset(&mfu_data->page[4], 0xff, sizeof(MfUltralightPage));
}

static void nfc_generate_mf_ul_with_config_common(NfcDevData* data, uint8_t num_pages) {
    nfc_generate_common_start(data);
    nfc_generate_mf_ul_common(data);

    MfUltralightData* mfu_data = &data->mf_ul_data;
    mfu_data->pages_total = num_pages;
    mfu_data->pages_read = num_pages;
    nfc_generate_mf_ul_copy_uid_with_bcc(data);
    uint16_t config_index = (num_pages - 4);
    mfu_data->page[config_index].data[0] = 0x04; // STRG_MOD_EN
    mfu_data->page[config_index].data[3] = 0xff; // AUTH0
    mfu_data->page[config_index + 1].data[1] = 0x05; // VCTID
    memset(&mfu_data->page[config_index + 2], 0xff, sizeof(MfUltralightPage)); // Default PWD
    if(num_pages > 20) {
        mfu_data->page[config_index - 1].data[3] = MF_ULTRALIGHT_TEARING_FLAG_DEFAULT;
    }
}

static void nfc_generate_mf_ul_ev1_common(NfcDevData* data, uint8_t num_pages) {
    nfc_generate_mf_ul_with_config_common(data, num_pages);
    MfUltralightData* mfu_data = &data->mf_ul_data;
    memcpy(&mfu_data->version, version_bytes_mf0ulx1, sizeof(MfUltralightVersion));
    for(size_t i = 0; i < 3; ++i) {
        mfu_data->tearing_flag[i].data[0] = MF_ULTRALIGHT_TEARING_FLAG_DEFAULT;
    }
    // TODO: what's internal byte on page 2?
}

static void nfc_generate_mf_ul_11(NfcDevData* data) {
    nfc_generate_mf_ul_ev1_common(data, 20);
    MfUltralightData* mfu_data = &data->mf_ul_data;
    mfu_data->type = MfUltralightTypeUL11;
    mfu_data->version.prod_subtype = 0x01;
    mfu_data->version.storage_size = 0x0B;
    mfu_data->page[16].data[0] = 0x00; // Low capacitance version does not have STRG_MOD_EN
}

static void nfc_generate_mf_ul_h11(NfcDevData* data) {
    nfc_generate_mf_ul_ev1_common(data, 20);
    MfUltralightData* mfu_data = &data->mf_ul_data;
    mfu_data->type = MfUltralightTypeUL11;
    mfu_data->version.prod_subtype = 0x02;
    mfu_data->version.storage_size = 0x0B;
}

static void nfc_generate_mf_ul_21(NfcDevData* data) {
    nfc_generate_mf_ul_ev1_common(data, 41);
    MfUltralightData* mfu_data = &data->mf_ul_data;
    mfu_data->type = MfUltralightTypeUL21;
    mfu_data->version.prod_subtype = 0x01;
    mfu_data->version.storage_size = 0x0E;
    mfu_data->page[37].data[0] = 0x00; // Low capacitance version does not have STRG_MOD_EN
}

static void nfc_generate_mf_ul_h21(NfcDevData* data) {
    nfc_generate_mf_ul_ev1_common(data, 41);
    MfUltralightData* mfu_data = &data->mf_ul_data;
    mfu_data->type = MfUltralightTypeUL21;
    mfu_data->version.prod_subtype = 0x02;
    mfu_data->version.storage_size = 0x0E;
}

static void nfc_generate_ntag203(NfcDevData* data) {
    nfc_generate_common_start(data);
    nfc_generate_mf_ul_common(data);

    MfUltralightData* mfu_data = &data->mf_ul_data;
    mfu_data->type = MfUltralightTypeNTAG203;
    mfu_data->pages_total = 42;
    mfu_data->pages_read = 42;
    nfc_generate_mf_ul_copy_uid_with_bcc(data);
    mfu_data->page[2].data[1] = 0x48; // Internal byte
    memcpy(&mfu_data->page[3], default_data_ntag203, sizeof(MfUltralightPage));
}

static void nfc_generate_ntag21x_common(NfcDevData* data, uint8_t num_pages) {
    nfc_generate_mf_ul_with_config_common(data, num_pages);
    MfUltralightData* mfu_data = &data->mf_ul_data;
    memcpy(&mfu_data->version, version_bytes_ntag21x, sizeof(MfUltralightVersion));
    mfu_data->page[2].data[1] = 0x48; // Internal byte
    // Capability container
    mfu_data->page[3].data[0] = 0xE1;
    mfu_data->page[3].data[1] = 0x10;
}

static void nfc_generate_ntag213(NfcDevData* data) {
    nfc_generate_ntag21x_common(data, 45);
    MfUltralightData* mfu_data = &data->mf_ul_data;
    mfu_data->type = MfUltralightTypeNTAG213;
    mfu_data->version.storage_size = 0x0F;
    mfu_data->page[3].data[2] = 0x12;
    // Default contents
    memcpy(&mfu_data->page[4], default_data_ntag213, sizeof(default_data_ntag213));
}

static void nfc_generate_ntag215(NfcDevData* data) {
    nfc_generate_ntag21x_common(data, 135);
    MfUltralightData* mfu_data = &data->mf_ul_data;
    mfu_data->type = MfUltralightTypeNTAG215;
    mfu_data->version.storage_size = 0x11;
    mfu_data->page[3].data[2] = 0x3E;
    // Default contents
    memcpy(&mfu_data->page[4], default_data_ntag215_216, sizeof(default_data_ntag215_216));
}

static void nfc_generate_ntag216(NfcDevData* data) {
    nfc_generate_ntag21x_common(data, 231);
    MfUltralightData* mfu_data = &data->mf_ul_data;
    mfu_data->type = MfUltralightTypeNTAG216;
    mfu_data->version.storage_size = 0x13;
    mfu_data->page[3].data[2] = 0x6D;
    // Default contents
    memcpy(&mfu_data->page[4], default_data_ntag215_216, sizeof(default_data_ntag215_216));
}

static void
    nfc_generate_ntag_i2c_common(NfcDevData* data, MfUltralightType type, uint16_t num_pages) {
    nfc_generate_common_start(data);
    nfc_generate_mf_ul_common(data);

    MfUltralightData* mfu_data = &data->mf_ul_data;
    mfu_data->type = type;
    memcpy(&mfu_data->version, version_bytes_ntag_i2c, sizeof(version_bytes_ntag_i2c));
    mfu_data->pages_total = num_pages;
    mfu_data->pages_read = num_pages;
    memcpy(mfu_data->page[0].data, mfu_data->nfca_data.uid, mfu_data->nfca_data.uid_len);
    mfu_data->page[1].data[3] = mfu_data->nfca_data.sak;
    mfu_data->page[2].data[0] = mfu_data->nfca_data.atqa[0];
    mfu_data->page[2].data[1] = mfu_data->nfca_data.atqa[1];

    uint16_t config_register_page = 0;
    uint16_t session_register_page = 0;

    // Sync with mifare_ultralight.c
    switch(type) {
    case MfUltralightTypeNTAGI2C1K:
        config_register_page = 227;
        session_register_page = 229;
        break;
    case MfUltralightTypeNTAGI2C2K:
        config_register_page = 481;
        session_register_page = 483;
        break;
    case MfUltralightTypeNTAGI2CPlus1K:
    case MfUltralightTypeNTAGI2CPlus2K:
        config_register_page = 232;
        session_register_page = 234;
        break;
    default:
        furi_crash("Unknown MFUL");
        break;
    }

    memcpy(
        &mfu_data->page[config_register_page],
        default_config_ntag_i2c,
        sizeof(default_config_ntag_i2c));
    memcpy(
        &mfu_data->page[session_register_page],
        default_config_ntag_i2c,
        sizeof(default_config_ntag_i2c));
}

static void nfc_generate_ntag_i2c_1k(NfcDevData* data) {
    nfc_generate_ntag_i2c_common(data, MfUltralightTypeNTAGI2C1K, 231);
    MfUltralightData* mfu_data = &data->mf_ul_data;
    mfu_data->version.prod_ver_minor = 0x01;
    mfu_data->version.storage_size = 0x13;

    memcpy(&mfu_data->page[3], default_data_ntag_i2c, sizeof(default_data_ntag_i2c));
    mfu_data->page[3].data[2] = 0x6D; // Size of tag in CC
}

static void nfc_generate_ntag_i2c_2k(NfcDevData* data) {
    nfc_generate_ntag_i2c_common(data, MfUltralightTypeNTAGI2C2K, 485);
    MfUltralightData* mfu_data = &data->mf_ul_data;
    mfu_data->version.prod_ver_minor = 0x01;
    mfu_data->version.storage_size = 0x15;

    memcpy(&mfu_data->page[3], default_data_ntag_i2c, sizeof(default_data_ntag_i2c));
    mfu_data->page[3].data[2] = 0xEA; // Size of tag in CC
}

static void
    nfc_generate_ntag_i2c_plus_common(NfcDevData* data, MfUltralightType type, uint16_t num_pages) {
    nfc_generate_ntag_i2c_common(data, type, num_pages);

    MfUltralightData* mfu_data = &data->mf_ul_data;
    uint16_t config_index = 227;
    mfu_data->page[config_index].data[3] = 0xff; // AUTH0

    memset(&mfu_data->page[config_index + 2], 0xFF, sizeof(MfUltralightPage)); // Default PWD
}

static void nfc_generate_ntag_i2c_plus_1k(NfcDevData* data) {
    nfc_generate_ntag_i2c_plus_common(data, MfUltralightTypeNTAGI2CPlus1K, 236);
    MfUltralightData* mfu_data = &data->mf_ul_data;
    mfu_data->version.prod_ver_minor = 0x02;
    mfu_data->version.storage_size = 0x13;
}

static void nfc_generate_ntag_i2c_plus_2k(NfcDevData* data) {
    nfc_generate_ntag_i2c_plus_common(data, MfUltralightTypeNTAGI2CPlus2K, 492);
    MfUltralightData* mfu_data = &data->mf_ul_data;
    mfu_data->version.prod_ver_minor = 0x02;
    mfu_data->version.storage_size = 0x15;
}

static void nfc_generate_mf_classic_uid(uint8_t* uid, uint8_t length) {
    uid[0] = NXP_MANUFACTURER_ID;
    furi_hal_random_fill_buf(&uid[1], length - 1);
}

static void
    nfc_generate_mf_classic_common(MfClassicData* data, uint8_t uid_len, MfClassicType type) {
    data->nfca_data.uid_len = uid_len;
    data->nfca_data.atqa[0] = 0x44;
    data->nfca_data.atqa[1] = 0x00;
    data->nfca_data.sak = 0x08;
    data->type = type;
}

static void nfc_generate_mf_classic_sector_trailer(MfClassicData* data, uint8_t block) {
    // All keys are set to FFFF FFFF FFFFh at chip delivery and the bytes 6, 7 and 8 are set to FF0780h.
    MfClassicSectorTrailer* sec_tr = (MfClassicSectorTrailer*)data->block[block].data;
    sec_tr->access_bits.data[0] = 0xFF;
    sec_tr->access_bits.data[1] = 0x07;
    sec_tr->access_bits.data[2] = 0x80;
    sec_tr->access_bits.data[3] = 0x69; // Nice

    mf_classic_set_block_read(data, block, &data->block[block]);
    mf_classic_set_key_found(
        data, mf_classic_get_sector_by_block(block), MfClassicKeyTypeA, 0xFFFFFFFFFFFF);
    mf_classic_set_key_found(
        data, mf_classic_get_sector_by_block(block), MfClassicKeyTypeB, 0xFFFFFFFFFFFF);
}

static void nfc_generate_mf_classic_block_0(
    uint8_t* block,
    uint8_t uid_len,
    uint8_t sak,
    uint8_t atqa0,
    uint8_t atqa1) {
    // Block length is always 16 bytes, and the UID can be either 4 or 7 bytes
    furi_assert(uid_len == 4 || uid_len == 7);
    furi_assert(block);

    if(uid_len == 4) {
        // Calculate BCC
        block[uid_len] = 0;

        for(int i = 0; i < uid_len; i++) {
            block[uid_len] ^= block[i];
        }
    } else {
        uid_len -= 1;
    }

    block[uid_len + 1] = sak;
    block[uid_len + 2] = atqa0;
    block[uid_len + 3] = atqa1;

    for(int i = uid_len + 4; i < 16; i++) {
        block[i] = 0xFF;
    }
}

static void nfc_generate_mf_classic(NfcDevData* data, uint8_t uid_len, MfClassicType type) {
    nfc_generate_common_start(data);
    data->protocol = NfcDevProtocolMfClassic;
    MfClassicData* mfc_data = &data->mf_classic_data;
    nfc_generate_mf_classic_uid(mfc_data->block[0].data, uid_len);
    nfc_generate_mf_classic_common(mfc_data, uid_len, type);

    // Set the UID
    mfc_data->nfca_data.uid[0] = NXP_MANUFACTURER_ID;
    for(int i = 1; i < uid_len; i++) {
        mfc_data->nfca_data.uid[i] = mfc_data->block[0].data[i];
    }

    mf_classic_set_block_read(mfc_data, 0, &mfc_data->block[0]);

    uint16_t block_num = mf_classic_get_total_block_num(type);
    if(type == MfClassicType4k) {
        // Set every block to 0xFF
        for(uint16_t i = 1; i < block_num; i += 1) {
            if(mf_classic_is_sector_trailer(i)) {
                nfc_generate_mf_classic_sector_trailer(mfc_data, i);
            } else {
                memset(&mfc_data->block[i].data, 0xFF, 16);
            }
            mf_classic_set_block_read(mfc_data, i, &mfc_data->block[i]);
        }
        // Set SAK to 18
        data->nfca_data.sak = 0x18;
    } else if(type == MfClassicType1k) {
        // Set every block to 0xFF
        for(uint16_t i = 1; i < block_num; i += 1) {
            if(mf_classic_is_sector_trailer(i)) {
                nfc_generate_mf_classic_sector_trailer(mfc_data, i);
            } else {
                memset(&mfc_data->block[i].data, 0xFF, 16);
            }
            mf_classic_set_block_read(mfc_data, i, &mfc_data->block[i]);
        }
        // Set SAK to 08
        data->nfca_data.sak = 0x08;
    } else if(type == MfClassicTypeMini) {
        // Set every block to 0xFF
        for(uint16_t i = 1; i < block_num; i += 1) {
            if(mf_classic_is_sector_trailer(i)) {
                nfc_generate_mf_classic_sector_trailer(mfc_data, i);
            } else {
                memset(&mfc_data->block[i].data, 0xFF, 16);
            }
            mf_classic_set_block_read(mfc_data, i, &mfc_data->block[i]);
        }
        // Set SAK to 09
        data->nfca_data.sak = 0x09;
    }

    nfc_generate_mf_classic_block_0(
        data->mf_classic_data.block[0].data,
        uid_len,
        data->nfca_data.sak,
        data->nfca_data.atqa[0],
        data->nfca_data.atqa[1]);

    mfc_data->type = type;
}

static void nfc_generate_mf_classic_mini(NfcDevData* data) {
    nfc_generate_mf_classic(data, 4, MfClassicTypeMini);
}

static void nfc_generate_mf_classic_1k_4b_uid(NfcDevData* data) {
    nfc_generate_mf_classic(data, 4, MfClassicType1k);
}

static void nfc_generate_mf_classic_1k_7b_uid(NfcDevData* data) {
    nfc_generate_mf_classic(data, 7, MfClassicType1k);
}

static void nfc_generate_mf_classic_4k_4b_uid(NfcDevData* data) {
    nfc_generate_mf_classic(data, 4, MfClassicType4k);
}

static void nfc_generate_mf_classic_4k_7b_uid(NfcDevData* data) {
    nfc_generate_mf_classic(data, 7, MfClassicType4k);
}

static const NfcDataGenerator nfc_data_generator[NfcDataGeneratorTypeNum] = {
    [NfcDataGeneratorTypeMfUltralight] =
        {
            .name = "Mifare Ultralight",
            .handler = nfc_generate_mf_ul_orig,
        },
    [NfcDataGeneratorTypeMfUltralightEV1_11] =
        {
            .name = "Mifare Ultralight EV1 11",
            .handler = nfc_generate_mf_ul_11,
        },
    [NfcDataGeneratorTypeMfUltralightEV1_H11] =
        {
            .name = "Mifare Ultralight EV1 H11",
            .handler = nfc_generate_mf_ul_h11,
        },
    [NfcDataGeneratorTypeMfUltralightEV1_21] =
        {
            .name = "Mifare Ultralight EV1 21",
            .handler = nfc_generate_mf_ul_21,
        },
    [NfcDataGeneratorTypeMfUltralightEV1_H21] =
        {
            .name = "Mifare Ultralight EV1 H21",
            .handler = nfc_generate_mf_ul_h21,
        },
    [NfcDataGeneratorTypeNTAG203] =
        {
            .name = "NTAG203",
            .handler = nfc_generate_ntag203,
        },
    [NfcDataGeneratorTypeNTAG213] =
        {
            .name = "NTAG213",
            .handler = nfc_generate_ntag213,
        },
    [NfcDataGeneratorTypeNTAG215] =
        {
            .name = "NTAG215",
            .handler = nfc_generate_ntag215,
        },
    [NfcDataGeneratorTypeNTAG216] =
        {
            .name = "NTAG216",
            .handler = nfc_generate_ntag216,
        },
    [NfcDataGeneratorTypeNTAGI2C1k] =
        {
            .name = "NTAG I2C 1k",
            .handler = nfc_generate_ntag_i2c_1k,
        },
    [NfcDataGeneratorTypeNTAGI2C2k] =
        {
            .name = "NTAG I2C 2k",
            .handler = nfc_generate_ntag_i2c_2k,
        },
    [NfcDataGeneratorTypeNTAGI2CPlus1k] =
        {
            .name = "NTAG I2C Plus 1k",
            .handler = nfc_generate_ntag_i2c_plus_1k,
        },
    [NfcDataGeneratorTypeNTAGI2CPlus2k] =
        {
            .name = "NTAG I2C Plus 2k",
            .handler = nfc_generate_ntag_i2c_plus_2k,
        },
    [NfcDataGeneratorTypeMfClassicMini] =
        {
            .name = "Mifare Mini",
            .handler = nfc_generate_mf_classic_mini,
        },
    [NfcDataGeneratorTypeMfClassic1k_4b] =
        {
            .name = "Mifare Classic 1k 4byte UID",
            .handler = nfc_generate_mf_classic_1k_4b_uid,
        },
    [NfcDataGeneratorTypeMfClassic1k_7b] =
        {
            .name = "Mifare Classic 1k 7byte UID",
            .handler = nfc_generate_mf_classic_1k_7b_uid,
        },
    [NfcDataGeneratorTypeMfClassic4k_4b] =
        {
            .name = "Mifare Classic 4k 4byte UID",
            .handler = nfc_generate_mf_classic_4k_4b_uid,
        },
    [NfcDataGeneratorTypeMfClassic4k_7b] =
        {
            .name = "Mifare Classic 4k 7byte UID",
            .handler = nfc_generate_mf_classic_4k_7b_uid,
        },
};

const char* nfc_data_generator_get_name(NfcDataGeneratorType type) {
    return nfc_data_generator[type].name;
}

void nfc_data_generator_fill_data(NfcDataGeneratorType type, NfcDevData* data) {
    nfc_data_generator[type].handler(data);
}