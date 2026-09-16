#!/usr/bin/env python3
"""Single-username terminal interface for the sm-OSINT discovery pipeline."""

from __future__ import annotations

import curses
import os
from pathlib import Path
import shutil
import subprocess
import sys
from typing import Iterable


ROOT = Path(__file__).resolve().parent
MIN_ROWS = 20
MIN_COLS = 66


def project_environment() -> dict[str, str]:
    """Load simple KEY=VALUE entries from the project's ignored .env file.

    Values already exported by the caller win, so the TUI never overrides an
    explicitly configured deployment environment.
    """
    environment = os.environ.copy()
    env_file = ROOT / ".env"
    if not env_file.is_file():
        return environment
    for raw_line in env_file.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        key = key.strip()
        value = value.strip()
        if not key.isidentifier():
            continue
        if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
            value = value[1:-1]
        environment.setdefault(key, value)
    return environment


PROJECT_ENVIRONMENT = project_environment()


def executable(env_name: str, name: str) -> str:
    """Locate a configured executable, a local CMake build, or PATH binary."""
    configured = os.environ.get(env_name)
    if configured:
        return configured
    for candidate in (ROOT / name, ROOT / "build" / name):
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return str(candidate)
    return shutil.which(name) or str(ROOT / "build" / name)


def run_app(command: list[str]) -> tuple[int, list[str]]:
    try:
        result = subprocess.run(
            command, text=True, capture_output=True, check=False, env=PROJECT_ENVIRONMENT
        )
    except OSError as error:
        return 1, [f"Could not start {command[0]}.", str(error)]
    output = (result.stdout + result.stderr).strip()
    return result.returncode, output.splitlines() or ["The command returned no output."]


def clipped(lines: Iterable[str], width: int) -> list[str]:
    return [line[: max(0, width - 4)] for line in lines]


def visible_output(lines: list[str], offset: int, capacity: int) -> tuple[list[str], int, int]:
    """Return a scrollable viewport, where offset zero means the newest output."""
    capacity = max(1, capacity)
    start = max(0, len(lines) - capacity - offset)
    end = min(len(lines), start + capacity)
    return lines[start:end], start, end


class App:
    def __init__(self, screen: curses.window) -> None:
        self.screen = screen
        self.username = ""
        self.message = "Enter a username, then run the complete pipeline."
        self.output: list[str] = []
        self.output_offset = 0
        self.selection = 0
        self.actions = [
            ("Run complete pipeline", self.pipeline),
            ("Local discovery only", self.discovery),
            ("Public profile checks only", self.profile_checks),
            ("Quit", self.quit),
        ]

    def draw(self) -> None:
        self.screen.erase()
        rows, cols = self.screen.getmaxyx()
        if rows < MIN_ROWS or cols < MIN_COLS:
            self.screen.addstr(0, 0, f"Resize terminal to at least {MIN_COLS}x{MIN_ROWS}.")
            self.screen.refresh()
            return
        self.screen.attron(curses.A_BOLD)
        self.screen.addstr(1, 2, "sm-OSINT — username discovery")
        self.screen.attroff(curses.A_BOLD)
        self.screen.addstr(2, 2, "One username runs local discovery and public profile analysis.")
        self.screen.hline(3, 2, curses.ACS_HLINE, cols - 4)
        self.screen.addstr(5, 4, "Username: ")
        self.screen.attron(curses.A_UNDERLINE)
        self.screen.addstr(5, 14, (self.username or "(press e to enter)")[: cols - 18])
        self.screen.attroff(curses.A_UNDERLINE)
        for index, (label, _) in enumerate(self.actions):
            attr = curses.A_REVERSE if index == self.selection else curses.A_NORMAL
            self.screen.addstr(7 + index, 4, label.ljust(34), attr)
        self.screen.hline(12, 2, curses.ACS_HLINE, cols - 4)
        self.screen.attron(curses.A_BOLD)
        self.screen.addstr(13, 2, "Status")
        self.screen.attroff(curses.A_BOLD)
        self.screen.addstr(14, 2, self.message[: cols - 4])
        if self.output:
            self.screen.attron(curses.A_BOLD)
            visible, start, end = visible_output(self.output, self.output_offset, rows - 19)
            self.screen.addstr(16, 2, f"Output (lines {start + 1}-{end} of {len(self.output)})")
            self.screen.attroff(curses.A_BOLD)
            for index, line in enumerate(clipped(visible, cols)):
                self.screen.addstr(17 + index, 2, line)
        self.screen.addstr(rows - 1, 2, "PgUp/PgDn Scroll output   e Edit   Up/Down Navigate   Enter Select   q Quit")
        self.screen.refresh()

    def prompt_username(self) -> bool:
        rows, cols = self.screen.getmaxyx()
        self.screen.move(rows - 3, 2)
        self.screen.clrtoeol()
        self.screen.addstr(rows - 3, 2, "Username: ")
        self.screen.addstr(rows - 3, 12, self.username)
        self.screen.move(rows - 3, 12 + len(self.username))
        curses.echo()
        curses.curs_set(1)
        try:
            value = self.screen.getstr(rows - 3, 12, cols - 16).decode("utf-8").strip()
        except UnicodeDecodeError:
            self.message = "Please enter valid UTF-8 text."
            return False
        finally:
            curses.noecho()
            curses.curs_set(0)
        if not value:
            self.message = "Username is required."
            return False
        self.username = value
        return True

    def require_username(self) -> bool:
        return bool(self.username) or self.prompt_username()

    def set_output(self, lines: list[str]) -> None:
        self.output = lines
        self.output_offset = 0

    def scroll_output(self, direction: int) -> None:
        rows, _ = self.screen.getmaxyx()
        capacity = max(1, rows - 19)
        maximum = max(0, len(self.output) - capacity)
        self.output_offset = min(maximum, max(0, self.output_offset + direction * capacity))

    def discovery(self) -> None:
        if not self.require_username():
            return
        code, output = run_app([executable("SM_OSINT_BIN", "sm-osint"), "--query", self.username, "10"])
        self.set_output(output)
        self.message = "Local discovery complete." if code == 0 else f"Local discovery failed (exit {code})."

    def profile_checks(self) -> None:
        if not self.require_username():
            return
        code, output = run_app([executable("SM_OSINT_CHECKER_BIN", "account-checker"), "--username", self.username])
        self.set_output(output)
        self.message = "Public profile checks complete." if code == 0 else f"Profile checks failed (exit {code})."

    def pipeline(self) -> None:
        if not self.require_username():
            return
        discovery_code, discovery = run_app([executable("SM_OSINT_BIN", "sm-osint"), "--query", self.username, "10"])
        checker_code, checks = run_app([executable("SM_OSINT_CHECKER_BIN", "account-checker"), "--username", self.username])
        self.set_output(["=== Local discovery ===", *discovery, "", "=== Public profile checks ===", *checks])
        if discovery_code == 0 and checker_code == 0:
            self.message = "Complete pipeline finished."
        else:
            self.message = f"Pipeline finished with errors (discovery {discovery_code}, checks {checker_code})."

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
            if key == curses.KEY_PPAGE:
                self.scroll_output(1)
            elif key == curses.KEY_NPAGE:
                self.scroll_output(-1)
            elif key == curses.KEY_HOME:
                self.output_offset = max(0, len(self.output) - max(1, self.screen.getmaxyx()[0] - 19))
            elif key == curses.KEY_END:
                self.output_offset = 0
            if key in (ord("e"), ord("E")):
                self.prompt_username()
            elif key == curses.KEY_UP:
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
