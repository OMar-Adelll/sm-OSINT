#!/usr/bin/env python3
"""A terminal UI frontend for the existing sm-osint command-line program.

This file deliberately contains no database or discovery logic.  It is a thin
interactive layer over ``sm-osint --add``, ``--query``, and ``--stats``.
"""

from __future__ import annotations

import curses
import os
from pathlib import Path
import shutil
import subprocess
import sys
from typing import Iterable


PROGRAM = Path(os.environ.get("SM_OSINT_BIN", Path(__file__).with_name("sm-osint")))
MIN_ROWS = 16
MIN_COLS = 58


def command_prefix() -> list[str]:
    """Return the command used to reach the untouched command-line app."""
    if PROGRAM.exists():
        return [str(PROGRAM)]
    found = shutil.which("sm-osint")
    return [found] if found else [str(PROGRAM)]


def run_app(arguments: list[str]) -> tuple[int, list[str]]:
    try:
        result = subprocess.run(
            [*command_prefix(), *arguments],
            text=True,
            capture_output=True,
            check=False,
        )
    except OSError as error:
        return 1, [
            "Could not start sm-osint.",
            f"Expected executable: {PROGRAM}",
            str(error),
            "Set SM_OSINT_BIN to use an executable elsewhere.",
        ]

    output = (result.stdout + result.stderr).strip()
    return result.returncode, output.splitlines() or ["The command returned no output."]


def clipped(lines: Iterable[str], width: int) -> list[str]:
    return [line[: max(0, width - 4)] for line in lines]


class App:
    def __init__(self, screen: curses.window) -> None:
        self.screen = screen
        self.message = "Ready. Select an action with ↑/↓, then press Enter."
        self.output: list[str] = []
        self.selection = 0
        self.actions = [
            ("Search usernames", self.search),
            ("Add encrypted username", self.add),
            ("View data statistics", self.stats),
            ("Quit", self.quit),
        ]

    def draw(self) -> None:
        self.screen.erase()
        rows, cols = self.screen.getmaxyx()
        if rows < MIN_ROWS or cols < MIN_COLS:
            self.screen.addstr(0, 0, f"Resize terminal to at least {MIN_COLS}×{MIN_ROWS}.")
            self.screen.refresh()
            return

        self.screen.attron(curses.A_BOLD)
        self.screen.addstr(1, 2, "sm-OSINT")
        self.screen.attroff(curses.A_BOLD)
        self.screen.addstr(2, 2, "Username discovery terminal interface")
        self.screen.hline(3, 2, curses.ACS_HLINE, cols - 4)

        for index, (label, _) in enumerate(self.actions):
            attribute = curses.A_REVERSE if index == self.selection else curses.A_NORMAL
            self.screen.addstr(5 + index, 4, label.ljust(30), attribute)

        self.screen.hline(10, 2, curses.ACS_HLINE, cols - 4)
        self.screen.attron(curses.A_BOLD)
        self.screen.addstr(11, 2, "Status")
        self.screen.attroff(curses.A_BOLD)
        self.screen.addstr(12, 2, self.message[: cols - 4])

        output_rows = rows - 16
        if self.output:
            self.screen.attron(curses.A_BOLD)
            self.screen.addstr(14, 2, "Result")
            self.screen.attroff(curses.A_BOLD)
            for index, line in enumerate(clipped(self.output[-output_rows:], cols)):
                self.screen.addstr(15 + index, 2, line)
        self.screen.addstr(rows - 1, 2, "↑/↓ Navigate   Enter Select   q Quit")
        self.screen.refresh()

    def prompt(self, label: str, default: str = "") -> str | None:
        rows, cols = self.screen.getmaxyx()
        prompt = f"{label}: "
        self.screen.move(rows - 3, 2)
        self.screen.clrtoeol()
        self.screen.addstr(rows - 3, 2, prompt)
        self.screen.addstr(rows - 3, len(prompt) + 2, default)
        self.screen.move(rows - 3, len(prompt) + 2 + len(default))
        self.screen.refresh()
        curses.echo()
        curses.curs_set(1)
        try:
            value = self.screen.getstr(rows - 3, len(prompt) + 2, cols - len(prompt) - 5)
            return value.decode("utf-8").strip()
        except UnicodeDecodeError:
            self.message = "Please enter valid UTF-8 text."
            return None
        finally:
            curses.noecho()
            curses.curs_set(0)

    def search(self) -> None:
        username = self.prompt("Username or prefix")
        if not username:
            self.message = "Search cancelled: enter a username or prefix."
            return
        limit = self.prompt("Maximum results", "10")
        if limit is None or not limit:
            self.message = "Search cancelled."
            return
        if not limit.isdigit() or int(limit) < 1:
            self.message = "Maximum results must be a positive whole number."
            return
        status, self.output = run_app(["--query", username, limit])
        self.message = "Search complete." if status == 0 else f"Search failed (exit code {status})."

    def add(self) -> None:
        username = self.prompt("Username to store")
        if not username:
            self.message = "Add cancelled: enter a username."
            return
        status, self.output = run_app(["--add", username])
        self.message = "Username stored." if status == 0 else f"Could not store username (exit code {status})."

    def stats(self) -> None:
        status, self.output = run_app(["--stats"])
        self.message = "Statistics loaded." if status == 0 else f"Could not load statistics (exit code {status})."

    def quit(self) -> None:
        raise SystemExit

    def run(self) -> None:
        curses.curs_set(0)
        self.screen.keypad(True)
        while True:
            self.draw()
            key = self.screen.getch()
            if key in (ord("q"), ord("Q")):
                return
            if key == curses.KEY_UP:
                self.selection = (self.selection - 1) % len(self.actions)
            elif key == curses.KEY_DOWN:
                self.selection = (self.selection + 1) % len(self.actions)
            elif key in (curses.KEY_ENTER, 10, 13):
                self.actions[self.selection][1]()


def main() -> int:
    if not sys.stdin.isatty() or not sys.stdout.isatty():
        print("tui.py must be started from an interactive terminal.", file=sys.stderr)
        return 1
    curses.wrapper(lambda screen: App(screen).run())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
