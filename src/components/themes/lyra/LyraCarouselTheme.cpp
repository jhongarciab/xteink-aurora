#include "LyraCarouselTheme.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalStorage.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "ReadingStatsStore.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "components/icons/book.h"
#include "components/icons/book24.h"
#include "components/icons/chart.h"
#include "components/icons/cover.h"
#include "components/icons/file24.h"
#include "components/icons/folder.h"
#include "components/icons/folder24.h"
#include "components/icons/heart.h"
#include "components/icons/heart24.h"
#include "components/icons/hotspot.h"
#include "components/icons/image24.h"
#include "components/icons/library.h"
#include "components/icons/recent.h"
#include "components/icons/settings.h"
#include "components/icons/settings2.h"
#include "components/icons/text24.h"
#include "components/icons/trophy.h"
#include "components/icons/trophy24.h"
#include "components/icons/transfer.h"
#include "components/icons/wifi.h"
#include "fontIds.h"

namespace {
constexpr int kCoverTopPad = 10;
constexpr int kTitleFontId = UI_12_FONT_ID;
constexpr int kDotSize = 8;
constexpr int kDotGap = 6;
constexpr int kCornerRadius = 6;
constexpr int kThinOutlineW = 1;
constexpr int kSelectionLineW = 3;
constexpr int kCenterOutlineW = 4;
constexpr int kTitleTopClearance = 4;
constexpr int kTitleBottomGap = 3;
constexpr int kTitleWidth = 480;
constexpr int kFooterTopGap = 4;
constexpr int kFooterLabelToBarGap = 3;
constexpr int kFooterProgressBarHeight = 5;
constexpr int kFooterPercentTopGap = 2;
constexpr int kNearSideW = 145;
constexpr int kNearSideLeftH = 430;
constexpr int kNearSideRightH = 375;
constexpr int kMenuIconSize = 32;
constexpr int kMenuIconPad = 14;
constexpr int kHighlightPad = 12;
constexpr int kVisibleMenuSlots = 5;

int lastCarouselSelectorIndex = -1;

const uint8_t* iconForName(UIIcon icon, int size) {
  if (size == 24) {
    switch (icon) {
      case UIIcon::Folder:
        return Folder24Icon;
      case UIIcon::Text:
        return Text24Icon;
      case UIIcon::Image:
        return Image24Icon;
      case UIIcon::Book:
        return Book24Icon;
      case UIIcon::File:
        return File24Icon;
      case UIIcon::Trophy:
        return Trophy24Icon;
      case UIIcon::Heart:
        return Heart24Icon;
      default:
        return nullptr;
    }
  }

  if (size == 32) {
    switch (icon) {
      case UIIcon::Folder:
        return FolderIcon;
      case UIIcon::Book:
        return BookIcon;
      case UIIcon::Chart:
        return ChartIcon;
      case UIIcon::Recent:
        return RecentIcon;
      case UIIcon::Settings:
        return Settings2Icon;
      case UIIcon::Apps:
        return SettingsIcon;
      case UIIcon::Transfer:
        return TransferIcon;
      case UIIcon::Library:
        return LibraryIcon;
      case UIIcon::Trophy:
        return TrophyIcon;
      case UIIcon::Wifi:
        return WifiIcon;
      case UIIcon::Hotspot:
        return HotspotIcon;
      case UIIcon::Heart:
        return HeartIcon;
      default:
        return nullptr;
    }
  }

  return nullptr;
}

void drawCoverPlaceholder(GfxRenderer& renderer, int x, int y, int maxW, int maxH) {
  renderer.drawRoundedRect(x, y, maxW, maxH, 1, kCornerRadius, true);
  renderer.fillRoundedRect(x, y + maxH / 3, maxW, 2 * maxH / 3, kCornerRadius, false, false, true, true,
                           Color::Black);
  renderer.drawIcon(CoverIcon, x + maxW / 2 - 16, y + 8, 32, 32);
}

const ReadingBookStats* getBookStats(const RecentBook& book) {
  const ReadingBookStats* stats = nullptr;
  if (!book.bookId.empty()) stats = READING_STATS.findBook(book.bookId);
  if (!stats && !book.path.empty()) stats = READING_STATS.findBook(book.path);
  if (!stats && !book.path.empty()) {
    stats = READING_STATS.findMatchingBookForPath(book.path, book.title, book.author);
  }
  return stats;
}

void drawPerspectiveBitmap(GfxRenderer& renderer, Bitmap& bitmap, const int x, const int y, const int width,
                           const int leftHeight, const int rightHeight) {
  if (width <= 0 || leftHeight <= 0 || rightHeight <= 0 || bitmap.getWidth() <= 0 || bitmap.getHeight() <= 0) return;

  const int outputRowSize = (bitmap.getWidth() + 3) / 4;
  auto* outputRow = static_cast<uint8_t*>(malloc(outputRowSize));
  auto* rowBytes = static_cast<uint8_t*>(malloc(bitmap.getRowBytes()));
  if (!outputRow || !rowBytes) {
    free(outputRow);
    free(rowBytes);
    return;
  }

  const int maxHeight = std::max(leftHeight, rightHeight);
  for (int row = 0; row < bitmap.getHeight(); ++row) {
    if (bitmap.readNextRow(outputRow, rowBytes) != BmpReaderError::Ok) break;
    const int sourceY = bitmap.isTopDown() ? row : bitmap.getHeight() - 1 - row;
    for (int sourceX = 0; sourceX < bitmap.getWidth(); ++sourceX) {
      const uint8_t value = (outputRow[sourceX / 4] >> (6 - ((sourceX * 2) % 8))) & 0x3;
      if (value >= 3) continue;
      const int destX = (sourceX * width) / bitmap.getWidth();
      const int columnHeight = leftHeight + ((rightHeight - leftHeight) * destX) / std::max(1, width - 1);
      const int top = (maxHeight - columnHeight) / 2;
      const int destY = y + top + (sourceY * columnHeight) / bitmap.getHeight();
      renderer.drawPixel(x + destX, destY, true);
    }
  }
  free(outputRow);
  free(rowBytes);
}
}  // namespace

