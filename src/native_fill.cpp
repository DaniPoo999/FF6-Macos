#include "ff6/native_fill.hpp"
#include <algorithm>
#include <optional>
#include <bit>

namespace ff6 {
namespace { void require(bool value, const char* why); }
std::array<std::uint32_t, 2> native_particle_pair_layout(ByteView operands,
                                                       unsigned group_stride) {
    require(operands.size()==6 && group_stride>0 && group_stride<=65535,
            "Invalid paired-particle operand layout");
    const unsigned extent=read16(operands,0);
    require(extent==read16(operands,2) && extent%group_stride==0 &&
                operands[4]==operands[5], "Paired-particle aliases disagree");
    const unsigned groups=extent/group_stride, interval=operands[4];
    // This bounded native lowering keeps the final destination in a register.
    // Its register allocation supports up to seven pairs; this is an encoder
    // limit, not a default or a selected original particle count.
    require(groups>=2 && groups<=7 && interval>0 && groups*2*interval<=255,
            "Paired particles exceed native lowering bounds");
    return {groups,interval};
}

Bytes make_native_particle_pairs(ByteView operands, unsigned group_stride) {
    const auto shape=native_particle_pair_layout(operands,group_stride);
    const unsigned groups=shape[0], interval=shape[1];
    // Pinned C++ ABI: optional task records have a 192-byte payload followed
    // by a presence byte, in a 200-byte record. x10 already points to the
    // source; x19 owns the object. Native member/register positions below
    // contain no original copy count, delay, payload or CPU instructions.
    constexpr unsigned owner=19, source=10, tasks=6736, record=200, group=record*8;
    Bytes code;
    const auto word=[&](std::uint32_t v){append32(code,v);};
    const auto mov=[&](unsigned reg,unsigned value){require(value<=65535,"Particle address overflow");word(0x52800000U|(value<<5U)|reg);};
    const auto address=[&](unsigned reg,unsigned value){mov(reg,value);word(0x8b000000U|(reg<<16U)|(owner<<5U)|reg);};
    const auto pair=[&](bool load,unsigned a,unsigned b,unsigned base,unsigned offset){
        require(offset%16==0 && offset/16<64,"Particle vector-pair offset overflow");
        word((load?0xad400000U:0xad000000U)|((offset/16)<<15U)|(b<<10U)|(base<<5U)|a);
    };
    const auto vector=[&](bool load,unsigned reg,unsigned base,unsigned offset){
        require(offset%16==0 && offset/16<4096,"Particle vector offset overflow");
        word((load?0x3dc00000U:0x3d800000U)|((offset/16)<<10U)|(base<<5U)|reg);
    };
    const auto byte=[&](bool load,unsigned reg,unsigned base,unsigned offset){
        require(offset<4096,"Particle byte offset overflow");
        word((load?0x39400000U:0x39000000U)|(offset<<10U)|(base<<5U)|reg);
    };
    const auto odd_register=[&](unsigned g){return g==6?0U:12U+g;};
    const auto publish=[&](unsigned g,unsigned particle,unsigned member,unsigned reg){
        const unsigned address=tasks+g*group+particle*record+member;
        const unsigned base=g<2?24U:g<5?25U:23U;
        const unsigned origin=g<2?5113U:g<5?9936U:14739U;
        require(address>=origin,"Particle publication precedes native base");
        byte(false,reg,base,address-origin);
    };
    // First copy seeds vector values retained for the final destination. The
    // compiler interleaves fresh source reads, preserving its alias semantics.
    pair(true,2,0,source,96);address(11,tasks);
    pair(true,3,1,source,80);pair(false,1,0,11,96);
    pair(true,4,0,source,160);pair(true,5,1,source,144);pair(false,1,0,11,160);
    pair(true,6,0,source,128);pair(true,7,1,source,112);pair(false,1,0,11,128);
    pair(true,16,1,source,32);pair(true,0,17,source,16);pair(false,17,1,11,32);
    pair(true,17,1,source,64);pair(true,18,19,source,48);pair(false,19,1,11,64);
    pair(true,19,20,source,0);pair(false,19,20,11,0);
    for(unsigned g=0;g<groups;++g) {
        if(g>0) {
            const unsigned dest=tasks+g*group;
            // Vector pair order and reversed stores are native compiler
            // scheduling. The destination count is entirely input-owned.
            for(const unsigned offset : {48U,16U,112U,80U,160U}) {
                pair(true,19,20,source,offset);
                vector(false,20,owner,dest+offset+16);
                vector(false,19,owner,dest+offset);
            }
            vector(true,19,source,144);vector(false,19,owner,dest+144);
            vector(true,19,source,0);vector(false,19,owner,dest);
        }
        const auto dest=odd_register(g);address(dest,tasks+g*group+record);
        if(g+1==groups) {
            pair(false,16,18,dest,32);pair(false,2,7,dest,96);pair(false,17,3,dest,64);
            vector(true,2,source,176);pair(false,4,2,dest,160);pair(false,6,5,dest,128);
        } else {
            pair(true,20,19,source,48);vector(true,21,source,32);pair(false,21,20,dest,32);
            if(g==0)pair(true,1,20,source,0);
            pair(true,22,21,source,112);vector(true,23,source,96);pair(false,23,22,dest,96);
            vector(true,22,source,80);pair(false,19,22,dest,64);
            pair(true,22,19,source,160);pair(false,22,19,dest,160);
            vector(true,19,source,144);pair(false,21,19,dest,128);
            if(g==0)vector(true,19,source,0);else pair(true,19,20,source,0);
            pair(false,19,20,dest,0);
        }
    }
    const auto last=odd_register(groups-1);
    byte(true,last==0?1:0,source,192);byte(false,last==0?1:0,11,192);
    for(unsigned i=1;i<groups*2-1;++i) {
        if(i%2) {byte(true,11,source,192);byte(false,11,odd_register(i/2),192);}
        else {address(11,tasks+(i/2)*group);byte(true,12,source,192);byte(false,12,11,192);}
    }
    for(unsigned i=0;i<(groups-1)*2;++i) {mov(11,(i+1)*interval);publish(i/2,i%2,3,11);}
    mov(11,1);
    for(unsigned g=0;g<groups;++g)publish(g,0,97,11);
    mov(11,(groups*2-1)*interval);publish(groups-1,0,3,11);
    for(unsigned g=0;g+1<groups;++g)publish(g,1,97,31);
    byte(true,10,source,192);byte(false,10,last,192);pair(false,1,0,last,0);
    publish(groups-1,1,97,31);mov(10,groups*2*interval);publish(groups-1,1,3,10);
    return code;
}

Bytes make_native_coordinate_pair(ByteView coordinates,
                                  std::span<const std::uint32_t> format) {
    require(coordinates.size() == 2 && coordinates[1] > coordinates[0] &&
                format.size() == 3, "Invalid coordinate-pair layout");
    const auto input = format[0], scratch = format[1], output = format[2];
    require(input < 31 && scratch < 31 && output < 31 &&
                scratch != input && scratch != output,
            "Invalid coordinate-pair native registers");
    const unsigned span = coordinates[1] - coordinates[0];
    const auto low = static_cast<unsigned>(std::countr_zero(span));
    const auto high = static_cast<unsigned>(std::bit_width(span));
    const auto run = span >> low;
    require(run >= 3 && (run & (run + 1U)) == 0,
            "Coordinate span has no bounded shift/subtract lowering");
    Bytes code;
    append32(code, 0x53000000U | ((32U - high) << 16U) |
                       ((31U - high) << 10U) | (input << 5U) | scratch);
    append32(code, 0x4b000000U | (input << 16U) | (low << 10U) |
                       (scratch << 5U) | output);
    if (coordinates[0])
        append32(code, 0x11000000U | (unsigned{coordinates[0]} << 10U) |
                           (output << 5U) | output);
    return code;
}

Bytes make_native_text_column_fragment(unsigned width, unsigned columns,
                                      std::span<const std::uint32_t> format) {
    require(format.size() == 7 && columns > 0 && columns <= 64 &&
                width > 0 && width <= columns, "Invalid native text-column extent");
    const auto operation = format[0], rows = format[1];
    const auto base = format[2], scratch = format[3];
    require(base < 31 && scratch < 31 && base != scratch,
            "Invalid native text-column registers");
    Bytes code;
    if (operation == 1) {
        require(rows == 0 && format[4] == 0 && format[5] == 0 && format[6] == 0,
                "Unexpected native column-scale metadata");
        const auto low = std::countr_zero(width);
        const auto high = static_cast<unsigned>(std::bit_width(width));
        const auto run = width >> low;
        require(run >= 3 && (run & (run + 1U)) == 0,
                "Column stride has no bounded shift/subtract lowering");
        // Multiply by a contiguous run of set bits: (input << high) -
        // (input << low). Both shift choices derive from the supplied width.
        append32(code, 0xd3400000U | ((64U - high) << 16U) |
                           ((63U - high) << 10U) | (scratch << 5U) | base);
        append32(code, 0xcb000000U | (scratch << 16U) |
                           (static_cast<unsigned>(low) << 10U) | (base << 5U) | base);
        return code;
    }
    require(operation == 0 && rows > 0 && rows <= 2 && format[4] < 32 &&
                format[5] < 31 && format[6] < 31 &&
                format[5] != base && format[5] != scratch &&
                format[6] != base && format[6] != scratch,
            "Invalid native text-column clear ABI");
    // The caller's base points at the last payload byte of the final cell.
    // A RasterCell has 13 value bytes in a 16-byte record; preserve padding.
    const unsigned anchor = ((rows - 1U) * columns + width) * 16U - 4U;
    constexpr std::array<unsigned, 3> component{0, 8, 12};
    constexpr std::array<std::uint32_t, 3> scaled{0xfd000000U, 0xb9000000U, 0x39000000U};
    constexpr std::array<std::uint32_t, 3> unscaled{0xfc000000U, 0xb8000000U, 0x38000000U};
    for (unsigned row = 0; row < rows; ++row) {
        for (unsigned column = 0; column < width; ++column) {
            for (unsigned part = 0; part < component.size(); ++part) {
                const auto position = (row * columns + column) * 16U + component[part];
                const auto distance = anchor - position;
                const auto value = format[4 + part];
                if (distance == 0) {
                    append32(code, scaled[part] | (base << 5U) | value);
                } else if (distance <= 256) {
                    append32(code, unscaled[part] | (((512U - distance) & 511U) << 12U) |
                                       (base << 5U) | value);
                } else {
                    require(distance <= 4095, "Native text-column address exceeds immediate range");
                    append32(code, 0xd1000000U | (distance << 10U) | (base << 5U) | scratch);
                    append32(code, scaled[part] | (scratch << 5U) | value);
                }
            }
        }
    }
    return code;
}

std::array<std::uint32_t, 3> native_text_region_layout(
    ByteView operands, std::span<const std::uint32_t> format) {
    require(format.size()==5,"Invalid text region format");
    const auto base=format[0], bytes=format[1], columns=format[2], rows=format[3], aliases=format[4];
    require(base<=65535 && bytes>0 && bytes<=4 && columns>0 && columns<=256 &&
                rows>0 && rows<=256 && aliases>0 && aliases<=32 &&
                columns*bytes*(rows+1)<=65536-base && operands.size()%2==0 &&
                operands.size()>aliases*2 && (operands.size()/2-aliases)%3==0 &&
                (operands.size()/2-aliases)/3<=32,"Invalid text region extent");
    const unsigned origin=read16(operands,0);
    require(origin>=base && (origin-base)%(columns*bytes)==0 &&
                origin-base<=columns*bytes*rows,"Unaligned text publication origin");
    for (unsigned i=1;i<aliases;++i)
        require(read16(operands,i*2)==origin,"Text publication aliases disagree");
    std::vector<std::pair<unsigned,unsigned>> spans;
    unsigned height=0;
    for (std::size_t at=aliases*2;at<operands.size();at+=6) {
        const unsigned address=read16(operands,at), width_bytes=read16(operands,at+2), h=read16(operands,at+4);
        const auto width=width_bytes/bytes;
        require(address>=origin && (address-origin)%bytes==0 &&
                    (address-origin)/bytes<columns && width_bytes%bytes==0 && width>0 && width<=columns-(address-origin)/bytes &&
                    h>0 && h<=rows && (height==0 || height==h),"Invalid or incompatible text rectangle");
        const auto left=(address-origin)/bytes;
        spans.emplace_back(left,left+width);height=h;
    }
    std::sort(spans.begin(),spans.end());
    for (std::size_t i=1;i<spans.size();++i)
        require(spans[i-1].second==spans[i].first,"Text rectangles overlap or leave a gap");
    return {spans.front().first,spans.back().second,height};
}

Bytes make_native_text_region_copy(ByteView operands, std::span<const std::uint32_t> format) {
    require(format.size()==8,"Invalid native text-copy ABI");
    const auto region=native_text_region_layout(operands,format.first(5));
    const auto owner=format[5], member=format[6], stack=format[7];
    const auto columns=format[2], stride=columns*16, extent=region[2]*stride;
    const auto count=region[1]-region[0];
    // RasterCell has a 13-byte payload in a 16-byte native record. Copying
    // overlapping head/tail words preserves its padding and exact compiler ABI.
    require(count<=10 && stride<=4095 && extent<=65536 &&
                extent+(region[1]-1)*16<32768 && owner>=18 && owner<31 &&
                member<=32760 && member%8==0 && stack<=32760 && stack%8==0,
            "Text copy exceeds native ABI bounds");
    Bytes code; const auto word=[&](std::uint32_t v){append32(code,v);};
    word(0x92800008U|((extent-1)<<5U)); // negative row displacement in x8
    for (unsigned i=0;i<count;++i) {
        const auto reg=i<9?9+i:0;
        word(0x52800000U|((extent+(region[0]+i)*16)<<5U)|reg);
    }
    const auto body=code.size();
    for (unsigned i=0;i<count;++i) {
        const auto reg=i<9?9+i:0, offset=extent+(region[0]+i)*16;
        word(0xf9400001U|((member/8)<<10U)|(owner<<5U));
        word(0x8b080021U); // source row
        word(0x8b000022U|(reg<<16U));
        word(0xf94003e3U|((stack/8)<<10U));
        word(0x8b080063U); // destination row
        word(0x8b000064U|(reg<<16U));
        word(0xf9400021U|((offset/8)<<10U));
        word(0xf8405042U); word(0xf8005082U); // payload tail at byte five
        word(0xf9000061U|((offset/8)<<10U));
    }
    word(0xb1000108U|(stride<<10U));
    const auto displacement=(static_cast<std::int64_t>(body)-static_cast<std::int64_t>(code.size()))/4;
    word(0x54000001U|((static_cast<std::uint32_t>(displacement)&0x7ffffU)<<5U));
    return code;
}

Bytes make_native_text_region_clear(ByteView operands, std::span<const std::uint32_t> format) {
    const auto region=native_text_region_layout(operands,format);
    const auto stride=format[2]*16, extent=region[2]*stride;
    require(stride<=4095 && extent<=4095,"Text clear exceeds native immediate bounds");
    // x9 owns cells, x8 the row displacement; d0/w10/w11 contain the native
    // blank cell. The caller supplies these values; this block owns geometry.
    Bytes code;const auto word=[&](std::uint32_t v){append32(code,v);};
    word(0x8b08012cU);
    for (auto x=region[0];x<region[1];++x) {
        word(0xfd000180U|((x*2)<<10U));
        word(0xb900018aU|((x*4+2)<<10U));
        if (x+1==region[1]) word(0x91000108U|(stride<<10U));
        word(0x3900018bU|((x*16+12)<<10U));
    }
    word(0xf100011fU|(extent<<10U));
    const auto displacement=-static_cast<std::int64_t>(code.size()/4);
    word(0x54000001U|((static_cast<std::uint32_t>(displacement)&0x7ffffU)<<5U));
    return code;
}

std::array<std::uint32_t, 2> native_label_region_layout(ByteView operands,
    std::span<const std::uint32_t> format) {
    require(format.size()==5 && format[0]>0 && format[0]<=32 && format[1]>0 && format[1]<=32 &&
                format[2]>0 && format[2]<=4 && format[3]>0 && format[3]<=256 &&
                format[4]>0 && format[4]<=256 && operands.size()==2*(format[0]+format[1]),
            "Invalid label-region format");
    const unsigned width_bytes=read16(operands,0), height=read16(operands,format[0]*2);
    require(width_bytes%format[2]==0 && width_bytes>0 && width_bytes/format[2]<=format[3] &&
                height>0 && height<=format[4],"Invalid label region");
    for (unsigned i=0;i<format[0]+format[1];++i)
        require(read16(operands,i*2)==(i<format[0]?width_bytes:height),"Label rectangle aliases disagree");
    return {width_bytes/format[2],height};
}

Bytes make_native_label_region_clear(ByteView operands, std::span<const std::uint32_t> format) {
    const auto region=native_label_region_layout(operands,format);
    const auto width=region[0];
    // x10 owns cells; x8/x9 bound the rows. w11 holds the linear cell index.
    // Row start/end are supplied by the caller's separately restored fields.
    Bytes code;const auto word=[&](std::uint32_t v){append32(code,v);};
    word(0x1100016bU|((width-1)<<10U));
    word(0x2f00e420U); // native blank tile/palette
    word(0x5280014cU); word(0x5280002dU); // native priority and enabled flag
    const auto body=code.size();
    for (unsigned i=0;i<width;++i) {
        const auto remaining=width-1-i;
        if (remaining!=0) word(0x5100016eU|(remaining<<10U));
        word(0x8b20514eU|((remaining!=0?14U:11U)<<16U));
        word(0xfd0001c0U); word(0xb90009ccU);
        if (remaining==0) {
            word(0x91000508U);
            word(0x1100016bU|(format[3]<<10U));
        }
        word(0x390031cdU);
    }
    word(0xeb09011fU);
    const auto displacement=(static_cast<std::int64_t>(body)-static_cast<std::int64_t>(code.size()))/4;
    word(0x54000003U|((static_cast<std::uint32_t>(displacement)&0x7ffffU)<<5U));
    return code;
}

std::array<std::uint32_t, 5> native_paired_tile_copy_layout(
    ByteView operands, unsigned destination_origin, unsigned destination_tiles) {
    require(operands.size()==14 && destination_origin<65536 && destination_origin%16==0 &&
                destination_tiles>0 && destination_tiles<=512 &&
                destination_tiles*16<=65536-destination_origin,
            "Invalid paired tile-copy format");
    const unsigned base=read16(operands,0), first=read16(operands,2), end=read16(operands,4),
        second=read16(operands,6), second_end=read16(operands,8);
    require(base%32==0 && first%32==0 && second%32==0 && end>first &&
                second_end>second && end-first==second_end-second && (end-first)%32==0 &&
                (end-first)/32<=8 && second>=first && base+second_end<=65536,
            "Invalid paired tile-copy source intervals");
    const unsigned count=(end-first)/32;
    std::array<std::uint32_t,5> result{count,base+first,second-first,0,0};
    for (unsigned i=0;i<2;++i) {
        const unsigned address=read16(operands,10+2*i);
        require(address>=destination_origin && (address-destination_origin)%16==0,
                "Invalid paired tile-copy destination");
        result[3+i]=(address-destination_origin)/16;
        require(result[3+i]<destination_tiles && count<=destination_tiles-result[3+i],
                "Paired tile copy exceeds its destination");
    }
    require(result[3]+count<=result[4] || result[4]+count<=result[3],
            "Paired tile-copy destinations overlap");
    return result;
}

Bytes make_native_paired_tile_copy(ByteView operands, std::span<const std::uint32_t> format) {
    require(format.size()==6,"Invalid native paired tile-copy ABI");
    const auto layout=native_paired_tile_copy_layout(operands,format[0],format[1]);
    const auto begin=format[2], end=format[3], origin=format[4], error=format[5];
    require(begin<=32752 && begin%8==0 && end==begin+8 && origin%4==0 && error%4==0 &&
                origin<=UINT32_MAX-656U && (error<origin || error>=origin+96+80*(layout[0]-1)),
            "Invalid native paired tile-copy linkage");
    // Pinned native caller: x27 owns assets, x24 the copied tile vector,
    // w9 the character's tile origin, consumed by the final copy. x8/x10-x12
    // and q0-q3 are scratch.
    // A Tile is 64 decoded bytes; source offsets use 32-byte encoded tiles.
    Bytes code;
    const auto word=[&](std::uint32_t value){append32(code,value);};
    const auto index=[&](unsigned reg,unsigned value) {
        require(value<=4095,"Paired tile source exceeds native index encoding");
        word(0x11000000U|(value<<10U)|(9U<<5U)|reg);
    };
    const auto checked=[&](unsigned reg) {
        word(0xeb00001fU|(reg<<16U)|(10U<<5U));
        const auto distance=std::int64_t{error}-origin-static_cast<std::int64_t>(code.size());
        require(distance%4==0 && distance/4>=-262144 && distance/4<262144,
                "Paired tile bounds branch exceeds native range");
        word(0x54000009U|((static_cast<std::uint32_t>(distance/4)&0x7ffffU)<<5U));
    };
    const auto pointer=[&](unsigned target,unsigned reg) {
        word(0x8b001800U|(reg<<16U)|(8U<<5U)|target);
    };
    const auto load=[&](unsigned reg) {
        word(0xad400400U|(reg<<5U));
        word(0xad410c02U|(reg<<5U));
    };
    const auto store=[&](unsigned tile) {
        for (unsigned component=4;component>0;--component) {
            const unsigned v=component-1;
            word(0x3d800000U|((tile*4+v)<<10U)|(24U<<5U)|v);
        }
    };
    index(12,layout[1]/32);
    word(0xf940000aU|((end/8)<<10U)|(27U<<5U));
    word(0xf9400008U|((begin/8)<<10U)|(27U<<5U));
    word(0xcb08014aU); // native vector end minus begin
    word(0x9346fd4aU); // signed division by sizeof(Tile)
    checked(12);
    index(11,(layout[1]+layout[2])/32);
    pointer(12,12); load(12); store(layout[3]);
    checked(11);
    // The first second-run copy uses a materialized destination pointer and
    // paired stores. Subsequent copies use direct, descending vector stores.
    word(0x5280000cU|((layout[4]*64)<<5U));
    word(0x8b0c030cU);
    pointer(11,11); load(11);
    word(0xad010d82U); word(0xad000580U);
    for (unsigned i=1;i<layout[0];++i) {
        index(11,layout[1]/32+i); checked(11);
        pointer(11,11); load(11); store(layout[3]+i);
        const unsigned reg=i+1==layout[0]?9U:11U;
        const unsigned address=i+1==layout[0]?8U:11U;
        index(reg,(layout[1]+layout[2])/32+i); checked(reg);
        pointer(address,reg); load(address); store(layout[4]+i);
    }
    return code;
}

std::array<std::uint32_t, 2> native_clamped_phase_layout(
    ByteView operands, unsigned intervals, unsigned endpoints) {
    const auto valid=[](bool ok) {
        if (!ok) throw std::invalid_argument("Invalid clamped phase layout");
    };
    valid(intervals>0 && intervals<=256 && endpoints>0 && endpoints<=256 &&
          operands.size()==intervals+2*endpoints);
    const unsigned interval=operands.front();
    valid(std::has_single_bit(interval));
    for (unsigned i=0;i<intervals;++i) valid(operands[i]==interval);
    unsigned span=0;
    for (unsigned i=0;i<endpoints;++i) {
        const unsigned first=operands[intervals+2*i], last=operands[intervals+2*i+1];
        valid(first>=last);
        valid(i==0 || first-last==span);
        span=first-last;
    }
    return {interval,span};
}

Bytes make_native_clamped_phase(std::span<const std::uint32_t> layout,
                               std::span<const std::uint32_t> format) {
    const auto valid=[](bool ok) {
        if (!ok) throw std::invalid_argument("Invalid native clamped phase ABI");
    };
    valid(layout.size()==2 && format.size()==5);
    const auto bits=format[0], input=format[1], phase=format[2], limit=format[3], output=format[4];
    valid(bits>0 && bits<=8 && input<31 && phase<31 && limit<31 && output<31 &&
          input!=phase && input!=limit && phase!=limit &&
          layout[0]<=128 && std::has_single_bit(layout[0]) && layout[1]<=255);
    const auto shift=static_cast<unsigned>(std::countr_zero(layout[0]));
    Bytes code;
    // UBFX forms the periodic phase; MOVZ supplies the content-selected clamp.
    append32(code,0x53000000U|(shift<<16U)|((shift+bits-1U)<<10U)|(input<<5U)|phase);
    append32(code,0x52800000U|(layout[1]<<5U)|limit);
    unsigned condition=3; // unsigned lower
    if (layout[1]==(1U<<(bits-1U))) {
        // At the midpoint of a power-of-two domain, testing its high bit is
        // equivalent to comparing against the bound. Reproduce that compiler
        // fold only when the input data proves it; other bounds use CMP.
        append32(code,0x6a40001fU|(input<<16U)|(shift<<10U)|(limit<<5U));
        condition=0; // equal: the tested high bit was clear
    } else {
        append32(code,0x6b00001fU|(limit<<16U)|(phase<<5U));
    }
    append32(code,0x1a800000U|(limit<<16U)|(condition<<12U)|(phase<<5U)|output);
    return code;
}

std::array<std::uint32_t, 4> native_particle_strip_layout(
    ByteView operands, std::span<const std::uint32_t> groups) {
    const auto valid=[](bool ok) {
        if (!ok) throw std::invalid_argument("Invalid particle strip layout");
    };
    // Record width belongs to the six-field source format, not the number of
    // authored records. Both traversal directions must describe the same strip.
    valid(groups.size()==5 && groups[0]>=6 && groups[0]<=255 && groups[1]>0 &&
          groups[1]<=32 && groups[2]>0 && groups[2]<=32 && groups[3]>0 &&
          groups[3]<=32 && groups[4]>0 && groups[4]<=32);
    valid(operands.size()==2*groups[1]+groups[2]+groups[3]+groups[4]);
    unsigned count=0;
    for (unsigned i=0;i<groups[1];++i) {
        const unsigned extent=operands[2*i], stride=operands[2*i+1];
        valid(stride==groups[0] && extent>stride && extent%stride==0);
        const auto n=extent/stride;
        valid(i==0 || count==n); count=n;
    }
    std::array<std::uint32_t,4> result{count,0,0,0};
    std::size_t at=2*groups[1];
    for (unsigned group=2;group<5;++group) {
        const auto value=operands[at];
        for (unsigned i=0;i<groups[group];++i) valid(operands[at+i]==value);
        result[group-1]=value; at+=groups[group];
    }
    valid(std::has_single_bit(result[1]+1U) && std::has_single_bit(result[2]+1U));
    return result;
}

Bytes make_native_particle_strip(std::span<const std::uint32_t> layout,
                                std::span<const std::uint32_t> format) {
    const auto valid=[](bool ok) {
        if (!ok) throw std::invalid_argument("Invalid native particle strip ABI");
    };
    // Native register contract: assets=x27, presentation=x21, scene=x23,
    // age=w20. Only the caller-clobbered integer temporaries are scratch.
    // Format: record, stride, region kind, code origin, finished continuation,
    // split draw continuation, bounds error, throwing helper, exception landing,
    // draw helper, particle pointer member, sine begin member, finished member.
    valid(layout.size()==4 && format.size()==13);
    const auto record=format[0], stride=format[1], kind=format[2], origin=format[3];
    valid(layout[0]>=2 && layout[0]<=255 && layout[1]<=255 && layout[2]<=255 &&
          layout[3]<=255 && std::has_single_bit(layout[1]+1U) &&
          std::has_single_bit(layout[2]+1U) && record>0 && record<layout[0] &&
          stride>=6 && stride<=255 && record*stride+5<=4095 && kind<=3 &&
          format[10]<=32760 && format[10]%8==0 && format[11]<=32752 &&
          format[11]%8==0 && format[12]<=4095);
    for (unsigned i=3;i<10;++i) valid(format[i]%4==0);
    valid(origin<=UINT32_MAX-148U);
    if (kind==2) {
        for (unsigned i=4;i<9;++i) valid(format[i]==0);
        for (unsigned i=10;i<13;++i) valid(format[i]==0);
    } else {
        if (kind!=1) valid(format[5]==0);
        if (kind!=0) valid(format[7]==0 && format[8]==0);
    }
    const auto p=record*stride;
    Bytes out;
    const auto word=[&](std::uint32_t v){append32(out,v);};
    const auto branch=[&](unsigned target,unsigned bits,std::uint32_t opcode,unsigned shift) {
        const auto distance=std::int64_t{target}-std::int64_t{origin}-static_cast<std::int64_t>(out.size());
        valid(distance%4==0 && distance/4>=-(std::int64_t{1}<<(bits-1U)) &&
              distance/4<(std::int64_t{1}<<(bits-1U)));
        word(opcode|((static_cast<std::uint32_t>(distance/4)&((1U<<bits)-1U))<<shift));
    };
    const auto b=[&](unsigned target){branch(target,26,0x14000000U,0);};
    const auto bl=[&](unsigned target){branch(target,26,0x94000000U,0);};
    const auto condition=[&](unsigned target,unsigned condition){branch(target,19,0x54000000U|condition,5);};
    const auto byte=[&](unsigned dest,unsigned base,unsigned offset) {
        valid(dest<31 && base<31 && offset<=4095);
        word(0x39400000U|(offset<<10U)|(base<<5U)|dest);
    };
    const auto pointer=[&](unsigned dest,unsigned offset) {
        word(0xf9400000U|((offset/8U)<<10U)|(27U<<5U)|dest);
    };
    const auto move=[&](unsigned dest,unsigned source) {
        word(0xaa0003e0U|(source<<16U)|dest);
    };
    const auto mask=[&](unsigned reg,unsigned value) {
        if (value==0) word(0x2a1f03e0U|reg);
        else word(0x12000000U|((static_cast<unsigned>(std::bit_width(value))-1U)<<10U)|(reg<<5U)|reg);
    };
    const auto draw=[&]() {
        byte(11,9,p);byte(4,9,p+1);
        word(0x11000000U|(layout[3]<<10U)|(8U<<5U)|2U);
        word(0x0b000000U|(11U<<16U)|(10U<<5U)|3U);
        move(0,27);move(1,23);bl(format[9]);
    };
    if (kind==2) {draw();return out;}
    const bool split=kind==1, owns_error=kind==0;
    // Labels follow ordinary loop control: carry update, counter advance,
    // fractional addition, then checked sine lookup. Split regions place the
    // carry update last because their draw continuation is noncontiguous.
    const auto counter=origin+(split?56U:76U);
    const auto accumulate=origin+(split?68U:88U);
    const auto carry=origin+(split?92U:56U);
    const auto draw_at=split?format[5]:origin+(owns_error?120U:116U);
    if (owns_error) valid(format[6]==origin+112U);
    byte(8,21,format[12]);branch(format[4],14,0x37000008U,5);
    pointer(9,format[10]);byte(8,9,p+2);byte(10,9,p+5);
    branch(draw_at,19,0x34000014U,5); // Zero age needs no simulation.
    byte(16,9,p+3);byte(11,9,p+4);
    pointer(13,format[11]+8);pointer(12,format[11]);
    word(0xcb000000U|(12U<<16U)|(13U<<5U)|13U);
    word(0x52800000U|(1U<<5U)|14U);move(15,20);b(accumulate);
    const auto emit_carry=[&]() {
        word(0x11000400U|(8U<<5U)|8U);mask(8,layout[1]);
        word(0x38606800U|(10U<<16U)|(12U<<5U)|10U);mask(10,layout[2]);
        move(16,17);
    };
    if (!split) emit_carry();
    valid(origin+out.size()==counter);
    word(0x11000400U|(14U<<5U)|14U);
    word(0x71000400U|(15U<<5U)|15U);condition(draw_at,0);
    valid(origin+out.size()==accumulate);
    word(0x0b000000U|(11U<<16U)|(16U<<5U)|16U);
    word(0x71040000U|(16U<<5U)|17U);condition(counter,3);
    word(0x92401c00U|(14U<<5U)|10U);
    word(0xeb00001fU|(10U<<16U)|(13U<<5U));
    condition(split?format[6]:carry,split?9U:8U);
    if (split) {valid(origin+out.size()==carry);emit_carry();b(counter);}
    else {
        if (owns_error) {bl(format[7]);b(format[8]);}
        else b(format[6]);
        valid(origin+out.size()==draw_at);draw();
    }
    return out;
}

std::array<std::uint32_t, 3> native_palette_cycle_layout(
    ByteView operands, unsigned address_origin, unsigned colors) {
    const auto valid = [](bool ok) {
        if (!ok) throw std::invalid_argument("Invalid palette rotation layout");
    };
    valid(colors>=2 && colors<=128 && address_origin%2==0 &&
          address_origin<=65536U-2*colors && operands.size()>=10 &&
          operands.size()%4==2 && operands.size()<=4*colors+2);
    const auto pairs=(operands.size()-2)/4;
    std::vector<bool> destinations(colors);
    unsigned first=colors, last=0;
    for (std::size_t at=0; at+2<operands.size(); at+=4) {
        const unsigned source=read16(operands,at), destination=read16(operands,at+2);
        valid(source>=address_origin && destination>=address_origin &&
              source<address_origin+2*colors && destination<address_origin+2*colors &&
              source%2==0 && destination%2==0);
        const auto s=(source-address_origin)/2, d=(destination-address_origin)/2;
        valid(!destinations[d]); destinations[d]=true;
        first=std::min(first,std::min(s,d)); last=std::max(last,std::max(s,d));
    }
    const auto count=last-first+1;
    valid(count==pairs);
    // Each destination receives its next neighbor, wrapping at the selected
    // endpoint. This validates both permutation and direction; counting the
    // operands alone would incorrectly admit a gap or a different choreography.
    for (std::size_t at=0; at+2<operands.size(); at+=4) {
        const unsigned s=(read16(operands,at)-address_origin)/2;
        const unsigned d=(read16(operands,at+2)-address_origin)/2;
        valid(s==first+(d-first+1)%count);
    }
    const unsigned period=read16(operands,operands.size()-2);
    valid(period!=0 && std::has_single_bit(period));
    return {first,count,period};
}

Bytes make_native_palette_cycle(ByteView operands, std::span<const std::uint32_t> format) {
    const auto valid = [](bool ok) {
        if (!ok) throw std::invalid_argument("Invalid native palette rotation ABI");
    };
    valid(format.size()==6);
    const auto layout=native_palette_cycle_layout(operands,format[0],format[1]);
    const auto first=layout[0], count=layout[1], period=layout[2];
    const auto clock=format[2], flag=format[3], pointer=format[4], destination=format[5];
    valid(clock<=16380 && clock%4==0 && flag<=16380 && flag%4==0 &&
          pointer<=32760 && pointer%8==0 && destination<=8192 && destination%2==0 &&
          destination+2*(first+count)<=8192);
    // All source colors are read through the pointer; the stack palette must
    // not overwrite any of the independent inputs retained by this fragment.
    const auto overlaps=[](unsigned a,unsigned an,unsigned b,unsigned bn) {
        return a<b+bn && b<a+an;
    };
    valid(!overlaps(clock,4,flag,4) && !overlaps(clock,4,pointer,8) &&
          !overlaps(flag,4,pointer,8));
    for (const auto& input : {std::pair{clock,4U},std::pair{flag,4U},std::pair{pointer,8U}})
        valid(!overlaps(destination+2*first,2*count,input.first,input.second));
    Bytes out;
    const auto word=[&](std::uint32_t v){append32(out,v);};
    const auto mov=[&](unsigned rd,unsigned value){
        valid(value<=65535);word(0x52800000U|(value<<5U)|rd);
    };
    const auto add=[&](unsigned rd,unsigned rn,unsigned value){
        valid(value<=4095);word(0x11000000U|(value<<10U)|(rn<<5U)|rd);
    };
    const auto cmp=[&](unsigned rn,unsigned value){
        valid(value<=4095);word(0x7100001fU|(value<<10U)|(rn<<5U));
    };
    const auto select=[&](unsigned rd,unsigned yes,unsigned no,unsigned condition){
        word(0x1a800000U|(no<<16U)|(condition<<12U)|(yes<<5U)|rd);
    };
    const auto color=[&](unsigned rd,unsigned index){
        word(0x78605800U|(index<<16U)|(12U<<5U)|rd);
    };
    const auto store=[&](unsigned reg,unsigned i){
        word(0x790003e0U|(((destination/2)+first+i)<<10U)|reg);
    };
    const auto view=[&](){
        word(0xb94003e0U|((flag/4)<<10U)|20U);
        word(0x72001c1fU|(20U<<5U)); // TST low byte; caller consumes its flags.
    };
    word(0xb94003e0U|((clock/4)<<10U)|8U);
    word(0x53007c00U|(static_cast<unsigned>(std::countr_zero(period))<<16U)|(8U<<5U)|8U);
    if (count==3 && first>0 && period>=4) {
        // Three-way unrolling uses two bounded conditional wraps. With at
        // least two clock bits removed, ceil(2^32/count) is an exact quotient
        // multiplier over the entire remaining input domain. The division,
        // destinations and emitted stores all derive from the decoded cycle.
        const auto multiplier=static_cast<std::uint32_t>(((std::uint64_t{1}<<32U)+count-1U)/count);
        mov(9,multiplier&65535U);
        word(0x72a00000U|((multiplier>>16U)<<5U)|9U);
        word(0x9ba07c00U|(9U<<16U)|(8U<<5U)|9U); // UMULL
        word(0xd360fc00U|(9U<<5U)|9U); // high word of the product
        word(0x0b000000U|(9U<<16U)|(static_cast<unsigned>(std::countr_zero(count-1U))<<10U)|(9U<<5U)|9U);
        mov(10,first-1U);
        word(0x4b000000U|(9U<<16U)|(8U<<5U)|8U);
        // OR is valid only if every possible phase uses disjoint address bits.
        const bool disjoint=((first-1U)&(std::bit_ceil(count)-1U))==0;
        word((disjoint?0x2a000000U:0x0b000000U)|(10U<<16U)|(8U<<5U)|9U);
        add(10,8,first+1U);mov(11,first);cmp(8,count-1U);select(10,11,10,0);
        mov(11,first+count-1U);cmp(8,0);select(9,11,9,0);
        view();add(8,8,first);
        word(0xf94003e0U|((pointer/8)<<10U)|12U);
        color(8,8);
        word(0x1a9f17e0U|11U); // CSET EQ; preserve the preceding view flags.
        store(8,0);color(8,10);color(9,9);
        for (unsigned i=1; i<count; ++i) store(7U+i,i);
    } else {
        // Other cycle sizes use ordinary unsigned quotient/remainder and an
        // unrolled sequence of bounded wraps. They have a different native
        // extent, so frozen-template size/hash checks still fail closed.
        mov(10,count);
        word(0x1ac00800U|(10U<<16U)|(8U<<5U)|9U); // UDIV
        word(0x1b008000U|(10U<<16U)|(8U<<10U)|(9U<<5U)|8U); // MSUB
        word(0xf94003e0U|((pointer/8)<<10U)|12U);
        for (unsigned i=0; i<count; ++i) {
            add(9,8,i);cmp(9,count);
            word(0x51000000U|(count<<10U)|(9U<<5U)|10U);
            select(9,10,9,2);add(9,9,first);color(10,9);store(10,i);
        }
        view();word(0x1a9f17e0U|11U);
    }
    return out;
}

Bytes make_native_strided_row_fragment(std::span<const std::uint32_t> layout,
                                      std::span<const std::uint32_t> format,
                                      unsigned variant_count) {
    const auto valid = [](bool ok) {
        if (!ok) throw std::invalid_argument("Invalid native strided-row fragment");
    };
    valid(layout.size()==6 && format.size()==6 && variant_count>=2 && variant_count<=256);
    const auto rows=layout[0], split=layout[1], tail_row=layout[2], step=layout[3];
    valid(rows>0 && rows<=65535 && split<rows && tail_row<rows && step>0 && step<=255 &&
          layout[4]<=255 && layout[5]<=256U-variant_count);
    const auto op=format[0], a=format[1], b=format[2], c=format[3], extra=format[5];
    valid(op<=2 && a<31 && b<31 && c<31 && format[4]==0);
    const bool last = tail_row+1U==rows;
    Bytes out;
    if (op==0) {
        valid(extra==0 && b!=c);
        // If the tail is the final row, equality is sufficient; otherwise use
        // unsigned ordering. Flags come from the separately restored row test.
        const auto rn=last?c:b, rm=last?b:c, condition=last?0U:3U;
        append32(out,0x1a800000U|(rm<<16U)|(condition<<12U)|(rn<<5U)|a);
    } else if (op==1) {
        valid(b!=c && extra<=16380 && extra%4==0);
        append32(out,0xb94003e0U|((extra/4)<<10U)|c);
        // Addition can use disjoint bits only when every possible base is
        // aligned and the complete selected variant family fits below it.
        const bool disjoint = std::has_single_bit(step) && layout[4]%step==0 &&
                              layout[5]+variant_count-1U<step;
        append32(out,(disjoint?0x2a000000U:0x0b000000U)|(c<<16U)|(b<<5U)|a);
    } else {
        valid(a!=b && extra>0 && extra<=4095);
        const auto bound=last?rows*step:rows;
        valid(bound<=4095);
        append32(out,0x11000000U|(extra<<10U)|(c<<5U)|c);
        append32(out,0x7100001fU|(bound<<10U)|((last?b:a)<<5U));
    }
    return out;
}
namespace {
struct RowBand { unsigned first, count, x, y; };
RowBand read_row_band(ByteView operands, unsigned origin, unsigned rows) {
    const auto valid = [](bool condition) {
        if (!condition) throw std::invalid_argument("Unsupported native row-band layout");
    };
    valid(operands.size() == 8 && rows >= 32 && rows <= 256 && rows % 32 == 0 &&
          origin <= 65536U - rows * 4U);
    const unsigned address = read16(operands,0), extent = read16(operands,2);
    valid(address >= origin && (address-origin) % 4 == 0 && extent % 4 == 0);
    const RowBand band{(address-origin)/4,extent/4,read16(operands,4),read16(operands,6)};
    // This compiler profile uses one mixed boundary vector, then a full-band
    // vector. Other boundary shapes require another lowering and are rejected.
    valid(band.first < rows && band.first % 8 == 7 && band.count >= 9 &&
          band.count <= rows-band.first && (band.first+band.count) % 8 == 0 &&
          band.x > 0 && band.x <= 32767 && band.y <= 32767);
    return band;
}
} // namespace
Bytes make_row_band_pattern(ByteView operands, std::span<const std::uint32_t> format) {
    if (format.size() != 3 || format[2] > 3)
        throw std::invalid_argument("Invalid row-band vector format");
    const auto band = read_row_band(operands,format[0],format[1]);
    Bytes result;
    for (unsigned lane = 0; lane < (format[2] == 3 ? 12U : 4U); ++lane) {
        const auto pattern = format[2] == 3 ? lane / 4U : format[2];
        // Pool order is native lowering metadata: outside, mixed, then inside.
        const bool inside = pattern == 2 ||
            (pattern == 1 && lane % 4U >= band.first % 4);
        append16(result,static_cast<std::uint16_t>(inside ? band.x : 0));
        append16(result,static_cast<std::uint16_t>(band.y));
    }
    return result;
}
Bytes make_native_row_band_fill(ByteView operands, std::span<const std::uint32_t> format) {
    const auto valid = [](bool condition) {
        if (!condition) throw std::invalid_argument("Invalid native row-band linkage");
    };
    valid(format.size() == 7);
    const auto band = read_row_band(operands,format[0],format[1]);
    const unsigned rows=format[1], member=format[2], address=format[3];
    valid(member <= 4095U-rows*4U && member % 2 == 0 && address % 4 == 0 &&
          std::uint64_t{address} + rows + 24 <= std::uint64_t{UINT32_MAX} + 1);
    for (unsigned i=4; i<7; ++i) {
        valid(format[i] % 16 == 0 && format[i] <= UINT32_MAX-15U &&
              (std::uint64_t{format[i]}+16 <= address || format[i] >= std::uint64_t{address}+rows+24));
        for (unsigned j=4; j<i; ++j) valid(format[i] != format[j]);
    }
    Bytes code;
    const auto word = [&](std::uint32_t value) { append32(code,value); };
    const auto load = [&](unsigned pattern, unsigned reg, unsigned scratch) {
        const auto target = format[4+pattern];
        const auto pc = std::uint64_t{address}+code.size();
        const auto pages = (std::int64_t{target & ~4095U}-static_cast<std::int64_t>(pc & ~std::uint64_t{4095}))/4096;
        valid(pages >= -(std::int64_t{1}<<20) && pages < (std::int64_t{1}<<20));
        const auto encoded=static_cast<std::uint32_t>(pages)&0x1fffffU;
        word(0x90000000U | ((encoded&3U)<<29U) | ((encoded>>2U)<<5U) | scratch);
        word(0x3dc00000U | (((target&4095U)/16U)<<10U) | (scratch<<5U) | reg);
    };
    bool outside_loaded=false;
    std::optional<unsigned> inside_loaded;
    for (unsigned group=0; group<rows; group+=32) {
        // Four independent eight-row stores: addresses precede their paired
        // vector writes. Registers/scheduling are this native compiler's ABI.
        for (unsigned i=0; i<4; ++i)
            word(0x91000000U | ((member+(group+i*8U)*4U)<<10U) | (24U<<5U) | (8U+i));
        for (unsigned i=0; i<4; ++i) {
            const auto scratch=i==0 ? 12U : 8U;
            std::array<unsigned,2> registers{};
            for (unsigned half=0; half<2; ++half) {
                const auto first=group+i*8U+half*4U;
                const auto last=first+3U;
                const bool outside=last<band.first || first>=band.first+band.count;
                if (outside) {
                    if (!outside_loaded) { load(0,0,scratch); outside_loaded=true; }
                } else {
                    const unsigned pattern=first<band.first ? 1U : 2U;
                    if (inside_loaded!=pattern) { load(pattern,1,scratch); inside_loaded=pattern; }
                    registers[half]=1;
                }
            }
            word(0xad000000U | (registers[1]<<10U) | ((8U+i)<<5U) | registers[0]);
        }
    }
    valid(code.size()==rows+24U);
    return code;
}
Bytes make_native_palette_initialization(ByteView operands,
                                         std::span<const std::uint32_t> format) {
    const auto valid = [](bool condition) {
        if (!condition) throw std::invalid_argument("Invalid native palette initialization");
    };
    valid(operands.size() == 36 && format.size() == 7);
    const auto destination_origin = format[0], source_origin = format[1];
    const auto source_count = format[2], neutral_address = format[3];
    const auto destination_member = format[4], source_member = format[5];
    const auto neutral_member = format[6];
    valid(destination_origin <= 65536U - 512U && source_count > 0 && source_count <= 16);
    valid(source_origin <= 65536U - source_count * 32U && neutral_address <= 65535);
    valid(neutral_address < source_origin || neutral_address >= source_origin + source_count * 32U);
    valid(destination_member <= 4095U - 15U * 32U && destination_member % 2 == 0);
    valid(source_member % 16 == 0 && source_member <= 1024U - source_count * 32U);
    valid(neutral_member % 16 == 0 && neutral_member <= 992);
    const auto disjoint = [](unsigned a, unsigned n, unsigned b, unsigned m) {
        return a + n <= b || b + m <= a;
    };
    valid(disjoint(source_member, source_count * 32U, neutral_member, 32));
    valid(disjoint(destination_member, 512, source_member, source_count * 32U));
    valid(disjoint(destination_member, 512, neutral_member, 32));
    struct Transfer { unsigned destination, source; };
    std::array<Transfer, 9> transfers{};
    std::array<bool, 16> used{};
    for (unsigned i = 0; i < transfers.size(); ++i) {
        const unsigned destination = read16(operands, i * 4U);
        const unsigned source = read16(operands, i * 4U + 2U);
        valid(destination >= destination_origin && (destination - destination_origin) % 32U == 0);
        const auto bank = (destination - destination_origin) / 32U;
        valid(bank < used.size() && !used[bank]);
        used[bank] = true;
        auto offset = neutral_member;
        if (source != neutral_address) {
            valid(source >= source_origin && (source - source_origin) % 32U == 0);
            const auto index = (source - source_origin) / 32U;
            valid(index < source_count);
            offset = source_member + index * 32U;
        }
        transfers[i] = {destination_member + bank * 32U, offset};
    }
    std::vector<Transfer> repeated;
    for (unsigned i = 0; i < transfers.size(); ++i)
        if (i != 0 && i != 1 && i != 3) repeated.push_back(transfers[i]);
    valid(std::ranges::all_of(repeated, [&](const auto& row) {
        return row.source == repeated.front().source;
    }));
    // The native C++ fill names disjoint banks in ascending order. Sorting is
    // safe only because duplicate destinations were rejected above.
    std::ranges::sort(repeated, {}, &Transfer::destination);
    Bytes code;
    const auto word = [&](std::uint32_t value) { append32(code, value); };
    const auto address = [&](unsigned reg, unsigned offset) {
        word(0x91000000U | (offset << 10U) | reg); // ADD Xd, X0, #member
    };
    const auto load = [&](unsigned first, unsigned second, unsigned offset) {
        word(0xad400000U | ((offset / 16U) << 15U) | (second << 10U) | first);
    };
    const auto store = [&](unsigned first, unsigned second, unsigned pointer) {
        word(0xad000000U | (second << 10U) | (pointer << 5U) | first);
    };
    // Register reuse and interleaving describe this native compiler profile;
    // every source/destination selection comes from the transfer records.
    address(8, repeated[0].destination);
    load(1, 0, transfers[0].source);
    address(9, transfers[0].destination);
    store(1, 0, 9);
    address(9, transfers[1].destination);
    load(0, 2, transfers[1].source);
    store(0, 2, 9);
    address(9, transfers[3].destination);
    load(0, 2, transfers[3].source);
    store(0, 2, 9);
    load(0, 1, repeated[0].source);
    load(3, 2, repeated[0].source);
    store(0, 2, 8);
    for (unsigned i = 1; i + 1 < repeated.size(); ++i) {
        address(8, repeated[i].destination);
        store(i == 1 ? 0U : 3U, i == 1 ? 2U : 1U, 8);
    }
    address(9, repeated.back().destination);
    // Stop before the caller's task search. Its priority-dependent pointer is
    // restored by a separate ROM field. The unchanged caller stores the final
    // vector pair using X9, V3 and V1 prepared by this prefix.
    return code;
}
namespace {
void require(bool value, const char* why) {
    if (!value) throw std::invalid_argument(why);
}
// Expand an already normalized predicate into a compact selected value.
// Both base and difference come from the extracted choices.
void append_choice_scale(Bytes& code, unsigned base, unsigned scale, unsigned destination) {
    require(scale > 0 && scale < 8 && base + scale < 8 && destination < 31,
            "Invalid native choice scaling");
    const auto word = [&](std::uint32_t value) { append32(code, value); };
    const auto low = static_cast<unsigned>(std::countr_zero(scale));
    const auto odd = scale >> low;
    const auto merge = [&](unsigned distance) {
        word(0x2a000000U | (destination << 16U) | (distance << 10U) |
             (destination << 5U) | destination);
    };
    if (odd > 1) {
        const auto high = static_cast<unsigned>(std::bit_width(odd)) - 1U;
        if (std::popcount(odd) == 2) merge(high);
        else for (unsigned i = 0; i < high; ++i) merge(1);
    }
    if (low) word(0x53000000U | ((32U - low) << 16U) | ((31U - low) << 10U) |
                  (destination << 5U) | destination);
    if (base) word(0x11000000U | (base << 10U) | (destination << 5U) | destination);
}

// The caller owns NZCV; materialize either extracted value without changing
// flags or borrowing another register. Unit predicate encodings are ISA rules.
void append_condition_choice(Bytes& code, unsigned when_true, unsigned when_false,
                             unsigned destination, unsigned condition) {
    require(when_true < 8 && when_false < 8 && destination < 31 && condition < 14,
            "Invalid native condition choice");
    const auto word = [&](std::uint32_t value) { append32(code, value); };
    if (when_true == when_false) {
        word(0x52800000U | (when_true << 5U) | destination);
        return;
    }
    const auto base = std::min(when_true, when_false);
    const auto scale = std::max(when_true, when_false) - base;
    const auto selected = when_true < when_false ? (condition ^ 1U) : condition;
    word(0x1a9f07e0U | ((selected ^ 1U) << 12U) | destination);
    append_choice_scale(code, base, scale, destination);
}

struct Band { unsigned rows, shade; };

std::vector<Band> read_bands(ByteView source, bool pad_canvas) {
    require(!source.empty() && source.size() % 2 == 0 && source.size() <= 448,
            "Invalid native fill band extent");
    std::vector<Band> bands;
    unsigned rows = 0;
    for (std::size_t at = 0; at < source.size(); at += 2) {
        const unsigned length = source[at], color = source[at + 1];
        require(length > 0 && length <= 127 && length <= 224U - rows && (color & 224U) == 224U,
                "Invalid native fill gray band");
        bands.push_back({length, color & 31U});
        rows += length;
    }
    if (pad_canvas && rows < 224) bands.push_back({224U - rows, bands.back().shade});
    return bands;
}

// A small native-code writer for constant memory fills. Opcode encodings and
// register choices are platform/compiler metadata. Every extent, color and
// repeated fill is derived from the supplied bands; no authored band sequence
// or previously compiled block is stored here.
class FillWriter {
    Bytes code_;
    NativeFillLayout layout_;
    unsigned pattern_ = 0;
    bool zero_vector_ = false;
    void word(std::uint32_t value) {
        require(code_.size() < 4096, "Native fill fragment exceeds profile bound");
        append32(code_, value);
    }
    void scalar(unsigned reg, unsigned value, bool upper = false) {
        require(value <= 65535, "Native fill scalar exceeds instruction field");
        word((upper ? 0x72a00000U : 0x52800000U) | (value << 5U) | reg);
    }
    void store(unsigned reg, unsigned offset, unsigned scale, bool vector = false) {
        require((vector && scale == 4) || (!vector && (scale == 1 || scale == 2)),
                "Invalid native fill store width");
        if ((offset & ((1U << scale) - 1U)) == 0) {
            require((offset >> scale) <= 4095, "Native fill store exceeds scaled offset");
            const auto op = vector ? 0x3d800000U : scale == 1 ? 0x79000000U : 0xb9000000U;
            word(op | ((offset >> scale) << 10U) | (19U << 5U) | reg);
        } else {
            require(offset < 256, "Native fill store exceeds unscaled offset");
            const auto op = vector ? 0x3c800000U : scale == 1 ? 0x78000000U : 0xb8000000U;
            word(op | (offset << 12U) | (19U << 5U) | reg);
        }
    }
    void zero_chunk(unsigned offset, unsigned size) {
        require(size % 16 == 0, "Unaligned native vector fill");
        while (size >= 32) {
            size -= 32;
            require((offset + size) % 16 == 0 && (offset + size) / 16 <= 63,
                    "Native pair store exceeds its offset field");
            word(0xad000000U | (((offset + size) / 16) << 15U) | (19U << 5U));
        }
        if (size) store(0, offset, 4, true);
    }
public:
    explicit FillWriter(NativeFillLayout layout) : layout_(layout) {
        require(layout.code % 4 == 0 && layout.patterns % 16 == 0 && layout.fill_helper % 4 == 0,
                "Unaligned native fill linkage");
        // Native std::vector<Rgb555> data member at scene offset 0xb8.
        word(0xf9400000U | ((0xb8U / 8U) << 10U) | (28U << 5U) | 19U);
    }
    void zero(unsigned offset, unsigned size) {
        require(size >= 16, "Zero fill is outside the reviewed lowering profile");
        if (!zero_vector_) { word(0x6f00e400U); zero_vector_ = true; }
        if (size % 16) {
            store(0, offset + size - 16, 4, true);
            size -= size % 16;
        }
        // The reviewed native lowering schedules a large memset in two groups.
        // These are compiler store-width/scheduling rules, not game row counts.
        if (size >= 160) {
            zero_chunk(offset, 96);
            zero_chunk(offset + 96, size - 96);
        } else zero_chunk(offset, size);
    }
    void pixels(unsigned offset, unsigned low, std::optional<unsigned> high) {
        scalar(8, low * 1057U);
        if (high) scalar(8, *high * 1057U, true);
        store(8, offset, high ? 2 : 1);
    }
    void pattern(unsigned offset, unsigned size) {
        const auto target = std::uint64_t{layout_.patterns} + std::uint64_t{pattern_++} * 16;
        const auto pc = std::uint64_t{layout_.code} + code_.size();
        require(target <= UINT32_MAX && pc <= UINT32_MAX, "Native fill address overflow");
        const auto pages = static_cast<std::int64_t>(target >> 12U) -
                           static_cast<std::int64_t>(pc >> 12U);
        require(pages >= -(1 << 20) && pages < (1 << 20), "Native fill pool is out of range");
        const auto page_bits = static_cast<std::uint32_t>(pages) & 0x1fffffU;
        word(0x90000001U | ((page_bits & 3U) << 29U) | ((page_bits >> 2U) << 5U));
        word(0x91000021U | ((static_cast<std::uint32_t>(target) & 4095U) << 10U));
        require(offset <= 4095, "Native fill destination offset overflow");
        word(0x91000000U | (offset << 10U) | (19U << 5U));
        scalar(2, size);
        const auto delta = std::int64_t{layout_.fill_helper} -
                           (std::int64_t{layout_.code} + static_cast<std::int64_t>(code_.size()));
        require(delta % 4 == 0 && delta >= -(1 << 27) && delta < (1 << 27),
                "Native fill helper is out of branch range");
        word(0x94000000U | (static_cast<std::uint32_t>(delta / 4) & 0x3ffffffU));
        zero_vector_ = false; // Native call ABI does not preserve v0.
    }
    Bytes finish() { return std::move(code_); }
};

// A separate native compiler profile for inline fills. It reloads the vector
// pointer between runs because the C++ source accesses it through the scene.
// Register scheduling follows the pinned native ABI; colors, extents and run
// ordering are supplied data. Unsupported lowering shapes fail closed.
class InlineFillWriter {
    Bytes code_;
    void word(std::uint32_t value) {
        require(code_.size() < 4096, "Inline fill fragment exceeds profile bound");
        append32(code_, value);
    }
    void load(unsigned reg = 8) { word(0xf8400000U | (184U << 12U) | (19U << 5U) | reg); }
    void scalar(unsigned reg, unsigned value, bool upper = false) {
        require(value <= 65535, "Inline fill scalar exceeds instruction field");
        word((upper ? 0x72a00000U : 0x52800000U) | (value << 5U) | reg);
    }
    void duplicate(unsigned reg, bool wide = false) {
        word((wide ? 0x4e020c00U : 0x0e020c00U) | (reg << 5U) | 1U);
    }
    void add(unsigned destination, unsigned base, unsigned offset) {
        require(offset <= 4095, "Inline fill address offset overflow");
        word(0x91000000U | (offset << 10U) | (base << 5U) | destination);
    }
    void store(unsigned reg, unsigned offset, unsigned scale, unsigned base = 8, bool vector = false) {
        require((vector && (scale == 3 || scale == 4)) ||
                    (!vector && (scale == 1 || scale == 2)), "Invalid inline store width");
        if ((offset & ((1U << scale) - 1U)) == 0) {
            require((offset >> scale) <= 4095, "Inline store exceeds scaled offset");
            const auto op = vector ? (scale == 4 ? 0x3d800000U : 0xfd000000U) :
                                     (scale == 1 ? 0x79000000U : 0xb9000000U);
            word(op | ((offset >> scale) << 10U) | (base << 5U) | reg);
        } else {
            require(offset < 256, "Inline store exceeds unscaled offset");
            const auto op = vector ? (scale == 4 ? 0x3c800000U : 0xfc000000U) :
                                     (scale == 1 ? 0x78000000U : 0xb8000000U);
            word(op | (offset << 12U) | (base << 5U) | reg);
        }
    }
    void pair(unsigned reg, unsigned offset) {
        require(offset % 16 == 0 && offset / 16 <= 63, "Inline pair store exceeds offset field");
        word(0xad000000U | ((offset / 16) << 15U) | (reg << 10U) | (8U << 5U) | reg);
    }
    void packed(unsigned reg, unsigned color, unsigned offset, unsigned base = 8) {
        scalar(reg, color); scalar(reg, color, true); store(reg, offset, 2, base);
    }
public:
    void zero(unsigned offset, unsigned size, bool first) {
        load();
        if (first) {
            require(size >= 32 && size % 8 == 0, "Initial inline zero fill is outside profile");
            word(0x6f00e401U);
            const auto pairs = size / 32;
            for (unsigned i = 0; i + 1 < pairs; ++i) pair(1, i * 32);
            // The native scheduler prepares the later zero register before the
            // last pair, sharing it with the short tail and final zero runs.
            word(0x6f00e400U); pair(1, (pairs - 1) * 32);
            if (size % 32 >= 16) store(1, pairs * 32, 4, 8, true);
            if (size % 16) store(0, size - 8, 3, 8, true);
        } else {
            require(size >= 16, "Inline zero tail is outside profile");
            add(8, 8, offset);
            if (size % 16) store(0, size - 16, 4, 8, true);
            size -= size % 16;
            while (size >= 32) { size -= 32; pair(0, size); }
            if (size) store(0, 0, 4, 8, true);
        }
    }
    void color(unsigned offset, unsigned rows, unsigned shade) {
        const auto color = shade * 1057U;
        if (rows <= 3) {
            load(); scalar(9, color);
            if (rows >= 2) { scalar(9, color, true); store(9, offset, 2); }
            if (rows & 1U) {
                if (rows >= 2) scalar(9, color);
                store(9, offset + (rows / 2) * 4, 1);
            }
        } else if (rows == 4) {
            scalar(8, color); duplicate(8); load(); store(1, offset, 3, 8, true);
        } else if (rows <= 6) {
            load(); scalar(9, color); duplicate(9); store(1, offset, 3, 8, true);
            if (rows == 5) store(9, offset + 8, 1);
            else packed(9, color, offset + 8);
        } else if (rows == 7) {
            scalar(8, color); duplicate(8); load(9); store(1, offset, 3, 9, true);
            packed(10, color, offset + 8, 9); store(8, offset + 12, 1, 9);
        } else if (rows == 8 || rows == 12) {
            load(); scalar(9, color); duplicate(9, true); store(1, offset, 4, 8, true);
            if (rows == 12) {
                if ((offset + 16) % 8 && offset + 16 >= 256) {
                    add(8, 8, offset + 16); store(1, 0, 3, 8, true);
                } else store(1, offset + 16, 3, 8, true);
            }
        } else if (rows == 15) {
            load(); add(9, 8, offset); scalar(10, color); duplicate(10, true);
            store(1, 0, 4, 9, true); store(1, 16, 3, 9, true);
            packed(9, color, offset + 24); store(10, offset + 28, 1);
        } else require(false, "Inline color fill is outside the reviewed lowering profile");
    }
    Bytes finish() { return std::move(code_); }
};
} // namespace

Bytes make_native_gray_fill(ByteView source, NativeFillLayout layout) {
    const auto bands = read_bands(source, true);
    FillWriter writer(layout);
    unsigned offset = 0;
    for (std::size_t i = 0; i < bands.size(); ++i) {
        const auto band = bands[i];
        unsigned size = band.rows * 2;
        if (!band.shade) {
            while (i + 1 < bands.size() && !bands[i + 1].shade) size += bands[++i].rows * 2;
            writer.zero(offset, size);
        } else if (band.rows == 1) {
            std::optional<unsigned> high;
            if (i + 1 < bands.size() && bands[i + 1].rows == 1 && bands[i + 1].shade) {
                high = bands[++i].shade;
                size += 2;
            }
            writer.pixels(offset, band.shade, high);
        } else writer.pattern(offset, size);
        offset += size;
    }
    require(offset == 448, "Incomplete native fill canvas");
    return writer.finish();
}

Bytes make_native_inline_gray_fill(ByteView source) {
    const auto bands = read_bands(source, false);
    require(bands.front().shade == 0, "Inline fill requires its initial zero register setup");
    InlineFillWriter writer;
    unsigned offset = 0;
    for (const auto& band : bands) {
        if (band.shade) writer.color(offset, band.rows, band.shade);
        else writer.zero(offset, band.rows * 2, offset == 0);
        offset += band.rows * 2;
    }
    return writer.finish();
}

std::array<std::uint32_t, 7> native_affine_copy_layout(ByteView source) {
    require(source.size() >= 9 && source.size() <= 114 && (source.size() - 2) % 7 == 0,
            "Invalid affine copy operand extent");
    // Original cells use two-byte records in a 128-column buffer. The native
    // image stores each decoded cell as uint32. These are representation rules,
    // distinct from the selected rectangle addresses and extent.
    constexpr unsigned origin = 0xc400, row_bytes = 256, cell_bytes = 2;
    const unsigned extent = read16(source, 0);
    require(extent > 0 && extent % (cell_bytes * 4) == 0 && extent <= row_bytes,
            "Affine copy width is outside the vector lowering profile");
    const unsigned rows = static_cast<unsigned>((source.size() - 2) / 7);
    const unsigned columns = extent / cell_bytes;
    const unsigned from = read16(source, 2), to = read16(source, 4);
    const unsigned blank = source[8];
    require(from >= origin && to >= origin && from % cell_bytes == 0 &&
                to % cell_bytes == 0 && blank < 64,
            "Invalid affine copy origin or cell identity");
    const unsigned source_row = (from - origin) / row_bytes;
    const unsigned source_column = (from - origin) % row_bytes / cell_bytes;
    const unsigned target_row = (to - origin) / row_bytes;
    const unsigned target_column = (to - origin) % row_bytes / cell_bytes;
    require(source_row + rows <= 32 && target_row + rows <= 32 &&
                source_column + columns <= 128 && target_column + columns <= 128,
            "Affine copy rectangle exceeds the image");
    require(source_row + rows <= target_row || target_row + rows <= source_row ||
                source_column + columns <= target_column || target_column + columns <= source_column,
            "Affine copy rectangles overlap");
    for (unsigned row = 0; row < rows; ++row) {
        const auto at = std::size_t{2} + std::size_t{row} * 7;
        require(read16(source, at) == from + row * row_bytes &&
                    read16(source, at + 2) == to + row * row_bytes &&
                    read16(source, at + 4) == from + row * row_bytes && source[at + 6] == blank,
                "Affine copy rows, clears or cell identities disagree");
    }
    return {rows, columns, source_row, source_column, target_row, target_column, blank};
}

Bytes make_native_affine_copy(ByteView source) {
    const auto shape = native_affine_copy_layout(source);
    const auto rows = shape[0], columns = shape[1], blank = shape[6];
    require(std::uint64_t{rows} * columns / 4 * 12 + 4 <= 4096,
            "Affine copy fragment exceeds its bounded profile");
    Bytes code;
    bool first = true;
    for (unsigned row = 0; row < rows; ++row)
        for (unsigned column = 0; column < columns; column += 4) {
            const auto from = ((shape[2] + row) * 128 + shape[3] + column) * 4;
            const auto to = ((shape[4] + row) * 128 + shape[5] + column) * 4;
            require(from % 16 == 0 && to % 16 == 0 && from / 16 <= 4095 && to / 16 <= 4095,
                    "Affine copy vector address exceeds its instruction field");
            // x8 owns the native image. The first copy uses v0, then retains
            // its empty-cell vector there and uses v1 for subsequent copies.
            const unsigned reg = first ? 0U : 1U;
            append32(code, 0x3dc00000U | ((from / 16) << 10U) | (8U << 5U) | reg);
            append32(code, 0x3d800000U | ((to / 16) << 10U) | (8U << 5U) | reg);
            if (first) {
                append32(code, 0x4f000400U | ((blank & 31U) << 5U) | ((blank >> 5U) << 16U));
                first = false;
            }
            append32(code, 0x3d800000U | ((from / 16) << 10U) | (8U << 5U));
        }
    return code;
}

std::array<std::uint32_t, 8> native_tile_run_layout(ByteView source) {
    require(source.size() % 4 == 0 && source.size() >= 20 && source.size() <= 256,
            "Invalid native tile-run record extent");
    struct Row { unsigned address, first, end; };
    std::vector<Row> rows;
    for (std::size_t at = 0; at < source.size(); at += 4)
        rows.push_back({read16(source, at), source[at + 2], source[at + 3]});
    const auto bank = rows.front().address / 1024;
    std::size_t split = 0;
    while (split < rows.size() && rows[split].address / 1024 == bank) ++split;
    require(split >= 3 && split <= 32 && rows.size() - split >= 2 && rows.size() - split <= 32,
            "Native tile-run layers are outside the reviewed profile");
    const auto second_bank = rows[split].address / 1024;
    require(second_bank != bank && rows.front().first == 0,
            "Native tile-run origins are outside the reviewed profile");
    std::size_t rising = 0;
    while (rising < split && rows[rising].first == 0) ++rising;
    require(rising >= 2 && rising < split, "Native tile runs lack blank and increasing rows");
    const auto blank = static_cast<unsigned>(rising - 1);
    const auto stride = rows[rising].first;
    require(rows[split + 1].first > rows[split].first,
            "Native second-layer tiles do not increase");
    const auto second_stride = rows[split + 1].first - rows[split].first;
    require(rows[split].end > rows[split].first, "Empty native second-layer tile run");
    const auto width = rows.front().end, second_width = rows[split].end - rows[split].first;
    require(width >= 2 && width <= 16 && second_width >= 2 && second_width <= 16 &&
                stride >= width && second_stride >= second_width,
            "Native tile-run width/stride is outside the reviewed profile");
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const bool second = i >= split;
        const auto row = static_cast<unsigned>(second ? i - split : i);
        const auto first = second ? rows[split].first + row * second_stride :
                                   row < blank ? 0 : (row - blank) * stride;
        require(rows[i].address / 1024 == (second ? second_bank : bank) &&
                    rows[i].address % 1024 == row * 32 && rows[i].first == first &&
                    rows[i].end == first + (second ? second_width : width),
                "Native tile rows are not the declared contiguous rectangles");
    }
    return {static_cast<std::uint32_t>(split), blank, width, stride,
            static_cast<std::uint32_t>(rows.size() - split), second_width,
            rows[split].first, second_stride};
}

