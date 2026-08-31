#include "MtgTokenActivity.h"

#include <Arduino.h>
#include <Bitmap.h>
#include <HalStorage.h>

#include <cstdio>
#include <cstdlib>
#include <utility>

namespace fui = freeink::ui;

MtgTokenActivity::MtgTokenActivity(
    GfxRenderer& renderer,
    MappedInputManager& mappedInput)
    : Activity("MtgTokens", renderer, mappedInput),
      UiAppHost(renderer) {}

void MtgTokenActivity::onEnter() {
  Activity::onEnter();

  resetUi();

  resetSlotsToEmpty();
  loadState();

  view = View::TokenGrid;
  editingStat = StatField::None;

  selectedSlot = 0;
  selectedEditItem = 0;
  selectedLetter = 0;
  selectedTokenIndex = 0;

  editStatValue = 0;
  tokenLoadError = false;

  tokenChoices.clear();
  tokenChoices.reserve(64);

  app.on(
      ACTION_SELECT_SLOT,
      &MtgTokenActivity::onTokenSelected,
      this);

  app.on(
      ACTION_EDIT_ITEM,
      &MtgTokenActivity::onEditItemSelected,
      this);

  app.setScreen(
      &MtgTokenActivity::tokenScreen,
      this);

  requestUpdate();
}

void MtgTokenActivity::onExit() {
  if (stateDirty) {
    saveState();
  }

  tokenChoices.clear();

  Activity::onExit();
}

void MtgTokenActivity::resetSlotsToEmpty() {
  for (int i = 0; i < SLOT_COUNT; ++i) {
    slots[i].name.clear();
    slots[i].power.clear();
    slots[i].toughness.clear();
    slots[i].color.clear();
    slots[i].artFile.clear();
    slots[i].oracleText.clear();
    slots[i].quantity = 0;
  }
}

void MtgTokenActivity::clearSelectedCard() {
  TokenSlot& slot =
      slots[selectedSlot];

  slot.name.clear();
  slot.power.clear();
  slot.toughness.clear();
  slot.color.clear();
  slot.artFile.clear();
  slot.oracleText.clear();
  slot.quantity = 0;

  tokenChoices.clear();

  editingStat = StatField::None;
  editStatValue = 0;

  markStateDirty();

  // Destructive menu actions save immediately.
  saveState();

  setView(
      View::TokenGrid);
}

void MtgTokenActivity::clearBoard() {
  resetSlotsToEmpty();

  tokenChoices.clear();

  selectedSlot = 0;
  selectedEditItem = 0;
  selectedLetter = 0;
  selectedTokenIndex = 0;

  editingStat = StatField::None;
  editStatValue = 0;

  tokenLoadError = false;

  markStateDirty();

  // Explicit board clear persists immediately.
  saveState();

  setView(
      View::TokenGrid);
}

void MtgTokenActivity::markStateDirty() {
  stateDirty = true;
  stateDirtyAt = millis();
}

void MtgTokenActivity::maybeSaveState() {
  if (!stateDirty) {
    return;
  }

  if (
      millis() - stateDirtyAt <
      STATE_SAVE_DEBOUNCE_MS) {
    return;
  }

  saveState();
}

std::string MtgTokenActivity::escapeSessionField(
    const std::string& value) {
  std::string result;

  result.reserve(
      value.size() + 16);

  for (const char character : value) {
    switch (character) {
      case '\\':
        result += "\\\\";
        break;

      case '\n':
        result += "\\n";
        break;

      case '\r':
        result += "\\r";
        break;

      case '\t':
        result += "\\t";
        break;

      default:
        result.push_back(
            character);
        break;
    }
  }

  return result;
}

std::string MtgTokenActivity::unescapeSessionField(
    const std::string& value) {
  std::string result;

  result.reserve(
      value.size());

  for (size_t i = 0;
       i < value.size();
       ++i) {
    const char character =
        value[i];

    if (
        character != '\\' ||
        i + 1 >= value.size()) {
      result.push_back(
          character);

      continue;
    }

    const char escaped =
        value[++i];

    switch (escaped) {
      case 'n':
        result.push_back(
            '\n');
        break;

      case 'r':
        result.push_back(
            '\r');
        break;

      case 't':
        result.push_back(
            '\t');
        break;

      case '\\':
        result.push_back(
            '\\');
        break;

      default:
        result.push_back(
            '\\');

        result.push_back(
            escaped);
        break;
    }
  }

  return result;
}

bool MtgTokenActivity::splitSessionLine(
    const std::string& line,
    const size_t expectedFields,
    std::vector<std::string>& fields) {
  fields.clear();
  fields.reserve(
      expectedFields);

  size_t start = 0;

  while (true) {
    const size_t end =
        line.find(
            '\t',
            start);

    if (end ==
        std::string::npos) {
      fields.push_back(
          line.substr(start));

      break;
    }

    fields.push_back(
        line.substr(
            start,
            end - start));

    start = end + 1;
  }

  return (
      fields.size() ==
      expectedFields);
}

