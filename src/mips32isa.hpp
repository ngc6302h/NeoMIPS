//#pragma warning(disable : 4114)
#pragma once
#include <string_view>
#include <array>

namespace NeoMIPS
{
    namespace ISA
    {
        namespace Encoding
        {
            //traditional enum instead of enum class just so we don't have to cast it to int every time
            
            namespace fmt
            {
                enum fmt : uint32_t
                {
                    S = 16, // Single
                    D = 17, // Double
                    W = 20, // Word
                    L = 21, // Long
                    PS = 22 //Paired single
                };
            }
            namespace cop
            {
                enum cop : uint32_t
                {
                    SPECIAL = 0b000000,
                    COP1 = 0b010001
                };
            }
        }

        namespace Directives
        {
            enum class Directive
            {
                ALIGN,
                ASCII,
                ASCIIZ,
                BYTE,
                DATA,
                DOUBLE,
                END_MACRO,
                EQV,
                EXTERN,
                FLOAT,
                GLOBL,
                HALF,
                INCLUDE,
                KDATA,
                KTEXT,
                MACRO,
                SET,
                SPACE,
                TEXT,
                WORD,
                invalid
            };

            namespace Literals
            {
                constexpr const std::u32string_view ALIGN{ U".align" };
                constexpr const std::u32string_view ASCII{ U".ascii" };
                constexpr const std::u32string_view ASCIIZ{ U".asciiz" };
                constexpr const std::u32string_view BYTE{ U".byte" };
                constexpr const std::u32string_view DATA{ U".data" };
                constexpr const std::u32string_view DOUBLE{ U".double" };
                constexpr const std::u32string_view END_MACRO{ U".end_macro" };
                constexpr const std::u32string_view EQV{ U".eqv" };
                constexpr const std::u32string_view EXTERN{ U".extern" };
                constexpr const std::u32string_view FLOAT{ U".float" };
                constexpr const std::u32string_view GLOBL{ U".globl" };
                constexpr const std::u32string_view HALF{ U".half" };
                constexpr const std::u32string_view INCLUDE{ U".include" };
                constexpr const std::u32string_view KDATA{ U".kdata" };
                constexpr const std::u32string_view KTEXT{ U".ktext" };
                constexpr const std::u32string_view MACRO{ U".macro" };
                constexpr const std::u32string_view SET{ U".set" };
                constexpr const std::u32string_view SPACE{ U".space" };
                constexpr const std::u32string_view TEXT{ U".text" };
                constexpr const std::u32string_view WORD{ U".word" };
            }
        }

        namespace Instructions
        {
            enum class Instruction
            {
                ABS_D,
                ABS_S,
                ADD,
                ADD_D,
                ADD_S,
                ADDI,
                ADDIU,
                ADDU,
                AND,
                ANDI,
                BC1F,
                BC1T,
                BEQ,
                BGEZ,
                BGEZAL,
                BGTZ,
                BLEZ,
                BLTZ,
                BLTZAL,
                BNE,
                BREAK,
                C_EQ_D,
                C_EQ_S,
                C_LE_D,
                C_LE_S,
                C_LT_D,
                C_LT_S,
                CEIL_W_D,
                CEIL_W_S,
                CLO,
                CLZ,
                CVT_D_S,
                CVT_D_W,
                CVT_S_D,
                CVT_S_W,
                CVT_W_D,
                CVT_W_S,
                DIV,
                DIV_D,
                DIV_S,
                DIVU,
                ERET,
                FLOOR_W_D,
                FLOOR_W_S,
                J,
                JAL,
                JALR_RA,
                JALR,
                JR,
                LB,
                LBU,
                LDC1,
                LH,
                LHU,
                LL,
                LUI,
                LW,
                LWC1,
                LWL,
                LWR,
                MADD,
                MADDU,
                MFC0,
                MFC1,
                MFHI,
                MFLO,
                MOV_D,
                MOV_S,
                MOVF,
                MOVF_D,
                MOVF_S,
                MOVN,
                MOVN_D,
                MOVN_S,
                MOVT,
                MOVT_D,
                MOVT_S,
                MOVZ,
                MOVZ_D,
                MOVZ_S,
                MSUB,
                MSUBU,
                MTC0,
                MTC1,
                MTHI,
                MTLO,
                MUL,
                MUL_D,
                MUL_S,
                MULT,
                MULTU,
                NEG_D,
                NEG_S,
                NOP,
                NOR,
                OR,
                ORI,
                ROUND_W_D,
                ROUND_W_S,
                SB,
                SC,
                SDC1,
                SH,
                SLL,
                SLLV,
                SLT,
                SLTI,
                SLTIU,
                SLTU,
                SQRT_D,
                SQRT_S,
                SRA,
                SRAV,
                SRL,
                SRLV,
                SUB,
                SUB_D,
                SUB_S,
                SUBU,
                SW,
                SWC1,
                SWL,
                SWR,
                SYSCALL,
                TEQ,
                TEQI,
                TGE,
                TGEI,
                TGEIU,
                TGEU,
                TLT,
                TLTI,
                TLTIU,
                TLTU,
                TNE,
                TNEI,
                TRUNC_W_D,
                TRUNC_W_S,
                XOR,
                XORI,
                invalid
            };