void LyraCarouselTheme::setPreRenderIndex(int index) { lastCarouselSelectorIndex = index; }

void LyraCarouselTheme::drawRecentBookCover(GfxRenderer& renderer, Rect rect,
                                            const std::vector<RecentBook>& recentBooks, const int selectorIndex,
                                            bool& coverRendered, bool& coverBufferStored, bool& bufferRestored,
                                            std::function<bool()> storeCoverBuffer) const {
  (void)bufferRestored;
  if (recentBooks.empty()) {
    drawEmptyRecents(renderer, rect);
    return;
  }

  const int bookCount = static_cast<int>(recentBooks.size());
  const bool inCarouselRow = selectorIndex < bookCount;
  int centerIdx = inCarouselRow ? selectorIndex : (lastCarouselSelectorIndex >= 0 ? lastCarouselSelectorIndex : 0);
  centerIdx = std::max(0, std::min(centerIdx, bookCount - 1));
  if (centerIdx != lastCarouselSelectorIndex) {
    coverRendered = false;
    coverBufferStored = false;
  }

  const int screenW = renderer.getScreenWidth();
  const int titleLineHeight = renderer.getLineHeight(kTitleFontId);
  const int authorLineHeight = renderer.getLineHeight(SMALL_FONT_ID);
  const std::string title = renderer.truncatedText(kTitleFontId, recentBooks[centerIdx].title.c_str(), kTitleWidth,
                                                   EpdFontFamily::BOLD);
  const int titleBlockHeight = titleLineHeight;
  const int titleY = rect.y + kTitleTopClearance;
  const int authorY = titleY + titleBlockHeight + 2;
  const int centerTileY = std::max(rect.y + kCoverTopPad, authorY + authorLineHeight + kTitleBottomGap);
  const int centerX = (screenW - kCenterCoverW) / 2;
  const int sideTileY = centerTileY + (kCenterCoverH - std::max(kNearSideLeftH, kNearSideRightH)) / 2;
  const int leftNearX = std::max(0, centerX - kNearSideW + 6);
  const int rightNearX = std::min(screenW - kNearSideW, centerX + kCenterCoverW - 6);

  auto drawCover = [&](int bookIdx, int x, int y, int maxW, int maxH) -> bool {
    if (bookIdx < 0 || bookIdx >= bookCount) return false;
    const RecentBook& book = recentBooks[bookIdx];
    bool hasCover = false;
    if (!book.coverBmpPath.empty()) {
      std::string thumbPath = UITheme::getCoverThumbPath(book.coverBmpPath, maxW, maxH);
      const std::string centerThumbPath =
          UITheme::getCoverThumbPath(book.coverBmpPath, kCenterCoverW, kCenterCoverH);
      const std::string legacyThumbPath =
          UITheme::getCoverThumbPath(book.coverBmpPath, LyraCarouselMetrics::values.homeCoverHeight);
      if (!Storage.exists(thumbPath.c_str())) {
        if (Storage.exists(centerThumbPath.c_str())) {
          thumbPath = centerThumbPath;
        } else if (Storage.exists(legacyThumbPath.c_str())) {
          thumbPath = legacyThumbPath;
        }
      }
      FsFile file;
      if (Storage.openFileForRead("HOME", thumbPath, file)) {
        Bitmap bitmap(file);
        if (bitmap.parseHeaders() == BmpReaderError::Ok) {
          const float bmpRatio = static_cast<float>(bitmap.getWidth()) / static_cast<float>(bitmap.getHeight());
          const float tileRatio = static_cast<float>(maxW) / static_cast<float>(maxH);
          const float cropX = (bmpRatio > tileRatio) ? (1.0f - tileRatio / bmpRatio) : 0.0f;
          renderer.drawBitmap(bitmap, x, y, maxW, maxH, cropX, 0.0f);
          renderer.maskRoundedRectOutsideCorners(x, y, maxW, maxH, kCornerRadius, Color::White);
          hasCover = true;
        }
        file.close();
      }
    }
    if (!hasCover) {
      drawCoverPlaceholder(renderer, x, y, maxW, maxH);
    }
    return true;
  };

  auto drawSideCover = [&](int bookIdx, const int x, const int width, const int leftHeight,
                           const int rightHeight) -> bool {
    if (bookIdx < 0 || bookIdx >= bookCount) return false;
    const RecentBook& book = recentBooks[bookIdx];
    if (!book.coverBmpPath.empty()) {
      std::string thumbPath = UITheme::getCoverThumbPath(book.coverBmpPath, width, std::max(leftHeight, rightHeight));
      // Side thumbnails may not exist yet: reuse the center/legacy thumbnail
      // generated by the Home cover loader instead of falling back immediately.
      if (!Storage.exists(thumbPath.c_str())) {
        const std::string centerThumbPath = UITheme::getCoverThumbPath(book.coverBmpPath, kCenterCoverW, kCenterCoverH);
        const std::string legacyThumbPath =
            UITheme::getCoverThumbPath(book.coverBmpPath, LyraCarouselMetrics::values.homeCoverHeight);
        if (Storage.exists(centerThumbPath.c_str())) {
          thumbPath = centerThumbPath;
        } else if (Storage.exists(legacyThumbPath.c_str())) {
          thumbPath = legacyThumbPath;
        }
      }
      FsFile file;
      if (Storage.openFileForRead("HOME", thumbPath, file)) {
        Bitmap bitmap(file);
        if (bitmap.parseHeaders() == BmpReaderError::Ok) {
          renderer.fillRect(x, sideTileY, width, std::max(leftHeight, rightHeight), false);
          drawPerspectiveBitmap(renderer, bitmap, x, sideTileY, width, leftHeight, rightHeight);
          renderer.maskRoundedRectOutsideCorners(x, sideTileY, width, std::max(leftHeight, rightHeight), kCornerRadius,
                                                 Color::White);
          file.close();
          renderer.drawLine(x, sideTileY + (std::max(leftHeight, rightHeight) - leftHeight) / 2,
                            x + width - 1, sideTileY + (std::max(leftHeight, rightHeight) - rightHeight) / 2,
                            kThinOutlineW, true);
          return true;
        }
        file.close();
      }
    }
    renderer.fillRect(x, sideTileY, width, std::max(leftHeight, rightHeight), false);
    const int maxHeight = std::max(leftHeight, rightHeight);
    for (int dx = 0; dx < width; ++dx) {
      const int height = leftHeight + ((rightHeight - leftHeight) * dx) / std::max(1, width - 1);
      renderer.fillRect(x + dx, sideTileY + (maxHeight - height) / 2, 1, height, true);
    }
    return false;
  };

  if (!coverRendered) {
    lastCarouselSelectorIndex = centerIdx;
    renderer.fillRect(rect.x, rect.y, rect.width, rect.height, false);

    const int prevIdx = (centerIdx + bookCount - 1) % bookCount;
    const int nextIdx = (centerIdx + 1) % bookCount;
    if (bookCount >= 2) drawSideCover(prevIdx, leftNearX, kNearSideW, kNearSideLeftH, kNearSideRightH);
    if (bookCount >= 2) drawSideCover(nextIdx, rightNearX, kNearSideW, kNearSideRightH, kNearSideLeftH);

    renderer.fillRect(centerX - kCenterOutlineW, centerTileY - kCenterOutlineW, kCenterCoverW + 2 * kCenterOutlineW,
                      kCenterCoverH + 2 * kCenterOutlineW, false);
    drawCover(centerIdx, centerX, centerTileY, kCenterCoverW, kCenterCoverH);

    if (!title.empty()) {
      const int titleWidth = renderer.getTextWidth(kTitleFontId, title.c_str(), EpdFontFamily::BOLD);
      renderer.drawText(kTitleFontId, (screenW - titleWidth) / 2, titleY, title.c_str(), true,
                        EpdFontFamily::BOLD);
    }
    const std::string author = renderer.truncatedText(SMALL_FONT_ID, recentBooks[centerIdx].author.c_str(), kCenterCoverW);
    if (!author.empty()) {
      const int authorWidth = renderer.getTextWidth(SMALL_FONT_ID, author.c_str());
      renderer.drawText(SMALL_FONT_ID, centerX + (kCenterCoverW - authorWidth) / 2, authorY, author.c_str(), true);
    }

    const int dotsY = centerTileY + kCenterCoverH + 8;
    const int totalDotsW = bookCount * kDotSize + (bookCount - 1) * kDotGap;
    int dotX = centerX + (kCenterCoverW - totalDotsW) / 2;
    for (int i = 0; i < bookCount; ++i) {
      if (i == centerIdx) {
        renderer.fillRect(dotX, dotsY, kDotSize, kDotSize, true);
      } else {
        renderer.drawRect(dotX, dotsY, kDotSize, kDotSize, true);
      }
      dotX += kDotSize + kDotGap;
    }

    const ReadingBookStats* stats = getBookStats(recentBooks[centerIdx]);
    const bool hasStats = stats != nullptr && stats->totalReadingMs > 0;
    const int footerWidth = std::min(kCenterCoverW, std::max(0, screenW - 2 * LyraCarouselMetrics::values.contentSidePadding));
    const int footerX = centerX + (kCenterCoverW - footerWidth) / 2;
    // Keep the stats footer inside the carousel tile so it cannot be covered by
    // the shortcut row rendered immediately below it.
    int footerY = std::min(dotsY + kDotSize + kFooterTopGap, rect.y + rect.height - 42);
    if (hasStats) {
      const uint32_t seconds = static_cast<uint32_t>(std::min<uint64_t>(stats->totalReadingMs / 1000ULL, UINT32_MAX));
      const uint32_t hours = seconds / 3600;
      const uint32_t minutes = (seconds % 3600) / 60;
      char timeText[24];
      if (hours > 0) snprintf(timeText, sizeof(timeText), "%luh %lum", static_cast<unsigned long>(hours),
                              static_cast<unsigned long>(minutes));
      else snprintf(timeText, sizeof(timeText), "%lum", static_cast<unsigned long>(minutes));
      renderer.drawText(SMALL_FONT_ID, footerX, footerY, timeText, true);
    }
    // Reserve the reading-time row even when a book has no stats yet. This
    // keeps the progress bar and percentage anchored across all carousel books.
    footerY += renderer.getLineHeight(SMALL_FONT_ID) + kFooterLabelToBarGap;
    const uint8_t progress = stats ? std::min<uint8_t>(stats->lastProgressPercent, 100) : 0;
    const int filledWidth = (footerWidth * progress) / 100;
    renderer.fillRectDither(footerX, footerY, footerWidth, kFooterProgressBarHeight, Color::LightGray);
    if (filledWidth > 0) renderer.fillRect(footerX, footerY, filledWidth, kFooterProgressBarHeight, true);
    char progressLabel[8];
    snprintf(progressLabel, sizeof(progressLabel), "%u%%", progress);
    const int progressLabelWidth = renderer.getTextWidth(SMALL_FONT_ID, progressLabel);
    renderer.drawText(SMALL_FONT_ID, footerX + footerWidth - progressLabelWidth,
                      footerY + kFooterProgressBarHeight + kFooterPercentTopGap, progressLabel, true);

    coverBufferStored = storeCoverBuffer();
    coverRendered = coverBufferStored;
  }

  const int outlineW = inCarouselRow ? kSelectionLineW : kThinOutlineW;
  renderer.drawRoundedRect(centerX, centerTileY, kCenterCoverW, kCenterCoverH, outlineW, kCornerRadius, true);
}

