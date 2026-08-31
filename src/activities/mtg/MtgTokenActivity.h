#pragma once

#include "MappedInputManager.h"
#include "activities/Activity.h"
#include "components/UiAppHost.h"
#include "util/ButtonNavigator.h"

class MtgTokenActivity final : public Activity, private UiAppHost {
 public:
  explicit MtgTokenActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  struct TokenSlot {
    const char* name;
    const char* stats;
    int quantity;
  };

  static constexpr int SLOT_COUNT = 4;

  TokenSlot slots[SLOT_COUNT] = {
      {"PLANT", "0/1", 12},
      {"ELEMENTAL", "4/4", 3},
      {"TREASURE", "", 8},
      {"BEAST", "4/4", 2},
  };

  int selectedSlot = 0;

  ButtonNavigator buttonNavigator;

  static constexpr freeink::ui::ActionId ACTION_SELECT_SLOT = 1;

  static void tokenScreen(UiScreen& screen, void* user);
  static void onTokenSelected(const freeink::ui::ActionEvent& event, void* user);

  void buildTokenScreen(UiScreen& screen);
  void moveSelection(int delta);
  void adjustQuantity(int delta);
};