bool MtgTokenActivity::saveState() {
  if (!Storage.ready()) {
    return false;
  }

  if (!Storage.ensureDirectoryExists(
          SESSION_DIRECTORY)) {
    return false;
  }

  std::string data;

  data.reserve(2048);

  /*
   * X3MTG2 adds oracleText as the seventh field.
   *
   * Newlines are escaped here so each tile remains exactly
   * one physical line in the session file.
   */
  data += "X3MTG2\n";

  for (int i = 0; i < SLOT_COUNT; ++i) {
    const TokenSlot& slot =
        slots[i];

    data += escapeSessionField(
        slot.name);

    data += '\t';

    data += escapeSessionField(
        slot.power);

    data += '\t';

    data += escapeSessionField(
        slot.toughness);

    data += '\t';

    data += escapeSessionField(
        slot.color);

    data += '\t';

    data += escapeSessionField(
        slot.artFile);

    data += '\t';

    data += std::to_string(
        slot.quantity);

    data += '\t';

    data += escapeSessionField(
        slot.oracleText);

    data += '\n';
  }

  HalFile file;

  if (!Storage.openFileForWrite(
          "MTG",
          SESSION_PATH,
          file)) {
    return false;
  }

  const size_t written =
      file.write(
          data.data(),
          data.size());

  file.flush();
  file.close();

  if (written != data.size()) {
    return false;
  }

  stateDirty = false;

  return true;
}

bool MtgTokenActivity::loadState() {
  stateDirty = false;

  if (!Storage.ready()) {
    return false;
  }

  HalFile file;

  if (!Storage.openFileForRead(
          "MTG",
          SESSION_PATH,
          file)) {
    return false;
  }

  std::string line;

  if (!readLine(
          file,
          line)) {
    file.close();

    return false;
  }

  const bool version1 =
      line == "X3MTG1";

  const bool version2 =
      line == "X3MTG2";

  if (!version1 &&
      !version2) {
    file.close();

    return false;
  }

  TokenSlot loadedSlots[SLOT_COUNT];

  for (int i = 0;
       i < SLOT_COUNT;
       ++i) {
    if (!readLine(
            file,
            line)) {
      file.close();

      return false;
    }

    std::vector<std::string> fields;

    const size_t expectedFields =
        version2 ? 7 : 6;

    if (!splitSessionLine(
            line,
            expectedFields,
            fields)) {
      file.close();

      return false;
    }

    TokenSlot& slot =
        loadedSlots[i];

    if (version2) {
      slot.name =
          unescapeSessionField(
              fields[0]);

      slot.power =
          unescapeSessionField(
              fields[1]);

      slot.toughness =
          unescapeSessionField(
              fields[2]);

      slot.color =
          unescapeSessionField(
              fields[3]);

      slot.artFile =
          unescapeSessionField(
              fields[4]);

      slot.oracleText =
          unescapeSessionField(
              fields[6]);
    } else {
      /*
       * Backward compatibility with the session format
       * from the previous firmware build.
       */
      slot.name =
          fields[0];

      slot.power =
          fields[1];

      slot.toughness =
          fields[2];

      slot.color =
          fields[3];

      slot.artFile =
          fields[4];

      slot.oracleText.clear();
    }

    long quantity =
        std::strtol(
            fields[5].c_str(),
            nullptr,
            10);

    if (quantity < 0) {
      quantity = 0;
    } else if (quantity > 999) {
      quantity = 999;
    }

    slot.quantity =
        static_cast<int>(
            quantity);

    if (slot.name.empty()) {
      slot.power.clear();
      slot.toughness.clear();
      slot.color.clear();
      slot.artFile.clear();
      slot.oracleText.clear();
      slot.quantity = 0;
    }
  }

  file.close();

  for (int i = 0;
       i < SLOT_COUNT;
       ++i) {
    slots[i] =
        std::move(
            loadedSlots[i]);
  }

  return true;
}

void MtgTokenActivity::moveSelection(
    const int delta) {
  selectedSlot += delta;

  if (selectedSlot < 0) {
    selectedSlot =
        SLOT_COUNT - 1;
  } else if (
      selectedSlot >=
      SLOT_COUNT) {
    selectedSlot = 0;
  }

  requestUpdate();
}

void MtgTokenActivity::adjustQuantity(
    const int delta) {
  TokenSlot& slot =
      slots[selectedSlot];

  if (slot.name.empty()) {
    return;
  }

  int next =
      slot.quantity +
      delta;

  if (next < 0) {
    next = 0;
  } else if (next > 999) {
    next = 999;
  }

  if (next ==
      slot.quantity) {
    return;
  }

  slot.quantity =
      next;

  markStateDirty();

  requestUpdate();
}

void MtgTokenActivity::moveEditSelection(
    const int delta) {
  selectedEditItem += delta;

  if (selectedEditItem < 0) {
    selectedEditItem =
        EDIT_ITEM_COUNT - 1;
  } else if (
      selectedEditItem >=
      EDIT_ITEM_COUNT) {
    selectedEditItem = 0;
  }

  requestUpdate();
}

void MtgTokenActivity::moveLetterSelection(
    const int delta) {
  selectedLetter += delta;

  if (selectedLetter < 0) {
    selectedLetter =
        LETTER_COUNT - 1;
  } else if (
      selectedLetter >=
      LETTER_COUNT) {
    selectedLetter = 0;
  }

  requestUpdate();
}