Bytes make_native_tile_runs(ByteView source) {
    const auto shape = native_tile_run_layout(source);
    const auto rows = shape[0], blank = shape[1], columns = shape[2], stride = shape[3];
    const auto rows2 = shape[4], columns2 = shape[5], first2 = shape[6], stride2 = shape[7];
    // ARM64 encodings, register assignments and RasterCell member offsets are
    // native compiler/ABI metadata. No original row counts, tile identities,
    // native fragment bytes or original machine instructions are retained.
    Bytes code;
    auto word = [&](std::uint32_t value) { append32(code, value); };
    auto immediate = [&](std::uint32_t op, unsigned value, unsigned shift) {
        require(value <= 65535, "Native tile-run immediate overflow");
        word(op | (value << shift));
    };
    auto branch = [&](std::uint32_t op) { const auto at = code.size(); word(op); return at; };
    auto link = [&](std::size_t at, std::size_t target, bool conditional) {
        const auto delta = (static_cast<std::int64_t>(target) - static_cast<std::int64_t>(at)) / 4;
        require(at % 4 == 0 && target % 4 == 0 && at + 4 <= code.size() &&
                    target <= code.size() && delta >= -32768 && delta <= 32767,
                "Native tile-run branch outside fragment");
        const auto bits = static_cast<std::uint32_t>(delta);
        const auto value = read32(code, at) | (conditional ? (bits & 0x7ffffU) << 5 : bits & 0x3ffffffU);
        for (unsigned i = 0; i < 4; ++i) code[at + i] = static_cast<std::uint8_t>(value >> (i * 8));
    };
    word(0x72000289U); // Native caller's layer selector.
    immediate(0x52800008U, rows2, 5); immediate(0x5280000aU, rows, 5);
    word(0x9a881148U); word(0xd377d908U); // Native rows * 32 cells * 16 bytes.
    const auto to_second = branch(0x34000009U);
    word(0xd2800009U); word(0xd280000aU);
    immediate(0x1280000bU, blank * stride - 1, 5);
    const auto to_choice = branch(0x14000000U);
    const auto blank_run = code.size();
    std::size_t shared_tail = 0;
    for (unsigned column = 0; column < columns; ++column) {
        immediate(0xfd000188U, column * 2, 10);
        if (column + 1 == columns) {
            shared_tail = code.size(); word(0x8b09000cU);
        }
        immediate(0xb9000193U, column * 4 + 2, 10);
        immediate(0x39000196U, column * 16 + 12, 10);
    }
    word(0x9100054aU); word(0x91080129U);
    immediate(0x1100016bU, stride, 10); word(0xeb09011fU);
    const auto to_end = branch(0x54000000U);
    const auto choice = code.size(); word(0x8b09000cU);
    immediate(0xf100015fU, blank, 10);
    const auto to_blank = branch(0x54000003U);
    for (unsigned column = 0; column + 1 < columns; ++column) {
        if (column) immediate(0x1100016dU, column, 10);
        immediate(column ? 0xb900018dU : 0xb900018bU, column * 4, 10);
        immediate(0xf800019aU, column * 16 + 4, 12);
        immediate(0x39000196U, column * 16 + 12, 10);
    }
    immediate(0x1100016dU, columns - 1, 10);
    immediate(0x2900598dU, (columns - 1) * 4, 15);
    const auto to_tail = branch(0x14000000U);
    const auto second = code.size(); word(0xd2800009U);
    immediate(0x5280000aU, first2 + columns2 - 1, 5);
    const auto second_row = code.size(); word(0x8b09000bU);
    for (unsigned column = 0; column + 1 < columns2; ++column) {
        immediate(0x5100014cU, columns2 - column - 1, 10);
        immediate(0xb900016cU, column * 4, 10);
        immediate(0xf800017cU, column * 16 + 4, 12);
        immediate(0x39000176U, column * 16 + 12, 10);
    }
    immediate(0xb900016aU, (columns2 - 1) * 4, 10);
    immediate(0xf800017cU, (columns2 - 1) * 16 + 4, 12);
    word(0x91080129U); immediate(0x1100014aU, stride2, 10);
    immediate(0x39000176U, (columns2 - 1) * 16 + 12, 10);
    word(0xeb09011fU);
    const auto to_second_row = branch(0x54000001U);
    link(to_second, second, true); link(to_choice, choice, false);
    link(to_end, code.size(), true); link(to_blank, blank_run, true);
    link(to_tail, shared_tail, false); link(to_second_row, second_row, true);
    require(code.size() <= 4096, "Native tile-run fragment exceeds bound");
    return code;
}
std::array<std::uint32_t, 2> native_trail_layout(ByteView source) {
    require(source.size() == 5, "Invalid native trail input extent");
    const unsigned extent = read16(source, 0), phases = source[4];
    require(extent == source[2] && extent == source[3] && extent % 8 == 0,
            "Native trail record bounds disagree");
    // This register allocation holds three to five live pointers. Wider
    // layouts require another explicitly reviewed compiler lowering.
    require(extent / 8 >= 3 && extent / 8 <= 5,
            "Native trail count exceeds register profile");
    require(phases >= 3 && std::has_single_bit(phases + 1),
            "Native trail period is outside remainder profile");
    return {extent / 8, phases};
}
namespace {
class TrailWriter {
    Bytes bytes_;
public:
    unsigned size() const { return static_cast<unsigned>(bytes_.size()); }
    void word(std::uint32_t w) {
        require(bytes_.size() < 4096, "Native trail fragment exceeds bound");
        append32(bytes_, w);
    }
    void immediate(std::uint32_t op, unsigned value, unsigned shift, unsigned limit) {
        require(value <= limit, "Native trail immediate exceeds field");
        word(op | (value << shift));
    }
    void add(unsigned dst, unsigned base, unsigned value, bool wide = false) {
        immediate((wide ? 0x91000000U : 0x11000000U) | (base << 5) | dst, value, 10, 4095);
    }
    void scalar(unsigned dst, unsigned value) {
        immediate(0x52800000U | dst, value, 5, 65535);
    }
    void access(std::uint32_t op, unsigned dst, unsigned base, unsigned offset, unsigned scale = 2) {
        require(offset % (1U << scale) == 0, "Unaligned native trail member");
        immediate(op | (base << 5) | dst, offset >> scale, 10, 4095);
    }
    void load(unsigned dst, unsigned base, unsigned offset = 0) { access(0xb9400000U,dst,base,offset); }
    void store(unsigned src, unsigned base, unsigned offset = 0) { access(0xb9000000U,src,base,offset); }
    void move(unsigned dst, unsigned src) { word(0xaa0003e0U | (src << 16) | dst); }
    void multiply_add(unsigned dst, unsigned lhs, unsigned rhs, unsigned addend) {
        word(0x9ba00000U | (rhs << 16) | (addend << 10) | (lhs << 5) | dst);
    }
    unsigned branch(std::uint32_t op) { const auto at = size(); word(op); return at; }
    void link(unsigned at, unsigned target, unsigned external_bytes = 0) {
        require(at < size() && at % 4 == 0 && target % 4 == 0 &&
                    target <= size() + external_bytes && external_bytes <= 4096,
                "Native trail branch outside reviewed fragment");
        const auto delta = (static_cast<std::int64_t>(target) - at) / 4;
        require(delta >= -(1 << 18) && delta < (1 << 18), "Native trail branch exceeds field");
        const auto value = read32(bytes_,at) | ((static_cast<std::uint32_t>(delta) & 0x7ffffU) << 5);
        for (unsigned i = 0; i < 4; ++i) bytes_[at+i] = static_cast<std::uint8_t>(value >> (8*i));
    }
    Bytes finish() { return std::move(bytes_); }
};
}
Bytes make_native_trail_update(ByteView source, std::uint32_t spawn_bytes) {
    const auto [count, phases] = native_trail_layout(source);
    require(spawn_bytes > 0 && spawn_bytes <= 4096 && spawn_bytes % 4 == 0,
            "Invalid native trail continuation extent");
    // For d=2^k-1, quotient uses a corrected reciprocal and the remainder
    // subtracts (q<<k)-q. Verify rounding at the two worst uint32 endpoints.
    const auto k = static_cast<unsigned>(std::bit_width(phases));
    const auto scale = std::uint64_t{1} << (32U + k);
    const auto multiplier = (scale + phases - 1U) / phases - (std::uint64_t{1} << 32U);
    require(multiplier <= UINT32_MAX, "Native trail reciprocal exceeds word");
    const auto quotient = [&](std::uint32_t n) {
        const auto high = (std::uint64_t{n} * multiplier) >> 32U;
        return (high + ((std::uint64_t{n} - high) >> 1U)) >> (k - 1U);
    };
    const auto last = UINT32_MAX / phases;
    require(quotient(last * phases - 1U) == last - 1U && quotient(UINT32_MAX) == last,
            "Native trail reciprocal is inexact");
    // Native layout: twenty bytes per semantic Trail, two update banks and a
    // published bank. The following fields and asset prefix are native ABI.
    constexpr unsigned trail_base = 0xd90, stride = 20;
    const unsigned published = trail_base + count * stride * 2;
    const unsigned first = trail_base + count * stride * 3 + 48;
    TrailWriter w;
    w.add(13,0,trail_base,true); w.add(12,0,published,true);
    w.load(10,0,first); w.scalar(15,stride); w.multiply_add(11,10,15,13);
    std::vector<unsigned> pointers;
    for (unsigned i = 0; i < count; ++i) {
        const bool final = i + 1 == count;
        const unsigned base = i == 0 ? 11 : i == 1 ? 12 : final ? 13 : i == 2 ? 15 : 16;
        pointers.push_back(base);
        if (i == 1) { w.add(12,10,i); w.multiply_add(12,12,15,13); }
        else if (final) { w.add(17,10,i); w.scalar(1,stride); w.multiply_add(13,17,1,13); }
        else if (i == 2) { w.add(15,10,i); w.scalar(16,stride); w.multiply_add(15,15,16,13); }
        else if (i) { w.add(17,10,i); w.multiply_add(16,17,16,13); }
        const unsigned phase = i == 0 ? 14 : i == 1 ? 15 : 17;
        const unsigned temp = i < 2 ? 16 : 1, scratch = i < 2 ? 17 : 2;
        w.load(phase,base);
        const auto inactive = w.branch(0x34000000U | phase);
        w.load(temp,base,8);
        w.word(0x71000400U | (temp << 5) | temp);
        w.store(temp,base,8);
        const auto waiting = w.branch(0x54000001U);
        w.add(phase,phase,1);
        w.scalar(temp,static_cast<unsigned>(multiplier & 65535U));
        w.immediate(0x72a00000U | temp,static_cast<unsigned>(multiplier >> 16U),5,65535);
        w.word(0x9ba07c00U | (temp << 16) | (phase << 5) | temp);
        w.word(0xd360fc00U | (temp << 5) | temp);
        w.word(0x4b000000U | (temp << 16) | (phase << 5) | scratch);
        w.word(0x0b400400U | (scratch << 16) | (temp << 5) | temp);
        w.word(0x53007c00U | ((k-1) << 16) | (temp << 5) | temp);
        w.word(0x4b000000U | (temp << 16) | (k << 10) | (temp << 5) | temp);
        w.word(0x0b000000U | (temp << 16) | (phase << 5) | phase);
        w.store(phase,base); w.load(phase,base,4); w.store(phase,base,8);
        w.link(inactive,w.size()); w.link(waiting,w.size());
        if (!i) w.load(14,0,first+4);
        const unsigned px = i == 0 ? 16 : i == 1 ? 15 : 17;
        const unsigned py = i == 0 ? 17 : i == 1 ? 16 : 1;
        const unsigned result = final ? 14 : px;
        w.word(0x29400000U | (3U << 15) | (py << 10) | (base << 5) | px);
        w.word(0x0b0001c0U | (px << 16) | result);
        w.word(0x12003c00U | (result << 5) | result);
        w.store(result,base,12);
        const unsigned dest = published + i * stride;
        const unsigned address = i == 1 ? 15 : final ? 14 : 17;
        if (i && dest % 16) w.add(address,0,dest,true);
        w.access(0x3dc00000U,0,base,0,4);
        if (!i) {
            w.access(0x3d800000U,0,12,0,4); w.store(py,12,16);
        } else {
            w.store(py,0,dest+16);
            w.access(0x3d800000U,0,dest % 16 ? address : 0,dest % 16 ? 0 : dest,4);
        }
    }
    w.access(0x39400000U,14,0,first+12,0); w.add(17,14,1);
    w.access(0x39000000U,17,0,first+12,0);
    std::vector<unsigned> branches;
    for (unsigned i = 0; i < count; ++i) {
        const unsigned phase = i < 2 ? 17 : 12;
        w.load(phase,pointers[i]);
        if (i) w.move(11,pointers[i]);
        branches.push_back(w.branch((i+1 == count ? 0x35000000U : 0x34000000U) | phase));
    }
    for (unsigned i = 0; i < branches.size(); ++i)
        w.link(branches[i],w.size() + (i+1 == count ? spawn_bytes : 0),spawn_bytes);
    return w.finish();
}
Bytes make_native_trail_draw(ByteView source, std::uint32_t code, std::uint32_t draw_helper) {
    const auto count = native_trail_layout(source)[0];
    require(code % 4 == 0 && draw_helper % 4 == 0, "Unaligned native trail draw linkage");
    TrailWriter w;
    const unsigned published = 0xd90 + count * 20 * 2;
    for (unsigned i = 0; i < count; ++i) {
        const unsigned record = published + i * 20;
        w.load(8,20,record);
        w.word(0x38686aa2U); // Native byte frame lookup through x21 + x8.
        w.load(3,20,record+12); w.load(4,20,record+16);
        w.move(0,20); w.move(1,19);
        const auto pc = std::uint64_t{code} + w.size();
        require(pc <= UINT32_MAX, "Native trail draw address overflow");
        const auto delta = std::int64_t{draw_helper} - static_cast<std::int64_t>(pc);
        require(delta % 4 == 0 && delta >= -(1<<27) && delta < (1<<27),
                "Native trail draw helper out of range");
        w.word(0x94000000U | (static_cast<std::uint32_t>(delta / 4) & 0x3ffffffU));
    }
    return w.finish();
}
Bytes make_native_cell_apertures(ByteView origins, unsigned columns, unsigned rows,
                                unsigned width, unsigned height, unsigned blank_tile) {
    require(columns > 0 && columns <= 32 && rows > 1 && rows <= 8 &&
                width > 0 && width <= columns && height > 0 && height <= rows &&
                blank_tile <= 65535 && !origins.empty() && origins.size() % 4 == 0 &&
                origins.size() <= 128,
            "Invalid native aperture canvas/extent");
    std::vector<bool> aperture(columns * rows, false);
    for (std::size_t at = 0; at < origins.size(); at += 4) {
        const auto origin = read32(origins, at);
        require(origin < columns * rows && width <= columns - origin % columns &&
                    height <= rows - origin / columns,
                "Native aperture leaves its canvas");
        for (unsigned y = 0; y < height; ++y)
            for (unsigned x = 0; x < width; ++x) {
                const auto cell = origin + y * columns + x;
                require(!aperture[cell], "Overlapping native apertures");
                aperture[cell] = true;
            }
    }
    // This compiler profile keeps the blank tile in w8 and the vector data in
    // x9. The spill reloads and final scheduled x0 reload are native ABI details.
    // Only the stores selected by the supplied aperture data are emitted; no
    // original layout or previously compiled instruction sequence is retained.
    Bytes code;
    append32(code, 0x52800008U | (blank_tile << 5U));
    for (unsigned y = 0; y < rows; ++y) {
        if (y) append32(code, 0xf85283a9U); // ldur x9,[x29,#-216]
        if (y + 1 == rows) append32(code, 0xf85703a0U); // ldur x0,[x29,#-144]
        for (unsigned x = 0; x < columns; ++x) {
            const auto cell = y * columns + x;
            if (!aperture[cell])
                append32(code, 0xb9000000U | ((cell * 4U) << 10U) | (9U << 5U) | 8U);
        }
    }
    return code;
}
std::array<std::uint32_t, 2> native_banked_word_copy_layout(
    ByteView operands, std::span<const std::uint32_t> format) {
    require(operands.size() == 4 && format.size() == 3,
            "Invalid banked word-copy input or format");
    const auto origin = format[0], span = format[1], bank = format[2];
    require(origin < 65536 && origin % 2 == 0 && span > 0 && span <= 65536-origin &&
                bank >= 2 && bank <= 128 && std::has_single_bit(bank) && span % bank == 0,
            "Invalid banked word-copy address format");
    const auto address = unsigned{read16(operands,0)}, bytes = unsigned{read16(operands,2)};
    require(address >= origin && address-origin < span && (address-origin) % 2 == 0 &&
                bytes > 0 && bytes % 2 == 0 && bytes <= bank-(address-origin)%bank,
            "Word copy escapes its selected bank or has an invalid extent");
    return {(address-origin)%bank/2U, bytes/2U};
}

