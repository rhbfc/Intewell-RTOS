---
name: rtos-comment-style
description: Use when adding or rewriting comments in RTOS C code, especially kernel, driver, and UAPI headers. This skill applies when the user asks for concise kernel-style Chinese comments, comment cleanup, or guidance on what should and should not be commented.
---

# RTOS Comment Style

Use this skill to decide whether a comment is needed and how to write it.

## Workflow

1. Check whether the code already explains itself.
2. If intent, invariant, layout, lifecycle, or compatibility is not obvious, add one short comment.
3. Prefer a short block comment in Chinese.
4. Explain `why`, `what invariant`, or `what is encoded here`, not line-by-line behavior.
5. Keep code behavior unchanged.

## Rules

- Keep comments short, usually one or two lines.
- Focus on non-obvious state, data layout, lifecycle, compatibility, and tricky branches.
- Avoid Doxygen headers, parameter tables, and obvious restatements.
- Avoid mass-commenting every branch or loop.
- Preserve stable terms such as protocol names, field names, and subsystem names.
- Do not rewrite code just to support a comment.

## Reference

- See [references/comment_style.md](references/comment_style.md) for the full rule set and examples.