            namespace Literals
            {
                //Real instructions
                constexpr const std::u32string_view ABS_D{ U"abs.d" };
                constexpr const std::u32string_view ABS_S{ U"abs.s" };
                constexpr const std::u32string_view ADD{ U"add" };
                constexpr const std::u32string_view ADD_D{ U"add.d" };
                constexpr const std::u32string_view ADD_S{ U"add.s" };
                constexpr const std::u32string_view ADDI{ U"addi" };
                constexpr const std::u32string_view ADDIU{ U"addiu" };
                constexpr const std::u32string_view ADDU{ U"addu" };
                constexpr const std::u32string_view AND{ U"and" };
                constexpr const std::u32string_view ANDI{ U"andi" };
                constexpr const std::u32string_view BC1F{ U"bc1f" };
                constexpr const std::u32string_view BC1T{ U"bc1t" };
                constexpr const std::u32string_view BEQ{ U"beq" };
                constexpr const std::u32string_view BGEZ{ U"bgez" };
                constexpr const std::u32string_view BGEZAL{ U"bgezal" };
                constexpr const std::u32string_view BGTZ{ U"bgtz" };
                constexpr const std::u32string_view BLEZ{ U"blez" };
                constexpr const std::u32string_view BLTZ{ U"bltz" };
                constexpr const std::u32string_view BLTZAL{ U"bltzal" };
                constexpr const std::u32string_view BNE{ U"bne" };
                constexpr const std::u32string_view BREAK{ U"break" };
                constexpr const std::u32string_view C_EQ_D{ U"c.eq.d" };
                constexpr const std::u32string_view C_EQ_S{ U"c.eq.s" };
                constexpr const std::u32string_view C_LE_D{ U"c.le.d" };
                constexpr const std::u32string_view C_LE_S{ U"c.le.s" };
                constexpr const std::u32string_view C_LT_D{ U"c.lt.d" };
                constexpr const std::u32string_view C_LT_S{ U"c.lt.s" };
                constexpr const std::u32string_view CEIL_W_D{ U"ceil.w.d" };
                constexpr const std::u32string_view CEIL_W_S{ U"ceil.w.s" };
                constexpr const std::u32string_view CLO{ U"clo" };
                constexpr const std::u32string_view CLZ{ U"clz" };
                constexpr const std::u32string_view CVT_D_S{ U"cvt.d.s" };
                constexpr const std::u32string_view CVT_D_W{ U"cvt.d.w" };
                constexpr const std::u32string_view CVT_S_D{ U"cvt.s.d" };
                constexpr const std::u32string_view CVT_S_W{ U"cvt.s.w" };
                constexpr const std::u32string_view CVT_W_D{ U"cvt.w.d" };
                constexpr const std::u32string_view CVT_W_S{ U"cvt.w.s" };
                constexpr const std::u32string_view DIV{ U"div" };
                constexpr const std::u32string_view DIV_D{ U"div.d" };
                constexpr const std::u32string_view DIV_S{ U"div.s" };
                constexpr const std::u32string_view DIVU{ U"divu" };
                constexpr const std::u32string_view ERET{ U"eret" };
                constexpr const std::u32string_view FLOOR_W_D{ U"floor.w.d" };
                constexpr const std::u32string_view FLOOR_W_S{ U"floor.w.s" };
                constexpr const std::u32string_view J{ U"j" };
                constexpr const std::u32string_view JAL{ U"jal" };
                constexpr const std::u32string_view JALR_RA{ U"jalr" };
                constexpr const std::u32string_view JALR{ U"jalr" };
                constexpr const std::u32string_view JR{ U"jr" };
                constexpr const std::u32string_view LB{ U"lb" };
                constexpr const std::u32string_view LBU{ U"lbu" };
                constexpr const std::u32string_view LDC1{ U"ldc1" };
                constexpr const std::u32string_view LH{ U"lh" };
                constexpr const std::u32string_view LHU{ U"lhu" };
                constexpr const std::u32string_view LL{ U"ll" };
                constexpr const std::u32string_view LUI{ U"lui" };
                constexpr const std::u32string_view LW{ U"lw" };
                constexpr const std::u32string_view LWC1{ U"lwc1" };
                constexpr const std::u32string_view LWL{ U"lwl" };
                constexpr const std::u32string_view LWR{ U"lwr" };
                constexpr const std::u32string_view MADD{ U"madd" };
                constexpr const std::u32string_view MADDU{ U"maddu" };
                constexpr const std::u32string_view MFC0{ U"mfc0" };
                constexpr const std::u32string_view MFC1{ U"mfc1" };
                constexpr const std::u32string_view MFHI{ U"mfhi" };
                constexpr const std::u32string_view MFLO{ U"mflo" };
                constexpr const std::u32string_view MOV_D{ U"mov.d" };
                constexpr const std::u32string_view MOV_S{ U"mov.s" };
                constexpr const std::u32string_view MOVF{ U"movf" };
                constexpr const std::u32string_view MOVF_D{ U"movf.d" };
                constexpr const std::u32string_view MOVF_S{ U"movf.s" };
                constexpr const std::u32string_view MOVN{ U"movn" };
                constexpr const std::u32string_view MOVN_D{ U"movn.d" };
                constexpr const std::u32string_view MOVN_S{ U"movn.s" };
                constexpr const std::u32string_view MOVT{ U"movt" };
                constexpr const std::u32string_view MOVT_D{ U"movt.d" };
                constexpr const std::u32string_view MOVT_S{ U"movt.s" };
                constexpr const std::u32string_view MOVZ{ U"movz" };
                constexpr const std::u32string_view MOVZ_D{ U"movz.d" };
                constexpr const std::u32string_view MOVZ_S{ U"movz.s" };
                constexpr const std::u32string_view MSUB{ U"msub" };
                constexpr const std::u32string_view MSUBU{ U"msubu" };
                constexpr const std::u32string_view MTC0{ U"mtc0" };
                constexpr const std::u32string_view MTC1{ U"mtc1" };
                constexpr const std::u32string_view MTHI{ U"mthi" };
                constexpr const std::u32string_view MTLO{ U"mtlo" };
                constexpr const std::u32string_view MUL{ U"mul" };
                constexpr const std::u32string_view MUL_D{ U"mul.d" };
                constexpr const std::u32string_view MUL_S{ U"mul.s" };
                constexpr const std::u32string_view MULT{ U"mult" };
                constexpr const std::u32string_view MULTU{ U"multu" };
                constexpr const std::u32string_view NEG_D{ U"neg.d" };
                constexpr const std::u32string_view NEG_S{ U"neg.s" };
                constexpr const std::u32string_view NOP{ U"nop" };
                constexpr const std::u32string_view NOR{ U"nor" };
                constexpr const std::u32string_view OR{ U"or" };
                constexpr const std::u32string_view ORI{ U"ori" };
                constexpr const std::u32string_view ROUND_W_D{ U"round.w.d" };
                constexpr const std::u32string_view ROUND_W_S{ U"round.w.s" };
                constexpr const std::u32string_view SB{ U"sb" };
                constexpr const std::u32string_view SC{ U"sc" };
                constexpr const std::u32string_view SDC1{ U"sdc1" };
                constexpr const std::u32string_view SH{ U"sh" };
                constexpr const std::u32string_view SLL{ U"sll" };
                constexpr const std::u32string_view SLLV{ U"sllv" };
                constexpr const std::u32string_view SLT{ U"slt" };
                constexpr const std::u32string_view SLTI{ U"slti" };
                constexpr const std::u32string_view SLTIU{ U"sltiu" };
                constexpr const std::u32string_view SLTU{ U"sltu" };
                constexpr const std::u32string_view SQRT_D{ U"sqrt.d" };
                constexpr const std::u32string_view SQRT_S{ U"sqrt.s" };
                constexpr const std::u32string_view SRA{ U"sra" };
                constexpr const std::u32string_view SRAV{ U"srav" };
                constexpr const std::u32string_view SRL{ U"srl" };
                constexpr const std::u32string_view SRLV{ U"srlv" };
                constexpr const std::u32string_view SUB{ U"sub" };
                constexpr const std::u32string_view SUB_D{ U"sub.d" };
                constexpr const std::u32string_view SUB_S{ U"sub.s" };
                constexpr const std::u32string_view SUBU{ U"subu" };
                constexpr const std::u32string_view SW{ U"sw" };
                constexpr const std::u32string_view SWC1{ U"swc1" };
                constexpr const std::u32string_view SWL{ U"swl" };
                constexpr const std::u32string_view SWR{ U"swr" };
                constexpr const std::u32string_view SYSCALL{ U"syscall" };
                constexpr const std::u32string_view TEQ{ U"teq" };
                constexpr const std::u32string_view TEQI{ U"teqi" };
                constexpr const std::u32string_view TGE{ U"tge" };
                constexpr const std::u32string_view TGEI{ U"tgei" };
                constexpr const std::u32string_view TGEIU{ U"tgeiu" };
                constexpr const std::u32string_view TGEU{ U"tgeu" };
                constexpr const std::u32string_view TLT{ U"tlt" };
                constexpr const std::u32string_view TLTI{ U"tlti" };
                constexpr const std::u32string_view TLTIU{ U"tltiu" };
                constexpr const std::u32string_view TLTU{ U"tltu" };
                constexpr const std::u32string_view TNE{ U"tne" };
                constexpr const std::u32string_view TNEI{ U"tnei" };
                constexpr const std::u32string_view TRUNC_W_D{ U"trunc.w.d" };
                constexpr const std::u32string_view TRUNC_W_S{ U"trunc.w.s" };
                constexpr const std::u32string_view XOR{ U"xor" };
                constexpr const std::u32string_view XORI{ U"xori" };
            }
        }