Bytes make_native_banked_word_copy(ByteView operands, std::span<const std::uint32_t> format) {
    require(format.size() == 7, "Invalid native word-copy format");
    const auto layout = native_banked_word_copy_layout(operands,format.first(3));
    const auto source = format[3], destination = format[4], scratch = format[5], stack = format[6];
    require(source < 31 && destination < 31 && scratch < 31 && source != destination &&
                source != scratch && destination != scratch && stack <= 4095,
            "Invalid native word-copy registers or stack location");
    Bytes code;
    const auto count = layout[1]*2U;
    for (unsigned at = 0; at < count;) {
        const unsigned width = count-at >= 8 ? 8U : count-at >= 4 ? 4U : 2U;
        const auto load = width == 8 ? 0xf9400000U : width == 4 ? 0xb9400000U : 0x79400000U;
        const auto store = width == 8 ? 0xf8000000U : width == 4 ? 0xb8000000U : 0x78000000U;
        append32(code, load | (at/width << 10U) | (source << 5U) | scratch);
        if (at == 0)
            append32(code, 0x910003e0U | (stack << 10U) | destination); // add destination,sp,#offset
        // The unscaled store preserves an arbitrary even within-bank offset.
        // Every load/store stays wholly inside the requested extent; no padding
        // or overlapping tail can read unrelated source colors.
        append32(code, store | ((layout[0]*2U+at) << 12U) | (destination << 5U) | scratch);
        at += width;
    }
    return code;
}

