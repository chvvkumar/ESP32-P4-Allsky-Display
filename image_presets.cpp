// image_presets.cpp
#include "image_presets.h"
#include <string.h>

const ImagePreset IMAGE_PRESETS[] = {
    // id,                  label,                         url,                                                                       nominalPx, cropPct
    { "sdo_aia_304",  "Sun SDO/AIA 304A",   "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_1024_0304.jpg",                 1024, 88 },
    { "sdo_aia_171",  "Sun SDO/AIA 171A",   "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_1024_0171.jpg",                 1024, 88 },
    { "sdo_aia_193",  "Sun SDO/AIA 193A",   "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_1024_0193.jpg",                 1024, 88 },
    { "sdo_hmi_igr",  "Sun SDO/HMI Cont.",  "https://soho.nascom.nasa.gov/data/realtime/hmi_igr/1024/latest.jpg",               1024, 92 },
    { "sdo_hmi_mag",  "Sun SDO/HMI Magn.",  "https://soho.nascom.nasa.gov/data/realtime/hmi_mag/1024/latest.jpg",               1024, 92 },
    { "soho_c2",      "Sun SOHO LASCO C2",  "https://soho.nascom.nasa.gov/data/realtime/c2/1024/latest.jpg",                    1024, 90 },
    { "soho_c3",      "Sun SOHO LASCO C3",  "https://soho.nascom.nasa.gov/data/realtime/c3/1024/latest.jpg",                    1024, 90 },
    { "goes19_full",  "Earth GOES-19",      "https://cdn.star.nesdis.noaa.gov/GOES19/ABI/FD/GEOCOLOR/1808x1808.jpg",            1808, 96 },
};
const int IMAGE_PRESET_COUNT = (int)(sizeof(IMAGE_PRESETS) / sizeof(IMAGE_PRESETS[0]));

const ImagePreset* findImagePreset(const char* id) {
    if (!id) return nullptr;
    for (int i = 0; i < IMAGE_PRESET_COUNT; i++) {
        if (strcmp(IMAGE_PRESETS[i].id, id) == 0) return &IMAGE_PRESETS[i];
    }
    return nullptr;
}
