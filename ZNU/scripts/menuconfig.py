#!/usr/bin/env python3
"""Nothing here yet.
"""

import curses
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CONFIG_PATH = ROOT / ".config"

INSTRUCTIONS = (
    "Arrow keys navigate the menu. <Enter> selects submenus --->. "
    "Press <Esc> to go back or exit, <?> for Help, </> for Search. "
    "These menus only organize the tree; no options are defined yet."
)


class KconfigError(Exception):
    pass


class Comment:
    def __init__(self, text):
        self.text = text


class Menu:
    def __init__(self, title):
        self.title = title
        self.parent = None
        self.children = []

    def path(self):
        parts = []
        node = self
        while node.parent is not None:
            parts.append(node.title)
            node = node.parent
        return "/" + "/".join(reversed(parts))


def _unquote(text, filename, lineno):
    text = text.strip()
    if len(text) < 2 or text[0] != '"' or text[-1] != '"':
        raise KconfigError(f"{filename}:{lineno}: expected a quoted string")
    return text[1:-1]


def _parse_file(path, stack):
    filename = path.relative_to(ROOT)
    depth = len(stack)
    try:
        lines = path.read_text().splitlines()
    except OSError as exc:
        raise KconfigError(f"cannot read {filename}: {exc}") from exc

    for lineno, raw in enumerate(lines, 1):
        stripped = raw.strip()
        if not stripped or stripped.startswith("#"):
            continue
        keyword, _, rest = stripped.partition(" ")
        keyword = keyword.lower()
        rest = rest.strip()
        current = stack[-1]

        if keyword == "mainmenu":
            if len(stack) != 1 or stack[0].children:
                raise KconfigError(
                    f"{filename}:{lineno}: mainmenu must come first"
                )
            stack[0].title = _unquote(rest, filename, lineno)
        elif keyword == "source":
            rel = _unquote(rest, filename, lineno)
            _parse_file(ROOT / rel, stack)
        elif keyword == "menu":
            menu = Menu(_unquote(rest, filename, lineno))
            menu.parent = current
            current.children.append(menu)
            stack.append(menu)
        elif keyword == "endmenu":
            if rest or len(stack) <= depth:
                raise KconfigError(f"{filename}:{lineno}: unmatched endmenu")
            stack.pop()
        elif keyword == "comment":
            current.children.append(Comment(_unquote(rest, filename, lineno)))
        else:
            raise KconfigError(
                f"{filename}:{lineno}: unsupported statement '{keyword}'"
            )

    if len(stack) != depth:
        raise KconfigError(f"{filename}: menu is missing endmenu")


def load_kconfig(path):
    root = Menu("")
    _parse_file(path, [root])
    if not root.title:
        root.title = "ZNU Kernel Configuration"
    return root


def write_config(mainmenu):
    CONFIG_PATH.write_text(
        "#\n"
        "# Automatically generated file; DO NOT EDIT.\n"
        f"# {mainmenu}\n"
        "#\n"
    )


def _clip_add(win, y, x, text, attr=0):
    height, width = win.getmaxyx()
    if y < 0 or x < 0 or y >= height or x >= width or not text:
        return
    room = width - x
    if y == height - 1:
        room -= 1
    if room <= 0:
        return
    try:
        win.addstr(y, x, text[:room], attr)
    except curses.error:
        pass


def _wrap(text, width):
    if width <= 0:
        return [""]
    lines = []
    current = ""
    for word in text.split():
        if not current:
            current = word
        elif len(current) + 1 + len(word) <= width:
            current += " " + word
        else:
            lines.append(current)
            current = word
    if current:
        lines.append(current)
    return lines or [""]