Bytes make_native_vector_fill(ByteView extent, unsigned unit_shift,
                              unsigned pointer_register, unsigned vector_register, bool reverse) {
    require(extent.size() == 2 && unit_shift <= 4 && pointer_register < 31 && vector_register < 32,
            "Invalid vector-fill extent, units or registers");
    const auto selected = unsigned{read16(extent,0)};
    const auto unit = 1U << unit_shift;
    require(selected % unit == 0,"Inexact vector-fill transfer conversion");
    const auto count = selected / unit;
    require(count > 0 && count <= 256 && count % 16 == 0,"Vector fill requires complete vectors");
    Bytes code;
    const auto pair = [&](unsigned at) {
        append32(code,0xad000000U | ((at/16U) << 15U) |
                     (vector_register << 10U) | (pointer_register << 5U) | vector_register);
    };
    const auto single = [&](unsigned at) {
        append32(code,0x3d800000U | ((at/16U) << 10U) |
                     (pointer_register << 5U) | vector_register);
    };
    if (reverse) {
        unsigned remaining = count;
        while (remaining >= 32) { remaining -= 32; pair(remaining); }
        if (remaining) single(0);
    } else {
        unsigned at = 0;
        while (count-at >= 32) { pair(at); at += 32; }
        if (at != count) single(at);
    }
    return code;
}

