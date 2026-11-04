# Updating the OpenVPN source

The canonical source is [OpenVPN/openvpn](https://github.com/OpenVPN/openvpn).
The initial subtree is the official `v2.6.23` tag, commit
`49620843c487a7fee58e3030b7c88681c115e24e`. `src/openvpn/` contains the subtree; subsequent
commits layer the Amiga port over that baseline. The rest of this repository
(vpn.device, stack scripts and packaging) is maintained by the Amiga project.

## Importing another upstream release

Work on a branch and choose the intended version explicitly. The example tag
below is a placeholder, not a recommendation to upgrade to an unchecked release.

```sh
git switch -c update/openvpn-VERSION
git subtree pull --prefix=src/openvpn https://github.com/OpenVPN/openvpn.git vVERSION --squash
```

Review and resolve conflicts in the Amiga changes, rebuild with the pinned
toolchain, and run the agreed validation before merging. The `--squash` subtree
workflow records the upstream commit in `git-subtree-split` metadata and keeps
release imports distinct from Amiga port changes. Upstream's complete history
remains available from the official remote for investigation and comparisons.
Do not replace the directory with another downloaded ZIP.

## Contributing changes back to OpenVPN

Keep portable fixes separate from Amiga-only integration and packaging. A
source-only history can be exported with:

```sh
git fetch https://github.com/OpenVPN/openvpn.git tag v2.6.23
git subtree split --prefix=src/openvpn --branch=export/amiga-openvpn
```

The fetch makes the tracked upstream history available in a fresh clone;
use the currently tracked tag after an upgrade. Use that export to select small commits or patches for review against the
appropriate official OpenVPN branch. Open a contribution from a fork of
`OpenVPN/openvpn`, following that project's contribution instructions.
Francesco's Amiga repository and contributor forks of it are a separate GitHub
fork network; embedding a subtree does not change that relationship.

The `vpn.device` implementation and stack lifecycle scripts are not part of
upstream OpenVPN and remain in this project. Previously proposed IPv6 patches
are a separate review step, not a second authoritative driver distribution.
