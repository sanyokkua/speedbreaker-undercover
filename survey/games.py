"""Per-game survey parameters: which files the survey scripts read and write,
and the image's section ranges. SB_GAME picks the game (mw by default), as
it does for CMake:

    python3 survey/fix_switch_tables.py                  # Most Wanted
    SB_GAME=carbon python3 survey/fix_switch_tables.py   # Carbon

Paths are relative to the repository root (run the scripts from there).
Section ranges come from survey/xexsurvey.cpp's report (game*/survey/report.md).
"""
import os

GAMES = {
    # NFS: Most Wanted (Xbox 360, USA), default.xex SHA-256 aebdf3c1...
    'mw': dict(
        config='config/nfsmw.toml',
        switch_tables='config/switch_tables.toml',
        switch_tables_inline='config/switch_tables_inline.toml',
        survey='game/survey',          # image.bin, switch_tables.raw.toml, recomp.log
        ppc='ppc',
        text=(0x820E0000, 0x828C8BA0),  # .text + the .embsec code sections
        pdata=(0x820B1200, 0x820DD6B8),
        data=[(0x82000600, 0x820B106C), (0x828D0000, 0x82C8CE8C)],  # .rdata, .data
        # fix_switch_tables.py: XenonAnalyse misread code as tables (their
        # "labels" are instruction words).
        drop={0x825725B0, 0x82576FD0},
        # fix_switch_tables.py: tables XenonAnalyse missed.
        add=[
            # 0x825A45D0: lis/addi r12=0x82063BA8; lhzx r0 = half[r5]; r12 = 0x825A45F8 + r0;
            # bctr. No bounds check (r5 is a trusted argument); 18 entries, the next
            # halfword is ASCII ("un"). Offsets relative to 0x825A45F8.
            dict(base=0x825A45D0, r=5, labels=[
                0x825A45F8, 0x825A4618, 0x825A4644, 0x825A465C, 0x825A46C4, 0x825A46E0,
                0x825A4720, 0x825A47C4, 0x825A4848, 0x825A4898, 0x825A48F8, 0x825A492C,
                0x825A4970, 0x825A49A0, 0x825A49CC, 0x825A49F8, 0x825A4A2C, 0x825A47B4]),
        ],
        # leaf_switch_bounds.py: hand-found bounds. Tail-call stubs that end in
        # `b +4` into the next (.pdata) function: without an explicit size
        # XenonRecomp treats the b as internal and swallows the neighbour,
        # including its jump table.
        manual_bounds={
            0x8218C508: 0x10,
            0x822F33D8: 0x28,
        },
    ),
    # NFS: Carbon Collector's Edition (Xbox 360, USA; title 454107EC, media
    # 1F8BEB88), default.xex SHA-256 922c7a1a...
    'carbon': dict(
        config='config/carbon/carbon.toml',
        switch_tables='config/carbon/switch_tables.toml',
        switch_tables_inline='config/carbon/switch_tables_inline.toml',
        survey='game-carbon/survey',
        ppc='ppc-carbon',
        text=(0x82110000, 0x82B35A64),
        pdata=(0x820C9400, 0x82105C40),
        data=[(0x82000600, 0x820C928C), (0x82B40000, 0x8301DDF0)],
        drop=set(),
        add=[
            # Byte-offset tables: lis/addi r12=table; lbzx r0 = byte[rI];
            # r12 = base + r0; mtctr; nop; nop; bctr. No bounds check (a
            # trusted index); XenonAnalyse finds neither. Offsets are bytes
            # from the instruction after the bctr.
            # 0x8268BED0 (leaf, after padding): table 0x82064E50, index r3,
            # 26 entries and 6 zeros to the table's 32-byte end (zero is
            # case 0's code: kept, harmless).
            dict(base=0x8268BED4, r=3, labels=[
                0x8268BEFC, 0x8268BF0C, 0x8268BF14, 0x8268BF24, 0x8268BF2C, 0x8268BF34,
                0x8268BF3C, 0x8268BF34, 0x8268BF3C, 0x8268BF44, 0x8268BF44, 0x8268BF64,
                0x8268BF64, 0x8268BF4C, 0x8268BF44, 0x8268BF64, 0x8268BF64, 0x8268BF5C,
                0x8268BF04, 0x8268BF4C, 0x8268BF1C, 0x8268BF64, 0x8268BF54, 0x8268BF64,
                0x8268BF64, 0x8268BF6C, 0x8268BEFC, 0x8268BEFC, 0x8268BEFC, 0x8268BEFC,
                0x8268BEFC, 0x8268BEFC]),
            # 0x826F8744: table 0x82064EF8, index r10 = r8 & 63; 9 cases and 7
            # zeros (16 bytes; the bytes after aren't multiples of 4, so not
            # offsets).
            dict(base=0x826F8744, r=10, labels=[
                0x826F876C, 0x826F8778, 0x826F8784, 0x826F8790, 0x826F879C, 0x826F87A8,
                0x826F87B4, 0x826F87C0, 0x826F87D0, 0x826F876C, 0x826F876C, 0x826F876C,
                0x826F876C, 0x826F876C, 0x826F876C, 0x826F876C]),
        ],
        # Tail-call stubs that end in `b +4` into the next (.pdata) function,
        # as Most Wanted's: without an explicit size XenonRecomp treats the b
        # as internal and swallows the neighbour, and 4 of its jump tables'
        # cases end up outside the function (0x8222ECD8, 0x8237EDC0).
        manual_bounds={
            0x8222ECC8: 0x10,
            0x8237ED98: 0x28,
        },
    ),
    # NFS: ProStreet (Xbox 360, USA, En/Es), default.xex SHA-256 4f8b623a...
    # Sections from game-prostreet/survey/xexsurvey.txt.
    'prostreet': dict(
        config='config/prostreet/prostreet.toml',
        switch_tables='config/prostreet/switch_tables.toml',
        switch_tables_inline='config/prostreet/switch_tables_inline.toml',
        survey='game-prostreet/survey',
        ppc='ppc-prostreet',
        text=(0x82140000, 0x82ADCED4),
        pdata=(0x820F1400, 0x821344C0),
        data=[(0x82000400, 0x820F1354), (0x82AE0000, 0x8309C790)],
        drop=set(),
        add=[
            # Byte-offset tables as Carbon's: lis/addi r12=table; lbzx r0 =
            # byte[rI]; r12 = base + r0; mtctr; nop; nop; bctr. No bounds
            # check (a trusted index); XenonAnalyse finds neither. Entries run
            # to the table's 16-byte end (zero is case 0's code: harmless).
            # 0x8286ECE4 (in .pdata fn 0x8286EB38): table 0x82071B70, index
            # r10, 9 cases and 7 zeros.
            dict(base=0x8286ECE4, r=10, labels=[0x8286ECE8, 0x8286ECF4, 0x8286ED00, 0x8286ED0C, 0x8286ED18, 0x8286ED24, 0x8286ED30, 0x8286ED3C, 0x8286ED4C, 0x8286ECE8, 0x8286ECE8, 0x8286ECE8, 0x8286ECE8, 0x8286ECE8, 0x8286ECE8, 0x8286ECE8]),
            # 0x82898F34 (leaf after .pdata fn 0x82898DB0): table 0x820719B0,
            # index r3, 12 cases and 4 zeros.
            dict(base=0x82898F34, r=3, labels=[0x82898F38, 0x82898F48, 0x82898F58, 0x82898F68, 0x82898F78, 0x82898F88, 0x82898F98, 0x82898FA8, 0x82898FB8, 0x82898FC8, 0x82898FD8, 0x82898FE8, 0x82898F38, 0x82898F38, 0x82898F38, 0x82898F38]),
        ],
        # Tail-call stubs that end in `b +4` into the next (.pdata) function,
        # as Most Wanted's and Carbon's: without an explicit size XenonRecomp
        # treats the b as internal and swallows the neighbour with its jump
        # tables (0x823C3800, 0x824BBE14, 0x824BC044, 0x824BC228).
        manual_bounds={
            0x823C37C8: 0x10,
            0x824BBC90: 0x28,
        },
    ),
    # NFS: Undercover (Xbox 360, USA, En/Fr/Es), default.xex SHA-256 97ac0c07...
    # Sections from game-undercover/survey/xexsurvey.txt.
    'undercover': dict(
        config='config/undercover/undercover.toml',
        switch_tables='config/undercover/switch_tables.toml',
        switch_tables_inline='config/undercover/switch_tables_inline.toml',
        survey='game-undercover/survey',
        ppc='ppc-undercover',
        text=(0x821E0000, 0x83085E2C),
        pdata=(0x82172800, 0x821D21C8),
        data=[(0x82000400, 0x82172680), (0x83090000, 0x83760538)],
        drop=set(),
        add=[
            # Offset tables, as Carbon's and ProStreet's: lis/addi r12=table;
            # lbzx (or slwi+lhzx) r0 = table[rI]; r12 = base + r0 (scaled by 4
            # at 0x82A774D8); mtctr; bctr. XenonAnalyse finds none. Most switch
            # on a trusted argument with no bounds check: entries run while the
            # offset is word-aligned and lands inside the function
            # (checked by hand); 0x82B140BC (r9 <= 14,
            # cmplwi 0x82B14030) and 0x82C929FC (r11 <= 34) are bounded.
            dict(base=0x8291FE44, r=10, labels=[0x8291FE48, 0x8291FE54, 0x8291FE60, 0x8291FE6C, 0x8291FE78, 0x8291FE84, 0x8291FE90, 0x8291FE9C, 0x8291FEAC, 0x8291FE48, 0x8291FE48, 0x8291FE48, 0x8291FE48, 0x8291FE48, 0x8291FE48, 0x8291FE48, 0x8291FE48]),
            dict(base=0x829375B4, r=5, labels=[0x829375B8, 0x8293760C, 0x829375D0, 0x829375D0, 0x829375D0, 0x829375B8, 0x829375D8, 0x829375D8, 0x829375B8, 0x829375B8, 0x829375E8, 0x829375D0, 0x829375D0, 0x829375F0, 0x829375F8, 0x829375B8, 0x829375B8, 0x829375B8, 0x829375B8]),
            dict(base=0x82A1F5D8, r=6, labels=[0x82A1FA94, 0x82A1FAC8, 0x82A1F928, 0x82A1F9DC, 0x82A1F5DC, 0x82A1F700, 0x82A1F818, 0x82A1FB98, 0x82A1FC2C, 0x82A1FC98, 0x82A1FCFC, 0x82A1FD60, 0x82A1FAFC, 0x82A1FB30, 0x82A1FB64, 0x82A1F5DC]),
            dict(base=0x82A62FBC, r=7, labels=[0x82A62FC0, 0x82A62FCC, 0x82A62FD4, 0x82A62FE0, 0x82A62FEC, 0x82A62FF8, 0x82A6304C, 0x82A63058]),
            dict(base=0x82A774D8, r=5, labels=[0x82A774DC, 0x82A77540, 0x82A775B4, 0x82A775C8, 0x82A77600, 0x82A77650, 0x82A776AC, 0x82A776B4, 0x82A776BC, 0x82A776C4, 0x82A776CC, 0x82A776E0, 0x82A776F4, 0x82A774DC, 0x82A774DC, 0x82A774DC, 0x82A774DC, 0x82A774DC, 0x82A774DC, 0x82A775BC, 0x82A774DC]),
            dict(base=0x82B140BC, r=9, labels=[0x82B14100, 0x82B14108, 0x82B14110, 0x82B14118, 0x82B14120, 0x82B14128, 0x82B14130, 0x82B14138, 0x82B14140, 0x82B14148, 0x82B14150, 0x82B1415C, 0x82B14164, 0x82B1416C, 0x82B14174]),
            dict(base=0x82BF4D7C, r=28, labels=[0x82BF4D80, 0x82BF4D80, 0x82BF4854, 0x82BF4D80, 0x82BF4D80, 0x82BF4D80, 0x82BF4D80, 0x82BF4854, 0x82BF4854, 0x82BF4D80, 0x82BF4854, 0x82BF4854, 0x82BF4D80, 0x82BF4D80, 0x82BF4D80, 0x82BF4D80]),
            dict(base=0x82C929FC, r=11, labels=[0x82C92A00, 0x82C92A20, 0x82C92AC8, 0x82C92AC8, 0x82C92A00, 0x82C92A00, 0x82C92A58, 0x82C92A00, 0x82C92A94, 0x82C92A94, 0x82C92A94, 0x82C92A94, 0x82C92A94, 0x82C92A94, 0x82C92A94, 0x82C92A94, 0x82C92A94, 0x82C92A94, 0x82C92A94, 0x82C92A94, 0x82C92A94, 0x82C92A94, 0x82C92A94, 0x82C92A94, 0x82C92A94, 0x82C92A94, 0x82C92AE4, 0x82C92AE4, 0x82C92A70, 0x82C92A10, 0x82C92D68, 0x82C92DE0, 0x82C92B08, 0x82C92B08, 0x82C9AEB8]),
        ],
        # Tail-call stubs that end in `b +4` into the next (.pdata) function,
        # as the other games': without an explicit size XenonRecomp treats the
        # b as internal and swallows the neighbour with its jump tables
        # (0x82847784, 0x824B4EE0/0x824B5110/0x824B52F0, 0x82938568).
        manual_bounds={
            0x82847710: 0x10,
            0x824B4D40: 0x28,
            0x82938450: 0x18,
            # Leaf after padding with the 0x829375B4 offset table, up to the
            # next .pdata function.
            0x82937570: 0xA8,
        },
    ),
}

NAME = os.environ.get('SB_GAME', 'mw')
if NAME not in GAMES:
    raise SystemExit(f'SB_GAME={NAME}: not one of {", ".join(GAMES)}')
G = GAMES[NAME]