Bytes make_native_vector_rows(ByteView extent, std::span<const std::uint32_t> format) {
    require(format.size() == 8, "Invalid vector-row format extent");
    const auto unit = format[0], base = format[1], work = format[2], vector = format[3];
    const auto origin = format[4], stride = format[5], rows = format[6], forward = format[7];
    require(unit <= 4 && base < 31 && work < 31 && base != work && vector < 32 &&
                origin <= 65535 && stride > 0 && stride <= 4096 && rows > 0 && rows <= 32 &&
                (rows == 32 || (forward >> rows) == 0) &&
                std::uint64_t{origin} + std::uint64_t{rows-1U}*stride <= 65535,
            "Invalid vector-row layout or registers");
    require(extent.size() == 2 && (unsigned{read16(extent,0)} >> unit) <= stride,
            "Vector rows overlap");
    Bytes code;
    for (unsigned row = 0; row < rows; ++row) {
        const auto offset = origin + row*stride;
        auto pointer = work;
        if (offset <= 4095) {
            append32(code,0x91000000U | (offset << 10U) | (base << 5U) | work);
        } else {
            // Only the final row may reuse its dead base register. Larger
            // offsets use the compiler's ordinary MOV/ADD address formation.
            require(row+1U == rows,"Vector row requires an unreviewed address strategy");
            append32(code,0x52800000U | (offset << 5U) | work);
            append32(code,0x8b000000U | (work << 16U) | (base << 5U) | base);
            pointer = base;
        }
        if (row == 0) append32(code,0x6f00e400U | vector);
        const auto stores = make_native_vector_fill(extent,unit,pointer,vector,
                                                   ((forward >> row) & 1U) == 0);
        code.insert(code.end(),stores.begin(),stores.end());
    }
    return code;
}

