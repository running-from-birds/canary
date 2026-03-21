# Canary — CLAUDE.md

Context file for Claude Code. Read this before making any changes to this project.

## What this is

A vocoder VST that tracks and follows a chord progression in the DAW. More functionality to be added.

## Stack

TBD

### IMPORTANT ###
NEVER edit `main` directly. ALWAYS create a new branch if editing, even for temporary edits.


## Workflow

When implementing any new feature:
1. Claude creates a new git branch with a descriptive name (e.g. `feat/short-description`)
2. Claude makes the changes on that branch
3. Claude creates a PR with a brief description of what was changed and why
4. Ian/Chris will manually review and report back with notes and changes to add to the branch
5. Last step before a PR is merged is always updating `CLAUDE.md` with any relevant updates made in the branch
6. Ian/Chris will manually squash and merge the PR in GitHub once it's confirmed that it works as intended
7. Claude confirms that it has been sucessully deployed