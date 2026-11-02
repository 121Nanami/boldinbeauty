"""Shared helper for the PIE verify scripts since the 2D lobby (WBP_Hub / UBIBHubWidget) was deleted on 2026-10-07 (owner
request: the Ark is the hub). The menus' behaviour was always the engine-independent page model (BIBRules/BIBPages.h); the
Ark's panels show the same pages. `hub()` returns an object with the old widget's page calls (click_action, get_grid_id, ...)
driving that model through UBIBArkLibrary, so the older scripts keep their checks. It is None until the player walks the Ark.

    import bib_hub
    h = bib_hub.hub()            # None while travelling / signing in
    h.click_action("tab:6")      # what a tile or a panel button does
"""

import unreal

ARK = unreal.BIBArkLibrary


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def controller():
    w = world()
    return unreal.GameplayStatics.get_player_controller(w, 0) if w else None


def in_ark():
    """The player walks the Ark (the hub) with no stage running."""
    pc = controller()
    return pc is not None and ARK.is_in_ark(pc)


class Hub:
    def __init__(self, pc):
        self.pc = pc

    def click_action(self, action_id):
        ARK.panel_action(self.pc, action_id)

    def click_nav(self, tab):
        ARK.panel_action(self.pc, "tab:%d" % tab)

    def click_back(self):
        ARK.panel_action(self.pc, "back")

    def get_status_line(self):
        return str(ARK.get_status_line(self.pc))

    def get_page_title(self):
        return str(ARK.get_panel_title(self.pc))

    def get_current_tab(self):
        return int(ARK.get_page_tab(self.pc))

    def get_grid_count(self):
        return len(ARK.get_page_grid_ids(self.pc))

    def get_grid_id(self, index):
        ids = ARK.get_page_grid_ids(self.pc)
        return str(ids[index]) if 0 <= index < len(ids) else ""

    def get_bar_count(self):
        return len(ARK.get_page_bar_ids(self.pc))

    def get_bar_id(self, index):
        ids = ARK.get_page_bar_ids(self.pc)
        return str(ids[index]) if 0 <= index < len(ids) else ""


def hub():
    pc = controller()
    if pc is None or not ARK.is_in_ark(pc):
        return None
    return Hub(pc)