Bytes make_native_short_fill(ByteView extent, unsigned pointer_register, unsigned value_register) {
    require(extent.size() == 2 && pointer_register < 31 && value_register < 31 &&
                pointer_register != value_register, "Invalid short-fill input or registers");
    const auto count = unsigned{read16(extent, 0)};
    require(count > 8 && count < 16, "Short fill exceeds the two-store profile");
    const auto registers = (pointer_register << 5U) | value_register;
    Bytes code;
    if (count % 8 == 4) {
        // One full register and an exact half-register tail need no overlap.
        append32(code, 0xf9000000U | registers);
        append32(code, 0xb9000000U | ((count - 4U) / 4U << 10U) | registers);
    } else {
        // A tail-first unaligned store overlaps the head while remaining wholly
        // within the requested extent. The caller supplies repeated-byte data.
        append32(code, 0xf8000000U | ((count - 8U) << 12U) | registers);
        append32(code, 0xf9000000U | registers);
    }
    return code;
}
std::array<std::uint32_t, 2> native_condition_choices(
    ByteView operands, unsigned aliases, unsigned shift, unsigned width) {
    require(aliases > 0 && aliases <= 256 && operands.size() == aliases * 2U &&
                width >= 1 && width <= 3 && shift <= 8 - width,
            "Invalid condition choice input format");
    const auto mask = (1U << width) - 1U;
    std::array<std::uint32_t, 2> values{};
    for (unsigned side = 0; side < 2; ++side) {
        values[side] = (unsigned{operands[side * aliases]} >> shift) & mask;
        for (unsigned i = 1; i < aliases; ++i)
            require(((unsigned{operands[side * aliases + i]} >> shift) & mask) == values[side],
                    "Condition choice aliases disagree");
    }
    return values;
}

Bytes make_native_condition_choice(ByteView operands, std::span<const std::uint32_t> format) {
    require(format.size() == 5 || format.size() == 7 || format.size() == 8 || format.size() == 9,
            "Invalid condition choice native format");
    const auto choices = native_condition_choices(operands, format[0], format[1], format[2]);
    Bytes code;
    if (format.size() == 7) {
        // A preceding caller comparison establishes a known value in a live
        // register on the true branch. Reuse it only when the extracted choice
        // agrees; a changed choice requires another lowering, never a guess.
        require(format[3] < 31 && format[4] < 14 && format[5] < 31 &&
                    format[6] < 8 && choices[0] == format[6] && choices[1] == 0,
                "Condition choice disagrees with caller-known value");
        append32(code, 0x1a9f0000U | (format[4] << 12U) |
                           (format[5] << 5U) | format[3]);
        return code;
    }
    if (format.size() == 9) {
        // Preserve the caller operation immediately preceding the selection.
        // These are native loop/member ABI bindings, not original code words.
        // Including it distinguishes a reviewed predicate from identical unit
        // predicates in unrelated code without weakening duplicate detection.
        const auto kind = format[5], source = format[6];
        require(kind <= 1 && source < 31, "Invalid condition prelude ABI");
        if (kind == 0) {
            require(format[7] <= 4095 && format[8] == 0,
                    "Invalid condition comparison ABI");
            append32(code, 0x7100001fU | (format[7] << 10U) | (source << 5U));
        } else {
            require(format[7] < 31 && format[8] <= 16380 && format[8] % 4 == 0,
                    "Invalid condition publication ABI");
            append32(code, 0xb9000000U | ((format[8] / 4U) << 10U) |
                               (format[7] << 5U) | source);
        }
    }
    append_condition_choice(code, choices[0], choices[1], format[3], format[4]);
    if (format.size() == 8) {
        const auto retained = format[5], result = format[6], condition = format[7];
        require(retained < 31 && result < 31 && retained != format[3] && condition < 14,
                "Invalid retained condition value ABI");
        append32(code, 0x1a800000U | (format[3] << 16U) | (condition << 12U) |
                           (retained << 5U) | result);
    }
    return code;
}

std::array<std::uint32_t, 3> native_condition_cell_values(
    ByteView operands, std::span<const std::uint32_t> format) {
    require(format.size() == 5 && format[0] > 0 && format[0] <= 256 &&
                operands.size() == 2U * format[0] + 2 && format[3] <= 65535 &&
                format[4] > 0 && format[4] <= 64,
            "Invalid conditional cell source format");
    const auto colors = native_condition_choices(operands.first(2U * format[0]),
                                                 format[0], format[1], format[2]);
    const auto address = read16(operands, 2U * format[0]);
    require(address >= format[3], "Conditional cell precedes canvas");
    const auto offset = (address - format[3]) * format[4];
    require(offset <= 16380 && offset % 4 == 0, "Conditional cell exceeds native word extent");
    return {colors[0], colors[1], offset};
}

Bytes make_native_cell_condition(ByteView operands, std::span<const std::uint32_t> format) {
    require(format.size() == 10 && format[5] < 31 && format[6] < 14 &&
                format[7] < 31 && format[8] < 31 && format[9] <= 16380 && format[9] % 4 == 0,
            "Invalid conditional cell native format");
    const auto values = native_condition_cell_values(operands, format.first(5));
    const auto offset = values[2] + format[9];
    require(offset <= 16380, "Conditional cell publication exceeds native extent");
    Bytes code;
    append32(code, 0xb9000000U | ((offset / 4U) << 10U) | (format[8] << 5U) | format[7]);
    append_condition_choice(code, values[0], values[1], format[5], format[6]);
    return code;
}

Bytes make_native_dimension_choice(ByteView operands, std::span<const std::uint32_t> format) {
    require((format.size() == 5 || format.size() == 7) && format[1] <= 2 && format[2] < 31 &&
                format[3] < 31 && format[2] != format[3] && format[4] < 14,
            "Invalid dimension choice native format");
    const auto values = native_byte_choice_layout(operands, format[0]);
    require(std::has_single_bit(values[0]) && std::has_single_bit(values[1]),
            "Dimension choice requires positive byte powers of two");
    const auto scratch = format[2], result = format[3], condition = format[4];
    Bytes code;
    if (format.size() == 7) {
        require(format[5] < 31 && format[6] <= 4095,
                "Invalid dimension caller comparison");
        // The optional caller comparison owns NZCV. Its semantic selector and
        // native register identify the caller; dimension arithmetic retains it.
        append32(code, 0x7100001fU | (format[6] << 10U) | (format[5] << 5U));
    }
    if (format[1] == 2) {
        append_condition_choice(code, static_cast<unsigned>(std::countr_zero(values[0])),
                                static_cast<unsigned>(std::countr_zero(values[1])),
                                result, condition);
        return code;
    }
    const auto project = [&](std::uint32_t value) -> std::int64_t {
        return format[1] == 1 ? -std::int64_t{value} : std::int64_t{value};
    };
    const auto first = project(values[0]), second = project(values[1]);
    const auto immediate = [&](unsigned reg, std::int64_t value) {
        if (value < 0)
            append32(code, 0x12800000U | (static_cast<std::uint32_t>(-value - 1) << 5U) | reg);
        else
            append32(code, 0x52800000U | (static_cast<std::uint32_t>(value) << 5U) | reg);
    };
    if (first == second) {
        immediate(result, first);
    } else if (std::max(first, second) - std::min(first, second) == 1) {
        // Adjacent choices share a base; conditional increment preserves NZCV.
        immediate(scratch, std::min(first, second));
        const auto increment = first > second ? condition : condition ^ 1U;
        append32(code, 0x1a800400U | (scratch << 16U) | ((increment ^ 1U) << 12U) |
                           (scratch << 5U) | result);
    } else {
        immediate(result, first);
        immediate(scratch, second);
        append32(code, 0x1a800000U | (scratch << 16U) | (condition << 12U) |
                           (result << 5U) | result);
    }
    return code;
}

namespace {
// Lower a small lookup of a normalized Boolean. Values are extracted data;
// registers and arithmetic encodings describe the native compiler contract.
void append_boolean_lookup(Bytes& code, unsigned yes, unsigned no,
                           unsigned input, unsigned result) {
    require(yes < 8 && no < 8 && input < 31 && result < 31,
            "Invalid Boolean lookup values or registers");
    if (yes == no) {
        append32(code, 0x52800000U | (yes << 5U) | result);
        return;
    }
    const auto base = std::min(yes, no), scale = std::max(yes, no) - base;
    if (yes < no) append32(code, 0x52000000U | (input << 5U) | result);
    else append32(code, 0xaa0003e0U | (input << 16U) | result);
    const auto low = static_cast<unsigned>(std::countr_zero(scale)), odd = scale >> low;
    const auto merge = [&](unsigned distance) {
        append32(code, 0x2a000000U | (result << 16U) | (distance << 10U) |
                           (result << 5U) | result);
    };
    if (odd > 1) {
        const auto high = static_cast<unsigned>(std::bit_width(odd)) - 1U;
        if (std::popcount(odd) == 2) merge(high);
        else for (unsigned i = 0; i < high; ++i) merge(1);
    }
    if (low) append32(code, 0x53000000U | ((32U-low) << 16U) |
                               ((31U-low) << 10U) | (result << 5U) | result);
    if (base) append32(code, 0x11000000U | (base << 10U) | (result << 5U) | result);
}

std::uint32_t native_branch_delta(std::uint32_t from, std::uint32_t to, unsigned bits) {
    const auto delta = std::int64_t{to} - from;
    const auto limit = std::int64_t{1} << (bits-1);
    require(from % 4 == 0 && to % 4 == 0 && delta/4 >= -limit && delta/4 < limit,
            "Native lookup branch outside range or alignment");
    return static_cast<std::uint32_t>(delta/4) & ((1U << bits)-1U);
}
}

