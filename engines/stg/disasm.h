/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STG_DISASM_H
#define STG_DISASM_H

#include "common/str.h"
#include "engines/stg/script.h"

namespace STG {

/** One instruction as text, e.g. "PUSH 32" or "NATIVE_35". */
Common::String disassembleInstruction(const std::vector<uint16> &code, uint32 ip);

/**
 * The whole program as an indented listing: objects, states, event
 * blocks and their instructions, with if/else nesting shown.
 */
Common::String disassembleScript(const ScriptFile &script);

} // End of namespace STG

#endif