void LyraCarouselTheme::drawCarouselBorder(GfxRenderer& renderer, Rect rect, bool inCarouselRow) const {
  // The pre-rendered frame already carries the thin outline. Only overlay the
  // thick selection border when the carousel row is actually focused; otherwise
  // there is nothing to do (e-ink pixels can only be set black, never cleared,
  // so drawing the same thin line again would be a no-op that wastes time).
  if (!inCarouselRow) return;
  const int centerTileY = rect.y + kTitleTopClearance + renderer.getLineHeight(kTitleFontId) +
                          renderer.getLineHeight(SMALL_FONT_ID) + kTitleBottomGap;
  const int centerX = (renderer.getScreenWidth() - kCenterCoverW) / 2;
  renderer.drawRoundedRect(centerX, centerTileY, kCenterCoverW, kCenterCoverH, kSelectionLineW, kCornerRadius, true);
}

void LyraCarouselTheme::drawButtonMenu(GfxRenderer& renderer, Rect rect, int buttonCount, int selectedIndex,
                                       const std::function<std::string(int index)>& buttonLabel,
                                       const std::function<UIIcon(int index)>& rowIcon,
                                       const std::function<std::string(int index)>& buttonSubtitle,
                                       const std::function<bool(int index)>& showAccessory) const {
  (void)rect;
  (void)buttonLabel;
  (void)buttonSubtitle;
  (void)showAccessory;
  if (buttonCount <= 0) return;

  const int visibleCount = std::min(buttonCount, kVisibleMenuSlots);
  const int safeSelectedIndex = (selectedIndex >= 0 && selectedIndex < buttonCount) ? selectedIndex : -1;
  const int maxWindowStart = std::max(0, buttonCount - visibleCount);
  int windowStart = 0;
  if (safeSelectedIndex >= 0) {
    windowStart = std::clamp(safeSelectedIndex - visibleCount / 2, 0, maxWindowStart);
  }

  const int screenW = renderer.getScreenWidth();
  const int tileH = kMenuIconPad + kMenuIconSize + kMenuIconPad;
  const int tileW = screenW / visibleCount;
  const int rowY = renderer.getScreenHeight() - LyraCarouselMetrics::values.buttonHintsHeight - tileH;

  renderer.fillRect(0, rowY, screenW, tileH, false);

  for (int slot = 0; slot < visibleCount; ++slot) {
    const int i = windowStart + slot;
    const int tileX = slot * tileW;
    const int iconX = tileX + (tileW - kMenuIconSize) / 2;
    const int iconY = rowY + kMenuIconPad;

    if (safeSelectedIndex == i) {
      const int highlightSize = kMenuIconSize + 2 * kHighlightPad;
      const int highlightY = rowY + (tileH - highlightSize) / 2;
      renderer.fillRoundedRect(iconX - kHighlightPad, highlightY, highlightSize, highlightSize, kCornerRadius,
                               Color::Black);
    }

    if (rowIcon != nullptr) {
      const uint8_t* bmp = iconForName(rowIcon(i), kMenuIconSize);
      if (bmp != nullptr) {
        if (safeSelectedIndex == i) {
          if (renderer.isDarkMode()) {
            renderer.drawIconBlack(bmp, iconX, iconY, kMenuIconSize, kMenuIconSize);
          } else {
            renderer.drawIconInverted(bmp, iconX, iconY, kMenuIconSize, kMenuIconSize);
          }
        } else {
          renderer.drawIcon(bmp, iconX, iconY, kMenuIconSize, kMenuIconSize);
        }
      }
    }
  }

  if (buttonCount > visibleCount) {
    const int midY = rowY + tileH / 2;
    if (windowStart > 0) {
      renderer.drawLine(10, midY, 20, midY - 9, 2, true);
      renderer.drawLine(10, midY, 20, midY + 9, 2, true);
    }
    if (windowStart + visibleCount < buttonCount) {
      renderer.drawLine(screenW - 10, midY, screenW - 20, midY - 9, 2, true);
      renderer.drawLine(screenW - 10, midY, screenW - 20, midY + 9, 2, true);
    }
  }
}

void LyraCarouselTheme::drawList(const GfxRenderer& renderer, Rect rect, int itemCount, int selectedIndex,
                                 const std::function<std::string(int index)>& rowTitle,
                                 const std::function<std::string(int index)>& rowSubtitle,
                                 const std::function<UIIcon(int index)>& rowIcon,
                                 const std::function<std::string(int index)>& rowValue, bool highlightValue,
                                 const std::function<bool(int index)>& rowCompleted) const {
  LyraTheme::drawList(renderer, rect, itemCount, selectedIndex, rowTitle, rowSubtitle, rowIcon, rowValue,
                      highlightValue, rowCompleted);
}

void LyraCarouselTheme::drawTabBar(const GfxRenderer& renderer, Rect rect, const std::vector<TabInfo>& tabs,
                                   bool selected) const {
  LyraTheme::drawTabBar(renderer, rect, tabs, selected);
}