class UI:
    BUTTONS = ("Select", "Exit", "Help", "Save")

    def __init__(self, stdscr, root):
        self.stdscr = stdscr
        self.root = root
        self.stack = [root]
        self.cursor = [0]
        self.scroll = [0]
        self.focus = "items"
        self.button = 0
        self._init_colors()

    def _init_colors(self):
        self.color = curses.has_colors()
        if not self.color:
            self.screen_attr = curses.A_NORMAL
            self.dialog_attr = curses.A_NORMAL
            self.title_attr = curses.A_BOLD
            self.selected_attr = curses.A_REVERSE
            self.button_attr = curses.A_REVERSE
            self.inactive_attr = curses.A_NORMAL
            self.shadow_attr = curses.A_DIM
            return
        curses.start_color()
        curses.init_pair(1, curses.COLOR_CYAN, curses.COLOR_BLUE)
        curses.init_pair(2, curses.COLOR_BLACK, curses.COLOR_WHITE)
        curses.init_pair(3, curses.COLOR_YELLOW, curses.COLOR_WHITE)
        curses.init_pair(4, curses.COLOR_WHITE, curses.COLOR_BLUE)
        curses.init_pair(5, curses.COLOR_YELLOW, curses.COLOR_BLUE)
        curses.init_pair(6, curses.COLOR_BLACK, curses.COLOR_BLACK)
        self.screen_attr = curses.color_pair(1)
        self.dialog_attr = curses.color_pair(2)
        self.title_attr = curses.color_pair(3) | curses.A_BOLD
        self.selected_attr = curses.color_pair(4) | curses.A_BOLD
        self.button_attr = curses.color_pair(5) | curses.A_BOLD
        self.inactive_attr = curses.color_pair(2)
        self.shadow_attr = curses.color_pair(6)

    def current(self):
        return self.stack[-1]

    def _clamp(self):
        items = self.current().children
        if not items:
            self.cursor[-1] = 0
            self.scroll[-1] = 0
            return
        self.cursor[-1] = max(0, min(self.cursor[-1], len(items) - 1))

    def run(self):
        curses.curs_set(0)
        self.stdscr.keypad(True)
        # A click is a mouse event, not Esc. Esc was closing the menu immediately.
        curses.mousemask(curses.ALL_MOUSE_EVENTS)
        curses.mouseinterval(0)
        while True:
            self._clamp()
            self.draw()
            key = self.stdscr.getch()
            if key in (curses.KEY_RESIZE, curses.KEY_MOUSE, -1):
                continue
            if self.focus == "buttons":
                if self._button_key(key):
                    return
                continue
            if self._item_key(key):
                return

    def _item_key(self, key):
        items = self.current().children
        if key in (curses.KEY_UP, ord("k")) and items:
            self.cursor[-1] = (self.cursor[-1] - 1) % len(items)
        elif key in (curses.KEY_DOWN, ord("j")) and items:
            self.cursor[-1] = (self.cursor[-1] + 1) % len(items)
        elif key in (curses.KEY_ENTER, ord("\n"), ord(" "), curses.KEY_RIGHT):
            self._activate_item()
        elif key in (27, curses.KEY_LEFT, curses.KEY_BACKSPACE):
            return self._back()
        elif key in (ord("\t"),):
            self.focus = "buttons"
        elif key in (ord("q"), ord("Q")):
            return self._confirm_exit()
        elif key in (ord("s"), ord("S")):
            self._save()
        elif key in (ord("?"), ord("h"), ord("H"), curses.KEY_F1):
            self._help()
        elif key == ord("/"):
            self._search()
        elif ord("a") <= key <= ord("z") or ord("A") <= key <= ord("Z"):
            self._jump_letter(chr(key).lower())
        return False

    def _button_key(self, key):
        if key in (curses.KEY_LEFT, ord("h")):
            self.button = (self.button - 1) % len(self.BUTTONS)
        elif key in (curses.KEY_RIGHT, ord("l"), ord("\t")):
            self.button = (self.button + 1) % len(self.BUTTONS)
        elif key in (curses.KEY_UP, curses.KEY_DOWN, 27):
            self.focus = "items"
        elif key in (curses.KEY_ENTER, ord("\n"), ord(" ")):
            return self._activate_button()
        elif key in (ord("q"), ord("Q")):
            return self._confirm_exit()
        elif key in (ord("?"), curses.KEY_F1):
            self._help()
        elif key == ord("/"):
            self.focus = "items"
            self._search()
        return False

    def _activate_button(self):
        name = self.BUTTONS[self.button]
        if name == "Select":
            self.focus = "items"
            self._activate_item()
        elif name == "Exit":
            return self._confirm_exit()
        elif name == "Help":
            self._help()
        elif name == "Save":
            self._save()
        return False

    def _activate_item(self):
        items = self.current().children
        if not items:
            return
        item = items[self.cursor[-1]]
        if isinstance(item, Menu):
            self.stack.append(item)
            self.cursor.append(0)
            self.scroll.append(0)

    def _back(self):
        if len(self.stack) == 1:
            return self._confirm_exit()
        self.stack.pop()
        self.cursor.pop()
        self.scroll.pop()
        self.focus = "items"
        return False

    def _selected_item(self):
        items = self.current().children
        if not items:
            return None
        return items[self.cursor[-1]]

    def draw(self):
        # Draw on stdscr itself. getch() refreshes that window, and a separate
        # window was being covered by the blue background a moment later.
        screen = self.stdscr
        screen.bkgd(" ", self.screen_attr)
        screen.erase()
        height, width = screen.getmaxyx()
        if height < 16 or width < 60:
            _clip_add(screen, 0, 0, "menuconfig needs at least 60x16", self.screen_attr)
            screen.refresh()
            return

        items = self.current().children
        box_w = min(72, width - 4)
        instr = _wrap(INSTRUCTIONS, box_w - 6)[:3]
        list_h = min(max(len(items), 1), height - len(instr) - 10)
        list_h = max(1, list_h)
        box_h = len(instr) + list_h + 8
        if box_h > height:
            list_h = max(1, height - len(instr) - 8)
            box_h = len(instr) + list_h + 8
        top = max(0, (height - box_h) // 2)
        left = max(0, (width - box_w) // 2)

        if top + 1 + box_h <= height and left + 2 + box_w <= width:
            self._fill(screen, top + 1, left + 2, box_h, box_w, self.shadow_attr)
        self._fill(screen, top, left, box_h, box_w, self.dialog_attr)
        self._box(screen, top, left, box_h, box_w, self.dialog_attr)

        title = self.root.title if len(self.stack) == 1 else self.current().path()
        label = f" {title} "
        if len(label) > box_w - 4:
            label = " " + title[: box_w - 6] + ".. "
        _clip_add(screen, top, left + max(2, (box_w - len(label)) // 2), label, self.title_attr)

        row = top + 1
        for line in instr:
            _clip_add(screen, row, left + 2, line, self.dialog_attr)
            row += 1
        row += 1

        self._draw_list(screen, row, left + 2, list_h, box_w - 4, items)
        self._draw_buttons(screen, top + box_h - 2, left, box_w)
        screen.refresh()

    def _draw_list(self, win, y, x, list_h, list_w, items):
        win.attrset(self.dialog_attr)
        win.hline(y, x + 1, curses.ACS_HLINE, list_w - 2)
        win.hline(y + list_h + 1, x + 1, curses.ACS_HLINE, list_w - 2)
        win.vline(y + 1, x, curses.ACS_VLINE, list_h)
        win.vline(y + 1, x + list_w - 1, curses.ACS_VLINE, list_h)
        for corner, cy, cx in (
            (curses.ACS_ULCORNER, y, x),
            (curses.ACS_URCORNER, y, x + list_w - 1),
            (curses.ACS_LLCORNER, y + list_h + 1, x),
            (curses.ACS_LRCORNER, y + list_h + 1, x + list_w - 1),
        ):
            try:
                win.addch(cy, cx, corner, self.dialog_attr)
            except curses.error:
                pass

        if not items:
            return
        cursor = self.cursor[-1]
        scroll = self.scroll[-1]
        if cursor < scroll:
            scroll = cursor
        if cursor >= scroll + list_h:
            scroll = cursor - list_h + 1
        self.scroll[-1] = scroll

        inner = list_w - 2
        for slot in range(list_h):
            index = scroll + slot
            if index >= len(items):
                break
            item = items[index]
            selected = self.focus == "items" and index == cursor
            attr = self.selected_attr if selected else self.dialog_attr
            if isinstance(item, Menu):
                text = f" {item.path()}  --->"
            else:
                text = f"*** {item.text} ***"
                text = text.center(inner)
            text = text[:inner].ljust(inner)
            _clip_add(win, y + 1 + slot, x + 1, text, attr)

        if scroll > 0:
            try:
                win.addch(y, x + list_w - 2, curses.ACS_UARROW, self.title_attr)
            except curses.error:
                pass
        if scroll + list_h < len(items):
            try:
                win.addch(y + list_h + 1, x + list_w - 2, curses.ACS_DARROW, self.title_attr)
            except curses.error:
                pass

    def _fill(self, win, y, x, height, width, attr):
        blank = " " * width
        for row in range(height):
            _clip_add(win, y + row, x, blank, attr)

    def _box(self, win, y, x, height, width, attr):
        win.attrset(attr)
        try:
            win.hline(y, x + 1, curses.ACS_HLINE, width - 2)
            win.hline(y + height - 1, x + 1, curses.ACS_HLINE, width - 2)
            win.vline(y + 1, x, curses.ACS_VLINE, height - 2)
            win.vline(y + 1, x + width - 1, curses.ACS_VLINE, height - 2)
            win.addch(y, x, curses.ACS_ULCORNER, attr)
            win.addch(y, x + width - 1, curses.ACS_URCORNER, attr)
            win.addch(y + height - 1, x, curses.ACS_LLCORNER, attr)
            win.addch(y + height - 1, x + width - 1, curses.ACS_LRCORNER, attr)
        except curses.error:
            pass

    def _draw_buttons(self, win, y, x0, box_w):
        labels = []
        for index, name in enumerate(self.BUTTONS):
            shown = f" < {name} > "
            labels.append(shown)
        gap = 2
        total = sum(len(label) for label in labels) + gap * (len(labels) - 1)
        x = x0 + max(2, (box_w - total) // 2)
        for index, label in enumerate(labels):
            active = self.focus == "buttons" and index == self.button
            attr = self.button_attr if active else self.inactive_attr
            _clip_add(win, y, x, label, attr)
            x += len(label) + gap

    def _dialog(self, title, body_lines, buttons, selected=0):
        height, width = self.stdscr.getmaxyx()
        body_lines = body_lines[: max(1, height - 8)]
        inner = max(len(title) + 4, max((len(line) for line in body_lines), default=24), 24)
        box_w = min(width - 4, max(20, inner + 4))
        box_h = min(height, len(body_lines) + 6)
        top = max(0, (height - box_h) // 2)
        left = max(0, (width - box_w) // 2)
        win = curses.newwin(box_h, box_w, top, left)
        win.keypad(True)
        while True:
            win.bkgd(" ", self.dialog_attr)
            win.erase()
            win.attrset(self.dialog_attr)
            win.box()
            label = f" {title} "
            _clip_add(win, 0, max(2, (box_w - len(label)) // 2), label, self.title_attr)
            for row, line in enumerate(body_lines, 1):
                _clip_add(win, row, 2, line[: box_w - 4], self.dialog_attr)
            labels = [f" < {name} > " for name in buttons]
            total = sum(len(item) for item in labels) + 2 * (len(labels) - 1)
            x = max(2, (box_w - total) // 2)
            button_y = box_h - 2
            for index, item in enumerate(labels):
                attr = self.button_attr if index == selected else self.inactive_attr
                _clip_add(win, button_y, x, item, attr)
                x += len(item) + 2
            win.refresh()
            key = win.getch()
            if key in (curses.KEY_MOUSE, curses.KEY_RESIZE, -1):
                continue
            if key in (curses.KEY_LEFT, curses.KEY_UP):
                selected = (selected - 1) % len(buttons)
            elif key in (curses.KEY_RIGHT, curses.KEY_DOWN, ord("\t")):
                selected = (selected + 1) % len(buttons)
            elif key in (curses.KEY_ENTER, ord("\n"), ord(" ")):
                return buttons[selected]
            elif key == 27:
                return None
            else:
                for index, name in enumerate(buttons):
                    if key in (ord(name[0].lower()), ord(name[0].upper())):
                        return name
        return None

    def _confirm_exit(self):
        choice = self._dialog(
            "Save",
            ["Do you wish to save your new configuration?"],
            ["Yes", "No"],
            selected=0,
        )
        if choice is None:
            return False
        if choice == "Yes":
            write_config(self.root.title)
        return True

    def _save(self):
        write_config(self.root.title)
        self._dialog(
            "Save",
            [f"Configuration written to {CONFIG_PATH.name}"],
            ["OK"],
        )

    def _help(self):
        item = self._selected_item()
        lines = [
            "Menus follow the kernel source tree.",
            "Nothing in them can be toggled yet.",
            "",
            "Up/Down     Move",
            "Enter       Open a submenu",
            "Esc/Left    Go back, or exit from the top",
            "Tab         Move to the buttons",
            "?           Help",
            "/           Search menus",
            "S           Save .config",
            "Q           Quit",
        ]
        if isinstance(item, Menu):
            lines[0:0] = [item.path(), ""]
        elif isinstance(item, Comment):
            lines[0:0] = [item.text, ""]
        self._dialog("Help", lines, ["OK"])

    def _search(self):
        query = self._input("Search", "Menu name or path:")
        if query is None:
            return
        needle = query.strip().casefold().lstrip("/")
        if not needle:
            return
        matches = []

        def walk(menu):
            for child in menu.children:
                if isinstance(child, Menu):
                    if needle in child.path().casefold().lstrip("/"):
                        matches.append(child)
                    walk(child)

        walk(self.root)
        if not matches:
            self._dialog("Search", [f"No menus match '{query}'"], ["OK"])
            return
        if len(matches) == 1:
            self._jump(matches[0])
            return
        labels = [menu.path() for menu in matches]
        choice = self._choose_list("Search", labels)
        if choice is not None:
            self._jump(matches[choice])

    def _input(self, title, prompt):
        height, width = self.stdscr.getmaxyx()
        box_w = min(64, width - 4)
        box_h = 7
        top = max(0, (height - box_h) // 2)
        left = max(0, (width - box_w) // 2)
        win = curses.newwin(box_h, box_w, top, left)
        win.keypad(True)
        curses.curs_set(1)
        text = ""
        try:
            while True:
                win.bkgd(" ", self.dialog_attr)
                win.erase()
                win.box()
                _clip_add(win, 0, 2, f" {title} ", self.title_attr)
                _clip_add(win, 2, 2, prompt, self.dialog_attr)
                field = text
                room = box_w - 6
                if len(field) > room:
                    field = field[-room:]
                _clip_add(win, 3, 2, field.ljust(room), self.selected_attr)
                win.refresh()
                key = win.getch()
                if key in (curses.KEY_MOUSE, curses.KEY_RESIZE, -1):
                    continue
                if key in (curses.KEY_ENTER, ord("\n")):
                    return text
                if key == 27:
                    return None
                if key in (curses.KEY_BACKSPACE, 127, 8):
                    text = text[:-1]
                elif 32 <= key <= 126:
                    text += chr(key)
        finally:
            curses.curs_set(0)

    def _choose_list(self, title, labels):
        height, width = self.stdscr.getmaxyx()
        box_w = min(width - 4, max(len(title), max(len(item) for item in labels)) + 6)
        list_h = min(len(labels), height - 8)
        box_h = list_h + 4
        top = max(0, (height - box_h) // 2)
        left = max(0, (width - box_w) // 2)
        win = curses.newwin(box_h, box_w, top, left)
        win.keypad(True)
        selected = 0
        while True:
            win.bkgd(" ", self.dialog_attr)
            win.erase()
            win.box()
            _clip_add(win, 0, 2, f" {title} ", self.title_attr)
            start = 0
            if selected >= list_h:
                start = selected - list_h + 1
            for slot in range(list_h):
                index = start + slot
                if index >= len(labels):
                    break
                attr = self.selected_attr if index == selected else self.dialog_attr
                _clip_add(win, 1 + slot, 2, labels[index][: box_w - 4].ljust(box_w - 4), attr)
            win.refresh()
            key = win.getch()
            if key in (curses.KEY_MOUSE, curses.KEY_RESIZE, -1):
                continue
            if key in (curses.KEY_UP, ord("k")):
                selected = (selected - 1) % len(labels)
            elif key in (curses.KEY_DOWN, ord("j")):
                selected = (selected + 1) % len(labels)
            elif key in (curses.KEY_ENTER, ord("\n"), ord(" ")):
                return selected
            elif key == 27:
                return None

    def _jump(self, menu):
        chain = []
        node = menu
        while node.parent is not None:
            chain.append(node)
            node = node.parent
        chain.reverse()
        self.stack = [self.root]
        self.cursor = []
        self.scroll = []
        parent = self.root
        for menu in chain:
            index = next(
                i for i, child in enumerate(parent.children) if child is menu
            )
            self.cursor.append(index)
            self.scroll.append(0)
            self.stack.append(menu)
            parent = menu
        self.cursor.append(0)
        self.scroll.append(0)
        self.focus = "items"

    def _jump_letter(self, letter):
        items = self.current().children
        if not items:
            return
        start = self.cursor[-1]
        for offset in range(1, len(items) + 1):
            index = (start + offset) % len(items)
            item = items[index]
            name = item.path() if isinstance(item, Menu) else item.text
            if letter in name.casefold():
                self.cursor[-1] = index
                return


def main():
    if not sys.stdin.isatty() or not sys.stdout.isatty():
        sys.stderr.write("menuconfig: this target needs an interactive terminal\n")
        return 1
    os.environ.setdefault("ESCDELAY", "25")
    try:
        root = load_kconfig(ROOT / "Kconfig")
    except KconfigError as exc:
        sys.stderr.write(f"menuconfig: {exc}\n")
        return 1
    try:
        curses.wrapper(lambda stdscr: UI(stdscr, root).run())
    except KeyboardInterrupt:
        return 130
    return 0


if __name__ == "__main__":
    sys.exit(main())