        namespace PseudoInstructions
        {
            //PURE pseudoinstructions - instructions that do not exist as variations of pure instructions. These are just aliases for real instructions
            enum class Pseudoinstruction
            {
                B,
                BEQ,
                BEQZ,
                BGE,
                BGEU,
                BGT,
                BGTU,
                BLE,
                BLEU,
                BLT,
                BLTU,
                BNEZ,
                L_D,
                L_S,
                LA,
                LD,
                LI,
                MFC1_D,
                MOVE,
                MTC1_D,
                MULO,
                MULOU,
                MULU,
                NEG,
                NEGU,
                NOT,
                REM,
                REMU,
                ROL,
                ROR,
                S_D,
                S_S,
                SD,
                SEQ,
                SGE,
                SGEU,
                SGT,
                SGTU,
                SLE,
                SLEU,
                SNE,
                SUBI,
                SUBIU,
                ULH,
                ULHU,
                ULW,
                USH,
                USW,
                invalid
            };

            namespace Literals
            {
                constexpr const std::u32string_view B{ U"b" };
                constexpr const std::u32string_view BEQ{ U"beq" };
                constexpr const std::u32string_view BEQZ{ U"beqz" };
                constexpr const std::u32string_view BGE{ U"bge" };
                constexpr const std::u32string_view BGEU{ U"bgeu" };
                constexpr const std::u32string_view BGT{ U"bgt" };
                constexpr const std::u32string_view BGTU{ U"bgtu" };
                constexpr const std::u32string_view BLE{ U"ble" };
                constexpr const std::u32string_view BLEU{ U"bleu" };
                constexpr const std::u32string_view BLT{ U"blt" };
                constexpr const std::u32string_view BLTU{ U"blru" };
                constexpr const std::u32string_view BNEZ{ U"bnez" };
                constexpr const std::u32string_view L_D{ U"l.d" };
                constexpr const std::u32string_view L_S{ U"l.s" };
                constexpr const std::u32string_view LA{ U"la" };
                constexpr const std::u32string_view LD{ U"ld" };
                constexpr const std::u32string_view LI{ U"li" };
                constexpr const std::u32string_view MFC1_D{ U"mfc1.d" };
                constexpr const std::u32string_view MOVE{ U"move" };
                constexpr const std::u32string_view MTC1_D{ U"mtc1.d" };
                constexpr const std::u32string_view MULO{ U"mulo" };
                constexpr const std::u32string_view MULOU{ U"mulou" };
                constexpr const std::u32string_view MULU{ U"mulu" };
                constexpr const std::u32string_view NEG{ U"neg" };
                constexpr const std::u32string_view NEGU{ U"negu" };
                constexpr const std::u32string_view NOT{ U"not" };
                constexpr const std::u32string_view REM{ U"rem" };
                constexpr const std::u32string_view REMU{ U"remu" };
                constexpr const std::u32string_view ROL{ U"rol" };
                constexpr const std::u32string_view ROR{ U"ror" };
                constexpr const std::u32string_view S_D{ U"s.d" };
                constexpr const std::u32string_view S_S{ U"s.s" };
                constexpr const std::u32string_view SD{ U"sd" };
                constexpr const std::u32string_view SEQ{ U"seq" };
                constexpr const std::u32string_view SGE{ U"sge" };
                constexpr const std::u32string_view SGEU{ U"sgeu" };
                constexpr const std::u32string_view SGT{ U"sgt" };
                constexpr const std::u32string_view SGTU{ U"sgtu" };
                constexpr const std::u32string_view SLE{ U"sle" };
                constexpr const std::u32string_view SLEU{ U"sleu" };
                constexpr const std::u32string_view SNE{ U"sne" };
                constexpr const std::u32string_view SUBI{ U"subi" };
                constexpr const std::u32string_view SUBIU{ U"subiu" };
                constexpr const std::u32string_view ULH{ U"ulh" };
                constexpr const std::u32string_view ULHU{ U"ulhu" };
                constexpr const std::u32string_view ULW{ U"ulw" };
                constexpr const std::u32string_view USH{ U"ush" };
                constexpr const std::u32string_view USW{ U"usw" };
            }
        }

