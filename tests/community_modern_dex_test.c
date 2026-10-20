#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "port/community_modern_dex.h"

static uint32_t CRC(const uint8_t *p, unsigned size)
{
    uint32_t crc = 0xffffffffu;
    for (unsigned i=0;i<size;++i) {
        crc ^= p[i];
        for (unsigned bit=0;bit<8;++bit) crc = (crc & 1) ? (crc >> 1)^0xedb88320u : crc >> 1;
    }
    return ~crc;
}
static void Unpack(const uint8_t *carrier,uint8_t *record)
{
    memset(record,0,62);
    for(unsigned i=0;i<496;++i) record[i/8] |= (carrier[i]>>7)<<(i%8);
}
static void Pack(uint8_t *carrier,uint8_t *record)
{
    uint32_t crc=CRC(record,58);
    for(unsigned i=0;i<4;++i) record[58+i]=(uint8_t)(crc>>(i*8));
    for(unsigned i=0;i<496;++i) carrier[i]=(carrier[i]&0x7f)|(((record[i/8]>>(i%8))&1)<<7);
}
static void Invalid(uint8_t *carrier)
{
    uint8_t before[496];memcpy(before,carrier,496);
    CommunityModernDexEntry entry={91,92,93};
    assert(CommunityModernDex_Read(carrier,496,512,&entry)==COMMUNITY_MODERN_DEX_INVALID);
    assert(entry.seen==91&&entry.caught==92&&entry.genders==93);
    assert(!CommunityModernDex_Record(carrier,496,512,1,0));
    assert(!memcmp(before,carrier,496));
}
int main(void)
{
    assert(CRC((const uint8_t *)"123456789",9)==0xcbf43926u);
    uint8_t carrier[496],lower[496],valid[496],record[62];
    for(unsigned i=0;i<496;++i) lower[i]=carrier[i]=(uint8_t)((i*43+19)&127);
    CommunityModernDexEntry entry;
    assert(CommunityModernDex_Read(carrier,496,512,&entry)==COMMUNITY_MODERN_DEX_LEGACY);
    assert(!entry.seen&&!entry.caught&&!entry.genders&&!memcmp(carrier,lower,496));
    entry=(CommunityModernDexEntry){91,92,93};
    assert(CommunityModernDex_Read(carrier,495,512,&entry)==COMMUNITY_MODERN_DEX_INVALID);
    assert(CommunityModernDex_Read(NULL,496,512,&entry)==COMMUNITY_MODERN_DEX_INVALID);
    assert(entry.seen==91&&entry.caught==92&&entry.genders==93);
    assert(!CommunityModernDex_Record(carrier,495,512,1,0));
    assert(!CommunityModernDex_Record(carrier,496,511,1,0));
    assert(!CommunityModernDex_Record(carrier,496,577,1,0));
    assert(!CommunityModernDex_Record(carrier,496,512,2,0));
    assert(!CommunityModernDex_Record(carrier,496,512,1,3));
    assert(!memcmp(carrier,lower,496));
    for(unsigned i=0;i<65;++i) {
        assert(CommunityModernDex_Record(carrier,496,512+i,i%2,i%3));
        assert(CommunityModernDex_Read(carrier,496,512+i,&entry)==COMMUNITY_MODERN_DEX_VALID);
        assert(entry.seen&&entry.caught==i%2&&entry.genders==(i%3<2 ? 1u<<(i%3) : 0));
        for(unsigned j=0;j<496;++j) assert((carrier[j]&127)==lower[j]);
    }
    for(unsigned i=0;i<65;++i) {
        assert(CommunityModernDex_Record(carrier,496,512+i,1,0));
        assert(CommunityModernDex_Record(carrier,496,512+i,0,1));
        assert(CommunityModernDex_Read(carrier,496,512+i,&entry)==1);
        assert(entry.seen&&entry.caught&&entry.genders==3);
    }
    memcpy(valid,carrier,496);
    /* Every single carrier-bit flip is refused, without altering caller data. */
    for(unsigned bit=0;bit<496;++bit) {
        memcpy(carrier,valid,496);carrier[bit]^=128;Invalid(carrier);
    }
    const unsigned offsets[]={4,5,6,7,16,25,42,43,57};
    const unsigned values[]={2,64,1,3,129,129,131,1,1};
    for(unsigned i=0;i<sizeof(offsets)/sizeof(offsets[0]);++i) {
        memcpy(carrier,valid,496);Unpack(carrier,record);
        record[offsets[i]]=(uint8_t)values[i];Pack(carrier,record);Invalid(carrier);
    }
    memcpy(carrier,valid,496);Unpack(carrier,record);
    record[8]&=~1u;Pack(carrier,record);Invalid(carrier); /* caught without seen */
    record[17]&=~1u;Pack(carrier,record);Invalid(carrier); /* gender without seen */
    memset(carrier,0,496);carrier[495]=128;Invalid(carrier); /* unknown legacy data */
    assert(!CommunityModernDex_IsSpecies(511)&&CommunityModernDex_IsSpecies(512));
    assert(CommunityModernDex_IsSpecies(576)&&!CommunityModernDex_IsSpecies(577));
    return 0;
}