void MtgTokenActivity::moveTokenSelection(
    const int delta) {
  if (tokenChoices.empty()) {
    return;
  }

  selectedTokenIndex += delta;

  if (selectedTokenIndex < 0) {
    selectedTokenIndex =
        static_cast<int>(
            tokenChoices.size()) -
        1;
  } else if (
      selectedTokenIndex >=
      static_cast<int>(
          tokenChoices.size())) {
    selectedTokenIndex = 0;
  }

  requestUpdate();
}

int MtgTokenActivity::parseStatValue(
    const std::string& value) {
  if (value.empty()) {
    return 0;
  }

  char* end = nullptr;

  const long parsed =
      std::strtol(
          value.c_str(),
          &end,
          10);

  if (
      end == value.c_str() ||
      *end != '\0') {
    return 0;
  }

  if (parsed < -99) {
    return -99;
  }

  if (parsed > 999) {
    return 999;
  }

  return static_cast<int>(
      parsed);
}

void MtgTokenActivity::beginStatEdit(
    const StatField field) {
  editingStat =
      field;

  const TokenSlot& slot =
      slots[selectedSlot];

  if (field ==
      StatField::Power) {
    editStatValue =
        parseStatValue(
            slot.power);

  } else if (
      field ==
      StatField::Toughness) {
    editStatValue =
        parseStatValue(
            slot.toughness);

  } else {
    editStatValue = 0;
  }

  setView(
      View::StatEditor);
}

void MtgTokenActivity::adjustStatValue(
    const int delta) {
  int next =
      editStatValue +
      delta;

  if (next < -99) {
    next = -99;
  } else if (next > 999) {
    next = 999;
  }

  if (next ==
      editStatValue) {
    return;
  }

  editStatValue =
      next;

  requestUpdate();
}

void MtgTokenActivity::commitStatEdit() {
  TokenSlot& slot =
      slots[selectedSlot];

  if (editingStat ==
      StatField::Power) {
    slot.power =
        std::to_string(
            editStatValue);

  } else if (
      editingStat ==
      StatField::Toughness) {
    slot.toughness =
        std::to_string(
            editStatValue);

  } else {
    return;
  }

  editingStat =
      StatField::None;

  markStateDirty();

  setView(
      View::EditMenu);
}

void MtgTokenActivity::setView(
    const View nextView) {
  view = nextView;

  closeRouting();

  requestUpdate();
}

void MtgTokenActivity::activateCurrentSelection() {
  switch (view) {
    case View::TokenGrid:
      selectedEditItem = 0;

      setView(
          View::EditMenu);
      break;

    case View::EditMenu:
      switch (selectedEditItem) {
        case 0:
          beginStatEdit(
              StatField::Power);
          break;

        case 1:
          beginStatEdit(
              StatField::Toughness);
          break;

        case 2:
          // Edit Color comes next.
          break;

        case 3:
          selectedLetter = 0;

          setView(
              View::AlphabetPicker);
          break;

        case 4:
          setView(
              View::CardText);
          break;

        case 5:
          clearSelectedCard();
          break;

        case 6:
          clearBoard();
          break;

        default:
          break;
      }

      break;

    case View::AlphabetPicker: {
      const char letter =
          static_cast<char>(
              'A' +
              selectedLetter);

      selectedTokenIndex = 0;

      loadTokensForLetter(
          letter);

      setView(
          View::TokenPicker);
      break;
    }

    case View::TokenPicker:
      if (!tokenLoadError &&
          !tokenChoices.empty()) {
        assignSelectedToken();

        setView(
            View::TokenGrid);
      }

      break;

    case View::StatEditor:
      commitStatEdit();
      break;

    case View::CardText:
      // Card text is read-only.
      break;
  }
}

void MtgTokenActivity::goBackOneLevel() {
  switch (view) {
    case View::TokenGrid:
      break;

    case View::EditMenu:
      setView(
          View::TokenGrid);
      break;

    case View::AlphabetPicker:
      setView(
          View::EditMenu);
      break;

    case View::TokenPicker:
      tokenChoices.clear();

      selectedTokenIndex = 0;
      tokenLoadError = false;

      setView(
          View::AlphabetPicker);
      break;

    case View::StatEditor:
      // Back cancels the unsaved numeric edit.
      editingStat =
          StatField::None;

      setView(
          View::EditMenu);
      break;

    case View::CardText:
      setView(
          View::EditMenu);
      break;
  }
}

void MtgTokenActivity::onTokenSelected(
    const fui::ActionEvent& event,
    void* user) {
  auto* self =
      static_cast<MtgTokenActivity*>(
          user);

  if (event.value < 0 ||
      event.value >= SLOT_COUNT) {
    return;
  }

  self->selectedSlot =
      event.value;

  self->requestUpdate();
}

void MtgTokenActivity::onEditItemSelected(
    const fui::ActionEvent& event,
    void* user) {
  auto* self =
      static_cast<MtgTokenActivity*>(
          user);

  if (event.value < 0 ||
      event.value >=
          EDIT_ITEM_COUNT) {
    return;
  }

  self->selectedEditItem =
      event.value;

  self->activateCurrentSelection();
}

