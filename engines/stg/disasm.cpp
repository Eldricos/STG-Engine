/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/stg/disasm.h"

#include "engines/stg/opcodes.h"
#include "engines/stg/vm.h"

namespace STG {

Common::String disassembleInstruction(const std::vector<uint16> &code, uint32 ip) {
	auto word = [&](uint32 i) -> uint16 { return i < code.size() ? code[i] : 0; };
	uint16 op = word(ip);
	const OpcodeInfo *info = getOpcodeInfo(op);
	if (!info)
		return Common::format("??? %04x", op);

	Common::String text = info->name;
	switch (op) {
	case OP_PUSH:
		text += Common::format(" %d", (int16)word(ip + 1));
		break;
	case OP_PUSH32:
		text += Common::format(" 0x%04x%04x", word(ip + 2), word(ip + 1));
		break;
	case OP_PUSH_BLOCK:
		for (int i = 1; i < 8; i++)
			text += Common::format(" %04x", word(ip + i));
		break;
	default:
		break;
	}
	return text;
}

Common::String disassembleScript(const ScriptFile &script) {
	const std::vector<uint16> &code = script.getCode();
	Common::String out;
	const std::vector<ScriptObject> &objects = script.getObjects();

	for (size_t o = 0; o < objects.size(); o++) {
		const ScriptObject &object = objects[o];
		out += Common::format("object %u: type %u, id 0x%04x\n", (uint)o, object.type, object.id);
		for (const ScriptState &state : object.states) {
			out += Common::format("  state %u\n", state.id);
			for (const ScriptBlock &block : state.blocks) {
				const char *eventName = block.event == kEventEnterState ? " (enter)"
				                        : block.event == kEventLeaveState ? " (leave)" : "";
				out += Common::format("    on event %u%s", block.event, eventName);
				if (block.nextState != -1)
					out += Common::format(", then state %d", block.nextState);
				out += "\n";

				int depth = 3;
				for (uint32 ip = block.codeStart; ip < code.size() && code[ip] != OP_END;
				     ip += getInstructionLength(code[ip])) {
					uint16 op = code[ip];
					if (op == OP_ENDIF || op == OP_ELSE)
						depth--;
					const OpcodeInfo *info = getOpcodeInfo(op);
					Common::String line = Common::String(depth * 2, ' ') + disassembleInstruction(code, ip);
					if (info && op >= OP_FIRST_NATIVE && info->note && *info->note)
						line += Common::String(std::max<int>(1, 40 - (int)line.size()), ' ') + "; " + info->note;
					out += Common::format("%5u %s\n", ip, line.c_str());
					if ((op >= OP_IF_EQ && op <= OP_IF_LE) || op == OP_ELSE)
						depth++;
				}
			}
		}
	}
	return out;
}

} // End of namespace STG