Bytes make_native_boolean_value(ByteView choices, std::span<const std::uint32_t> format) {
    require(choices.size() == 2 && format.size() == 2,
            "Invalid native Boolean value ABI");
    Bytes code;
    append_boolean_lookup(code, choices[0], choices[1], format[0], format[1]);
    return code;
}

Bytes make_native_boolean_result(ByteView choices, std::span<const std::uint32_t> format) {
    require(choices.size() == 2 && format.size() == 4 && format[0] < 31 &&
                format[1] < 31 && format[2] <= UINT32_MAX-24U,
            "Invalid native Boolean result ABI");
    Bytes code;
    append32(code, 0x94000000U | native_branch_delta(format[2], format[3], 26));
    append_boolean_lookup(code, choices[0], choices[1], format[0], format[1]);
    require(!(format[3] >= format[2] && format[3] < format[2]+code.size()),
            "Boolean result helper overlaps generated fragment");
    return code;
}

Bytes make_native_optional_byte_search(ByteView choices,
                                      std::span<const std::uint32_t> format) {
    require(choices.size() == 2 && choices[0] < 8 && choices[1] < 8 &&
                format.size() == 12, "Invalid native optional search input");
    for (unsigned i = 0; i < 7; ++i) {
        require(format[i] < 31, "Invalid optional search register");
        for (unsigned j = 0; j < i; ++j)
            require(format[i] != format[j], "Aliased optional search registers");
    }
    const auto source = format[0], pointer = format[1], remaining = format[2];
    const auto key = format[3], present = format[4], needle = format[5], result = format[6];
    const auto stride = format[7], count = format[8], biased_key = format[9];
    const auto origin = format[10], finish = format[11];
    require(stride > 0 && stride <= 255 && count > 0 && count <= 256 && biased_key <= 511 &&
                origin <= UINT32_MAX-68U && origin % 4 == 0 && finish % 4 == 0,
            "Invalid optional search layout/linkage");
    Bytes mapping;
    // Identity mapping needs no post-search operation: the live predicate is
    // already the selected value. This compiler fold is derived from both
    // supplied choices rather than retained as an implicit color assignment.
    if (choices[0] != 0 || choices[1] != 1)
        append_boolean_lookup(mapping, choices[1], choices[0], result, result);
    const auto size = 48U + static_cast<unsigned>(mapping.size());
    require(!(finish >= origin && finish < origin+size), "Optional search exit overlaps fragment");
    const auto match = mapping.empty() ? finish : origin+44U;
    Bytes code;
    append32(code, 0xaa0003e0U | (source << 16U) | pointer);
    append32(code, 0x52800000U | (((count-1)*stride) << 5U) | key);
    append32(code, 0xaa0003e0U | (key << 16U) | remaining);
    const auto displacement = (biased_key + 256U) & 511U;
    append32(code, 0x38400000U | (displacement << 12U) | (pointer << 5U) | key);
    append32(code, 0x12001c00U | (key << 5U) | key);
    append32(code, 0x38400400U | (stride << 12U) | (pointer << 5U) | present);
    append32(code, 0x6b20001fU | (needle << 16U) | (key << 5U));
    append32(code, 0x1a8013e0U | (present << 16U) | result);
    append32(code, 0x37000000U | (native_branch_delta(origin+32U, match, 14) << 5U) | result);
    append32(code, 0xd1000000U | (stride << 10U) | (remaining << 5U) | key);
    append32(code, 0xb5000000U | (native_branch_delta(origin+40U, origin+8U, 19) << 5U) | remaining);
    code.insert(code.end(), mapping.begin(), mapping.end());
    append32(code, 0x14000000U | native_branch_delta(origin+size-4U, finish, 26));
    return code;
}

std::array<std::uint32_t, 3> native_caption_choice_layout(
    ByteView operands, unsigned aliases, unsigned shift, unsigned width) {
    require(aliases > 0 && aliases <= 256 && operands.size() == aliases * 2U + 2U &&
                width >= 1 && width <= 3 && shift <= 8 - width,
            "Invalid caption choice input format");
    const auto mask = (1U << width) - 1U;
    const auto first = (unsigned{operands[0]} >> shift) & mask;
    const auto second = (unsigned{operands[aliases]} >> shift) & mask;
    for (unsigned i = 0; i < aliases; ++i)
        require(((unsigned{operands[i]} >> shift) & mask) == first &&
                    ((unsigned{operands[aliases + i]} >> shift) & mask) == second,
                "Caption choice aliases disagree");
    const auto encoded = unsigned{read16(operands, aliases * 2U)};
    const auto scroll = encoded < 32768U ? static_cast<std::int32_t>(encoded)
                                       : static_cast<std::int32_t>(encoded) - 65536;
    return {first, second, static_cast<std::uint32_t>(-scroll)};
}

Bytes make_native_caption_choice(ByteView operands, unsigned aliases,
    unsigned shift, unsigned width, unsigned input_register, bool inverted,
    unsigned key_length, std::uint32_t code_address, std::uint32_t helper_address) {
    const auto layout = native_caption_choice_layout(operands, aliases, shift, width);
    require(input_register < 31 && key_length <= 65535 &&
                code_address % 4 == 0 && helper_address % 4 == 0,
            "Invalid caption choice native ABI");
    const auto when_true = layout[inverted ? 1U : 0U];
    const auto when_false = layout[inverted ? 0U : 1U];
    Bytes code;
    const auto word = [&](std::uint32_t value) { append32(code, value); };
    const auto immediate = [&](unsigned reg, std::uint32_t value) {
        if (value <= 65535U)
            word(0x52800000U | (value << 5U) | reg);
        else {
            require(~value <= 65535U, "Caption displacement exceeds native immediate");
            word(0x12800000U | ((~value) << 5U) | reg);
        }
    };
    if (when_true == when_false) {
        immediate(3, when_true);
    } else {
        // Normalize towards the larger choice, expand its difference without
        // scratch registers or flags, then add the smaller value. Neither
        // palette nor its unit-scale compiler special case is stored here.
        const bool reverse = when_true < when_false;
        const auto base = std::min(when_true, when_false);
        const auto scale = std::max(when_true, when_false) - base;
        if (reverse)
            word(0x52000003U | (input_register << 5U)); // eor w3,input,#1
        else
            word(0xaa0003e3U | (input_register << 16U)); // mov x3,input
        const auto low = static_cast<unsigned>(std::countr_zero(scale));
        const auto odd = scale >> low;
        const auto merge = [&](unsigned distance) {
            word(0x2a030063U | (distance << 10U)); // orr w3,w3,w3,lsl #distance
        };
        if (odd > 1) {
            const auto high = static_cast<unsigned>(std::bit_width(odd)) - 1U;
            if (std::popcount(odd) == 2) merge(high);
            else for (unsigned i = 0; i < high; ++i) merge(1);
        }
        if (low)
            word(0x53000063U | ((32U - low) << 16U) | ((31U - low) << 10U));
        if (base) word(0x11000063U | (base << 10U)); // add w3,w3,#base
    }
    if (key_length) immediate(2, key_length);
    immediate(4, 0); // No horizontal displacement in this native caption call.
    immediate(5, layout[2]);
    const auto call = std::uint64_t{code_address} + code.size();
    require(call + 4 <= std::uint64_t{UINT32_MAX} + 1 &&
                !(helper_address >= code_address && helper_address < call + 4),
            "Caption call overlaps or exceeds its fragment");
    const auto delta = std::int64_t{helper_address} - static_cast<std::int64_t>(call);
    require(delta >= -(std::int64_t{1} << 27) && delta < (std::int64_t{1} << 27),
            "Caption helper exceeds branch range");
    word(0x94000000U | (static_cast<std::uint32_t>(delta / 4) & 0x3ffffffU));
    return code;
}

Bytes make_native_bit_number_prefix(ByteView operands, std::span<const std::uint32_t> format) {
    require(format.size() == 10, "Invalid bit-number prefix format");
    // Share the authenticated caption input record, but consume only its two
    // palette projections. The scroll projection is not owned by this fragment.
    const auto choices = native_caption_choice_layout(operands, format[0], format[1], format[2]);
    const auto source = format[3], bit = format[4], inverted = format[5], unit = format[6];
    const auto stack = format[7], ordinal = format[8], digits = format[9];
    require(source >= 6 && source < 31 && bit < 32 && inverted <= 1 &&
                ((unit == 31) || (unit >= 6 && unit < 31 && unit != source && bit == 0 && inverted)) &&
                stack <= 4096 && stack % 8 == 0 && ordinal <= 65535 && digits > 0 && digits <= 10,
            "Invalid bit-number prefix ABI");
    const auto when_true = choices[inverted ? 1U : 0U];
    const auto when_false = choices[inverted ? 0U : 1U];
    Bytes code;
    const auto word = [&](std::uint32_t value) { append32(code, value); };
    if (when_true == when_false) {
        word(0x52800005U | (when_true << 5U));
    } else {
        const bool reverse = when_true < when_false;
        if (reverse && unit < 31) {
            // The native caller already owns a unit Boolean mask. Its value
            // represents predicate width, independently of either palette.
            word(0x0a200005U | (source << 16U) | (unit << 5U));
        } else {
            // The input is a declared caller-clobbered scratch register. The
            // reviewed inverse-bit lowering complements it before narrowing.
            if (reverse) word(0x2a2003e0U | (source << 16U) | source);
            if (bit == 0)
                word(0x12000005U | (source << 5U));
            else
                word(0x53000005U | (bit << 16U) | (bit << 10U) | (source << 5U));
        }
        append_choice_scale(code, std::min(when_true, when_false),
                            std::max(when_true, when_false) - std::min(when_true, when_false), 5);
    }
    // A stack value of one page means x0 was supplied by the preceding native
    // instruction. It is an ABI sentinel, not a layout or original game value.
    if (stack < 4096) word(0x910003e0U | (stack << 10U));
    word(0x52800001U | (ordinal << 5U));
    word(0x52800002U | (digits << 5U));
    return code;
}

Bytes make_native_flag_caption(ByteView operands, std::span<const std::uint32_t> format) {
    require(format.size() == 9, "Invalid flag caption native format");
    const auto layout = native_caption_choice_layout(operands, format[0], format[1], format[2]);
    const auto condition = format[3], stack = format[4], key = format[5], length = format[6];
    const auto code_address = format[7], helper_address = format[8];
    require(condition < 14 && stack <= 4095 && stack % 8 == 0 &&
                length > 0 && length <= 65535 &&
                std::uint64_t{key} + length <= std::uint64_t{UINT32_MAX} + 1 &&
                code_address % 4 == 0 && helper_address % 4 == 0,
            "Invalid flag caption ABI/linkage");
    Bytes code;
    append_condition_choice(code, layout[0], layout[1], 3, condition);
    const auto word = [&](std::uint32_t value) { append32(code, value); };
    const auto page_instruction = std::uint64_t{code_address} + code.size();
    const auto pages = (std::int64_t{key & ~4095U} -
                       static_cast<std::int64_t>(page_instruction & ~std::uint64_t{4095})) / 4096;
    require(pages >= -(std::int64_t{1} << 20) && pages < (std::int64_t{1} << 20),
            "Caption resource key exceeds page range");
    const auto encoded = static_cast<std::uint32_t>(pages) & 0x1fffffU;
    word(0x90000001U | ((encoded & 3U) << 29U) | ((encoded >> 2U) << 5U));
    word(0x91000021U | ((key & 4095U) << 10U));
    word(0x910003e0U | (stack << 10U));
    word(0x52800002U | (length << 5U));
    word(0x52800004U); // This native caption has no horizontal displacement.
    if (layout[2] <= 65535)
        word(0x52800005U | (layout[2] << 5U));
    else {
        require(~layout[2] <= 65535, "Flag caption displacement exceeds native immediate");
        word(0x12800005U | ((~layout[2]) << 5U));
    }
    const auto call = std::uint64_t{code_address} + code.size();
    require(call + 4 <= std::uint64_t{UINT32_MAX} + 1 &&
                !(helper_address >= code_address && helper_address < call + 4) &&
                (std::uint64_t{key} + length <= code_address || key >= call + 4),
            "Flag caption call/key overlaps or exceeds its fragment");
    const auto delta = std::int64_t{helper_address} - static_cast<std::int64_t>(call);
    require(delta >= -(std::int64_t{1} << 27) && delta < (std::int64_t{1} << 27),
            "Flag caption helper exceeds branch range");
    word(0x94000000U | (static_cast<std::uint32_t>(delta / 4) & 0x3ffffffU));
    return code;
}

std::vector<std::uint32_t> native_number_choice_layout(
    ByteView operands, std::span<const std::uint32_t> format) {
    require(format.size() == 9, "Invalid numbered choice layout format");
    const auto aliases = format[0], base = format[3], bytes = format[4];
    const auto columns = format[5], rows = format[6], pixels = format[7], baseline = format[8];
    require(aliases > 0 && aliases <= 256 && operands.size() >= aliases * 2U + 4U &&
                (operands.size() - aliases * 2U - 2U) % 2 == 0 &&
                (operands.size() - aliases * 2U - 2U) / 2 <= 256 &&
                base <= 65535 && bytes > 0 && bytes <= 4 && columns > 0 && columns <= 256 &&
                rows > 0 && rows <= 256 && pixels > 0 && pixels <= 256 && baseline < pixels,
            "Invalid numbered choice input or canvas");
    const auto choices = native_caption_choice_layout(
        operands.first(aliases * 2U + 2U), aliases, format[1], format[2]);
    const auto encoded = read16(operands, aliases * 2U);
    const auto scroll = std::int32_t{encoded} - (encoded >= 32768 ? 65536 : 0);
    std::vector<std::uint32_t> layout{choices[0], choices[1]};
    for (std::size_t at = aliases * 2U + 2U; at < operands.size(); at += 2) {
        const auto address = std::uint32_t{read16(operands, at)};
        require(address >= base && (address - base) % bytes == 0 &&
                    (address - base) / bytes < columns * rows,
                "Numbered choice cell outside source canvas");
        const auto cell = (address - base) / bytes;
        const auto y = static_cast<std::int32_t>((cell / columns) * pixels) -
                       static_cast<std::int32_t>(baseline) - scroll;
        require(y >= 0 && y < static_cast<std::int32_t>(rows * pixels),
                "Numbered choice outside native canvas");
        layout.push_back((cell % columns) * pixels);
        layout.push_back(static_cast<std::uint32_t>(y));
    }
    return layout;
}

Bytes make_native_number_choice(ByteView operands, std::span<const std::uint32_t> format) {
    require(format.size() == 16, "Invalid numbered choice native format");
    const auto layout = native_number_choice_layout(operands, format.first(9));
    const auto index = format[9], stack = format[10], ordinal = format[11], digits = format[12];
    const auto condition = format[13], code_address = format[14], helper_address = format[15];
    require(index < (layout.size() - 2U) / 2U && stack <= 4095 && stack % 8 == 0 &&
                ordinal <= 65535 && digits > 0 && digits <= 10 && condition < 14 &&
                code_address % 4 == 0 && helper_address % 4 == 0,
            "Invalid numbered choice ABI or linkage");
    Bytes code;
    const auto word = [&](std::uint32_t value) { append32(code, value); };
    const auto immediate = [&](unsigned reg, std::uint32_t value) {
        require(value <= 65535, "Numbered argument exceeds native immediate");
        word(0x52800000U | (value << 5U) | reg);
    };
    append_condition_choice(code, layout[0], layout[1], 5, condition);
    word(0x910003e0U | (stack << 10U)); // add x0,sp,#native canvas displacement
    immediate(1, ordinal);
    immediate(2, digits);
    immediate(3, layout[2U + index * 2U]);
    immediate(4, layout[3U + index * 2U]);
    const auto call = std::uint64_t{code_address} + code.size();
    require(call + 4 <= std::uint64_t{UINT32_MAX} + 1 &&
                !(helper_address >= code_address && helper_address < call + 4),
            "Numbered call overlaps or exceeds its fragment");
    const auto delta = std::int64_t{helper_address} - static_cast<std::int64_t>(call);
    require(delta >= -(std::int64_t{1} << 27) && delta < (std::int64_t{1} << 27),
            "Numbered helper exceeds branch range");
    word(0x94000000U | (static_cast<std::uint32_t>(delta / 4) & 0x3ffffffU));
    return code;
}

