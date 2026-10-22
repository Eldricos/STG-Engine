/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "test/test.h"

#include "common/endian.h"
#include "common/stream.h"
#include "engines/stg/disasm.h"
#include "engines/stg/opcodes.h"
#include "engines/stg/script.h"
#include "engines/stg/vm.h"

namespace {

/** Wrap program words in an SGEN file with empty sections and no resources. */
std::vector<byte> makeScriptFile(const std::vector<uint16> &program) {
	Common::MemoryWriteStreamDynamic file;
	file.writeUint32BE(MKTAG('S', 'G', 'E', 'N'));
	file.writeUint16LE(22);
	file.writeUint16LE(2);
	file.writeUint32LE(0); // file size - 12, patched below
	file.writeUint32LE((uint32)program.size() * 2);
	for (int i = 0; i < 6; i++)
		file.writeUint32LE(0);
	for (uint16 word : program)
		file.writeUint16LE(word);
	std::vector<byte> bytes = file.getData();
	WRITE_LE_UINT32(&bytes[8], (uint32)bytes.size() - 12);
	return bytes;
}

bool loadProgram(STG::ScriptFile &script, const std::vector<uint16> &program) {
	std::vector<byte> bytes = makeScriptFile(program);
	Common::MemoryReadStream stream(bytes.data(), (uint32)bytes.size());
	return script.load(stream);
}

using namespace STG;

const uint16 P = OP_PUSH;

/** Records native calls; NATIVE_68 pops two values and pushes their sum. */
class TestHost : public ScriptHost {
public:
	void callNative(ScriptVM &vm, ScriptInstance &self, uint16 op) override {
		calls.push_back(op);
		if (op == 0x68) {
			int16 a = vm.pop();
			int16 b = vm.pop();
			vm.push((int16)(a + b));
		}
	}
	void unhandledEvent(ScriptVM &vm, ScriptInstance &self, uint16 event, const std::vector<int16> &args) override {
		unhandled.push_back(event);
	}
	std::vector<uint16> calls;
	std::vector<uint16> unhandled;
};

} // End of anonymous namespace

TEST(script_program_structure) {
	// One object (type 1, id 7) with two states; state 0 has two blocks.
	std::vector<uint16> program = {
		1, 7,
		0x8000, 2, 0xFFFF, P, 5, OP_END, 9, 1, OP_PUSH32, 1, 2, OP_END, 0x3000,
		0x8001, 2, 0xFFFF, OP_END, 0x3000,
		0x2000,
		0x1000
	};
	ScriptFile script;
	CHECK(loadProgram(script, program));
	CHECK_EQ(script.getObjects().size(), (size_t)1);
	const ScriptObject &object = script.getObjects()[0];
	CHECK_EQ(object.type, 1);
	CHECK_EQ(object.id, 7);
	CHECK_EQ(object.states.size(), (size_t)2);
	CHECK_EQ(object.states[0].blocks.size(), (size_t)2);
	CHECK_EQ(object.states[0].blocks[1].event, 9);
	CHECK_EQ(object.states[0].blocks[1].nextState, 1);
	CHECK_EQ(object.states[1].id, 1);

	Common::String listing = disassembleScript(script);
	CHECK(listing.find("PUSH32 0x00020001") != Common::String::npos);
	CHECK(listing.find("on event 9, then state 1") != Common::String::npos);

	// Missing terminators are rejected.
	program.pop_back();
	CHECK(!loadProgram(script, program));
}

TEST(script_vm_arithmetic_order) {
	// The original computes "top op below": push 10, push 3 gives 3-10, 3/10...
	std::vector<uint16> program = {
		1, 1, 0x8000,
		10, 0xFFFF, P, 10, P, 3, OP_SUB, OP_SET_RESULT, OP_END,
		11, 0xFFFF, P, 3, P, 12, OP_DIV, OP_SET_RESULT, OP_END,
		12, 0xFFFF, P, 5, P, 9, OP_SIGN_DIFF, OP_SET_RESULT, OP_END,
		13, 0xFFFF, P, 0x7FFF, OP_INC, OP_HIGH_BITS, OP_SET_RESULT, OP_END,
		14, 0xFFFF, P, 0x3456, P, 0x00FF, OP_AND, P, 1, OP_ADD, OP_NOT, OP_SET_RESULT, OP_END,
		0x3000, 0x2000, 0x1000
	};
	ScriptFile script;
	CHECK(loadProgram(script, program));
	ScriptVM vm(script, nullptr);
	ScriptInstance &obj = vm.getInstance(0);
	obj.state = 0;
	CHECK_EQ(vm.sendEvent(obj, 10), -7);
	CHECK_EQ(vm.sendEvent(obj, 11), 4);
	CHECK_EQ(vm.sendEvent(obj, 12), 1);
	CHECK_EQ(vm.sendEvent(obj, 13), (int16)0x8000);
	CHECK_EQ(vm.sendEvent(obj, 14), (int16)~0x57);
}

