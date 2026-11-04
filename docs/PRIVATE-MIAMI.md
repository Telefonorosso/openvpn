# Private Miami test: authorized operator procedure

This is separate from the normal maintainer build. It requires an operator who
owns/has permission to use the licensed assets and can configure the chosen
repository's Actions runner. No ROM, MUI distribution, Miami registration key,
VPN credential or private machine path is supplied by this source repository.
The recorded handoff used one temporary runner and removed it afterward.

## Required environment

Use a dedicated empty private working directory on a Linux x86_64 host, not an
existing audit, installation or performance runner. Restrict it to the operator.
The tested host was Debian 13. Required commands: Python 3.12+, `lha` (lhasa),
`ip` (iproute2), `unshare` (util-linux), `openssl`, `dpkg-deb`, Git and GitHub CLI.
The one-job GitHub runner also needs its documented OS runtime dependencies.
No compiler or SDK installation is needed here: CI downloads hosted-built
Amiga and Copperline artifacts from the same workflow run.

Verify user/network namespaces and TUN availability before registration:

```sh
unshare -Urn true
test -c /dev/net/tun
python3 --version
command -v lha ip unshare openssl dpkg-deb git gh
```

The full test configures only its new network namespace, never the host's.
It downloads/extracts a pinned Debian server executable and AmiSSL runtime
into temporary storage, makes fresh test certificates and starts no service.

Supply existing **read-only original** assets:

- AmigaOS tree with `Libs`, `Fonts`, `System/MUI`, `C`, `Rexxc` and `System/RexxMast`.
  The checked fixture used Kickstart 3.1 and MUI 3.8.
- Matching authorized Kickstart ROM.
- MiamiDX registration directory containing `MIAMI.KEY1`, `MIAMI.KEY2`,
  `MIAMIDX.KEY` (not the demo alone; administrative tools/events need registration).
- `a2065.device`, matching the emulator's A2065 hardware.

The job stages private copies. It never changes installed hardware, OS images
or original asset files. Different ROM/font/theme/layout combinations can fail
the guarded confirmation fixture; inspect the private screenshot and update
that fixture only after actual visual verification. Never replace the guard
with a blind timed Yes keypress or publish a registered screenshot.

## Repository configuration and runner

1. Select the repository explicitly: authoritative `Telefonorosso/openvpn`, or
   the contributor fork for pre-adoption verification. Never silently use a
   different GitHub repository from an ambient checkout.
2. In Settings > Secrets and variables > Actions > **Variables**, configure:

   | Name | Value |
   | --- | --- |
   | `MIAMI_TEST_RUNNER` | A unique one-job label, e.g. `miami-private-REVIEW-ID` |
   | `MIAMI_GUEST_SYSTEM` | Absolute path to the authorized OS/MUI tree |
   | `MIAMI_ROM` | Absolute path to its matching ROM file |
   | `MIAMI_KEYS_DIR` | Absolute path to the registration directory |
   | `MIAMI_NETWORK_DEVICE` | Absolute path to `a2065.device` |

   These are paths/labels, not credential contents. Keep credential bytes out
   of repository variables, logs, PRs and artifacts. The workflow validates
   nonempty configuration before queuing the private job.
3. In Settings > Actions > Runners > **New self-hosted runner**, select Linux
   x64. Follow GitHub's download/hash instructions in the new private directory.
   Obtain the registration command for this exact repository. Use its unique
   label and append `--ephemeral --disableupdate` to `config.sh`. The tested
   runner was 2.338.2; a later supported runner is an infrastructure update,
   not permission to change the emulator/toolchain pins.
4. Registration tokens are short-lived secrets. Use the repository's authorized
   account and private terminal. An assisting agent must capture token/API
   responses in memory and redact any command output; never echo/save the
   token, send it in chat, commit it or put it in a shell trace. Do not install
   a persistent service or reuse/preempt another owner's runner.
5. Register the runner but **do not start `run.sh` yet**. Check there is no
   other queued/in-progress manually requested job that could use its label.
   The unique label prevents unintended job pickup.

GitHub's runner download/registration instructions depend on the repository
and current supported runner. Use that page's freshly generated commands,
not a copied expired registration token. No private secret is part of this guide.

## Dispatch and verify the exact reviewed source

The workflow must already exist on the selected repository's default branch
for ordinary UI/manual dispatch. The operator chooses a reviewed branch or tag
and records its **full commit SHA** before dispatch. Do not run unreviewed fork
code on the licensed host.

In Actions > Build and test > Run workflow, select that ref and enable
`miami_native`. The CLI equivalent is:

```sh
gh workflow run ci.yml --repo OWNER/REPOSITORY --ref REVIEWED-BRANCH-OR-TAG -f miami_native=true
```

The capitalized values are chosen repository/ref parameters, not literal names.
Record the returned run URL/ID. Verify its full `headSha` equals the reviewed
SHA, all five public prerequisite jobs actually succeeded, and **MiamiDX on
licensed AmigaOS** is queued for the unique runner. Recheck no competing job.
Only then start `./run.sh` in the private runner directory. Do not enable auto
pickup before these checks. The ephemeral runner executes one job and exits.

The job verifies hosted emulator hashes/source pin and fresh binary source
metadata, runs the real Miami socket/reconnect test, then the full encrypted
client with production scripts. No harness changes or weakened assertions are
needed to make this run. Inspect actual job steps, not aggregate text alone.

## Required verdict and retained evidence

Require job success and both JSON results to have `passed: true` and numeric
`returncode: 0`. Full VPN results also require the chosen `source_commit`,
`routes_verified: true`, two application UDP replies from assigned tunnel source
`10.8.0.2`, CA/server verification, full hook completion, pushed/restored DNS and
clean Miami/ARexx shutdown. The encrypted peer rejects cleartext NAT shortcuts.
Retain the workflow URL, source SHA, public pins and candidate/client/driver
hashes. Verify cited packaged binaries match the ones tested.

Download `miami-native-results` and `miami-vpn-results`. The full test uploads
only whitelisted logs/hashes: no registration files, private certificates,
private runtime/ROM or registered screenshot. Control tokens are redacted.
Unknown dialogs, timeouts, queued/skipped/cancelled jobs or partial cycles fail
or remain unverified. Inspect failure screenshots privately on the operator's
host; the workflow never uploads them.

## Cleanup after the job

Verify the one-job listener/worker, emulator and test server have stopped, the
runner is no longer registered, and only task-owned scratch remains. If a job
was cancelled before automatic deregistration, remove just that runner through
the selected repository's Settings > Actions > Runners before deleting its
private directory. Never stop another runner to satisfy cleanup.

Keep small sanitized result/run/hash evidence. Delete the task's private runner
work directory, temporary runtime copies, generated credentials and diagnostics;
verify no own process still references them. Preserve every original licensed
asset and unrelated service. Remove only task-created repository variables or
explicitly retain a documented reusable configuration. A future manual run
needs a newly authorized runner; the old green run is historical evidence.

See [GitHub's runner guide](https://docs.github.com/en/actions/how-tos/manage-runners/self-hosted-runners/add-runners)
and [manual workflow runs](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/manually-run-a-workflow).
