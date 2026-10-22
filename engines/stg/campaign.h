/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STG_CAMPAIGN_H
#define STG_CAMPAIGN_H

#include <functional>
#include <map>
#include <set>
#include <vector>

#include "common/str.h"

namespace Common {
class Serializer;
}

namespace STG {

/**
 * The campaign: sgens.exe's game manager (0x419250), which runs
 * DESIGN/STATES.INI from STELLAR.DAT. Each [section] is a list of
 * commands; some start a full-screen activity (a movie, an away mission),
 * and the script carries on from the next line, or from the section the
 * outcome names, when it ends.
 *
 * The bridge (the strategic map) isn't implemented, so its commands are
 * passed over: OnTransport, which waits for the player to beam down, is
 * taken at once; OnTactical, a chance to engage a fleet, never is (there's
 * no space combat yet); the Enterprise is never "at" a place.
 */
class Campaign {
public:
	/** What the engine is asked to do next. */
	struct Action {
		enum Type {
			kNone,       ///< the script has stopped
			kIntro,      ///< INTRO1.BMP
			kMovie,      ///< MOVIE/MOV_nn.AVI
			kActionGame, ///< away mission n (a location's type); call missionEnded() after
			kScrollText, ///< TEXTn.TXT
			kOptions,    ///< the menu, with nothing to return to
			kEndGame,    ///< credits, then quit
			kAutoSave    ///< save to the autosave slot
		};
		Type type = kNone;
		int number = 0;
	};

	/** Read the script's text; false if it has no [Start]. */
	bool load(const Common::String &text);
	bool isLoaded() const { return !_sections.empty(); }

	/** Start the script at a section (e.g. "Start" or "NewGame"), with everything reset. */
	void start(const Common::String &section);
	/** Run up to the next thing the engine must do. */
	Action next();
	/** An away mission asked for by next() ended. */
	void missionEnded(bool accomplished);

	bool isRunning() const { return _running; }
	/** The section running, for saved games. */
	const Common::String &getSection() const { return _section; }

	/** Random choices (JumpRandom); returns 0..count-1. Defaults to the first. */
	void setRandom(std::function<int(int count)> random) { _random = random; }

	void sync(Common::Serializer &s);

private:
	void jump(const Common::String &section);
	/** Run one line; true if it produced an action. */
	bool execute(const std::vector<Common::String> &words, Action &action);
	/** The conditions in front of a command (IfFlag 5 IfNotFlag 15 Jump X); false if one fails. */
	bool test(const std::vector<Common::String> &words, size_t &i) const;

	std::map<Common::String, std::vector<std::vector<Common::String>>> _sections;
	Common::String _section;
	size_t _line = 0;
	bool _running = false;
	std::set<int> _flags;
	std::set<int> _played, _won, _lost;
	std::set<Common::String> _destroyed;
	std::set<Common::String> _objectFlags;
	std::vector<std::pair<Common::String, int>> _stack;
	// The away mission running, with where to go after it.
	Common::String _wonSection, _lostSection;
	int _mission = 0;
	std::function<int(int)> _random;
};

} // End of namespace STG

#endif
