/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/stg/optionsmenu.h"

#include <algorithm>

#include "common/image/bmp.h"

namespace STG {

namespace {

const char *const kArchive = "OPTIONS.DAT";

/** The panels' corner on the screen. */
const int kPanelX = 227, kPanelY = 29;
/** Slot rows on the Load and Save panels, within the panel, as OPTSAVE.BMP draws them. */
const int kSlotTop = 33, kSlotPitch = 30, kSlotHeight = 26;
const int kSlotNameX = 31, kSlotDateX = 232, kSlotRight = 309;
const int kMaxNameLength = 48;
/** The volume sliders, within the Settings panel. */
const Common::Rect kEffectsSlider(106, 126, 318, 144);
const Common::Rect kMusicSlider(106, 157, 318, 175);
const int kSliderSteps = 30;

/** The selected key cell's highlight, KEYHIGH.BMP, from the cell (0x483C8B). */
const int kKeyHighX = -3, kKeyHighY = -6;

} // End of anonymous namespace

// Rectangles from 0x47DCBB; panel widgets are relative to the panel.
const OptionsMenu::WidgetInfo OptionsMenu::kWidgets[kWidgetCount] = {
	{ "LOAD.BMP", 3, Common::Rect(107, 32, 227, 59), false },
	{ "SAVE.BMP", 3, Common::Rect(107, 62, 227, 89), false },
	{ "NEW.BMP", 3, Common::Rect(107, 92, 227, 119), false },
	{ "QUIT.BMP", 3, Common::Rect(107, 122, 227, 149), false },
	{ "SETTINGS.BMP", 3, Common::Rect(107, 152, 227, 179), false },
	{ "CONTROLS.BMP", 3, Common::Rect(107, 182, 227, 209), false },
	{ "RETURN.BMP", 3, Common::Rect(378, 389, 542, 422), false },
	{ "ACCEPT.BMP", 2, Common::Rect(378, 389, 460, 422), false },
	{ "CANCEL.BMP", 2, Common::Rect(459, 389, 541, 422), false },
	{ "AUTOSAVE.BMP", 1, Common::Rect(6, 303, 231, 331), true },
	{ "DIFFICUL.BMP", 3, Common::Rect(3, 182, 151, 211), true },
	{ "DIF-OPS.BMP", 3, Common::Rect(151, 182, 234, 211), true },
	{ "WINDOW.BMP", 2, Common::Rect(3, 212, 151, 241), true },
	{ "WIN-OPS.BMP", 2, Common::Rect(151, 212, 234, 241), true },
	{ "SUBTITL.BMP", 2, Common::Rect(3, 242, 151, 271), true },
	{ "SUB-OPS.BMP", 2, Common::Rect(151, 242, 234, 271), true },
	{ "STARBTTN.BMP", 2, Common::Rect(170, 0, 319, 32), true },
	{ "AWAYBTTN.BMP", 2, Common::Rect(3, 0, 152, 32), true },
	{ "LKSPRING.BMP", 2, Common::Rect(171, 300, 285, 333), true },
	{ "LKSONOFF.BMP", 2, Common::Rect(286, 300, 316, 333), true }
};

OptionsMenu::OptionsMenu(const ResourceManager &resources) : _resources(resources) {
	_slots.resize(kSlots + 1);
}

const FramedImage *OptionsMenu::image(const Common::String &name, int frames) {
	auto it = _images.find(name);
	if (it != _images.end())
		return it->second.get();
	std::unique_ptr<FramedImage> result;
	::Image::BitmapDecoder decoder;
	if (_resources.loadImage({ kArchive, name }, decoder) && decoder.hasPalette()) {
		result.reset(new FramedImage());
		result->set(decoder, _palette, -frames);
	}
	const FramedImage *found = result.get();
	_images[name] = std::move(result);
	return found;
}

const FramedImage *OptionsMenu::find(const Common::String &name) const {
	auto it = _images.find(name);
	return it == _images.end() ? nullptr : it->second.get();
}

bool OptionsMenu::load() {
	// Every picture is matched to the menu's own palette.
	::Image::BitmapDecoder decoder;
	if (!_resources.loadImage({ kArchive, "CAPTAINS.BMP" }, decoder) || !decoder.hasPalette())
		return false;
	_palette = decoder.getPalette();
	for (int i = 0; i < 256; i++) {
		byte r, g, b;
		_palette.get(i, r, g, b);
		_dim[i] = _palette.findBestColor(r * 2 / 5, g * 2 / 5, b * 2 / 5);
	}
	_images.clear();
	image("CAPTAINS.BMP");
	for (const WidgetInfo &w : kWidgets)
		image(w.image, w.frames);
	for (const char *name : { "OPTSAVE.BMP", "OPTSET.BMP", "OPTNEW.BMP", "OPTQUIT.BMP", "AWAY.BMP", "STARSHIP.BMP",
	                          "NAMEBAR.BMP", "VOL-ON.BMP", "KEYHIGH.BMP" })
		image(name);
	image("SAVELOAD.BMP", 4);

	std::unique_ptr<Common::SeekableReadStream> font = _resources.open({ kArchive, "OPTIONS.FNT" });
	if (font)
		_font.load(*font);
	return find("CAPTAINS.BMP") != nullptr;
}

void OptionsMenu::open(const GameSettings &settings, bool canReturn, bool canSave, bool canChangeDifficulty) {
	_settings = _working = settings;
	_encounter = false;
	_canReturn = canReturn;
	_canSave = canSave;
	_canChangeDifficulty = canChangeDifficulty;
	_panel = kPanelMain;
	_pressed = -1;
	_result = kNone;
	_selected = -1;
	play(1000);
}

OptionsMenu::Result OptionsMenu::takeResult() {
	Result result = _result;
	_result = kNone;
	return result;
}

bool OptionsMenu::takeSettingsChanged() {
	bool changed = _settingsChanged;
	_settingsChanged = false;
	return changed;
}

void OptionsMenu::play(int id) {
	if (_sound)
		_sound(id);
}

bool OptionsMenu::isShown(int widget) const {
	switch (widget) {
	case kReturnButton:
		return _panel == kPanelMain;
	case kAcceptButton:
	case kCancelButton:
		return _panel != kPanelMain;
	case kAutoSaveButton:
		return _panel == kPanelLoad;
	case kDifficultyLabel:
	case kDifficultyValue:
	case kWindowLabel:
	case kWindowValue:
	case kSubtitlesLabel:
	case kSubtitlesValue:
		return _panel == kPanelSettings;
	case kStarshipTab:
	case kLookspringLabel:
	case kLookspringValue:
		return _panel == kPanelControls && !_starshipPage;
	case kAwayTab:
		return _panel == kPanelControls && _starshipPage;
	default:
		return true;
	}
}

bool OptionsMenu::isEnabled(int widget) const {
	if (widget <= kControlsButton) {
		// The main buttons go dim while a panel is open.
		return _panel == kPanelMain && (widget != kSaveButton || _canSave);
	}
	if (widget == kReturnButton)
		return _canReturn;
	if (widget == kDifficultyLabel || widget == kDifficultyValue)
		return _canChangeDifficulty;
	return isShown(widget);
}

Common::Rect OptionsMenu::screenRect(int widget) const {
	Common::Rect r = kWidgets[widget].rect;
	if (kWidgets[widget].inPanel)
		r.translate(kPanelX, kPanelY);
	return r;
}

int OptionsMenu::hit(int x, int y) const {
	for (int i = 0; i < kWidgetCount; i++) {
		if (isShown(i) && isEnabled(i) && screenRect(i).contains(x, y))
			return i;
	}
	return -1;
}

void OptionsMenu::mouseDown(int x, int y, int button) {
	if (_captureCell >= 0) {
		// The panel holds the mouse while it waits, so a button is the key.
		assignKey(button);
		return;
	}
	if (button != 1)
		return;
	_pressed = hit(x, y);
	if (_pressed < 0)
		_dragging = clickPanel(x, y, false);
}

void OptionsMenu::mouseMove(int x, int y) {
	if (_dragging)
		clickPanel(x, y, true);
}

void OptionsMenu::mouseUp(int x, int y) {
	if (_dragging) {
		_dragging = false;
		// Let the player hear the new effects volume.
		if (_panel == kPanelSettings)
			play(1001);
	}
	int pressed = _pressed;
	_pressed = -1;
	if (pressed >= 0 && hit(x, y) == pressed)
		click(pressed);
}

void OptionsMenu::keyDown(const Common::KeyState &key) {
	if (_captureCell >= 0) {
		// The original takes Escape as a key here too; this lets it cancel.
		if (key.keycode == Common::KEYCODE_ESCAPE)
			_captureCell = -1;
		else
			assignKey(KeyMap::virtualKey(key.keycode));
		return;
	}
	if (key.keycode == Common::KEYCODE_ESCAPE) {
		if (_panel != kPanelMain)
			cancel();
		else if (_canReturn)
			click(kReturnButton);
		return;
	}
	if (key.keycode == Common::KEYCODE_RETURN) {
		if (_panel != kPanelMain)
			accept();
		else if (_canReturn)
			click(kReturnButton);
		return;
	}
	if (_panel == kPanelSave && _selected >= 0) {
		if (key.keycode == Common::KEYCODE_BACKSPACE) {
			if (!_editName.empty())
				_editName.pop_back();
		} else if (key.ascii >= 32 && key.ascii < 127 && (int)_editName.size() < kMaxNameLength &&
		           _font.getStringWidth(_editName + (char)key.ascii) < kSlotDateX - kSlotNameX - 12) {
			_editName += (char)key.ascii;
		}
	}
}

void OptionsMenu::openPanel(Panel panel) {
	play(1001);
	_panel = panel;
	_working = _settings;
	_selected = -1;
	_editName.clear();
	_starshipPage = false;
	_captureCell = -1;
	if (panel == kPanelControls)
		fillCells();
}

Common::Rect OptionsMenu::cellRect(int cell) {
	int action = cell / 2;
	int x = (action < 10 ? 84 : 252) + 34 * (cell % 2), y = 37 + 30 * (action % 10);
	return Common::Rect(x, y, x + 32, y + 26);
}

int OptionsMenu::hitCell(int x, int y) const {
	int cells = 2 * (_starshipPage ? KeyMap::kStarshipActions : KeyMap::kAwayActions);
	for (int i = 0; i < cells; i++) {
		if (cellRect(i).contains(x - kPanelX, y - kPanelY))
			return i;
	}
	return -1;
}

void OptionsMenu::fillCells() {
	for (int page = 0; page < 2; page++) {
		const KeyMap &map = page ? _working.starshipKeys : _working.awayKeys;
		const byte *actions = page ? KeyMap::kStarshipPanel : KeyMap::kAwayPanel;
		int count = page ? KeyMap::kStarshipActions : KeyMap::kAwayActions;
		for (int i = 0; i < count; i++)
			map.findKeys(actions[i], _cellKeys[page][2 * i], _cellKeys[page][2 * i + 1]);
	}
}

void OptionsMenu::assignKey(int vk) {
	// 0x485AF3 takes the key while the cell waits: not the function keys,
	// and for Mouse Look only the left or right button.
	if (vk < 0 || (vk >= 0x70 && vk <= 0x7B))
		return;
	int page = _starshipPage ? 1 : 0;
	byte action = (page ? KeyMap::kStarshipPanel : KeyMap::kAwayPanel)[_captureCell / 2];
	if (action == KeyMap::kAwayMouseLook && !page && vk != 1 && vk != 2)
		return;
	// 0x483CAB: the cell's old key does nothing now, the new key does the
	// cell's action, and any other cell showing the new key is cleared.
	KeyMap &map = page ? _working.starshipKeys : _working.awayKeys;
	int &cell = _cellKeys[page][_captureCell];
	if (cell >= 0)
		map.set(cell, KeyMap::kNone);
	map.set(vk, action);
	int cells = 2 * (page ? KeyMap::kStarshipActions : KeyMap::kAwayActions);
	for (int i = 0; i < cells; i++) {
		if (_cellKeys[page][i] == vk)
			_cellKeys[page][i] = -1;
	}
	cell = vk;
	_captureCell = -1;
}

void OptionsMenu::click(int widget) {
	switch (widget) {
	case kLoadButton:
		openPanel(kPanelLoad);
		break;
	case kSaveButton:
		openPanel(kPanelSave);
		break;
	case kNewButton:
		openPanel(kPanelNew);
		break;
	case kQuitButton:
		openPanel(kPanelQuit);
		break;
	case kSettingsButton:
		openPanel(kPanelSettings);
		break;
	case kControlsButton:
		openPanel(kPanelControls);
		break;
	case kReturnButton:
		play(1003);
		play(2002);
		_result = kReturn;
		break;
	case kAcceptButton:
		accept();
		break;
	case kCancelButton:
		cancel();
		break;
	case kAutoSaveButton:
		if (_slots[kAutoSaveSlot].used) {
			_selected = kAutoSaveSlot;
			play(1001);
		} else {
			play(1002);
		}
		break;
	case kDifficultyLabel:
	case kDifficultyValue:
		_working.difficulty = (_working.difficulty + 1) % 3;
		play(1001);
		break;
	case kWindowLabel:
	case kWindowValue:
		// The view is always the large window's here, so the choice is
		// shown dimmed and refused with the empty-slot sound.
		play(1002);
		break;
	case kSubtitlesLabel:
	case kSubtitlesValue:
		_working.subtitles = !_working.subtitles;
		play(1001);
		break;
	case kStarshipTab:
	case kAwayTab:
		_starshipPage = widget == kStarshipTab;
		_captureCell = -1;
		play(1001);
		break;
	case kLookspringLabel:
	case kLookspringValue:
		_working.lookspring = !_working.lookspring;
		play(1001);
		break;
	default:
		break;
	}
}

void OptionsMenu::accept() {
	play(1003);
	switch (_panel) {
	case kPanelLoad:
		if (_selected < 0)
			return;
		_result = kLoad;
		break;
	case kPanelSave:
		if (_selected < 0)
			return;
		if (Common::trim(_editName).empty())
			_editName = "Saved game";
		_result = kSave;
		break;
	case kPanelNew:
		play(2001);
		_result = kNewGame;
		break;
	case kPanelQuit:
		play(2001);
		_result = kQuit;
		break;
	case kPanelSettings:
	case kPanelControls:
		if (_working != _settings) {
			_settings = _working;
			_settingsChanged = true;
		}
		break;
	default:
		break;
	}
	_panel = kPanelMain;
}

void OptionsMenu::cancel() {
	play(1003);
	play(2000);
	_working = _settings;
	_captureCell = -1;
	_panel = kPanelMain;
	_selected = -1;
}

bool OptionsMenu::clickPanel(int x, int y, bool drag) {
	int px = x - kPanelX, py = y - kPanelY;
	if (_panel == kPanelSettings) {
		for (int i = 0; i < 2; i++) {
			const Common::Rect &slider = i == 0 ? kEffectsSlider : kMusicSlider;
			// While dragging, any height will do.
			if (px < slider.left - (drag ? 20 : 0) || px >= slider.right + (drag ? 20 : 0) ||
			    (!drag && (py < slider.top || py >= slider.bottom)))
				continue;
			if (drag && (py < slider.top - 30 || py >= slider.bottom + 30))
				continue;
			int value = (px - slider.left) * kSliderSteps / slider.width();
			value = std::max(0, std::min((int)GameSettings::kMaxVolume, value));
			(i == 0 ? _working.effectsVolume : _working.musicVolume) = value;
			return true;
		}
		return false;
	}
	if (_panel == kPanelControls && !drag) {
		// A cell waits for a key (0x483843).
		_captureCell = hitCell(x, y);
		return false;
	}
	if ((_panel == kPanelLoad || _panel == kPanelSave) && !drag && px >= kSlotNameX - 28 && px < kSlotRight) {
		for (int i = 0; i < kSlots; i++) {
			int top = kSlotTop + kSlotPitch * i;
			if (py < top - 2 || py >= top + kSlotHeight + 2)
				continue;
			if (_panel == kPanelLoad && !_slots[i].used) {
				play(1002);
				return false;
			}
			if (_panel == kPanelSave && _selected != i)
				_editName = _slots[i].used ? _slots[i].name : Common::String();
			_selected = i;
			play(1001);
			return false;
		}
	}
	return false;
}

void OptionsMenu::drawWidget(Graphics::Surface &screen, int widget, int frame) const {
	const FramedImage *img = find(kWidgets[widget].image);
	if (!img)
		return;
	Common::Rect r = screenRect(widget);
	img->draw(screen, frame, r.left, r.top);
	if (widget == kWindowLabel || widget == kWindowValue) {
		// Dimmed: it does nothing here.
		r.clip(Common::Rect(0, 0, screen.w, screen.h));
		for (int y = r.top; y < r.bottom; y++) {
			byte *row = (byte *)screen.getBasePtr(0, y);
			for (int x = r.left; x < r.right; x++)
				row[x] = _dim[row[x]];
		}
	}
}

void OptionsMenu::drawText(Graphics::Surface &screen, const Common::String &text, int x, int y) const {
	// The font is drawn in the menu's palette as it is.
	_font.drawString(screen, text, x, y);
}

void OptionsMenu::drawSlider(Graphics::Surface &screen, const Common::Rect &rect, int value) const {
	const FramedImage *on = find("VOL-ON.BMP");
	if (!on)
		return;
	int lit = value + 1;
	for (int i = 0; i < lit && i < kSliderSteps; i++)
		on->draw(screen, 0, kPanelX + rect.left + 2 + i * 7, kPanelY + rect.top);
}

void OptionsMenu::draw(Graphics::Surface &screen) const {
	if (const FramedImage *background = find("CAPTAINS.BMP"))
		background->draw(screen, 0, 0, 0);

	// The panel over the picture.
	const char *panelImage = nullptr;
	switch (_panel) {
	case kPanelLoad:
	case kPanelSave:
		panelImage = "OPTSAVE.BMP";
		break;
	case kPanelSettings:
		panelImage = "OPTSET.BMP";
		break;
	case kPanelNew:
		panelImage = "OPTNEW.BMP";
		break;
	case kPanelQuit:
		panelImage = "OPTQUIT.BMP";
		break;
	case kPanelControls:
		panelImage = _starshipPage ? "STARSHIP.BMP" : "AWAY.BMP";
		break;
	default:
		break;
	}
	if (panelImage) {
		if (const FramedImage *panel = find(panelImage))
			panel->draw(screen, 0, kPanelX, kPanelY);
	}

	if (_panel == kPanelLoad || _panel == kPanelSave) {
		if (const FramedImage *title = find("SAVELOAD.BMP"))
			// Load, Save, Save at start of current mission (unused: missions
			// are saved as they are), Save at start of current encounter.
			title->draw(screen, _panel == kPanelLoad ? 0 : _encounter ? 3 : 1, kPanelX + 3, kPanelY + 3);
		if (_selected >= 0) {
			if (const FramedImage *bar = find("NAMEBAR.BMP"))
				bar->draw(screen, 0, kPanelX + 4, kPanelY + kSlotTop + kSlotPitch * _selected - 3,
				          FramedImage::kFirstPixel);
		}
		int textY = (kSlotHeight - _font.getHeight()) / 2;
		for (int i = 0; i < kSlots; i++) {
			int y = kPanelY + kSlotTop + kSlotPitch * i + textY;
			Common::String name = _slots[i].used ? _slots[i].name : Common::String("Empty");
			if (_panel == kPanelSave && i == _selected)
				name = _editName + "_";
			drawText(screen, name, kPanelX + kSlotNameX, y);
			if (_slots[i].used)
				drawText(screen, _slots[i].date, kPanelX + kSlotDateX, y);
		}
		if (_panel == kPanelLoad && _slots[kAutoSaveSlot].used)
			drawText(screen, _slots[kAutoSaveSlot].date, kPanelX + kSlotDateX,
			         kPanelY + kWidgets[kAutoSaveButton].rect.top + textY);
	} else if (_panel == kPanelSettings) {
		drawSlider(screen, kEffectsSlider, _working.effectsVolume);
		drawSlider(screen, kMusicSlider, _working.musicVolume);
	} else if (_panel == kPanelControls) {
		// Two cells for each action, down two columns.
		int page = _starshipPage ? 1 : 0;
		if (_captureCell >= 0) {
			if (const FramedImage *high = find("KEYHIGH.BMP")) {
				Common::Rect r = cellRect(_captureCell);
				high->draw(screen, 0, kPanelX + r.left + kKeyHighX, kPanelY + r.top + kKeyHighY,
				           FramedImage::kFirstPixel);
			}
		}
		int cells = 2 * (page ? KeyMap::kStarshipActions : KeyMap::kAwayActions);
		for (int i = 0; i < cells; i++) {
			if (_cellKeys[page][i] < 0)
				continue;
			Common::String key = KeyMap::keyName(_cellKeys[page][i]);
			Common::Rect r = cellRect(i);
			drawText(screen, key, kPanelX + r.left + (r.width() - _font.getStringWidth(key)) / 2, kPanelY + r.top + 5);
		}
	}

	// The widgets: 0 dim, 1 normal, 2 lit.
	for (int i = 0; i < kWidgetCount; i++) {
		if (!isShown(i))
			continue;
		int frame;
		switch (i) {
		case kDifficultyValue:
			frame = _working.difficulty;
			break;
		case kWindowValue:
			frame = 0; // LARGE, the only window here
			break;
		case kSubtitlesValue:
			frame = _working.subtitles ? 0 : 1;
			break;
		case kLookspringValue:
			frame = _working.lookspring ? 0 : 1;
			break;
		case kAutoSaveButton:
			// OPTSAVE.BMP has the row; this lights it.
			if (_selected != kAutoSaveSlot && _pressed != kAutoSaveButton)
				continue;
			frame = 0;
			break;
		default: {
			bool open = (i == kLoadButton && (_panel == kPanelLoad)) || (i == kSaveButton && _panel == kPanelSave) ||
			            (i == kNewButton && _panel == kPanelNew) || (i == kQuitButton && _panel == kPanelQuit) ||
			            (i == kSettingsButton && _panel == kPanelSettings) ||
			            (i == kControlsButton && _panel == kPanelControls);
			int frames = kWidgets[i].frames;
			if (frames == 3)
				frame = i == _pressed || open ? 2 : isEnabled(i) ? 1 : 0;
			else
				frame = i == _pressed ? 1 : 0;
			break;
		}
		}
		drawWidget(screen, i, frame);
	}

	drawText(screen, "v1.00", 500, 373);
}

} // End of namespace STG