void MtgTokenActivity::loop() {
  maybeSaveState();

  const auto route =
      routeTouch(
          mappedInput);

  if (route.routed &&
      app.invalidated()) {
    requestUpdate();
  }

  if (route) {
    return;
  }

  if (mappedInput.wasReleased(
          MappedInputManager::Button::Back)) {
    if (view ==
        View::TokenGrid) {
      if (stateDirty) {
        saveState();
      }

      activityManager.goHome();

    } else {
      goBackOneLevel();
    }

    return;
  }

  if (mappedInput.wasReleased(
          MappedInputManager::Button::Confirm)) {
    activateCurrentSelection();

    return;
  }

  /*
   * Fixed side buttons.
   */
  buttonNavigator.onPressAndContinuous(
      {MappedInputManager::Button::Up},
      [this] {
        switch (view) {
          case View::TokenGrid:
            moveSelection(-1);
            break;

          case View::EditMenu:
            moveEditSelection(-1);
            break;

          case View::AlphabetPicker:
            moveLetterSelection(-1);
            break;

          case View::TokenPicker:
            moveTokenSelection(-1);
            break;

          case View::StatEditor:
            adjustStatValue(1);
            break;

          case View::CardText:
            break;
        }
      });

  buttonNavigator.onPressAndContinuous(
      {MappedInputManager::Button::Down},
      [this] {
        switch (view) {
          case View::TokenGrid:
            moveSelection(1);
            break;

          case View::EditMenu:
            moveEditSelection(1);
            break;

          case View::AlphabetPicker:
            moveLetterSelection(1);
            break;

          case View::TokenPicker:
            moveTokenSelection(1);
            break;

          case View::StatEditor:
            adjustStatValue(-1);
            break;

          case View::CardText:
            break;
        }
      });

  /*
   * Bottom-right pair changes meaning by screen.
   */
  switch (view) {
    case View::TokenGrid:
      buttonNavigator.onPressAndContinuous(
          {MappedInputManager::Button::Left},
          [this] {
            adjustQuantity(-1);
          });

      buttonNavigator.onPressAndContinuous(
          {MappedInputManager::Button::Right},
          [this] {
            adjustQuantity(1);
          });
      break;

    case View::EditMenu:
      buttonNavigator.onPressAndContinuous(
          {MappedInputManager::Button::Left},
          [this] {
            moveEditSelection(-1);
          });

      buttonNavigator.onPressAndContinuous(
          {MappedInputManager::Button::Right},
          [this] {
            moveEditSelection(1);
          });
      break;

    case View::AlphabetPicker:
      buttonNavigator.onPressAndContinuous(
          {MappedInputManager::Button::Left},
          [this] {
            moveLetterSelection(-1);
          });

      buttonNavigator.onPressAndContinuous(
          {MappedInputManager::Button::Right},
          [this] {
            moveLetterSelection(1);
          });
      break;

    case View::TokenPicker:
      buttonNavigator.onPressAndContinuous(
          {MappedInputManager::Button::Left},
          [this] {
            moveTokenSelection(-1);
          });

      buttonNavigator.onPressAndContinuous(
          {MappedInputManager::Button::Right},
          [this] {
            moveTokenSelection(1);
          });
      break;

    case View::StatEditor:
      buttonNavigator.onPressAndContinuous(
          {MappedInputManager::Button::Left},
          [this] {
            adjustStatValue(-1);
          });

      buttonNavigator.onPressAndContinuous(
          {MappedInputManager::Button::Right},
          [this] {
            adjustStatValue(1);
          });
      break;

    case View::CardText:
      break;
  }
}

void MtgTokenActivity::tokenScreen(
    UiScreen& screen,
    void* user) {
  auto* self =
      static_cast<MtgTokenActivity*>(
          user);

  switch (self->view) {
    case View::TokenGrid:
      self->buildTokenScreen(
          screen);
      break;

    case View::EditMenu:
      self->buildEditScreen(
          screen);
      break;

    case View::AlphabetPicker:
      self->buildAlphabetScreen(
          screen);
      break;

    case View::TokenPicker:
      self->buildTokenPickerScreen(
          screen);
      break;

    case View::StatEditor:
      self->buildStatEditorScreen(
          screen);
      break;

    case View::CardText:
      self->buildCardTextScreen(
          screen);
      break;
  }
}

bool MtgTokenActivity::readLine(
    HalFile& file,
    std::string& line) {
  line.clear();

  while (file.available()) {
    const int value =
        file.read();

    if (value < 0) {
      break;
    }

    const char character =
        static_cast<char>(
            value);

    if (character == '\r') {
      continue;
    }

    if (character == '\n') {
      return true;
    }

    line.push_back(
        character);
  }

  return !line.empty();
}

