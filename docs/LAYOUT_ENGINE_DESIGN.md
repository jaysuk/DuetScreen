# Data-Driven Layout Engine — Design Proposal

**Status:** Draft / for discussion
**Scope:** Make the DuetScreen dashboard user-customisable by describing it as data rather than compiled composition.

---

## 1. Motivation

Today the dashboard is fixed. Users who want a bigger temperature graph, no file browser, or the
status panel front-and-centre have no route to that short of building custom firmware.

DuetScreen already lets users customise *presentation* through the theme system — colours, fonts,
dark mode, persisted per theme with a reset-to-defaults path
([`CustomTheme.cpp`](../src/UI/Styles/Themes/CustomTheme.cpp)). This proposal extends that same idea
from styling to **composition**: which widgets appear, where they sit, and how big they are.

The reference point is the Flexible Layouts plugin for Duet Web Control. DWC can do this easily
because it is a browser app with a JavaScript plugin system that arranges panels at runtime.
DuetScreen is compiled C++ on LVGL, so there is no plugin sandbox to lean on — the equivalent has to
be a native layout engine that reads a layout document and builds the UI from it. The document is
data, so it can be edited on-device, shipped as a preset, or shared as a file.

---

## 2. What the current architecture gives us for free

The codebase is unusually well set up for this. Worth stating explicitly, because it changes the
size of the job:

| Existing capability | Why it matters |
| --- | --- |
| **MVP with self-managing presenters** — [`View.h`](../src/UI/Core/View.h) constructs a presenter per view and *unbinds it from the Model on destruction* | A widget that is not on screen costs nothing. Creating and destroying widgets at runtime does not leak subscriptions. This is normally the hard part of a dynamic layout system, and it is already solved. |
| **Grid layout is already the primitive** — [`Dashboard.cpp:16-37`](../src/UI/Widgets/Dashboard/Dashboard.cpp#L16-L37) builds the screen from `LV_GRID_FR` track descriptors | The engine is largely "read those track sizes from JSON instead of a `constexpr` array". |
| **Runtime child creation has precedent** — `List<T>::setItemCount` takes a factory returning `std::unique_ptr<T>` ([`List.h`](../src/UI/Components/List/List.h)); `TabView` owns `std::vector<std::unique_ptr<LvContainer>>` | The ownership pattern the engine needs is already used elsewhere in the UI. |
| **`AppDrawer` is a proto-registry** — a static `AppInfo[]` table of name/icon/target ([`AppDrawer.cpp`](../src/UI/Components/AppDrawer/AppDrawer.cpp)) | Confirms the shape of a widget descriptor, and is a natural second consumer of the real registry. |
| **`LvObj` supports runtime reparenting and lookup** — `setParent`, `getChildByName`, `setGridCell`, `setGridDsc` ([`LvObj.h`](../src/UI/Components/LVGL/LvObj.h)) | No new LVGL plumbing required. |
| **Storage is `nlohmann::json` key/value** ([`Storage.h`](../src/Storage.h)) | A layout document persists with no new serialisation layer. |
| **Snapshot image tests** (`tests/ref_imgs`) | Gives a way to prove the refactor is pixel-identical rather than "looks about right". |

### The one real obstacle

Widgets are **statically composed as member variables**
([`Dashboard.h:43-60`](../src/UI/Widgets/Dashboard/Dashboard.h#L43-L60),
[`HomeView.h:79-120`](../src/UI/Screens/Home/HomeView.h#L79-L120)), and other code reaches through
concrete accessors:

```cpp
HomeView::instance().getDashboard().getStatusView()
```

Once a widget may or may not exist, every one of those call sites has to tolerate its absence. This
is the main refactor risk and is dealt with in Phase 0 below, deliberately isolated from any
behavioural change.

---

## 3. Non-goals

Explicitly out of scope for the first implementation:

- **Full screens are not data-driven.** Only the dashboard. `Navigation`, `AppDrawer` and the
  screen-level views stay compiled. Much smaller blast radius, and the dashboard is the screen users
  actually look at.
- **No free-form pixel positioning.** Placement snaps to grid cells. See §6.
- **No third-party widget plugins.** The registry is populated at compile time. Loadable widget code
  is a much larger conversation (code signing, ABI, sandboxing) and is not required to deliver user
  customisation.
- **No live re-layout during drag.** Rebuild happens on explicit apply. See §5.3.

---

## 4. Architecture

Four layers. Each is independently useful and independently reviewable.

### 4.1 Widget registry

A self-registering factory keyed by a stable string ID. The ID is the wire format — it must never be
translated and never renamed once shipped.

```cpp
namespace UI::Layout
{
    struct GridHint
    {
        uint8_t minCols = 1;
        uint8_t minRows = 1;
    };

    struct WidgetDescriptor
    {
        std::string_view id;        // "temperature_graph" — stable identifier, never translated
        std::string_view nameKey;   // i18n key, resolved at render time (language changes at runtime)
        std::string_view icon;      // asset name, as used by AppDrawer
        GridHint             hint;  // stops the editor producing an unusable size
        bool                 singleton = false;

        // Creates the widget. `instanceId` is the derived storage-key prefix, see §4.2.
        std::function<std::unique_ptr<LvObj>(const std::string& instanceId,
                                             LvObj& parent,
                                             const nlohmann::json& props)> create;

        // Optional gate — e.g. hide heightmap when the machine has no probe.
        std::function<bool()> available = nullptr;
    };

    class WidgetRegistry
    {
      public:
        static WidgetRegistry& get();          // function-local static: avoids static-init-order issues

        void add(WidgetDescriptor descriptor);
        const WidgetDescriptor* find(std::string_view id) const;
        std::vector<const WidgetDescriptor*> availableWidgets() const;
    };
}
```

Factories return `std::unique_ptr<LvObj>`. Because every widget derives from `LvObj` (via
`View<Presenter>`) and unbinds in its destructor, the owning container can hold a plain
`std::vector<std::unique_ptr<LvObj>>` and lifetime works out with no special handling.

Two implementation notes:

- Use a **function-local static** for the registry so registration from translation-unit-level
  constructors cannot hit static-initialisation-order problems.
- **Resolve `_()` lazily**, at the point the editor renders a label — not at registration time.
  Language is switchable at runtime, and caching a translated string at static-init would freeze it.

### 4.2 Layout document

JSON, versioned, stored via the existing `Storage` layer or as a file.

```json
{
  "schema": 1,
  "name": "Classic",
  "root": {
    "type": "grid",
    "cols": ["3fr", "2fr"],
    "rows": ["content", "1fr"],
    "children": [
      {
        "widget": "tool_list",
        "col": 0, "row": 0,
        "align": { "x": "stretch", "y": "start" },
        "props": { "maxHeight": "50%" }
      },
      {
        "widget": "temperature_graph",
        "col": 0, "row": 1,
        "props": { "yMin": 0, "yMax": 300, "xSeconds": 60 }
      },
      {
        "type": "tabs",
        "col": 1, "row": 0, "rowSpan": 2,
        "children": [
          { "widget": "file_browser", "tab": "file.jobs",   "props": { "folder": "jobs"   } },
          { "widget": "file_browser", "tab": "file.macros", "props": { "folder": "macros" } },
          { "widget": "status",       "tab": "app_drawer.status" }
        ]
      }
    ]
  }
}
```

**Container types** are deliberately few, and each maps onto something that already exists:

| `type` | Maps to | Notes |
| --- | --- | --- |
| `grid` | `LvObj::setGridDsc` / `setGridCell` | Children carry `col`, `row`, `colSpan`, `rowSpan`, `align` |
| `row` | [`Row`](../src/UI/Components/Containers/Row.h) (flex row) | Children carry `grow` |
| `column` | [`Column`](../src/UI/Components/Containers/Column.h) (flex column) | Children carry `grow` |
| `tabs` | [`TabView`](../src/UI/Components/Containers/TabView.h) | Children carry `tab` (an i18n key) |

**Track size tokens** need a small parser (roughly ten lines):

| Token | Becomes |
| --- | --- |
| `"3fr"` | `LV_GRID_FR(3)` |
| `"content"` | `LV_GRID_CONTENT` |
| `"120px"` | literal `120` |
| `"50%"` | `LV_PCT(50)` |

#### Per-instance storage keys — design this in from day one

In the current dashboard, `file_browser` appears **twice** with hand-written storage key prefixes
([`Dashboard.h:47-59`](../src/UI/Widgets/Dashboard/Dashboard.h#L47-L59)):

```cpp
.sortBy = {"ui:dashboard:file:jobs:sort_by",   OM::FileSystem::SortBy::DATE},
.sortBy = {"ui:dashboard:file:macros:sort_by", OM::FileSystem::SortBy::NAME},
```

The builder must derive a **stable per-instance key prefix** from the node's path in the document
(for example `ui:layout:<layoutName>:<nodePath>:`) and hand it to the factory as `instanceId`, so
that per-widget settings such as sort order and display mode survive across restarts and are not
shared between two instances of the same widget.

Retrofitting this after the fact is painful — every widget factory signature changes. Build it in
from the start.

### 4.3 Layout builder

```cpp
class LayoutBuilder
{
  public:
    struct Result
    {
        bool ok;
        std::vector<std::string> errors;
    };

    static Result validate(const nlohmann::json& doc);
    static std::unique_ptr<LayoutInstance> build(const nlohmann::json& doc, LvObj& parent);
};
```

`LayoutInstance` owns the created objects and exposes an `id → LvObj*` map for lookup (§Phase 0).

Two rules that are not negotiable on a device with no keyboard and no rescue console:

1. **Validate before building.** A malformed, unknown-widget, or unsatisfiable document logs its
   errors and falls back to the built-in default layout. *A bad layout must never leave the user with
   an unusable screen.*
2. **Rebuild wholesale, do not diff.** Teardown is destroying the `LayoutInstance`; the
   `unique_ptr`s unbind the presenters automatically. Incremental diffing buys a flicker-free apply
   at a large cost in complexity and failure modes. Rebuild only on explicit *Apply* — never per drag
   frame — which also keeps heap churn and fragmentation bounded.

### 4.4 The stock dashboard becomes the first layout

Express the current dashboard as `layouts/default.json`, delete the hardcoded composition from
`Dashboard.h`/`Dashboard.cpp`, and prove pixel-identity against the existing `tests/ref_imgs`
snapshots.

This is the forcing function for the whole design: **if the current dashboard cannot be expressed in
the schema, the schema is wrong.** It surfaces every hand-tuned sizing decision (§7) before any user
ever sees the feature.

---

## 5. How the end user changes the layout

Three tiers, in descending order of value-per-unit-of-work.

### 5.1 Tier 1 — Presets

*Settings → Display → Layout*, presented as a gallery of 4–5 curated layouts with preview
thumbnails: **Classic**, **Big Temps**, **Files First**, **Minimal**.

Thumbnails can be generated by the existing snapshot test harness at build time, so they never drift
from reality.

Most users do not want to design a UI — they want to pick one. **If this is the only tier that ever
ships, it is still a real feature**, and it is a small amount of work on top of Phases 0–3.

### 5.2 Tier 2 — On-screen editor

Long-press the dashboard (or *Settings → Edit layout*) to enter edit mode:

- Widgets get a dashed outline and a drag handle.
- Drag a widget onto another cell to move or swap it.
- Drag an edge to resize by whole grid cells.
- `+` opens a widget picker populated from the registry, filtered by `available()`.
- Drag-to-bin, or a trash affordance, removes a widget.
- *Save* / *Cancel* / *Reset to default*.

[`DraggableButton`](../src/UI/Components/Button/DraggableButton.cpp) is a starting point for the drag
mechanics.

**Snap to grid cells, never to free pixels.** Free-form positioning is a trap on an 800×480 panel
driven by fingers: it is fiddly to operate, it produces overlapping and unreachable widgets, and the
resulting layouts shatter under rotation or on a different panel size. Cell snapping keeps every
reachable state a valid one.

### 5.3 Tier 3 — Import, export, share

The layout is just a JSON file. Copy it to the SD card, or push it over the existing connection.
This is where community layouts come from, and it is nearly free once the schema is stable.

The natural sequel is a **DWC plugin that edits the layout JSON on a large screen with a mouse and
pushes it to the panel**. That is the point at which this genuinely feels like Flexible Layouts, and
it reuses well-trodden ground.

---

## 6. Phasing

| Phase | Work | User-visible |
| --- | --- | --- |
| **0** | Lookup-by-id API on the dashboard; migrate `getDashboard().getX()` call sites to tolerate a missing widget | No |
| **1** | `WidgetRegistry` + descriptors for the ~8 dashboard widgets | No |
| **2** | Schema, `LayoutBuilder`, validation, fallback-to-default | No |
| **3** | Stock dashboard expressed as `default.json`; snapshot tests prove pixel-identical; delete hardcoded composition | No |
| **4** | Preset gallery in Settings | **Yes — first shipping win** |
| **5** | On-screen grid editor | Yes |
| **6** | Import / export / share | Yes |

**Phase 0 should land as a standalone PR before anything becomes dynamic.** It is the riskiest change
and by far the easiest to review while it still has no behavioural effect.

---

## 7. Risks and open questions

- **Static composition → dynamic ownership.** The Phase 0 refactor touches every place that reaches
  into a concrete widget. Sequencing it first, with no behaviour change, is what keeps this
  tractable.

- **Hand-tuned sizing is load-bearing.** Calls like `m_toolList.setMaxHeight(LV_PCT(50))` and
  `LV_SIZE_CONTENT` inside `fr` tracks ([`Dashboard.cpp:39-46`](../src/UI/Widgets/Dashboard/Dashboard.cpp#L39-L46))
  encode real layout knowledge. If `props` cannot express them, layouts will look subtly wrong in
  ways that are tedious to diagnose. Phase 3 is what flushes these out.

- **Memory and fragmentation.** Repeated build/teardown cycles allocate and free many LVGL objects.
  Rebuilding only on explicit apply bounds this, but heap behaviour on the target should be measured
  before the editor ships.

- **Rotation and panel size.** `ID_DISPLAY_ROTATION` already exists. The simplest correct answer is
  to **store a separate layout per orientation** rather than attempting automatic reflow. Whether
  layouts should also declare a target resolution is an open question, and matters if the schema is
  to survive future hardware.

- **Widget availability is dynamic.** A layout referencing a heightmap on a machine with no probe
  must degrade gracefully — leave the cell empty rather than refusing the whole document.

- **Evaluated and rejected: LVGL's XML system.** There is a stubbed
  [`LvXml`](../src/UI/Components/LVGL/LvXml.h) wrapper, but `LV_USE_XML` is currently off. It is not
  a good fit here: it instantiates raw LVGL objects rather than presenter-backed `View<P>` classes,
  so a registry would still be needed to bridge to them, and its schema is verbose for anything
  human-edited. A purpose-built JSON schema is more controllable and matches the existing `Storage`
  layer.

---

## 8. Testing

- **Snapshot parity (Phase 3).** The existing `tests/ref_imgs` suite must pass unchanged once the
  dashboard is engine-produced. This is the single most valuable test in the plan.
- **Preset snapshots.** Extend the suite to render each shipped preset, which doubles as thumbnail
  generation for the gallery.
- **Schema validation.** Unit-test the validator against malformed documents: unknown widget IDs,
  out-of-range grid coordinates, overlapping cells, missing required props, wrong schema version.
  Fuzzing the parser is cheap here and worth doing, since the document may arrive from an SD card.
- **Fallback path.** Explicitly test that a corrupt stored layout boots into the default rather than
  a broken or blank screen.