TEST(script_vm_conditionals) {
	// result = 1; if (arg == 5) { if (arg > 9) result = 2 else result = 3 } else result = 4
	std::vector<uint16> program = {
		1, 1, 0x8000,
		10, 0xFFFF,
		P, 1, OP_SET_RESULT,
		P, 5, OP_IF_EQ,
		  P, 9, OP_IF_GT,
		    P, 2, OP_SET_RESULT,
		  OP_ELSE,
		    P, 3, OP_SET_RESULT,
		  OP_ENDIF,
		OP_ELSE,
		  P, 4, OP_SET_RESULT,
		OP_ENDIF,
		OP_END,
		0x3000, 0x2000, 0x1000
	};
	ScriptFile script;
	CHECK(loadProgram(script, program));
	ScriptVM vm(script, nullptr);
	ScriptInstance &obj = vm.getInstance(0);
	obj.state = 0;
	CHECK_EQ(vm.sendEvent(obj, 10, { 5 }), 3); // 5 == 5, but not > 9
	CHECK_EQ(vm.sendEvent(obj, 10, { 6 }), 4);
}

TEST(script_vm_states_and_events) {
	// State 0: on enter set global 1 = 10; on event 5 go to state 1 via
	// CASE when the argument is 7. State 1: on leave set global 2, on enter
	// count in global 3. Event 6 uses GOTO_STATE.
	std::vector<uint16> program = {
		1, 42,
		0x8000,
		  2, 0xFFFF, P, 10, P, 1, OP_SET_GLOBAL, OP_END,
		  5, 0xFFFF, P, 1, P, 7, OP_CASE, P, 99, P, 4, OP_SET_GLOBAL, OP_END,
		  3, 0xFFFF, P, 1, P, 2, OP_SET_GLOBAL, OP_END,
		0x3000,
		0x8001,
		  2, 0xFFFF, P, 3, OP_GET_GLOBAL, OP_INC, P, 3, OP_SET_GLOBAL, OP_END,
		  6, 0xFFFF, P, 0, OP_GOTO_STATE, OP_PUSH_SELF, OP_SET_RESULT, OP_END,
		0x3000,
		0x2000, 0x1000
	};
	ScriptFile script;
	CHECK(loadProgram(script, program));
	TestHost host;
	ScriptVM vm(script, &host);
	CHECK_EQ(vm.findInstance(42), 0);
	ScriptInstance &obj = vm.getInstance(0);

	vm.setState(obj, 0);
	CHECK_EQ(vm.getGlobal(1), 10);

	vm.sendEvent(obj, 5, { 3 }); // not 7: falls through to global 4 = 99
	CHECK_EQ(obj.state, 0);
	CHECK_EQ(vm.getGlobal(4), 99);

	vm.sendEvent(obj, 5, { 7 }); // CASE matches: leaves state 0, enters state 1
	CHECK_EQ(obj.state, 1);
	CHECK_EQ(vm.getGlobal(2), 1);
	CHECK_EQ(vm.getGlobal(3), 1);

	CHECK_EQ(vm.sendEvent(obj, 6), 42); // PUSH_SELF; then back to state 0
	CHECK_EQ(obj.state, 0);

	// Leaving "no state" at the very start, and leaving state 1 (which has no
	// leave handler), both fall through to the host, as in the original.
	CHECK_EQ(host.unhandled.size(), (size_t)2);
	CHECK_EQ(host.unhandled[0], (uint16)kEventLeaveState);
	CHECK_EQ(host.unhandled[1], (uint16)kEventLeaveState);
	vm.sendEvent(obj, 77); // no handler
	CHECK_EQ(host.unhandled.back(), 77);
}

TEST(script_vm_locals_args_natives_and_parents) {
	// Event 9 stores its two arguments in locals and returns local0 - local1,
	// then calls NATIVE_68 (a + b in the test host). The child object has no
	// handler of its own and inherits the parent's.
	std::vector<uint16> program = {
		1, 1, 0x8000,
		  9, 0xFFFF,
		    P, 2, OP_ALLOC_LOCALS,
		    P, 0, OP_SET_LOCAL, P, 1, OP_SET_LOCAL,
		    P, 1, OP_GET_LOCAL, P, 0, OP_GET_LOCAL, OP_SUB, OP_SET_RESULT,
		    P, 100, P, 20, 0x68, OP_SET_RESULT,
		    OP_END,
		0x3000, 0x2000,
		1, 2, 0x8000, 2, 0xFFFF, OP_END, 0x3000, 0x2000,
		0x1000
	};
	ScriptFile script;
	CHECK(loadProgram(script, program));
	TestHost host;
	ScriptVM vm(script, &host);
	ScriptInstance &parent = vm.getInstance(0);
	ScriptInstance &child = vm.getInstance(1);
	parent.state = 0;
	child.state = 0;
	child.parent = &parent;

	// First argument ends up on top: local0 = 50, local1 = 8.
	CHECK_EQ(vm.sendEvent(parent, 9, { 50, 8 }), 120);
	CHECK_EQ(parent.locals.size(), (size_t)2);
	CHECK_EQ(parent.locals[0], 50);
	CHECK_EQ(parent.locals[1], 8);
	CHECK_EQ(host.calls.size(), (size_t)1);

	// Inherited handler runs with the child's own locals.
	vm.sendEvent(child, 9, { 1, 2 });
	CHECK_EQ(child.locals.size(), (size_t)2);
	CHECK_EQ(child.locals[0], 1);
}

TEST(script_opcode_table) {
	CHECK(getOpcodeInfo(OP_PUSH) != nullptr);
	CHECK_EQ(getInstructionLength(OP_PUSH_BLOCK), 8);
	int implemented = 0;
	for (uint16 op = 0; op < kOpcodeCount; op++)
		implemented += getOpcodeInfo(op) != nullptr;
	CHECK_EQ(implemented, 243); // every case in the original's switch
	CHECK(getOpcodeInfo(kOpcodeCount) == nullptr);
}