bool MtgTokenActivity::readTsvRecord(
    HalFile& file,
    std::vector<std::string>& fields) {
  fields.clear();
  fields.reserve(8);

  std::string current;
  current.reserve(256);

  bool inQuotes = false;
  bool quotePending = false;
  bool sawData = false;

  while (file.available()) {
    const int value =
        file.read();

    if (value < 0) {
      break;
    }

    const char character =
        static_cast<char>(
            value);

    sawData = true;

    /*
     * A quote encountered while already quoted can either:
     *
     *   ""  = literal quote
     *   "   = end of quoted field
     *
     * We defer deciding until the next byte arrives.
     */
    if (quotePending) {
      if (character == '"') {
        current.push_back('"');

        quotePending = false;

        continue;
      }

      inQuotes = false;
      quotePending = false;

      // Process the current character again as an
      // unquoted delimiter/newline/value below.
    }

    if (inQuotes) {
      if (character == '"') {
        quotePending = true;
      } else {
        /*
         * This includes embedded '\n' characters.
         * They remain part of the Oracle text field.
         */
        current.push_back(
            character);
      }

      continue;
    }

    if (
        character == '"' &&
        current.empty()) {
      inQuotes = true;

      continue;
    }

    if (character == '\r') {
      continue;
    }

    if (character == '\t') {
      fields.push_back(
          std::move(
              current));

      current.clear();

      continue;
    }

    if (character == '\n') {
      fields.push_back(
          std::move(
              current));

      return true;
    }

    current.push_back(
        character);
  }

  if (
      sawData ||
      !current.empty() ||
      !fields.empty()) {
    fields.push_back(
        std::move(
            current));

    return true;
  }

  return false;
}

bool MtgTokenActivity::loadTokensForLetter(
    const char letter) {
  tokenChoices.clear();

  selectedTokenIndex = 0;
  tokenLoadError = false;

  if (!Storage.ready()) {
    tokenLoadError = true;

    return false;
  }

  char indexPath[32];

  snprintf(
      indexPath,
      sizeof(indexPath),
      "/MTG/index/%c.tsv",
      letter);

  HalFile file =
      Storage.open(
          indexPath);

  if (!file) {
    tokenLoadError = true;

    return false;
  }

  std::vector<std::string> fields;

  /*
   * First logical TSV record is the header.
   *
   * readTsvRecord() understands quoted fields containing
   * actual newline characters, unlike readLine().
   */
  if (!readTsvRecord(
          file,
          fields)) {
    file.close();

    tokenLoadError = true;

    return false;
  }

  while (readTsvRecord(
      file,
      fields)) {
    /*
     * Per-letter index:
     *
     * 0 token_id
     * 1 name
     * 2 power
     * 3 toughness
     * 4 colors
     * 5 art_file
     * 6 oracle_text
     */
    if (fields.size() < 7) {
      continue;
    }

    TokenChoice choice;

    choice.name =
        fields[1];

    choice.power =
        fields[2];

    choice.toughness =
        fields[3];

    choice.colors =
        fields[4];

    choice.artFile =
        fields[5];

    choice.oracleText =
        fields[6];

    if (!choice.name.empty()) {
      tokenChoices.push_back(
          std::move(
              choice));
    }
  }

  file.close();

  return true;
}

void MtgTokenActivity::assignSelectedToken() {
  if (selectedTokenIndex < 0 ||
      selectedTokenIndex >=
          static_cast<int>(
              tokenChoices.size())) {
    return;
  }

  const TokenChoice& choice =
      tokenChoices[
          selectedTokenIndex];

  TokenSlot& slot =
      slots[selectedSlot];

  const bool wasEmpty =
      slot.name.empty();

  slot.name =
      choice.name;

  slot.power =
      choice.power;

  slot.toughness =
      choice.toughness;

  slot.color =
      choice.colors;

  slot.artFile =
      choice.artFile;

  slot.oracleText =
      choice.oracleText;

  /*
   * Populate a brand-new tile at x1.
   * Changing an existing card preserves quantity.
   */
  if (wasEmpty) {
    slot.quantity = 1;
  }

  tokenChoices.clear();

  selectedTokenIndex = 0;
  tokenLoadError = false;

  markStateDirty();
}

bool MtgTokenActivity::drawTokenArt(
    const TokenSlot& slot,
    const fui::Rect& artRect) {
  if (slot.artFile.empty()) {
    return false;
  }

  if (!Storage.ready()) {
    return false;
  }

  HalFile file =
      Storage.open(
          slot.artFile.c_str());

  if (!file) {
    return false;
  }

  Bitmap bitmap(
      file);

  const BmpReaderError parseResult =
      bitmap.parseHeaders();

  if (parseResult !=
      BmpReaderError::Ok) {
    file.close();

    return false;
  }

  if (!bitmap.is1Bit()) {
    file.close();

    return false;
  }

  const int sourceWidth =
      bitmap.getWidth();

  const int sourceHeight =
      bitmap.getHeight();

  if (sourceWidth <= 0 ||
      sourceHeight <= 0 ||
      artRect.width <= 0 ||
      artRect.height <= 0) {
    file.close();

    return false;
  }

  int drawWidth =
      sourceWidth;

  int drawHeight =
      sourceHeight;

  if (
      drawWidth > artRect.width ||
      drawHeight > artRect.height) {
    if (
        sourceWidth *
                artRect.height >
        sourceHeight *
                artRect.width) {
      drawWidth =
          artRect.width;

      drawHeight =
          sourceHeight *
          artRect.width /
          sourceWidth;
    } else {
      drawHeight =
          artRect.height;

      drawWidth =
          sourceWidth *
          artRect.height /
          sourceHeight;
    }
  }

  const int drawX =
      artRect.x +
      (artRect.width -
       drawWidth) /
          2;

  const int drawY =
      artRect.y +
      (artRect.height -
       drawHeight) /
          2;

  renderer.drawBitmap1Bit(
      bitmap,
      drawX,
      drawY,
      drawWidth,
      drawHeight);

  file.close();

  return true;
}

