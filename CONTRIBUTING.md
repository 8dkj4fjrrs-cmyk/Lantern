# Contributing to Lantern Engine

Thank you for helping. Lantern is an MIT-licensed C++20 2D game engine and
editor; by contributing you agree your contribution is licensed under the
same [MIT License](LICENSE).

## Before you start

- Read **[AGENTS.md](AGENTS.md)**. It holds the working rules, the
  architecture rules and the code style that every change follows, whether
  it is written by a person or an AI assistant. For example:
  - the engine never depends on the editor;
  - Axmol stays behind `engine/runtime`;
  - editor changes go through undoable commands;
  - file formats are versioned.
- Read the [roadmap](docs/roadmap.md) and the
  [known limitations](docs/known-limitations.md). Larger changes fit best
  when they follow the plan.
- Design decisions are recorded as ADRs in [docs/adr](docs/adr). A change to
  the architecture needs a new ADR.

## Building and testing

See [docs/building.md](docs/building.md). Before sending a change:

```sh
cmake --workflow --preset dev        # build + all tests
cmake --workflow --preset core       # the parts without Axmol
```

- Every bug fix comes with a test that fails without it.
- New features come with tests, and with documentation when they change
  something users see, a public API, a build step or a file format.
- Say where you ran the tests (system and compiler). Never report a build
  or test as passing unless it was run.

## Sending a change

1. Keep changes small and focused; one topic per pull request.
2. Explain *why* in the description; the code shows *what*.
3. Update `CHANGELOG.md` for anything users notice.
4. New dependencies need:
   - a reason;
   - a pinned version;
   - their license checked against MIT distribution;
   - an entry in `THIRD_PARTY_NOTICES.md`;
   - their license text in `licenses/` if games will contain them
     (`tools/release/fetch_licenses.py`).

## Reporting problems

Include:
- what you did, what you expected and what happened;
- the Lantern version (Help > About);
- your system;
- the Console's messages (the Console panel's **Copy** button copies them).

For problems with an exported game, also give the output of starting the
game program from a terminal.
