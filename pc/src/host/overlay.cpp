/*
 * F1 overlay (Dear ImGui): display, audio and port settings. While it is open the game
 * keeps running but receives no pad input. Settings are applied immediately and saved
 * to <save_dir>/recvx.ini.
 */
#include "../ps2_platform.h"
extern "C" {
#include "../audio/audio.h"
}
#include "prof.h"
#include "cheat.h"
#include "actors.h"
#include "../gs/stream.h"
#include "tps.h"
#include "menu_mouse.h"
#include "bindings.h"
#include "settings.h"
#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <ctype.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

extern "C" void pad_set_game_input_enabled(int on);

static SDL_Window* win;
static bool ready, visible, cheats_visible;
static bool tps_key_wait;   /* the next key pressed becomes the third-person toggle key */

void overlay_init(void* window, void* gl_context)
{
    win = (SDL_Window*)window;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;   /* our settings live in recvx.ini */
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForOpenGL(win, (SDL_GLContext)gl_context);
    ImGui_ImplOpenGL3_Init("#version 330 core");
    ready = true;
    const char* show = SDL_getenv("RECVX_SHOW_OVERLAY");   /* debugging aid (screenshots): 1 F1, 2 the cheats (F2) */
    visible = show && strcmp(show, "2");
    cheats_visible = show && !strcmp(show, "2");
}

int overlay_event(const void* e)
{
    const SDL_Event* ev = (const SDL_Event*)e;
    if (!ready) return 0;
    if (tps_key_wait && ev->type == SDL_EVENT_KEY_DOWN && !ev->key.repeat) {
        if (ev->key.scancode != SDL_SCANCODE_ESCAPE) { g_tps.key = ev->key.scancode; settings_save_later(); }
        tps_key_wait = false;
        return 1;
    }
    if (bind_hotkey(ev, B_SETTINGS)) {
        visible = !visible;
        return 1;
    }
    if (bind_hotkey(ev, B_CHEATS)) {
        cheats_visible = !cheats_visible;
        return 1;
    }
    ImGui_ImplSDL3_ProcessEvent(ev);
    if (!visible && !cheats_visible) return 0;
    return (ev->type == SDL_EVENT_KEY_DOWN || ev->type == SDL_EVENT_KEY_UP || ev->type == SDL_EVENT_TEXT_INPUT ||
            ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev->type == SDL_EVENT_MOUSE_BUTTON_UP || ev->type == SDL_EVENT_MOUSE_WHEEL);
}

/* the rows of a page of the shared table (settings.c): all of them, or on the pages that have
   their own windows (EXTRAS: F2) the ones flagged SET_F1. Values are applied by
   settings_poll (next pumped frame) and recvx.ini written once they stop changing. true = changed */
/* the picture calibration test pattern (settings_calib), in the colours the game's picture gets
   (the overlay itself is drawn without the calibration) */
static void calib_pattern(void)
{
    float sq = ImGui::GetFontSize() * 1.5f, gap = sq * 0.5f, w = CALIB_STEPS * (sq + gap) + gap;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto grey = [](int v) { int g = (int)(gs_picture(v / 255.0f) * 255.0f + 0.5f); return IM_COL32(g, g, g, 255); };
    for (int k = 0; k < 2; k++) {
        float x = p.x + k * (w + gap);
        dl->AddRectFilled(ImVec2(x, p.y), ImVec2(x + w, p.y + sq + 2 * gap), grey(k ? 255 : 0));
        for (int i = 0; i < CALIB_STEPS; i++)
            dl->AddRectFilled(ImVec2(x + gap + i * (sq + gap), p.y + gap), ImVec2(x + gap + i * (sq + gap) + sq, p.y + gap + sq), grey(settings_calib[k][i]));
    }
    ImGui::Dummy(ImVec2(2 * w + gap, sq + 2 * gap));
}