void MtgTokenActivity::buildEditScreen(
    UiScreen& screen) {
  screen.header(
      "Edit Token");

  const auto labels =
      mappedInput.mapLabels(
          "Back",
          "Select",
          "Up",
          "Down");

  const fui::FooterAction footer[] = {
      {labels.btn1},
      {labels.btn2},
      {labels.btn3},
      {labels.btn4},
  };

  screen.footer(
      footer,
      4);

  /*
   * Seven rows easily fit on the X3 and give us one
   * centralized menu for all token operations.
   */
  screen.insetContent(
      fui::makeInsets(6));

  screen.button(
      "Edit Power",
      ACTION_EDIT_ITEM,
      0,
      selectedEditItem == 0
          ? fui::StateSelected
          : fui::StateNormal);

  screen.button(
      "Edit Toughness",
      ACTION_EDIT_ITEM,
      1,
      selectedEditItem == 1
          ? fui::StateSelected
          : fui::StateNormal);

  screen.button(
      "Edit Color",
      ACTION_EDIT_ITEM,
      2,
      selectedEditItem == 2
          ? fui::StateSelected
          : fui::StateNormal);

  screen.button(
      "Change Card",
      ACTION_EDIT_ITEM,
      3,
      selectedEditItem == 3
          ? fui::StateSelected
          : fui::StateNormal);

  screen.button(
      "Read Card Text",
      ACTION_EDIT_ITEM,
      4,
      selectedEditItem == 4
          ? fui::StateSelected
          : fui::StateNormal);

  screen.button(
      "Clear Card",
      ACTION_EDIT_ITEM,
      5,
      selectedEditItem == 5
          ? fui::StateSelected
          : fui::StateNormal);

  screen.button(
      "Clear Board",
      ACTION_EDIT_ITEM,
      6,
      selectedEditItem == 6
          ? fui::StateSelected
          : fui::StateNormal);
}

void MtgTokenActivity::buildStatEditorScreen(
    UiScreen& screen) {
  const char* title =
      editingStat ==
              StatField::Power
          ? "Edit Power"
          : "Edit Toughness";

  screen.header(
      title);

  const auto labels =
      mappedInput.mapLabels(
          "Back",
          "Save",
          "-1",
          "+1");

  const fui::FooterAction footer[] = {
      {labels.btn1},
      {labels.btn2},
      {labels.btn3},
      {labels.btn4},
  };

  screen.footer(
      footer,
      4);

  screen.insetContent(
      fui::makeInsets(16));

  const fui::Rect body =
      screen.body();

  auto& target =
      screen.target();

  fui::TextStyle tokenStyle =
      screen.theme().bodyText;

  tokenStyle.align =
      fui::TextAlign::Center;

  fui::TextStyle valueStyle =
      screen.theme().titleText;

  valueStyle.align =
      fui::TextAlign::Center;

  valueStyle.bold =
      true;

  const int tokenHeight =
      target.lineHeight(
          tokenStyle.font) +
      8;

  const fui::Rect tokenRect =
      fui::makeRect(
          body.x,
          body.y,
          body.width,
          tokenHeight);

  const fui::Rect valueRect =
      fui::makeRect(
          body.x,
          tokenRect.bottom() + 16,
          body.width,
          body.height -
              tokenHeight -
              16);

  const char* tokenName =
      slots[selectedSlot]
              .name
              .empty()
          ? "EMPTY"
          : slots[selectedSlot]
                .name
                .c_str();

  target.text(
      tokenRect,
      tokenName,
      tokenStyle);

  char valueText[16];

  snprintf(
      valueText,
      sizeof(valueText),
      "%d",
      editStatValue);

  target.text(
      valueRect,
      valueText,
      valueStyle);
}

void MtgTokenActivity::buildCardTextScreen(
    UiScreen& screen) {
  screen.header(
      "Card Text");

  const auto labels =
      mappedInput.mapLabels(
          "Back",
          "",
          "",
          "");

  const fui::FooterAction footer[] = {
      {labels.btn1},
      {labels.btn2},
      {labels.btn3},
      {labels.btn4},
  };

  screen.footer(
      footer,
      4);

  screen.insetContent(
      fui::makeInsets(12));

  const fui::Rect body =
      screen.body();

  auto& target =
      screen.target();

  const TokenSlot& slot =
      slots[selectedSlot];

  fui::TextStyle nameStyle =
      screen.theme().titleText;

  nameStyle.align =
      fui::TextAlign::Center;

  nameStyle.bold =
      true;

  const int nameHeight =
      target.lineHeight(
          nameStyle.font) +
      10;

  const fui::Rect nameRect =
      fui::makeRect(
          body.x,
          body.y,
          body.width,
          nameHeight);

  target.text(
      nameRect,
      slot.name.empty()
          ? "EMPTY"
          : slot.name.c_str(),
      nameStyle);

  const fui::Rect textRect =
      fui::makeRect(
          body.x,
          nameRect.bottom() + 8,
          body.width,
          body.bottom() -
              nameRect.bottom() -
              8);

  fui::TextStyle textStyle =
      screen.theme().bodyText;

  textStyle.align =
      fui::TextAlign::Left;

  /*
   * FreeInkUI wraps words and also honors the original
   * Scryfall '\n' hard breaks.
   *
   * FreeInkUI currently caps wrapped text at 16 lines.
   */
  textStyle.maxLines = 16;

  if (slot.oracleText.empty()) {
    textStyle.align =
        fui::TextAlign::Center;

    target.text(
        textRect,
        "No card text available",
        textStyle);

    return;
  }

  target.text(
      textRect,
      slot.oracleText.c_str(),
      textStyle);
}

