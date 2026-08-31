#include "MtgTokenActivity.h"

#include <cstdio>

namespace fui = freeink::ui;

MtgTokenActivity::MtgTokenActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("MtgTokens", renderer, mappedInput), UiAppHost(renderer) {}

void MtgTokenActivity::onEnter() {
  Activity::onEnter();

  resetUi();

  app.on(ACTION_SELECT_SLOT, &MtgTokenActivity::onTokenSelected, this);
  app.setScreen(&MtgTokenActivity::tokenScreen, this);

  requestUpdate();
}

void MtgTokenActivity::onExit() {
  Activity::onExit();
}

void MtgTokenActivity::moveSelection(const int delta) {
  selectedSlot += delta;

  if (selectedSlot < 0) {
    selectedSlot = SLOT_COUNT - 1;
  } else if (selectedSlot >= SLOT_COUNT) {
    selectedSlot = 0;
  }

  requestUpdate();
}

void MtgTokenActivity::adjustQuantity(const int delta) {
  int next = slots[selectedSlot].quantity + delta;

  if (next < 0) {
    next = 0;
  } else if (next > 999) {
    next = 999;
  }

  if (next == slots[selectedSlot].quantity) {
    return;
  }

  slots[selectedSlot].quantity = next;
  requestUpdate();
}

void MtgTokenActivity::onTokenSelected(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<MtgTokenActivity*>(user);

  if (event.value < 0 || event.value >= SLOT_COUNT) {
    return;
  }

  self->selectedSlot = event.value;
  self->requestUpdate();
}

void MtgTokenActivity::loop() {
  const auto route = routeTouch(mappedInput);

  if (route.routed && app.invalidated()) {
    requestUpdate();
  }

  if (route) {
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    activityManager.goHome();
    return;
  }

  buttonNavigator.onPrevious([this] {
    moveSelection(-1);
  });

  buttonNavigator.onNext([this] {
    moveSelection(1);
  });

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
}

void MtgTokenActivity::tokenScreen(UiScreen& screen, void* user) {
  static_cast<MtgTokenActivity*>(user)->buildTokenScreen(screen);
}

void MtgTokenActivity::buildTokenScreen(UiScreen& screen) {
  screen.header("MTG Tokens");

  const fui::FooterAction footer[] = {
      {"Back"},
      {""},
      {"-1"},
      {"+1"},
  };

  screen.footer(footer, 4);
  screen.insetContent(fui::makeInsets(8));

  const fui::Rect body = screen.body();

  constexpr int gap = 8;

  const int cellWidth = (body.width - gap) / 2;
  const int cellHeight = (body.height - gap) / 2;

  auto& target = screen.target();

  fui::TextStyle nameStyle = screen.theme().bodyText;
  nameStyle.align = fui::TextAlign::Center;
  nameStyle.bold = true;

  fui::TextStyle artStyle = screen.theme().smallText;
  artStyle.align = fui::TextAlign::Center;

  fui::TextStyle statStyle = screen.theme().bodyText;
  statStyle.align = fui::TextAlign::Center;
  statStyle.bold = true;

  fui::TextStyle quantityStyle = screen.theme().titleText;
  quantityStyle.align = fui::TextAlign::Center;
  quantityStyle.bold = true;

  const fui::Paint black = fui::Paint::solid(fui::Color::Black);

  for (int i = 0; i < SLOT_COUNT; ++i) {
    const int column = i % 2;
    const int row = i / 2;

    const int x = body.x + column * (cellWidth + gap);
    const int y = body.y + row * (cellHeight + gap);

    const int width =
        column == 1 ? body.right() - x : cellWidth;

    const int height =
        row == 1 ? body.bottom() - y : cellHeight;

    const fui::Rect cell = fui::makeRect(x, y, width, height);
    const bool selected = i == selectedSlot;

    screen.frame().hit(
        cell,
        ACTION_SELECT_SLOT,
        static_cast<int16_t>(i),
        fui::InputTouch,
        selected ? fui::StateSelected : fui::StateNormal);

    target.stroke(cell, black, selected ? 3 : 1, 4);

    const fui::Rect inner =
        cell.inset(fui::Insets{8, 8, 8, 8});

    const int nameHeight =
        target.lineHeight(nameStyle.font) + 6;

    const int statHeight =
        target.lineHeight(statStyle.font) + 4;

    const int quantityHeight =
        target.lineHeight(quantityStyle.font) + 4;

    const fui::Rect nameRect =
        fui::makeRect(
            inner.x,
            inner.y,
            inner.width,
            nameHeight);

    const int quantityY =
        inner.bottom() - quantityHeight;

    const fui::Rect quantityRect =
        fui::makeRect(
            inner.x,
            quantityY,
            inner.width,
            quantityHeight);

    const int statY =
        quantityY - statHeight;

    const fui::Rect statRect =
        fui::makeRect(
            inner.x,
            statY,
            inner.width,
            statHeight);

    const int artY =
        nameRect.bottom() + 4;

    const int artHeight =
        statY - artY - 4;

    const fui::Rect artRect =
        fui::makeRect(
            inner.x,
            artY,
            inner.width,
            artHeight);

    target.text(nameRect, slots[i].name, nameStyle);

    if (artHeight > 10) {
      target.stroke(artRect, black, 1, 2);
      target.text(artRect, "ART", artStyle);
    }

    if (slots[i].stats[0] != '\0') {
      target.text(statRect, slots[i].stats, statStyle);
    }

    char quantityText[16];
    snprintf(
        quantityText,
        sizeof(quantityText),
        "x %d",
        slots[i].quantity);

    target.text(
        quantityRect,
        quantityText,
        quantityStyle);
  }
}

void MtgTokenActivity::render(RenderLock&&) {
  renderer.clearScreen();

  renderUi();

  renderer.displayBuffer();
}