static bool settings_imgui(int page, bool all = true)
{
    bool changed = false;
    for (int i = 0; i < g_settings_n; i++) {
        const Setting* s = &g_settings[i];
        if (s->page != page || !(all || (s->flags & SET_F1)) || (s->shown && !s->shown())) continue;
        if (s->group && all) ImGui::SeparatorText(s->group);
        const char* lock = settings_locked(s);
        char label[96], val[96];
        int v = settings_get(s), hi = settings_hi(s), st = s->step > 0 ? s->step : 1;
        SDL_snprintf(label, sizeof label, "%s%s##set%d", s->label, (s->flags & SET_RESTART) ? " (next start)" : "", i);
        ImGui::BeginDisabled(lock != nullptr);
        if (s->flags & SET_ACTION) {
            if (ImGui::Button(label)) settings_act(s);
        } else if (settings_is_bool(s)) {
            bool b = v != 0;
            if (ImGui::Checkbox(label, &b)) { settings_set(s, b); changed = true; }
        } else if (s->names || (s->name && (s->count || (hi - s->lo) / st <= 10))) {   /* a list (tenths: a slider) */
            if (ImGui::BeginCombo(label, settings_value(s, val, sizeof val), ImGuiComboFlags_HeightLarge)) {
                for (int k = s->lo; k <= hi; k += st) {
                    char nm[96];
                    if (ImGui::Selectable(s->name ? s->name(k, nm, sizeof nm) : s->names[k - s->lo], k == v)) { settings_set(s, k); changed = true; }
                    if (k == v) ImGui::SetItemDefaultFocus();   /* the list opens at the current value (a long one: "You appear as") */
                }
                ImGui::EndCombo();
            }
        } else {   /* a number; a value name (tenths) is shown as the slider's text */
            char fmt[96];
            const char* f = s->fmt ? s->fmt : "%d";
            if (s->name) {
                char* o = fmt;
                for (const char* c = s->name(v, val, sizeof val); *c && o < fmt + sizeof fmt - 2; c++) { if (*c == '%') *o++ = '%'; *o++ = *c; }
                *o = 0;
                f = fmt;
            }
            if (ImGui::SliderInt(label, &v, s->lo, hi, f)) {
                settings_set(s, st > 1 ? (v + st / 2) / st * st : v);
                changed = true;
            }
        }
        ImGui::EndDisabled();
        if ((ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) || ((s->flags & SET_CALIB) && ImGui::IsItemActive())) && (s->help || lock)) {
            ImGui::BeginTooltip();
            ImGui::Text("%s%s%s", lock ? lock : "", lock && s->help ? "\n" : "", s->help ? s->help : "");
            if (s->flags & SET_CALIB) calib_pattern();
            ImGui::EndTooltip();
        }
    }
    return changed;
}

static bool settings_imgui_any(int page)
{
    for (int i = 0; i < g_settings_n; i++) if (g_settings[i].page == page && (g_settings[i].flags & SET_F1)) return true;
    return false;
}

