#pragma once

#include <HalStorage.h>

#include <string>
#include <vector>

#include "MappedInputManager.h"
#include "activities/Activity.h"
#include "components/UiAppHost.h"
#include "util/ButtonNavigator.h"

class MtgTokenActivity final : public Activity, private UiAppHost {
 public:
  explicit MtgTokenActivity(
      GfxRenderer& renderer,
      MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class View {
    TokenGrid,
    EditMenu,
    AlphabetPicker,
    TokenPicker,
    StatEditor,
    CardText,
  };

  enum class StatField {
    None,
    Power,
    Toughness,
  };

  struct TokenSlot {
    std::string name;
    std::string power;
    std::string toughness;
    std::string color;
    std::string artFile;
    std::string oracleText;
    int quantity;
  };

  struct TokenChoice {
    std::string name;
    std::string power;
    std::string toughness;
    std::string colors;
    std::string artFile;
    std::string oracleText;
  };

  static constexpr int SLOT_COUNT = 4;
  static constexpr int EDIT_ITEM_COUNT = 6;
  static constexpr int LETTER_COUNT = 26;
  static constexpr int TOKEN_ROWS_PER_PAGE = 8;

  static constexpr unsigned long STATE_SAVE_DEBOUNCE_MS = 750;

  static constexpr const char* SESSION_DIRECTORY =
      "/MTG/session";

  static constexpr const char* SESSION_PATH =
      "/MTG/session/state.tsv";

  TokenSlot slots[SLOT_COUNT] = {
      {"", "", "", "", "", "", 0},
      {"", "", "", "", "", "", 0},
      {"", "", "", "", "", "", 0},
      {"", "", "", "", "", "", 0},
  };

  View view = View::TokenGrid;
  StatField editingStat = StatField::None;

  int selectedSlot = 0;
  int selectedEditItem = 0;
  int selectedLetter = 0;
  int selectedTokenIndex = 0;

  int editStatValue = 0;

  bool tokenLoadError = false;

  bool stateDirty = false;
  unsigned long stateDirtyAt = 0;

  std::vector<TokenChoice> tokenChoices;

  ButtonNavigator buttonNavigator;

  static constexpr freeink::ui::ActionId ACTION_SELECT_SLOT = 1;
  static constexpr freeink::ui::ActionId ACTION_EDIT_ITEM = 2;

  static void tokenScreen(
      UiScreen& screen,
      void* user);

  static void onTokenSelected(
      const freeink::ui::ActionEvent& event,
      void* user);

  static void onEditItemSelected(
      const freeink::ui::ActionEvent& event,
      void* user);

  void buildTokenScreen(UiScreen& screen);
  void buildEditScreen(UiScreen& screen);
  void buildAlphabetScreen(UiScreen& screen);
  void buildTokenPickerScreen(UiScreen& screen);
  void buildStatEditorScreen(UiScreen& screen);
  void buildCardTextScreen(UiScreen& screen);

  void moveSelection(int delta);
  void adjustQuantity(int delta);

  void moveEditSelection(int delta);
  void moveLetterSelection(int delta);
  void moveTokenSelection(int delta);

  void beginStatEdit(StatField field);
  void adjustStatValue(int delta);
  void commitStatEdit();

  void activateCurrentSelection();
  void goBackOneLevel();
  void setView(View nextView);

  bool loadTokensForLetter(char letter);
  void assignSelectedToken();

  bool drawTokenArt(
      const TokenSlot& slot,
      const freeink::ui::Rect& artRect);

  void resetSlotsToEmpty();
  void clearSelectedCard();
  void clearBoard();

  void markStateDirty();
  void maybeSaveState();

  bool saveState();
  bool loadState();

  static int parseStatValue(
      const std::string& value);

  static bool readLine(
      HalFile& file,
      std::string& line);

  static bool readTsvRecord(
      HalFile& file,
      std::vector<std::string>& fields);

  static std::string escapeSessionField(
      const std::string& value);

  static std::string unescapeSessionField(
      const std::string& value);

  static bool splitSessionLine(
      const std::string& line,
      size_t expectedFields,
      std::vector<std::string>& fields);
};