        using namespace Directives;
        using namespace Instructions;
        using namespace PseudoInstructions;

        constexpr std::array<std::pair<const std::u32string_view, Directive>, 20> DIRECTIVES{ {
            {Directives::Literals::ALIGN, Directive::ALIGN}, {Directives::Literals::ASCII, Directive::ASCII}, { Directives::Literals::ASCIIZ, Directive::ASCIIZ},
            {Directives::Literals::BYTE, Directive::BYTE}, {Directives::Literals::DATA, Directive::DATA}, { Directives::Literals::DOUBLE, Directive::DOUBLE}, { Directives::Literals::END_MACRO, Directive::END_MACRO},
            {Directives::Literals::EQV, Directive::EQV},{ Directives::Literals::EXTERN, Directive::EQV}, { Directives::Literals::FLOAT, Directive::FLOAT}, {Directives::Literals::GLOBL, Directive::GLOBL},
            {Directives::Literals::HALF, Directive::HALF}, { Directives::Literals::INCLUDE, Directive::INCLUDE}, { Directives::Literals::KDATA, Directive::KDATA}, {Directives::Literals::KTEXT, Directive::KTEXT},
            {Directives::Literals::MACRO, Directive::MACRO}, {Directives::Literals::SET, Directive::SET},{ Directives::Literals::SPACE, Directive::SPACE}, {Directives::Literals::TEXT, Directive::TEXT},
            { Directives::Literals::WORD, Directive::WORD}}
        };

