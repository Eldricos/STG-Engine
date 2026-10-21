/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STG_VM_H
#define STG_VM_H

#include <map>
#include <set>
#include <vector>

#include "engines/stg/script.h"

namespace STG {

class ScriptVM;

/** Events the interpreter itself sends. */
enum ScriptEvent {
	kEventEnterState = 2,
	kEventLeaveState = 3
};

/** A running script object: its definition plus mutable state. */
struct ScriptInstance {
	const ScriptObject *object = nullptr;
	int16 state = -1;
	/** Cleared by DISABLE_OBJECT; a disabled object ignores events. */
	bool enabled = true;
	/** Set by GOTO_STATE and CASE; applied when the current block ends. */
	int16 pendingState = -1;
	std::vector<int16> locals;
	/** Handlers missing here are looked up in the parent (this+0x48 in the original). */
	ScriptInstance *parent = nullptr;
	/** (event << 8 | state) -> word index of the block's code. */
	std::map<uint16, const ScriptBlock *> handlers;
};

/**
 * The game side of the interpreter: everything the scripts do to the world
 * goes through here. Opcodes at or above OP_FIRST_NATIVE, plus
 * SEND_SIGNAL and GAME_INIT, are forwarded to callNative().
 */
class ScriptHost {
public:
	virtual ~ScriptHost() {}

	/**
	 * Execute a game opcode. Pop arguments with vm.pop() and push results
	 * with vm.push(), following the opcode's stack effect.
	 */
	virtual void callNative(ScriptVM &vm, ScriptInstance &self, uint16 op) = 0;

	/** Called when no script handles an event; the original falls back to built-in behaviour. */
	virtual void unhandledEvent(ScriptVM &vm, ScriptInstance &self, uint16 event) {}
};

/**
 * Interpreter for the SGEN bytecode, reimplementing the one at 0x44F9D4
 * in sgens.exe: a 16-bit stack machine with per-object locals, shared
 * globals, structured if/else and event driven state machines.
 */
class ScriptVM {
public:
	static const int kGlobalCount = 32768;
	static const int kMaxStack = 256;

	/** host may be null, in which case game opcodes only balance the stack. */
	ScriptVM(const ScriptFile &script, ScriptHost *host);

	/** Create an instance for every object, all in state -1, globals cleared. */
	void reset();

	int getInstanceCount() const { return (int)_instances.size(); }
	ScriptInstance &getInstance(int index) { return _instances[index]; }
	/** Index of the instance with the given object id, or -1. */
	int findInstance(uint16 id) const;

	/**
	 * Deliver an event to an object, running the matching block. Returns the
	 * block's result (SET_RESULT), or 0 if nothing handled the event.
	 */
	int16 sendEvent(ScriptInstance &self, uint16 event, const std::vector<int16> &args = {});

	/** Change state, sending leave and enter events as the original does. */
	void setState(ScriptInstance &self, int16 state);

	int16 getGlobal(int index) const;
	void setGlobal(int index, int16 value);

	/** Stack access for ScriptHost::callNative(). */
	void push(int16 value);
	int16 pop();

	/** Opcodes that reached the stub host, for tracking what's left to implement. */
	const std::set<uint16> &getStubbedOpcodes() const { return _stubbed; }
	uint64 getInstructionCount() const { return _instructionCount; }

private:
	int16 execute(ScriptInstance &self, uint32 ip, const std::vector<int16> &args);
	/** Skip to the matching else/endif, as the original's method 0x22C does. */
	uint32 skipConditional(uint32 ip) const;
	void callNative(ScriptInstance &self, uint16 op);
	uint16 word(uint32 ip) const;

	const ScriptFile &_script;
	ScriptHost *_host;
	std::vector<ScriptInstance> _instances;
	std::vector<int16> _globals;
	std::vector<int16> _stack;
	std::set<uint16> _stubbed;
	uint64 _instructionCount = 0;
	int _depth = 0;
};

} // End of namespace STG

#endif