void MtgTokenActivity::buildAlphabetScreen(
    UiScreen& screen) {
  screen.header(
      "Change Card");

  const auto labels =
      mappedInput.mapLabels(
          "Back",
          "Select",
          "Left",
          "Right");

  const fui::FooterAction footer[] = {
      {labels.btn1},
      {labels.btn2},
      {labels.btn3},
      {labels.btn4},
  };

  screen.footer(
      footer,
      4);

  screen.insetContent(
      fui::makeInsets(8));

  const fui::Rect body =
      screen.body();

  constexpr int COLUMN_COUNT = 5;
  constexpr int ROW_COUNT = 6;
  constexpr int GAP = 6;

  const int cellWidth =
      (body.width -
       ((COLUMN_COUNT - 1) *
        GAP)) /
      COLUMN_COUNT;

  const int cellHeight =
      (body.height -
       ((ROW_COUNT - 1) *
        GAP)) /
      ROW_COUNT;

  auto& target =
      screen.target();

  fui::TextStyle letterStyle =
      screen.theme().titleText;

  letterStyle.align =
      fui::TextAlign::Center;

  letterStyle.bold =
      true;

  const fui::Paint black =
      fui::Paint::solid(
          fui::Color::Black);

  for (int i = 0;
       i < LETTER_COUNT;
       ++i) {
    const int column =
        i % COLUMN_COUNT;

    const int row =
        i / COLUMN_COUNT;

    const int x =
        body.x +
        column *
            (cellWidth + GAP);

    const int y =
        body.y +
        row *
            (cellHeight + GAP);

    const fui::Rect cell =
        fui::makeRect(
            x,
            y,
            cellWidth,
            cellHeight);

    const bool selected =
        i ==
        selectedLetter;

    target.stroke(
        cell,
        black,
        selected ? 3 : 1,
        4);

    char letter[2] = {
        static_cast<char>(
            'A' + i),
        '\0',
    };

    target.text(
        cell,
        letter,
        letterStyle);
  }
}

void MtgTokenActivity::buildTokenPickerScreen(
    UiScreen& screen) {
  const char letter =
      static_cast<char>(
          'A' +
          selectedLetter);

  char headerText[32];

  snprintf(
      headerText,
      sizeof(headerText),
      "%c Tokens",
      letter);

  screen.header(
      headerText);

  const auto labels =
      mappedInput.mapLabels(
          "Back",
          "Select",
          "Up",
          "Down");

  const fui::FooterAction footer[] = {
      {labels.btn1},
      {labels.btn2},
      {labels.btn3},
      {labels.btn4},
  };

  screen.footer(
      footer,
      4);

  screen.insetContent(
      fui::makeInsets(8));

  const fui::Rect body =
      screen.body();

  auto& target =
      screen.target();

  if (tokenLoadError) {
    fui::TextStyle errorStyle =
        screen.theme().bodyText;

    errorStyle.align =
        fui::TextAlign::Center;

    errorStyle.bold =
        true;

    target.text(
        body,
        "Could not read token index",
        errorStyle);

    return;
  }

  if (tokenChoices.empty()) {
    fui::TextStyle emptyStyle =
        screen.theme().bodyText;

    emptyStyle.align =
        fui::TextAlign::Center;

    target.text(
        body,
        "No tokens found",
        emptyStyle);

    return;
  }

  fui::TextStyle rowStyle =
      screen.theme().bodyText;

  rowStyle.align =
      fui::TextAlign::Left;

  const fui::Paint black =
      fui::Paint::solid(
          fui::Color::Black);

  const int totalTokens =
      static_cast<int>(
          tokenChoices.size());

  const int pageStart =
      (selectedTokenIndex /
       TOKEN_ROWS_PER_PAGE) *
      TOKEN_ROWS_PER_PAGE;

  const int rowHeight =
      body.height /
      TOKEN_ROWS_PER_PAGE;

  for (int row = 0;
       row <
           TOKEN_ROWS_PER_PAGE;
       ++row) {
    const int tokenIndex =
        pageStart + row;

    if (tokenIndex >=
        totalTokens) {
      break;
    }

    const int y =
        body.y +
        row *
            rowHeight;

    const int height =
        row ==
                TOKEN_ROWS_PER_PAGE -
                    1
            ? body.bottom() - y
            : rowHeight;

    const fui::Rect rowRect =
        fui::makeRect(
            body.x,
            y,
            body.width,
            height);

    const bool selected =
        tokenIndex ==
        selectedTokenIndex;

    target.stroke(
        rowRect,
        black,
        selected ? 3 : 1,
        2);

    const fui::Rect textRect =
        rowRect.inset(
            fui::Insets{
                10,
                4,
                10,
                4});

    target.text(
        textRect,
        tokenChoices[
            tokenIndex]
            .name
            .c_str(),
        rowStyle);
  }
}

