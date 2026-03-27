---
name: feature-split-zh-commit
description: Split workspace changes into focused commits by function, behavior, or subsystem, and write Chinese git commit messages. Use when Codex needs to avoid mixing unrelated edits, work in a dirty tree, stage only the relevant files or hunks, or submit one or more Chinese commits after implementing changes.
---

# Function Split And Chinese Commit

Use this skill to keep change sets cohesive. Split work by functional intent first, then by file layout, and commit each unit with a concise Chinese message.

## Workflow

1. Inspect `git status --short` and `git diff` before changing or staging anything.
2. Separate user-requested work from unrelated pre-existing edits in the tree.
3. Define commit units around one purpose only: one feature, one fix, one refactor, one test bundle, one doc bundle, or one build change.
4. Read [references/functional-splitting.md](references/functional-splitting.md) when deciding whether changes belong in the same commit.
5. Read [references/chinese-commit.md](references/chinese-commit.md) before writing the commit subject and optional body.
6. Verify each unit independently when practical, then stage only the matching files or hunks.
7. Re-check `git diff --cached` before every commit to confirm the staged change set still has one clear purpose.

## Guardrails

- Prefer functional boundaries over directory boundaries.
- Keep code and directly supporting tests or docs in the same commit when they validate the same change.
- Separate mechanical cleanup, formatting, renames, or generated files unless they are required for the same functional result.
- Never absorb unrelated dirty-tree changes into the current commit.
- If one file contains multiple unrelated edits, split by hunk or reshape the edit so each commit remains coherent.
- If the user explicitly asks for a single commit, keep the internal change set cohesive instead of inventing extra commits.

## Commit Rules

- Use Chinese for the commit subject.
- Keep the first line concise and action-oriented.
- Match the prefix to the actual change type instead of using vague wording.
- Add a Chinese commit body only when it clarifies motivation, scope, or verification.

## References

- [references/functional-splitting.md](references/functional-splitting.md): how to define commit boundaries and handle dirty worktrees
- [references/chinese-commit.md](references/chinese-commit.md): Chinese commit subject patterns and body templates
