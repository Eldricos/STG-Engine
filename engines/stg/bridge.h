/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STG_BRIDGE_H
#define STG_BRIDGE_H

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "common/audio/mixer.h"
#include "common/graphics/surface.h"
#include "common/keyboard.h"
#include "common/random.h"
#include "engines/stg/campaign.h"
#include "engines/stg/font.h"
#include "engines/stg/ini.h"
#include "engines/stg/resources.h"
#include "engines/stg/rgbimage.h"

namespace STG {

/**
 * The bridge: sgens.exe's strategic star map (module 0x47557D), where
 * Picard takes the Enterprise from star to star, scans for Soran and
 * beams down or engages.
 *
 * Above, the map of the ten stars of DESIGN/UNIVERSE.INI, with the fleets,
 * the Nexus and Picard and Data at the rail; below, the panel: the text
 * and scanner display, the stardate, and the buttons. Time passes only as
 * the player acts: a warp takes a stardate, a scan a tenth.
 *
 * The campaign runs it through its script commands (CampaignWorld); what
 * the player does comes back as events. It draws in true colour, as the
 * original's 16-bit screen did, every bitmap in its own palette.
 */
class Bridge : public CampaignWorld {
public:
	static const int kWidth = 640, kHeight = 480;
	static const int kSuns = 10, kFleets = 25;

	/** What the player did that the campaign must act on. */
	struct Event {
		enum Type { kNone, kTransport, kTactical, kCombat, kOptions };
		Type type = kNone;
		/** The fleet to fight (kCombat). */
		int fleet = 0;
	};

	Bridge(const ResourceManager &resources, Audio::Mixer *mixer, Campaign &campaign, Common::RandomSource &random);
	~Bridge() override;

	/** Read UNIVERSE.INI and the pictures; false if STELLAR.DAT lacks them. */
	bool load();
	/** A new game: everything as UNIVERSE.INI starts it (0x4199F8). */
	void reset();

	// CampaignWorld
	void bridgeCommand(const Common::String &command, const std::vector<Common::String> &args) override;
	bool isBridgeShown() const override { return _shown; }
	bool isEnterpriseAt(const Common::String &sun) const override;
	bool getObjectFlag(const Common::String &object, int flag) const override;
	double getStardate() const override { return _stardate; }
	bool isAudioQueueEmpty() const override { return _queue.empty(); }
	bool isWarping() const override;
	bool isZooming() const override { return _zoomLeft > 0; }
	bool isEnterpriseIdle() const override;

	/** Another activity takes the screen: the bridge goes, with its sounds. */
	void hide();
	bool isShown() const { return _shown; }

	/** Run by ms. */
	void update(uint32 ms);
	/** Draw on a 640 x 480 ARGB8888 surface. */
	void draw(Graphics::Surface &screen);

	void mouseMove(int x, int y);
	void mouseDown(int x, int y);
	void mouseUp(int x, int y);
	void keyDown(const Common::KeyState &key);
	Event takeEvent();

	/** Subtitles on or off. */
	void setSubtitles(bool on) { _subtitles = on; }

	/** Save or load the map's state (0x565AC0..), not the pictures. */
	void sync(Common::Serializer &s);
	/** After loading: fleets that were warping are gone (0x419703). */
	void afterLoad();

private:
	struct Sun;
	struct Planet;
	struct Fleet;
	struct QueueItem {
		Common::String path;
		/** 1 effect, 2 music, 3 voice. */
		int type;
	};
	struct Button;
	enum MapMode { kZoomingIn = 0, kZoomingOut = 1, kSystemView = 2, kSectorView = 3 };
	enum PanelMode {
		kPanelEmpty = 0,
		kPanelScanning = 1,
		kPanelResult = 2,
		kPanelFederation = 5,
		kPanelMaleTalking = 6,
		kPanelMaleListening = 7,
		kPanelFemaleTalking = 8,
		kPanelFemaleListening = 9
	};
	enum ContextMode { kContextIdle = 0, kContextTransport = 1, kContextTactical = 2 };

	// Data.
	const RgbImage *image(const Common::String &path, int frames = 1, int key = 254);
	const RgbImage *overlay(const Common::String &path, int frameHeight, const Common::String &palette);
	std::unique_ptr<Common::SeekableReadStream> open(const Common::String &path) const;
	int findSun(const Common::String &name) const;
	/** "Fleet3" is 3, "Enterprise" 0; -1 if it isn't a fleet. */
	int findFleet(const Common::String &name) const;
	std::set<int> &flags(const Common::String &object);
	const std::set<int> *findFlags(const Common::String &object) const;
	/** The sun an object belongs to, or -1. */
	int sunOf(const Common::String &object) const;