        constexpr std::array<std::pair<const std::u32string_view, Instruction>, 154> INSTRUCTIONS
        { {
            {Instructions::Literals::ABS_D, Instruction::ABS_D} , {Instructions::Literals::ABS_S, Instruction::ABS_S},
            {Instructions::Literals::ADD, Instruction::ADD}, {Instructions::Literals::ADD_D, Instruction::ADD_D}, {Instructions::Literals::ADD_S, Instruction::ADD_S}, {Instructions::Literals::ADDI, Instruction::ADDI},
            {Instructions::Literals::ADDIU, Instruction::ADDIU}, {Instructions::Literals::ADDU, Instruction::ADDU}, {Instructions::Literals::AND, Instruction::AND}, {Instructions::Literals::ANDI, Instruction::ANDI},
            {Instructions::Literals::BC1F, Instruction::BC1F}, {Instructions::Literals::BC1T, Instruction::BC1T},
            {Instructions::Literals::BEQ, Instruction::BEQ}, {Instructions::Literals::BGEZ, Instruction::BGEZ}, {Instructions::Literals::BGEZAL, Instruction::BGEZAL}, {Instructions::Literals::BGTZ, Instruction::BGTZ},
            {Instructions::Literals::BLEZ, Instruction::BLEZ}, {Instructions::Literals::BLTZ, Instruction::BLTZ}, {Instructions::Literals::BLTZAL, Instruction::BLTZAL}, {Instructions::Literals::BNE, Instruction::BNE},
            {Instructions::Literals::BREAK, Instruction::BREAK}, {Instructions::Literals::C_EQ_D, Instruction::C_EQ_D},
            {Instructions::Literals::C_EQ_S, Instruction::C_EQ_S}, {Instructions::Literals::C_LE_D, Instruction::C_LE_D},
            {Instructions::Literals::C_LE_S, Instruction::C_LE_S}, {Instructions::Literals::C_LT_D, Instruction::C_LT_D},
            {Instructions::Literals::C_LT_S, Instruction::C_LT_S}, { Instructions::Literals::CEIL_W_D,Instruction::CEIL_W_D},{Instructions::Literals::CEIL_W_S, Instruction::CEIL_W_S},
            {Instructions::Literals::CLO, Instruction::CLO}, {Instructions::Literals::CLZ, Instruction::CLZ}, {Instructions::Literals::CVT_D_S, Instruction::CVT_D_S}, { Instructions::Literals::CVT_D_W, Instruction::CVT_D_W},
            {Instructions::Literals::CVT_S_D, Instruction::CVT_S_D}, { Instructions::Literals::CVT_S_W, Instruction::CVT_S_W}, {Instructions::Literals::CVT_W_D, Instruction::CVT_W_D}, {Instructions::Literals::CVT_W_S, Instruction::CVT_W_S},
            {Instructions::Literals::DIV, Instruction::DIV}, {Instructions::Literals::DIV_D, Instruction::DIV_D}, {Instructions::Literals::DIV_S, Instruction::DIV_S}, { Instructions::Literals::DIVU, Instruction::DIVU},
            {Instructions::Literals::ERET, Instruction::ERET}, {Instructions::Literals::FLOOR_W_D, Instruction::FLOOR_W_D}, {Instructions::Literals::FLOOR_W_S, Instruction::FLOOR_W_S}, {Instructions::Literals::J, Instruction::J},
            {Instructions::Literals::JAL, Instruction::JAL}, {Instructions::Literals::JALR_RA, Instruction::JALR_RA}, {Instructions::Literals::JALR, Instruction::JALR}, {Instructions::Literals::JR, Instruction::JR},
            {Instructions::Literals::LB, Instruction::LB}, {Instructions::Literals::LBU, Instruction::LBU}, {Instructions::Literals::LDC1, Instruction::LDC1}, {Instructions::Literals::LH, Instruction::LH}, {Instructions::Literals::LHU, Instruction::LHU},
            {Instructions::Literals::LL, Instruction::LL}, {Instructions::Literals::LUI, Instruction::LUI}, {Instructions::Literals::LW, Instruction::LW}, {Instructions::Literals::LWC1, Instruction::LWC1},
            {Instructions::Literals::LWL, Instruction::LWL}, {Instructions::Literals::LWR, Instruction::LWR}, {Instructions::Literals::MADD, Instruction::MADD}, {Instructions::Literals::MADDU, Instruction::MADDU}, {Instructions::Literals::MFC0, Instruction::MFC0},
            {Instructions::Literals::MFC1, Instruction::MFC1}, {Instructions::Literals::MFHI, Instruction::MFHI},{ Instructions::Literals::MFLO, Instruction::MFLO}, {Instructions::Literals::MOV_D, Instruction::MOV_D}, {Instructions::Literals::MOV_S, Instruction::MOV_S},
            {Instructions::Literals::MOVF, Instruction::MOVF}, {Instructions::Literals::MOVF_D, Instruction::MOVF_D},
            {Instructions::Literals::MOVF_S, Instruction::MOVF_S}, {Instructions::Literals::MOVN, Instruction::MOVN}, {Instructions::Literals::MOVN_D, Instruction::MOVN_D},
            {Instructions::Literals::MOVN_S, Instruction::MOVN_S}, {Instructions::Literals::MOVT, Instruction::MOVT}, {Instructions::Literals::MOVT_D, Instruction::MOVT_D},
            {Instructions::Literals::MOVT_S, Instruction::MOVT_S}, {Instructions::Literals::MOVZ, Instruction::MOVZ}, { Instructions::Literals::MOVZ_D, Instruction::MOVZ_D}, { Instructions::Literals::MOVZ_S, Instruction::MOVZ_S},
            {Instructions::Literals::MSUB, Instruction::MSUB}, { Instructions::Literals::MSUBU, Instruction::MSUBU}, {Instructions::Literals::MTC0, Instruction::MTC0},{ Instructions::Literals::MTC1, Instruction::MTC1},
            {Instructions::Literals::MTHI, Instruction::MTHI}, {Instructions::Literals::MTLO, Instruction::MTLO}, {Instructions::Literals::MUL, Instruction::MUL}, {Instructions::Literals::MUL_D, Instruction::MUL_D},
            {Instructions::Literals::MUL_S, Instruction::MUL_S}, {Instructions::Literals::MULT, Instruction::MULT}, {Instructions::Literals::MULTU, Instruction::MULTU}, {Instructions::Literals::NEG_D, Instruction::NEG_D}, {Instructions::Literals::NEG_S, Instruction::NEG_S},
            {Instructions::Literals::NOP, Instruction::NOP}, {Instructions::Literals::NOR, Instruction::NOR}, {Instructions::Literals::OR, Instruction::OR},
            {Instructions::Literals::ORI, Instruction::ORI}, {Instructions::Literals::ROUND_W_D, Instruction::ROUND_W_D}, { Instructions::Literals::ROUND_W_S, Instruction::ROUND_W_S}, { Instructions::Literals::SB, Instruction::SB},
            {Instructions::Literals::SC, Instruction::SC}, {Instructions::Literals::SDC1, Instruction::SDC1}, { Instructions::Literals::SH, Instruction::SH},
            {Instructions::Literals::SLL, Instruction::SLL}, { Instructions::Literals::SLLV, Instruction::SLLV}, { Instructions::Literals::SLT, Instruction::SLT}, { Instructions::Literals::SLTI, Instruction::SLTI},
            {Instructions::Literals::SLTIU, Instruction::SLTIU}, { Instructions::Literals::SLTU, Instruction::SLTU}, { Instructions::Literals::SQRT_D, Instruction::SQRT_D},
            {Instructions::Literals::SQRT_S, Instruction::SQRT_S}, { Instructions::Literals::SRA, Instruction::SRA}, {Instructions::Literals::SRAV, Instruction::SRAV}, {Instructions::Literals::SRL, Instruction::SRL},
            {Instructions::Literals::SRLV, Instruction::SRLV}, {Instructions::Literals::SUB, Instruction::SUB}, {Instructions::Literals::SUB_D, Instruction::SUB_D},
            {Instructions::Literals::SUB_S, Instruction::SUB_S},{ Instructions::Literals::SUBU, Instruction::SUBU}, { Instructions::Literals::SW, Instruction::SW}, { Instructions::Literals::SWC1, Instruction::SWC1},
            {Instructions::Literals::SWL, Instruction::SWL}, { Instructions::Literals::SWR, Instruction::SWR}, { Instructions::Literals::SYSCALL, Instruction::SYSCALL},
            {Instructions::Literals::TEQ, Instruction::TEQ}, { Instructions::Literals::TEQI, Instruction::TEQI}, { Instructions::Literals::TGE, Instruction::TGE}, { Instructions::Literals::TGEI, Instruction::TGEI},
            {Instructions::Literals::TGEIU, Instruction::TGEIU}, { Instructions::Literals::TGEU, Instruction::TGEU}, { Instructions::Literals::TLT, Instruction::TLT},
            {Instructions::Literals::TLTU, Instruction::TLTU}, { Instructions::Literals::TNE, Instruction::TNE}, { Instructions::Literals::TNEI, Instruction::TNEI}, { Instructions::Literals::TRUNC_W_D, Instruction::TRUNC_W_D},
            {Instructions::Literals::TRUNC_W_S, Instruction::TRUNC_W_S}, { Instructions::Literals::XOR, Instruction::XOR}, { Instructions::Literals::XORI, Instruction::XORI} }
        };

