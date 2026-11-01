#include "ula.hpp"
#include <limits>
#include <iostream>

void ULA::calculate() {
    // Reset de flags e resultado antes de cada cálculo
    overflow = false;
    result = 0;

    // Lambda para conversão segura de uint16_t para int16_t (signed)
    auto as_i16 = [](uint16_t x) -> int16_t {
        return static_cast<int16_t>(x);
    };

    // Lambda para checagem de overflow (verifica se cabe no intervalo [-32768, 32767])
    auto fits_in_i16 = [](int32_t x) -> bool {
        return (x <= std::numeric_limits<int16_t>::max()) &&
               (x >= std::numeric_limits<int16_t>::min());
    };

    switch (op) {
        case ADD: {
            // Soma com cálculo intermediário em 32 bits para detecção de overflow
            int32_t res32 = static_cast<int32_t>(as_i16(A)) + static_cast<int32_t>(as_i16(B));
            if (!fits_in_i16(res32)) {
                overflow = true;
            }
            result = static_cast<int16_t>(static_cast<uint16_t>(res32 & 0xFFFF));
            break;
        }

        case SUB: {
            // Subtração (A - B) com detecção de overflow
            int32_t res32 = static_cast<int32_t>(as_i16(A)) - static_cast<int32_t>(as_i16(B));
            if (!fits_in_i16(res32)) {
                overflow = true;
            }
            result = static_cast<int16_t>(static_cast<uint16_t>(res32 & 0xFFFF));
            break;
        }

        case AND_OP: {
            // Operação lógica bit a bit AND (valores unsigned)
            result = static_cast<int16_t>(A & B);
            break;
        }

        case OR_OP: {
            // Operação lógica bit a bit OR (valores unsigned)
            result = static_cast<int16_t>(A | B);
            break;
        }

        default: {
            overflow = true;
            result = 0;
            break;
        }
    }
}

void ULA::execute(operation ULA_operacao, uint16_t a, uint16_t b, uint16_t /*shamt*/) {
    op = ULA_operacao;
    A = a;
    B = b;
    calculate();
}

bool ULA::executeOpcode(uint16_t opcode, uint16_t a, uint16_t b) {
    switch (opcode) {
        case OP_ADD:
            execute(ADD, a, b);
            return true;
        case OP_SUB:
            execute(SUB, a, b);
            return true;
        case OP_AND:
            execute(AND_OP, a, b);
            return true;
        case OP_OR:
            execute(OR_OP, a, b);
            return true;
        default:
            return false;
    }
}
