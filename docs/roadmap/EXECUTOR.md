# Executor protocol for the Attome AE roadmap

You are implementing Attome Agentic Engineering one task at a time. The plan is in this folder: one `<ID>.json`
file per feature, grouped into phases and workstreams by `docs/tools/build_roadmap.py` and shown with checkboxes
in `docs/agentic-engineering-roadmap.html`. Implementation plans exist for phases P0–P2; plans for later phases are
written at the start of each phase. Follow these rules exactly. When a rule and a task disagree, the rule wins;
stop and ask the owner.

## Before any task

1. Read `docs/feature-plan.md` for the feature, and the research summary in
   `docs/research/competitive-landscape-2026-11.md` once per session.
2. Work in phase order (P0, P1, ...). Inside a phase, take the first task whose `after` tasks are all done.
3. Read every path in the task's `read_first` list in full before changing anything.
4. Check the task still matches the repository: every file, function or module the steps name must exist (or be
   the thing the step creates). If not, stop and ask. Do not guess a replacement.

## Doing the task

5. Do the `steps` in order, literally. Do not add features, refactors or renames the steps do not name.
6. Respect every `do_not` line. If finishing the task seems to need one of them, stop and ask.
7. If the task's `stop_and_ask_if` condition happens, stop and ask the owner with what you found and the options.
8. Never, under any task:
   - let the agent or an LLM change solver settings, material values or checker thresholds that judge a design;
   - report a number from a surrogate, an estimate or a coarse mesh as a verified result;
   - import the `gmsh` Python module into code that will be distributed as a binary (run gmsh and ccx as subprocesses);
   - add a dependency whose licence is AGPL, or FluidX3D, or anything not listed in `docs/licences.md`;
   - mix units: every number is mm, N, MPa, tonne, s, K (or °C where stated) and the unit is stored with it;
   - push to a remote unless the owner says so.

## Finishing the task

9. Run every command in `verify`. A task is done only when each command passes as `verify` describes. Paste the
   command and the result lines into your report. If a check fails twice after a fix, stop and ask.
10. Check `done_when` is true.
11. Commit with the message `<task id>: <task sentence>` and the attribution line the repository uses.
    One task per commit.
12. Tick the task in the roadmap page (or record its id as done in your report).

## Finishing a feature

13. When all of a feature's tasks are done, check it against the Definition of done at the top of the roadmap
    page and every line of its `acceptance` list. Report each line as pass or fail with evidence.
14. Tick the feature only when every line passes.

## Finishing a phase

15. Phase exit checks are reviewed by the owner. Report them with evidence; do not tick them yourself.

## Report format after each task

```
Task: <id> <task sentence>
Changed: <files>
Verify:
  $ <command>
  <result lines>
Done when: <true/false and why>
Questions: <none, or what needs the owner>
```