std::array<std::uint32_t, 2> native_byte_choice_layout(ByteView source, unsigned aliases) {
    require(aliases > 0 && aliases <= 256 && source.size() == 2U * aliases,
            "Invalid byte-choice source extent");
    const std::array<std::uint32_t, 2> result{source[0], source[aliases]};
    for (unsigned side = 0; side < 2; ++side)
        for (unsigned i = 0; i < aliases; ++i)
            require(source[side * aliases + i] == result[side],
                    "Independent byte choices disagree");
    return result;
}
Bytes make_native_byte_choice(const std::array<std::uint32_t, 2>& values,
                              unsigned input, unsigned result, unsigned scratch) {
    require(values[0] <= 255 && values[1] <= 255 && input < 31 && result < 31 &&
                scratch < 31 && scratch != input && scratch != result,
            "Invalid byte-choice values or native registers");
    Bytes code;
    const auto word = [&](std::uint32_t value) { append32(code, value); };
    const auto move = [&](unsigned reg, unsigned value) {
        word(0x52800000U | (value << 5U) | reg);
    };
    if (values[0] == values[1]) {
        move(result, values[0]);
    } else if (values[0] <= 1 && values[1] <= 1) {
        // A normalized Boolean already is the ascending unit choice. EOR #1
        // inverts it for descending choices. Selection comes from supplied data.
        word(values[0] == 0 ? 0x2a0003e0U | (input << 16U) | result
                            : 0x52000000U | (input << 5U) | result);
    } else {
        // false XOR (selector * (false XOR true)) selects arbitrary bytes.
        // Multiplication reads the input before writing an aliased result.
        // None of these arithmetic instructions alters the caller's flags.
        move(scratch, values[0] ^ values[1]);
        word(0x1b007c00U | (scratch << 16U) | (input << 5U) | result);
        move(scratch, values[0]);
        word(0x4a000000U | (scratch << 16U) | (result << 5U) | result);
    }
    return code;
}

Bytes make_native_pointer_byte_choice(const std::array<std::uint32_t, 2>& values,
                                     std::span<const std::uint32_t> format) {
    require(format.size() == 7, "Invalid pointer/choice format extent");
    for (unsigned i = 0; i < 6; ++i)
        require(format[i] < 31, "Invalid pointer/choice register");
    require(format[6] < 64 && format[3] != format[0] &&
                format[3] != format[1] && format[3] != format[2],
            "Pointer argument overlaps byte-choice working registers");
    auto choice = make_native_byte_choice(values, format[0], format[1], format[2]);
    Bytes code;
    append32(code, 0x8b000000U | (format[5] << 16U) | (format[6] << 10U) |
                       (format[4] << 5U) | format[3]);
    code.insert(code.end(), choice.begin(), choice.end());
    return code;
}

Bytes make_native_boolean_argument(ByteView operand, unsigned shift, unsigned width,
                                   unsigned input_register, unsigned argument_register,
                                   std::uint32_t code_address, std::uint32_t helper_address) {
    require(operand.size() == 1 && width >= 1 && width <= 3 && shift <= 8 - width &&
                input_register < 31 && argument_register < 8 &&
                code_address % 4 == 0 && helper_address % 4 == 0,
            "Invalid native Boolean argument format");
    const auto scale = (unsigned{operand[0]} >> shift) & ((1U << width) - 1U);
    Bytes code;
    const auto word = [&](std::uint32_t value) { append32(code, value); };
    if (!scale) {
        word(0x52800000U | argument_register); // movz argument,#0
    } else {
        // EOR #1 inverts a normalized Boolean. Its unit value is the native
        // Boolean encoding, not a retained selection from the input byte.
        word(0x52000000U | (input_register << 5U) | argument_register);
        const auto low = static_cast<unsigned>(std::countr_zero(scale));
        const auto odd = scale >> low;
        const auto merge = [&](unsigned distance) {
            word(0x2a000000U | (argument_register << 16U) | (distance << 10U) |
                 (argument_register << 5U) | argument_register);
        };
        if (odd > 1) {
            const auto high = static_cast<unsigned>(std::bit_width(odd)) - 1U;
            if (std::popcount(odd) == 2) {
                merge(high);
            } else {
                // The remaining odd three-bit value is a contiguous run.
                // Starting from 0/1, each ORR adds its next occupied bit.
                for (unsigned i = 0; i < high; ++i) merge(1);
            }
        }
        if (low)
            word(0x53000000U | ((32U - low) << 16U) | ((31U - low) << 10U) |
                 (argument_register << 5U) | argument_register);
    }
    const auto call = std::uint64_t{code_address} + code.size();
    require(call + 4 <= std::uint64_t{UINT32_MAX} + 1 &&
                !(helper_address >= code_address && helper_address < call + 4),
            "Native Boolean call overlaps or exceeds its fragment");
    const auto delta = std::int64_t{helper_address} - static_cast<std::int64_t>(call);
    require(delta >= -(std::int64_t{1} << 27) && delta < (std::int64_t{1} << 27),
            "Native Boolean helper exceeds branch range");
    word(0x94000000U | (static_cast<std::uint32_t>(delta / 4) & 0x3ffffffU));
    return code;
}
Bytes make_native_tagged_byte_constant(unsigned value, unsigned tag, unsigned destination) {
    require(value <= 255 && tag <= 255 && destination < 31,
            "Invalid native tagged-byte constant");
    std::uint32_t word = 0;
    if (value == 0) {
        // MOVZ clears the low word implicitly. Its upper-word immediate is
        // native type metadata; the source value determines this lowering.
        word = 0xd2c00000U | (tag << 5U) | destination;
    } else if (tag == 0) {
        word = 0xd2800000U | (value << 5U) | destination;
    } else {
        // ORR xD,xzr can hold a repeated word made of one contiguous run.
        // Both halves must agree. The native tag alone cannot supply the cue.
        require(value == tag, "Tagged byte requires more than one native word");
        const auto trailing = static_cast<unsigned>(std::countr_zero(value));
        const auto run = value >> trailing;
        require((run & (run + 1U)) == 0,
                "Tagged byte lacks a single-word logical representation");
        const auto rotation = (32U - trailing) & 31U;
        const auto ones = static_cast<unsigned>(std::bit_width(run));
        word = 0xb20003e0U | (rotation << 16U) | ((ones - 1U) << 10U) | destination;
    }
    Bytes code;
    append32(code, word);
    return code;
}

Bytes make_native_inline_byte_append(unsigned value, std::span<const std::uint32_t> f) {
    require(value <= 255 && f.size() == 11, "Invalid byte append value/format");
    const auto data = f[0], unit = f[1], vector = f[2], end = f[3], capacity = f[4];
    const auto temporary = f[5], pointer = f[6], byte = f[7], guard = f[8];
    const auto address = f[9], target = f[10];
    const std::array<unsigned, 6> registers{data, vector, end, capacity, temporary, pointer};
    for (unsigned i = 0; i < registers.size(); ++i) {
        require(registers[i] < 31, "Invalid byte append register");
        for (unsigned j = 0; j < i; ++j)
            require(registers[i] != registers[j], "Overlapping byte append registers");
    }
    require(unit <= 1 && byte < 512 && guard <= 480 && guard % 8 == 0 &&
                (byte < guard || byte >= guard + 32) && address % 4 == 0 && target % 4 == 0,
            "Invalid byte append stack/linkage");
    Bytes code;
    const auto word = [&](std::uint32_t v) { append32(code, v); };
    const auto selected = value == 0 ? 31U : data;
    if (value != 0 && !(unit && value == 1))
        word(0x52800000U | (value << 5U) | data);
    word(0x390003e0U | (byte << 10U) | selected); // byte temporary
    word(0xa9400000U | (1U << 15U) | (capacity << 10U) | (vector << 5U) | end);
    word(0xa90003e0U | (((guard + 16) / 8) << 15U) | (end << 10U) | vector);
    word(0x910003e0U | ((guard + 24) << 10U) | temporary);
    word(0x910003e0U | (byte << 10U) | pointer);
    word(0xa90003e0U | ((guard / 8) << 15U) | (pointer << 10U) | temporary);
    word(0xeb00001fU | (capacity << 16U) | (end << 5U));
    const auto branch = std::uint64_t{address} + code.size();
    const auto finish = branch + 12;
    require(finish <= std::uint64_t{UINT32_MAX} + 1 &&
                !(target >= address && target < finish), "Byte append target overlaps fragment");
    const auto delta = std::int64_t{target} - static_cast<std::int64_t>(branch);
    require(delta >= -(std::int64_t{1} << 20) && delta < (std::int64_t{1} << 20),
            "Byte append cold target out of range");
    word(0x54000002U | ((static_cast<std::uint32_t>(delta / 4) & 0x7ffffU) << 5U));
    word(0x38001400U | (end << 5U) | selected); // publish byte and advance once
    word(0xf9000400U | (vector << 5U) | end);
    return code;
}

Bytes make_native_stack_byte_argument(unsigned value, std::span<const std::uint32_t> f) {
    require(value <= 255 && (f.size() == 8 || f.size() == 10), "Invalid stack byte value/format");
    const auto scratch = f[0], byte = f[1], argument = f[2], object = f[3], save = f[4];
    const auto link = f[5], address = f[f.size() - 2], target = f.back();
    require(scratch > 0 && scratch < 31 && byte <= 4095 && argument > 0 && argument < 8 &&
                object <= 31 && object != 0 && save <= 31 && save != 0 &&
                scratch != argument && scratch != object && scratch != save &&
                argument != object && argument != save &&
                (save == 31 || save != object) && link <= 1 &&
                address % 4 == 0 && target % 4 == 0, "Invalid stack byte ABI/linkage");
    const bool frame = f.size() == 10;
    const auto base = frame ? f[6] : 31U;
    const auto mirror = frame ? f[7] : 31U;
    if (frame) {
        // A pinned native frame pointer is distinct from all inputs and outputs.
        // The optional mirror keeps the temporary's address for later native use.
        require(base == 29 && byte <= 256 &&
                    scratch != base && argument != base && object != base && save != base &&
                    (mirror == 31 || (mirror > 0 && mirror < 29 && mirror != scratch &&
                        mirror != argument && mirror != object && mirror != save)),
                "Invalid frame byte ABI");
    }
    Bytes code;
    const auto word = [&](std::uint32_t v) { append32(code, v); };
    const auto selected = value == 0 ? 31U : scratch;
    if (value) word(0x52800000U | (value << 5U) | scratch);
    if (frame) {
        word(0x38000000U | (((0U - byte) & 511U) << 12U) | (base << 5U) | selected);
        if (mirror != 31) word(0xd1000000U | (byte << 10U) | (base << 5U) | mirror);
        word(0xd1000000U | (byte << 10U) | (base << 5U) | argument);
    } else {
        word(0x390003e0U | (byte << 10U) | selected);
        word(0x910003e0U | (byte << 10U) | argument);
    }
    if (save != 31) word(0xaa0003e0U | save);
    if (object != 31) word(0xaa0003e0U | (object << 16U));
    const auto branch = std::uint64_t{address} + code.size();
    require(branch + 4 <= std::uint64_t{UINT32_MAX} + 1 &&
                !(target >= address && target < branch + 4), "Stack byte target overlaps fragment");
    const auto delta = std::int64_t{target} - static_cast<std::int64_t>(branch);
    require(delta >= -(std::int64_t{1} << 27) && delta < (std::int64_t{1} << 27),
            "Stack byte target out of range");
    word((link ? 0x94000000U : 0x14000000U) |
         (static_cast<std::uint32_t>(delta / 4) & 0x3ffffffU));
    return code;
}

Bytes make_native_split_index_branch(ByteView operands, std::span<const std::uint32_t> format) {
    const auto valid = [](bool ok) {
        if (!ok) throw std::invalid_argument("Invalid native split-index branch");
    };
    valid(format.size() == 9);
    const auto origin = format[0], stride = format[1], aliases = format[2];
    const auto low_bits = format[3], high_bits = format[4];
    const auto index_reg = format[5], high_reg = format[6], scratch = format[7], delta = format[8];
    valid(origin <= 65535 && stride > 0 && stride <= 256 && aliases > 0 && aliases <= 256 &&
          low_bits > 0 && low_bits <= 11 && high_bits > 0 && high_bits <= 12-low_bits &&
          index_reg < 31 && high_reg < 31 && scratch < 31 && index_reg != high_reg &&
          index_reg != scratch && high_reg != scratch && delta > 0 && delta < (1U << 18) &&
          operands.size() == aliases * 2U);
    const auto address = std::uint32_t{read16(operands,0)};
    valid(address > origin && (address-origin) % stride == 0);
    for (std::size_t at = 2; at < operands.size(); at += 2)
        valid(read16(operands,at) == address);
    const auto boundary = (address-origin) / stride;
    valid(boundary <= (1U << (low_bits+high_bits)));
    Bytes code;
    if (boundary == (1U << low_bits)) {
        // At the split, any nonzero high part exceeds the lower bank. The
        // native format maps the low flag bits into their full-index position.
        append32(code,0x53000000U | ((32U-low_bits) << 16U) | ((high_bits-1U) << 10U) |
                      (high_reg << 5U) | scratch);
        append32(code,0x35000000U | (delta << 5U) | scratch);
    } else {
        append32(code,0x7100001fU | ((boundary-1U) << 10U) | (index_reg << 5U));
        append32(code,0x54000008U | (delta << 5U));
    }
    return code;
}
Bytes make_native_selection_branch(ByteView operands, std::span<const std::uint32_t> format) {
    const auto valid = [](bool ok) {
        if (!ok) throw std::invalid_argument("Invalid native selection branch");
    };
    valid(format.size() == 7 || format.size() == 9);
    const auto source_shift = format[0], width = format[1], shift = format[2];
    const auto input = format[3], scratch = format[4], target = format[5], choices = format[6];
    valid(width > 0 && width <= 5 && source_shift <= 16-width && shift <= 11-width &&
          input < 31 && scratch < 31 && input != scratch && target >= 4 && target < (1U << 18) &&
          (choices == 1 || choices == 2) && (choices != 1 || shift == 0) &&
          !operands.empty() && operands.size() <= 512 && operands.size() % 2 == 0 &&
          (choices != 2 || operands.size() == 4));
    if (format.size() == 9)
        valid(choices == 1 && format[7] < 31 && format[8] <= 32760 && format[8] % 8 == 0);
    const auto domain = (1U << width)-1U;
    const auto selected = [&](std::size_t at) { return (read16(operands,at) >> source_shift) & domain; };
    const unsigned first = selected(0);
    Bytes code;
    const auto word = [&](std::uint32_t value) { append32(code,value); };
    const auto compare = [&](unsigned reg, unsigned value) { word(0x7100001fU | (value << 10U) | (reg << 5U)); };
    const auto branch = [&] {
        const auto delta = target - static_cast<unsigned>(code.size()/4);
        word(0x54000001U | (delta << 5U));
    };
    if (choices == 1) {
        for (std::size_t at = 2; at < operands.size(); at += 2) valid(selected(at) == first);
        if (!first) word(0x35000000U | (target << 5U) | input);
        else { compare(input,first); branch(); }
        if (format.size() == 9)
            word(0xf94003e0U | ((format[8]/8U) << 10U) | format[7]);
        return code;
    }
    const unsigned second = selected(2), difference = first ^ second;
    const unsigned mask = (domain ^ difference) << shift;
    // Two choices differing in one bit can share a masked comparison. Only a
    // contiguous mask uses this compact native immediate; all other pairs use
    // explicit comparisons, so no particular palette choice is a code constant.
    const unsigned low = mask ? static_cast<unsigned>(std::countr_zero(mask)) : 0;
    const unsigned run = mask >> low;
    if (std::has_single_bit(difference) && mask && (run & (run+1U)) == 0) {
        const unsigned bits = static_cast<unsigned>(std::bit_width(run));
        const unsigned encoded = (((32U-low)&31U) << 6U) | (bits-1U);
        const unsigned common = (first & second) << shift;
        if (!common) word(0x7200001fU | (encoded << 10U) | (input << 5U));
        else {
            word(0x12000000U | (encoded << 10U) | (input << 5U) | scratch);
            compare(scratch,common);
        }
        branch();
    } else {
        word(0x53000000U | (shift << 16U) | ((shift+width-1U) << 10U) | (input << 5U) | scratch);
        compare(scratch,first);
        // If the first matched, retain Z=1; otherwise compare the second.
        word(0x7a401804U | (second << 16U) | (scratch << 5U));
        branch();
    }
    return code;
}
} // namespace ff6