        constexpr std::array<std::pair<const std::u32string_view, Pseudoinstruction>, 47> PSEUDOINSTRUCTIONS
        { {
            {PseudoInstructions::Literals::B, Pseudoinstruction::B}, {PseudoInstructions::Literals::BEQ, Pseudoinstruction::BEQ}, {PseudoInstructions::Literals::BEQZ, Pseudoinstruction::BEQZ},
            {PseudoInstructions::Literals::BGE, Pseudoinstruction::BGE}, {PseudoInstructions::Literals::BGEU, Pseudoinstruction::BGEU}, {PseudoInstructions::Literals::BGT, Pseudoinstruction::BGT},
            {PseudoInstructions::Literals::BGTU, Pseudoinstruction::BGTU}, {PseudoInstructions::Literals::BLE, Pseudoinstruction::BLE}, {PseudoInstructions::Literals::BLEU, Pseudoinstruction::BLEU},
            {PseudoInstructions::Literals::BLTU, Pseudoinstruction::BLTU}, {PseudoInstructions::Literals::BNEZ, Pseudoinstruction::BNEZ}, {PseudoInstructions::Literals::LA, Pseudoinstruction::LA},
            {PseudoInstructions::Literals::LD, Pseudoinstruction::LD}, {PseudoInstructions::Literals::LI, Pseudoinstruction::LI}, {PseudoInstructions::Literals::L_D, Pseudoinstruction::L_D},
            {PseudoInstructions::Literals::L_S, Pseudoinstruction::L_S}, {PseudoInstructions::Literals::MFC1_D, Pseudoinstruction::MFC1_D}, {PseudoInstructions::Literals::MOVE, Pseudoinstruction::MOVE},
            {PseudoInstructions::Literals::MTC1_D, Pseudoinstruction::MTC1_D}, {PseudoInstructions::Literals::MULO, Pseudoinstruction::MULO}, {PseudoInstructions::Literals::MULOU, Pseudoinstruction::MULOU},
            {PseudoInstructions::Literals::NEG, Pseudoinstruction::NEG}, {PseudoInstructions::Literals::NEGU, Pseudoinstruction::NEGU}, {PseudoInstructions::Literals::NOT, Pseudoinstruction::NOT},
            {PseudoInstructions::Literals::REM, Pseudoinstruction::REM}, {PseudoInstructions::Literals::REMU, Pseudoinstruction::REMU}, {PseudoInstructions::Literals::ROL, Pseudoinstruction::ROL},
            {PseudoInstructions::Literals::ROR, Pseudoinstruction::ROR}, {PseudoInstructions::Literals::SD, Pseudoinstruction::SD}, {PseudoInstructions::Literals::SEQ, Pseudoinstruction::SEQ},
            {PseudoInstructions::Literals::SGE, Pseudoinstruction::SGE}, {PseudoInstructions::Literals::SGEU, Pseudoinstruction::SGEU}, {PseudoInstructions::Literals::SGT, Pseudoinstruction::SGTU},
            {PseudoInstructions::Literals::SGTU, Pseudoinstruction::SGTU}, {PseudoInstructions::Literals::SLE, Pseudoinstruction::SLE}, {PseudoInstructions::Literals::SLEU, Pseudoinstruction::SLEU},
            {PseudoInstructions::Literals::SNE, Pseudoinstruction::SNE}, {PseudoInstructions::Literals::SUBI, Pseudoinstruction::SUBI}, {PseudoInstructions::Literals::SUBIU, Pseudoinstruction::SUBIU},
            {PseudoInstructions::Literals::S_D, Pseudoinstruction::S_D}, {PseudoInstructions::Literals::S_S, Pseudoinstruction::S_S}, {PseudoInstructions::Literals::ULH, Pseudoinstruction::ULH},
            {PseudoInstructions::Literals::ULHU, Pseudoinstruction::ULHU}, {PseudoInstructions::Literals::ULW, Pseudoinstruction::ULW}, {PseudoInstructions::Literals::USH, Pseudoinstruction::USH},
            {PseudoInstructions::Literals::USW, Pseudoinstruction::USW}

        } };
    }
}