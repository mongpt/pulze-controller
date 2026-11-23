// ---------------------------------------------------------------------
// favoritePages.js - the Favorites page layout. No DOM, no Bluetooth.
//
// A layout is an array of pages. A page is an array of preset ids, at
// most FAVORITE_SLOTS_PER_PAGE long. A preset's position in its page is
// the footswitch it lands on (position 0 = footswitch 1), so a page
// never has gaps. The same id can sit on several pages, but only once
// per page.
//
// Every function returns a new layout, or null when the change is not
// allowed. Nothing edits its input.
// ---------------------------------------------------------------------

// Must match PRESETS_PER_BANK and MAX_PAGES in the firmware's config.h.
const FAVORITE_SLOTS_PER_PAGE = 4;
const MAX_FAVORITE_PAGES = 64;

// ---- building a layout from saved data ---------------------------------

// Fills pages four at a time. This is how the footswitch used to lay
// out one flat favorites list, so old libraries keep their layout.
function pagesFromOrder(order) {
  const pages = [];
  for (
    let i = 0;
    i < order.length && pages.length < MAX_FAVORITE_PAGES;
    i += FAVORITE_SLOTS_PER_PAGE
  ) {
    pages.push(order.slice(i, i + FAVORITE_SLOTS_PER_PAGE));
  }
  return pages;
}

function sanitizePages(rawPages, validIds) {
  if (!Array.isArray(rawPages)) return [];

  return rawPages
    .filter(Array.isArray)
    .slice(0, MAX_FAVORITE_PAGES)
    .map(page => uniqueIds(page.filter(id => validIds.has(id)))
      .slice(0, FAVORITE_SLOTS_PER_PAGE));
}

function uniqueIds(ids) {
  return [...new Set(ids)];
}

// ---- questions about a layout ------------------------------------------

function isPresetOnAnyPage(pages, id) {
  return pages.some(page => page.includes(id));
}

// ---- changing pages ----------------------------------------------------

function addPage(pages) {
  if (pages.length >= MAX_FAVORITE_PAGES) return null;
  return [...pages, []];
}

function removePage(pages, pageIndex) {
  return pages.filter((_, i) => i !== pageIndex);
}

// ---- changing the presets on a page ------------------------------------

function addPresetToPage(pages, pageIndex, id) {
  const page = pages[pageIndex];
  if (!page || page.length >= FAVORITE_SLOTS_PER_PAGE || page.includes(id)) {
    return null;
  }
  return replacePage(pages, pageIndex, [...page, id]);
}

// Used by the star. Opens a new page when the last one is full.
function appendToLastPage(pages, id) {
  const last = pages[pages.length - 1];
  const hasRoom = last && last.length < FAVORITE_SLOTS_PER_PAGE;
  const target = hasRoom ? pages : addPage(pages);
  if (!target) return null;

  return addPresetToPage(target, target.length - 1, id);
}

function removePresetFromPage(pages, pageIndex, slot) {
  const page = pages[pageIndex];
  if (!page) return pages;
  return replacePage(pages, pageIndex, page.filter((_, s) => s !== slot));
}

function removePresetFromAllPages(pages, id) {
  return pages.map(page => page.filter(pageId => pageId !== id));
}

// from and to are { page, slot }. The preset ends up at to.slot on the
// destination page, or at its end when to.slot is past the last preset.
function movePreset(pages, from, to) {
  const id = pages[from.page] && pages[from.page][from.slot];
  if (id === undefined || !pages[to.page]) return null;

  const withoutSource = removePresetFromPage(pages, from.page, from.slot);
  const destination = withoutSource[to.page];
  if (destination.length >= FAVORITE_SLOTS_PER_PAGE || destination.includes(id)) {
    return null;
  }

  const slot = Math.min(to.slot, destination.length);
  const moved = [...destination.slice(0, slot), id, ...destination.slice(slot)];
  return replacePage(withoutSource, to.page, moved);
}

function replacePage(pages, pageIndex, page) {
  return pages.map((existing, i) => (i === pageIndex ? page : existing));
}

// ---- footswitch transfer -----------------------------------------------

// Splits a layout into what goes over Bluetooth:
//  - presetIds: each distinct preset once, in order of first use. The
//    preset records follow in this order.
//  - pageTable: [pageCount, then per page: slotCount and one uint16
//    (little-endian) per slot, an index into presetIds].
// Sending a repeated preset once keeps the transfer short.
function buildTransferLayout(pages) {
  const presetIds = [];
  const indexById = new Map();
  const table = [pages.length];

  for (const page of pages) {
    table.push(page.length);
    for (const id of page) {
      if (!indexById.has(id)) {
        indexById.set(id, presetIds.length);
        presetIds.push(id);
      }
      const index = indexById.get(id);
      table.push(index & 0xff, (index >> 8) & 0xff);
    }
  }

  return { presetIds, pageTable: Uint8Array.from(table) };
}
