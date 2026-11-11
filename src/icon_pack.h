/* Runtime side of scripts/gen_icons.py: unpacks one icon into a .swatchbin image and gives it to the game
   (mod platform set_file) under media\ui\textures\data_bound\Upgrade_Parts.zip\Custom\<prefix>_<slot>[_LG]_Icon.swatchbin.
   Works as C and C++, needs no libc. Include after icon_data.h. */
#pragma once
#define ICON_FILE_MAX 524371u
static unsigned char g_icon_buf[ICON_FILE_MAX];

static unsigned icon_unpack(const IconEntry *e, unsigned char *out){
    const unsigned char *h = e->large ? k_icon_hdr_large : k_icon_hdr_small;
    unsigned n = 0, i;
    for(i = 0; i < 0x53; i++) out[n++] = h[i];
    const unsigned char *p = e->rle, *end = e->rle + e->rle_size;
    while(p + 17 <= end){
        unsigned cnt = p[0], k;
        for(k = 0; k < cnt; k++){ unsigned j; for(j = 0; j < 16; j++) out[n++] = p[1 + j]; }
        p += 17;
    }
    return n;
}
/* "Custom\nitro_kit_LG_Icon.swatchbin" style names; returns length (without 0). */
static int icon_name(const IconEntry *e, char *dst, int sep){
    static const char pre[] = "Custom";
    int n = 0, i; const char *s;
    for(i = 0; pre[i]; i++) dst[n++] = pre[i];
    dst[n++] = (char)sep;
    for(s = ICON_PREFIX; *s; s++) dst[n++] = *s;
    dst[n++] = '_';
    for(s = e->slot; *s; s++) dst[n++] = *s;
    if(e->large){ dst[n++] = '_'; dst[n++] = 'L'; dst[n++] = 'G'; }
    { static const char suf[] = "_Icon.swatchbin"; for(i = 0; suf[i]; i++) dst[n++] = suf[i]; }
    dst[n] = 0;
    return n;
}
static int icon_slot_equal(const char *a, const char *b){ while(*a && *a == *b){ a++; b++; } return *a == *b; }
/* 1 when the mod carries a picture for that slot (large = the 916x458 one) */
static int icon_has(const char *slot, int large){
    const IconEntry *e;
    for(e = k_icons; e->slot; e++) if(e->large == large && icon_slot_equal(e->slot, slot)) return 1;
    return 0;
}