	// Commands.
	void moveEnterprise(int sun);
	void moveFleet(int fleet, int sun);
	void moveSoran(const Common::String &object);
	void warpFleetIn(int fleet, int sun, const Common::String &sound);
	void warpFleetAway(int fleet, const Common::String &sound);
	void hideFleet(int fleet);
	void warpAway(int sun);
	void destroySun(int sun);
	void setCourse(int sun, bool fromScript);
	void zoomIn(int sun);
	void zoomOut();
	void setControls(bool enabled);
	void setText(const Common::String &kind, const Common::String &object, const Common::String &text);
	void setFleetAlert(int fleet, const Common::String &level);

	// Sound.
	void queue(const Common::String &path, int type);
	void updateQueue(uint32 ms);
	void playEffect(const Common::String &path);
	void startAmbience();

	// Simulation.
	void updateFleets(uint32 ms);
	void arrive();
	void updateMap(uint32 ms);
	void updateAnimations(uint32 ms);
	void pickNexus(bool speak);
	void startScan(const Common::String &object, bool shortRange);
	void finishScan();
	void simulate(int sun);
	void updateButtons();
	int contextState() const;
	void pressContext();
	/** Add to the stardate (0x472A91): it climbs a tenth every half second. */
	void addTime(double amount) { _targetStardate += amount; }
	void select(const Common::String &object);

	// Drawing.
	struct Projected {
		int x, y, r;
		bool visible;
	};
	Projected project(double x, double y, double z, int size) const;
	void sunPosition(int sun, double &x, double &y, double &z) const;
	void fleetPosition(int fleet, double &x, double &y, double &z) const;
	void planetPosition(const Planet &planet, int sun, double &x, double &y, double &z) const;
	int fleetSlot(int fleet) const;
	void drawMap(Graphics::Surface &screen);
	void drawPanel(Graphics::Surface &screen);
	void drawText(Graphics::Surface &screen, const Common::String &text, int x, int y, const Common::Rect *clip = nullptr);
	void drawDisc(Graphics::Surface &screen, const RgbImage &texture, int frame, int cx, int cy, int r, double light);
	/** The object under (x, y) on the map, or empty. */
	Common::String hit(int x, int y) const;
	std::vector<Common::String> wrapText(const Common::String &text, int width) const;

	const ResourceManager &_resources;
	Audio::Mixer *_mixer;
	Campaign &_campaign;
	Common::RandomSource &_random;
	IniFile _ini;
	std::map<Common::String, std::unique_ptr<RgbImage>> _images;
	Font _font;
	Graphics::Palette _fontPalette;
	std::vector<Sun> _suns;
	std::vector<Fleet> _fleets; // [0] the Enterprise
	struct Figure;
	/** Data and Picard at the rail. */
	std::vector<Figure> _figures;

	// The saved state.
	double _stardate = 48623.4, _targetStardate = 48623.4;
	std::map<Common::String, std::set<int>> _flags;
	Common::String _soran;
	Common::String _clueWave;
	Common::String _selected;
	int _nexusGraphic = 1;
	bool _nexusShown = false;
	int _longScans = 0;
	bool _nexusSpoken = false;
	bool _simEnabled = false;
	bool _controls = true;
	int _contextMode = kContextIdle;

	// The rest.
	bool _shown = false;
	bool _subtitles = true;
	bool _busy = false;
	MapMode _mapMode = kSectorView;
	int _system = -1;
	double _camera[3], _cameraTarget[3], _cameraStep[3];
	uint32 _zoomLeft = 0;
	uint32 _time = 0;
	uint32 _stardateTimer = 0;
	uint32 _stardateLit = 0;
	int _nexusWipe = 640;
	int _nexusOldGraphic = 1;
	int _nexusState = 1; // 1 shown, 2 wiping out, 3 wiping in
	std::deque<QueueItem> _queue;
	bool _queuePlaying = false;
	Audio::SoundHandle _queueHandle, _ambience, _effect;
	uint32 _queueLeft = 0;
	uint32 _idleMusic = 0;
	bool _idleToggle = false;
	Common::String _subtitle;
	// The text and scanner panel.
	int _panelMode = kPanelEmpty;
	int _panelBeforeComm = kPanelEmpty;
	Common::String _scanObject;
	bool _scanShort = false;
	uint32 _scanTime = 0;
	std::vector<Common::String> _textLines;
	int _textScroll = 0;
	uint32 _commTime = 0;
	int _mouth = 0, _eyes = 0;
	// Input.
	std::vector<Button> _buttons;
	int _pressed = -1;
	int _mouseX = 0, _mouseY = 0;
	std::deque<Event> _events;
	/** A combat to start once the queue is quiet. */
	int _pendingCombat = 0;
};

} // End of namespace STG

#endif