void MtgTokenActivity::buildTokenScreen(
    UiScreen& screen) {
  screen.header(
      "MTG Tokens");

  const auto labels =
      mappedInput.mapLabels(
          "Back",
          "Edit",
          "-1",
          "+1");

  const fui::FooterAction footer[] = {
      {labels.btn1},
      {labels.btn2},
      {labels.btn3},
      {labels.btn4},
  };

  screen.footer(
      footer,
      4);

  screen.insetContent(
      fui::makeInsets(4));

  const fui::Rect body =
      screen.body();

  constexpr int gap = 4;

  const int cellWidth =
      (body.width - gap) /
      2;

  const int cellHeight =
      (body.height - gap) /
      2;

  auto& target =
      screen.target();

  fui::TextStyle nameStyle =
      screen.theme().bodyText;

  nameStyle.align =
      fui::TextAlign::Center;

  nameStyle.bold =
      true;

  fui::TextStyle artStyle =
      screen.theme().smallText;

  artStyle.align =
      fui::TextAlign::Center;

  fui::TextStyle statStyle =
      screen.theme().bodyText;

  statStyle.align =
      fui::TextAlign::Center;

  statStyle.bold =
      true;

  fui::TextStyle quantityStyle =
      screen.theme().titleText;

  quantityStyle.align =
      fui::TextAlign::Center;

  quantityStyle.bold =
      true;

  fui::TextStyle emptyStyle =
      screen.theme().bodyText;

  emptyStyle.align =
      fui::TextAlign::Center;

  const fui::Paint black =
      fui::Paint::solid(
          fui::Color::Black);

  for (int i = 0;
       i < SLOT_COUNT;
       ++i) {
    const int column =
        i % 2;

    const int row =
        i / 2;

    const int x =
        body.x +
        column *
            (cellWidth + gap);

    const int y =
        body.y +
        row *
            (cellHeight + gap);

    const int width =
        column == 1
            ? body.right() - x
            : cellWidth;

    const int height =
        row == 1
            ? body.bottom() - y
            : cellHeight;

    const fui::Rect cell =
        fui::makeRect(
            x,
            y,
            width,
            height);

    const bool selected =
        i ==
        selectedSlot;

    screen.frame().hit(
        cell,
        ACTION_SELECT_SLOT,
        static_cast<int16_t>(
            i),
        fui::InputTouch,
        selected
            ? fui::StateSelected
            : fui::StateNormal);

    target.stroke(
        cell,
        black,
        selected ? 3 : 1,
        4);

    const fui::Rect inner =
        cell.inset(
            fui::Insets{
                4,
                4,
                4,
                4});

    const int nameHeight =
        target.lineHeight(
            nameStyle.font) +
        2;

    const int statHeight =
        target.lineHeight(
            statStyle.font) +
        2;

    const int quantityHeight =
        target.lineHeight(
            quantityStyle.font) +
        2;

    const fui::Rect nameRect =
        fui::makeRect(
            inner.x,
            inner.y,
            inner.width,
            nameHeight);

    const int quantityY =
        inner.bottom() -
        quantityHeight;

    const fui::Rect quantityRect =
        fui::makeRect(
            inner.x,
            quantityY,
            inner.width,
            quantityHeight);

    const int statY =
        quantityY -
        statHeight;

    const fui::Rect statRect =
        fui::makeRect(
            inner.x,
            statY,
            inner.width,
            statHeight);

    const int artY =
        nameRect.bottom() +
        2;

    const int artHeight =
        statY -
        artY -
        2;

    const fui::Rect artRect =
        fui::makeRect(
            inner.x,
            artY,
            inner.width,
            artHeight);

    const TokenSlot& slot =
        slots[i];

    if (slot.name.empty()) {
      target.text(
          nameRect,
          "EMPTY",
          nameStyle);

      target.text(
          artRect,
          "Edit to add token",
          emptyStyle);

      continue;
    }

    target.text(
        nameRect,
        slot.name.c_str(),
        nameStyle);

    if (artHeight > 10) {
      target.stroke(
          artRect,
          black,
          1,
          2);

      const bool artDrawn =
          drawTokenArt(
              slot,
              artRect);

      if (!artDrawn) {
        target.text(
            artRect,
            "ART",
            artStyle);
      }
    }

    if (!slot.power.empty() ||
        !slot.toughness.empty()) {
      char statText[32];

      snprintf(
          statText,
          sizeof(statText),
          "%s/%s",
          slot.power.c_str(),
          slot.toughness.c_str());

      target.text(
          statRect,
          statText,
          statStyle);
    }

    char quantityText[16];

    snprintf(
        quantityText,
        sizeof(quantityText),
        "x %d",
        slot.quantity);

    target.text(
        quantityRect,
        quantityText,
        quantityStyle);
  }
}

void MtgTokenActivity::render(
    RenderLock&&) {
  renderer.clearScreen();

  renderUi();

  renderer.displayBuffer();
}