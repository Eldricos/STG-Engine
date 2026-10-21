/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/stg/vm.h"

#include "common/debug.h"
#include "engines/stg/opcodes.h"

namespace STG {

namespace {

/** Handler key, as built by the original's registration method (0x455F28). */
uint16 handlerKey(uint16 event, int16 state) {
	return (uint16)(((event & 0xFF) << 8) | ((uint16)state & 0xFF));
}

/** Guards against runaway scripts; real blocks are a few hundred instructions at most. */
const uint32 kMaxInstructionsPerBlock = 100000;
/** Guards against events that keep triggering state changes. */
const int kMaxEventDepth = 64;

} // End of anonymous namespace

ScriptVM::ScriptVM(const ScriptFile &script, ScriptHost *host) : _script(script), _host(host) {
	reset();
}

void ScriptVM::reset() {
	_instances.clear();
	_instances.resize(_script.getObjects().size());
	for (size_t i = 0; i < _instances.size(); i++) {
		ScriptInstance &instance = _instances[i];
		instance.object = &_script.getObjects()[i];
		for (const ScriptState &state : instance.object->states) {
			for (const ScriptBlock &block : state.blocks)
				instance.handlers[handlerKey(block.event, state.id)] = &block;
		}
	}
	_globals.assign(kGlobalCount, 0);
	_stack.clear();
}

int ScriptVM::findInstance(uint16 id) const {
	for (size_t i = 0; i < _instances.size(); i++) {
		if (_instances[i].object->id == id)
			return (int)i;
	}
	return -1;
}

int16 ScriptVM::getGlobal(int index) const {
	return index >= 0 && index < kGlobalCount ? _globals[index] : 0;
}

void ScriptVM::setGlobal(int index, int16 value) {
	if (index >= 0 && index < kGlobalCount)
		_globals[index] = value;
	else
		warning("Script: global %d out of range", index);
}

void ScriptVM::push(int16 value) {
	if (_stack.size() >= kMaxStack) {
		warning("Script: stack overflow");
		return;
	}
	_stack.push_back(value);
}

int16 ScriptVM::pop() {
	if (_stack.empty()) {
		warning("Script: stack underflow");
		return 0;
	}
	int16 value = _stack.back();
	_stack.pop_back();
	return value;
}

uint16 ScriptVM::word(uint32 ip) const {
	const std::vector<uint16> &code = _script.getCode();
	return ip < code.size() ? code[ip] : (uint16)OP_END;
}

int16 ScriptVM::sendEvent(ScriptInstance &self, uint16 event, const std::vector<int16> &args) {
	if (!self.enabled)
		return 0;
	if (_depth >= kMaxEventDepth) {
		warning("Script: event %u to object %u nested too deeply", event, self.object->id);
		return 0;
	}

	if (self.state != -1) {
		for (ScriptInstance *owner = &self; owner; owner = owner->parent) {
			auto it = owner->handlers.find(handlerKey(event, self.state));
			if (it == owner->handlers.end())
				continue;

			const ScriptBlock *block = it->second;
			_depth++;
			int16 result = execute(self, block->codeStart, args);
			if (self.pendingState != -1) {
				int16 state = self.pendingState;
				self.pendingState = -1;
				setState(self, state);
			} else if (block->nextState != -1) {
				setState(self, block->nextState);
			}
			_depth--;
			return result;
		}
	}

	if (_host)
		_host->unhandledEvent(*this, self, event);
	return 0;
}

void ScriptVM::setState(ScriptInstance &self, int16 state) {
	if (self.state == state)
		return;
	sendEvent(self, kEventLeaveState);
	self.state = state;
	sendEvent(self, kEventEnterState);
}

uint32 ScriptVM::skipConditional(uint32 ip) const {
	int depth = 1;
	while (true) {
		uint16 op = word(ip);
		if (op >= OP_IF_EQ && op <= OP_IF_LE) {
			depth++;
			ip++;
		} else if (op == OP_ENDIF) {
			ip++;
			if (--depth <= 0)
				return ip;
		} else if (op == OP_ELSE) {
			ip++;
			if (depth <= 1)
				return ip;
		} else if (op == OP_END) {
			return ip;
		} else {
			ip += getInstructionLength(op);
		}
	}
}

void ScriptVM::callNative(ScriptInstance &self, uint16 op) {
	if (_host) {
		_host->callNative(*this, self, op);
		return;
	}

	// No host: keep the stack balanced so execution can continue.
	const OpcodeInfo *info = getOpcodeInfo(op);
	if (_stubbed.insert(op).second)
		debugC(1, "script", "Script: opcode %02x (%s) not implemented", op, info ? info->name : "?");
	if (info) {
		for (int i = 0; i < info->pops; i++)
			pop();
		for (int i = 0; i < info->pushes; i++)
			push(0);
	}
}

int16 ScriptVM::execute(ScriptInstance &self, uint32 ip, const std::vector<int16> &args) {
	// Each block runs on a fresh stack, with the event arguments pushed last
	// to first so the first argument is on top.
	std::vector<int16> savedStack;
	savedStack.swap(_stack);
	for (size_t i = args.size(); i-- > 0;)
		push(args[i]);

	int16 result = 0;
	uint32 executed = 0;
	bool running = true;

	while (running) {
		if (++executed > kMaxInstructionsPerBlock) {
			warning("Script: object %u ran too long", self.object->id);
			break;
		}
		_instructionCount++;

		uint16 op = word(ip++);
		switch (op) {
		case OP_END:
			running = false;
			break;
		case OP_NOP:
		case OP_ENDIF:
			break;
		case OP_PUSH:
			push((int16)word(ip++));
			break;
		case OP_POP:
			pop();
			break;
		case OP_PUSH_SELF:
			push((int16)self.object->id);
			break;
		case OP_PUSH_BLOCK:
			for (int i = 0; i < 7; i++)
				push((int16)word(ip++));
			break;
		case OP_PUSH32:
			push((int16)word(ip++));
			push((int16)word(ip++));
			break;
		case OP_SET_RESULT:
			result = pop();
			break;
		case OP_GOTO_STATE:
			self.pendingState = pop();
			break;
		case OP_CASE: {
			int16 c = pop();
			int16 b = pop();
			int16 a = pop();
			push(a);
			if (a == c) {
				self.pendingState = b;
				running = false;
			}
			break;
		}
		case OP_IF_EQ:
		case OP_IF_NE:
		case OP_IF_GT:
		case OP_IF_GE:
		case OP_IF_LT:
		case OP_IF_LE: {
			int16 b = pop();
			int16 a = pop();
			push(a);
			bool pass = op == OP_IF_EQ ? a == b : op == OP_IF_NE ? a != b : op == OP_IF_GT ? a > b
			          : op == OP_IF_GE ? a >= b : op == OP_IF_LT ? a < b : a <= b;
			if (!pass)
				ip = skipConditional(ip);
			break;
		}
		case OP_ELSE:
			ip = skipConditional(ip);
			break;
		case OP_RETURN:
			running = false;
			break;
		case OP_ALLOC_LOCALS: {
			int16 count = pop();
			if (count > 0 && (size_t)count > self.locals.size())
				self.locals.resize(count, 0);
			break;
		}
		case OP_GET_LOCAL: {
			int16 index = pop();
			push(index >= 0 && (size_t)index < self.locals.size() ? self.locals[index] : 0);
			break;
		}
		case OP_SET_LOCAL: {
			int16 index = pop();
			int16 value = pop();
			if (index >= 0 && (size_t)index < self.locals.size())
				self.locals[index] = value;
			else
				warning("Script: object %u local %d out of range", self.object->id, index);
			break;
		}
		case OP_GET_GLOBAL:
			push(getGlobal(pop()));
			break;
		case OP_SET_GLOBAL: {
			int16 index = pop();
			int16 value = pop();
			setGlobal(index, value);
			break;
		}
		case OP_INC:
			push((int16)(pop() + 1));
			break;
		case OP_DEC:
			push((int16)(pop() - 1));
			break;
		case OP_ADD:
		case OP_SUB:
		case OP_MUL:
		case OP_DIV:
		case OP_SIGN_DIFF:
		case OP_AND:
		case OP_OR:
		case OP_XOR: {
			// The original computes "top op below".
			int top = pop();
			int below = pop();
			int value = 0;
			switch (op) {
			case OP_ADD: value = top + below; break;
			case OP_SUB: value = top - below; break;
			case OP_MUL: value = top * below; break;
			case OP_DIV:
				if (below == 0) {
					warning("Script: object %u divides by zero", self.object->id);
					value = 0;
				} else {
					value = top / below;
				}
				break;
			case OP_SIGN_DIFF: value = top > below ? 1 : top < below ? -1 : 0; break;
			case OP_AND: value = top & below; break;
			case OP_OR: value = top | below; break;
			default: value = top ^ below; break;
			}
			push((int16)value);
			break;
		}
		case OP_NOT:
			push((int16)~pop());
			break;
		case OP_HIGH_BITS:
			push((int16)(pop() & 0xE000));
			break;
		case OP_LOW_BITS:
			push((int16)(pop() & 0x1FFF));
			break;
		default:
			if (!getOpcodeInfo(op)) {
				warning("Script: object %u has unknown opcode %04x", self.object->id, op);
				running = false;
				break;
			}
			callNative(self, op);
			break;
		}
	}

	_stack.swap(savedStack);
	return result;
}

} // End of namespace STG