/* Controls: the bindings of both sets; a click on a binding waits for a key (bindings.c) */
static void controls_section(void)
{
    static int dev, cap_row = -1, cap_slot;
    static char msg[128];
    char label[128], nm[64];
    if (cap_row >= 0) {
        int c = bind_capture_poll();
        if (c != -1) {
            if (c != -2) {
                bind_set(cap_row, dev, cap_slot, c == -3 ? 0 : c);
                if (bind_swapped()) SDL_snprintf(msg, sizeof msg, "Swapped with %s", bind_swapped());
                else if (bind_combo(cap_row, dev, cap_slot)) SDL_snprintf(msg, sizeof msg, "Shared: hold aim and press it to reload");
                else msg[0] = 0;
                settings_save_later();
            }
            cap_row = -1;
        }
    }
    settings_imgui(SP_CONTROLS);   /* input source, which gamepad */
    if (ImGui::BeginTabBar("##dev")) {
        if (ImGui::BeginTabItem("Keyboard / mouse")) { if (dev != BIND_KB) cap_row = -1; dev = BIND_KB; ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Gamepad")) { if (dev != BIND_PAD) cap_row = -1; dev = BIND_PAD; ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
    char pad_name[96];
    if (dev == BIND_PAD) ImGui::TextDisabled("%s; the left stick also moves, as on the DS2. The third-person rows play in third\n"
                                             "person with Modern gamepad controls, instead of the game rows of the same buttons.", bind_pad_desc(pad_name, sizeof pad_name));
    ImGui::TextDisabled("Click a binding, then press the key/button (Esc cancels, Delete clears); right click clears.\n"
                        "Reload can share a button: then Aim + button reloads.");
    if (msg[0]) ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "%s", msg);
    if (ImGui::BeginTable("##binds", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("action", ImGuiTableColumnFlags_WidthStretch, 2.2f);
        ImGui::TableSetupColumn("1", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("2", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        int group = -1;
        for (int i = 0; i < B_COUNT; i++) {
            int r = bind_order[i];
            if (!bind_row_shown(r, dev)) continue;
            if (bind_rows[r].group != group) {
                group = bind_rows[r].group;
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (group == BG_GAME) ImGui::SeparatorText("Game (as the KEY CONFIG type sets them)");
                else ImGui::SeparatorText(group == BG_TPS ? (dev ? "Third person (gamepad, Modern)" : "Third person (keyboard and mouse)") : "Hotkeys");
                if (group == BG_GAME) { ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("type %c", 'A' + pc_keytype()); }
            }
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            int conflict = bind_conflict(r, dev);
            if (conflict) ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", bind_row_label(r, label, sizeof label));
            else ImGui::TextUnformatted(bind_row_label(r, label, sizeof label));
            if (conflict && ImGui::IsItemHovered()) ImGui::SetTooltip("Also bound to: %s", bind_rows[conflict - 1].name);
            for (int k = 0; k < BIND_SLOTS; k++) {
                ImGui::TableSetColumnIndex(1 + k);
                char b[96];
                if (cap_row == r && cap_slot == k) SDL_snprintf(b, sizeof b, "press... %ds##%d_%d", (bind_capture_left_ms() + 999) / 1000, r, k);
                else SDL_snprintf(b, sizeof b, "%s%s##%d_%d", bind_combo(r, dev, k) ? "Aim + " : "", bind_code_name(dev, bind_get(r, dev, k), nm, sizeof nm), r, k);
                if (ImGui::Button(b, ImVec2(-FLT_MIN, 0))) { cap_row = r; cap_slot = k; msg[0] = 0; bind_capture_start(dev); }
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) { bind_set(r, dev, k, 0); settings_save_later(); }
            }
        }
        ImGui::EndTable();
    }
    if (ImGui::Button(dev ? "Reset gamepad to defaults" : "Reset keyboard / mouse to defaults")) { bind_reset(dev); settings_save_later(); msg[0] = 0; }
    ImGui::TextDisabled("Also in the game's OPTION menu: ENHANCED SETTINGS -> CONTROLS.");
}

static void settings_window(void)
{
    bool changed = false;
    int ww, wh;
    SDL_GetWindowSizeInPixels(win, &ww, &wh);

    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(460, 0), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Resident Evil Code: Veronica X  -  settings (F1)  -  " RECVX_VERSION, &visible)) { ImGui::End(); return; }

    if (ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen)) {
        changed |= settings_imgui(SP_DISPLAY);
        ImGui::TextDisabled("Output %d x %d, %.1f fps presented", ww, wh, ImGui::GetIO().Framerate);
    }
    if (ImGui::CollapsingHeader("Gameplay", ImGuiTreeNodeFlags_DefaultOpen)) changed |= settings_imgui(SP_GAMEPLAY);
    if (ImGui::CollapsingHeader("Controls")) controls_section();
    if (ImGui::CollapsingHeader("Third person", ImGuiTreeNodeFlags_DefaultOpen)) {
        changed |= settings_imgui(SP_TPS);
        char key[96];
        SDL_snprintf(key, sizeof key, tps_key_wait ? "Press a key (Esc cancels)" : "Toggle key: %s (change)", SDL_GetScancodeName((SDL_Scancode)g_tps.key));
        if (ImGui::Button(key)) tps_key_wait = true;
        ImGui::TextDisabled(g_tps.free_cam
                            ? "W/A/S/D walk toward that screen direction, Shift run, mouse turns the camera; right mouse aim, left fire;\n"
                              "E action, R reload, Q cancel, Tab status, M map (Controls changes them). F1 frees the mouse."
                            : "W/S walk, A/D turn, Shift run, mouse look; right mouse aim, left fire;\n"
                              "E action, R reload, Q cancel, Tab status, M map (Controls changes them). F1 frees the mouse.");
        if (g_tps.pad_modern)
            ImGui::TextDisabled("Gamepad: left stick walks (pushed far runs), right stick camera / aim; LT aim, RT fire, A action,\n"
                                "X run, B cancel, Y inventory, Back map, LB knife, RB reload, d-pad down quick turn, RS camera behind.");
    }
    if (ImGui::CollapsingHeader("Audio", ImGuiTreeNodeFlags_DefaultOpen)) changed |= settings_imgui(SP_AUDIO);
    if (ImGui::CollapsingHeader("Appearance", ImGuiTreeNodeFlags_DefaultOpen)) changed |= settings_imgui(SP_APPEAR);
    if (settings_imgui_any(SP_EXTRAS) && ImGui::CollapsingHeader("Extras", ImGuiTreeNodeFlags_DefaultOpen))   /* a page with its own window (F2): the SET_F1 rows */
        changed |= settings_imgui(SP_EXTRAS, false);

    if (ImGui::CollapsingHeader("Performance")) {
        static ProfFrame h[240];
        bool on = prof_enabled != 0;
        if (ImGui::Checkbox("Measure (small overhead; always on with --profile)", &on)) prof_enabled = on;
        int n = prof_history(h, 240);
        if (n > 0) {
            float total[240], avg[PROF_COUNT] = { 0 }, avg_total = 0, worst = 0;
            int m = n < 60 ? n : 60;
            for (int i = 0; i < n; i++) total[i] = h[i].total;
            for (int i = n - m; i < n; i++) {
                avg_total += h[i].total / m;
                if (h[i].total > worst) worst = h[i].total;
                for (int c = 0; c < PROF_COUNT; c++) avg[c] += h[i].ms[c] / m;
            }
            ImGui::Text("game frame %.2f ms avg, %.2f ms worst (target 33.37 ms = 29.97 Hz)", avg_total, worst);
            ImGui::PlotLines("##ft", total, n, 0, "frame time (ms)", 0.0f, 66.7f, ImVec2(-1, 60));
            for (int c = 0; c < PROF_COUNT; c++) {
                ImGui::Text("%-8s %6.2f ms", prof_names[c], avg[c]);
                ImGui::SameLine(160);
                ImGui::ProgressBar(avg_total > 0 ? avg[c] / avg_total : 0, ImVec2(-1, 0), "");
            }
            ImGui::TextDisabled("'wait' is idle time; run with --profile for a CSV.");
        }
    }
    ImGui::End();
    if (changed) settings_save_later();
}

/* F2 teleport: location, room and entry by name (teleport.c), a search over all rooms, or the raw numbers */
static void teleport_pick(CheatState* c)
{
    static bool raw;
    static char filter[48];
    char b[128], e[160];
    int n = tp_count(), k, cur = tp_find(c->tp_stg, c->tp_rom);
    if (n && !raw) {
        if (ImGui::BeginCombo("Location", tp_stage_name(c->tp_stg), ImGuiComboFlags_HeightLarge)) {
            for (int s = 0; s < 10; s++) {
                int first = tp_stage_first(s, &k);
                if (k && ImGui::Selectable(tp_stage_name(s), s == c->tp_stg)) { c->tp_stg = s; c->tp_rom = tp_room(first)->rom; c->tp_pos = 0; }
            }
            ImGui::EndCombo();
        }
        if (ImGui::BeginCombo("Room", cur >= 0 ? tp_room_name(cur, 1, b, sizeof b) : "-", ImGuiComboFlags_HeightLarge)) {
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##filter", "type to search every location", filter, sizeof filter);
            for (int i = 0; i < n; i++) {
                const TpRoom* r = tp_room(i);
                tp_room_name(i, 1, b, sizeof b);
                if (filter[0] ? !SDL_strcasestr(b, filter) && !SDL_strcasestr(tp_stage_name(r->stg), filter) : r->stg != c->tp_stg) continue;
                if (filter[0]) snprintf(e, sizeof e, "%s   %s##%d", b, tp_stage_name(r->stg), i);
                else snprintf(e, sizeof e, "%s##%d", b, i);
                if (ImGui::Selectable(e, i == cur)) { c->tp_stg = r->stg; c->tp_rom = r->rom; c->tp_pos = 0; }
                if (i == cur && ImGui::IsWindowAppearing()) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (cur >= 0) {
            int m = tp_entries(cur);
            if (c->tp_pos >= m) c->tp_pos = 0;
            if (ImGui::BeginCombo("Entry", tp_entry_name(cur, c->tp_pos, b, sizeof b), ImGuiComboFlags_HeightLarge)) {
                for (int p = 0; p < m; p++) {
                    snprintf(e, sizeof e, "%s##%d", tp_entry_name(cur, p, b, sizeof b), p);
                    if (ImGui::Selectable(e, p == c->tp_pos)) c->tp_pos = p;
                    if (ImGui::IsItemHovered() && tp_entry_sources(cur, p, b, sizeof b)[0]) ImGui::SetTooltip("Doors here from:\n%s", b);
                }
                ImGui::EndCombo();
            }
        }
        ImGui::TextDisabled("A room may not work in every story state.");
    } else {
        if (!n) ImGui::TextDisabled(tp_reading() ? "Reading the room list from the disc..." : "The room list could not be read.");
        ImGui::TextDisabled("Uses the game's door transition; an invalid room or position may crash.");
        ImGui::InputInt("Stage", &c->tp_stg);
        ImGui::InputInt("Room", &c->tp_rom);
        ImGui::InputInt("Position", &c->tp_pos);
        c->tp_stg = SDL_clamp(c->tp_stg, 0, 9); c->tp_rom = SDL_clamp(c->tp_rom, 0, 99); c->tp_pos = SDL_clamp(c->tp_pos, 0, TP_POS - 1);
    }
    ImGui::Checkbox("Raw ids", &raw);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stage, room and entry position as numbers (as RECVX_TELEPORT), also rooms not in the list.");
    ImGui::SameLine();
}

static void cheat_window(void)
{
    CheatState* c = &g_cheat;
    ImGui::SetNextWindowPos(ImVec2(500, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Cheats (F2)", &cheats_visible)) { ImGui::End(); return; }
    ImGui::Text("%s  HP %d   stage %d  room %d  pos %d", c->player ? "Chris" : "Claire", c->hp, c->stg, c->rom, c->pos);
    bool b = c->inf_health;
    if (ImGui::Checkbox("Infinite health", &b)) c->inf_health = b;
    ImGui::SameLine();
    if (ImGui::Button("Heal now")) __atomic_or_fetch(&c->request, CHEAT_HEAL, __ATOMIC_ACQ_REL);
    b = c->inf_ammo;
    if (ImGui::Checkbox("Infinite ammo", &b)) c->inf_ammo = b;
    {
        static const char* modes[] = { "Off", "Through walls", "Through walls + fly (R/F, PgUp/PgDn, R2/L2)" };
        int m = c->noclip;
        if (ImGui::Combo("Noclip", &m, modes, IM_ARRAYSIZE(modes))) c->noclip = m;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Walls, objects and enemies no longer block you; doors and triggers still work.\n"
                              "Fly: R / Page Up / R2 up, F / Page Down / L2 down.\n"
                              "Turning it off after flying puts you on the floor level below.");
    }
    b = c->unlock;
    if (ImGui::Checkbox("Unlock all", &b)) c->unlock = b;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Examining a locked door, card reader or key slot uses its key there, as if you carried it.\n"
                          "Nothing is added to the inventory. Doors locked by the story stay locked.\n"
                          "Turned on inside a room, some doors notice it only after you enter the room again.");

    ImGui::SeparatorText("Give item");
    char label[48];
    snprintf(label, sizeof label, "%3d  %s", c->give_id, c->give_id > 0 && c->give_id < CHEAT_ITEMS ? c->names[c->give_id] : "-");
    if (ImGui::BeginCombo("Item", label, ImGuiComboFlags_HeightLarge)) {
        for (int id = 1; id < CHEAT_ITEMS; id++) {
            if (!c->names[id][0]) continue;
            snprintf(label, sizeof label, "%3d  %s", id, c->names[id]);
            if (ImGui::Selectable(label, id == c->give_id)) c->give_id = id;
        }
        ImGui::EndCombo();
    }
    ImGui::InputInt("Count", &c->give_count);
    c->give_count = SDL_clamp(c->give_count, 1, 999);
    if (ImGui::Button("Add to inventory") && c->give_id) __atomic_or_fetch(&c->request, CHEAT_GIVE, __ATOMIC_ACQ_REL);

    ImGui::SeparatorText("Inventory");
    for (int i = 1; i < 2 + c->slots && i < 12; i++) {
        unsigned id = c->inv[i] >> 16 & 0xFF;
        char nm[16];
        if (!id) continue;
        if (id >= CHEAT_ITEMS || !c->names[id][0]) SDL_snprintf(nm, sizeof nm, "item %u", id);
        ImGui::Text("%s %-24s %u", (unsigned)i == c->inv[0] ? "equipped" : "        ", id < CHEAT_ITEMS && c->names[id][0] ? c->names[id] : nm, c->inv[i] & 0xFFFF);
    }

    ImGui::SeparatorText("Spawn enemy");
    if (c->spawn_idx < 0 || c->spawn_idx >= spawn_count()) c->spawn_idx = 0;
    if (ImGui::BeginCombo("Enemy", spawn_name(c->spawn_idx), ImGuiComboFlags_HeightLarge)) {
        for (int i = 0; i < spawn_count(); i++)
            if (ImGui::Selectable(spawn_name(i), i == c->spawn_idx)) c->spawn_idx = i;
        ImGui::EndCombo();
    }
    if (ImGui::Button("Spawn in front of me")) __atomic_or_fetch(&c->request, CHEAT_SPAWN, __ATOMIC_ACQ_REL);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Loads the enemy from another room if this one has no such enemy.\n"
                          "Bosses and scripted enemies may not work outside their room (docs/ENEMIES.md).\n"
                          "Spawned enemies are gone after leaving the room; their kills are not recorded.");

    ImGui::SeparatorText("Teleport");
    teleport_pick(c);
    if (ImGui::Button("Current room")) { c->tp_stg = c->stg; c->tp_rom = c->rom; c->tp_pos = c->pos; }
    ImGui::SameLine();
    if (ImGui::Button("Go")) __atomic_or_fetch(&c->request, CHEAT_TELEPORT, __ATOMIC_ACQ_REL);
    if (c->result) ImGui::TextDisabled("%s", c->result);
    ImGui::End();
}

void overlay_render(void)
{
    if (!ready) return;
    bool windows = visible || cheats_visible;
    pad_set_game_input_enabled(!windows);
    tps_update_mouse_mode(win, windows);
    menu_mouse_update(win, windows);
    /* without a window ImGui leaves the cursor alone (menu_mouse_update shows/hides it) */
    if (windows) ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
    else ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    bool crosshair = g_tps.on && g_tps.crosshair && g_tps.cam_active && g_tps.aiming;
    if (!visible && !cheats_visible && !g_config.show_fps && !crosshair) return;
    {   /* readable on large displays: scale text with the window height (1080p = 1x) */
        int ww, wh;
        SDL_GetWindowSizeInPixels(win, &ww, &wh);
        ImGui::GetStyle().FontScaleDpi = wh > 1080 ? wh / 1080.0f : 1.0f;
    }
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    if (visible) settings_window();
    if (cheats_visible) cheat_window();
    if (crosshair) {   /* third person: the screen centre is where the shot goes (tps_glue.c) */
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        ImVec2 c(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
        float s = ImGui::GetIO().DisplaySize.y / 120.0f, g = s * 0.6f, t = s > 8 ? 2.0f : 1.5f;
        ImU32 col = IM_COL32(255, 60, 40, 220), shadow = IM_COL32(0, 0, 0, 160);
        for (int pass = 0; pass < 2; pass++) {
            ImU32 k = pass ? col : shadow;
            float o = pass ? 0.0f : 1.0f;
            dl->AddLine(ImVec2(c.x - g - s + o, c.y + o), ImVec2(c.x - g + o, c.y + o), k, t);
            dl->AddLine(ImVec2(c.x + g + o, c.y + o), ImVec2(c.x + g + s + o, c.y + o), k, t);
            dl->AddLine(ImVec2(c.x + o, c.y - g - s + o), ImVec2(c.x + o, c.y - g + o), k, t);
            dl->AddLine(ImVec2(c.x + o, c.y + g + o), ImVec2(c.x + o, c.y + g + s + o), k, t);
            dl->AddCircleFilled(ImVec2(c.x + o, c.y + o), t, k);
        }
    }
    if (g_config.show_fps) {
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 10, 10), ImGuiCond_Always, ImVec2(1, 0));
        ImGui::SetNextWindowBgAlpha(0.4f);
        ImGui::Begin("##fps", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                                       ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::Text("%.1f fps", ImGui::GetIO().Framerate);
        ImGui::End();
    }
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
