# Maintainer quick start

This is your project. You approve changes and decide when to release. The new
layout gives you one client/driver, automatic builds and tests, and downloadable
packages. You can use the GitHub website for normal review and downloads.

## First adoption: one-time steps

1. Open [the reorganization PR](https://github.com/Telefonorosso/openvpn/pull/2).
   Most of the large diff is imported official OpenVPN source. The practical
   changes are the layout, CMake, CI and guides; the existing stack scripts and
   historical binaries are preserved. [Validation evidence](docs/HANDOFF.md)
   explains exactly what was tested.
2. Review the proposal. The contributor marks it ready after validation; if
   it still shows Draft, ask them to finish that step. If GitHub displays a workflow approval banner, use **Approve and run** after
   reviewing it. An absent banner is not an error: fork approval settings vary.
3. When satisfied, use **Create a merge commit**, then confirm the merge. This
   initial import carries the subtree history needed for future upstream updates.
   Preserve it: do not squash or rebase this import. If that merge option is
   unavailable, a repository owner can enable **Allow merge commits** under
   Settings > General > Pull Requests. Ask for help rather than changing history.
4. Open **Actions > Build and test**. The main-branch push should start a run.
   If Actions is disabled, the owner enables it in Settings > Actions > General.
   Keep fork review requirements; this project does not need private secrets
   for its normal public build. Restricted organization policies may need an
   administrator's approval; do not work around them with a broad access token.
5. Confirm the five public jobs below pass on that main commit, then download
   its candidate. That is the acceptance check on your repository. The earlier
   green runs linked in the handoff were in the contributor fork, not your main.

**Run workflow** appears after the workflow is on the default branch. To rebuild
from the website: Actions > Build and test > Run workflow, select `main`, leave
`miami_native` unchecked, then Run workflow. It builds and tests; it does not
install anything, create a tag or publish a release.

## Download the Amiga build

1. Sign into GitHub. Open a completed successful **Build and test** run for the
   commit you want. Click its **release-candidate** artifact under **Artifacts**.
2. Extract GitHub's outer artifact ZIP. Inside are the actual Amiga `.zip` and
   `.tar.gz` packages, `SOURCE_COMMIT`, dependency/source provenance and
   `SHA256SUMS`. Choose one inner Amiga package; they contain the same payload.
3. `SOURCE_COMMIT` identifies the built commit. Use the package matching the
   intended version and verify its SHA-256 against `SHA256SUMS`.
4. Follow [installation locations](docs/INSTALL.md). Select one stack's hooks,
   preserve your configuration, and supply AmiSSL separately. The GitHub
   **Code > Download ZIP** button downloads source, not an installable build.

Candidates are test builds. A release appears on the repository's Releases page
only after you authorize tagging and publish its reviewed draft. Artifacts can
expire; they are not permanent release downloads.

## What green means

| Job/result | What it proves |
| --- | --- |
| Version and release tag | Port/upstream versions agree; pushed release tags match `VERSION` |
| Host regressions (Linux and macOS) | Three portable driver regressions on each host |
| AmigaPorts GCC cross-build | Amiga client/driver/helpers compile, target FIFO passes volamos memory checks, packages are created |
| Copperline guest smoke tests | Actual target FIFO/device probes and real Roadshow demo UDP/device interaction |
| MiamiDX on licensed AmigaOS | Only when explicitly run: real Miami socket cycles and two encrypted client/DNS/route/cleanup cycles |

The normal PR/main run has **five successful jobs**; Miami and tag-release jobs
are normally skipped. A skipped Miami job is not Miami validation. Queued,
cancelled or failed jobs are not passes. An artifact may already exist while
later tests are still running: wait for the full expected run to finish.

Full Miami testing needs licensed OS/MUI/registration assets and an authorized
private runner. There is no standing private runner included in this PR.
[Private operator instructions](docs/PRIVATE-MIAMI.md) explain complete setup,
execution, evidence and cleanup.
Request an authorized operator's run for Miami-affecting changes and retain its
exact commit, actual results and artifact links. The recorded old run does not
approve new runtime code automatically.

## Make a change with help from an agent

Use this prompt; the agent needs no prior chat history:

> Read AGENTS.md and docs/AGENT-START.md. Confirm this is the OpenVPN repository,
> report the current branch/commit and preserve other work. Make the requested
> change on a branch, run the applicable existing checks, and open a normal PR
> with exact source/result evidence. Keep upstream history and legacy files.
> Ask me before merging, tagging, publishing or closing an issue.

For a small text edit, GitHub's pencil button can create a branch and PR. For
code, let the agent prepare the branch/PR. Review its summary and checks before
merging. Never upload a whole replacement source ZIP over the tracked source.
[Source locations](docs/REPOSITORY.md) explain where changes belong.

## If something fails

- Open the red job and its first failed step. Copy the run URL and the first
  error into an issue or give them to the agent. Preserve the source SHA.
- Download `host-results-*`, `volamos-results`, `emulator-results` or the Miami
  result artifact when relevant. Do not share credentials, registration files,
  private ROMs or registered GUI screenshots.
- A cancelled run may have been superseded by a newer commit: inspect the latest
  run. Do not bypass a genuine failure or publish an earlier artifact as newer.
- A checksum/download failure needs investigation, not disabled verification.
- If a workflow needs repository permissions, the owner decides the change.
  Our contributor account cannot change your Actions settings or merge for you.

## Releases and upstream updates

Give the agent [the exact release procedure](docs/RELEASING.md) and approve the
specific version and commit when ready. It prepares tested packages and a draft;
you decide publication. No release is authorized just by adopting this PR.

Official OpenVPN updates use [the subtree procedure](docs/UPSTREAM.md). Keep
these as reviewed updates with preserved Amiga changes and full relevant tests.
The agent guide covers version bumps, pins and the validation decision table.

GitHub references: [fork workflow approval](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/approve-runs-from-forks),
[manual runs](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/manually-run-a-workflow),
[artifact downloads](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/download-workflow-artifacts?tool=webui),
[merge methods](https://docs.github.com/en/pull-requests/reference/pull-request-merges).
